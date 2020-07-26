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
# ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#  define _WINSOCK_DEPRECATED_NO_WARNINGS
# endif
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

char* msg = nullptr;
int msg_len = 0;

void writecb(struct bufferevent* bev, void* user_data)
{
	auto output = bufferevent_get_output(bev);
	if (evbuffer_get_length(output) == 0) {
		bufferevent_write(bev, msg, msg_len);
	}
}

void eventcb(struct bufferevent* bev, short events, void* user_data)
{
	printf("eventcb events=%04X\n", events);
	if (events & BEV_EVENT_CONNECTED) {
		printf("connected\n");
		bufferevent_write(bev, msg, msg_len);
		return;
	} else if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
		printf("Connection closed.\n");
	} else if (events & BEV_EVENT_ERROR) {
		printf("Got an error on the connection: %s\n",
			   strerror(errno));/*XXX win32*/
	}
	/* None of the other events can happen here, since we haven't enabled
		* timeouts */

	bufferevent_free(bev);
}

int main(int argc, char** argv)
{
#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(0x0201, &wsa_data);
#endif

	if (argc < 2) {
		printf("Usage: `%s ip [port] [msg_size]`, default port is 10009, default msg_size is 512 bytes\n", argv[0]);
		return 1;
	}

	auto ip = argv[1];

	int port = 10009;
	if (argc > 2) {
		port = atoi(argv[2]);
		if (port <= 0 || port > 65535) {
			puts("Invalid port");
			return 1;
		}
	}

	msg_len = 512;
	if (argc > 3) {
		msg_len = atoi(argv[3]);
		if (msg_len <= 0) {
			fprintf(stderr, "msg_len must bigger than 0\n");
			return 1;
		}
	}
	msg = new char[msg_len];
	memset(msg, 'H', msg_len);

	sockaddr_in sin = { 0 };
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(ip);
	sin.sin_port = htons(port);

	auto base = event_base_new();
	if (!base) {
		fprintf(stderr, "init libevent failed\n");
		return -1;
	}

	auto bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
	if (!bev) {
		fprintf(stderr, "allocate bufferevent failed\n");
		return 1;
	}
	bufferevent_setcb(bev, nullptr, writecb, eventcb, nullptr);
	bufferevent_enable(bev, EV_WRITE);

	if (bufferevent_socket_connect(bev, (sockaddr*)(&sin), sizeof(sin)) < 0) {
		fprintf(stderr, "error starting connection\n");
		return -1;
	}

	event_base_dispatch(base);

	return 0;
}
