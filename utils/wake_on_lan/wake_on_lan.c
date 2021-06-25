
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stddef.h>
#include <errno.h>
#include <getopt.h>
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

// 6 'FF', and 16 MACs 
#define WOL_PACKET_LEN (17 * 6)
#define WOL_PORT 9
#define MAC_LEN 6
#define MAC_STR_LEN 17
#define MAC_STR_BUF_LEN (MAC_STR_LEN + 1)
#define MAX_IP_LEN 16
#define DEFAULT_IP "255.255.255.255"
#define USAGE "Usage: %s [-i ip] [mac1, ...] [-h help]\n"

typedef struct mac_addr {
	unsigned char mac[MAC_LEN];
	char str[MAC_STR_BUF_LEN];
}mac_addr;

typedef struct wol_addr {
	char ip[MAX_IP_LEN];
	mac_addr* mac;
}wol_addr;

int create_wol_socket()
{
	int fd, yes = 1;
	if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		fprintf(stderr, "socket failed: %d:%s\n", errno, strerror(errno));
		return -1;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (char*)&yes, sizeof(yes)) < 0) {
		fprintf(stderr, "setsockopt failed: %d:%s\n", errno, strerror(errno));
		return -1;
	}

	return fd;
}

int send_wol_packet(const wol_addr* wol_addr, const int fd)
{
	int i;
	struct sockaddr_in addr;
	unsigned char pkt[WOL_PACKET_LEN];

	addr.sin_family = AF_INET;
	addr.sin_port = htons(WOL_PORT);

	if ((addr.sin_addr.s_addr = inet_addr(wol_addr->ip)) == 0) {
		fprintf(stderr, "invalid ip address: %s\n", wol_addr->ip);
		return -1;
	}

	memset(pkt, 0xFF, 6);
	for (i = 1; i <= 16; i++) {
		memcpy(pkt + i * 6, wol_addr->mac->mac, MAC_LEN);
	}

	if (sendto(fd, pkt, sizeof(pkt), 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "sendto %s failed: %d:%s\n", wol_addr->ip, errno, strerror(errno));
		return -1;
	}

	printf("sendto %s with WOL packet ok, MAC is %s\n", wol_addr->ip, wol_addr->mac->str);
	return 0;
}

mac_addr* read_mac_addr(const char* arg)
{
	int i;
	mac_addr* addr;
	unsigned char mac[MAC_LEN];

	if (strlen(arg) != MAC_STR_LEN || arg[2] != ':' || arg[5] != ':' || arg[8] != ':' || arg[11] != ':' || arg[14] != ':') {
		fprintf(stderr, "invalid MAC address: %s\n", arg);
		return NULL;
	}

	for (i = 0; i < MAC_LEN; i++) {
		int hi, lo;
		hi = *(arg + i * 3);
		lo = *(arg + i * 3 + 1);
		if (!isxdigit(hi) || !isxdigit(lo)) {
			fprintf(stderr, "invalid MAC address: %s\n", arg);
			return NULL;
		}
		mac[i] = (isdigit(hi) ? (hi - '0') : (toupper(hi) - 'A') + 10) << 4;
		mac[i] |= (isdigit(lo) ? (lo - '0') : (toupper(lo) - 'A') + 10) & 0x0F;
	}

	addr = (mac_addr*)malloc(sizeof(mac_addr));
	if (!addr) {
		fprintf(stderr, "malloc failed: %d:%s\n", errno, strerror(errno));
		exit(-1);
	}

	memcpy(addr->mac, mac, MAC_LEN);
	strncpy(addr->str, arg, MAC_STR_BUF_LEN);
	return addr;
}

int main(int argc, char** argv)
{
	int arg, i, fd;
	wol_addr wol;

	strncpy(wol.ip, DEFAULT_IP, MAX_IP_LEN);
	wol.mac = NULL;

	while ((arg = getopt(argc, argv, "hi:")) != -1) {
		if (arg == 'h') {
			printf(USAGE, argv[0]);
			return 0;
		} else if (arg == 'i') {
			strncpy(wol.ip, optarg, MAX_IP_LEN);
		} else {
			fprintf(stderr, USAGE, argv[0]);
			return -1;
		}
	}

	if (argc < 2) {
		fprintf(stderr, USAGE, argv[0]);
		return -1;
	}

#ifdef _WIN32
	socket_init();
#endif

	if ((fd = create_wol_socket()) < 0) {
		return -1;
	}

	for (i = optind; i < argc; i++) {
		if ((wol.mac = read_mac_addr(argv[i]))) {
			send_wol_packet(&wol, fd);
			free(wol.mac);
		}
	}

	socket_close(fd);
	return 0;
}
