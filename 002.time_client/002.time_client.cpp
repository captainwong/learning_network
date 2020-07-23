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
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <stdio.h>
#include <errno.h>

int main(int argc, char** argv)
{
#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(0x0201, &wsa_data);
#endif

	if (argc < 3) {
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


}
