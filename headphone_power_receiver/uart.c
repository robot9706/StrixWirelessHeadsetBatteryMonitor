#include "uart.h"

#define BUFFER_SIZE 128

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "log.h"

static uint8_t dataPointer = 0;
static uint8_t dataBuffer[BUFFER_SIZE];

void uart_event_handle(app_uart_evt_t * p_event)
{
	if (p_event->evt_type == APP_UART_DATA_READY)
	{  
		app_uart_get(&dataBuffer[dataPointer]);
        dataPointer++;
		
		if (dataPointer >= BUFFER_SIZE)
		{
			dataPointer = 0;
		}
	}
}

void uart_init(uint8_t rxPin, uint8_t txPin, unsigned long speed)
{
  const app_uart_comm_params_t comm_params =
  {
	rxPin,
    txPin,
    0,
    0,
    APP_UART_FLOW_CONTROL_DISABLED,
    false,
    speed
  };

	uint32_t err_code;
	APP_UART_FIFO_INIT(&comm_params,
					   BUFFER_SIZE,
                       BUFFER_SIZE,
                       uart_event_handle,
                       APP_IRQ_PRIORITY_LOW,
                       err_code);

	APP_ERROR_CHECK(err_code);
}

void uart_init_115200(uint8_t rxPin, uint8_t txPin)
{
	uart_init(rxPin, txPin, UART_BAUDRATE_BAUDRATE_Baud115200);
}

bool uart_put(uint8_t b)
{
	return (app_uart_put(b) == NRF_SUCCESS);
}

bool uart_put_string(const uint8_t *str)
{
	uint_fast8_t i = 0;
	uint8_t ch = str[i++];
	while (ch != '\0')
	{
		if(!uart_put(ch))
		{
			return false;
		}
		ch = str[i++];
	}
	
	return true;
}

void uart_write(const uint8_t* buffer, int count)
{
	for (int i = 0; i < count; i++)
	{
		app_uart_put(buffer[i]);
	}
}

bool uart_flush()
{
	return (app_uart_flush() == NRF_SUCCESS);
}

uint16_t uart_available()
{
		return dataPointer;
}

uint16_t uart_read()
{
	uint8_t data = dataBuffer[0];
	memcpy(&dataBuffer[0], &dataBuffer[1], dataPointer - 1);
	dataPointer = dataPointer - 1;
	
	return data;
}
