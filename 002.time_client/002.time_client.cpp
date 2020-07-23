/**
Time Protocol Client
RFC 868
*/

#ifndef _WIN32
#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
#  include <arpa/inet.h>
# endif
#include <sys/socket.h>
#else
# ifndef  _CRT_SECURE_NO_WARNINGS
#  define  _CRT_SECURE_NO_WARNINGS
# endif
# ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#  define _WINSOCK_DEPRECATED_NO_WARNINGS
# endif
# ifndef close
#  define close closesocket
# endif
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>

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

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		fprintf(stderr, "socket\n");
		return 1;
	}

	sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	if (connect(fd, (sockaddr*)&addr, sizeof(addr))) {
		fprintf(stderr, "connect failed\n");
		close(fd);
		return 1;
	}

	char buff[4];
	int recvd = 0;
	while (1) {
		int ret = recv(fd, buff + recvd, 4 - recvd, 0);
		if (ret <= 0) {
			fprintf(stderr, "recv\n");
			close(fd);
			return 1;
		}
		recvd += ret;
		if (recvd == 4) {
			break;
		}
	}

	time_t t = ntohl(*(uint32_t*)(buff));
	printf(asctime(localtime(&t)));
	close(fd);

	return 0;
}
