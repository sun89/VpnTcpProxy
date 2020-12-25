#ifndef DebugMsg_h
#define DebugMsg_h

#include "Arduino.h"
#define DB_INFO   1
#define DB_DEBUG  10

void db_printf(uint8_t level, char *fmt, ...);
uint8_t db_getLevel();
void db_setLevel(uint8_t lv);

#endif
