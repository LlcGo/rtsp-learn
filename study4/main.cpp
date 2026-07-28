
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


#define AAC_FILE_NAME   "../data/test-long.aac"
#define H264_FILE_NAME   "../data/test-long.h264"
#define SERVER_PORT      8554
#define BUF_MAX_SIZE     (1024*1024)


static int createTcpSocket()
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

static inline int startCode3(char* buf)
{
	if (buf[0] == 0 && buf[1] == 0 && buf[2] == 1)
		return 1;
	else
		return 0;
}

static inline int startCode4(char* buf)
{
	if (buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1)
		return 1;
	else
		return 0;
}

static char* findNextStartCode(char* buf, int len)
{
	int i;

	if (len < 3)
		return NULL;

	for (i = 0; i < len - 3; ++i)
	{
		if (startCode3(buf) || startCode4(buf))
			return buf;

		++buf;
	}

	if (startCode3(buf))
		return buf;

	return NULL;
}

static int getFrameFromH264File(FILE* fp, char* frame, int size) {
	int frameSize;

	int rSize = fread(frame, 1, size, fp);

	if (startCode3(frame) || startCode4(frame))
	{
		return -1;
	}

	char* next = findNextStartCode(frame + 3, rSize - 3);
	if (!next)
	{
		return -1;
	}
	else
	{
		frameSize = (next - frame);
		fseek(fp, (frameSize - rSize), SEEK_CUR);
	}
	
	return frameSize;
}

struct AdtsHeader {
	unsigned int syncword;  //12 bit 同步字 '1111 1111 1111'，说明一个ADTS帧的开始
	unsigned int id;        //1 bit MPEG 标示符， 0 for MPEG-4，1 for MPEG-2
	unsigned int layer;     //2 bit 总是'00'
	unsigned int protectionAbsent;  //1 bit 1表示没有crc，0表示有crc
	unsigned int profile;           //1 bit 表示使用哪个级别的AAC
	unsigned int samplingFreqIndex; //4 bit 表示使用的采样频率
	unsigned int privateBit;        //1 bit
	unsigned int channelCfg; //3 bit 表示声道数
	unsigned int originalCopy;         //1 bit
	unsigned int home;                  //1 bit

	/*下面的为改变的参数即每一帧都不同*/
	unsigned int copyrightIdentificationBit;   //1 bit
	unsigned int copyrightIdentificationStart; //1 bit
	unsigned int aacFrameLength;               //13 bit 一个ADTS帧的长度包括ADTS头和AAC原始流
	unsigned int adtsBufferFullness;           //11 bit 0x7FF 说明是码率可变的码流

	/* number_of_raw_data_blocks_in_frame
	 * 表示ADTS帧中有number_of_raw_data_blocks_in_frame + 1个AAC原始帧
	 * 所以说number_of_raw_data_blocks_in_frame == 0
	 * 表示说ADTS帧中有一个AAC数据块并不是说没有。(一个AAC原始帧包含一段时间内1024个采样及相关数据)
	 */
	unsigned int numberOfRawDataBlockInFrame; //2 bit
};


static int parseAdtsHeader(uint8_t* in, struct AdtsHeader* res) {
	static int frame_number = 0;
	memset(res, 0, sizeof(*res));

	if ((in[0] == 0xFF) && ((in[1] & 0xF0) == 0xF0))
	{
		res->id = ((unsigned int)in[1] & 0x08) >> 3;
		res->layer = ((unsigned int)in[1] & 0x06) >> 1;
		res->protectionAbsent = (unsigned int)in[1] & 0x01;
		res->profile = ((unsigned int)in[2] & 0xc0) >> 6;
		res->samplingFreqIndex = ((unsigned int)in[2] & 0x3c) >> 2;
		res->privateBit = ((unsigned int)in[2] & 0x02) >> 1;
		res->channelCfg = ((((unsigned int)in[2] & 0x01) << 2) | (((unsigned int)in[3] & 0xc0) >> 6));
		res->originalCopy = ((unsigned int)in[3] & 0x20) >> 5;
		res->home = ((unsigned int)in[3] & 0x10) >> 4;
		res->copyrightIdentificationBit = ((unsigned int)in[3] & 0x08) >> 3;
		res->copyrightIdentificationStart = (unsigned int)in[3] & 0x04 >> 2;
		res->aacFrameLength = (((((unsigned int)in[3]) & 0x03) << 11) |
			(((unsigned int)in[4] & 0xFF) << 3) |
			((unsigned int)in[5] & 0xE0) >> 5);
		res->adtsBufferFullness = (((unsigned int)in[5] & 0x1f) << 6 |
			((unsigned int)in[6] & 0xfc) >> 2);
		res->numberOfRawDataBlockInFrame = ((unsigned int)in[6] & 0x03);

		return 0;
	}
	else
	{
		printf("failed to parse adts header\n");
		return -1;
	}
}



static int rtpSendAACFrame(int clientSockfd,
	struct RtpPacket* rtpPacket, uint8_t* frame, uint32_t frameSize) {
	
	rtpPacket->payload[0] = 0x00;
	rtpPacket->payload[1] = 0x10;
	rtpPacket->payload[2] = (frameSize >> 5) & 0xFF;
	rtpPacket->payload[3] = (frameSize & 0x1F) << 3;

	memcpy(rtpPacket->payload+4, frame, frameSize);

	rtpSendPacketOverTcp(clientSockfd, rtpPacket, frameSize + 4, 0x02);

	rtpPacket->rtpHeader.seq++;

	rtpPacket->rtpHeader.timestamp += 1025;

	return 0;
}

static int rtpSendH264Frame(int clientSockfd,
	struct RtpPacket* rtpPacket, char* frame, uint32_t frameSize)
{

	return 0;
}

static int handleCmd_OPTIONS(char* result, int seq)
{
	sprintf(result, "RTSP/1.0 200 OK\r\n"
		"CSeq: %d\r\n"
		"Public: OPTIONS, DESCRIBE, SETUP, PLAY\r\n"
		"\r\n",
		seq);

	return 0;
}

static int handleCmd_DESCRIBE(char* result, int cseq, char* url)
{
	char sdp[500];
	char localIp[100];

	sscanf(url, "rtsp://%[^:]:", localIp);

	sprintf(sdp, "v=0\r\n"
		"o=- 9%ld 1 IN IP4 %s\r\n"
		"t=0 0\r\n"
		"a=control:*\r\n"
		"m=video 0 RTP/AVP/TCP 96\r\n"
		"a=rtpmap:96 H264/90000\r\n"
		"a=control:track0\r\n"
		"m=audio 1 RTP/AVP/TCP 97\r\n"
		"a=rtpmap:97 mpeg4-generic/44100/2\r\n"
		"a=fmtp:97 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210;\r\n"
		"a=control:track1\r\n",

		time(NULL), localIp);

	sprintf(result, "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
		"Content-Base: %s\r\n"
		"Content-type: application/sdp\r\n"
		"Content-length: %zu\r\n\r\n"
		"%s",
		cseq,
		url,
		strlen(sdp),
		sdp);

	return 0;
}

static int handleCmd_SETUP(char* result, int cseq)
{
	if (cseq == 3) {
		sprintf(result, "RTSP/1.0 200 OK\r\n"
			"CSeq: %d\r\n"
			"Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
			"Session: 66334873\r\n"
			"\r\n",
			cseq);
	}
	else if (cseq == 4) {
		sprintf(result, "RTSP/1.0 200 OK\r\n"
			"CSeq: %d\r\n"
			"Transport: RTP/AVP/TCP;unicast;interleaved=2-3\r\n"
			"Session: 66334873\r\n"
			"\r\n",
			cseq);
	}


	return 0;
}

static int handleCmd_PLAY(char* result, int cseq)
{
	sprintf(result, "RTSP/1.0 200 OK\r\n"
		"CSeq: %d\r\n"
		"Range: npt=0.000-\r\n"
		"Session: 66334873; timeout=10\r\n\r\n",
		cseq);

	return 0;
}

static void doClient(int clientSockfd, const char* clientIP, int clientPort) {

	char method[40];
	char url[100];
	char version[40];
	int CSeq;

	char* rBuf = (char*)malloc(BUF_MAX_SIZE);
	char* sBuf = (char*)malloc(BUF_MAX_SIZE);

	while (true) {
		int recvLen;

		recvLen = recv(clientSockfd, rBuf, BUF_MAX_SIZE, 0);
		if (recvLen <= 0) {
			break;
		}

		rBuf[recvLen] = '\0';
		printf("接收请求 rBuf = %s \n", rBuf);

		const char* sep = "\n";

		char* line = strtok(rBuf, sep);
		while (line) {
			if (strstr(line, "OPTIONS") ||
				strstr(line, "DESCRIBE") ||
				strstr(line, "SETUP") ||
				strstr(line, "PLAY")) {
				if (sscanf(line, "%s %s %s\r\n", method, url, version) != 3) {
					// error
				}
			}
			else if (strstr(line, "CSeq")) {
				if (sscanf(line, "CSeq: %d\r\n", &CSeq) != 1) {
					// error
				}
			}
			else if (!strncmp(line, "Transport:", strlen("Transport:"))) {
				// Transport: RTP/AVP/UDP;unicast;client_port=13358-13359
				// Transport: RTP/AVP;unicast;client_port=13358-13359

				if (sscanf(line, "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n") != 0) {
					// error
					printf("parse Transport error \n");
				}
			}
			line = strtok(NULL, sep);
		}

		if (!strcmp(method, "OPTIONS")) {
			if (handleCmd_OPTIONS(sBuf, CSeq))
			{
				printf("failed to handle options\n");
				break;
			}
		}
		else if (!strcmp(method, "DESCRIBE")) {
			if (handleCmd_DESCRIBE(sBuf, CSeq, url))
			{
				printf("failed to handle describe\n");
				break;
			}
		}
		else if (!strcmp(method, "SETUP")) {
			if (handleCmd_SETUP(sBuf, CSeq))
			{
				printf("failed to handle setup\n");
				break;
			}
		}
		else if (!strcmp(method, "PLAY")) {
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
		printf("响应 sBuf = %s \n", sBuf);

		send(clientSockfd, sBuf, strlen(sBuf), 0);


		//开始播放，发送RTP包
		if (!strcmp(method, "PLAY")) {
			std::thread t1([&] {
				// 封装rtp
				struct RtpPacket* rtpPacket = (struct RtpPacket*)malloc(50000);
				rtpHeaderInit(rtpPacket, 0, 0, 0, RTP_VESION, RTP_PAYLOAD_TYPE_H264, 0, 0, 0, 0x121313);
				
				// 读取h264文件
				FILE* fp = fopen(H264_FILE_NAME, "rb");
				
				// 解析h264
				char* frame = (char*)malloc(50000);
				while (true)
				{
					int frameSize = getFrameFromH264File(fp, frame, 50000);
					int startCode = 0;
					if (startCode3(frame))
					{
						startCode = 3;
					}
					else
					{
						startCode = 4;
					}

					// 发送通过rtph264
					rtpSendH264Frame(clientSockfd, rtpPacket, frame + startCode, frameSize);
					
					rtpPacket->rtpHeader.timestamp += 90000 / 25;

					Sleep(20);
				}
				free(frame);
				free(rtpPacket);
				
			});
		
			std::thread t2([&] {
				struct AdtsHeader adtHeader;
				// 构建rtp包
				struct RtpPacket* rtpPakcet = (struct RtpPacket*)malloc(5000);
				rtpHeaderInit(rtpPakcet, 0, 0, 0, RTP_VESION, RTP_PAYLOAD_TYPE_AAC, 1, 0, 0, 0x123);

				// 读取acc文件
				FILE*fp = fopen(AAC_FILE_NAME, "rb");
				uint8_t*frame = (uint8_t*)malloc(50000);
				while (true)
				{
					// 解析acc头
					fread(frame, 1, 7, fp);
					parseAdtsHeader(frame, &adtHeader);

					// 发送acc音频
					rtpSendAACFrame(clientSockfd, rtpPakcet, frame, adtHeader.aacFrameLength - 7);

					Sleep(30);
				}
				free(rtpPakcet);
				free(frame);

			});

			t1.join();
			t2.join();
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

int main(int argc, char* argv[])
{
	
	return 0;
}