#ifndef __LOG__
#define __LOG__

#include "uart.h"

#define LOG(...) uart_put_string((const uint8_t *)__VA_ARGS__); uart_put_string((const uint8_t *)"\r\n");
#define LOGF(...) { char buf[64]; sprintf(buf, __VA_ARGS__); LOG(buf); }

#endif
