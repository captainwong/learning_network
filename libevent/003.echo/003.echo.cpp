/**
Network Working Group                                          J. Postel
Request for Comments: 862                                            ISI
																May 1983



							 Echo Protocol




This RFC specifies a standard for the ARPA Internet community.  Hosts on
the ARPA Internet that choose to implement an Echo Protocol are expected
to adopt and implement this standard.

A very useful debugging and measurement tool is an echo service.  An
echo service simply sends back to the originating source any data it
receives.

TCP Based Echo Service

   One echo service is defined as a connection based application on TCP.
   A server listens for TCP connections on TCP port 7.  Once a
   connection is established any data received is sent back.  This
   continues until the calling user terminates the connection.

UDP Based Echo Service

   Another echo service is defined as a datagram based application on
   UDP.  A server listens for UDP datagrams on UDP port 7.  When a
   datagram is received, the data from it is sent back in an answering
   datagram.
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
#include <time.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

void readcb(struct bufferevent* bev, void* user_data)
{
	printf("readcb\n");
	evbuffer_add_buffer(bufferevent_get_output(bev), bufferevent_get_input(bev));
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

void accept_cb(evconnlistener* listener, evutil_socket_t fd, sockaddr* addr, int socklen, void* context)
{
	char str[INET_ADDRSTRLEN] = { 0 };
	auto sin = (sockaddr_in*)addr;
	inet_ntop(AF_INET, &sin->sin_addr, str, INET_ADDRSTRLEN);
	printf("accpet TCP connection from: %s:%d\n", str, sin->sin_port);

	auto base = evconnlistener_get_base(listener);
	auto bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	if (!bev) {
		fprintf(stderr, "Error constructing bufferevent!\n");
		event_base_loopbreak(base);
		return;
	}

	bufferevent_setcb(bev, readcb, nullptr, eventcb, nullptr);
	bufferevent_enable(bev, EV_WRITE | EV_READ);
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

	int port = 10007;

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
	event_base_dispatch(base);

	return 0;
}
