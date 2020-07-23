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

#ifndef _WIN32
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#else
# ifndef  _CRT_SECURE_NO_WARNINGS
#  define  _CRT_SECURE_NO_WARNINGS
# endif
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <chrono>

int64_t bytes_recvd = 0;
int64_t msges_recvd = 0;
int64_t last_time_bytes_recvd = 0;
std::chrono::steady_clock::time_point last_time_checked = {};

void readcb(struct bufferevent* bev, void* user_data)
{
	auto input = bufferevent_get_input(bev);
	int len = evbuffer_get_length(input);
	evbuffer_drain(input, len);
	bytes_recvd += len;
	msges_recvd++;
}

void eventcb(struct bufferevent* bev, short events, void* user_data)
{
	printf("eventcb events=%04X\n", events);
	if (events & BEV_EVENT_ERROR) {
		printf("Got an error on the connection: %s\n",
			   strerror(errno));/*XXX win32*/
	} else if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
		printf("Connection closed.\n");
	}
	/* None of the other events can happen here, since we haven't enabled
		* timeouts */

	bufferevent_free(bev);
}

void timer_cb(evutil_socket_t fd, short what, void* arg)
{
	auto now = std::chrono::steady_clock::now();
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time_checked).count();
	auto bytes = bytes_recvd - last_time_bytes_recvd;
	//if (msges_recvd == 0) { msges_recvd = 1; }
	printf("%4.3f MiB/s %4.3f Ki Msgs/s %6.2f bytes per msg\n",
		   double(bytes * 1000 / ms / 1024 / 1024),
		   double(msges_recvd * 1000 / ms / 1024),
		   double(bytes) / double(msges_recvd));

	last_time_bytes_recvd = bytes_recvd;
	msges_recvd = 0;
	last_time_checked = now;
}

void accept_cb(evconnlistener* listener, evutil_socket_t fd, sockaddr* addr, int socklen, void* context)
{
	char str[INET_ADDRSTRLEN] = { 0 };
	auto sin = (sockaddr_in*)addr;
	inet_ntop(AF_INET, &sin->sin_addr, str, INET_ADDRSTRLEN);
	printf("accpet TCP connection from: %s:%d\n", str, sin->sin_port);

	//evutil_closesocket(fd);
	//shutdown(fd, 2);

	auto base = evconnlistener_get_base(listener);
	auto bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	if (!bev) {
		fprintf(stderr, "Error constructing bufferevent!\n");
		event_base_loopbreak(base);
		return;
	}

	bufferevent_setcb(bev, readcb, nullptr, eventcb, nullptr);
	bufferevent_enable(bev, EV_READ);
}

void accpet_error_cb(evconnlistener* listener, void* context)
{
	auto base = evconnlistener_get_base(listener);
	int err = EVUTIL_SOCKET_ERROR();
	fprintf(stderr, "accpet_error_cb:%d:%s\n", err, evutil_socket_error_to_string(err));
	event_base_loopexit(base, nullptr);
}

int main(int argc, char** argv)
{
#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(0x0201, &wsa_data);
#endif

	int port = 10009;

	sockaddr_in sin = { 0 };
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(port);

	auto base = event_base_new();
	if (!base) {
		fprintf(stderr, "init libevent failed\n");
		return -1;
	}

	auto listener = evconnlistener_new_bind(base, 
											accept_cb, 
											nullptr, 
											LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, 
											-1, // backlog, -1 for default
											(sockaddr*)(&sin), 
											sizeof(sin));
	if (!listener) {
		fprintf(stderr, "create listener failed\n");
		return -1;
	}
	printf("%s is listening on port %d\n", argv[0], port);
	evconnlistener_set_error_cb(listener, accpet_error_cb);

	last_time_checked = std::chrono::steady_clock::now();
	auto timer = event_new(base, -1, EV_PERSIST, timer_cb, nullptr);
	if (!timer) {
		fprintf(stderr, "create timer failed\n");
		return -1;
	}
	struct timeval three_sec = { 3, 0 };
	event_add(timer, &three_sec);

	event_base_dispatch(base);

	return 0;
}
