#include "Arduino.h"
#include "GRE.h"
#include <ESP8266WiFi.h>

#include "DebugMsg.h"

struct GreSeqPacket *_greHeader;
char _servername[100];
ip_addr_t _serverIP;
int _serverport;
    
    
uint16_t _peerCallId;
uint8_t _readBuffer[1500];
int _readBufferLength;
bool _readAvailable;

uint32_t _ackNumber;
uint32_t _lastAckNumber;
    
struct raw_pcb *_greControlBlock;

uint16_t GRE_protocolType;
uint8_t GRE_payloadLength;
uint16_t GRE_callId;
uint32_t GRE_sequenceNumber;
int (*GRE_recvCallback)(uint8_t *, int);

bool _greSetup();
bool _pppLcpConfig();
int _getPppLcpConfigOptions(uint8_t *buf, int bufsize);
// LWIP callback run when a gre response is received (static wrapper)
static uint8_t _greReceivedStatic(void *gre, raw_pcb *pcb, pbuf *packetBuffer, const ip_addr_t * addr);
// LWIP callback run when a gre response is received
uint8_t GreReceived(pbuf * packetBuffer, const ip_addr_t * addr);


void GRE_setProtocolType(uint16_t pro) {
  GRE_protocolType = pro;
}

//void GRE_set

void _initVariable() {
  GRE_sequenceNumber = 0; //0x12345678;
  _ackNumber = 0xffffffff;
  _lastAckNumber = 0xffffffff;
  _readAvailable = false;
  GRE_recvCallback = nullptr;
}

int GRE_write(uint8_t *data, int length) {
  int packetSize = 0;
  int greSize;
  bool actField = false;
  uint16_t flag = 0;
  uint32_t *pSeqNumber;
  uint32_t *pAckNumber;
  uint8_t *pPayload;

  db_printf(DB_DEBUG, "GRE_write() Begin\n");
  
  flag = 0x3001;
  if (_ackNumber != _lastAckNumber) {
    actField = true;
    flag = 0x3081;
  }
  greSize = sizeof(struct GrePacket);
  greSize += 4;  //Write Packet need Sequence number field
  if (actField) greSize += 4; // Need for act number field
    
  packetSize = greSize + length;
  db_printf(DB_DEBUG, "GRE_write() Length = %d\n", packetSize);

  // Allocate packet buffer structure. Buffer memory is allocated as one 
  // large chunk. This includes protocol headers as well.
  struct pbuf * packetBuffer = pbuf_alloc( PBUF_IP, packetSize, PBUF_RAM);
  if(packetBuffer == nullptr) {
    db_printf(DB_DEBUG, "GRE_write() pbuf alloc fail\n");
    return -1;
  }
  
  // Check if packet buffer correctly created
  if((packetBuffer->len != packetBuffer->tot_len) ||
    (packetBuffer->next != nullptr)) {
    
    // Free packet buffer memory and exit
    db_printf(DB_DEBUG, "GRE_write() pbuf alloc not valid\n");
    pbuf_free(packetBuffer);
    return  -2;
  }

  struct GrePacket *gre = (struct GrePacket *)packetBuffer->payload;
  gre->flagsAndVersion = htons(flag);
  gre->protocolType = htons(GRE_protocolType);
  gre->payloadLength = htons(length);
  gre->callId = htons(GRE_callId);
  
  pSeqNumber = (uint32_t *)packetBuffer->payload;
  pSeqNumber += sizeof(struct GrePacket)/4;
  *pSeqNumber = htonl(GRE_sequenceNumber++);
  pAckNumber = pSeqNumber;
  if (actField == true) {
    pAckNumber += 1;
    *pAckNumber = htonl(_ackNumber);
    _lastAckNumber = _ackNumber;
  }

  pPayload = (uint8_t*)(pAckNumber + 1);
  memcpy(pPayload, data, length);  
  
  // Finally, send the packet and register timestamp
  raw_sendto(_greControlBlock, packetBuffer, &_serverIP);
  
  // Free packet buffer memory
  pbuf_free(packetBuffer);

  db_printf(DB_DEBUG, "GRE_write() Success\n");
  return length;
}

int GRE_writeAck() {
  int packetSize = 0;
  int greSize;
  bool actField = false;
  uint16_t flag = 0;
  uint32_t *pSeqNumber;
  uint32_t *pAckNumber;
  uint8_t *pPayload;
  struct GreAckPacket *gre;

  db_printf(DB_DEBUG, "GRE_writeAck() Begin\n");
  
  flag = 0x2081;
  if (_ackNumber == _lastAckNumber) {
    return 0;
  }

  // Allocate packet buffer structure. Buffer memory is allocated as one 
  // large chunk. This includes protocol headers as well.
  struct pbuf * packetBuffer = pbuf_alloc( PBUF_IP, 12, PBUF_RAM);
  if(packetBuffer == nullptr) {
    db_printf(DB_DEBUG, "GRE_writeAck() pbuf alloc fail\n");
    return -1;
  }
  
  // Check if packet buffer correctly created
  if((packetBuffer->len != packetBuffer->tot_len) ||
    (packetBuffer->next != nullptr)) {
    
    // Free packet buffer memory and exit
    db_printf(DB_DEBUG, "GRE_writeAck() pbuf alloc not valid\n");
    pbuf_free(packetBuffer);
    return  -2;
  }

  gre = (struct GreAckPacket *)packetBuffer->payload;
  gre->flagsAndVersion = htons(flag);
  gre->protocolType = htons(GRE_protocolType);
  gre->payloadLength = htons(0);
  gre->callId = htons(GRE_callId);
  gre->acknowledgmentNumber = htonl(_ackNumber);

  _lastAckNumber = _ackNumber;

  // Finally, send the packet and register timestamp
  raw_sendto(_greControlBlock, packetBuffer, &_serverIP);
  
  // Free packet buffer memory
  pbuf_free(packetBuffer);

  db_printf(DB_DEBUG, "GRE_writeAck() Success\n");
  return 12;
}

int GRE_read(uint8_t *data, int length) {
  if (_readAvailable == false) {
    return 0;
  }

  memcpy(data, _readBuffer, _readBufferLength);
  _readAvailable = false;
  int ret = _readBufferLength;
  _readBufferLength = 0;
  return ret;
}

bool GRE_init(const char *servername, int port) {

  db_printf(DB_DEBUG, "GRE_init() Begin\n");
  _initVariable();
  
  strcpy(_servername, servername);
  _serverport = port;

  IPAddress ip;
  if(WiFi.hostByName(_servername, ip) == false) {
    // Unable to resolve hostname
    db_printf(DB_DEBUG, "GRE_init() Resolve Server name fail\n");
    return false;
  }
  _serverIP = ip;

  if (_greSetup() != true) {
    db_printf(DB_DEBUG, "GRE_init() _greSetup Failed\n");
    return false;
  }

  db_printf(DB_DEBUG, "GRE_init() Success\n");
  return true;
}



bool _greSetup() {

  db_printf(DB_DEBUG, "_greSetup() Begin\n");
  
  // Create new GRE detection data
  _greControlBlock = raw_new(IP_PROTO_GRE);
  if (_greControlBlock == nullptr) {
    db_printf(DB_DEBUG, "_greSetup() raw_new() fail\n");
    return false;
  }

  // When LWIP detects a packet corresponding to specified protocol control
  // block, the GreReceivedStatic callback is executed
  raw_recv( _greControlBlock, _greReceivedStatic, NULL);

  // Selects the local interfaces where detection will be made.
  // In this case, all local interfaces
  raw_bind(_greControlBlock, IP_ADDR_ANY);

  db_printf(DB_DEBUG, "_greSetup() Success\n");
  return true;
}

//////////////////////////////////////////////////////////////////////////////
// LWIP callback run when a gre response is received (static wrapper)
static uint8_t _greReceivedStatic(void *gre, raw_pcb *pcb, pbuf *packetBuffer, const ip_addr_t * addr) {
  db_printf(DB_DEBUG, "_greReceivedStatic() Begin\n");
  
  // Check parameters
  if(
    //gre == nullptr ||
    pcb == nullptr ||
    packetBuffer == nullptr ||
    addr == nullptr)
  {
    // 0 is returned to raw_recv. In this way the packet will be matched 
    // against further PCBs and/or forwarded to other protocol layers.
    db_printf(DB_DEBUG, "_greReceivedStatic() nullptr\n");
    return 0;
  }

  db_printf(DB_DEBUG, "_greReceivedStatic() End\n");
  return GreReceived(packetBuffer, addr);
}

//////////////////////////////////////////////////////////////////////////////
// LWIP callback run when a gre response is received
uint8_t GreReceived(pbuf * packetBuffer, const ip_addr_t * addr) {
  db_printf(DB_DEBUG, "GreReceived() Begin\n");
  
  // Check parameters
  if(packetBuffer == nullptr || addr == nullptr)
  {
    // Not free the packet, and return zero. The packet will be matched against
    // further PCBs and/or forwarded to other protocol layers.
    db_printf(DB_DEBUG, "GreReceived() error 1\n");
    return 0;
  }
  
  // Save IPv4 header structure to read ttl value
  struct ip_hdr * ip = (struct ip_hdr *)packetBuffer->payload;
  if(ip == nullptr)
  {
    // Not free the packet, and return zero. The packet will be matched against
    // further PCBs and/or forwarded to other protocol layers.
    db_printf(DB_DEBUG, "GreReceived() error 2\n");
    return 0;
  }

  // Move the ->payload pointer skipping the IPv4 header of the packet with 
  // pbuf_header function. If such function fails, it returns nonzero
  if (pbuf_header(packetBuffer, -PBUF_IP_HLEN) != 0)
  {  
    // Not free the packet, and return zero. The packet will be matched against
    // further PCBs and/or forwarded to other protocol layers.
    db_printf(DB_DEBUG, "GreReceived() error 3\n");
    return 0;
  }

  struct GrePacket *greHeader =   (struct GrePacket *)packetBuffer->payload;
  if(greHeader == nullptr)
  {
    // Restore original position of ->payload pointer
    pbuf_header(packetBuffer, PBUF_IP_HLEN);
  
    // Not free the packet, and return zero. The packet will be matched against
    // further PCBs and/or forwarded to other protocol layers.
    db_printf(DB_DEBUG, "GreReceived() error 4\n");
    return 0;
  }

  db_printf(DB_DEBUG, "GreReceived() Process Header\n");
  int greHeaderSize = sizeof(GrePacket);
  bool seqField = false;
  bool ackField = false;
  uint16_t flag = ntohs(greHeader->flagsAndVersion);
  int payloadSize = 0;

  payloadSize = ntohs(greHeader->payloadLength);
  
  pbuf_header(packetBuffer, -greHeaderSize);
  
  // flag seq number present
  if ( flag & 0x1000) {
    char * pack = (char *)packetBuffer->payload;
    _ackNumber = (pack[0] << 24) | (pack[1] << 16) | (pack[2] << 8) | pack[3];
    pbuf_header(packetBuffer, -4);
  }

  // flag ack number present
  if ( flag & 0x0080) {
    pbuf_header(packetBuffer, -4);
  }

  _readAvailable = false;
  memcpy(_readBuffer, (uint8_t*)packetBuffer->payload, payloadSize);
  _readBufferLength = payloadSize;
  _readAvailable = true;

  uint8_t *cbData = nullptr;
  if (GRE_recvCallback != nullptr) {

      int cbRet = (*GRE_recvCallback)((uint8_t*)packetBuffer->payload, payloadSize);
      if (cbRet == 1) {
        GRE_write((uint8_t*)packetBuffer->payload, payloadSize);
      }
      // Just send GRE Ack
      if (cbRet == 2) {
        GRE_writeAck();
      }

  } else {
    db_printf(DB_DEBUG, "GreReceived() GRE_recvCallback is nullptr\n");
  }

  // Eat the packet by calling pbuf_free() and returning non-zero.
  // The packet will not be passed to other raw PCBs or other protocol layers.
  pbuf_free(packetBuffer);
  db_printf(DB_DEBUG, "GreReceived() End\n");
  return 1;

}

int GRE_available() {
  if (_readAvailable == false)
    return 0;
    
  return _readBufferLength;
}
