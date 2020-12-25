#ifndef GRE_h
#define GRE_h

extern "C"
{
  #include <lwip/raw.h>
  #include <user_interface.h>
}

#define IP_PROTO_GRE 47
struct GrePacket {
  uint16_t flagsAndVersion;
  uint16_t protocolType;
  uint16_t payloadLength;
  uint16_t callId;
};

struct GreAckPacket {
  uint16_t flagsAndVersion;
  uint16_t protocolType;
  uint16_t payloadLength;
  uint16_t callId;
  uint32_t acknowledgmentNumber;
};

bool GRE_init(const char *servername, int port);
int GRE_write(uint8_t *data, int length);
int GRE_writeAck();
int GRE_read(uint8_t *data, int length);
int GRE_available();
void GRE_setProtocolType(uint16_t pro);

extern uint16_t GRE_protocolType;
extern uint8_t GRE_payloadLength;
extern uint16_t GRE_callId;
extern uint32_t GRE_sequenceNumber;
extern int (*GRE_recvCallback)(uint8_t *, int);



#endif
