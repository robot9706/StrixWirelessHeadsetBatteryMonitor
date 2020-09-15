#include <stdint.h>

#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "app_uart.h"
#include "app_fifo.h"
#include "app_error.h"

void uart_init(uint8_t rxPin, uint8_t txPin, unsigned long speed);
void uart_init_115200(uint8_t rxPin, uint8_t txPin);

bool uart_put(uint8_t b);
bool uart_put_string(const uint8_t *str);

bool uart_flush(void);
uint16_t uart_available(void);

uint16_t uart_read(void);
