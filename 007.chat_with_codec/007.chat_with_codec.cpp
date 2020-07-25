/**
Echo Server
with codec
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
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include "chat.h"


// key:name, value:client
std::unordered_map<std::string, Client*> client_name_map = {};
// key:room id, value:room
std::unordered_map<int, Room*> rooms = {};

void handle_msg(bufferevent* bev, Client* client, Msg* msg)
{
	switch (msg->header.type) {		
	case MsgType::login:
	{
		client->name.assign(msg->data, msg->header.len);
		Msg msg;
		msg.header.type = MsgType::login_result;
		bool result = true;
		msg.header.len = sizeof(result);
		evbuffer_add(bufferevent_get_output(bev), &msg.header, MSG_HEADER_LEN);
		evbuffer_add(bufferevent_get_output(bev), &result, msg.header.len);
		break;
	}
	
	case MsgType::query_all_live_rooms:
		break;
	case MsgType::rooms:
		break;
	case MsgType::get_room_info:
		break;
	case MsgType::room:
		break;
	case MsgType::join:
		break;
	case MsgType::joined:
		break;
	case MsgType::leave:
		break;
	case MsgType::leaved:
		break;
	case MsgType::broadcast:
		break;
	case MsgType::groupcast:
		break;
	default:
		break;
	}

	delete[] msg->data;
}

void readcb(struct bufferevent* bev, void* user_data)
{
	auto me = (Client*)user_data;
	//evbuffer_add_buffer(bufferevent_get_output(bev), bufferevent_get_input(bev));
	auto input = bufferevent_get_input(bev);
	size_t readable_len = evbuffer_get_length(input);
	while (readable_len >= MSG_HEADER_LEN) {
		Msg msg;
		evbuffer_copyout(input, &msg.header, MSG_HEADER_LEN);
		if (readable_len >= msg.header.len + MSG_HEADER_LEN) {
			msg.data = new char[msg.header.len];
			evbuffer_drain(input, MSG_HEADER_LEN);
			evbuffer_remove(input, msg.data, msg.header.len);
			handle_msg(bev, me, &msg);
		}
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

	if (!me->name.empty() && me->room >= 0) { // 已加入房间，通知房间内其他人该用户离线
		auto room = rooms[me->room];
		for (auto client : room->clients) {
			if (client.second->name != me->name) {
				auto output = bufferevent_get_output(client.second->bev);
				evbuffer_add_printf(output, "#%d %s offline\n", me->fd, me->name.data());
			}
		}
		room->clients.erase(me->fd);
	} else if (!me->name.empty()) { // 已注册但未加入房间
		printf("client #%d %s offline\n", me->fd, me->name.data());
	} else { // 未注册
		printf("client #%d offline\n", me->fd);
	}

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

	char buf[64];
	sprintf(buf, "Welcome #%d!\n", fd);
	Msg msg;
	msg.header.len = strlen(buf);
	msg.header.type = MsgType::msg;
	evbuffer_add(bufferevent_get_output(bev), &msg.header, MSG_HEADER_LEN);
	evbuffer_add(bufferevent_get_output(bev), buf, msg.header.len);

	auto client = new Client();
	client->fd = fd;
	client->bev = bev;
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

	int port = 10002;
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
