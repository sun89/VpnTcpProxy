#include "DebugMsg.h"


uint8_t debugLevel_PPTP = DB_INFO;

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
