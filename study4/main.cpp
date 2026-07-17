#pragma once
#pragma comment(lib,"ws2_32.lib")
#include <cstdio>
#include <cstdint>

/*
  *    0                   1                   2                   3
  *    7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0
  *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
  *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *   |                           timestamp                           |
  *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *   |           synchronization source (SSRC) identifier            |
  *   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  *   |            contributing source (CSRC) identifiers             |
  *   :                             ....                              :
  *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *
  */

struct RTPHeader {
	uint8_t version:2 ;
	uint8_t padding : 1;
	uint8_t extension : 1;
	uint8_t csrcLen : 4;
};

int main() 
{
	struct RTPHeader RTPH;
	RTPH.version = 0;
	RTPH.padding = 0;
	RTPH.extension = 0;
	RTPH.csrcLen = 2;

	printf("h1 byte: 0x%02X\n", *(uint8_t*)&RTPH);  
	return 0;
}