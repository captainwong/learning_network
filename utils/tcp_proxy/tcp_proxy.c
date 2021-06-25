/*
 * iproxy.c -- proxy that enables tcp service access to iOS devices
 *
 * Copyright (C) 2009-2020 Nikias Bassen <nikias@gmx.li>
 * Copyright (C) 2014      Martin Szulecki <m.szulecki@libimobiledevice.org>
 * Copyright (C) 2009      Paul Sladen <libiphone@paul.sladen.org>
 *
 * Based upon iTunnel source code, Copyright (c) 2008 Jing Su.
 * http://www.cs.toronto.edu/~jingsu/itunnel/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stddef.h>
#include <errno.h>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
typedef unsigned int socklen_t;
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netinet/in.h>
#include <signal.h>
#endif
#include "../../common/socket.h"

#ifndef ETIMEDOUT
#define ETIMEDOUT 138
#endif

static int debug_level = 0;

#ifdef _MSC_VER
#define strcasecmp stricmp
#endif

static uint16_t listen_port = 0;
static uint16_t remote_port = 0;
struct sockaddr_in remote_address;
char* remote_name = 0;

struct client_data {
	int fd;
	int sfd;
	volatile int stop_ctos;
	volatile int stop_stoc;
};

static void* run_stoc_loop(void* arg)
{
	struct client_data* cdata = (struct client_data*)arg;
	int recv_len;
	int sent;
	char buffer[131072];

	printf("%s: fd = %d\n", __func__, cdata->fd);

	while (!cdata->stop_stoc && cdata->fd > 0 && cdata->sfd > 0) {
		recv_len = socket_receive_timeout(cdata->sfd, buffer, sizeof(buffer), 0, 5000);
		if (recv_len <= 0) {
			if (recv_len == 0 || recv_len == -ETIMEDOUT) {
				// try again
				continue;
			} else {
				fprintf(stderr, "recv failed: %s\n", strerror(-recv_len));
				break;
			}
		} else {
			// send to socket
			sent = socket_send(cdata->fd, buffer, recv_len);
			if (sent < recv_len) {
				if (sent <= 0) {
					fprintf(stderr, "send failed: %s\n", strerror(errno));
					break;
				} else {
					fprintf(stderr, "only sent %d from %d bytes\n", sent, recv_len);
				}
			}
		}
	}

	socket_close(cdata->fd);

	cdata->fd = -1;
	cdata->stop_ctos = 1;

	return NULL;
}

static void* run_ctos_loop(void* arg)
{
	struct client_data* cdata = (struct client_data*)arg;
	int recv_len;
	int sent;
	char buffer[131072];
#ifdef _WIN32
	HANDLE stoc = NULL;
#else
	pthread_t stoc;
#endif

	printf("%s: fd = %d\n", __func__, cdata->fd);

	cdata->stop_stoc = 0;
#ifdef _WIN32
	stoc = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)run_stoc_loop, cdata, 0, NULL);
#else
	pthread_create(&stoc, NULL, run_stoc_loop, cdata);
#endif

	while (!cdata->stop_ctos && cdata->fd > 0 && cdata->sfd > 0) {
		recv_len = socket_receive_timeout(cdata->fd, buffer, sizeof(buffer), 0, 5000);
		if (recv_len <= 0) {
			if (recv_len == 0 || recv_len == -ETIMEDOUT) {
				// try again
				continue;
			} else {
				fprintf(stderr, "recv failed: %s\n", strerror(-recv_len));
				break;
			}
		} else {
			// send to local socket
			sent = socket_send(cdata->sfd, buffer, recv_len);
			if (sent < recv_len) {
				if (sent <= 0) {
					fprintf(stderr, "send failed: %s\n", strerror(errno));
					break;
				} else {
					fprintf(stderr, "only sent %d from %d bytes\n", sent, recv_len);
				}
			}
		}
	}

	socket_close(cdata->fd);

	cdata->fd = -1;
	cdata->stop_stoc = 1;

#ifdef _WIN32
	WaitForSingleObject(stoc, INFINITE);
#else
	pthread_join(stoc, NULL);
#endif

	return NULL;
}

static void* acceptor_thread(void* arg)
{
	struct client_data* cdata;
#ifdef _WIN32
	HANDLE ctos = NULL;
#else
	pthread_t ctos;
#endif

	if (!arg) {
		fprintf(stderr, "invalid client_data provided!\n");
		return NULL;
	}

	cdata = (struct client_data*)arg;
	cdata->sfd = -1;

	fprintf(stdout, "Requesting connecion to %s:%d\n", remote_name, remote_port);
	cdata->sfd = socket_connect_addr((struct sockaddr*)&remote_address, remote_port);

	if (cdata->sfd < 0) {
		fprintf(stderr, "Error connecting to device: %s\n", strerror(errno));
	} else {
		cdata->stop_ctos = 0;

#ifdef _WIN32
		ctos = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)run_ctos_loop, cdata, 0, NULL);
		WaitForSingleObject(ctos, INFINITE);
#else
		pthread_create(&ctos, NULL, run_ctos_loop, cdata);
		pthread_join(ctos, NULL);
#endif
	}

	if (cdata->fd > 0) {
		socket_close(cdata->fd);
	}
	if (cdata->sfd > 0) {
		socket_close(cdata->sfd);
	}
	free(cdata);

	return NULL;
}

static void print_usage(char** argv, int is_error)
{
	char* name = NULL;
	name = strrchr(argv[0], '/');
	name = strrchr(argv[0], '\\');
	fprintf(is_error ? stderr : stdout, "Usage: %s LOCAL_PORT REMOTE_ADDRESS [REMOTE_PORT]\n", (name ? name + 1 : argv[0]));
	fprintf(is_error ? stderr : stdout,
			"Proxy that enables TCP service access to remote devices.\n"
	);
}

int main(int argc, char** argv)
{
	int mysock = -1;
	struct hostent* host;
	
	if (argc < 3) {
		print_usage(argv, 1);
		return 2;
	}

#ifdef _WIN32
	socket_init();
#endif

	remote_port = listen_port = atoi(argv[1]);
	if (!listen_port) {
		fprintf(stderr, "Invalid listen port specified!\n");
		return -EINVAL;
	}

	remote_name = argv[2];
	host = gethostbyname(argv[2]);
	if (!host) {
		fprintf(stderr, "Invalid remote address specified!\n");
		return -1;
	}

	if (argc > 3) {
		remote_port = atoi(argv[3]);
	}
	if (!remote_port) {
		fprintf(stderr, "Invalid remote port specified!\n");
		return -EINVAL;
	}

	memset(&remote_address, 0, sizeof(remote_address));
	remote_address.sin_family = AF_INET;
	memcpy(&remote_address.sin_addr.s_addr, host->h_addr, host->h_length);
	remote_address.sin_port = htons(remote_port);

#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif
	// first create the listening socket endpoint waiting for connections.
	mysock = socket_create(listen_port);
	if (mysock < 0) {
		fprintf(stderr, "Error creating socket: %s\n", strerror(errno));
		return -errno;
	} else {
#ifdef _WIN32
		HANDLE acceptor = NULL;
#else
		pthread_t acceptor;
#endif
		struct client_data* cdata;
		int c_sock;
		while (1) {
			printf("waiting for connection\n");
			c_sock = socket_accept(mysock, listen_port);
			if (c_sock) {
				printf("accepted connection, fd = %d\n", c_sock);
				cdata = (struct client_data*)malloc(sizeof(struct client_data));
				if (!cdata) {
					socket_close(c_sock);
					fprintf(stderr, "ERROR: Out of memory\n");
					return -1;
				}
				cdata->fd = c_sock;
#ifdef _WIN32
				acceptor = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)acceptor_thread, cdata, 0, NULL);
				CloseHandle(acceptor);
#else
				pthread_create(&acceptor, NULL, acceptor_thread, cdata);
				pthread_detach(acceptor);
#endif
			} else {
				break;
			}
		}
		socket_close(c_sock);
		socket_close(mysock);
	}

	return 0;
}
