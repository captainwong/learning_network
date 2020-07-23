/**
Network Working Group                                    J. Postel - ISI
Request for Comments: 868                           K. Harrenstien - SRI
																May 1983



							 Time Protocol




This RFC specifies a standard for the ARPA Internet community.  Hosts on
the ARPA Internet that choose to implement a Time Protocol are expected
to adopt and implement this standard.

This protocol provides a site-independent, machine readable date and
time.  The Time service sends back to the originating source the time in
seconds since midnight on January first 1900.

One motivation arises from the fact that not all systems have a
date/time clock, and all are subject to occasional human or machine
error.  The use of time-servers makes it possible to quickly confirm or
correct a system's idea of the time, by making a brief poll of several
independent sites on the network.

This protocol may be used either above the Transmission Control Protocol
(TCP) or above the User Datagram Protocol (UDP).

When used via TCP the time service works as follows:

   S: Listen on port 37 (45 octal).

   U: Connect to port 37.

   S: Send the time as a 32 bit binary number.

   U: Receive the time.

   U: Close the connection.

   S: Close the connection.

   The server listens for a connection on port 37.  When the connection
   is established, the server returns a 32-bit time value and closes the
   connection.  If the server is unable to determine the time at its
   site, it should either refuse the connection or close it without
   sending anything.
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
#include <string.h>
#include <time.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

void readcb(struct bufferevent* bev, void* user_data)
{
	printf("readcb\n");
	auto input = bufferevent_get_input(bev);
	if (evbuffer_get_length(input) >= 4) {
		char buff[4];
		evbuffer_remove(input, buff, 4);
		time_t t = ntohl(*(uint32_t*)(buff));
		printf("%s", asctime(localtime(&t)));
		bufferevent_free(bev);
	}
}

void eventcb(struct bufferevent* bev, short events, void* user_data)
{
	printf("eventcb events=%04X\n", events);
	if (events & BEV_EVENT_CONNECTED) {
		printf("connected\n");
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
		printf("Usage: `%s ip [port]`, default port is 10037\n", argv[0]);
		return 1;
	}

	auto ip = argv[1];
	int port = 10037;
	if (argc > 3) {
		port = atoi(argv[2]);
		if (port <= 0 || port > 65535) {
			puts("Invalid port");
			return 1;
		}
	}

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
		event_base_free(base);
		return 1;
	}
	bufferevent_setcb(bev, readcb, nullptr, eventcb, nullptr);
	bufferevent_enable(bev, EV_READ);

	if (bufferevent_socket_connect(bev, (sockaddr*)(&sin), sizeof(sin)) < 0) {
		fprintf(stderr, "error starting connection\n");		
		bufferevent_free(bev);
		event_base_free(base);
		return -1;
	}
	
	event_base_dispatch(base);

	return 0;
}
