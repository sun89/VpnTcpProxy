#include "Arduino.h"
#include "PPTP_Client.h"
#include <ESP8266WiFi.h>
#include "ESPAsyncTCP.h"
#include "SyncClient.h"
#include "md5.h"
#include "DebugMsg.h"
#include "MSCHAP.h"

void printIP(ip_addr_t ip) {
  printf("%d.", ip.addr &0xff);
  printf("%d.", (ip.addr >> 8) & 0xff);
  printf("%d.", (ip.addr >> 16) & 0xff);
  printf("%d", (ip.addr >> 24) & 0xff);
  
  
  
  fflush(stdout);
}

#define PPTPC_printHexDB(level, mem, len) db_printHex(level, mem, len)

struct StartControlConnection _pptpStartConn;
struct OutGoingCallRequest _pptpOutCallReq;
struct OutGoingCallReply _pptpOutCallReply;
struct SetLinkInfo _pptpSetLinkInfo;
    
SyncClient _syncClient;
bool _pptpConnected;
char PPTPC_servername[100];
ip_addr_t PPTPC_serverIP;
int PPTPC_serverport;
char PPTPC_username[50];
char PPTPC_password[50];

uint16_t PPTPC_authenProtocol;
#define CHAP_MD5      0x05
#define CHAP_MSCHAP1  0x80
#define CHAP_MSCHAP2  0x81
uint8_t PPTPC_authenChapMode;
uint16_t PPTPC_callId;
uint16_t PPTPC_peerCallId;
uint8_t PPTPC_chapIdentifier;
uint8_t PPTPC_chapChallenge[256];
int PPTPC_chapChallengeSize;
bool PPTPC_chapAuthenStatus;
bool PPTPC_chapAuthenResponse;
md5_context_t PPTPC_md5Context;
MSCHAP_CTX mschap_ctx;

static bool lcpComplete;
static bool papComplete;
static bool papLoginState;
static bool cbcpComplete;
static int mplsIdentifier;
ip_addr_t localIP;
ip_addr_t remoteIP;
ip_addr_t netmask;

uint32_t waitTimeoutMs = 5000;
    
void PPTPC_buildStartControlConnectionRequest(struct StartControlConnection *req);
void PPTPC_buildOutGoingCallRequest(struct OutGoingCallRequest *req);
void PPTPC_buildSetLinkInfoRequest(struct SetLinkInfo *req);
bool PPTPC_startControlConnection();
bool PPTPC_outGoingCall();
    
bool PPTPC_pppLcpConfig();
bool PPTPC_pppPap();
bool PPTPC_pppCbcp();
bool PPTPC_pppIpcp();
bool PPTPC_pppMplscp();
int PPTPC_pppPapOptions(uint8_t *buff, char *username, char *password);
int PPTPC_getPppLcpConfigOptions(uint8_t *buf, int bufsize);

bool PPTPC_pptpInterfaceInit();

static int PPTPC_receiveCallback(uint8_t *data, int length);
 
void PPTPC_init(const char *server, int port, const char *user, const char *password) {
  _pptpConnected = false;
  papComplete = false;
  lcpComplete = false;
  papLoginState = false;
  cbcpComplete = false;
  mplsIdentifier = -1;
  PPTPC_chapChallengeSize = 0;
  PPTPC_chapAuthenStatus = false;
  PPTPC_chapAuthenResponse = false;
  PPTPC_authenChapMode = 0;

  localIP.addr = 0x00000000;
  remoteIP.addr = 0x0000000;
  netmask.addr = 0xffffffff;
  strcpy(PPTPC_servername, server);
  PPTPC_serverport = port;
  strcpy(PPTPC_username, user);
  strcpy(PPTPC_password, password);

  IPAddress ip;
  if(WiFi.hostByName(PPTPC_servername, ip) == false) {
    // Unable to resolve hostname
    db_printf(DB_INFO, "Resolve Server name fail\n");
    return;
  }
  PPTPC_serverIP = ip;

}

void PPTPC_reinit() {
  _pptpConnected = false;
  papComplete = false;
  lcpComplete = false;
  papLoginState = false;
  cbcpComplete = false;
  mplsIdentifier = -1;
  PPTPC_chapChallengeSize = 0;
  PPTPC_chapAuthenStatus = false;
  PPTPC_chapAuthenResponse = false;
  PPTPC_authenChapMode = 0;
  
  localIP.addr = 0x00000000;
  remoteIP.addr = 0x0000000;
  netmask.addr = 0xffffffff;
  
}

bool PPTPC_startControlConnection() {
  struct StartControlConnection *conn;

  db_printf(DB_DEBUG, "PPTPC_startControlConnection() Begin\n");
  conn = &_pptpStartConn;
  PPTPC_buildStartControlConnectionRequest(conn);
  
  if(_syncClient.write((uint8_t*)conn, sizeof(struct StartControlConnection)) <= 0){
    db_printf(DB_DEBUG, "PPTPC_startControlConnection() Write state 1 fail\n");
    return false;
    
  }

  while(_syncClient.connected() && _syncClient.available() < 156){
      delay(10);
  }

  if (_syncClient.available() != 156) {
    db_printf(DB_DEBUG, "PPTPC_startControlConnection() Expect 156 returnn but %d\n", _syncClient.available());
    return false;
  }

  memset(conn, 0, sizeof(struct StartControlConnection));
  _syncClient.read((uint8_t*)conn, sizeof(_pptpStartConn));
  
  db_printf(DB_DEBUG, "Server Hostname: %s\n", conn->hostname);
  db_printf(DB_DEBUG, "Server Vender: %s\n", conn->venderString);

  if (conn->resultCode != 1) {
    db_printf(DB_DEBUG, "PPTPC_startControlConnection() result code error %d\n", conn->resultCode);
    return false;
  }

  db_printf(DB_DEBUG, "PPTPC_startControlConnection() Success\n");
  return true;
}

bool PPTPC_outGoingCall() {
  struct OutGoingCallRequest *conn;
  struct OutGoingCallReply *resp;

  db_printf(DB_DEBUG, "PPTPC_outGoingCall() Begin\n");
  
  conn = &_pptpOutCallReq;
  resp = &_pptpOutCallReply;
  
  PPTPC_buildOutGoingCallRequest(conn);
  db_printf(DB_DEBUG, "PPTPC_outGoingCall() Use Call ID: %d\n", ntohs(conn->callId));
  
  if(_syncClient.write((uint8_t*)conn, sizeof(struct OutGoingCallRequest)) <= 0){
    db_printf(DB_DEBUG, "PPTPC_outGoingCall() Write state 1 fail\n");
    return false;
    
  }

  while(_syncClient.connected() && _syncClient.available() < 32){
      delay(10);
  }

  if (_syncClient.available() != 32) {
    db_printf(DB_DEBUG, "PPTPC_outGoingCall() Expect 32 returnn but %d\n", _syncClient.available());
    return false;
  }

   memset(resp, 0, sizeof(struct OutGoingCallReply));
  _syncClient.read((uint8_t*)resp, sizeof(OutGoingCallReply));
  
  if (resp->resultCode != 1) {
    db_printf(DB_DEBUG, "PPTPC_outGoingCall() result code error %d\n", resp->resultCode);
    return false;
  }

  PPTPC_peerCallId = ntohs(resp->callId);
  db_printf(DB_DEBUG, "PPTPC_outGoingCall() Server(peer) Call ID: %d\n", PPTPC_peerCallId);
  db_printf(DB_DEBUG, "PPTPC_outGoingCall() Success\n");
  return true;
}

bool PPTPC_setLinkInfo() {
  struct SetLinkInfo *conn;
  struct SetLinkInfo *resp;

  db_printf(DB_DEBUG, "PPTPC_setLinkInfo() Begin\n");
  
  conn = &_pptpSetLinkInfo;
  resp = &_pptpSetLinkInfo;
  
  PPTPC_buildSetLinkInfoRequest(conn);
  
  if(_syncClient.write((uint8_t*)conn, sizeof(struct SetLinkInfo)) <= 0){
    db_printf(DB_DEBUG, "PPTPC_setLinkInfo() Write state 1 fail\n");
    return false;
    
  }

  while(_syncClient.connected() && _syncClient.available() < 24){
      delay(10);
  }

  if (_syncClient.available() != 24) {
    db_printf(DB_DEBUG, "PPTPC_setLinkInfo() Expect 24 returnn but %d\n", _syncClient.available());
    return false;
  }

   memset(resp, 0, sizeof(struct SetLinkInfo));
  _syncClient.read((uint8_t*)resp, sizeof(SetLinkInfo));
  
  if (ntohs(resp->peerCallId) != PPTPC_callId) {
    db_printf(DB_DEBUG, "PPTPC_setLinkInfo() callId error %d\n", ntohs(resp->peerCallId));
    return false;
  }

  db_printf(DB_DEBUG, "PPTPC_setLinkInfo() Success\n");
  return true;
}

int PPTPC_getPppLcpConfigOptions(uint8_t *buf, int bufsize) {
  buf[0] = 0x01;  // Max Receive Unit
  buf[1] = 0x04;  // length = 4
  buf[2] = 0x05;  // |
  buf[3] = 0x78;  // |- 1400

  buf[4] = 0x05;  // Magic Number
  buf[5] = 0x06;  // length = 6
  buf[6] = 0x1c;  // |
  buf[7] = 0xb4;  // |
  buf[8] = 0x54;  // |
  buf[9] = 0x69;  // | - 0x1cb45469

  buf[10] = 0x0d; // Callback
  buf[11] = 0x03; // length = 3
  buf[12] = 0x06; // Operation: Location is determined during CBCP negotiation(6)

  return 13;
}

bool PPTPC_pppLcpConfig() {
  int greSize;
  int packetSize = 0;
  uint8_t lcpOptions[30];
  int lcpOptionsLength;
  struct PtpPacket *ptp;
  struct PppLcpPacket *lcp;

  db_printf(DB_DEBUG, "PPTPC_pppLcpConfig() Begin\n");
  memset(lcpOptions, 0, 30);
  
  ptp = (struct PtpPacket*)lcpOptions;
  ptp->address = 0xff;
  ptp->control = 0x03;
  ptp->protocol = htons(0xc021);  // LCP Protocol

  lcpOptionsLength = PPTPC_getPppLcpConfigOptions(&lcpOptions[8], 30);
  
  lcp = (struct PppLcpPacket*)&lcpOptions[4];
  lcp->code = 0x01; // config request
  lcp->identifier = 0x00; //id
  lcp->length = htons(lcpOptionsLength + 4);
  
  int ret = GRE_write(lcpOptions, lcpOptionsLength + 4 + 4);
  db_printf(DB_DEBUG, "PPTPC_pppLcpConfig() GRE_write Length = %d\n", ret);

  uint32_t tm = millis();
  while (lcpComplete != true) {
    delay(100);
    db_printf(DB_DEBUG, ". ");
    if (millis() - tm > waitTimeoutMs) {
      db_printf(DB_DEBUG, "PPTPC_pppLcpConfig() lcpComplete Wait timeout!!!\n");
      break;
    }
  }

  if (lcpComplete == true) {
    db_printf(DB_DEBUG, "PPTPC_pppLcpConfig() Success\n");
    return true;
  }

  db_printf(DB_DEBUG, "PPTPC_pppLcpConfig() Fail\n");
  return false;  
}

int PPTPC_pppPapOptions(uint8_t *buff, char *username, char *password) {
  int len;
  len = 1 + strlen(username) + 1 + strlen(password);
  buff[0] = strlen(username);
  buff++;
  memcpy(buff, username, strlen(username));
  buff += strlen(username);
  buff[0] = strlen(password);
  buff++;
  memcpy(buff, password, strlen(password));
 
  return len;
}

bool PPTPC_pppChapMd5() {
  uint8_t md5Result[16];
  int pppSize;
  int chapSize;
  uint8_t buff[50];
  struct PtpPacket *ptp;

  db_printf(DB_DEBUG, "PPTPC_pppChapMd5() Begin\n");
  memset(buff, 0, 50);
  
  db_printf(DB_DEBUG, "PPTPC_pppChapMd5() Wait for chap challenge\n");
  while (PPTPC_chapChallengeSize <= 0) {
    db_printf(DB_DEBUG, "+ ");
    delay(100);
  }

  db_printf(DB_DEBUG, "PPTPC_pppChapMd5() Calc MD5\n");
  MD5Init(&PPTPC_md5Context);
  MD5Update(&PPTPC_md5Context, (uint8_t*)&PPTPC_chapIdentifier, 1);
  MD5Update(&PPTPC_md5Context, (uint8_t*)PPTPC_password, strlen(PPTPC_password));
  MD5Update(&PPTPC_md5Context, (uint8_t*)PPTPC_chapChallenge, PPTPC_chapChallengeSize);
  MD5Final(md5Result, &PPTPC_md5Context);

  db_printf(DB_DEBUG, "PPTPC_pppChapMd5() CHAP response...\n");
  PPTPC_printHexDB(DB_DEBUG, md5Result, 16);
  chapSize = 1+1+2+1+16+strlen(PPTPC_username);
  pppSize = 4 + chapSize;
  buff[0] = 0xff;
  
  buff[1] = 0x03;
  
  buff[2] = 0xc2;
  buff[3] = 0x23;

  buff[4] = 0x02;

  buff[5] = PPTPC_chapIdentifier;

  buff[6] = chapSize >> 8;
  buff[7] = chapSize & 0xff;

  buff[8] = 16;

  int i;
  for (i=0; i< 16; i++) {
    buff[i+9] = md5Result[i];
  }
  
  memcpy(&buff[i +9], PPTPC_username, strlen(PPTPC_username));

  int ret = GRE_write(buff, pppSize);
  db_printf(DB_DEBUG, "PPTPC_pppChapMd5() GRE_write length = %d\n", ret);

  uint32_t tm = millis();
  while (PPTPC_chapAuthenResponse != true) {
    delay(100);
    db_printf(DB_DEBUG, "+ ");
    if (millis() - tm > 10000) {
      db_printf(DB_DEBUG, "Timout!!! ");
      break;
    }
  }

  if (PPTPC_chapAuthenStatus == true) {
    db_printf(DB_DEBUG, "PPTPC_pppChapMd5() Login Complete\n");
    return true;
  }

  db_printf(DB_DEBUG, "PPTPC_pppChapMd5() Login Fail\n");
  return false;
}

bool PPTPC_pppChapMsChap() {
  uint8_t chapResponse[100];
  int pppSize;
  int chapSize;
  uint8_t buff[100];
  struct PtpPacket *ptp;

  db_printf(DB_DEBUG, "PPTPC_pppChapMsChap() Begin\n");
  memset(buff, 0, 200);
  
  db_printf(DB_DEBUG, "PPTPC_pppChapMsChap() Wait for chap challenge\n");
  while (PPTPC_chapChallengeSize <= 0) {
    db_printf(DB_DEBUG, "+ ");
    delay(100);
  }
  
  db_printf(DB_DEBUG, "PPTPC_pppChapMsChap() Calc MSCHAP2\n");
  MSCHAP_GetResponse(&mschap_ctx, PPTPC_username, PPTPC_password, chapResponse);
  db_printf(DB_DEBUG, "PPTPC_pppChapMsChap() CHAP response...\n");
  PPTPC_printHexDB(DB_DEBUG, chapResponse, 49);
  chapSize = 1+1+2+1+49+strlen(PPTPC_username);
  pppSize = 4 + chapSize;
  buff[0] = 0xff;
  
  buff[1] = 0x03;
  
  buff[2] = 0xc2;
  buff[3] = 0x23;

  buff[4] = 0x02;

  buff[5] = PPTPC_chapIdentifier;

  buff[6] = chapSize >> 8;
  buff[7] = chapSize & 0xff;

  buff[8] = 49;

  int i;
  for (i=0; i< 49; i++) {
    buff[i+9] = chapResponse[i];
  }
  
  memcpy(&buff[i +9], PPTPC_username, strlen(PPTPC_username));

  int ret = GRE_write(buff, pppSize);
  db_printf(DB_DEBUG, "PPTPC_pppChapMsChap() GRE_write length = %d\n", ret);

  uint32_t tm = millis();
  while (PPTPC_chapAuthenResponse != true) {
    delay(100);
    db_printf(DB_DEBUG, "+ ");
    if (millis() - tm > 10000) {
      db_printf(DB_DEBUG, "Timout!!! ");
      break;
    }
  }

  if (PPTPC_chapAuthenStatus == true) {
    db_printf(DB_DEBUG, "PPTPC_pppChapMsChap() Login Complete\n");
    return true;
  }

  db_printf(DB_DEBUG, "PPTPC_pppChapMsChap() Login Fail\n");
  return false;
}

bool PPTPC_pppPap() {
  int pppSize;
  int papSize;
  uint8_t buff[50];
  struct PtpPacket *ptp;
  struct PppPapPacket *pap;

  db_printf(DB_DEBUG, "PPTPC_pppPap() Begin\n");
  
  memset(buff, 0, 50);

  papSize = 4 + PPTPC_pppPapOptions(&buff[8], PPTPC_username, PPTPC_password);
  pppSize = 4 + papSize;
  
  ptp = (struct PtpPacket*)buff;
  ptp->address = 0xff;
  ptp->control = 0x03;
  ptp->protocol = htons(0xc023);  // PPP PAP Protocol

  pap = (struct PppPapPacket*)&buff[4];
  pap->code = 0x01; // authen request
  pap->identifier = 0x00; //id
  pap->length = htons(papSize);

  int ret = GRE_write(buff, pppSize);
  db_printf(DB_DEBUG, "PPTPC_pppPap() GRE_write length = %d\n", ret);

  while (papComplete != true) {
    delay(100);
    db_printf(DB_DEBUG, ". ");
  }

  if (papLoginState == true) {
    db_printf(DB_DEBUG, "PPTPC_pppPap() Login Complete\n");
    return true;

  }

  db_printf(DB_DEBUG, "PPTPC_pppPap() Login Fail (user/pass Wrong)\n");
  return false;
}


bool PPTPC_pppCbcp() {
  db_printf(DB_DEBUG, "PPTPC_pppCbcp() Begin\n");
  while (cbcpComplete != true) {
    delay(100);
    db_printf(DB_DEBUG, ". ");
  }

  db_printf(DB_DEBUG, "PPTPC_pppCbcp() Complete\n");
  return true;
}

bool PPTPC_pppIpcp() {
  int pppSize;
  int ipcpSize;
  uint8_t buff[50], *options;
  struct PtpPacket *ptp;
  struct PppPapPacket *ipcp;
  int ret;

  db_printf(DB_DEBUG, "PPTPC_pppIpcp() Begin\n");
  
  while (remoteIP.addr == 0x00000000) {
    delay(100);
    db_printf(DB_DEBUG, ". ");
  }

  db_printf(DB_DEBUG, "PPTPC_pppIpcp() Remote IP Address: %d.%d.%d.%d\n", remoteIP.addr & 0xff, (remoteIP.addr >> 8) & 0xff, (remoteIP.addr >> 16) & 0xff, (remoteIP.addr >> 24) & 0xff);
  db_printf(DB_DEBUG, "PPTPC_pppIpcp() Get Remote IP Complete\n");

  memset(buff, 0, 50);

  options = &buff[8];
  *options++ = 0x03;  // type ip
  *options++ = 6;     // ip option length
  

  ipcpSize = 4 + 6;
  pppSize = 4 + ipcpSize;
  
  ptp = (struct PtpPacket*)buff;
  ptp->address = 0xff;
  ptp->control = 0x03;
  ptp->protocol = htons(0x8021);  // PPP IPCP Protocol

  ipcp = (struct PppPapPacket*)&buff[4];
  ipcp->code = 0x01; // authen request
  ipcp->identifier = 0x00; //id
  ipcp->length = htons(ipcpSize);

  ret = GRE_write(buff, pppSize);
  db_printf(DB_DEBUG, "PPTPC_pppIpcp() GRE_write length=%d\n", ret);

  while (localIP.addr == 0x00000000) {
    delay(100);
    db_printf(DB_DEBUG, ". ");
  }

  db_printf(DB_DEBUG, "PPTPC_pppIpcp() Local IP Address: %d.%d.%d.%d\n", localIP.addr & 0xff, (localIP.addr >> 8) & 0xff, (localIP.addr >> 16) & 0xff, (localIP.addr >> 24) & 0xff);
  db_printf(DB_DEBUG, "PPTPC_pppIpcp() Success\n");
  return true;
}

bool PPTPC_pppMplscp() {
  int pppSize;
  int mplscpSize;
  uint8_t buff[50], *options;
  struct PtpPacket *ptp;
  struct PppPapPacket *mplscp;
  int ret;

  db_printf(DB_DEBUG, "PPTPC_pppMplscp() Begin\n");
  
  while (mplsIdentifier < 0) {
    delay(100);
    db_printf(DB_DEBUG, ". ");
  }

  db_printf(DB_DEBUG, "PPTPC_pppMplscp() Receive MPLSCP ID: %d\n", mplsIdentifier);

  //Next sent Protocol reject for MPLS
  memset(buff, 0, 50);

  options = &buff[10];
  *options++ = 0x01;  // Config request
  *options++ = mplsIdentifier;     // id
  *options++ = 0x00;  // |
  *options++ = 0x04;  // |- length
  

  mplscpSize = 6 + 4;
  pppSize = 4 + mplscpSize;
  
  ptp = (struct PtpPacket*)buff;
  ptp->address = 0xff;
  ptp->control = 0x03;
  ptp->protocol = htons(0xc021);  // PPP LCP Protocol

  mplscp = (struct PppPapPacket*)&buff[4];
  mplscp->code = 0x08; // Protocol reject
  mplscp->identifier = 0x10; //id
  mplscp->length = htons(mplscpSize);
  buff[8] = 0x82; // |
  buff[9] = 0x81; // |- mplscp

  ret = GRE_write(buff, pppSize);
  db_printf(DB_DEBUG, "PPTPC_pppMplscp() GRE_write length=%d\n", ret);
  
  db_printf(DB_DEBUG, "PPTPC_pppMplscp() Success\n");
  return true;
}

int PPTPC_writeData(uint8_t *data, int length) {
  int pppSize;
  int ipv4Size;
  uint8_t buff[1450], *p;
  struct PtpPacket *ptp;
  int ret;
  
  db_printf(DB_DEBUG, "PPTPC_writeData() Begin\n");
  
  memset(buff, 0, 1450);  

  ipv4Size = length;
  pppSize = 4 + ipv4Size;
  
  ptp = (struct PtpPacket*)buff;
  ptp->address = 0xff;
  ptp->control = 0x03;
  ptp->protocol = htons(0x0021);  // PPP IPv4 Protocol

  p = &buff[4];
  memcpy(p, data, length);

  ret = GRE_write(buff, pppSize);
  db_printf(DB_DEBUG, "PPTPC_writeData() GRE_write len = %d Success\n", ret);

  
  return length;
}

#define PPTP_IF_TASK_PRIO 1
#define PPTP_IF_TASK_QUEUE_SIZE 2
os_event_t pptpInterfaceTaskQueue[PPTP_IF_TASK_QUEUE_SIZE];

struct netif pptpLwip_netif;

//ESP stack -> GRE
err_t ICACHE_FLASH_ATTR PPTPC_interfaceOutput(struct netif *netif, struct pbuf *p, const ip_addr_t *ipaddr) {
  uint8_t *data = (uint8_t*)p->payload;

  db_printf(DB_DEBUG, "PPTPC_interfaceOutput() ESP->VPN Length=%d Begin\n", p->len);
  
  if (data[9] == 0x2f) {
    db_printf(DB_DEBUG, "PPTPC_interfaceOutput() skip GRE\n");
    return 0;
  }
  
  PPTPC_printHexDB( DB_DEBUG, (uint8_t*)p->payload, p->len); 
  db_printf(DB_DEBUG, "\n");
    
  PPTPC_writeData((uint8_t*)p->payload, p->len);
  db_printf(DB_DEBUG, "PPTPC_interfaceOutput() Success\n");
  return 0;
}

// GRE -> ESP Stack
void ICACHE_FLASH_ATTR PPTPC_interfaceInput(uint8_t *data, uint32_t len) {

  db_printf(DB_DEBUG, "PPTPC_interfaceInput() VPN->ESP Length=%d Begin\n", len);

  struct pbuf *pb;

  pb = pbuf_alloc(PBUF_LINK, len, PBUF_RAM);
  if (pb == NULL) {
    db_printf(DB_DEBUG, "PPTPC_interfaceInput() pbuf_alloc is null\n");
    return;
  }

  PPTPC_printHexDB( DB_DEBUG, data, len); 
    
  pbuf_take(pb, data, len);
  // Post it to the send task
  system_os_post(PPTP_IF_TASK_PRIO, 0, (os_param_t)pb);
  db_printf(DB_DEBUG, "PPTPC_interfaceInput() Success\n");
}

err_t ICACHE_FLASH_ATTR PPTPC_pptpLwipInit(struct netif *netif) {
  db_printf(DB_DEBUG, "PPTPC_pptpLwipInit() Begin\n");
  //NETIF_INIT_SNMP(netif, snmp_ifType_other, 0);
  netif->name[0] = 'p';
  netif->name[1] = 't';

  netif->output = PPTPC_interfaceOutput;
  netif->mtu = 1400;
  netif->flags = NETIF_FLAG_LINK_UP;

  db_printf(DB_DEBUG, "PPTPC_pptpLwipInit() Success\n");
  return 0;
}

static void ICACHE_FLASH_ATTR pptpInterfaceTask(os_event_t *e) {
  db_printf(DB_DEBUG, "pptpInterfaceTask() Begin\n");
  struct pbuf *pb = (struct pbuf *)e->par;
  if (pb == NULL) {
    db_printf(DB_DEBUG, "pptpInterfaceTask()  pb us null\n");
    return;
  }
  
  if (pptpLwip_netif.input(pb, &pptpLwip_netif) != ERR_OK) {
    db_printf(DB_DEBUG, "pptpInterfaceTask() pptpLwip_netif.input != OK\n");
    pbuf_free(pb);
  }

  db_printf(DB_DEBUG, "pptpInterfaceTask() End\n");
}

bool PPTPC_pptpInterfaceInit() {

  system_os_task(pptpInterfaceTask, PPTP_IF_TASK_PRIO, pptpInterfaceTaskQueue, PPTP_IF_TASK_QUEUE_SIZE);

  netif_add(&pptpLwip_netif, &localIP, &netmask, &remoteIP, NULL, PPTPC_pptpLwipInit, ip_input);
  netif_set_up(&pptpLwip_netif);


  return true;
}

bool PPTPC_connect() {

  db_printf(DB_DEBUG, "PPTPC_connect() Begin\n");
  if(!_syncClient.connect(PPTPC_servername, PPTPC_serverport)){
    db_printf(DB_INFO, "PPTPC_connect() syncClient.connect() Fail\n");
    return false;
  }

  if (PPTPC_startControlConnection() != true) {
    db_printf(DB_INFO, "PPTPC_connect() PPTPC_startControlConnection() Fail\n");
    return false;
  }

  if (PPTPC_outGoingCall() != true) {
    db_printf(DB_INFO, "PPTPC_connect() PPTPC_outGoingCall() Fail\n");
    return false;
  }

  if (PPTPC_setLinkInfo() != true) {
    db_printf(DB_INFO, "PPTPC_connect() PPTPC_setLinkInfo() Fail\n");
    return false;
  }


  // Begin GRE
  if (GRE_init( PPTPC_servername, PPTPC_serverport) != true) {
    db_printf(DB_INFO, "PPTPC_connect() GRE_init() Fail\n");
    return false;
  }
  GRE_callId = PPTPC_peerCallId;
  GRE_protocolType = 0x880b;  // PPP
  GRE_recvCallback = &PPTPC_receiveCallback;

  if (PPTPC_pppLcpConfig() != true) {
    db_printf(DB_INFO, "PPTPC_connect() PPTPC_pppLcpConfig() Fail\n");
    return false;
  }

  if (PPTPC_authenProtocol == 0xc023) {     // PAP Authen
    if (PPTPC_pppPap() != true) {
      db_printf(DB_INFO, "PPTPC_connect() PPTPC_pppPap() Fail\n");
      return false;
    }
  } else if (PPTPC_authenProtocol == 0xc223) {  // CHAP Authen
    
    // CHAP-MD5 Authen
    if (PPTPC_authenChapMode == CHAP_MD5) {
      if (PPTPC_pppChapMd5() != true) {
        db_printf(DB_INFO, "PPTPC_connect() PPTPC_pppChapMd5() Fail\n");
        return false;
      }
    } else if ((PPTPC_authenChapMode == CHAP_MSCHAP2) || (PPTPC_authenChapMode == CHAP_MSCHAP1)) {   // MS-CHAP Authen
      if (PPTPC_pppChapMsChap() != true) {
        db_printf(DB_INFO, "PPTPC_connect() PPTPC_pppChapMsChap() Fail\n");
        return false;
      }
    } else {
      db_printf(DB_INFO, "PPTPC_connect() PPTPC_pppChap() Unknown, Fail\n");
      return false;
    }
  } else {
    db_printf(DB_INFO, "PPTPC_connect() Fail Unknown Authen type 0x%04X\n", PPTPC_authenProtocol);
    return false;
  }

  if (PPTPC_pppCbcp() != true) {
    db_printf(DB_INFO, "PPTPC_connect() PPTPC_pppCbcp() Fail\n");
    return false;
  }

  if (PPTPC_pppIpcp() != true) {
    db_printf(DB_INFO, "PPTPC_connect() PPTPC_pppIpcp Failed\n");
    return false;
  }

/*
  if (PPTPC_pppMplscp() != true) {
    db_printf(DB_INFO, "PPTPC_connect() PPTPC_pppMplscp Failed\n");
    return false;
  }
*/

  // Begin to link pptp tunel to lwip
  if (PPTPC_pptpInterfaceInit() != true) {
    db_printf(DB_INFO, "PPTPC_connect() PPTPC_pptpInterfaceInit() Failed\n");
    return false;
  }
 
  db_printf(DB_DEBUG, "PPTPC_connect() Success\n");
  _pptpConnected = true;
  return true;
}

void PPTPC_buildSetLinkInfoRequest(struct SetLinkInfo *req){
  memset(req, 0, sizeof(struct SetLinkInfo));
  req->length = htons (24);
  req->pptpMessageType = htons(1);
  req->magic = htonl(0x1a2b3c4d);
  req->controlMessageType = htons(15);
  req->peerCallId = htons(PPTPC_peerCallId);
  req->sendAccm = htonl(0xffffffff);
  req->receiveAccm = htonl(0xffffffff);
  
}

void PPTPC_buildOutGoingCallRequest(struct OutGoingCallRequest *req) {
  PPTPC_callId = random(0x10000); // 16 bit random
  memset(req, 0, sizeof(struct OutGoingCallRequest));
  req->length = htons (168);
  req->pptpMessageType = htons(1);
  req->magic = htonl(0x1a2b3c4d);
  req->controlMessageType = htons(7);
  req->callId = htons(PPTPC_callId);
  req->callSerialNumber = htons(9);
  req->minimumBps = htonl(300);
  req->maximumBps = htonl(1000000000UL);
  req->bearerType = htonl(3);
  req->framingType = htonl(3);
  req->recvWindowSize = htons(64);
  req->processingDelay = htons(0);
  
}

void PPTPC_buildStartControlConnectionRequest(struct StartControlConnection *req) {
  memset(req, 0, sizeof(struct StartControlConnection));
  req->length = htons (156);
  req->pptpMessageType = htons(1);
  req->magic = htonl(0x1a2b3c4d);
  req->controlMessageType = htons(1);
  req->protocolVersion = htons(0x100); // 01.00
  req->framingCapabilities = htonl(1);
  req->bearerCapabilities = htonl(1);
  req->maximumChannel = htons(0);
  req->firmwareRevision = htons(0);
  strcpy(req->venderString, "ESP8266-Sun89-Natthapol89.com");
}

bool PPTPC_checkLcpRequest(uint8_t *lcpOptions, int length) {
  uint8_t *p = lcpOptions;
  uint8_t type;
  uint8_t len;
  uint8_t db;
  uint16_t ds;
  uint32_t dl;

  db_printf(DB_DEBUG, "PPTPC_checkLcpRequest() Begin\n");
  
  while(length > 0) {
    type = *p;
    len = *(p+1);
    if (type == 0x01) {   // Type: Maximum MRU
      if (len != 4) {
        db_printf(DB_DEBUG, "PPTPC_checkLcpRequest() error MRU len != 4\n");
        return false;
      }
      
      ds = *(p + 3) | (*(p + 2) << 8);
      db_printf(DB_DEBUG, "PPTPC_checkLcpRequest() Max MRU=%d\n", ds);
      if (ds > 1450) {
        db_printf(DB_DEBUG, "PPTPC_checkLcpRequest() error Max MRU > 1450\n");
        return false;
      }
      
    } else if (type == 0x03) {  // type: Authen protocol
      ds = *(p + 3) | (*(p + 2) << 8);
      if (ds == 0xc023) {         // PAP
        db_printf(DB_DEBUG, "PPTPC_checkLcpRequest() Authen mode = PAP\n");
        
      } else if (ds == 0xc223) {  // CHAP  
        PPTPC_authenChapMode = p[4];
        if (p[4] == CHAP_MD5) {      // CHAP-MD5
          db_printf(DB_DEBUG, "PPTPC_checkLcpRequest() Authen mode = CHAP-MD5\n");
        
        } else if (p[4] == CHAP_MSCHAP1) {  //MS-CHAP-1
          db_printf(DB_DEBUG, "PPTPC_checkLcpRequest() Authen mode = MS-CHAP-1\n");

        } else if (p[4] == CHAP_MSCHAP2) {  //MS-CHAP-2 
          db_printf(DB_DEBUG, "PPTPC_checkLcpRequest() Authen mode = MS-CHAP-2\n");

        } else {
          db_printf(DB_DEBUG, "PPTPC_checkLcpRequest() Authen mode = CHAP (Fail)\n");
          return false;
        }
      } else {
        db_printf(DB_DEBUG, "PPTPC_checkLcpRequest() Authen mode = Unknown 0x%04X\n", ds);
      }

      PPTPC_authenProtocol = ds;
      
    } else if (type == 0x05) {  //type: Magic number
      if (len != 6) {
        db_printf(DB_DEBUG, "PPTPC_checkLcpRequest() Error Magic Number len != 6\n");
        return false;
      }

      dl = p[5] | (p[4] << 8) | (p[3] << 16) | (p[2] << 24);
      db_printf(DB_DEBUG, "PPTPC_checkLcpRequest() Magic Number = 0x%X\n", dl);
      
    } else {
      db_printf(DB_DEBUG, "PPTPC_checkLcpRequest() Error Unknown option type =%d\n", type);
      //return false;
    }

    p = p + len;
    length = length - len;
  }

  db_printf(DB_DEBUG, "PPTPC_checkLcpRequest() Success\n");
  return true;
}

static int PPTPC_receiveCallback(uint8_t *data, int length) {
  struct PtpPacket *ptp;
  struct PppLcpPacket *lcp;
  struct PppPapPacket *pap;
  uint8_t *lcpOptions;
  int optionLength;
  uint8_t buff0[30];
  uint8_t *src = data;
  ip_addr_t *ip;
  
  //Serial.println("Callback");
  db_printf(DB_DEBUG, "PPTPC_receiveCallback() Begin\n");

  ptp = (struct PtpPacket *)data;
  db_printf(DB_DEBUG, "PPTPC_receiveCallback() PTP Address: 0x%02X, Control: 0x%02X, Protocol: 0x%04X\n", ptp->address, ptp->control, ntohs(ptp->protocol));

  data += 4;  //Skip ptp data

  // PPP LCP
  if (ntohs(ptp->protocol) == 0xc021) {
    lcp = (struct PppLcpPacket*)data;
    optionLength = ntohs(lcp->length) - 4;
    data +=4;

    db_printf(DB_DEBUG, "LCP Code: 0x%02X, Identifier: 0x%02X, Length: %d\n", lcp-> code, lcp->identifier, ntohs(lcp->length));
    
    // LCP Configuration Request
    if (lcp->code == 0x01) {

      if (PPTPC_checkLcpRequest(data, optionLength) == true) {
        lcp->code = 0x02; //LCP Config Ack
        lcpComplete = true;
      } else {
        db_printf(DB_DEBUG, "PPTPC_receiveCallback() ** Fail: LCP Option from server not support by client\n");
        lcp->code = 0x03; //LCP Config Nack
        
      }
      return 1; // tell parent to write gre answer

    }

    // LCP Echo Request
    if (lcp->code == 0x09) {
      lcp->code = 0x0a; //LCP echo reply
      return 1; // tell parent to write gre answer

    }
  }

  // PPP PAP
  if (ntohs(ptp->protocol) == 0xc023) {
    pap = (struct PppPapPacket*)data;
    optionLength = ntohs(pap->length) - 4;
    data +=4;

    db_printf(DB_DEBUG, "PAP Code: 0x%02X, Identifier: 0x%02X, Length: %d\n", pap->code, pap->identifier, ntohs(pap->length));
    
    // PAP Authen Ack
    if (pap->code == 0x02) {
      papComplete = true;
      papLoginState = true;
      return 0;
    }

    // PAP Authen Nack (user/pass fail)
    if (pap->code == 0x03) {
      papComplete = true;
      papLoginState = false;
      return 0;
    }
  }

  // PPP CHAP
  if (ntohs(ptp->protocol) == 0xc223) {
    lcp = (struct PppLcpPacket*)data;

    db_printf(DB_DEBUG, "PPTPC_receiveCallback() CHAP Code: 0x%02X, Identifier: 0x%02X, Length: %d\n", lcp-> code, lcp->identifier, ntohs(lcp->length));

    // Challenge
    if (lcp->code == 0x01) {

      memcpy(PPTPC_chapChallenge, data + 5, *(data +4));
      PPTPC_chapIdentifier = *(data + 1);
      PPTPC_chapChallengeSize = *(data +4);

      if (PPTPC_authenChapMode == CHAP_MSCHAP2) {
        MSCHAP_Init(&mschap_ctx, 2, PPTPC_chapChallenge);
      } else if (PPTPC_authenChapMode == CHAP_MSCHAP1) {
        MSCHAP_Init(&mschap_ctx, 1, PPTPC_chapChallenge);
      }
      return 0; // tell parent to not write gre answer
    }

    // Chap Success
    if (lcp->code == 0x03) {
      PPTPC_chapAuthenStatus = true;
      PPTPC_chapAuthenResponse = true;
      return 0;
    }

    // Chap Failure (May be User/Password incorrect)
    if (lcp->code == 0x04) {
      PPTPC_chapAuthenStatus = false;
      PPTPC_chapAuthenResponse = true;
      return 0;
    }
    
  }
  
  // PPP CBCP
  if (ntohs(ptp->protocol) == 0xc029) {
    lcp = (struct PppLcpPacket*)data;

    db_printf(DB_DEBUG, "PPTPC_receiveCallback() CBCP Code: 0x%02X, Identifier: 0x%02X, Length: %d\n", lcp-> code, lcp->identifier, ntohs(lcp->length));

    // CBCP Request
    if (lcp->code == 0x01) {
      lcp->code = 0x02; //CBCP Response
      return 1; // tell parent to write gre answer
    }

    // CBCP Ack
    if (lcp->code == 0x03) {
      cbcpComplete = true;
      return 0; // tell parent to not write gre answer
    }
    
  }

  // PPP MPLSCP
  if (ntohs(ptp->protocol) == 0x8281) {
    lcp = (struct PppLcpPacket*)data;

    db_printf(DB_DEBUG, "PPTPC_receiveCallback() MPLSCP Code: 0x%02X, Identifier: 0x%02X, Length: %d\n", lcp-> code, lcp->identifier, ntohs(lcp->length));

    // MPLS Request
    if (lcp->code == 0x01) {
      mplsIdentifier = lcp->identifier;
      return 0; // tell parent to not write gre answer
    }
    
  }

  // PPP IPCP (Internet protocol Control Protocol)
  if (ntohs(ptp->protocol) == 0x8021) {
    lcp = (struct PppLcpPacket*)data;
    uint8_t *opt = data + 4;

    db_printf(DB_DEBUG, "PPTPC_receiveCallback() IPCP Code: 0x%02X, Identifier: 0x%02X, Length: %d\n", lcp-> code, lcp->identifier, ntohs(lcp->length));

    // 0x01 IPCP Request (Server request to set own ip), 0x02 IPCP Ack
    if (lcp->code == 0x01 || lcp->code == 0x02) {
      optionLength = lcp->length;
      while (optionLength > 0) {
        if (*opt == 0x03) { // IP Address
          if (lcp->code == 0x01) {  // assigned server ip
            remoteIP.addr = *(uint32_t*)(opt + 2);
            //printf("Remote IP Address: ");
            //printIP(remoteIP);
            //printf("\n");
          }
          if (lcp->code == 0x02) {  // assigned client ip
            localIP.addr = *(uint32_t*)(opt + 2);
            //printf("Local IP Address: ");
            //printIP(localIP);
            //printf("\n");
          }
          
          break;
        }
        optionLength -= *(opt + 1);
        opt += *(opt + 1);
      }
      lcp->code = 0x02; //IPCP Ack
      return 1; // tell parent to write gre answer
    }

    // IPCP Nack, Server assigned IP to client
    if (lcp->code == 0x03) {
      lcp->code = 0x01; // new request
      lcp->identifier = lcp->identifier + 1;  // increment ident
      return 1; // tell parent to write gre answer
    }
    
  }

  // PPP IPv4 Protocol)
  if (ntohs(ptp->protocol) == 0x0021) {
    PPTPC_interfaceInput(data, length - 4);
    return 0;
  }
  
  return 2;
}

static uint32_t handleTmSec;
static uint32_t cntTmSec;
void PPTPC_handle() {
  uint32_t sec = millis() / 1000;
  if (sec < handleTmSec) {
    handleTmSec = sec;
    return;
  }

  if (sec - handleTmSec > 1) {
    handleTmSec = sec;

    if (!_syncClient.connected()) {
      printf( "PPTPC_handle() PPTP Disconnected --> Reset...\n");
      fflush(stdout);
      ESP.reset();
    }
    
    cntTmSec++;
    if (cntTmSec >= 10) {
      cntTmSec = 0;
      PPTPC_setLinkInfo();
      db_printf(DB_DEBUG, "PPTPC_setLinkInfo() at Sec %d\n", handleTmSec);
    }
  }
  
}
