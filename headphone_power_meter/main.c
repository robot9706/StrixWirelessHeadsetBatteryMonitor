#include <stdbool.h>
#include <stdint.h>
#include "nordic_common.h"
#include "nrf.h"
#include "app_util_platform.h"
#include "ble_advdata.h"
#include "nordic_common.h"
#include "softdevice_handler.h"
#include "nrf_gpio.h"
#include "nrf_drv_config.h"
#include "nrf_delay.h"
#include "nrf_drv_adc.h"
#include "nrf_drv_config.h"
#include "uart.h"
#include "log.h"

// #define LOGGING

#define NRF_CLOCK_LFCLKSRC  {.source        = NRF_CLOCK_LF_SRC_XTAL,            \
                             .rc_ctiv       = 0,                                \
                             .rc_temp_ctiv  = 0,                                \
                             .xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_20_PPM}

#define CENTRAL_LINK_COUNT              0
#define PERIPHERAL_LINK_COUNT           0

#define APP_CFG_NON_CONN_ADV_TIMEOUT    0
#define NON_CONNECTABLE_ADV_INTERVAL    MSEC_TO_UNITS(3000, UNIT_0_625_MS)

#define APP_COMPANY_IDENTIFIER          0x0059

#define DEAD_BEEF                       0xDEADBEEF

// IO
#define IO_MIC_DETECT 2
#define IO_CHARGE_DETECT 30

// Advertising info
#define VERSION_HEADER 1
static ble_gap_adv_params_t m_adv_params;
__packed struct power_info {
	uint8_t lengthAndVersion;
	uint8_t flags;
	uint16_t adc;
	uint16_t volts;
};

#define APP_BEACON_INFO_LENGTH sizeof(struct power_info)
static struct power_info m_power_info;

// ADC
#define ADC_BUFFER_SIZE 1
static nrf_adc_value_t adc_buffer[ADC_BUFFER_SIZE];
static nrf_drv_adc_channel_t m_channel_config = NRF_DRV_ADC_DEFAULT_CHANNEL(NRF_ADC_CONFIG_INPUT_2); //GPIO 0 = P0.01

// Assert callback
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

static float adc_to_voltage(uint16_t adc)
{
	//y = 0.0100306*x - 3.140088 @ R1=680k, R2=300k /w LinearFit
	return 0.0100306f * (float)adc - 3.140088f;
}

static void advertising_data_update(void)
{
	// Calculate data
	uint16_t adc = adc_buffer[0];
	float measureVolts = adc_to_voltage(adc);
	
	// Set custom data
	m_power_info.lengthAndVersion = ((sizeof(struct power_info) & 0x0F) << 4 | VERSION_HEADER);
	m_power_info.adc = adc;
	m_power_info.volts = (uint16_t)(measureVolts * 1000);
	m_power_info.flags = (nrf_gpio_pin_read(IO_MIC_DETECT) ? 1 : 0) | (nrf_gpio_pin_read(IO_CHARGE_DETECT) ? 2 : 0);
	
#ifdef LOGGING
	LOGF("ADC: %d, V: %d, F: %d\r\n", m_power_info.adc, m_power_info.volts, m_power_info.flags);
#endif
	
	// Create data structures
    ble_advdata_manuf_data_t manuf_specific_data;
    manuf_specific_data.company_identifier = APP_COMPANY_IDENTIFIER;
    manuf_specific_data.data.p_data = (uint8_t*)&m_power_info;
    manuf_specific_data.data.size = APP_BEACON_INFO_LENGTH;

    // Build and set advertising data
	ble_advdata_t advdata;
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type             = BLE_ADVDATA_NO_NAME;
    advdata.flags                 = BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;
    advdata.p_manuf_specific_data = &manuf_specific_data;

    uint32_t err_code = ble_advdata_set(&advdata, NULL);
    APP_ERROR_CHECK(err_code);
}

static void advertising_init(void)
{
    advertising_data_update();
	
    // Initialize advertising parameters (used when starting advertising).
    memset(&m_adv_params, 0, sizeof(m_adv_params));

    m_adv_params.type        = BLE_GAP_ADV_TYPE_ADV_NONCONN_IND;
    m_adv_params.p_peer_addr = NULL; // Undirected advertisement.
    m_adv_params.fp          = BLE_GAP_ADV_FP_ANY;
    m_adv_params.interval    = NON_CONNECTABLE_ADV_INTERVAL;
    m_adv_params.timeout     = APP_CFG_NON_CONN_ADV_TIMEOUT;
}

static void advertising_start(void)
{
    uint32_t err_code;

    err_code = sd_ble_gap_adv_start(&m_adv_params);
    APP_ERROR_CHECK(err_code);
}

static void ble_stack_init(void)
{
    uint32_t err_code;
    
    nrf_clock_lf_cfg_t clock_lf_cfg = NRF_CLOCK_LFCLKSRC;
    
    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);

    ble_enable_params_t ble_enable_params;
    err_code = softdevice_enable_get_default_config(CENTRAL_LINK_COUNT,
                                                    PERIPHERAL_LINK_COUNT,
                                                    &ble_enable_params);
    APP_ERROR_CHECK(err_code);
    
    //Check the ram settings against the used number of links
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT,PERIPHERAL_LINK_COUNT);
    
    // Enable BLE stack.
    err_code = softdevice_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);
}

static void update_status()
{
	APP_ERROR_CHECK(nrf_drv_adc_buffer_convert(adc_buffer, ADC_BUFFER_SIZE));
	nrf_drv_adc_sample();
}

static void adc_event(nrf_drv_adc_evt_t const * p_event)
{
}

static void adc_config(void)
{
    ret_code_t ret_code;
    nrf_drv_adc_config_t config = NRF_DRV_ADC_DEFAULT_CONFIG;

    ret_code = nrf_drv_adc_init(&config, adc_event);
    APP_ERROR_CHECK(ret_code);

    nrf_drv_adc_channel_enable(&m_channel_config);
}

static void gpio_init(void)
{
	nrf_gpio_cfg_input(IO_MIC_DETECT, NRF_GPIO_PIN_PULLUP);
	nrf_gpio_cfg_input(IO_CHARGE_DETECT, NRF_GPIO_PIN_NOPULL);
}

static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}

int main(void)
{
#ifdef LOGGING
	uart_init_115200(4, 3);
	LOG("HELLO");
#endif
	
	adc_config();
	gpio_init();
	
    ble_stack_init();
    advertising_init();

    advertising_start();
	
    for (;; )
    {
		update_status();
		
        power_manage();
		
        nrf_delay_ms(1000);
		
		advertising_data_update();
    }
}

