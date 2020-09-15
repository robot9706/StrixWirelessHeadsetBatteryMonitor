#include "uart.h"

#define BUFFER_SIZE 128

void uart_error_handle(app_uart_evt_t * p_event)
{
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
                      uart_error_handle,
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

bool uart_flush()
{
	return (app_uart_flush() == NRF_SUCCESS);
}

uint16_t uart_available()
{
		return app_uart_fifo_count();
}

uint16_t uart_read()
{
	uint8_t p;
	if (app_uart_get(&p) != NRF_SUCCESS)
	{
		return 0;
	}
	
	return p;
}
