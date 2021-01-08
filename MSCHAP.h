#ifndef MSCHAP_h
#define MSCHAP_h

#include <stdint.h>

typedef struct {
  uint8_t AuthenticatorChallenge[16];
  uint8_t PeerChallenge[16];
  char username[256];
  char password[256];
  uint8_t NtResponse[24];
  uint8_t version;
} MSCHAP_CTX;

void MSCHAP_Init(MSCHAP_CTX *ctx, uint8_t mschapVersion, uint8_t AuthenticatorChallenge[16]);
bool MSCHAP_GetResponse(MSCHAP_CTX *ctx, char *username, char *password, uint8_t response[49]);
bool MSCHAP_CheckAuthenticatorResponse(MSCHAP_CTX *ctx, uint8_t ReceivedResponse[42]);
void MSCHAP_Test();

#endif
