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
#include <string>
#pragma comment(lib, "ws2_32.lib")
#include <stdint.h>

#define SERVER_PORT      8554

#define SERVER_RTP_PORT  55532
#define SERVER_RTCP_PORT 55533

static int createSocket() {
	int sockfd = 0;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	int on = 1;
	if (sockfd < 0)
	{
		printf("create socket error");
		return -1;
	}
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
	return sockfd;
}

static int bindSokcetAddr(int sockfd, const char* ip, int port) {
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);
	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		return -1;
	}
	return 0;
}

static int acceptClient(int sockfd,char *ip,int *port) {
	int clientFd = 0;

	socklen_t len = 0;
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	len = sizeof(addr);
	
	clientFd = accept(sockfd, (struct sockaddr*)&addr, &len);
	if (clientFd < 0)
		return -1;

	strcpy(ip, inet_ntoa(addr.sin_addr));
	*port = ntohs(addr.sin_port);

	return clientFd;
}

static int handleCmd_OPTIONS(char* result, int cseq)
{
 
    return 0;
}

static int handleCmd_DESCRIBE(char* result, int cseq, char* url)
{

    return 0;
}

static int handleCmd_SETUP(char* result, int cseq, int clientRtpPort)
{
   
    return 0;
}

static int handleCmd_PLAY(char* result, int cseq)
{
  /*  sprintf(result, "RTSP/1.0 200 OK\r\n"
        "CSeq: %d\r\n"
        "Range: npt=0.000-\r\n"
        "Session: 66334873; timeout=10\r\n\r\n",
        cseq);*/

    return 0;
}

static void doClient(int clientSockfd,char * clientIp, int clientPort)
{
	char method[40];
	char url[100];
	char version[40];
	int CSeq;

	int clientRtpPort, clientRtcpPort;
	char* rBuf = (char*)malloc(10000);
	char* sBuf = (char*)malloc(10000);

	while (true)
	{
		int recvLen;

		recvLen = recv(clientSockfd, rBuf, 2000, 0);
		if (recvLen <= 0) {
			break;
		}

		rBuf[recvLen] = '\0';
		std::string recvStr = rBuf;
		printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
		printf("%s rBuf = %s \n", __FUNCTION__, rBuf);

		const char* sep = "\n";
		char* line = strtok(rBuf, sep);
		while (line) {
			if (strstr(line, "OPTIONS") || strstr(line, "DESCRIBE") 
				|| strstr(line,"SETUP") || strstr(line,"PLAY"))
			{
				if (sscanf(line, "%s %s %s\r\n", method, url, version) != 3) 
				{
					 // error
				}
			}
			else if(strstr(line,"CSeq"))
			{
				if (sscanf(line, "%CSeq : %d\r\n", &CSeq) != 1)
				{
					//error
				 }
			}else if (!strncmp(line,"Transport:",strlen("Transport:")))
			{
				// Transport: RTP/AVP/UDP;unicast;client_port=13358-13359
				// Transport: RTP/AVP;unicast;client_port=13358-13359
				if (sscanf(line, "Transport: RTP/AVP/UDP;unicast;client_port=%d-%d\r\n", &clientRtpPort, &clientRtcpPort) != 2)
				{
					// error
					printf("parse Transport error \n");
				}
			}
			line = strtok(NULL, sep);
		}

		if (!strcmp(method, "OPTIONS"))
		{
			if (handleCmd_OPTIONS(sBuf, CSeq)) {
				printf("failed to handle options\n");
				break;
			}
		}
		else if (!strcmp(method, "DESCRIBE"))
		{
			if (handleCmd_DESCRIBE(sBuf, CSeq, url))
			{
				printf("failed to handle describe\n");
				break;
			}
		}
		else if (!strcmp(method, "SETUP"))
		{
			if (handleCmd_SETUP(sBuf, CSeq, clientRtpPort)) 
			{
				printf("failed to handle setup\n");
				break;
			}
		}
		else if (!strcmp(method, "PLAY"))
		{
			if (handleCmd_PLAY(sBuf, CSeq))
			{
				printf("failed to handle play\n");
				break;
			}
		}
		else {
			printf("未定义的method = %s \n", method);
			break;
		}

		printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
		printf("%s sBuf = %s \n", __FUNCTION__, sBuf);

		send(clientSockfd, sBuf, strlen(sBuf), 0);

		// 开始播放,发送RTP包
		if (!strcmp(method, "PLAY")) {
			printf("start play\n");
			printf("client ip:%s\n", clientIp);
			printf("client port %d\n", clientRtpPort);

			while (true)
			{
				Sleep(40);
			}

			break;
		}

		memset(method, 0, sizeof(method) / sizeof(char));
		memset(url, 0, sizeof(url) / sizeof(char));
		CSeq = 0;
	}

	closesocket(clientSockfd);
	free(rBuf);
	free(sBuf);
}

int main()
{

	// 启动windows socket start
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		printf("PC Server Socket Start Up Error \n");
		return -1;
	}
	// 启动windows socket end

	int serverSockfd;

	serverSockfd = createSocket();
	if (serverSockfd < 0)
	{
		WSACleanup();
		printf("failed to create tcp socket\n");
		return -1;
	}

	if (bindSokcetAddr(serverSockfd, "0.0.0.0", SERVER_PORT) < 0)
	{
		printf("failed to bind addr\n");
		return -1;
	}

	if (listen(serverSockfd, 10) < 0)
	{
		printf("failed to listen\n");
		return -1;
	}

	printf("%s rtsp://127.0.0.1:%d\n", __FILE__, SERVER_PORT);

	while (true)
	{
		int clientFd;
		char clientIp[40];
		int clientPort;
		
		clientFd = acceptClient(serverSockfd, clientIp, &clientPort);
		if (clientFd < 0)
		{
			printf("failed to accept client\n");
			return -1;
		}

		printf("accept client;client ip:%s,client port:%d\n", clientIp, clientPort);

		doClient(clientFd, clientIp, clientPort);
	}

	closesocket(serverSockfd);
	return 0;

}

