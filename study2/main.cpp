//
// Created by sun on 10/11/21.
//

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

#define H264_FILE_NAME   "../data/test.h264"
#define SERVER_PORT      8554
#define SERVER_RTP_PORT  55532
#define SERVER_RTCP_PORT 55533
#define BUF_MAX_SIZE     (1024*1024)

struct AdtsHeader
{
    unsigned int syncword; //总是0xFFF,代表一个ADTS帧的开始，作为分界符，用于同步每帧起始位置。
    uint8_t id; // 一般用0，因为都是属于MPEG的规范。
    uint8_t layer; //always 0
    uint8_t protectionAbsent; //这里代表是否有CRC检验字段，1代表没有，0代表有。
    uint8_t profile; //代表使用哪个级别和规范的AAC，其中01代表Low Complexity(LC),其中profile等于Audio Object Type的值减1，其中所有Audio Object Type值在下面所示。
    uint8_t samplingfrequencyindex; //采样率下标，由于AAC的采样率范围是8KHz-96KHz，所以具体用那个，这个字段决定。
    uint8_t privatebit; //一般默认0即可
    uint8_t channelconfiguration; //通道配置即声道数，一般2表示立体声双声道。具体取值范围参考下表。
    uint8_t originalitycopy; // 一般默认0即可
    uint8_t home; // 一般默认0即可
    uint8_t   copyrightedidentificationbit;// 一般默认0即可
    uint8_t   copyrightedidentificationStart;// 一般默认0即可

    unsigned int aacFrameLength; //一个ADTS帧的长度包括ADTS头和AAC原始流。用AAC原始流长度+7或者9。当proection_ansent = 0 则+9proection_ansent = 1 则+7
    unsigned int aDTSBufferFullness; //0x7FF 说明是码率可变的码流。0x000代表是固定码率的码流。
    
     /* number_of_raw_data_blocks_in_frame
     * 表示ADTS帧中有number_of_raw_data_blocks_in_frame + 1个AAC原始帧
     * 所以说number_of_raw_data_blocks_in_frame == 0
     * 表示说ADTS帧中有一个AAC数据块并不是说没有。(一个AAC原始帧包含一段时间内1024个采样及相关数据)
     */
    uint8_t numberOfRawDataBlockInFrame; //2 bit
};

static int pareseAdtHeader(uint8_t* in, struct AdtsHeader*res)
{
    static int frame_number = 0;
    memset(res, 0, sizeof(*res));

    if ((in[0] == 0xFF) && ((in[1] & 0xF0) == 0xF0))
    {
        res->id = ((uint8_t)in[1] & 0x08) >> 3; //第二个字节与0x08与运算之后，获得第13位bit对应的值
        res->layer = ((uint8_t)in[1] & 0x06) >> 1;//第二个字节与0x06与运算之后，右移1位，获得第14,15位两个bit对应的值
        res->protectionAbsent = (uint8_t)in[1]&0x01;
        res->profile = ((uint8_t)in[2] & 0xc0) >> 6;
        res->samplingfrequencyindex = ((uint8_t)in[2] & 0x3C) >> 2;
        res->privatebit = ((uint8_t)in[2] & 0x02) >> 1;
        res->channelconfiguration = (((uint8_t)in[2] & 0x01) << 2) | (((unsigned int)in[3] & 0xc0) >> 6);
        res->originalitycopy = (((uint8_t)in[3] & 0x20) >> 5);
        res->home = (((uint8_t)in[3] & 0x10) >> 4);
        res->copyrightedidentificationbit = (((uint8_t)in[3] & 0x08) >> 3);
        res->copyrightedidentificationStart = (((uint8_t)in[3] & 0x04) >> 2);
        res->aacFrameLength = (((unsigned int)in[3] & 0x03)<<11) | (((unsigned int)in[4] &0xFF)<<3) | ((unsigned int)in[5] & 0xE0) >> 5;
        res->aDTSBufferFullness = (((unsigned int)in[5] & 0x1f) << 6 | ((unsigned int)in[6] & 0xfc) >> 2);
        res->numberOfRawDataBlockInFrame = ((uint8_t)in[6] & 0x03);
    }
}

static int createTcpSocket()
{
    int sockfd;
    int on = 1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return -1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

    return sockfd;
}

static int createUdpSocket()
{
    int sockfd;
    int on = 1;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        return -1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

    return sockfd;
}

static int bindSocketAddr(int sockfd, const char* ip, int port)
{
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) < 0)
        return -1;

    return 0;
}

static int acceptClient(int sockfd, char* ip, int* port)
{
    int clientfd;
    socklen_t len = 0;
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    len = sizeof(addr);

    clientfd = accept(sockfd, (struct sockaddr*)&addr, &len);
    if (clientfd < 0)
        return -1;

    strcpy(ip, inet_ntoa(addr.sin_addr));
    *port = ntohs(addr.sin_port);

    return clientfd;
}

// 校验开头是否位3个bit 001
static inline int startCode3(char* buf)
{
   
}

// 校验开始是否为4个bit 0001
static inline int startCode4(char* buf)
{
  
}

// 查找下一个开头帧startCode
static char* findNextStartCode(char* buf, int len)
{
   
}

// 第一次读500000 进去一个帧，帧用char*表示
// 获取H264文件
static int getFrameFromH264File(FILE* fp, char* frame, int size) {
    size_t rSize = fread(frame, 1, size, fp);
}

// rtp发送H264
static int rtpSendH264Frame(int serverRtpSockfd, const char* ip, int16_t port,
    struct RtpPacket* rtpPacket, char* frame, uint32_t frameSize)
{
   
}


static int handleCmd_OPTIONS(char* result, int cseq)
{
    sprintf(result, "RTSP/1.0 200 OK\r\n"
        "CSeq: %d\r\n"
        "Public: OPTIONS, DESCRIBE, SETUP, PLAY\r\n"
        "\r\n",
        cseq);

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
        "m=video 0 RTP/AVP 96\r\n"
        "a=rtpmap:96 H264/90000\r\n"
        "a=control:track0\r\n",
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

static int handleCmd_SETUP(char* result, int cseq, int clientRtpPort)
{
    sprintf(result, "RTSP/1.0 200 OK\r\n"
        "CSeq: %d\r\n"
        "Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n"
        "Session: 66334873\r\n"
        "\r\n",
        cseq,
        clientRtpPort,
        clientRtpPort + 1,
        SERVER_RTP_PORT,
        SERVER_RTCP_PORT);

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

    int serverRtpSockfd = -1, serverRtcpSockfd = -1;
    int clientRtpPort, clientRtcpPort;
    char* rBuf = (char*)malloc(BUF_MAX_SIZE);
    char* sBuf = (char*)malloc(BUF_MAX_SIZE);

    while (true) {
        int recvLen;

        recvLen = recv(clientSockfd, rBuf, BUF_MAX_SIZE, 0);
        if (recvLen <= 0) {
            break;
        }

        rBuf[recvLen] = '\0';
        printf("%s rBuf = %s \n", __FUNCTION__, rBuf);

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

                if (sscanf(line, "Transport: RTP/AVP/UDP;unicast;client_port=%d-%d\r\n",
                    &clientRtpPort, &clientRtcpPort) != 2) {
                    // error
                    printf("parse Transport error \n");
                }
            }
            line = strtok(NULL, sep);
        }

        if (!strcmp(method, "OPTIONS"))
        {
            if (handleCmd_OPTIONS(sBuf, CSeq))
            {
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

            serverRtpSockfd = createUdpSocket();
            serverRtcpSockfd = createUdpSocket();

            if (serverRtcpSockfd < 0 || serverRtpSockfd < 0)
            {
                printf("failed to create udp socket\n");
                break;
            }

            if (bindSocketAddr(serverRtcpSockfd, "0.0.0.0", SERVER_RTCP_PORT) < 0 ||
                bindSocketAddr(serverRtpSockfd, "0.0.0.0", SERVER_RTP_PORT))
            {
                printf("failed to bind addr\n");
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
        printf("sBuf = %s \n", sBuf);
        printf("%s sBuf = %s \n", __FUNCTION__, sBuf);

        send(clientSockfd, sBuf, strlen(sBuf), 0);


        //开始播放，发送RTP包
        if (!strcmp(method, "PLAY")) {

            // rtp包资源
            FILE *file = fopen(H264_FILE_NAME, "w+");

            struct RtpPacket *rtpPack = (struct RtpPacket*)malloc(500000);
            // rtp头初始化
            rtpHeaderInit(rtpPack, 0, 0, 0, RTP_VESION, RTP_PALYLOAD_TYPE_H264, 0, 0, 0, 0x88923423);

            char* frame = (char*)malloc(500000);
            int size;
            // 发送
            while (true)
            {
                //读取fp内的帧
                getFrameFromH264File(file, frame, 500000);

            }
           
        }

        memset(method, 0, sizeof(method) / sizeof(char));
        memset(url, 0, sizeof(url) / sizeof(char));
        CSeq = 0;

    }

    closesocket(clientSockfd);
    if (serverRtpSockfd) {
        closesocket(serverRtpSockfd);
    }
    if (serverRtcpSockfd > 0) {
        closesocket(serverRtcpSockfd);
    }

    free(rBuf);
    free(sBuf);

}


int main(int argc, char* argv[])
{
    // 启动windows socket start
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("PC Server Socket Start Up Error \n");
        return -1;
    }
    // 启动windows socket end

    int rtspServerSockfd;


    rtspServerSockfd = createTcpSocket();
    if (rtspServerSockfd < 0)
    {
        WSACleanup();
        printf("failed to create tcp socket\n");
        return -1;
    }

    if (bindSocketAddr(rtspServerSockfd, "0.0.0.0", SERVER_PORT) < 0)
    {
        printf("failed to bind addr\n");
        return -1;
    }

    if (listen(rtspServerSockfd, 10) < 0)
    {
        printf("failed to listen\n");
        return -1;
    }

    printf("%s rtsp://127.0.0.1:%d\n", __FILE__, SERVER_PORT);

    while (true) {
        int clientSockfd;
        char clientIp[40];
        int clientPort;

        clientSockfd = acceptClient(rtspServerSockfd, clientIp, &clientPort);
        if (clientSockfd < 0)
        {
            printf("failed to accept client\n");
            return -1;
        }

        printf("accept client;client ip:%s,client port:%d\n", clientIp, clientPort);

        doClient(clientSockfd, clientIp, clientPort);
    }
    closesocket(rtspServerSockfd);

    return 0;
}