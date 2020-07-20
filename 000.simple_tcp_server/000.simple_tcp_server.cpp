#include <stdio.h>

#ifndef _WIN32
#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
#  include <arpa/inet.h>
# endif
#include <sys/socket.h>
#else
#include <WinSock2.h>
#endif

#include <event2/listener.h>


void accept_cb(evconnlistener* listener, evutil_socket_t fd, sockaddr* addr, int socklen, void* context)
{
	char str[INET_ADDRSTRLEN] = { 0 };
	auto sin = (sockaddr_in*)addr;
	inet_ntop(AF_INET, &sin->sin_addr, str, INET_ADDRSTRLEN);
	printf("accpet TCP connection from: %s\n", str);
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

	int port = 8000;
	if (argc > 1) {
		port = atoi(argv[1]);
	}

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

	evconnlistener_set_error_cb(listener, accpet_error_cb);
	event_base_dispatch(base);

	return 0;
}
