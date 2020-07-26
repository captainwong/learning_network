/**
Network Working Group                                          J. Postel
Request for Comments: 863                                            ISI
																May 1983



							Discard Protocol




This RFC specifies a standard for the ARPA Internet community.  Hosts on
the ARPA Internet that choose to implement a Discard Protocol are
expected to adopt and implement this standard.

A useful debugging and measurement tool is a discard service.  A discard
service simply throws away any data it receives.

TCP Based Discard Service

   One discard service is defined as a connection based application on
   TCP.  A server listens for TCP connections on TCP port 9.  Once a
   connection is established any data received is thrown away.  No
   response is sent.  This continues until the calling user terminates
   the connection.

UDP Based Discard Service

   Another discard service is defined as a datagram based application on
   UDP.  A server listens for UDP datagrams on UDP port 9.  When a
   datagram is received, it is thrown away.  No response is sent.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <chrono>

int64_t bytes_recvd = 0;
int64_t msges_recvd = 0;
int64_t last_time_bytes_recvd = 0;
std::chrono::steady_clock::time_point last_time_checked = {};

#define MAX_EPOLL_EVENTS 64

int print_error(const char* msg)
{
	int err = errno;
	fprintf(stderr, "%s %d:%s\n", msg, err, strerror(err));
	return err;
}

int make_socket_non_block(int fd)
{
	int opts = fcntl(fd, F_GETFL);
	if (opts < 0) {
		perror("fcntl F_GETFL");
		return -1;
	}
	opts |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, opts) < 0) {
		return print_error("make_socket_non_block failed");
	}
	return 0;
}

void print_events(uint32_t events)
{
	printf("%08X : ", events);
#define check_and_print_event(ev, e) if(ev & e) { printf("%s ", #e); }
	check_and_print_event(events, EPOLLIN);
	check_and_print_event(events, EPOLLPRI);
	check_and_print_event(events, EPOLLOUT);
	check_and_print_event(events, EPOLLHUP);
	check_and_print_event(events, EPOLLRDHUP);
	check_and_print_event(events, EPOLLERR);
	printf("\n");
}


int main(int argc, char** argv)
{
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd < 0) {
		return print_error("socket failed");
	}

	int ret = make_socket_non_block(listenfd);
	if (ret != 0) {
		return ret;
	}

	sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(10009);

	ret = bind(listenfd, (sockaddr*)&addr, sizeof(addr));
	if (ret < 0) {
		return print_error("bind failed");
	}

	ret = listen(listenfd, SOMAXCONN);
	if (ret < 0) {
		return print_error("listen failed");
	}

	int epfd = epoll_create1(0);
	if (epfd < 0) {
		return print_error("epoll_create1 failed");
	}

	epoll_event ev, events[MAX_EPOLL_EVENTS];
	ev.data.fd = listenfd;
	ev.events = EPOLLIN | EPOLLET;
	ret = epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);
	if (ret) {
		return print_error("epoll_ctl failed");
	}

	last_time_checked = std::chrono::steady_clock::now();

	while (1) {
		int n = epoll_wait(epfd, events, MAX_EPOLL_EVENTS, 3000);
		//printf("got %d events\n", n);
		for (int i = 0; i < n; i++) {
			if (events[i].events & EPOLLERR ||
				events[i].events & EPOLLHUP ||
				!(events[i].events & EPOLLIN)) {
				fprintf(stderr, "epoll error\n");
				printf("events #%d ", i); print_events(events[i].events);
				close(events[i].data.fd);
				continue;
			}

			if (listenfd == events[i].data.fd) {
				/* We have a notification on the listening socket, which
				 means one or more incoming connections. */
				while (1) {
					sockaddr inaddr = { 0 };
					socklen_t inlen = 0;
					int fd = accept(listenfd, &inaddr, &inlen);
					printf("accept ret=%d\n", fd);
					if (fd == -1) {
						ret = errno;
						print_error("accept error");
						if (ret == EAGAIN || ret == EWOULDBLOCK) {
							printf("all incoming connections processed\n");
						} else {
							printf("real error\n");
						}
						break;
					}

					char str[INET_ADDRSTRLEN] = { 0 };
					auto sin = (sockaddr_in*)&inaddr;
					inet_ntop(AF_INET, &sin->sin_addr, str, INET_ADDRSTRLEN);
					printf("accpet TCP connection #%d from: %s:%d\n", fd, str, sin->sin_port);

					ret = make_socket_non_block(fd);
					if (ret != 0) {
						close(fd);
						break;
					}

					ev.data.fd = fd;
					ev.events = EPOLLIN | EPOLLET;
					ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
					if (ret < 0) {
						print_error("epoll_ctl on client fd failed");
						close(fd);
						break;
					}
				}
			} else {
				/* We have data on the fd waiting to be read. Read it. 
				 We must read whatever data is available
				 completely, as we are running in edge-triggered mode
				 and won't get a notification again for the same
				 data. */

				char buf[10240];
				bool should_close = false;
				while (1) {
					ssize_t bytes = read(events[i].data.fd, buf, sizeof(buf));
					if (bytes == -1) {
						/* If errno == EAGAIN, that means we have read all
						 data. So go back to the main loop. */
						if (errno != EAGAIN) {
							should_close = true;
						}
						break;
					} else if (bytes == 0) {
						/* End of file. The remote has closed the
						 connection. */
						should_close = true;
						break;
					}

					bytes_recvd += bytes;
					msges_recvd++;

					/*if (bytes < sizeof(buf)) {
						break;
					}*/
				}

				if (should_close) {
					printf("connection #%d closed\n", events[i].data.fd);
					/* Closing the descriptor will make epoll remove it
					 from the set of descriptors which are monitored. */
					close(events[i].data.fd);
				}
			}
		}

		auto now = std::chrono::steady_clock::now();
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time_checked).count();
		if (ms >= 3000) {
			auto bytes = bytes_recvd - last_time_bytes_recvd;
			printf("%4.3f MiB/s %4.3f Ki Msgs/s %6.2f bytes per msg\n",
				   (bytes * 1000.0 / ms / 1024 / 1024),
				   (msges_recvd * 1000.0 / ms / 1024),
				   (bytes) * 1.0 / (msges_recvd));

			last_time_bytes_recvd = bytes_recvd;
			msges_recvd = 0;
			last_time_checked = now;
		}
	}

	return 0;
}
