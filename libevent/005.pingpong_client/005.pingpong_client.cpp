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

#if !defined(LIBEVENT_VERSION_NUMBER) || LIBEVENT_VERSION_NUMBER < 0x02010100
#error "This version of Libevent is not supported; Get 2.1.1-alpha or later."
#endif

char* block = nullptr;
int block_size = 0;
int session_count = 0;
int session_connected = 0;
int timeout = 0;

int64_t totalBytesRead = 0;
int64_t totalBytesWrite = 0;
int64_t totalMsgRead = 0;

struct Session {
	int id = 0;
	int64_t bytesRead = 0;
	int64_t bytesWritten = 0;
	int64_t msgRead = 0;
};

void readcb(struct bufferevent* bev, void* user_data)
{
	auto input = bufferevent_get_input(bev);
	auto session = (Session*)user_data;
	int len = evbuffer_get_length(input);
	session->bytesRead += len;
	session->bytesWritten += len;
	session->msgRead++;
	evbuffer_add_buffer(bufferevent_get_output(bev), input);
}

void timer_cb(evutil_socket_t fd, short what, void* arg)
{
	auto bev = (bufferevent*)arg;
	Session* session = nullptr;
	bufferevent_getcb(bev, nullptr, nullptr, nullptr, (void**)&session);
	printf("client #%d timeout\n", session->id);

	//auto bev = (bufferevent*)arg;
	//bufferevent_free(bev);
	//evutil_closesocket(fd);

	bufferevent_disable(bev, EV_WRITE);
	// SHUT_WR
	shutdown(fd, 1);
}

void eventcb(struct bufferevent* bev, short events, void* user_data)
{
	auto session = (Session*)user_data;
	printf("eventcb #%d events=%04X\n", session->id, events);
	if (events & BEV_EVENT_CONNECTED) {
		printf("client #%d connected\n", session->id);
		if (++session_connected == session_count) {
			printf("All connected\n");
		}
		bufferevent_write(bev, block, block_size);
		auto base = bufferevent_get_base(bev);		
		auto timer = event_new(base, bufferevent_getfd(bev), EV_TIMEOUT, timer_cb, bev);
		if (!timer) {
			fprintf(stderr, "create timer failed\n");
			event_base_loopbreak(base);
			return;
		}
		struct timeval tv = { timeout, 0 };
		event_add(timer, &tv);
		return;
	} else if (events & (BEV_EVENT_EOF)) {
		printf("Connection #%d closed.\n", session->id);
	} else if (events & BEV_EVENT_ERROR) {
		printf("Got an error on the connection #%d: %s\n",
			   session->id, strerror(errno));/*XXX win32*/
	}
	/* None of the other events can happen here, since we haven't enabled
		* timeouts */

	totalBytesRead += session->bytesRead;
	totalBytesWrite += session->bytesWritten;
	totalMsgRead += session->msgRead;
	delete session;

	if (--session_connected == 0) {
		printf("All disconnected\n");
		printf("Read %.2f MiB, Write %.2f MiB, Average msg size is %.2f bytes, Throughput is %.2f MiB/s\n",
			   totalBytesRead / 1024.0 / 1024, 
			   totalBytesWrite / 1024.0 / 1024, 
			   totalBytesRead * 1.0 / totalMsgRead, 
			   totalBytesRead * 1.0 / (timeout * 1024.0 * 1024));
	}

	bufferevent_free(bev);
}

int main(int argc, char** argv)
{
#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(0x0201, &wsa_data);
#endif

	if (argc < 6) {
		printf("Usage: %s ip port session_count block_size timeout\ntimeout is in seconds\n", argv[0]);
		return 1;
	}

	auto ip = argv[1];
	int port = atoi(argv[2]);
	if (port <= 0 || port > 65535) {
		puts("Invalid port");
		return 1;
	}
	session_count = atoi(argv[3]);
	if (session_count <= 0) {
		puts("Invalid session_count");
		return 1;
	}
	block_size = atoi(argv[4]);
	if (block_size <= 0) {
		puts("Invalid block_size");
		return 1;
	}

	block = new char[block_size];
	for (int i = 0; i < block_size; i++) {
		block[i] = i % 128;
	}

	timeout = atoi(argv[5]);
	if (timeout <= 0) {
		puts("Invalid timeout");
		return 1;
	}

	printf("using libevent %s\n", event_get_version());
	printf("starting pingpong to %s:%d with %d sessions, block_size=%d bytes, timeout=%ds\n",
		   ip, port, session_count, block_size, timeout);

	sockaddr_in sin = { 0 };
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(ip);
	sin.sin_port = htons(port);

	auto base = event_base_new();
	if (!base) {
		fprintf(stderr, "init libevent failed\n");
		return -1;
	}

	for (int i = 0; i < session_count; i++) {
		auto bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
		if (!bev) {
			fprintf(stderr, "allocate bufferevent failed\n");
			return 1;
		}
		auto session = new Session();
		session->id = i;
		bufferevent_setcb(bev, readcb, nullptr, eventcb, session);
		bufferevent_enable(bev, EV_READ);

		if (bufferevent_socket_connect(bev, (sockaddr*)(&sin), sizeof(sin)) < 0) {
			fprintf(stderr, "error starting connection\n");
			return -1;
		}
	}

	event_base_dispatch(base);

	return 0;
}
