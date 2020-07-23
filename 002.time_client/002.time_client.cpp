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
#include <unistd.h>
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
	printf("%s", asctime(localtime(&t)));
	close(fd);

	return 0;
}
