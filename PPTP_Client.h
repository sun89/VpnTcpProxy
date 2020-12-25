#ifndef PPTP_Client_h
#define PPTP_Client_h

//#include "Arduino.h"
#include "ESPAsyncTCP.h"
#include "SyncClient.h"
#include "GRE.h"

extern "C"
{
  #include <lwip/raw.h>
  #include <user_interface.h>
}

struct PtpPacket {
  uint8_t address;
  uint8_t control;
  uint16_t protocol;
};

struct PppLcpPacket {
  uint8_t code;
  uint8_t identifier;
  uint16_t length;
};

struct PppPapPacket {
  uint8_t code;
  uint8_t identifier;
  uint16_t length;
};




struct StartControlConnection {
  uint16_t length;
  uint16_t pptpMessageType;
  uint32_t magic;
  uint16_t controlMessageType;
  uint16_t reserved0;
  uint16_t protocolVersion;
  //uint16_t reserved1;
  uint8_t resultCode;
  uint8_t errorCode;
  uint32_t framingCapabilities;
  uint32_t bearerCapabilities;
  uint16_t maximumChannel;
  uint16_t firmwareRevision;
  char hostname[64];
  char venderString[64];
};

struct OutGoingCallRequest {
  uint16_t length;
  uint16_t pptpMessageType;
  uint32_t magic;
  uint16_t controlMessageType;
  uint16_t reserved0;
  uint16_t callId;
  uint16_t callSerialNumber;
  uint32_t minimumBps;
  uint32_t maximumBps;
  uint32_t bearerType;
  uint32_t framingType;
  uint16_t recvWindowSize;
  uint16_t processingDelay;
  uint16_t phoneNumberLength;
  uint16_t reserved1;
  char phoneNumber[64];
  char subAddress[64];
};

struct OutGoingCallReply {
  uint16_t length;
  uint16_t pptpMessageType;
  uint32_t magic;
  uint16_t controlMessageType;
  uint16_t reserved0;
  uint16_t callId;
  uint16_t peerCallId;
  uint8_t resultCode;
  uint8_t errorCode;
  uint16_t causeCode;
  uint32_t connectSpeed;
  uint16_t recvWindowSize;
  uint16_t processingDelay;
  uint16_t phyChannelId;
};


struct SetLinkInfo {
  uint16_t length;
  uint16_t pptpMessageType;
  uint32_t magic;
  uint16_t controlMessageType;
  uint16_t reserved0;
  uint16_t peerCallId;
  uint16_t reserved1;
  uint32_t sendAccm;
  uint32_t receiveAccm;
};

void PPTPC_init(const char *server, int port, const char *user, const char *password);
bool PPTPC_connect();
int PPTPC_writeData(uint8_t *data, int length);
//static int PPTPC_receiveCallback(uint8_t *data, int length);
bool PPTPC_setLinkInfo();
void PPTPC_handle();

extern ip_addr_t localIP;
extern ip_addr_t remoteIP;
extern ip_addr_t netmask;


#endif
