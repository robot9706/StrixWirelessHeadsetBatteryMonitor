#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf_sdm.h"
#include "ble.h"
#include "softdevice_handler.h"
#include "app_util.h"
#include "app_error.h"
#include "uart.h"
#include "log.h"

#define DATA_HEADER 0xA5

#define COMMAND_ENABLE 0xAF
#define COMMAND_DISABLE 0xBF

#define SCAN_INTERVAL 0x00A0 // Scan interval in 0.625ms units (100ms)
#define SCAN_WINDOW   0x00A0 // Scan window in 0.625ms units (100ms)

#define DEADBEEF 0xDEADBEEF

#define CLIENT_MAC 0x7E, 0x92, 0xC2, 0xFB, 0x54, 0xC6

static ble_gap_scan_params_t scan_parameters;

static bool sendData = false; // Flag to signal when to send data

static void scan_start(void)
{
	scan_parameters.active = 0;            		// Non active scanning.
    scan_parameters.selective = 0;          	// Non selective.
    scan_parameters.interval = SCAN_INTERVAL;	// Scan interval.
    scan_parameters.window = SCAN_WINDOW;  		// Scan window.
    scan_parameters.p_whitelist = NULL;         // No whitelist provided.
    scan_parameters.timeout = 0x0000;       	// No timeout.

    uint32_t err_code = sd_ble_gap_scan_start(&scan_parameters);
    APP_ERROR_CHECK(err_code);
}

void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEADBEEF, line_num, p_file_name);
}

static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    const ble_gap_evt_t   * p_gap_evt = &p_ble_evt->evt.gap_evt;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_ADV_REPORT:
        {
			if (sendData)
			{
				const ble_gap_addr_t * const peer_addr  = &p_gap_evt->params.adv_report.peer_addr;
				const uint8_t checkAddress[6] = { CLIENT_MAC };
				
				if (memcmp(peer_addr->addr, checkAddress, 6) == 0)
				{
					uart_put(DATA_HEADER);
					uart_put(p_gap_evt->params.adv_report.dlen);
					uart_write((uint8_t *)p_gap_evt->params.adv_report.data, p_gap_evt->params.adv_report.dlen);
				}
			}
            break;
        }

		// Timeout event
        case BLE_GAP_EVT_TIMEOUT:
            if (p_gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_SCAN) // Scan timeout, restart scan
            {
                scan_start();
            }
            break;
			
        default:
            break;
    }
}

static void ble_stack_init(void)
{
    uint32_t err_code;

    // Initialize the SoftDevice handler module (internal lowspeed RC oscillator)
    SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_RC_250_PPM_TEMP_4000MS_CALIBRATION, NULL);

    // Enable BLE stack.
    ble_enable_params_t ble_enable_params;
    memset(&ble_enable_params, 0, sizeof(ble_enable_params));
    ble_enable_params.gatts_enable_params.service_changed = false;
    ble_enable_params.gap_enable_params.role              = BLE_GAP_ROLE_CENTRAL;

    err_code = sd_ble_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}

static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}

int main(void)
{
    uart_init_115200(2, 1); // Init the serial port

    ble_stack_init();
    scan_start();

    for (;; )
    {
        power_manage();
		
		if (uart_available() > 0)
		{
			switch (uart_read())
			{
				case COMMAND_ENABLE:
					sendData = true;
					break;
				case COMMAND_DISABLE:
					sendData = false;
					break;
			}
		}
    }
}
