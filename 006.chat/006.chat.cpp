/**
Echo Server
no codec, just flush all msg to other clients
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
#include <unordered_map>

struct Client {
	int fd = 0;
	bufferevent* bev = nullptr;

};

std::unordered_map<int, Client*> clients = {};

void readcb(struct bufferevent* bev, void* user_data)
{
	auto me = (Client*)user_data;
	//evbuffer_add_buffer(bufferevent_get_output(bev), bufferevent_get_input(bev));
	auto input = bufferevent_get_input(bev);
	size_t len = evbuffer_get_length(input);
	char stack[1024];
	char* buf = nullptr;
	if (len <= 1024) {
		buf = stack;
	} else {
		buf = new char[len];
	}
	evbuffer_remove(input, buf, len);
	for (auto client : clients) {
		if (client.second != me) {
			auto output = bufferevent_get_output(client.second->bev);
			evbuffer_add_printf(output, "#%d says:", me->fd);
			evbuffer_add(output, buf, len);
		}
	}
	if (buf != stack) {
		delete[] buf;
	}
}

void eventcb(struct bufferevent* bev, short events, void* user_data)
{
	auto me = (Client*)user_data;
	printf("eventcb #%d events=%04X\n", me->fd, events);
	if (events & BEV_EVENT_EOF) {
		//printf("Connection closed.\n");
	} else if (events & (BEV_EVENT_WRITING)) {
		printf("Got an error while writing #%d .\n", me->fd);
	} else if (events & (BEV_EVENT_ERROR)) {
		printf("Got an error on the connection #%d : %s\n",
			   me->fd, strerror(errno));
	}

	for (auto client : clients) {
		if (client.second != me) {
			auto output = bufferevent_get_output(client.second->bev);
			evbuffer_add_printf(output, "#%d offline\n", me->fd);
		}
	}

	printf("client #%d offline\n", me->fd);
	clients.erase(me->fd);
	delete me;
	bufferevent_free(bev);
}

void accept_cb(evconnlistener* listener, evutil_socket_t fd, sockaddr* addr, int socklen, void* context)
{
	char str[INET_ADDRSTRLEN] = { 0 };
	auto sin = (sockaddr_in*)addr;
	inet_ntop(AF_INET, &sin->sin_addr, str, INET_ADDRSTRLEN);
	printf("accpet TCP connection #%d from: %s:%d\n", fd, str, sin->sin_port);

	//evutil_make_socket_nonblocking(fd);
	auto base = evconnlistener_get_base(listener);
	auto bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	if (!bev) {
		fprintf(stderr, "Error constructing bufferevent!\n");
		event_base_loopbreak(base);
		return;
	}

	evbuffer_add_printf(bufferevent_get_output(bev), "Welcome #%d!\n", fd);
	for (auto client : clients) {
		if (client.second->fd != fd) {
			auto output = bufferevent_get_output(client.second->bev);
			evbuffer_add_printf(output, "#%d online\n", fd);
		}
	}

	auto client = new Client();
	client->fd = fd;
	client->bev = bev;
	clients[fd] = client;
	bufferevent_setcb(bev, readcb, nullptr, eventcb, client);
	bufferevent_enable(bev, EV_READ);

	//evbuffer_add_buffer(bufferevent_get_output(bev), bufferevent_get_input(bev));
	
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

	int port = 10001;
	if (argc > 1) {
		port = atoi(argv[1]);
		if (port <= 0 || port > 65535) {
			puts("Invalid port");
			return 1;
		}
	}

	sockaddr_in sin = { 0 };
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(port);

	printf("using libevent %s\n", event_get_version());
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
