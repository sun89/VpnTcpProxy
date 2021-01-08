#include "DebugMsg.h"


uint8_t debugLevel_PPTP = DB_DEBUG;

void db_printf(uint8_t level, char *fmt, ...) {
  char buf[256];
  if (level <= debugLevel_PPTP) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 256, fmt, args);
    va_end(args);
    Serial.print(buf);
  }
}

uint8_t db_getLevel() {
  return debugLevel_PPTP;
  
}

void db_setLevel(uint8_t lv) {
   debugLevel_PPTP = lv;
  
}

#ifndef HEXDUMP_COLS
#define HEXDUMP_COLS 16
#endif
 
void db_printHex_(void *mem, unsigned int len)
{
        unsigned int i, j;
        
        for(i = 0; i < len + ((len % HEXDUMP_COLS) ? (HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++)
        {
                /* print offset */
                if(i % HEXDUMP_COLS == 0)
                {
                        printf("0x%06x: ", i);
                        fflush(stdout);
                }
 
                /* print hex data */
                if(i < len)
                {
                        printf("%02x ", 0xFF & ((char*)mem)[i]);
                        fflush(stdout);
                }
                else /* end of block, just aligning for ASCII dump */
                {
                        printf("   ");
                        fflush(stdout);
                }
                
                /* print ASCII dump */
                if(i % HEXDUMP_COLS == (HEXDUMP_COLS - 1))
                {
                        for(j = i - (HEXDUMP_COLS - 1); j <= i; j++)
                        {
                                if(j >= len) /* end of block, not really printing */
                                {
                                        putchar(' ');
                                }
                                else if(isprint(((char*)mem)[j])) /* printable char */
                                {
                                        putchar(0xFF & ((char*)mem)[j]);        
                                }
                                else /* other char */
                                {
                                        putchar('.');
                                }
                                fflush(stdout);
                        }
                        putchar('\n');
                        fflush(stdout);
                }
        }
        fflush(stdout);
}
 
void db_printHex(uint8_t level, void *mem, unsigned int len) {
  if (level <= debugLevel_PPTP) {
    db_printHex_((uint8_t*)mem, len);
  }
}
