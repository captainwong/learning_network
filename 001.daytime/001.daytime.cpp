/**
Network Working Group                                          J. Postel
Request for Comments: 867                                            ISI
																May 1983



							Daytime Protocol




This RFC specifies a standard for the ARPA Internet community.  Hosts on
the ARPA Internet that choose to implement a Daytime Protocol are
expected to adopt and implement this standard.

A useful debugging and measurement tool is a daytime service.  A daytime
service simply sends a the current date and time as a character string
without regard to the input.

TCP Based Daytime Service

   One daytime service is defined as a connection based application on
   TCP.  A server listens for TCP connections on TCP port 13.  Once a
   connection is established the current date and time is sent out the
   connection as a ascii character string (and any data received is
   thrown away).  The service closes the connection after sending the
   quote.

UDP Based Daytime Service

   Another daytime service service is defined as a datagram based
   application on UDP.  A server listens for UDP datagrams on UDP port
   13.  When a datagram is received, an answering datagram is sent
   containing the current date and time as a ASCII character string (the
   data in the received datagram is ignored).

Daytime Syntax

   There is no specific syntax for the daytime.  It is recommended that
   it be limited to the ASCII printing characters, space, carriage
   return, and line feed.  The daytime should be just one line.

	  One popular syntax is:

		 Weekday, Month Day, Year Time-Zone

		 Example:

			Tuesday, February 22, 1982 17:37:43-PST
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

void writecb(struct bufferevent* bev, void* user_data)
{
	printf("writecb\n");
	if (evbuffer_get_length(bufferevent_get_output(bev)) == 0) {
		printf("flushed time\n");
		bufferevent_free(bev);
		//shutdown(bufferevent_getfd(bev), 2);
	}
}

void eventcb(struct bufferevent* bev, short events, void* user_data)
{
	printf("eventcb events=%04X\n", events);
	if (events & BEV_EVENT_ERROR) {
		printf("Got an error on the connection: %s\n",
			   strerror(errno));/*XXX win32*/
	}else if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
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

	bufferevent_setcb(bev, nullptr, writecb, eventcb, nullptr);
	bufferevent_enable(bev, EV_WRITE);

	time_t t = time(nullptr);
	char* s = asctime(localtime(&t));
	bufferevent_write(bev, s, strlen(s));
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

	int port = 10013;

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
