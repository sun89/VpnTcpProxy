#include "TcpProxyServer.h"
#include "DebugMsg.h"

#include "Arduino.h"
#include <ESP8266WiFi.h>

extern "C"
{
  #include <lwip/raw.h>
  #include <user_interface.h>
  #include <lwip/inet.h>
}

#define TcpProxyServer_printHex(level, mem, len)  db_printHex(level, mem, len)

struct raw_pcb *tcpControlBlock;
ip_addr_t destServerIP;
ip_addr_t clientAddress;
ip_addr_t myIP;
uint16_t reservedPort;
uint16_t pptpPort = 1723;

bool TcpProxyServer_setReservedPort(unsigned short port) {
  reservedPort = port;
  return true;
}

bool TcpProxyServer_setDestinationServer(const char *destServer) {
  IPAddress tmpip;
  if(WiFi.hostByName(destServer, tmpip) == false) {
    // Unable to resolve hostname
    db_printf(DB_INFO, "TcpProxyServer_setDestinationServer() Resolve Server name fail\n");
    return false;
  }
  destServerIP = tmpip;
  return true;
}

bool TcpProxyServer_begin(const char *destServer, unsigned short _reservedPort) {
  db_printf(DB_DEBUG, "TcpProxyServer_begin() Start\n");
  reservedPort = 0;
  destServerIP.addr = 0;
  clientAddress.addr = 0;
  myIP.addr = 0;

  myIP = WiFi.localIP();
  
  IPAddress tmpip;
  if(WiFi.hostByName(destServer, tmpip) == false) {
    // Unable to resolve hostname
    db_printf(DB_INFO, "TcpProxyServer_begin() Resolve Server name fail\n");
    return false;
  }
  destServerIP = tmpip;
  reservedPort = _reservedPort;

  db_printf(DB_DEBUG, "TcpProxyServer_begin() End\n");
  return true;
  
}


 
bool TCP_write(ip_addr_t *ip, pbuf *packetBuffer) {
  db_printf(DB_DEBUG, "TCP_write() Begin\n");
  
  // Finally, send the packet and register timestamp
  raw_sendto(tcpControlBlock, packetBuffer, ip);
  
  db_printf(DB_DEBUG, "TCP_write() Success\n");
  return true;
}

struct TcpPseudoHeader {
  uint32_t src;
  uint32_t dest;
  uint8_t reserved;
  uint8_t protocol;
  uint16_t tcpLength;
};

void fillChecksum(uint8_t * tcpPacket, ip_addr_t *srcIP, ip_addr_t *destIP, int length) {
  struct TcpPseudoHeader pseudoHeader;
  uint8_t *p = tcpPacket;
  uint32_t chksum;
  uint8_t *psum;
  uint16_t finalsum;
  
  db_printf(DB_DEBUG, "fillChecksum() Start\n");

  memset(&pseudoHeader, 0, sizeof(struct TcpPseudoHeader));
  
  // Clear Checksum
  p[16] = 0;
  p[17] = 0;

  pseudoHeader.src = srcIP->addr;
  pseudoHeader.dest = destIP->addr;
  pseudoHeader.protocol = 6;  // TCP
  pseudoHeader.tcpLength = htons(length);

  chksum = 0;
  psum = (uint8_t*)&pseudoHeader;
  for (int i  =0; i < 12; i+=2) {
    uint16_t chunk = (psum[0] << 8) | psum[1];
    db_printf(DB_DEBUG, "fillChecksum() HDR Word = 0x%04X\n", chunk);
    chksum += chunk;
    psum += 2;
  }
  db_printf(DB_DEBUG, "fillChecksum() Pseudo Header Sum = 0x%08X\n", chksum);
  
  psum = (uint8_t *)tcpPacket;
  while (length > 1) {
    uint16_t chunk = (*psum << 8) | *(psum + 1);
    db_printf(DB_DEBUG, "fillChecksum() TCP Word = 0x%04X\n", chunk);
    chksum += chunk;
    psum += 2;
    length -= 2;
  }
      
  //if any bytes left, pad the bytes and add
  if(length > 0) {
    uint16_t chunk = (uint16_t)(*psum) << 8;
     db_printf(DB_DEBUG, "fillChecksum() Last Word = 0x%04X\n", chunk);
    chksum += chunk;
  }
      
  //printf("All Sum = 0x%08X\n", chksum);
  db_printf(DB_DEBUG, "fillChecksum() All Sum = 0x%08X\n", chksum);
  
  finalsum = (chksum & 0xffff) + (chksum >> 16);

  //Fold 32-bit sum to 16 bits: add carrier to result
  while (chksum >> 16) {
      chksum = (chksum & 0xffff) + (chksum >> 16);
  }
  chksum = ~chksum;
  finalsum = (uint16_t)chksum;
  db_printf(DB_DEBUG, "fillChecksum() Final Sum = 0x%04X\n", finalsum);
  p[16] = finalsum >> 8;
  p[17] = finalsum & 0xff;
  
  db_printf(DB_DEBUG, "fillChecksum() End\n");
}


ip_addr_t vpnClientIP;
static uint8_t tcpReceivedStatic(void *tcp, raw_pcb *pcb, pbuf *packetBuffer, const ip_addr_t * addr) {

  db_printf(DB_DEBUG, "tcpReceivedStatic() Start\n");
  // Check parameters
  if(
    //gre == nullptr ||
    pcb == nullptr ||
    packetBuffer == nullptr ||
    addr == nullptr)
  {
    // 0 is returned to raw_recv. In this way the packet will be matched 
    // against further PCBs and/or forwarded to other protocol layers.
    db_printf(DB_DEBUG, "tcpReceivedStatic() nullptr\n");
    return 0;
  }

  // Save IPv4 header structure to read ttl value
  struct ip_hdr * ip = (struct ip_hdr *)packetBuffer->payload;
  if(ip == nullptr)
  {
    // Not free the packet, and return zero. The packet will be matched against
    // further PCBs and/or forwarded to other protocol layers.
    db_printf(DB_DEBUG, "tcpReceivedStatic() error 2\n");
    return 0;
  }

  /** Source IP address of current_header */
  ip_addr_t current_iphdr_src;


  /** Destination IP address of current_header */
  ip_addr_t current_iphdr_dest;
  ip_addr_t tmp_ip;

  struct in_addr address;
   /* copy IP addresses to aligned ip_addr_t */
  ip_addr_copy(current_iphdr_dest, ip->dest);
  ip_addr_copy(current_iphdr_src, ip->src);
    
  address.s_addr=current_iphdr_src.addr;
  db_printf(DB_DEBUG, "tcpReceivedStatic() Source Address: %s\n", inet_ntoa(address));
    
   address.s_addr=current_iphdr_dest.addr;
  db_printf(DB_DEBUG, "tcpReceivedStatic() Destinarion Address: %s\n", inet_ntoa(address));

  uint8_t *p = (uint8_t *)packetBuffer->payload;
  uint16_t destPort = (p[22] << 8) | p[23];
  uint16_t srcPort = (p[20] << 8) | p[21];
  uint16_t tcpFlag = (p[32] << 8) | p[33];
  db_printf(DB_DEBUG, "tcpReceivedStatic() Source Port: %d\n", srcPort);
  db_printf(DB_DEBUG, "tcpReceivedStatic() Destination Port: %d\n", destPort);
  db_printf(DB_DEBUG, "tcpReceivedStatic() Packet Length Before: %d\n", packetBuffer->len);
  db_printf(DB_DEBUG, "TCP Flag: ");
  if (tcpFlag & 0x0010) db_printf(DB_DEBUG, "ack,");
  if (tcpFlag & 0x0008) db_printf(DB_DEBUG, "push,");
  if (tcpFlag & 0x0004) db_printf(DB_DEBUG, "reset,");
  if (tcpFlag & 0x0002) db_printf(DB_DEBUG, "sync,");
  if (tcpFlag & 0x0001) db_printf(DB_DEBUG, "fin,");
  db_printf(DB_DEBUG, "\n");
  
  TcpProxyServer_printHex(DB_DEBUG, ip, packetBuffer->len);

  // Port  'reservedPort' reserve for Web setting
  if ((destPort != reservedPort) && (destPort != pptpPort) && (srcPort != pptpPort)) {
    pbuf_header(packetBuffer, -PBUF_IP_HLEN);
    db_printf(DB_DEBUG, "tcpReceivedStatic() Packet Length After: %d\n", packetBuffer->len);
    
    // Request From Client -> Forward to Destination server
    if (current_iphdr_src.addr != destServerIP.addr) {
      
      clientAddress.addr = current_iphdr_src.addr;
      vpnClientIP.addr = current_iphdr_dest.addr;
      fillChecksum((uint8_t *)packetBuffer->payload, &myIP, &destServerIP, packetBuffer->len);
      TcpProxyServer_printHex(DB_DEBUG, (uint8_t *)packetBuffer->payload, packetBuffer->len);
      
      TCP_write(&destServerIP, packetBuffer);

      
    
    } else {    // Response From Destination Server -> Forward back to client
    
      fillChecksum((uint8_t *)packetBuffer->payload, &vpnClientIP, &clientAddress, packetBuffer->len);
      TcpProxyServer_printHex(DB_DEBUG, (uint8_t *)packetBuffer->payload, packetBuffer->len);
      TCP_write(&clientAddress, packetBuffer);
    }

    db_printf(DB_DEBUG, "tcpReceivedStatic() End - Proxy Processs Complete\n");

    // Eat Packet
    pbuf_free(packetBuffer);
    return 1;
  } else if (destPort == reservedPort) {
    db_printf(DB_DEBUG, "tcpReceivedStatic() None Proxy (Reserved Port)\n");

  }

  db_printf(DB_DEBUG, "tcpReceivedStatic() End - No Proxy this\n");
  return 0;
}

bool TcpProxyServer_start() {
  struct in_addr address;
  
  db_printf(DB_DEBUG, "TcpProxyServer_start() Begin\n");

  if(destServerIP.addr == 0x00000000) {
    // Unable to resolve hostname
    db_printf(DB_INFO, "TcpProxyServer_start() Resolve Server name fail\n");
    return false;
  }
  
  address.s_addr=destServerIP.addr;
  db_printf(DB_DEBUG, "TcpProxyServer_start() Destination Server Address: %s\n", inet_ntoa(address));
  
  tcpControlBlock = raw_new(IP_PROTO_TCP);
  if (tcpControlBlock == nullptr) {
    db_printf(DB_DEBUG, "TcpProxyServer_start() raw_new() fail\n");
    return false;
  }

  // When LWIP detects a packet corresponding to specified protocol control
  // block, the GreReceivedStatic callback is executed
  raw_recv( tcpControlBlock, tcpReceivedStatic, NULL);

  // Selects the local interfaces where detection will be made.
  // In this case, all local interfaces
  raw_bind(tcpControlBlock, IP_ADDR_ANY);

  db_printf(DB_DEBUG, "TcpProxyServer_start() Success\n");
  return true;
}
