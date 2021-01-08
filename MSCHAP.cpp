
#include "MSCHAP.h"


#include <iostream>
#include <stdint.h>
#include <string.h>
#include <Arduino.h>

#include "sha1.h"
#include "md4.h"
#include "des.h"
#include "DebugMsg.h"

int ChallengeHash( uint8_t PeerChallenge[16], uint8_t AuthenticatorChallenge[16], char *UserName, uint8_t Challenge[8]);
int NtPasswordHash(char *PasswordASCII,uint8_t PasswordHash[16] );
int ChallengeResponse(uint8_t Challenge[8], uint8_t PasswordHash[16], uint8_t Response[24] );
int DesEncrypt(uint8_t Clear[8], uint8_t Key[7], uint8_t Cypher[8] );

int GenerateNTResponse(uint8_t AuthenticatorChallenge[16], uint8_t PeerChallenge[16], char *UserName, char *Password, uint8_t Response[24], uint8_t mschapVersion) {
      uint8_t Challenge[8];
      uint8_t PasswordHash[16];
      char *p = Password;

      if ( mschapVersion == 2) {
        ChallengeHash( PeerChallenge, AuthenticatorChallenge, UserName, Challenge);
      } else if (mschapVersion == 1) {
        memcpy(Challenge, AuthenticatorChallenge, 8);
      } else {
        db_printf(DB_DEBUG, "GenerateNTResponse() Unknown CHAP Version %d\n", mschapVersion);
        return 0;
      }

      NtPasswordHash( p, PasswordHash);
      ChallengeResponse( Challenge, PasswordHash, Response );
     
      db_printf(DB_DEBUG, "GenerateNTResponse():: . . .");
      db_printHex(DB_DEBUG, Response, 24);
      return 1;
}

int ChallengeHash( uint8_t PeerChallenge[16], uint8_t AuthenticatorChallenge[16], char *UserName, uint8_t Challenge[8]) {
  
  uint8_t Digest[20];
  SHA1_CTX ctx;
  
      /*
       * SHAInit(), SHAUpdate() and SHAFinal() functions are an
       * implementation of Secure Hash Algorithm (SHA-1) [11]. These are
       * available in public domain or can be licensed from
       * RSA Data Security, Inc.
       */

  sha1_init(&ctx);
  sha1_update(&ctx, PeerChallenge, 16);
  sha1_update(&ctx, AuthenticatorChallenge, 16);

      /*
       * Only the user name (as presented by the peer and
       * excluding any prepended domain name)
       * is used as input to SHAUpdate().
       */

  sha1_update(&ctx, (const BYTE*)UserName, strlen(UserName));
  sha1_final(&ctx, Digest);
  memcpy(Challenge, Digest, 8);
    
  db_printf(DB_DEBUG, "SHA1 Digest: ");
  db_printHex(DB_DEBUG, Digest, 20);
  db_printf(DB_DEBUG, "SHA1 Challenge: ");
  db_printHex(DB_DEBUG, Challenge, 8);
  return 1;
   
}



int NtPasswordHash(char *PasswordASCII, uint8_t PasswordHash[16] ) {
  char passUnicode[256 * 2];
  int i;

  
  db_printf(DB_DEBUG, "Password ASCII\n");
  db_printHex(DB_DEBUG, PasswordASCII, strlen(PasswordASCII));
  
  memset(passUnicode, 0, 256*2);
  // Convert Ascii to Unicode
  for (i = 0; i < strlen(PasswordASCII); i++) {
    passUnicode[i * 2] = PasswordASCII[i];
  }
  
  
  db_printf(DB_DEBUG, "Password Unicode\n");
  db_printHex(DB_DEBUG, passUnicode, 256*2);
      /*
       * Use the MD4 algorithm [5] to irreversibly hash Password
       * into PasswordHash.  Only the password is hashed without
       * including any terminating 0.
       */
    
  auth_md4Sum(PasswordHash, (const unsigned char *)passUnicode, strlen(PasswordASCII) * 2 );
    
  db_printf(DB_DEBUG, "NtPasswordHash: ");
  db_printHex(DB_DEBUG, PasswordHash, 16);
  return 1;
}



int HashNtPasswordHash( uint8_t PasswordHash[16], uint8_t PasswordHashHash[16] ) {

      /*
       * Use the MD4 algorithm [5] to irreversibly hash
       * PasswordHash into PasswordHashHash.
       */
  
  auth_md4Sum(PasswordHashHash, (const unsigned char *)PasswordHash, 16 );
    
  db_printf(DB_DEBUG, "HashNtPasswordHash: ");
  db_printHex(DB_DEBUG, PasswordHashHash, 16);
  return 1;
  
}


int getBit(uint8_t data, int bitNumber) {
  int bit = data & (1 << (7 - bitNumber) );
  if (bit != 0)
    return 1;
  return 0;
}

void setBit(uint8_t *data, int bitNumber) {
  
  data[0] |= 1 << (7 - bitNumber);
}

int DesKey56BitTo64Bit(uint8_t key64[8], uint8_t key56[7]) {

  uint8_t bitValue;
  uint8_t *pout = key64;
  uint8_t *pin = key56;
  
  int bit56Number = 0;
  
  int bit64Number = 0;
  int loopCount = 56;
  int bit1Count = 0;
  
  memset(key64, 0, 8);
  
  while (loopCount > 0) {
    loopCount--;
      
    bitValue = getBit(*pin, bit56Number);
    
    if (bitValue) {
      setBit(pout, bit64Number);
      bit1Count++;
    }
    
    bit56Number++;
    if (bit56Number > 7) {
      bit56Number = 0;
      pin++;
    }
    
    bit64Number++;
    if (bit64Number > 6) {
      
      // Odd Parity set
      if (bit1Count % 2 == 0) setBit(pout, 7);
      
      bit1Count = 0;
      bit64Number = 0;
      pout++;
    }
    
    
  }
}


int ChallengeResponse(uint8_t Challenge[8], uint8_t PasswordHash[16], uint8_t Response[24] ) {
  //Set ZPasswordHash to PasswordHash zero-padded to 21 octets
  uint8_t ZPasswordHash[21];
  uint8_t key[3][8];
    
  memset(ZPasswordHash, 0, 21);
  memcpy(ZPasswordHash, PasswordHash, 16);
  //uint8_t dumm[8] = { 0x89 ,0xD6 ,0x80 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 };
    
  DesKey56BitTo64Bit(key[0], &ZPasswordHash[0]);   
  DesKey56BitTo64Bit(key[1], &ZPasswordHash[7]); 
  DesKey56BitTo64Bit(key[2], &ZPasswordHash[14]);  

  DesEncrypt( Challenge, key[0], &Response[0]);
  DesEncrypt( Challenge, key[1], &Response[8]);
  DesEncrypt( Challenge, key[2], &Response[16]);
             
  return 1;

}

int DesEncrypt(uint8_t Clear[8], uint8_t Key[7], uint8_t Cypher[8] ) {

      /*
       * Use the DES encryption algorithm [4] in ECB mode [10]
       * to encrypt Clear into Cypher such that Cypher can
       * only be decrypted back to Clear by providing Key.
       * Note that the DES algorithm takes as input a 64-bit
       * stream where the 8th, 16th, 24th, etc.  bits are
       * parity bits ignored by the encrypting algorithm.
       * Unless you write your own DES to accept 56-bit input
       * without parity, you will need to insert the parity bits
       * yourself.
       */
       
  BYTE schedule[16][6];
  des_key_setup(Key, schedule, DES_ENCRYPT);
  des_crypt(Clear, Cypher, schedule);
  
  db_printf(DB_DEBUG, "DesEncrypt:: Start\n");
  db_printf(DB_DEBUG, "Clear Data. . .");
  db_printHex(DB_DEBUG, Clear, 8);
  db_printf(DB_DEBUG, "Key . . .");
  db_printHex(DB_DEBUG, Key, 7);
  db_printf(DB_DEBUG, "Cypher Data. . .");
  db_printHex(DB_DEBUG, Cypher, 8);  
  return 1;
}

/////////////////////////// MSCHAP V1 ///////////////////////////////////////////////
int LmPasswordHash(char *Password,uint8_t PasswordHash[16] );
int DesHash( uint8_t Clear[7], uint8_t Cypher[8] );

int LmChallengeResponse( uint8_t Challenge[8], char *Password, uint8_t Response[24] ) {
  uint8_t PasswordHash[16];
  int i;
  
  // MSCHAP1 MAX Password is 14
  if (strlen(Password) > 14) {
    return 0;
  }

  
  
  LmPasswordHash( Password, PasswordHash);
  ChallengeResponse( Challenge, PasswordHash, Response );

  return 1;
}

int LmPasswordHash(char *Password,uint8_t PasswordHash[16] ) {
  char upperPass[15];
  int i;

  memset(upperPass, 0, 15);
  
  i = 0;
  while(Password[i] != '\0') {
    upperPass[i] = toupper(Password[i]);
    i++;
  }

  DesHash( (uint8_t*)&upperPass[0], &PasswordHash[0] );
  DesHash( (uint8_t*)&upperPass[7], &PasswordHash[8] );
  return 1;

}

int DesHash( uint8_t Clear[7], uint8_t Cypher[8] ) {
      /*
       * Make Cypher an irreversibly encrypted form of Clear by
       * encrypting known text using Clear as the secret key.
       * The known text consists of the string
       *
       *              KGS!@#$%
       */

  char StdText[] = "KGS!@#$%";
  DesEncrypt( (uint8_t*)StdText, Clear, Cypher );
  return 1;

}

//////////////////////////////////////////////////////////////////////////////////

int GenerateAuthenticatorResponse(char *PasswordASCII, uint8_t NtResponse[24], uint8_t PeerChallenge[16], uint8_t AuthenticatorChallenge[16], char *UserName, uint8_t *AuthenticatorResponse ) {
    
  uint8_t PasswordHash[16];
  uint8_t PasswordHashHash[16];
  uint8_t Challenge[8];
  uint8_t Digest[20];
  SHA1_CTX ctx;

      /*
       * "Magic" constants used in response generation
       */

  uint8_t Magic1[39] =
         {0x4D, 0x61, 0x67, 0x69, 0x63, 0x20, 0x73, 0x65, 0x72, 0x76,
          0x65, 0x72, 0x20, 0x74, 0x6F, 0x20, 0x63, 0x6C, 0x69, 0x65,
          0x6E, 0x74, 0x20, 0x73, 0x69, 0x67, 0x6E, 0x69, 0x6E, 0x67,
          0x20, 0x63, 0x6F, 0x6E, 0x73, 0x74, 0x61, 0x6E, 0x74};


  uint8_t Magic2[41] =
         {0x50, 0x61, 0x64, 0x20, 0x74, 0x6F, 0x20, 0x6D, 0x61, 0x6B,
          0x65, 0x20, 0x69, 0x74, 0x20, 0x64, 0x6F, 0x20, 0x6D, 0x6F,
          0x72, 0x65, 0x20, 0x74, 0x68, 0x61, 0x6E, 0x20, 0x6F, 0x6E,
          0x65, 0x20, 0x69, 0x74, 0x65, 0x72, 0x61, 0x74, 0x69, 0x6F,
          0x6E};
          
  int i;


      /*
       * Hash the password with MD4
       */

  NtPasswordHash( PasswordASCII, PasswordHash );

      /*
       * Now hash the hash
       */

  HashNtPasswordHash( PasswordHash, PasswordHashHash);

  sha1_init(&ctx);
  sha1_update(&ctx, PasswordHashHash, 16);
  sha1_update(&ctx, NtResponse, 24);
  sha1_update(&ctx, Magic1, 39);
  sha1_final(&ctx, Digest);
  
  ChallengeHash( PeerChallenge, AuthenticatorChallenge, UserName, Challenge);

  sha1_init(&ctx);
  sha1_update(&ctx, Digest, 20);
  sha1_update(&ctx, Challenge, 8);
  sha1_update(&ctx, Magic2, 41);
  sha1_final(&ctx, Digest);

      /*
       * Encode the value of 'Digest' as "S=" followed by
       * 40 ASCII hexadecimal digits and return it in
       * AuthenticatorResponse.
       * For example,
       *   "S=0123456789ABCDEF0123456789ABCDEF01234567"
       */
       
  AuthenticatorResponse[0] = 'S';
  AuthenticatorResponse[1] = '=';
  AuthenticatorResponse += 2;
  for (i = 0; i < 20; i++) {
    sprintf((char*)AuthenticatorResponse, "%02X", Digest[i]);
    AuthenticatorResponse += 2;
  }
  
    db_printf(DB_DEBUG, "AuthenticatorResponse: %s\n", AuthenticatorResponse);
    return 1;

}


bool CheckAuthenticatorResponse(char *Password, uint8_t NtResponse[24], uint8_t PeerChallenge[16], uint8_t AuthenticatorChallenge[16], char *UserName, uint8_t ReceivedResponse[42] ) {
  
  bool ResponseOK = false;
  uint8_t MyResponse[43];

  GenerateAuthenticatorResponse( Password, NtResponse, PeerChallenge, AuthenticatorChallenge, UserName, MyResponse);
    
  db_printf(DB_DEBUG, "Received  Response= %s\n", ReceivedResponse);
  db_printf(DB_DEBUG, "Calculate Response= %s\n", MyResponse);

  if (memcmp(MyResponse, ReceivedResponse, 42) == 0) 
    ResponseOK = true;
    
  return ResponseOK;
}

/* run this program using the console pauser or add your own getch, system("pause") or input loop */

void MSCHAP_Test() {
  
  //uint8_t serverChallenge[16] = { 0x2f, 0x05, 0x3e, 0xae, 0xf1, 0x2f, 0xe6, 0x97, 0xf9, 0x2f, 0x29, 0x80, 0x07, 0x2c, 0x40, 0x11};
  uint8_t serverChallenge[16] = { 0xe3 ,0xf6 ,0x74 ,0xb3 ,0xfc ,0x71 ,0x18 ,0xfe ,0x1c ,0x60 ,0xd6 ,0x2e ,0xb8 ,0xa0 ,0x06 ,0x70 };
  //uint8_t serverChallenge[16] = { 0x5B ,0x5D ,0x7C ,0x7D ,0x7B ,0x3F ,0x2F ,0x3E ,0x3C ,0x2C ,0x60 ,0x21 ,0x32 ,0x26 ,0x26 ,0x28 };
  
  //uint8_t peerChallenge[16]   = { 0xcf, 0xbb, 0x78, 0x31, 0xfe, 0xf2, 0x42, 0x09, 0xd8, 0xa7, 0x63, 0xcf, 0x96, 0x25, 0x0c, 0x6c};
  uint8_t peerChallenge[16]   = { 0xed ,0x9b ,0xfb ,0xe9 ,0xe5 ,0x90 ,0x11 ,0x74 ,0x8c ,0x56 ,0x74 ,0x24 ,0x97 ,0xb9 ,0x40 ,0xed };
  //uint8_t peerChallenge[16]   = { 0x21 ,0x40 ,0x23 ,0x24 ,0x25 ,0x5E ,0x26 ,0x2A ,0x28 ,0x29 ,0x5F ,0x2B ,0x3A ,0x33 ,0x7C ,0x7E };
  
  uint8_t NTResponse[24] = {0};
  char user[100] = "ppp-test-o";
  char password[100] = "ppp-password";
  //char user[100] = "User";
  //char password[100] = "clientPass";
  char receiveResponse[] = "S=2EC8D2A3B4969F9C00215F467BB0C5307F0B765B";
  
  printf("MSCHAP_Test\n");
  
  uint8_t test7[7] = { 0x89 ,0xAE ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 };
  uint8_t out8[8]; 
  DesKey56BitTo64Bit(out8, test7);
  db_printHex(0, test7, 7);
  db_printHex(0, out8, 8);
  
  GenerateNTResponse(serverChallenge, peerChallenge, user, password, NTResponse, 2);  //MS-CHAP-2
  CheckAuthenticatorResponse(password, NTResponse, peerChallenge, serverChallenge, user, (uint8_t*)receiveResponse);

}


void MSCHAP_Init(MSCHAP_CTX *ctx, uint8_t mschapVersion, uint8_t AuthenticatorChallenge[16]) {
  int i;
  uint8_t *p = ctx->PeerChallenge;

  db_printf(DB_DEBUG, "MSCHAP_Init() start\n");

  ctx->version = mschapVersion;
  if (ctx->version == 1) {
    db_printf(DB_DEBUG, "MSCHAP_Init() Version = MS-CHAP-1\n");
  } else if (ctx->version == 2) {
    db_printf(DB_DEBUG, "MSCHAP_Init() Version = MS-CHAP-2\n");
  } else {
    db_printf(DB_DEBUG, "MSCHAP_Init() Version = %d --- Unknown!!!\n", ctx->version );
  }
  
  memcpy(ctx->AuthenticatorChallenge, AuthenticatorChallenge, 16);
  memset(ctx->username, 0, 256);
  memset(ctx->password, 0, 256);
  
  // Random Peer Challenge
  for (i = 0; i < 16; i++) {
    p[i] = random(0, 0x100);
  }

  db_printf(DB_DEBUG, "AuthChallenge. . . \n");
  db_printHex(DB_DEBUG, ctx->AuthenticatorChallenge, 16);
  db_printf(DB_DEBUG, "PeerChallenge. . . \n");
  db_printHex(DB_DEBUG, ctx->PeerChallenge, 16);
  db_printf(DB_DEBUG, "MSCHAP_Init() end\n");
  
}

bool MSCHAP_GetResponse(MSCHAP_CTX *ctx, char *username, char *password, uint8_t response[49]) {
  int ret;
  uint8_t lmChallenge[24];
  memset(response, 0, 49);

  strcpy(ctx->username, username);
  strcpy(ctx->password, password);
  ret = GenerateNTResponse(ctx->AuthenticatorChallenge, ctx->PeerChallenge, ctx->username, ctx->password, ctx->NtResponse, ctx->version);
  if (ret == 0) {

    return false;
  }

  if (ctx->version == 2) {
    memcpy(response, ctx->PeerChallenge, 16);
    memcpy(&response[24], ctx->NtResponse, 24);
    db_printf(DB_DEBUG, "MSCHAP_GetResponse() Response. . .\n");
    db_printHex(DB_DEBUG, response, 49);
    return true;
  } else if (ctx->version == 1) {
    LmChallengeResponse( ctx->AuthenticatorChallenge, ctx->password, lmChallenge );
    memcpy(response, lmChallenge, 24);
    memcpy(&response[24], ctx->NtResponse, 24);
    response[48] = 1;
    return true;
  }

  return false;
}

bool MSCHAP_CheckAuthenticatorResponse(MSCHAP_CTX *ctx, uint8_t ReceivedResponse[42]) {
  bool ret = CheckAuthenticatorResponse(ctx->password, ctx->NtResponse, ctx->PeerChallenge, ctx->AuthenticatorChallenge, ctx->username, (uint8_t*)ReceivedResponse);
  db_printf(DB_DEBUG, "MSCHAP_CheckAuthenticatorResponse() Result = %s\n", ret==0?"false":"true");
  return ret;
}
