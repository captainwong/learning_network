/**
Ping Pong Client
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

#include "../007.chat_with_codec/chat.h"
#include <unordered_map>

#if !defined(LIBEVENT_VERSION_NUMBER) || LIBEVENT_VERSION_NUMBER < 0x02010100
#error "This version of Libevent is not supported; Get 2.1.1-alpha or later."
#endif

Client me = {};

typedef void(*command_func)(const char* data, int len);
std::unordered_map<std::string, command_func> command_funcs = {};

#define ADD_COMMAND_FUNC(func_name) command_funcs[#func_name] = func_name;

void help(const char* data, int len)
{
	printf("Commands are:\n"
		   "  help        Show commands\n"
		   "  rooms       Get rooms list\n"
		   "  room        Get room info by id\n"
		   "  join        Join a room by id\n"
		   "  leave       Leave current room\n"
		   "  broadcast   Send a message to all online users\n"
		   "  groupcast   Send a message to all users in current room\n"
		   "  chat        Send a message to user by id\n"
		   "  quit        Quit\n");
}

void rooms(const char* data, int len)
{
	MsgHeader msg;
	msg.len = 0;
	msg.type = MsgType::rooms;
	auto output = bufferevent_get_output(me.bev);
	evbuffer_add(output, &msg, MSG_HEADER_LEN);
}

void room(const char* data, int len)
{
	int room_id = 0;
	if (sscanf(data, "room %d ", &room_id) != 1) {

	}
}

void join(const char* data, int len)
{

}

void leave(const char* data, int len)
{

}

void broadcast(const char* data, int len)
{

}

void groupcast(const char* data, int len)
{

}

void chat(const char* data, int len)
{

}

void quit(const char* data, int len)
{

}

void init_command_funcs()
{
	ADD_COMMAND_FUNC(help);
	ADD_COMMAND_FUNC(rooms);
	ADD_COMMAND_FUNC(room);
	ADD_COMMAND_FUNC(join);
	ADD_COMMAND_FUNC(leave);
	ADD_COMMAND_FUNC(broadcast);
	ADD_COMMAND_FUNC(groupcast);
	ADD_COMMAND_FUNC(chat);
	ADD_COMMAND_FUNC(quit);
}

void handle_input(bufferevent* bev)
{
	
	printf("Input command:");
	char cmd[4096];
	int buflen = sizeof(cmd);
	char* p = cmd;
	int pos = 0;
	char c = 0;
	while ((c = getchar()) != EOF) {
		if (pos == buflen) {
			buflen *= 2;
			auto newp = new char[buflen];
			memcpy(newp, p, pos);
			if (p != cmd) {
				delete[] p;
			}
			p = newp;
		}
		p[pos++] = c;
	}

	const char* commands[] = { "help", "rooms", "room", "join", "leave", "broadcast", "groupcast", "chat", nullptr };
	for (auto command : commands) {
		if (!command) { break; }
		if (strstr(p, command)) {
			command_funcs[command](p, pos);
		}
	}

	if (p != cmd) {
		delete[] p;
	}
}

void handle_msg(bufferevent* bev, Msg* msg)
{
	switch (msg->header.type) {
	case MsgType::msg:
		msg->data[msg->header.len - 1] = '\0';
		printf("%s\n", (const char*)msg->data);
		break;
	case MsgType::login_result:
	{
		bool result = (bool)msg->data[0];
		if (result) {
			printf("Login success, %s\n", me.name.c_str());
		} else {
			printf("Login failed\n");
		}
		break;
	}
	case MsgType::rooms:
		break;
	case MsgType::room:
		break;
	case MsgType::joined:
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

	if (msg->data) {
		delete[] msg->data;
	}
}

void readcb(struct bufferevent* bev, void* user_data)
{
	auto input = bufferevent_get_input(bev);
	size_t readable_len = evbuffer_get_length(input);
	while (readable_len >= MSG_HEADER_LEN) {
		Msg msg;
		evbuffer_copyout(input, &msg.header, MSG_HEADER_LEN);
		if (readable_len >= msg.header.len + MSG_HEADER_LEN) {
			msg.data = new char[msg.header.len];
			evbuffer_drain(input, MSG_HEADER_LEN);
			evbuffer_remove(input, msg.data, msg.header.len);
			handle_msg(bev, &msg);
			readable_len = evbuffer_get_length(input);
		} else {
			break;
		}
	}
}

void eventcb(struct bufferevent* bev, short events, void* user_data)
{
	printf("eventcb events=%04X\n", events);
	if (events & BEV_EVENT_CONNECTED) {
		printf("client connected\n");
		auto base = bufferevent_get_base(bev);
		Msg msg;
		msg.header.type = MsgType::login;
		msg.data = (char*)me.name.c_str();
		msg.header.len = strlen(msg.data);
		evbuffer_add(bufferevent_get_output(bev), &msg.header, MSG_HEADER_LEN);
		evbuffer_add(bufferevent_get_output(bev), msg.data, msg.header.len);
		return;
	} else if (events & (BEV_EVENT_EOF)) {
		printf("Connection closed.\n");
	} else if (events & BEV_EVENT_ERROR) {
		printf("Got an error on the connection: %s\n",
			   strerror(errno));
	}

	bufferevent_free(bev);
}

int main(int argc, char** argv)
{
#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(0x0201, &wsa_data);
#endif

	if (argc < 4) {
		printf("Usage: %s ip port username\n", argv[0]);
		return 1;
	}

	auto ip = argv[1];
	int port = atoi(argv[2]);
	if (port <= 0 || port > 65535) {
		puts("Invalid port");
		return 1;
	}
	me.name = argv[3];

	printf("using libevent %s\n", event_get_version());
	printf("starting chat client to %s:%d, username is %s\n", ip, port, argv[3]);

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
	bufferevent_setcb(bev, readcb, nullptr, eventcb, nullptr);
	bufferevent_enable(bev, EV_READ);

	if (bufferevent_socket_connect(bev, (sockaddr*)(&sin), sizeof(sin)) < 0) {
		fprintf(stderr, "error starting connection\n");
		return -1;
	}

	event_base_dispatch(base);

	return 0;
}
