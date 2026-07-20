
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include "rtp.h"
#include <thread>

static int createSokcet()
{
	int socketfd;
	int on = 1;

	socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if (socketfd < 0) {
		return -1;
	}

	setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

	return socketfd;
}

static int bindSocketAddr(int socketfd,const char* ip,int port)
{ 
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	if (bind(socketfd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) < 0) {
		return -1;
	}
	
	return 0;
}

static int acceptClient(int socketfd,char*ip,int* port)
{
	int clientfd;
	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(addr));

	socklen_t size = sizeof(addr);
	/*
	* accept(
    _In_ SOCKET s,
    _Out_writes_bytes_opt_(*addrlen) struct sockaddr FAR * addr,
    _Inout_opt_ int FAR * addrlen
    );
	*/
	clientfd = accept(socketfd, (struct sockaddr*)&addr, &size);

	strcpy(ip, inet_ntoa(addr.sin_addr));
	*port = ntohs(addr.sin_port);

	return clientfd;
}

