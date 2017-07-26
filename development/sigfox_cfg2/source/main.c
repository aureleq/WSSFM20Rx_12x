/* Copyright (c) 2017 WISOL Corp. All Rights Reserved.
 *
 * The information contained herein is property of WISOL Cor.
 * Terms and conditions of usage are described in detail in WISOL STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * This heading must NOT be removed from
 * the file.
 *
 */

/** @file
 *
 * @brief tracking Sample Application main.c file.
 *
 * This file contains the source code for an tracking sample application.
 */

#include <stdbool.h>
#include <stdint.h>
#include "cfg_board_def.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "ble_conn_state.h"
#include "ble_flash.h"
#include "ble_bas.h"

#include "cfg_scenario.h"
#include "nordic_common.h"
#include "softdevice_handler.h"
#include "peer_manager.h"
#include "bsp.h"
#include "app_timer.h"
#include "nrf_delay.h"
#include "cfg_app_main.h"
#include "cfg_sigfox_module.h"
#include "cfg_bma250_module.h"
#include "cfg_tmp102_module.h"
#include "cfg_gps_module.h"
#include "cfg_dbg_log.h"
#include "cfg_wifi_module.h"
#include "cfg_board.h"
#include "nrf_drv_gpiote.h"
#include "fstorage.h"
#include "fds.h"
#include "nrf_drv_twi.h"
#include "nfc_t2t_lib.h"
#include "nfc_uri_msg.h"
#include "nfc_launchapp_msg.h"
#include "nfc_launchapp_rec.h"
#include "nfc_ndef_msg.h"
#include "nfc_text_rec.h"
#include "nrf_drv_saadc.h"
#include "hardfault.h"
#include "ble_dfu.h"
#include "ble_nus.h"
#include "nrf_drv_clock.h"
#include "cfg_external_sense_gpio.h"


#define IS_SRVC_CHANGED_CHARACT_PRESENT 1                                           /**< Include or not the service_changed characteristic. if not enabled, the server's database cannot be changed for the lifetime of the device*/

#if (NRF_SD_BLE_API_VERSION == 3)
#define NRF_BLE_MAX_MTU_SIZE            GATT_MTU_SIZE_DEFAULT                       /**< MTU size used in the softdevice enabling and to reply to a BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST event. */
#endif

#define APP_FEATURE_NOT_SUPPORTED       BLE_GATT_STATUS_ATTERR_APP_BEGIN + 2        /**< Reply when unsupported features are requested. */

#define CENTRAL_LINK_COUNT              0                                           /**< Number of central links used by the application. When changing this number remember to adjust the RAM settings*/
#define PERIPHERAL_LINK_COUNT           1                                           /**< Number of peripheral links used by the application. When changing this number remember to adjust the RAM settings*/

#define DEVICE_NAME                     "WISOL_SFM21R"                           /**< Name of device. Will be included in the advertising data. */
#define MANUFACTURER_NAME               "WISOL_Corp"                       /**< Manufacturer. Will be passed to Device Information Service. */
#define ASSETTRACKER_NAME               "ihere"
#define MINI_ASSETTRACKER_NAME          "ihere mini"
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */

#define APP_ADV_INTERVAL                300                                         /**< The advertising interval (in units of 0.625 ms. This value corresponds to 187.5 ms). */
#define APP_ADV_TIMEOUT_IN_SECONDS      30                                         /**< The advertising timeout in units of seconds. */

#define APP_CFG_NON_CONN_ADV_TIMEOUT    0                                 /**< Time for which the device must be advertising in non-connectable mode (in seconds). 0 disables timeout. */
#define NON_CONNECTABLE_ADV_INTERVAL    MSEC_TO_UNITS(1000, UNIT_0_625_MS) /**< The advertising interval for non-connectable advertisement (100 ms). This value can vary between 100ms to 10.24s). */


#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(100, UNIT_1_25_MS)            /**< Minimum acceptable connection interval (0.1 seconds). */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(200, UNIT_1_25_MS)            /**< Maximum acceptable connection interval (0.2 second). */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)  /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER) /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */

#define SEC_PARAM_BOND                  1                                           /**< Perform bonding. */
#define SEC_PARAM_MITM                  0                                           /**< Man In The Middle protection not required. */
#define SEC_PARAM_LESC                  0                                           /**< LE Secure Connections not enabled. */
#define SEC_PARAM_KEYPRESS              0                                           /**< Keypress notifications not enabled. */
#define SEC_PARAM_IO_CAPABILITIES       BLE_GAP_IO_CAPS_NONE                        /**< No I/O capabilities. */
#define SEC_PARAM_OOB                   0                                           /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE          7                                           /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE          16                                          /**< Maximum encryption key size. */

#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define TX_POWER_LEVEL                    (4)

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;                            /**< Handle of the current connection. */

static ble_dfu_t m_dfus;                                                            /**< Structure used to identify the DFU service. */


#ifdef CDEV_NUS_MODULE
static ble_nus_t                        m_nus;                                      /**< Structure to identify the Nordic UART Service. */
//static ble_uuid_t m_adv_uuids[] = {{BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}};  /**< Universally unique service identifier. */
#else
//static ble_uuid_t m_adv_uuids[] = {{BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE}}; /**< Universally unique service identifiers. */
#endif

static bool m_softdevice_init_flag;

bool ble_connect_on = false;
extern void cfg_examples_check_enter(void);

/**@brief Callback function for events in the DFU.
 *
 * @details This function will be called in case of an assert in the SoftDevice.

 * @param[in] p_dfu            Forward declaration of the ble_nus_t type.
 * @param[in] p_evt            
 */

static void ble_dfu_evt_handler(ble_dfu_t * p_dfu, ble_dfu_evt_t * p_evt)
{
    switch (p_evt->type)
    {
        case BLE_DFU_EVT_INDICATION_DISABLED:
            cPrintLog(CDBG_BLE_INFO, "Indication for BLE_DFU is disabled\r\n");
            break;

        case BLE_DFU_EVT_INDICATION_ENABLED:
            cPrintLog(CDBG_BLE_INFO, "Indication for BLE_DFU is enabled\r\n");
            break;

        case BLE_DFU_EVT_ENTERING_BOOTLOADER:
            cPrintLog(CDBG_BLE_INFO, "Device is entering bootloader mode!\r\n");
            break;
        default:
            cPrintLog(CDBG_BLE_INFO, "Unknown event from ble_dfu\r\n");
            break;
    }
}


#define BATTERY_LEVEL_MEAS_INTERVAL      APP_TIMER_TICKS(200, APP_TIMER_PRESCALER)  /**< Battery level measurement interval (ticks). */
#define USE_UICR_FOR_MAJ_MIN_VALUES 1
#define BATTERY_LEVEL_AVG_CNT 3

#if defined(USE_UICR_FOR_MAJ_MIN_VALUES)
#define MAJ_VAL_OFFSET_IN_BEACON_INFO   18                                /**< Position of the MSB of the Major Value in m_beacon_info array. */
#define UICR_ADDRESS                    0x10001080                        /**< Address of the UICR register used by this example. The major and minor versions to be encoded into the advertising data will be picked up from this location. */
#endif

unsigned int main_schedule_tick = 0;

APP_TIMER_DEF(m_main_timer_id);

#ifdef CDEV_NUS_MODULE
//APP_TIMER_DEF(m_nus_timer_id);
#endif

extern uint8_t frame_data[(SIGFOX_SEND_PAYLOAD_SIZE*2)+1];  //for hexadecimal

module_mode_t m_module_mode;
module_mode_t m_exception_mode;
bool m_init_excute;
module_parameter_t m_module_parameter;
bool m_module_parameter_update_req;
int m_module_ready_wait_timeout_tick = 0;

volatile bool main_wakeup_interrupt;

volatile bool mnfc_tag_on = false;
main_nfg_tag_on_callback mnfc_tag_on_CB = NULL;
volatile bool main_powerdown_request = false;

#ifdef CDEV_NUS_MODULE
nus_service_parameter_t m_nus_service_parameter;
#endif
module_peripheral_data_t m_module_peripheral_data;
module_peripheral_ID_t m_module_peripheral_ID;

#define MAX_REC_COUNT      3     /**< Maximum records count. */

/** @snippet [NFC Launch App usage_0] */
/* nRF Toolbox Android application package name */
#if 0
static const uint8_t m_android_package_name[] = {'n', 'o', '.', 'n', 'o', 'r', 'd', 'i', 'c', 's',
                                                 'e', 'm', 'i', '.', 'a', 'n', 'd', 'r', 'o', 'i',
                                                 'd', '.', 'n', 'r', 'f', 't', 'o', 'o', 'l', 'b',
                                                 'o', 'x'};
#else
#if 0
static const uint8_t m_android_package_name[] = {'h','t','t','p','s',':','/','/','p','l','a','y','.',
                                                'g','o','o','g','l','e','.','c','o','m','/','s','t',
                                                'o','r','e','/','a','p','p','s','/','d','e','t','a',
                                                'i','l','s','?','i','d','=','n','o','.','n','o','r',
                                                'd','i','c','s','e','m','i','.','a','n','d','r','o',
                                                'i','d','.','n','r','f','t','o','o','l','b','o','x',
                                                '&','h','l','=','e','n'};
#endif
#endif
/* nRF Toolbox application ID for Windows phone */
uint8_t m_ndef_msg_buf[256];

#ifdef CDEV_BATT_CHECK_MODULE
APP_TIMER_DEF(m_battery_timer_id);                                               /**< Battery measurement timer. */

#define ADC_REF_VOLTAGE_IN_MILLIVOLTS     600                                          /**< Reference voltage (in milli volts) used by ADC while doing conversion. */
#define ADC_PRE_SCALING_COMPENSATION      6                                            /**< The ADC is configured to use VDD with 1/3 prescaling as input. And hence the result of conversion is to be multiplied by 3 to get the actual value of the battery voltage.*/
#define ADC_REF_VBG_VOLTAGE_IN_MILLIVOLTS 1200                                         /**< Value in millivolts for voltage used as reference in ADC conversion on NRF51. */
#define ADC_INPUT_PRESCALER               3                                            /**< Input prescaler for ADC convestion on NRF51. */
#define ADC_RES_10BIT                     1024                                         /**< Maximum digital value for 10-bit ADC conversion. */

#if (CDEV_BOARD_TYPE == CDEV_BOARD_EVB)
#define DIODE_FWD_VOLT_DROP_MILLIVOLTS    50
#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE) (((((ADC_VALUE) * ADC_REF_VOLTAGE_IN_MILLIVOLTS) / ADC_RES_10BIT) * ADC_PRE_SCALING_COMPENSATION) * 5/3)
#elif  (CDEV_BOARD_TYPE == CDEV_BOARD_IHERE)
#define DIODE_FWD_VOLT_DROP_MILLIVOLTS    50
#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE) (((((ADC_VALUE) * ADC_REF_VOLTAGE_IN_MILLIVOLTS) / ADC_RES_10BIT) * ADC_PRE_SCALING_COMPENSATION) * 5/3)
#elif  (CDEV_BOARD_TYPE == CDEV_BOARD_IHERE_MINI)
#define DIODE_FWD_VOLT_DROP_MILLIVOLTS    50
#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE) (((((ADC_VALUE) * ADC_REF_VOLTAGE_IN_MILLIVOLTS) / ADC_RES_10BIT) * ADC_PRE_SCALING_COMPENSATION) * 5/3)
#else
#define DIODE_FWD_VOLT_DROP_MILLIVOLTS    0 //1344 //270                                          /**< Typical forward voltage drop of the diode (Part no: SD103ATW-7-F) that is connected in series with the voltage supply. This is the voltage drop when the forward current is 1mA. Source: Data sheet of 'SURFACE MOUNT SCHOTTKY BARRIER DIODE ARRAY' available at www.diodes.com. */
#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE) (((((ADC_VALUE) * ADC_REF_VOLTAGE_IN_MILLIVOLTS) / ADC_RES_10BIT) * ADC_PRE_SCALING_COMPENSATION) * 5/3)
#endif

//static ble_bas_t                        m_bas;                                   /**< Structure used to identify the battery service. */
static nrf_saadc_value_t adc_buf[2];
uint8_t   avg_report_volts;
uint16_t total_batt_lvl_in_milli_volts;
#else
uint8_t   avg_report_volts;
uint16_t total_batt_lvl_in_milli_volts;
#endif

static bool nus_service = false;

void nfc_uninit(void);
void main_set_module_state(module_mode_t state);
void advertising_start(bool start_flag, bool led_ctrl_flag);
bool main_schedule_state_is_idle(void);
void main_timer_schedule_stop(void);
void module_parameter_check_update(void);
void nus_send_data(char module);
uint32_t nus_send_id_pac_mac(uint8_t * send_buffer);


/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in]   line_num   Line number of the failing ASSERT call.
 * @param[in]   file_name  File name of the failing ASSERT call.
 */


void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

#ifdef CDEV_BATT_CHECK_MODULE
/**@brief Function for handling the Battery measurement timer timeout.
 *
 * @details This function will be called each time the battery level measurement timer expires.
 *          This function will start the ADC.
 *
 * @param[in] p_context   Pointer used for passing some arbitrary information (context) from the
 *                        app_start_timer() call to the timeout handler.
 */
static void battery_level_meas_timeout_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);
    #ifdef ADC_PRESENT
    nrf_drv_adc_sample();
    #else // SAADC_PRESENT
    uint32_t err_code;
    err_code = nrf_drv_saadc_sample();
    APP_ERROR_CHECK(err_code);
    #endif // ADC_PRESENT
}

void cfg_bas_timer_create(void)
{
    uint32_t   err_code;

    err_code = app_timer_create(&m_battery_timer_id,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                battery_level_meas_timeout_handler);
    APP_ERROR_CHECK(err_code);
}

void cfg_bas_timer_start(void)
{
    uint32_t    err_code;

    avg_report_volts = 0;
    total_batt_lvl_in_milli_volts = 0;
    err_code = app_timer_start(m_battery_timer_id, BATTERY_LEVEL_MEAS_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
}

static void un_adc_configure(void)
{
//    nrf_drv_saadc_abort();
    nrf_drv_saadc_channel_uninit(0);
    nrf_drv_saadc_uninit();
}

#if 0
/**@brief Function for handling the Battery Service events.
 *
 * @details This function will be called for all Battery Service events which are passed to the
 |          application.
 *
 * @param[in] p_bas  Battery Service structure.
 * @param[in] p_evt  Event received from the Battery Service.
 */
static void on_bas_evt(ble_bas_t * p_bas, ble_bas_evt_t * p_evt)
{
    uint32_t err_code;

    switch (p_evt->evt_type)
    {
        case BLE_BAS_EVT_NOTIFICATION_ENABLED:
            // Start battery timer
            err_code = app_timer_start(m_battery_timer_id, BATTERY_LEVEL_MEAS_INTERVAL, NULL);
            APP_ERROR_CHECK(err_code);
            break; // BLE_BAS_EVT_NOTIFICATION_ENABLED

        case BLE_BAS_EVT_NOTIFICATION_DISABLED:
            err_code = app_timer_stop(m_battery_timer_id);
            APP_ERROR_CHECK(err_code);
            break; // BLE_BAS_EVT_NOTIFICATION_DISABLED

        default:
            // No implementation needed.
            break;
    }
}

/**@brief Function for initializing the Battery Service.
 */
static void bas_init(void)
{
    uint32_t       err_code;
    ble_bas_init_t bas_init_obj;

    memset(&bas_init_obj, 0, sizeof(bas_init_obj));

    bas_init_obj.evt_handler          = on_bas_evt;
    bas_init_obj.support_notification = true;
    bas_init_obj.p_report_ref         = NULL;
    bas_init_obj.initial_batt_level   = 100;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init_obj.battery_level_char_attr_md.cccd_write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init_obj.battery_level_char_attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&bas_init_obj.battery_level_char_attr_md.write_perm);

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init_obj.battery_level_report_read_perm);

    err_code = ble_bas_init(&m_bas, &bas_init_obj);
    APP_ERROR_CHECK(err_code);
}
#endif

uint16_t get_avg_batt_lvl_in_milli_volts(void)
{
    return (total_batt_lvl_in_milli_volts/BATTERY_LEVEL_AVG_CNT);
}

 /**@brief Function for handling the ADC interrupt.
  *
  * @details  This function will fetch the conversion result from the ADC, convert the value into
  *           percentage and send it to peer.
  */
 void saadc_event_handler(nrf_drv_saadc_evt_t const * p_event)
 {
    static uint8_t  check_count = 0;
    if (p_event->type == NRF_DRV_SAADC_EVT_DONE)
    {
        nrf_saadc_value_t   adc_result;
        uint16_t            batt_lvl_in_milli_volts;
        uint32_t            err_code;

        adc_result = p_event->data.done.p_buffer[0];

        batt_lvl_in_milli_volts = ADC_RESULT_IN_MILLI_VOLTS(adc_result) + DIODE_FWD_VOLT_DROP_MILLIVOLTS;
        if(check_count++ < BATTERY_LEVEL_AVG_CNT)
        {
            err_code = nrf_drv_saadc_buffer_convert(p_event->data.done.p_buffer, 1);
            APP_ERROR_CHECK(err_code);

            total_batt_lvl_in_milli_volts += batt_lvl_in_milli_volts;
            err_code = app_timer_start(m_battery_timer_id, BATTERY_LEVEL_MEAS_INTERVAL, NULL);
            APP_ERROR_CHECK(err_code);
        }
        else
        {
            avg_report_volts = ((get_avg_batt_lvl_in_milli_volts()) / 100);
            check_count = 0;
            un_adc_configure();
            m_nus_service_parameter.battery_volt[0] = avg_report_volts;
            m_nus_service_parameter.module='B';
//            nus_send_data('B');
        }
        cPrintLog(CDBG_EXT_SEN_INFO, "BATTERY %d %d %d \n",batt_lvl_in_milli_volts,avg_report_volts, check_count);
    }
 }

/**@brief Function for configuring ADC to do battery level conversion.
 */
void adc_configure(void)
{
    #ifdef ADC_PRESENT
    ret_code_t err_code = nrf_drv_adc_init(NULL, adc_event_handler);
    APP_ERROR_CHECK(err_code);

    static nrf_drv_adc_channel_t channel =
        NRF_DRV_ADC_DEFAULT_CHANNEL(NRF_ADC_CONFIG_INPUT_DISABLED);
    // channel.config.config.input = NRF_ADC_CONFIG_SCALING_SUPPLY_ONE_THIRD;
    channel.config.config.input = (uint32_t)NRF_ADC_CONFIG_SCALING_SUPPLY_ONE_THIRD;
    nrf_drv_adc_channel_enable(&channel);

    err_code = nrf_drv_adc_buffer_convert(&adc_buf[0], 1);
    APP_ERROR_CHECK(err_code);
    #else //  SAADC_PRESENT
    ret_code_t err_code = nrf_drv_saadc_init(NULL, saadc_event_handler);
    APP_ERROR_CHECK(err_code);

#if (CDEV_BOARD_TYPE == CDEV_BOARD_IHERE) || (CDEV_BOARD_TYPE == CDEV_BOARD_IHERE_MINI)
    nrf_saadc_channel_config_t config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN1);
#else
    nrf_saadc_channel_config_t config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN0);
#endif
    err_code = nrf_drv_saadc_channel_init(0, &config);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(&adc_buf[0], 1);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(&adc_buf[1], 1);
    APP_ERROR_CHECK(err_code);
    #endif //ADC_PRESENT
}

#endif
#ifdef CDEV_NUS_MODULE

static void nus_data_init()
{
    memset(m_nus_service_parameter.gps_data,0,8);
    memset(m_nus_service_parameter.gps_cn0,0,1);
    memset(m_nus_service_parameter.wifi_data,0,12);
    memset(m_nus_service_parameter.temperature_data,0,2);
    m_nus_service_parameter.magnet_event = '0';
    m_nus_service_parameter.accellometer_event = '1';
}

/**@brief Callback function for time-out in NUS.
 *
 * @details This function will be called when the timer expires.

 * @param[in] p_context   unused parameter                  
 */

void nus_send_data(char module)
{
    static uint8_t nus_send_buffer[20];
    int strLen;
    uint32_t      err_code=0;
    memset(nus_send_buffer,0xFF,20);
    if(ble_connect_on) {
        switch(module)
        {
            case 'T':
                nus_send_buffer[0]='T';
                memcpy(&nus_send_buffer[1],m_nus_service_parameter.temperature_data,2);
                err_code = ble_nus_string_send(&m_nus, (uint8_t*)&nus_send_buffer, 3);
                break;
            case 'G':
                nus_send_buffer[0]='G';
                memcpy(&nus_send_buffer[1],m_nus_service_parameter.gps_data,8);
                nus_send_buffer[9]= m_nus_service_parameter.gps_cn0[0];
                err_code = ble_nus_string_send(&m_nus, (uint8_t*)&nus_send_buffer, 10);
                break;
            case 'W':
                nus_send_buffer[0]='W';
                memcpy(&nus_send_buffer[1],m_nus_service_parameter.wifi_data,6);
                nus_send_buffer[7]= m_nus_service_parameter.wifi_rssi[0];
                err_code = ble_nus_string_send(&m_nus, (uint8_t*)&nus_send_buffer, 8);
                break;
            case 'B':
                nus_send_buffer[0]='B';
                nus_send_buffer[1]= m_nus_service_parameter.battery_volt[0];
                err_code = ble_nus_string_send(&m_nus, (uint8_t*)&nus_send_buffer, 2);
                break;

            case 'A':
                nus_send_buffer[0]='A';
                memcpy(&nus_send_buffer[1],&m_nus_service_parameter.acc_x,2);
                memcpy(&nus_send_buffer[3],&m_nus_service_parameter.acc_y,2);
                memcpy(&nus_send_buffer[5],&m_nus_service_parameter.acc_z,2);
                err_code = ble_nus_string_send(&m_nus, (uint8_t*)&nus_send_buffer, 7);
                break;
            case 'I':
                err_code = nus_send_id_pac_mac(nus_send_buffer);
                break;
            case 'V':  //send version
                nus_send_buffer[0]='V';
                strLen = sprintf((char *)&nus_send_buffer[1], "%s_V%s_%02x", (const char *)m_cfg_model_name, (const char *)m_cfg_sw_ver, m_cfg_board_type);
                err_code = ble_nus_string_send(&m_nus, (uint8_t*)&nus_send_buffer, (strLen+1));
                strLen = sprintf((char *)&nus_send_buffer[1], "D %s", (const char *)m_cfg_build_date);
                err_code = ble_nus_string_send(&m_nus, (uint8_t*)&nus_send_buffer, (strLen+1));
                strLen = sprintf((char *)&nus_send_buffer[1], "T %s", (const char *)m_cfg_build_time);
                err_code = ble_nus_string_send(&m_nus, (uint8_t*)&nus_send_buffer, (strLen+1));
                break;

            default:
                break;
        }

        if (err_code != NRF_ERROR_INVALID_STATE)
        {
            APP_ERROR_CHECK(err_code);
        }
    }
}

#if 0
static void nus_timeout_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);
    uint32_t      err_code;
//    cPrintLog(CDBG_MAIN_LOG, "NUS SEND DATA\n",  __func__, __LINE__);
    if(ble_connect_on)
    {
        err_code = ble_nus_string_send(&m_nus, (uint8_t*)m_nus_service_parameter.gps_data, 16);
        //    err_code = ble_nus_string_send(&m_nus, (uint8_t*)&m_nus_service_parameter, sizeof(m_nus_service_parameter));
        if (err_code != NRF_ERROR_INVALID_STATE)
        {
            APP_ERROR_CHECK(err_code);
        }
    }
}

/**@brief timer create function for NUS.
 *
 * @details This function will be called when NUS starts.

 * @param[in]                   
 */

static void nus_timer_create()
{
    uint32_t      err_code;
    err_code = app_timer_create(&m_nus_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                nus_timeout_handler);
    APP_ERROR_CHECK(err_code);
}

/**@brief timer start function for NUS.
 *
 * @details This function will be called when NUS starts.

 * @param[in]                   
 */

static void nus_timer_start()
{
    uint32_t      err_code;

    err_code = app_timer_start(m_nus_timer_id, APP_TIMER_TICKS(1000*NUS_INTERVAL_TIME_DEFAULT, APP_TIMER_PRESCALER), NULL);
    APP_ERROR_CHECK(err_code);
}

/**@brief timer stop function for NUS.
 *
 * @details This function will be called when NUS stops.

 * @param[in]                   
 */

static void nus_timer_stop()
{
    uint32_t err_code;

    err_code = app_timer_stop(m_nus_timer_id);
    APP_ERROR_CHECK(err_code);
}
#endif

#if 0 //disable charging status (CDEV_BOARD_TYPE == CDEV_BOARD_IHERE_MINI)
APP_TIMER_DEF(m_charging_indicator_timer_id);
static void charging_indicator_timeout_handler(void * p_context)
{
    static char led_on_counter = 0;

    if(main_schedule_state_is_idle())
    {
        if(!cfg_get_ble_led_status())
        {
            if(nrf_gpio_pin_read(PIN_DEF_CHARGING_SIGNAL))
            {
//                cPrintLog(CDBG_COMMON_LOG, "====not chg\n");
                nrf_gpio_pin_write(PIN_DEF_BLE_LED_EN, 0);
                led_on_counter = 0;
            }
            else
            {
                if(++led_on_counter >= 4)
                {
//                    cPrintLog(CDBG_COMMON_LOG, "====led on\n");
                    nrf_gpio_pin_write(PIN_DEF_BLE_LED_EN, 1);
                    led_on_counter = 0;
                }
                else
                {

//                    cPrintLog(CDBG_COMMON_LOG, "====led off\n");
                    nrf_gpio_pin_write(PIN_DEF_BLE_LED_EN, 0);
                }
            }
        }
    }
}

static void charging_indicator_init(void)
{
    uint32_t      err_code;

    nrf_gpio_cfg_input(PIN_DEF_CHARGING_SIGNAL, NRF_GPIO_PIN_PULLUP);
    err_code = app_timer_create(&m_charging_indicator_timer_id, APP_TIMER_MODE_REPEATED, charging_indicator_timeout_handler);
    APP_ERROR_CHECK(err_code);
    
    err_code = app_timer_start(m_charging_indicator_timer_id, APP_TIMER_TICKS((500), APP_TIMER_PRESCALER), NULL);
    APP_ERROR_CHECK(err_code);
}
#else
#define charging_indicator_init()
#endif


/**@brief callback function for ble event in NUS.
 *
 * @details This function will be called when ble is connected

 * @param[in]                   
 */

static void on_nus_evt(ble_nus_t * p_nus, ble_evt_t * p_ble_evt)
{
    if ((p_nus == NULL) || (p_ble_evt == NULL))
    {
        return;
    }

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
 //           nus_timer_start();
            break;

        case BLE_GAP_EVT_DISCONNECTED:      
//            nus_timer_stop();
            if(nus_service)
            {
                ihere_mini_fast_schedule_start();
                nus_service = false;
            }
            break;
        case BLE_GATTS_EVT_WRITE:
            break;
        case BLE_GATTS_EVT_TIMEOUT:
            break;
        default:
            // No implementation needed.
            break;
    }

}

/**@brief Function for handling the data from the Nordic UART Service.
 *
 * @details This function will process the data received from the Nordic UART BLE Service and send
 *          it to the UART module.
 *
 * @param[in] p_nus    Nordic UART Service structure.
 * @param[in] p_data   Data to be send to UART module.
 * @param[in] length   Length of the data.
 */
/**@snippet [Handling the data received over BLE] */

extern bool mnus_acc_report;
static void nus_data_handler(ble_nus_t * p_nus, uint8_t * p_data, uint16_t length)
{
//    volatile uint32_t err_code;
//    uint8_t respose[4]="OK";
    if(length == 1)
    {
        switch(p_data[0])
        {
            case 'T':
                nus_send_data('I');
                main_wakeup_interrupt = true;
//                err_code = ble_nus_string_send(&m_nus, (uint8_t*)respose, 2);
                break;

            case 'L':
                nus_service = true;
                ihere_mini_normal_schedule_mode_change();
//                nus_timer_start();
                break;

            case 'E':
                nus_service = false;
                break;

            case 'S':
                mnus_acc_report = false;
                bma250_set_state(IO_SETUP);
                cfg_bma250_timers_stop();
                cfg_bma250_timers_start();
                break;

            case 'A' :
#if (CDEV_BOARD_TYPE == CDEV_BOARD_IHERE) || (CDEV_BOARD_TYPE == CDEV_BOARD_IHERE_MINI)
                mnus_acc_report = true;
                bma250_set_state(IO_SETUP);
                cfg_bma250_timers_stop();
                cfg_bma250_timers_start();
#endif
                break;

            case 'V':  //read version
                nus_send_data('V');
                break;

            default:
                break;
        }
    }
}
/**@snippet [Handling the data received over BLE] */

#endif

/**@brief Function for handling the data from FS.
 *
 * @details This function will set the data received from FS
 *          
 *
 * @param[in] item  configuration item
 */
unsigned int main_get_param_val(module_parameter_item_e item)
{
    unsigned int ret = 0;
    switch(item)
    {
        case module_parameter_item_idle_time:
            ret = m_module_parameter.idle_time;
            break;

        case module_parameter_item_beacon_interval:
            ret = m_module_parameter.beacon_interval;
            break;

        case module_parameter_item_wifi_scan_retry_time_sec:
            ret = m_module_parameter.wifi_scan_retry_time_sec;
            break;

        case module_parameter_item_start_wait_time_for_board_control_attach_sec:
            ret = m_module_parameter.start_wait_time_for_board_control_attach_sec;
            break;

        case module_parameter_item_start_wait_time_for_ctrl_mode_change_sec:
            ret = m_module_parameter.start_wait_time_for_ctrl_mode_change_sec;
            break;

        case module_parameter_item_gps_tracking_time_sec:
            ret = m_module_parameter.gps_acquire_tracking_time_sec;
            break;

        case module_parameter_item_boot_nfc_unlock:
            ret = m_module_parameter.boot_nfc_unlock;
            break;

        case module_parameter_item_fota_enable:
            ret = m_module_parameter.fota_enable;
            break;

        case module_parameter_item_scenario_mode:
            ret = m_module_parameter.scenario_mode;
            break;

        case module_parameter_item_magnetic_gpio_enable:
            ret = m_module_parameter.magnetic_gpio_enable;
            break;

        case module_parameter_item_wkup_gpio_enable:
            ret = m_module_parameter.wkup_gpio_enable;
            break;

        case module_parameter_item_wifi_testmode_enable:
            ret = m_module_parameter.wifi_testmode_enable;
            break;

        case module_parameter_item_snek_testmode_enable:
            ret = m_module_parameter.sigfox_snek_testmode_enable;
            break;

        case module_parameter_item_gps_cn0_current_savetime_enable:
            ret = m_module_parameter.cgps_cn0_current_savetime_enable;
            break;

        default:
            break;
    }
    return ret;
}

/**@brief Function for handling the data from FS.
 *
 * @details This function will get the data received from FS
 *          
 *
 * @param[in] item  configuration item
 */

void main_set_param_val(module_parameter_item_e item, unsigned int val)
{
    switch(item)
    {
        case module_parameter_item_idle_time:
            m_module_parameter.idle_time = (uint32_t)val;
            break;

        case module_parameter_item_beacon_interval:
            m_module_parameter.beacon_interval = (uint32_t)val;
            break;

        case module_parameter_item_wifi_scan_retry_time_sec:
            m_module_parameter.wifi_scan_retry_time_sec = (uint32_t)val;
#ifdef CDEV_WIFI_MODULE
            cWifi_set_retry_time(m_module_parameter.wifi_scan_retry_time_sec);
#endif
            break;
            
        case module_parameter_item_start_wait_time_for_board_control_attach_sec:
            m_module_parameter.start_wait_time_for_board_control_attach_sec = (uint32_t)val;
            break;

        case module_parameter_item_start_wait_time_for_ctrl_mode_change_sec:
            m_module_parameter.start_wait_time_for_ctrl_mode_change_sec = (uint32_t)val;
            break;

        case module_parameter_item_gps_tracking_time_sec:
            m_module_parameter.gps_acquire_tracking_time_sec = (uint32_t)val;
            break;

        case module_parameter_item_boot_nfc_unlock:
            m_module_parameter.boot_nfc_unlock = (bool)val;
            break;

        case module_parameter_item_fota_enable:
            m_module_parameter.fota_enable = (bool)val;
            break;

        case module_parameter_item_scenario_mode:
            m_module_parameter.scenario_mode = (uint16_t)val;
            break;

        case module_parameter_item_magnetic_gpio_enable:
            m_module_parameter.magnetic_gpio_enable = (bool)val;
            break;

        case module_parameter_item_wkup_gpio_enable:
            m_module_parameter.wkup_gpio_enable = (char)val;
            break;

        case module_parameter_item_wifi_testmode_enable:
            m_module_parameter.wifi_testmode_enable = (bool)val;
            break;

        case module_parameter_item_snek_testmode_enable:
            m_module_parameter.sigfox_snek_testmode_enable = (bool)val;
            break;

        case module_parameter_item_gps_cn0_current_savetime_enable:
            m_module_parameter.cgps_cn0_current_savetime_enable = (bool)val;
            break;

        default:
            break;
    }
}

/**@brief Function for handling the data from default value.
 *
 * @details This function will set the data if upgrading configuraion item
 *          
 */

static void module_parameter_default_init(void)
{
    memset(&m_module_parameter, 0, sizeof(m_module_parameter));
    m_module_parameter.magic_top = MODULE_PARAMETER_MAGIC_TOP;

    m_module_parameter.idle_time = MAIN_IDLE_TIME_DEFAULT;
    m_module_parameter.beacon_interval = BEACON_INTERVAL_TIME_DEFAULT;
    m_module_parameter.wifi_scan_retry_time_sec =  CWIFI_SEC_RETRY_TIMEOUT_SEC_DEFAULT;
    m_module_parameter.start_wait_time_for_board_control_attach_sec = START_WAIT_TIME_FOR_BOARD_CONTROL_ATTACH_SEC;
    m_module_parameter.start_wait_time_for_ctrl_mode_change_sec = START_WAIT_TIME_FOR_CTRL_MODE_CHANGE_SEC;
    m_module_parameter.gps_acquire_tracking_time_sec = CGPS_ACQUIRE_TRACKING_TIME_SEC;
    m_module_parameter.boot_nfc_unlock = MAIN_BOOT_NFC_UNLOCK_DEFAULT;
    m_module_parameter.fota_enable = MAIN_FOTA_ENABLE_DEFAULT;
    m_module_parameter.scenario_mode = MAIN_SCENARIO_MODE_DEFAULT;
    m_module_parameter.magnetic_gpio_enable = MAIN_MAGNETIC_GPIO_ENABLE_DEFAULT;  //MAIN_MAGNETIC_GPIO_ENABLE_DEFAULT
    m_module_parameter.wkup_gpio_enable = MAIN_WKUP_GPIO_ENABLE_DEFAULT;  //MAIN_WKUP_GPIO_ENABLE_DEFAULT
    m_module_parameter.wifi_testmode_enable = MAIN_WIFI_TESTMODE_ENABLE_DEFAULT;
    m_module_parameter.sigfox_snek_testmode_enable = MAIN_SIGFOX_SNEK_TESTMODE_ENABLE_DEFAULT;
    m_module_parameter.cgps_cn0_current_savetime_enable = CGPS_CN0_CURRENT_SAVETIME_ENABLE;

    m_module_parameter.magic_bottom = MODULE_PARAMETER_MAGIC_BOTTOM;
    m_module_parameter.guard_area_align4 = 0;
    
    m_module_parameter.board_ID = m_cfg_board_type;
}

static void module_parameter_fs_evt_handler(fs_evt_t const * const evt, fs_ret_t result);
FS_REGISTER_CFG(fs_config_t param_fs_config) =
{
    .callback  = module_parameter_fs_evt_handler, // Function for event callbacks.
    .num_pages = 1,      // Number of physical flash pages required.
    .priority  = 0xFE            // Priority for flash usage.
};

/**@brief Function for handling event from FS.
 *
 * @details This function will get the result received from FS
 *          
 *
 * @param[in] evt  unused
 * @param[in] result  fstorage return values
 */

static void module_parameter_fs_evt_handler(fs_evt_t const * const evt, fs_ret_t result)
{
    module_parameter_t *p_setting_val;
    if (result == FS_SUCCESS)
    {
        p_setting_val = (module_parameter_t *)(param_fs_config.p_start_addr);
        if(p_setting_val->magic_top == 0)cfg_board_reset();  //factory reset
    }
    else
    {
        // An error occurred.
        cPrintLog(CDBG_FLASH_ERR, "%s %d FS Evt Error!\n", __func__, __LINE__);
    }
}

/**@brief Function for handling address from FS.
 *
 * @details This function will get the address for writing configuration item
 *          
 *
 * @param[in] page_num  page number for writing configuration 
 * 
 * @return address of writing configuration
 */

static uint32_t const * address_of_page(uint16_t page_num)
{
    return param_fs_config.p_start_addr + (page_num * FS_MAX_WRITE_SIZE_WORDS /*NRF52 size is 1024*/);
}

static bool module_parameter_adjust_value(void)
{
    volatile int old_val;
    bool adjusted = false;
    if(((int)(m_module_parameter.idle_time) < 60) || ((int)(m_module_parameter.idle_time) > (60*60*24*7)))
    {
        adjusted = true;
        old_val = m_module_parameter.idle_time;
        if((int)(m_module_parameter.idle_time) < 60)m_module_parameter.idle_time = 60;
        else m_module_parameter.idle_time = (60*60*24*7);
        cPrintLog(CDBG_MAIN_LOG, "adjust idle_time %d to %d\n", old_val, m_module_parameter.idle_time);
    }

    if(((int)(m_module_parameter.beacon_interval) < 20) || ((int)(m_module_parameter.beacon_interval) > 10240))
    {
        adjusted = true;
        old_val = m_module_parameter.beacon_interval;
        if((int)(m_module_parameter.beacon_interval) < 20)m_module_parameter.beacon_interval = 20;
        else m_module_parameter.beacon_interval = 10240;
        cPrintLog(CDBG_MAIN_LOG, "adjust beacon_interval %d to %d\n", old_val, m_module_parameter.beacon_interval);
    }

    if(((int)(m_module_parameter.wifi_scan_retry_time_sec) < 0) || ((int)(m_module_parameter.wifi_scan_retry_time_sec) > CWIFI_SEC_RETRY_TIMEOUT_SEC_MAX))
    {
        adjusted = true;
        old_val = m_module_parameter.wifi_scan_retry_time_sec;
        if((int)(m_module_parameter.wifi_scan_retry_time_sec) < 0)m_module_parameter.wifi_scan_retry_time_sec = 0;  //0 is wifi off
        else m_module_parameter.wifi_scan_retry_time_sec = CWIFI_SEC_RETRY_TIMEOUT_SEC_MAX;
        cPrintLog(CDBG_MAIN_LOG, "adjust wifi_scan_retry_time_sec %d to %d\n", old_val, m_module_parameter.wifi_scan_retry_time_sec);
    }

    if(((int)(m_module_parameter.start_wait_time_for_board_control_attach_sec) < 1) || ((int)(m_module_parameter.start_wait_time_for_board_control_attach_sec) > CTBC_WAIT_READ_DEVICE_ID_TIME_OUT_TICK))
    {
        adjusted = true;
        old_val = m_module_parameter.start_wait_time_for_board_control_attach_sec;
        if((int)(m_module_parameter.start_wait_time_for_board_control_attach_sec) < 1)m_module_parameter.start_wait_time_for_board_control_attach_sec = 1;
        else m_module_parameter.start_wait_time_for_board_control_attach_sec = CTBC_WAIT_READ_DEVICE_ID_TIME_OUT_TICK;
        cPrintLog(CDBG_MAIN_LOG, "adjust start_wait_time_for_board_control_attach_sec %d to %d\n", old_val, m_module_parameter.start_wait_time_for_board_control_attach_sec);
    }

    if(((int)(m_module_parameter.start_wait_time_for_ctrl_mode_change_sec) < 1) || ((int)(m_module_parameter.start_wait_time_for_ctrl_mode_change_sec) > 30))
    {
        adjusted = true;
        old_val = m_module_parameter.start_wait_time_for_ctrl_mode_change_sec;
        if((int)(m_module_parameter.start_wait_time_for_ctrl_mode_change_sec) < 1)m_module_parameter.start_wait_time_for_ctrl_mode_change_sec = 1;
        else m_module_parameter.start_wait_time_for_ctrl_mode_change_sec = 30;
        cPrintLog(CDBG_MAIN_LOG, "adjust start_wait_time_for_ctrl_mode_change_sec %d to %d\n", old_val, m_module_parameter.start_wait_time_for_ctrl_mode_change_sec);
    }

    if(((int)(m_module_parameter.gps_acquire_tracking_time_sec) < 30) || ((int)(m_module_parameter.gps_acquire_tracking_time_sec) > 300))
    {
        adjusted = true;
        old_val = m_module_parameter.gps_acquire_tracking_time_sec;
        if((int)(m_module_parameter.gps_acquire_tracking_time_sec) < 30)m_module_parameter.gps_acquire_tracking_time_sec = 30;
        else m_module_parameter.gps_acquire_tracking_time_sec = 300;
        cPrintLog(CDBG_MAIN_LOG, "adjust gps_acquire_tracking_time_sec %d to %d\n", old_val, m_module_parameter.gps_acquire_tracking_time_sec);
    }

    if(((int)(m_module_parameter.boot_nfc_unlock) < 0) || ((int)(m_module_parameter.boot_nfc_unlock) > 1))
    {
        adjusted = true;
        old_val = m_module_parameter.boot_nfc_unlock;
        if((int)(m_module_parameter.boot_nfc_unlock) < 0)m_module_parameter.boot_nfc_unlock = 0;
        else m_module_parameter.boot_nfc_unlock = 1;
        cPrintLog(CDBG_MAIN_LOG, "adjust boot_nfc_unlock %d to %d\n", old_val, m_module_parameter.boot_nfc_unlock);
    }

    if(((int)(m_module_parameter.fota_enable) < 0) || ((int)(m_module_parameter.fota_enable) > 1))
    {
        adjusted = true;
        old_val = m_module_parameter.fota_enable;
        if((int)(m_module_parameter.fota_enable) < 0)m_module_parameter.fota_enable = 0;
        else m_module_parameter.fota_enable = 1;
        cPrintLog(CDBG_MAIN_LOG, "adjust fota_enable %d to %d\n", old_val, m_module_parameter.fota_enable);
    }

    if(((int)(m_module_parameter.scenario_mode) < 0) || ((int)(m_module_parameter.scenario_mode) > MODULE_SCENARIO_MODE_MAX))
    {
        adjusted = true;
        old_val = m_module_parameter.scenario_mode;
        if((int)(m_module_parameter.scenario_mode) < 0)m_module_parameter.scenario_mode = 0;
        else m_module_parameter.scenario_mode = MODULE_SCENARIO_MODE_MAX;
        cPrintLog(CDBG_MAIN_LOG, "adjust scenario_mode %d to %d\n", old_val, m_module_parameter.scenario_mode);
    }

    if(((int)(m_module_parameter.magnetic_gpio_enable) < 0) || ((int)(m_module_parameter.magnetic_gpio_enable) > 1))
    {
        adjusted = true;
        old_val = m_module_parameter.magnetic_gpio_enable;
        if((int)(m_module_parameter.magnetic_gpio_enable) < 0)m_module_parameter.magnetic_gpio_enable = 0;
        else m_module_parameter.magnetic_gpio_enable = 1;
        cPrintLog(CDBG_MAIN_LOG, "adjust magnetic_gpio_enable %d to %d\n", old_val, m_module_parameter.magnetic_gpio_enable);
    }

    if(((int)(m_module_parameter.wkup_gpio_enable) < 0) || ((int)(m_module_parameter.wkup_gpio_enable) > 2))
    {
        adjusted = true;
        old_val = m_module_parameter.wkup_gpio_enable;
        if((int)(m_module_parameter.wkup_gpio_enable) < 0)m_module_parameter.wkup_gpio_enable = 0;
        else m_module_parameter.wkup_gpio_enable = 2;
        cPrintLog(CDBG_MAIN_LOG, "adjust wkup_gpio_enable %d to %d\n", old_val, m_module_parameter.wkup_gpio_enable);
    }

    if(((int)(m_module_parameter.wifi_testmode_enable) < 0) || ((int)(m_module_parameter.wifi_testmode_enable) > 1))
    {
        adjusted = true;
        old_val = m_module_parameter.wifi_testmode_enable;
        if((int)(m_module_parameter.wifi_testmode_enable) < 0)m_module_parameter.wifi_testmode_enable = 0;
        else m_module_parameter.wifi_testmode_enable = 1;
        cPrintLog(CDBG_MAIN_LOG, "adjust wifi_testmode_enable %d to %d\n", old_val, m_module_parameter.wifi_testmode_enable);
    }

    if(((int)(m_module_parameter.sigfox_snek_testmode_enable) < 0) || ((int)(m_module_parameter.sigfox_snek_testmode_enable) > 1))
    {
        adjusted = true;
        old_val = m_module_parameter.sigfox_snek_testmode_enable;
        if((int)(m_module_parameter.sigfox_snek_testmode_enable) < 0)m_module_parameter.sigfox_snek_testmode_enable = 0;
        else m_module_parameter.sigfox_snek_testmode_enable = 1;
        cPrintLog(CDBG_MAIN_LOG, "adjust sigfox_snek_testmode_enable %d to %d\n", old_val, m_module_parameter.sigfox_snek_testmode_enable);
    }

    return adjusted;
}

static void module_parameter_init(void)
{
    fs_ret_t fs_ret;
    bool parameter_rebuild_req = false;

    if(FS_MAX_WRITE_SIZE_WORDS < sizeof(m_module_parameter))
    {
        cPrintLog(CDBG_FLASH_ERR, "%s %d page size:%d, param size:%d\n", __func__, __LINE__, FS_MAX_WRITE_SIZE_WORDS, sizeof(m_module_parameter));
        APP_ERROR_CHECK(NRF_ERROR_NO_MEM);  //fs_config num_pages size over
    }

    fs_ret = fs_init();
    if(fs_ret != FS_SUCCESS)
    {
        cPrintLog(CDBG_FLASH_ERR, "%s %d fs_init Error!\n", __func__, __LINE__);
    }

    memcpy(&m_module_parameter, param_fs_config.p_start_addr, sizeof(m_module_parameter));

    if(!(m_module_parameter.magic_top == MODULE_PARAMETER_MAGIC_TOP && m_module_parameter.magic_bottom == MODULE_PARAMETER_MAGIC_BOTTOM))
    {
        parameter_rebuild_req = true;
    }
    else if(m_module_parameter.board_ID != m_cfg_board_type)
    {
        parameter_rebuild_req = true;
    }

    if(!parameter_rebuild_req)
    {
        cPrintLog(CDBG_FLASH_INFO, "module_parameter loaded!\n");
//        cDataDumpPrintOut(CDBG_FLASH_INFO, param_fs_config.p_start_addr, 64);
    }
    else
    {
        if(m_module_parameter.magic_top == 0)cPrintLog(CDBG_MAIN_LOG, "factory reset!\n");
        cPrintLog(CDBG_FLASH_INFO, "fs init to default!\n");
        module_parameter_default_init();
        // Erase one page (page 0).
        fs_ret = fs_erase(&param_fs_config, address_of_page(0), 1, NULL);
        if (fs_ret == FS_SUCCESS)
        {
            fs_ret = fs_store(&param_fs_config, param_fs_config.p_start_addr, (uint32_t const *)&m_module_parameter, (sizeof(m_module_parameter)/4)+1, NULL);
            if (fs_ret != FS_SUCCESS)
            {
                cPrintLog(CDBG_FLASH_ERR, "%s %d fs_store error! %d\n", __func__, __LINE__, fs_ret);
            }
        }
        else
        {
            cPrintLog(CDBG_FLASH_ERR, "%s %d fs_erase error! %d\n", __func__, __LINE__, fs_ret);
        }
        nrf_delay_ms(10);
    }
    module_parameter_adjust_value();
#ifdef CDEV_WIFI_MODULE
    cWifi_set_retry_time(m_module_parameter.wifi_scan_retry_time_sec);
    cWifi_set_test_mode(m_module_parameter.wifi_testmode_enable);
#endif
    //for ID value cache
    memcpy(m_module_peripheral_ID.wifi_MAC_STA, m_module_parameter.wifi_MAC_STA, sizeof(m_module_peripheral_ID.wifi_MAC_STA));
    memcpy(m_module_peripheral_ID.sigfox_device_ID, m_module_parameter.sigfox_device_ID, sizeof(m_module_peripheral_ID.sigfox_device_ID));
    memcpy(m_module_peripheral_ID.sigfox_pac_code, m_module_parameter.sigfox_pac_code, sizeof(m_module_peripheral_ID.sigfox_pac_code));
}

static bool module_parameter_ID_value_update(void)
{
    bool ret = false;
    if((memcmp(m_module_peripheral_ID.wifi_MAC_STA, m_module_parameter.wifi_MAC_STA, sizeof(m_module_peripheral_ID.wifi_MAC_STA)) != 0)
        || (memcmp(m_module_peripheral_ID.sigfox_device_ID, m_module_parameter.sigfox_device_ID, sizeof(m_module_peripheral_ID.sigfox_device_ID)) != 0)
        || (memcmp(m_module_peripheral_ID.sigfox_pac_code, m_module_parameter.sigfox_pac_code, sizeof(m_module_peripheral_ID.sigfox_pac_code)) != 0)
    )
    {
        cPrintLog(CDBG_FLASH_INFO, "update ID Value!\n");
        //for ID value cache
        memcpy(m_module_parameter.wifi_MAC_STA, m_module_peripheral_ID.wifi_MAC_STA, sizeof(m_module_parameter.wifi_MAC_STA));
        memcpy(m_module_parameter.sigfox_device_ID, m_module_peripheral_ID.sigfox_device_ID, sizeof(m_module_parameter.sigfox_device_ID));
        memcpy(m_module_parameter.sigfox_pac_code, m_module_peripheral_ID.sigfox_pac_code, sizeof(m_module_parameter.sigfox_pac_code));

        m_module_parameter_update_req = true;
        module_parameter_check_update();
        ret = true;
    }
    return ret;
}

void module_parameter_check_update(void)
{
    fs_ret_t fs_ret;
    if(m_module_parameter_update_req)
    {
        nrf_delay_ms(1);
        cPrintLog(CDBG_FLASH_INFO, "%s %d excute parameter update!\n", __func__, __LINE__);
        fs_ret = fs_erase(&param_fs_config, address_of_page(0), 1, NULL);
        if (fs_ret == FS_SUCCESS)
        {
            fs_ret = fs_store(&param_fs_config, param_fs_config.p_start_addr, (uint32_t    const *)&m_module_parameter, (sizeof(m_module_parameter)/4)+1, NULL);
            if (fs_ret != FS_SUCCESS)
            {
                cPrintLog(CDBG_FLASH_ERR, "%s %d fs_store error! %d\n", __func__, __LINE__, fs_ret);
            }
        }
        else
        {
            cPrintLog(CDBG_FLASH_ERR, "%s %d fs_erase error! %d\n", __func__, __LINE__, fs_ret);
        }
        nrf_delay_ms(1);
        m_module_parameter_update_req = false;
    }
}

#if 0
/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
static void advertising_init(void)
{
    uint32_t      err_code;
    ble_advdata_t advdata;
    uint8_t       flags = BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;

    ble_advdata_manuf_data_t manuf_specific_data;

    manuf_specific_data.company_identifier = APP_COMPANY_IDENTIFIER;

#if defined(USE_UICR_FOR_MAJ_MIN_VALUES)
    // If USE_UICR_FOR_MAJ_MIN_VALUES is defined, the major and minor values will be read from the
    // UICR instead of using the default values. The major and minor values obtained from the UICR
    // are encoded into advertising data in big endian order (MSB First).
    // To set the UICR used by this example to a desired value, write to the address 0x10001080
    // using the nrfjprog tool. The command to be used is as follows.
    // nrfjprog --snr <Segger-chip-Serial-Number> --memwr 0x10001080 --val <your major/minor value>
    // For example, for a major value and minor value of 0xabcd and 0x0102 respectively, the
    // the following command should be used.
    // nrfjprog --snr <Segger-chip-Serial-Number> --memwr 0x10001080 --val 0xabcd0102
    uint16_t major_value = ((*(uint32_t *)UICR_ADDRESS) & 0xFFFF0000) >> 16;
    uint16_t minor_value = ((*(uint32_t *)UICR_ADDRESS) & 0x0000FFFF);

    uint8_t index = MAJ_VAL_OFFSET_IN_BEACON_INFO;

    m_beacon_info[index++] = MSB_16(major_value);
    m_beacon_info[index++] = LSB_16(major_value);

    m_beacon_info[index++] = MSB_16(minor_value);
    m_beacon_info[index++] = LSB_16(minor_value);
#endif

    manuf_specific_data.data.p_data = (uint8_t *) m_beacon_info;
    manuf_specific_data.data.size   = APP_BEACON_INFO_LENGTH;

    // Build and set advertising data.
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type             = BLE_ADVDATA_NO_NAME;
    advdata.flags                 = flags;
    advdata.p_manuf_specific_data = &manuf_specific_data;

    err_code = ble_advdata_set(&advdata, NULL);
    APP_ERROR_CHECK(err_code);

    // Initialize advertising parameters (used when starting advertising).
    memset(&m_adv_params, 0, sizeof(m_adv_params));

    m_adv_params.type        = BLE_GAP_ADV_TYPE_ADV_NONCONN_IND;
    m_adv_params.p_peer_addr = NULL;                             // Undirected advertisement.
    m_adv_params.fp          = BLE_GAP_ADV_FP_ANY;
    m_adv_params.interval    = MSEC_TO_UNITS(m_module_parameter.beacon_interval, UNIT_0_625_MS);  //NON_CONNECTABLE_ADV_INTERVAL;
    m_adv_params.timeout     = APP_CFG_NON_CONN_ADV_TIMEOUT;
}
#else

static void get_ble_mac_address()
{
    uint32_t err_code;
    ble_gap_addr_t      addr;

    err_code = sd_ble_gap_addr_get(&addr);
    APP_ERROR_CHECK(err_code);

    m_module_peripheral_ID.ble_MAC[0] = addr.addr[5];
    m_module_peripheral_ID.ble_MAC[1] = addr.addr[4];
    m_module_peripheral_ID.ble_MAC[2] = addr.addr[3];
    m_module_peripheral_ID.ble_MAC[3] = addr.addr[2];
    m_module_peripheral_ID.ble_MAC[4] = addr.addr[1];
    m_module_peripheral_ID.ble_MAC[5] = addr.addr[0];

}
/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
//    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
 //           err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
 //           APP_ERROR_CHECK(err_code);
            break;

        case BLE_ADV_EVT_IDLE:
            break;

        default:
            break;
    }
}


/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t err_code = NRF_SUCCESS;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_DISCONNECTED:
 //           err_code = bsp_indication_set(BSP_INDICATE_IDLE);
 //           APP_ERROR_CHECK(err_code);
            ble_connect_on = false;
            cPrintLog(CDBG_FCTRL_INFO, "BLE_GAP_EVT_DISCONNECTED.\r\n");
            break; // BLE_GAP_EVT_DISCONNECTED

        case BLE_GAP_EVT_CONNECTED:
//            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
//            APP_ERROR_CHECK(err_code);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            ble_connect_on = true;
            cPrintLog(CDBG_FCTRL_INFO, "BLE_GAP_EVT_CONNECTED.\r\n");
            break; // BLE_GAP_EVT_CONNECTED

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            cPrintLog(CDBG_FCTRL_INFO, "GATT Client Timeout.\r\n");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break; // BLE_GATTC_EVT_TIMEOUT

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            cPrintLog(CDBG_FCTRL_INFO, "GATT Server Timeout.\r\n");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break; // BLE_GATTS_EVT_TIMEOUT

        case BLE_EVT_USER_MEM_REQUEST:
            err_code = sd_ble_user_mem_reply(p_ble_evt->evt.gattc_evt.conn_handle, NULL);
            APP_ERROR_CHECK(err_code);
            break; // BLE_EVT_USER_MEM_REQUEST

        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
        {
            ble_gatts_evt_rw_authorize_request_t  req;
            ble_gatts_rw_authorize_reply_params_t auth_reply;

            req = p_ble_evt->evt.gatts_evt.params.authorize_request;

            if (req.type != BLE_GATTS_AUTHORIZE_TYPE_INVALID)
            {
                if ((req.request.write.op == BLE_GATTS_OP_PREP_WRITE_REQ)     ||
                    (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW) ||
                    (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_CANCEL))
                {
                    if (req.type == BLE_GATTS_AUTHORIZE_TYPE_WRITE)
                    {
                        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
                    }
                    else
                    {
                        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
                    }
                    auth_reply.params.write.gatt_status = APP_FEATURE_NOT_SUPPORTED;
                    err_code = sd_ble_gatts_rw_authorize_reply(p_ble_evt->evt.gatts_evt.conn_handle,
                                                               &auth_reply);
                    APP_ERROR_CHECK(err_code);
                }
            }
        } break; // BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST

#if (NRF_SD_BLE_API_VERSION == 3)
        case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
            err_code = sd_ble_gatts_exchange_mtu_reply(p_ble_evt->evt.gatts_evt.conn_handle,
                                                       NRF_BLE_MAX_MTU_SIZE);
            APP_ERROR_CHECK(err_code);
            break; // BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST
#endif

        default:
            // No implementation needed.
            break;
    }
}

/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t               err_code;
    ble_advdata_t          advdata;
    ble_adv_modes_config_t options;

    // Build advertising data struct to pass into @ref ble_advertising_init.
    memset(&advdata, 0, sizeof(advdata));
    if(m_module_parameter.scenario_mode == MODULE_SCENARIO_MWC_DEMO)
    {
        advdata.name_type               = BLE_ADVDATA_FULL_NAME;
        advdata.include_appearance      = false;
        advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    }
    else
    {
        advdata.name_type               = BLE_ADVDATA_FULL_NAME;
        advdata.include_appearance      = true;
        advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    }

//    options.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
//    options.uuids_complete.p_uuids  = m_adv_uuids;

    memset(&options, 0, sizeof(options));
    options.ble_adv_fast_enabled  = true;
    options.ble_adv_fast_interval = MSEC_TO_UNITS(m_module_parameter.beacon_interval, UNIT_0_625_MS);
        options.ble_adv_fast_timeout = 0; //APP_ADV_TIMEOUT_IN_SECONDS;

    err_code = ble_advertising_init(&advdata, NULL, &options, on_adv_evt, NULL);
    APP_ERROR_CHECK(err_code);
}
#endif

/**@brief Function for starting advertising.
 */
void advertising_start(bool start_flag, bool led_ctrl_flag)
{
    static bool started = false;
    uint32_t err_code;

    if(started != start_flag)
    {
        if(start_flag)
        {
            if(led_ctrl_flag)cfg_ble_led_control(true);
            err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
            if(err_code != NRF_ERROR_CONN_COUNT && err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
        }
        else
        {
            if(led_ctrl_flag)cfg_ble_led_control(false);        
            err_code = sd_ble_gap_adv_stop();
            if(err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
        }
        started = start_flag;
    }
}

/**@brief Function for handling Peer Manager events.
 *
 * @param[in] p_evt  Peer Manager event.
 */
static void pm_evt_handler(pm_evt_t const * p_evt)
{
    ret_code_t err_code;

    switch (p_evt->evt_id)
    {
        case PM_EVT_BONDED_PEER_CONNECTED:
        {
            cPrintLog(CDBG_BLE_DBG, "Connected to previously bonded device\r\n");
            err_code = pm_peer_rank_highest(p_evt->peer_id);
            if (err_code != NRF_ERROR_BUSY)
            {
                APP_ERROR_CHECK(err_code);
            }
        } break; // PM_EVT_BONDED_PEER_CONNECTED

        case PM_EVT_CONN_SEC_START:
            break; // PM_EVT_CONN_SEC_START

        case PM_EVT_CONN_SEC_SUCCEEDED:
        {
            cPrintLog(CDBG_BLE_DBG, "Link secured. Role: %d. conn_handle: %d, Procedure: %d\r\n",
                                 ble_conn_state_role(p_evt->conn_handle),
                                 p_evt->conn_handle,
                                 p_evt->params.conn_sec_succeeded.procedure);
            err_code = pm_peer_rank_highest(p_evt->peer_id);
            if (err_code != NRF_ERROR_BUSY)
            {
                APP_ERROR_CHECK(err_code);
            }
        } break; // PM_EVT_CONN_SEC_SUCCEEDED

        case PM_EVT_CONN_SEC_FAILED:
        {
            /** In some cases, when securing fails, it can be restarted directly. Sometimes it can
             *  be restarted, but only after changing some Security Parameters. Sometimes, it cannot
             *  be restarted until the link is disconnected and reconnected. Sometimes it is
             *  impossible, to secure the link, or the peer device does not support it. How to
             *  handle this error is highly application dependent. */
 
        } break; // PM_EVT_CONN_SEC_FAILED

        case PM_EVT_CONN_SEC_CONFIG_REQ:
        {
            // Reject pairing request from an already bonded peer.
            pm_conn_sec_config_t conn_sec_config = {.allow_repairing = false};
            pm_conn_sec_config_reply(p_evt->conn_handle, &conn_sec_config);
        } break; // PM_EVT_CONN_SEC_CONFIG_REQ

        case PM_EVT_STORAGE_FULL:
        {
            // Run garbage collection on the flash.
            err_code = fds_gc();
            if (err_code == FDS_ERR_BUSY || err_code == FDS_ERR_NO_SPACE_IN_QUEUES)
            {
                // Retry.
            }
            else
            {
                APP_ERROR_CHECK(err_code);
            }
        } break; // PM_EVT_STORAGE_FULL

        case PM_EVT_ERROR_UNEXPECTED:
            // Assert.
            APP_ERROR_CHECK(p_evt->params.error_unexpected.error);
            break; // PM_EVT_ERROR_UNEXPECTED

        case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
            break; // PM_EVT_PEER_DATA_UPDATE_SUCCEEDED

        case PM_EVT_PEER_DATA_UPDATE_FAILED:
            // Assert.
            APP_ERROR_CHECK_BOOL(false);
            break; // PM_EVT_PEER_DATA_UPDATE_FAILED

        case PM_EVT_PEER_DELETE_SUCCEEDED:
            break; // PM_EVT_PEER_DELETE_SUCCEEDED

        case PM_EVT_PEER_DELETE_FAILED:
            // Assert.
            APP_ERROR_CHECK(p_evt->params.peer_delete_failed.error);
            break; // PM_EVT_PEER_DELETE_FAILED

        case PM_EVT_PEERS_DELETE_SUCCEEDED:
            advertising_start(true, true);
            break; // PM_EVT_PEERS_DELETE_SUCCEEDED

        case PM_EVT_PEERS_DELETE_FAILED:
            // Assert.
            APP_ERROR_CHECK(p_evt->params.peers_delete_failed_evt.error);
            break; // PM_EVT_PEERS_DELETE_FAILED

        case PM_EVT_LOCAL_DB_CACHE_APPLIED:
            break; // PM_EVT_LOCAL_DB_CACHE_APPLIED

        case PM_EVT_LOCAL_DB_CACHE_APPLY_FAILED:
            // The local database has likely changed, send service changed indications.
            pm_local_database_has_changed();
            break; // PM_EVT_LOCAL_DB_CACHE_APPLY_FAILED

        case PM_EVT_SERVICE_CHANGED_IND_SENT:
            break; // PM_EVT_SERVICE_CHANGED_IND_SENT

        case PM_EVT_SERVICE_CHANGED_IND_CONFIRMED:
            break; // PM_EVT_SERVICE_CHANGED_IND_SENT

        default:
            // No implementation needed.
            break;
    }
}

/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the BLE Stack event interrupt handler after a BLE stack
 *          event has been received.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    /** The Connection state module has to be fed BLE events in order to function correctly
     * Remember to call ble_conn_state_on_ble_evt before calling any ble_conns_state_* functions. */
    ble_conn_state_on_ble_evt(p_ble_evt);
    pm_on_ble_evt(p_ble_evt);
    ble_conn_params_on_ble_evt(p_ble_evt);
//    bsp_btn_ble_on_ble_evt(p_ble_evt);
#ifdef CDEV_NUS_MODULE
    ble_nus_on_ble_evt(&m_nus, p_ble_evt);
    on_nus_evt(&m_nus, p_ble_evt);
#endif
#ifdef CDEV_BATT_CHECK_MODULE
//    ble_bas_on_ble_evt(&m_bas, p_ble_evt);
#endif
    on_ble_evt(p_ble_evt);
    ble_advertising_on_ble_evt(p_ble_evt);
    if(m_module_parameter.fota_enable)
    {
        ble_dfu_on_ble_evt(&m_dfus, p_ble_evt);
    }
    /*YOUR_JOB add calls to _on_ble_evt functions from each service your application is using
       ble_xxs_on_ble_evt(&m_xxs, p_ble_evt);
       ble_yys_on_ble_evt(&m_yys, p_ble_evt);
     */
}

/**@brief Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in]   sys_evt   System stack event.
 */

static void sys_evt_dispatch(uint32_t event)
{
    // Dispatch the system event to the fstorage module, where it will be
    // dispatched to the Flash Data Storage (FDS) module.
    fs_sys_event_handler(event);

    // Dispatch to the Advertising module last, since it will check if there are any
    // pending flash operations in fstorage. Let fstorage process system events first,
    // so that it can report correctly to the Advertising module.
    ble_advertising_on_sys_evt(event);

}

/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;

    nrf_clock_lf_cfg_t clock_lf_cfg = NRF_CLOCK_LFCLKSRC_250_PPM;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);

    ble_enable_params_t ble_enable_params;
    err_code = softdevice_enable_get_default_config(CENTRAL_LINK_COUNT,
                                                    PERIPHERAL_LINK_COUNT,
                                                    &ble_enable_params);
    APP_ERROR_CHECK(err_code);
#ifdef CDEV_NUS_MODULE
    ble_enable_params.common_enable_params.vs_uuid_count   = 2;
#endif
    //Check the ram settings against the used number of links
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT,PERIPHERAL_LINK_COUNT);

#if (NRF_SD_BLE_API_VERSION == 3)
    ble_enable_params.gatt_enable_params.att_mtu = NRF_BLE_MAX_MTU_SIZE;
#endif

    // Enable BLE stack.
    err_code = softdevice_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);
    m_softdevice_init_flag = true;

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for System events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);

}

/**@brief Function for the Peer Manager initialization.
 *
 * @param[in] erase_bonds  Indicates whether bonding information should be cleared from
 *                         persistent storage during initialization of the Peer Manager.
 */
static void peer_manager_init(bool erase_bonds)
{
    ble_gap_sec_params_t sec_param;
    ret_code_t           err_code;

    err_code = pm_init();
    APP_ERROR_CHECK(err_code);

//    if (erase_bonds)
//    {
//        err_code = pm_peers_delete();
//        APP_ERROR_CHECK(err_code);
//    }

    memset(&sec_param, 0, sizeof(ble_gap_sec_params_t));

    // Security parameters to be used for all security procedures.
    sec_param.bond           = SEC_PARAM_BOND;
    sec_param.mitm           = SEC_PARAM_MITM;
    sec_param.lesc           = SEC_PARAM_LESC;
    sec_param.keypress       = SEC_PARAM_KEYPRESS;
    sec_param.io_caps        = SEC_PARAM_IO_CAPABILITIES;
    sec_param.oob            = SEC_PARAM_OOB;
    sec_param.min_key_size   = SEC_PARAM_MIN_KEY_SIZE;
    sec_param.max_key_size   = SEC_PARAM_MAX_KEY_SIZE;
    sec_param.kdist_own.enc  = 1;
    sec_param.kdist_own.id   = 1;
    sec_param.kdist_peer.enc = 1;
    sec_param.kdist_peer.id  = 1;

    err_code = pm_sec_params_set(&sec_param);
    APP_ERROR_CHECK(err_code);

    err_code = pm_register(pm_evt_handler);
    APP_ERROR_CHECK(err_code);
}


#define APP_BEACON_INFO_LENGTH          0x11
static uint8_t m_beacon_info[APP_BEACON_INFO_LENGTH];

/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    if(m_module_parameter.scenario_mode == MODULE_SCENARIO_MWC_DEMO)
    {
        unsigned int index = 0;

        memset(m_beacon_info, 0, sizeof(m_beacon_info));
        strcpy((char*)&m_beacon_info[index], "SFM");
        index += 3;
        memcpy(&m_beacon_info[index], (m_nus_service_parameter.wifi_data), CWIFI_BSSID_SIZE);
        index += CWIFI_BSSID_SIZE;
        memcpy(&m_beacon_info[index], (m_nus_service_parameter.ap_key), strlen((const char *)m_nus_service_parameter.ap_key));
        index += strlen((const char *)m_nus_service_parameter.ap_key);
        
        err_code = sd_ble_gap_device_name_set(&sec_mode,
                                              (const uint8_t *)m_beacon_info,
                                              index);
        APP_ERROR_CHECK(err_code);
    }
    else
    {
#if (CDEV_BOARD_TYPE == CDEV_BOARD_IHERE)
        err_code = sd_ble_gap_device_name_set(&sec_mode,
                                              (const uint8_t *)ASSETTRACKER_NAME,
                                              strlen(ASSETTRACKER_NAME));
#elif (CDEV_BOARD_TYPE == CDEV_BOARD_IHERE_MINI)
        err_code = sd_ble_gap_device_name_set(&sec_mode,
                                              (const uint8_t *)MINI_ASSETTRACKER_NAME,
                                              strlen(MINI_ASSETTRACKER_NAME));
#else
        err_code = sd_ble_gap_device_name_set(&sec_mode,
                                              (const uint8_t *)DEVICE_NAME,
                                              strlen(DEVICE_NAME));
#endif
        APP_ERROR_CHECK(err_code);
    }

    /* YOUR_JOB: Use an appearance value matching the application's use case.
       err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_);
       APP_ERROR_CHECK(err_code); */

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_tx_power_set(TX_POWER_LEVEL);
    APP_ERROR_CHECK(err_code);
}



/**@brief Function for handling the YYY Service events.
 * YOUR_JOB implement a service handler function depending on the event the service you are using can generate
 *
 * @details This function will be called for all YY Service events which are passed to
 *          the application.
 *
 * @param[in]   p_yy_service   YY Service structure.
 * @param[in]   p_evt          Event received from the YY Service.
 *
 *
   static void on_yys_evt(ble_yy_service_t     * p_yy_service,
                       ble_yy_service_evt_t * p_evt)
   {
    switch (p_evt->evt_type)
    {
        case BLE_YY_NAME_EVT_WRITE:
            APPL_LOG("[APPL]: charact written with value %s. \r\n", p_evt->params.char_xx.value.p_str);
            break;

        default:
            // No implementation needed.
            break;
    }
   }*/

/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    uint32_t err_code;
    ble_dfu_init_t dfus_init;
//    ble_gap_addr_t      addr;

#ifdef CDEV_NUS_MODULE
    ble_nus_init_t nus_init;
    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = nus_data_handler;

    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
#endif
   //   Initialize the Device Firmware Update Service.
    memset(&dfus_init, 0, sizeof(dfus_init));

    dfus_init.evt_handler                               = ble_dfu_evt_handler;
    dfus_init.ctrl_point_security_req_write_perm        = SEC_SIGNED;
    dfus_init.ctrl_point_security_req_cccd_write_perm   = SEC_SIGNED;

    if(m_module_parameter.fota_enable)
    {
        if(m_cfg_available_bootloader_detected)
        {
            cPrintLog(CDBG_BLE_ERR, "bootloader detected!\n");
        }
        else
        {
            cPrintLog(CDBG_BLE_ERR, "=== warning! bootloader not detected! ===\n");
        }
        err_code = ble_dfu_init(&m_dfus, &dfus_init);
        APP_ERROR_CHECK(err_code);
    }

/*
    err_code = sd_ble_gap_addr_get(&addr);
    APP_ERROR_CHECK(err_code);

    m_module_peripheral_ID.ble_MAC[0] = addr.addr[5];
    m_module_peripheral_ID.ble_MAC[1] = addr.addr[4];
    m_module_peripheral_ID.ble_MAC[2] = addr.addr[3];
    m_module_peripheral_ID.ble_MAC[3] = addr.addr[2];
    m_module_peripheral_ID.ble_MAC[4] = addr.addr[1];
    m_module_peripheral_ID.ble_MAC[5] = addr.addr[0];
    */
    cPrintLog(CDBG_FCTRL_DBG, "MAC ADDR:");
    cDataDumpPrintOut(CDBG_FCTRL_DBG, m_module_peripheral_ID.ble_MAC, BLE_GAP_ADDR_LEN);

    /* YOUR_JOB: Add code to initialize the services used by the application.
       uint32_t                           err_code;
       ble_xxs_init_t                     xxs_init;
       ble_yys_init_t                     yys_init;

       // Initialize XXX Service.
       memset(&xxs_init, 0, sizeof(xxs_init));

       xxs_init.evt_handler                = NULL;
       xxs_init.is_xxx_notify_supported    = true;
       xxs_init.ble_xx_initial_value.level = 100;

       err_code = ble_bas_init(&m_xxs, &xxs_init);
       APP_ERROR_CHECK(err_code);

       // Initialize YYY Service.
       memset(&yys_init, 0, sizeof(yys_init));
       yys_init.evt_handler                  = on_yys_evt;
       yys_init.ble_yy_initial_value.counter = 0;

       err_code = ble_yy_service_init(&yys_init, &yy_init);
       APP_ERROR_CHECK(err_code);
     */
}

/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module which
 *          are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by simply
 *                setting the disconnect_on_fail config parameter, but instead we use the event
 *                handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for doing power management.
 */
static void power_manage(void)
{
    uint32_t err_code;
    if(m_softdevice_init_flag)
    {
        err_code = sd_app_evt_wait();
        if(m_softdevice_init_flag)
        {
            APP_ERROR_CHECK(err_code);
        }
    }
    else
    {
        __WFE();
    }
}


/**
 * @brief Function for starting idle timer.
 */
void main_timer_idle_start(void)
{
    uint32_t      err_code;

    err_code = app_timer_start(m_main_timer_id, APP_TIMER_TICKS(1000*m_module_parameter.idle_time, APP_TIMER_PRESCALER), NULL);
    APP_ERROR_CHECK(err_code);
}

/**
 * @brief Function for starting main schedule timer.
 *
*/
void main_timer_schedule_start(void)
{
    uint32_t      err_code;

    err_code = app_timer_start(m_main_timer_id, APP_TIMER_TICKS(APP_MAIN_SCHEDULE_MS, APP_TIMER_PRESCALER), NULL);
    APP_ERROR_CHECK(err_code);
}

/**
 * @brief Function for restarting main schedule at idle state
 *
*/
void main_timer_schedule_restart_check_idle(void)
{
    if(main_schedule_state_is_idle())
    {
        if(m_module_parameter.scenario_mode == MODULE_SCENARIO_IHERE_MINI)
        {
            ihere_mini_current_schedule_start();
        }
        else
        {
            main_timer_schedule_stop();
            main_timer_schedule_start();
            if(m_module_parameter.scenario_mode == MODULE_SCENARIO_MWC_DEMO)
            {
                advertising_start(false, true);
            }
        }
    }
}

/**
 * @brief Function for starting variable timer.
 */
void main_timer_variable_schedule_start(unsigned int schedule_time_sec)
{
    uint32_t      err_code;

    err_code = app_timer_start(m_main_timer_id, APP_TIMER_TICKS(1000*schedule_time_sec, APP_TIMER_PRESCALER), NULL);
    APP_ERROR_CHECK(err_code);
}

/**
 * @brief Function for stop  main schedule timer.
 */
void main_timer_schedule_stop(void)
{
    uint32_t      err_code;
    err_code = app_timer_stop(m_main_timer_id);
    APP_ERROR_CHECK(err_code);
}

/**
 * @brief Function for checking for entering  the scenario mode into the test mode
 *
 * @return true if possible, nor false
*/
bool main_check_ctrl_mode_allowed_state(void)
{
    bool ret = false;

    if(m_module_parameter.scenario_mode == MODULE_SCENARIO_MWC_DEMO)
    {
        if( m_module_mode == NONE
           || m_module_mode == WAIT_CTRL_MODE_CHANGE
           || m_module_mode == MANUAL_MODE
        )
        {
            ret = true;
        }
    }
    else
    {
        if( m_module_mode == NONE
           || m_module_mode == ACC  //boot step state ....
           || m_module_mode == WAIT_CTRL_MODE_CHANGE
           || m_module_mode == WIFI_BYPASS
           || m_module_mode == SIGFOX_BYPASS
           || m_module_mode == MANUAL_MODE
           || m_module_mode == GPS_BYPASS
           || m_module_mode == BLE_CONTROL
           || m_module_mode == ACC_BYPASS
        )
        {
            ret = true;
        }
    }
    
    if(!ret)
    {
        cPrintLog(CDBG_TBC_INFO, "%d mode change not allowed %d\n", __LINE__, m_module_mode);
    }
    return ret;
}

/**@brief Function for setting main state in main scheduler.
 *
 * @param[in] state  set the next state in main scheduler.
 *                         
 */

void main_set_module_state(module_mode_t state)
{
    m_init_excute = true; /*for run initial*/
    m_module_ready_wait_timeout_tick = 0; /*for timeout check*/
    m_module_mode = state;
}


bool main_schedule_state_is_accbypass(void)
{
    if(m_module_mode == ACC_BYPASS)
        return true;
    else
        return false;
}

/**@brief Function for indicating whether the current state is idle or not.
 *
 * @return true if it is idle, nor false
 *                         
 */


bool main_schedule_state_is_idle(void)
{
    if(m_module_mode == IDLE)
        return true;
    else
        return false;
}

void main_wkup_det_callback(void)
{
    if(main_schedule_state_is_idle())
    {
        cTBC_write_state_noti("WKUP_DET");
        main_wakeup_interrupt = true;
    }
}

void main_magnet_attach_callback(void)
{
    if(main_schedule_state_is_idle())
    {
        main_wakeup_interrupt = true;
    }
}

/**
 * @brief Callback function for handling main state  from main schedule timer.
 * @details manage main scheduling in normal demo
 */

static void main_schedule_timeout_handler_asset_tracker(void * p_context)
{
    UNUSED_PARAMETER(p_context);
#ifdef CDEV_GPS_MODULE
    bool send_gps_data_req = false;
    uint8_t gps_send_data[SIGFOX_SEND_PAYLOAD_SIZE];
    int get_nmea_result = 0;
    uint8_t *nmea_Buf;
    static int work_mode = GPS_START;  //module_work_e
#endif
    static bool old_ble_led = false;

    main_schedule_tick++;
    switch(m_module_mode)
    {
        case ACC:
            if(bma250_get_state() == NONE_ACC)
            {
                cPrintLog(CDBG_FCTRL_INFO, "%s %d ACC MODULE started\n",  __func__, __LINE__);
                bma250_set_state(SET_S);
                cfg_bma250_timers_start();
            }
            else if(bma250_get_state() == EXIT_ACC)
            {
                cfg_bma250_timers_stop();
//                bma250_set_state(NONE_ACC);
            }
            main_set_module_state(WAIT_CTRL_MODE_CHANGE);
            break;

        case WAIT_CTRL_MODE_CHANGE:
#if defined(FEATURE_CFG_BYPASS_CONTROL)
            if(m_exception_mode != NONE)
            {
                main_set_module_state(IDLE);
                break;
            }

            ++m_module_ready_wait_timeout_tick;
            if(cTBC_check_host_connected())
            {
                if(m_module_ready_wait_timeout_tick > (APP_MAIN_SCHEDULE_HZ * m_module_parameter.start_wait_time_for_ctrl_mode_change_sec))
                {
                    main_set_module_state(SCENARIO_MODE);
                }
                else
                {
                    main_set_module_state(MANUAL_MODE);
                }
            }
            else
            {
                if(m_module_ready_wait_timeout_tick > (APP_MAIN_SCHEDULE_HZ * m_module_parameter.start_wait_time_for_board_control_attach_sec))
                {
                    main_set_module_state(SCENARIO_MODE);
                }
                else
                {
                    if((m_module_ready_wait_timeout_tick == 1) || ((m_module_ready_wait_timeout_tick % 10) == 0))
                        cPrintLog(CDBG_FCTRL_INFO, "%s %d Wait for host conect! %d\n", __func__, __LINE__, m_module_ready_wait_timeout_tick);
                }
            }
#else
            main_set_module_state(MAIN_SCENARIO_LOOP);
#endif
            break;

        case MAIN_SCENARIO_LOOP:
            cPrintLog(CDBG_FCTRL_INFO, "==MAIN_SCENARIO_LOOP %u==\n", main_schedule_tick);
            main_set_module_state(BATTERY_CHECK);
            cfg_board_common_power_control(module_comm_pwr_common, true);
            break;

        case BATTERY_CHECK:
#ifdef CDEV_BATT_CHECK_MODULE
            ++m_module_ready_wait_timeout_tick;
            if(m_init_excute)
            {
                adc_configure();
                cfg_bas_timer_start();
                m_init_excute = false;
            }
            else
            {
                if(m_module_ready_wait_timeout_tick >= (APP_MAIN_SCHEDULE_HZ * 2))  //wait 2 sec
                {
                    bool log_once_flag = false;
                    uint16_t avg_batt_lvl_in_milli_volts;

                    if(m_module_ready_wait_timeout_tick == (APP_MAIN_SCHEDULE_HZ * 2))log_once_flag=true;
                    m_nus_service_parameter.battery_volt[0] = avg_report_volts;
                    m_nus_service_parameter.module='B';
                    avg_batt_lvl_in_milli_volts = get_avg_batt_lvl_in_milli_volts();
                    if(3000 <= avg_batt_lvl_in_milli_volts && 5200 >= avg_batt_lvl_in_milli_volts)
                    {
                        if(avg_batt_lvl_in_milli_volts < 3550)  //low battery
                        {
                            if(log_once_flag)cPrintLog(CDBG_MAIN_LOG, "Battery Low:%d\n", avg_batt_lvl_in_milli_volts);
                            main_powerdown_request = true;
                        }
                        else if(avg_batt_lvl_in_milli_volts < 3650)
                        {
                            if(log_once_flag)cPrintLog(CDBG_MAIN_LOG, "Battery Warning:%d\n", avg_batt_lvl_in_milli_volts);
                            if(m_module_ready_wait_timeout_tick > (APP_MAIN_SCHEDULE_HZ * 4))  //wait more 2 sec for warning noti
                            {
                                main_set_module_state(TMP);   //battery value is enough
                            }
                            else
                            {
                                if(((m_module_ready_wait_timeout_tick % APP_MAIN_SCHEDULE_HZ) == 0))
                                {
                                    cfg_ble_led_control(false);
                                }
                                else if(((m_module_ready_wait_timeout_tick % APP_MAIN_SCHEDULE_HZ) == 2))
                                {
                                    cfg_ble_led_control(true);
                                }
                            }
                        }
                        else
                        {
                            main_set_module_state(TMP);   //battery value is enough
                        }
                    }
                    else
                    {
                        if(log_once_flag)cPrintLog(CDBG_MAIN_LOG, "ADC Not Available:%d\n", avg_batt_lvl_in_milli_volts);
                        main_set_module_state(TMP);  //battery value is not available
                    }
                }
            }
#else
            main_set_module_state(TMP);
#endif
            break;

        case TMP:
            if(tmp102_get_state() == NONE_TMP)
            {
                cTBC_write_state_noti("Temperature");
                cPrintLog(CDBG_FCTRL_INFO, "%s %d TMP MODULE started\n",  __func__, __LINE__);
                tmp102_set_state(TMP_SET_S);
                cfg_tmp102_timers_start();
            }
            else if(tmp102_get_state() == EXIT_TMP)
            {
                cfg_tmp102_timers_stop();
                tmp102_set_state(NONE_TMP);
                main_set_module_state(GPS);
//                nus_send_data('T');
            }
            break;
        case BLE:
            cTBC_write_state_noti("BleAdvertising");
            cPrintLog(CDBG_BLE_INFO, "BLE MODULE started\r\n");
//            advertising_start(true, true);
            if(cfg_is_3colorled_contorl())
            {
                cfg_ble_led_control(false);
            }
            cPrintLog(CDBG_FCTRL_INFO, "%s %d BLE MODULE started\n", __func__, __LINE__);
#ifdef CDEV_NUS_MODULE
            m_nus_service_parameter.magnet_event = '0';
            m_nus_service_parameter.accellometer_event = '0';
#endif
            main_timer_schedule_stop();
            main_timer_idle_start();
            main_set_module_state(IDLE);
            cfg_board_common_power_control(module_comm_pwr_common, false);
            break;
        case GPS:
#ifdef CDEV_GPS_MODULE
            memset(gps_send_data, 0, sizeof(gps_send_data));

            if(work_mode == GPS_START)
            {
                if(cfg_is_3colorled_contorl())
                {
                    old_ble_led = cfg_ble_led_control(false);
                    cfg_wkup_output_control(true);
                }
                if(cGps_status_available() == CGPS_Result_OK)
                {
                    cTBC_write_state_noti("GPS");
                    cPrintLog(CDBG_FCTRL_INFO, "%s %d GPS MODULE started  \n", __func__, __LINE__);
                    cGps_nmea_acquire_request();                  
                    work_mode = GPS_WORK; //wait scan
                }
                else
                {
                    work_mode = GPS_END;
                }
            }
            else if(work_mode == GPS_WORK)
            {
                if(cGps_acquire_tracking_check() == CGPS_Result_OK)
                {
                    work_mode = GPS_END;
                }
                else if (cGps_acquire_tracking_check() == CGPS_Result_NoData)
                {
                    while(cGps_bus_busy_check()){nrf_delay_ms(100);};
                    work_mode = GPS_START;
                    if(cfg_is_3colorled_contorl())
                    {
                        cfg_ble_led_control(old_ble_led);
                        cfg_wkup_output_control(false);
                    }
                    main_set_module_state(WIFI);
                    nus_send_data('G');
                }
                else if(cGps_acquire_tracking_check() == CGPS_Result_NotStarted)
                {
                    while(cGps_bus_busy_check()){nrf_delay_ms(100);};
                    work_mode = GPS_START;
                    if(cfg_is_3colorled_contorl())
                    {
                        cfg_ble_led_control(old_ble_led);
                        cfg_wkup_output_control(false);
                    }
                    main_set_module_state(WIFI);
                    nus_send_data('G');
                }
            }
            else if(work_mode == GPS_END)
            {
                get_nmea_result = cGps_nmea_get_bufPtr(&nmea_Buf);
                if(get_nmea_result == CGPS_Result_OK)
                {
                    memcpy(gps_send_data, nmea_Buf, sizeof(gps_send_data));
                    send_gps_data_req = true;
                }
                if(send_gps_data_req)
                {
                    cfg_bin_2_hexadecimal(gps_send_data, SIGFOX_SEND_PAYLOAD_SIZE, (char *)frame_data);  //set sigfox payload
                    cPrintLog(CDBG_FCTRL_INFO, "[GPS] %d send request sigfox! frame_data:[%s]  \n", __LINE__, frame_data);
                    send_gps_data_req = false;
                    work_mode = GPS_START;
#ifdef CDEV_NUS_MODULE
                    memcpy(m_nus_service_parameter.gps_data, gps_send_data, sizeof(m_nus_service_parameter.gps_data));
//                    memcpy(m_nus_service_parameter.temperature_data, &gps_send_data[9], sizeof(m_nus_service_parameter.temperature_data));
                    m_nus_service_parameter.module='G';
#endif
                    if(cfg_is_3colorled_contorl())
                    {
                        cfg_ble_led_control(old_ble_led);
                        cfg_wkup_output_control(false);
                    }
#ifdef TEST_SIGFOX_CURRENT_CONSUMPTION
                    while(cGps_bus_busy_check()){nrf_delay_ms(100);};
                    work_mode = GPS_START;
                    if(cfg_is_3colorled_contorl())
                    {
                        cfg_ble_led_control(old_ble_led);
                        cfg_wkup_output_control(false);
                    }
                    main_set_module_state(WIFI);
#else
                    main_set_module_state(SIGFOX);
#endif
                }
                else
                {
                    while(cGps_bus_busy_check()){nrf_delay_ms(100);};
                    work_mode = GPS_START;
                    if(cfg_is_3colorled_contorl())
                    {
                        cfg_ble_led_control(old_ble_led);
                        cfg_wkup_output_control(false);
                    }
                    main_set_module_state(WIFI);

                }
                nus_send_data('G');
            }
#else
            cPrintLog(CDBG_FCTRL_INFO, "%s %d CDEV_GPS_MODULE Not Defined!\n", __func__, __LINE__);
            if(cfg_is_3colorled_contorl())
            {
                cfg_ble_led_control(old_ble_led);
                cfg_wkup_output_control(false);
            }
            main_set_module_state(WIFI);
#endif
            break;
        case WIFI:
            {
                bool send_data_req = false;
                uint8_t send_data[SIGFOX_SEND_PAYLOAD_SIZE] = {0,};
#ifdef CDEV_WIFI_MODULE
                int get_bssid_result;
                uint8_t *bssidBuf;
                int wifi_result;

                ++m_module_ready_wait_timeout_tick;
                if(m_init_excute)
                {
                    m_module_peripheral_data.sigfixdata_wifi_flag = 0;
#ifdef CDEV_NUS_MODULE
                    memset(m_nus_service_parameter.wifi_data, 0, sizeof(m_nus_service_parameter.wifi_data));
                    memset(m_nus_service_parameter.wifi_rssi, 0, sizeof(m_nus_service_parameter.wifi_rssi));
#endif
                    wifi_result = cWifi_ap_scan_req();
                    if(wifi_result == CWIFI_Result_OK)
                    {
                        cTBC_write_state_noti("WifiScan");
                        cPrintLog(CDBG_FCTRL_INFO, "%s %d WIFI MODULE started!\n", __func__, __LINE__);
                        if(cfg_is_3colorled_contorl())
                        {
                            old_ble_led = cfg_ble_led_control(true);
                            cfg_wkup_output_control(true);
                        }
                        m_init_excute = false;
                    }
                    else
                    {
                        cPrintLog(CDBG_FCTRL_INFO, "%s %d Not Availalble Wifi Module! send NUll data!\n", __func__, __LINE__);
                        send_data_req = true;
                    }
                }
                else
                {
                    if(!cWifi_is_scan_state() && !cWifi_bus_busy_check())  //wait scan
                    {
                        if(cfg_is_3colorled_contorl())
                        {
                            cfg_ble_led_control(old_ble_led);
                            cfg_wkup_output_control(false);
                        }
                        get_bssid_result = cWifi_get_BSSIDs_bufPtr(&bssidBuf);
                        cPrintLog(CDBG_FCTRL_INFO, "%s %d WIFI MODULE end! result:%d BSSID:", __func__, __LINE__, get_bssid_result);
                        cDataDumpPrintOut(CDBG_FCTRL_INFO, bssidBuf, (CWIFI_BSSID_CNT*CWIFI_BSSID_SIZE));
                        if(get_bssid_result == CWIFI_Result_OK)
                        {
                            m_module_peripheral_data.sigfixdata_wifi_flag = 1;
                            memcpy(send_data, bssidBuf, sizeof(send_data));
#ifdef CDEV_NUS_MODULE
                            {
                                int32_t *rssi;
                                cWifi_get_scan_result(NULL, NULL, &rssi, NULL);
                                memcpy(m_nus_service_parameter.wifi_data, bssidBuf, sizeof(m_nus_service_parameter.wifi_data));
                                m_nus_service_parameter.wifi_rssi[0] = (int8_t)rssi[0];
                                m_nus_service_parameter.wifi_rssi[1] = (int8_t)rssi[1];
//                                cPrintLog(CDBG_FCTRL_INFO, "==============WIFI RSSI %d %d", m_nus_service_parameter.wifi_rssi[0], m_nus_service_parameter.wifi_rssi[1]);
                                m_nus_service_parameter.module = 'W';
                                nus_send_data('W');
                            }
#endif
                        }
                        else if(get_bssid_result == CWIFI_Result_NoData)
                        {
                            cPrintLog(CDBG_FCTRL_INFO, "%s %d WIFI MODULE NoData! send NUll data!\n", __func__, __LINE__);
                        }
                        else
                        {
                            cPrintLog(CDBG_FCTRL_INFO, "%s %d Not Availalble Wifi Module! send NUll data!\n", __func__, __LINE__);
                        }
                        send_data_req = true;
                    }
                }                    
#else
                m_module_peripheral_data.sigfixdata_wifi_flag = 0;
                cPrintLog(CDBG_FCTRL_INFO, "%s %d CDEV_WIFI_MODULE Not Defined! send NUll data!\n", __func__, __LINE__);
                send_data_req = true;
#endif
                if(send_data_req)
                {
                    memcpy(m_module_peripheral_data.sigfixdata_wifi, send_data, sizeof(m_module_peripheral_data.sigfixdata_wifi));
                    cfg_bin_2_hexadecimal(send_data, SIGFOX_SEND_PAYLOAD_SIZE, (char *)frame_data);  //set sigfox payload
                    cPrintLog(CDBG_FCTRL_INFO, "%s %d send request sigfox! data:%s\n", __func__, __LINE__, frame_data);
                    main_set_module_state(SIGFOX);
                }
            }
            break;
        case WIFI_BYPASS:
#if defined(FEATURE_CFG_BYPASS_CONTROL)
#if defined(CDEV_WIFI_MODULE)
            if(m_init_excute)
            {
                if(cWifi_is_detected())
                {
                    if(cWifi_is_busy())
                    {
                        if(m_module_ready_wait_timeout_tick < 20)
                        {
                            if((m_module_ready_wait_timeout_tick == 1) || ((m_module_ready_wait_timeout_tick % 5) == 0))
                            {
                                cPrintLog(CDBG_FCTRL_ERR, "%s %d Wait Wifi Module%d\n", __func__, __LINE__, m_module_ready_wait_timeout_tick);
                            }
                        }
                        else
                        {
                            m_init_excute = false;
                        }
                    }
                    else
                    {
                        if(cWifi_bypass_req(cTBC_put_bypass_data, cTBC_mode_change_msg_noti) == CWIFI_Result_OK)
                        {
                            cPrintLog(CDBG_FCTRL_INFO, "%s %d WIFI_BYPASS requested!\n", __func__, __LINE__);
                            m_init_excute = false;
                        }
                        else
                        {
                            cTBC_mode_change_msg_noti(false, "Not Support!");
                            cPrintLog(CDBG_FCTRL_INFO, "%s %d Not Availalble Wifi Module!\n", __func__, __LINE__);
                            m_init_excute = false;
                        }
                    }
                }
                else
                {
                    cPrintLog(CDBG_FCTRL_ERR, "%s %d Wifi is not detected!\n", __func__, __LINE__);
                    cTBC_mode_change_msg_noti(false, "Not Available!");
                    main_set_module_state(WAIT_CTRL_MODE_CHANGE);
                }
            }
            else
            {
                if(!cWifi_is_bypass_state() && !cWifi_bus_busy_check())
                {
                    main_set_module_state(MANUAL_MODE);
                }
            }
#else
            cTBC_mode_change_msg_noti(false, "Not Support!");
            cPrintLog(CDBG_FCTRL_ERR, "%s %d WIFI_BYPASS mode not supported!\n", __func__, __LINE__);
            main_set_module_state(WAIT_CTRL_MODE_CHANGE);
#endif
#else  //FEATURE_CFG_BYPASS_CONTROL
            main_set_module_state(WAIT_CTRL_MODE_CHANGE);
#endif //FEATURE_CFG_BYPASS_CONTROL
            break;
        case WIFI_TEST_MODE:
            if(m_init_excute)
            {
#ifdef CDEV_WIFI_MODULE
                cWifi_enter_rftest_mode();
#else
                cPrintLog(CDBG_FCTRL_ERR, "Wifi Not Defined!\n");
#endif
                cTBC_boot_msg_noti("SettingMode");
                m_init_excute = false;
            }
            else
            {
                module_parameter_check_update();
            }
            break;
        case SIGFOX:
            if(sigfox_get_state() == SETUP_S)
            {
                if(m_init_excute)
                {
                    cTBC_write_state_noti("Sigfox");
                    cPrintLog(CDBG_FCTRL_INFO, "%s %d SIGFOX MODULE started\n", __func__, __LINE__);
                    cfg_sigfox_timers_start();

                    m_init_excute = false;
                }
            }
            else if(sigfox_check_exit_excute())
            {
                main_set_module_state(BLE);
                cfg_sigfox_timers_stop();
                sigfox_set_state(SETUP_S);
                nus_send_data('T');
                nus_send_data('B');
            }
            break;
        case SIGFOX_BYPASS:  //FEATURE_CFG_BYPASS_CONTROL
#ifdef FEATURE_CFG_BYPASS_CONTROL
            if(m_init_excute)
            {
                if(sigfox_bypass_req(cTBC_put_bypass_data, cTBC_mode_change_msg_noti) == NRF_SUCCESS)
                {
                    cPrintLog(CDBG_FCTRL_INFO, "%s %d start SIGFOX_BYPASS\n", __func__, __LINE__);
                    cTBC_write_state_noti("SigfoxBypass");
                }
                else
                {
                    cTBC_mode_change_msg_noti(false, "sigfox busy");
                    cPrintLog(CDBG_FCTRL_ERR, "%s %d start SIGFOX_BUSY\n", __func__, __LINE__);
                }
                m_init_excute = false;
            }
            else
            {
                if(sigfox_check_exit_excute())
                {
                    cPrintLog(CDBG_FCTRL_INFO, "%s %d stop SIGFOX_BYPASS\n", __func__, __LINE__);
                    cfg_sigfox_timers_stop();
                    sigfox_set_state(SETUP_S);
                    main_set_module_state(MANUAL_MODE);
                }
            }
#else
            main_set_module_state(WAIT_CTRL_MODE_CHANGE);
#endif
            break;

        case MANUAL_MODE:
            if(m_exception_mode != NONE)
            {
                main_set_module_state(IDLE);
                break;
            }
            m_module_ready_wait_timeout_tick++;
            if(m_init_excute)
            {
                cTBC_boot_msg_noti("ManualMode");
                m_init_excute = false;
            }
            else
            {
                module_parameter_check_update();
                if((m_module_ready_wait_timeout_tick == 1) || ((m_module_ready_wait_timeout_tick % (10 * 6)) == 0))
                {
                    cPrintLog(CDBG_FCTRL_DBG, "%s %d Wait control cmd%d\n", __func__, __LINE__, m_module_ready_wait_timeout_tick);
                }
            }
            break;

        case SCENARIO_MODE:
            cTBC_boot_msg_noti("ScenarioMode");
            main_set_module_state(MAIN_SCENARIO_LOOP);
            break;

        case GPS_BYPASS:
#if defined(FEATURE_CFG_BYPASS_CONTROL)
#if defined(CDEV_GPS_MODULE)
            if(m_init_excute)  // �����н� ����
            {
                if(cGps_bypass_available_check() == CGPS_Result_OK)  // Gps ���� üũ
                {
                    if(cGps_bypass_request() == CGPS_Result_OK)   // bypass ���� �������� üũ �� bypass�� ���� ����
                    {
                        cPrintLog(CDBG_FCTRL_INFO, "%s %d GPS_BYPASS requested!\n", __func__, __LINE__);
                        m_init_excute = false;
                    }
                    else   // �����н��� ������ �Ұ��� �����̸� not availableó��...
                    {
                        cTBC_mode_change_msg_noti(false, "Not Support!");
                        cPrintLog(CDBG_FCTRL_INFO, "%s %d Not Available Gps Module!\n", __func__, __LINE__);
                        m_init_excute = false;
                    }
                }
            }
            else
            {
                if(cGps_is_bypass_mode())
                {
                    // gps bypass
                }
                else if((cGps_bypass_get_mode_change() == CGPS_Result_OK)  &&  !cGps_bus_busy_check())
                {
                    cPrintLog(CDBG_FCTRL_INFO, "%s %d stop GPS_BYPASS\n", __func__, __LINE__);
                    main_set_module_state(MANUAL_MODE);
                }
                else
                {
                    cTBC_mode_change_msg_noti(false, "Not Support!");
                    cPrintLog(CDBG_FCTRL_ERR, "%s %d GPS_BYPASS mode not supported!\n", __func__, __LINE__);
                    main_set_module_state(WAIT_CTRL_MODE_CHANGE);
                }

            }

#else
            cTBC_mode_change_msg_noti(false, "Not Support!");
            cPrintLog(CDBG_FCTRL_ERR, "%s %d GPS_BYPASS mode not supported!\n", __func__, __LINE__);
            main_set_module_state(WAIT_CTRL_MODE_CHANGE);
#endif
#else
            main_set_module_state(WAIT_CTRL_MODE_CHANGE);
#endif
            break;
        case BLE_CONTROL:
        case ACC_BYPASS:
#ifdef FEATURE_CFG_BYPASS_CONTROL
            if(m_init_excute) 
            {
                if(bma250_get_state() == NONE_ACC)
                {
                    cPrintLog(CDBG_FCTRL_INFO, "%s %d ACC BYPASS started\n",  __func__, __LINE__);
                    cfg_i2c_master_uninit();
                    cfg_i2c_master_init();
                    nrf_delay_ms(1);
                    bma250_set_state(BYPASS_S);
                    cfg_bma250_timers_start();
                    m_init_excute = false;
                }
                else   // If it is not possible to enter into bypass ...
                {
                    cTBC_mode_change_msg_noti(false, "Not Support!");
                    cPrintLog(CDBG_FCTRL_INFO, "%s %d Not Available ACC BYPASS!\n", __func__, __LINE__);
                    m_init_excute = false;
                }
            }
            else
            {
                if(acc_is_bypass_mode())
                {
                    // acc bypass
                }
                else if(bma250_get_state() == EXIT_ACC)
                {
                    cPrintLog(CDBG_FCTRL_INFO, "%s %d stop GPS_BYPASS\n", __func__, __LINE__);
                    cfg_bma250_timers_stop();
                    bma250_set_state(NONE_ACC);
                    main_set_module_state(MANUAL_MODE);
                }
                else
                {
                    cTBC_mode_change_msg_noti(false, "Not Support!");
                    cPrintLog(CDBG_FCTRL_ERR, "%s %d ACC_BYPASS mode not supported!\n", __func__, __LINE__);
                    main_set_module_state(WAIT_CTRL_MODE_CHANGE);
                }
            }

#else
            main_set_module_state(WAIT_CTRL_MODE_CHANGE);
#endif
            break;

        case IDLE:
            cPrintLog(CDBG_FCTRL_INFO, "IDLE MODULE started\r\n");
//            advertising_start(false, true);

            main_timer_schedule_stop();
            main_timer_schedule_start();
//            m_module_mode = ACC;
            if(m_exception_mode != NONE)
            {
                cPrintLog(CDBG_FCTRL_INFO, "%s %d m_exception_mode detected : %d started\n", __func__, __LINE__, m_exception_mode);
                main_set_module_state(m_exception_mode);
                m_exception_mode = NONE;
            }
            else
            {
                main_set_module_state(MAIN_SCENARIO_LOOP);
                if(cfg_is_3colorled_contorl())
                {
                    cfg_ble_led_control(true);
                }
            }
            break;

        default:
            break;
    }
}

/**
 * @brief Callback function for handling main state  from main schedule timer.
 * @details manage main scheduling in MWC demo
 */

static void main_schedule_timeout_handler_MWC_demo(void * p_context)
{
    static bool old_ble_led = false;
    uint8_t *p_down_link_data;
    uint32_t down_link_data_size;

    main_schedule_tick++;
    switch(m_module_mode)
    {
        case WAIT_CTRL_MODE_CHANGE:
            ++m_module_ready_wait_timeout_tick;
            if(cTBC_check_host_connected())
            {
                if(m_module_ready_wait_timeout_tick > (APP_MAIN_SCHEDULE_HZ * m_module_parameter.start_wait_time_for_ctrl_mode_change_sec))
                {
                    main_set_module_state(SCENARIO_MODE);
                }
                else
                {
                    main_set_module_state(MANUAL_MODE);
                }
            }
            else
            {
                if(m_module_ready_wait_timeout_tick > (APP_MAIN_SCHEDULE_HZ * m_module_parameter.start_wait_time_for_board_control_attach_sec))
                {
                    main_set_module_state(SCENARIO_MODE);
                }
                else
                {
                    if((m_module_ready_wait_timeout_tick == 1) || ((m_module_ready_wait_timeout_tick % 10) == 0))
                        cPrintLog(CDBG_FCTRL_INFO, "%s %d Wait for host conect! %d\n", __func__, __LINE__, m_module_ready_wait_timeout_tick);
                }
            }
            break;

        case MAIN_SCENARIO_LOOP:
            cPrintLog(CDBG_FCTRL_INFO, "==MAIN_SCENARIO_LOOP %u==\n", main_schedule_tick);
            main_set_module_state(WIFI);
            cfg_board_common_power_control(module_comm_pwr_common, true);
            break;

        case BLE:
            main_timer_schedule_stop();
            if(m_module_parameter.scenario_mode == MODULE_SCENARIO_MWC_DEMO)
            {
                advertising_start(false, true);
                sigfox_get_ap_key(m_nus_service_parameter.ap_key);
                gap_params_init();
                advertising_init();
                advertising_start(true, true);
            }
            main_timer_idle_start();
            main_set_module_state(IDLE);
            cfg_board_common_power_control(module_comm_pwr_common, false);
            break;

        case WIFI:
            {
                bool send_data_req = false;
                uint8_t send_data[SIGFOX_SEND_PAYLOAD_SIZE];
                memset(send_data, 0, sizeof(send_data));
                send_data[0] = 'C'; //0x43
                send_data[1] = 0x01; //wifi password request
#ifdef CDEV_WIFI_MODULE
                int get_bssid_result;
                uint8_t *bssidBuf;
                int wifi_result;

                ++m_module_ready_wait_timeout_tick;
                if(m_init_excute)
                {
                    m_module_peripheral_data.sigfixdata_wifi_flag = 0;
                    wifi_result = cWifi_ap_get_available_first_BSSID("NECTAR");
                    if(wifi_result == CWIFI_Result_OK)
                    {
                        cTBC_write_state_noti("WifiScan");
                        cPrintLog(CDBG_FCTRL_INFO, "%s %d WIFI MODULE started!\n", __func__, __LINE__);
                        if(cfg_is_3colorled_contorl())
                        {
                            old_ble_led = cfg_ble_led_control(true);
                            cfg_wkup_output_control(true);
                        }
                        m_init_excute = false;
                    }
                    else
                    {
                        cPrintLog(CDBG_FCTRL_INFO, "%s %d Not Availalble Wifi Module! send NUll data!\n", __func__, __LINE__);
                        send_data_req = true;
                    }
                }
                else
                {
                    if(!cWifi_is_scan_state() && !cWifi_bus_busy_check())  //wait scan
                    {
                        if(cfg_is_3colorled_contorl())
                        {
                            cfg_ble_led_control(old_ble_led);
                            cfg_wkup_output_control(false);
                        }
                        get_bssid_result = cWifi_get_BSSIDs_bufPtr(&bssidBuf);
                        cPrintLog(CDBG_FCTRL_INFO, "%s %d WIFI MODULE end! result:%d BSSID:", __func__, __LINE__, get_bssid_result);
                        cDataDumpPrintOut(CDBG_FCTRL_INFO, bssidBuf, (CWIFI_BSSID_CNT*CWIFI_BSSID_SIZE));
                        if(get_bssid_result == CWIFI_Result_OK)
                        {
                            m_module_peripheral_data.sigfixdata_wifi_flag = 1;
                            memcpy(&send_data[2], bssidBuf, 6);
#ifdef CDEV_NUS_MODULE
                            memset(m_nus_service_parameter.wifi_data,0,12);
                            memcpy(m_nus_service_parameter.wifi_data, bssidBuf, CWIFI_BSSID_SIZE);
#endif
                        }
                        else if(get_bssid_result == CWIFI_Result_Busy)
                        {
                            cPrintLog(CDBG_FCTRL_INFO, "%s %d WIFI MODULE Busy! send NUll data!\n", __func__, __LINE__);
                        }
                        else if(get_bssid_result == CWIFI_Result_NoData)
                        {
                            cPrintLog(CDBG_FCTRL_INFO, "%s %d WIFI MODULE NoData! send NUll data!\n", __func__, __LINE__);
                        }
                        else
                        {
                            cPrintLog(CDBG_FCTRL_INFO, "%s %d Not Availalble Wifi Module! send NUll data!\n", __func__, __LINE__);
                        }
                        send_data_req = true;
                    }
                }                    
#else
                m_module_peripheral_data.sigfixdata_wifi_flag = 0;
                cPrintLog(CDBG_FCTRL_INFO, "%s %d CDEV_WIFI_MODULE Not Defined! send NUll data!\n", __func__, __LINE__);
                send_data_req = true;
#endif
                if(send_data_req)
                {
                    memcpy(m_module_peripheral_data.sigfixdata_wifi, send_data, sizeof(m_module_peripheral_data.sigfixdata_wifi));
                    cfg_bin_2_hexadecimal(send_data, SIGFOX_SEND_PAYLOAD_SIZE, (char *)frame_data);  //set sigfox payload
                    cPrintLog(CDBG_FCTRL_INFO, "%s %d send request sigfox! data:%s\n", __func__, __LINE__, frame_data);
                    main_set_module_state(SIGFOX);
                }
            }
            break;

        case MANUAL_MODE:  //setting only
            if(m_init_excute)
            {
                cTBC_boot_msg_noti("ManualMode");
#ifdef CDEV_GPS_MODULE
                cGps_power_control(false, true);
#endif
                m_init_excute = false;
            }
            else
            {
                module_parameter_check_update();
                if(m_exception_mode != NONE)
                {
                    if(m_exception_mode == SCENARIO_MODE)
                    {
                        main_set_module_state(MAIN_SCENARIO_LOOP);
                    }
                    else
                    {
                        cTBC_mode_change_msg_noti(false, "NotSupported");
                    }
                    m_exception_mode = NONE;
                }
            }
            break;

        case SIGFOX:
            if(sigfox_get_state() == SETUP_S)
            {
                if(m_init_excute)
                {
                    cTBC_write_state_noti("Sigfox");
                    cPrintLog(CDBG_FCTRL_INFO, "%s %d SIGFOX MODULE started\n", __func__, __LINE__);
                    if(cfg_is_3colorled_contorl())
                    {
                        old_ble_led = cfg_ble_led_control(false);
                    }
                    memset(m_nus_service_parameter.ap_key, 0, sizeof(m_nus_service_parameter.ap_key));
                    cfg_sigfox_timers_start();
                    m_init_excute = false;
                }
            }
            else if(sigfox_check_exit_excute())
            {
                if(cfg_is_3colorled_contorl())
                {
                    cfg_ble_led_control(old_ble_led);
                }
                main_set_module_state(BLE);
                cfg_sigfox_timers_stop();
                sigfox_set_state(SETUP_S);
                p_down_link_data = cfg_sigfox_get_downlink_ptr(&down_link_data_size);
                if(down_link_data_size > 0)
                {
                    cPrintLog(CDBG_FCTRL_INFO, "SIGFOX downlink data:%s, size:%d\n", p_down_link_data, down_link_data_size);
                    (void)p_down_link_data;
                    //todo ble advertising payload p_down_link_data
                    //advertising_start(false);
                    //advertising_start(true);
                }
            }
            break;

        case SCENARIO_MODE:
            if(m_init_excute)
            {
#ifdef CDEV_GPS_MODULE
                cGps_power_control(false, true);
#endif
                cTBC_boot_msg_noti("ScenarioMode");
                cPrintLog(CDBG_FCTRL_DBG, "Prepare NECTAR Service\n");
                m_init_excute = false;
            }
            else
            {
                if(m_module_ready_wait_timeout_tick++ > 10)
                {
                    cPrintLog(CDBG_FCTRL_DBG, "Start NECTAR Service\n");
                    main_set_module_state(MAIN_SCENARIO_LOOP);
                }
            }
            break;

        case IDLE:
            main_timer_schedule_stop();
            main_timer_schedule_start();
            main_set_module_state(MAIN_SCENARIO_LOOP);
            break;
        default:
            break;
    }
}

bool main_work_mode_change_request(cfg_board_work_mode_e mode)
{
    bool ret = false;
    if(m_exception_mode != NONE)
    {
        cPrintLog(CDBG_FCTRL_ERR, "%s mode_change busy! %d, %d\n", __func__, m_exception_mode, m_module_mode);
        return ret;
    }
    switch(mode)
    {
        case cfg_board_work_normal:
            cPrintLog(CDBG_FCTRL_INFO, "%s normal mode request!\n", __func__);
            m_exception_mode = SCENARIO_MODE;
            ret = true;
            advertising_start(true, true);
            break;

        case cfg_board_work_sigfox_bypass:
            cPrintLog(CDBG_FCTRL_INFO, "%s sigfox_bypass mode request!\n", __func__);
            m_exception_mode = SIGFOX_BYPASS;
            ret = true;
            advertising_start(false, true);
#ifdef CDEV_GPS_MODULE
            cGps_power_control(false, true);
#endif
            break;

        case cfg_board_work_wifi_bypass:
            cPrintLog(CDBG_FCTRL_INFO, "%s wifi_bypass mode request!\n", __func__);
            m_exception_mode = WIFI_BYPASS;
            ret = true;
            advertising_start(false, true);
#ifdef CDEV_GPS_MODULE
            cGps_power_control(false, true);
#endif
            break;

        case cfg_board_work_manual:
            cPrintLog(CDBG_FCTRL_INFO, "%s cfg_board_work_manual mode request!\n", __func__);
            m_exception_mode = MANUAL_MODE;
            ret = true;
            advertising_start(false, true);
            break;

        case cfg_board_work_gps_bypass:
            cPrintLog(CDBG_FCTRL_INFO, "%s cfg_board_work_gps_bypass mode request!\n", __func__);
            m_exception_mode = GPS_BYPASS;
            ret = true;
            advertising_start(false, true);
            break;

        case cfg_board_work_ble_crtl:
        case cfg_board_work_acc_bypass:
            cPrintLog(CDBG_FCTRL_INFO, "%s cfg_board_work_ble_crtl mode request!\n", __func__);
//            m_exception_mode = BLE_CONTROL;
            m_exception_mode = ACC_BYPASS;
            ret = true;
            advertising_start(false, true);
            break;

        default:
            break;
    }

    switch(m_module_mode)  //abort request
    {
        case WIFI_BYPASS:
#ifdef CDEV_WIFI_MODULE
            cWifi_abort_req();
#endif
            break;

        case SIGFOX_BYPASS:
            sigfox_abort_request();
            break;

        case GPS_BYPASS:
#ifdef CDEV_GPS_MODULE
            cGps_bypass_abort_request();
#endif
            break;
        case BLE_CONTROL:
        case ACC_BYPASS:
            nrf_delay_ms(10);
            bma250_set_state(EXIT_ACC);
            break;
        case IDLE:
            main_timer_schedule_stop();
            main_timer_schedule_start();
            break;

        default:
            break;
    }
    return ret;
}


cfg_board_work_mode_e main_work_mode_get_cur(void)
{
    cfg_board_work_mode_e cur_mode = cfg_board_work_normal;

    if(m_module_mode == SIGFOX_BYPASS)
    {
        cur_mode = cfg_board_work_sigfox_bypass;
    }
    else if(m_module_mode == WIFI_BYPASS)
    {
        cur_mode = cfg_board_work_wifi_bypass;
    }
    else if(m_module_mode == GPS_BYPASS)
    {
        cur_mode = cfg_board_work_gps_bypass;
    }
    else if(m_module_mode == ACC_BYPASS)
    {
        cur_mode = cfg_board_work_acc_bypass;
    }

    return cur_mode;
}

/**
 * @brief function for creating main schedule timer
 */

static void main_timer_create()
{
    app_timer_timeout_handler_t timeout_handler;
    volatile uint32_t      err_code;

    if(m_module_parameter.scenario_mode == MODULE_SCENARIO_ASSET_TRACKER)
    {
        cPrintLog(CDBG_FCTRL_INFO, "start mode : MODULE_SCENARIO_ASSET_TRACKER\n");
        timeout_handler = main_schedule_timeout_handler_asset_tracker;
    }
    else if(m_module_parameter.scenario_mode == MODULE_SCENARIO_MWC_DEMO)
    {
        cPrintLog(CDBG_FCTRL_INFO, "start mode : MODULE_SCENARIO_MWC_DEMO\n");
        timeout_handler = main_schedule_timeout_handler_MWC_demo;
    }
    else if(m_module_parameter.scenario_mode == MODULE_SCENARIO_IHERE_MINI)
    {
        cPrintLog(CDBG_FCTRL_INFO, "start mode : MODULE_SCENARIO_IHERE_MINI\n");
        timeout_handler = main_schedule_timeout_handler_ihere_mini;
    }
    else
    {
        cPrintLog(CDBG_FCTRL_ERR, "%s invalid scenario %d \n", __func__, m_module_parameter.scenario_mode);
        cPrintLog(CDBG_FCTRL_ERR, "goto ASSET TRACKER! \n");
        timeout_handler = main_schedule_timeout_handler_asset_tracker;
    }
    err_code = app_timer_create(&m_main_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                timeout_handler);
    APP_ERROR_CHECK(err_code);
}


/**
 * @brief function for prepare power down
 */
static void main_prepare_power_down(void)
{
    cPrintLog(CDBG_MAIN_LOG, "prepare power down\n");
    if(!m_cfg_i2c_master_init_flag)
        cfg_i2c_master_init();
    nrf_delay_ms(1);
    bma250_req_suppend_mode();
    nrf_delay_ms(1);
    tmp102_req_shutdown_mode();
    nrf_delay_ms(1);
    if(m_cfg_i2c_master_init_flag)
        cfg_i2c_master_uninit();
    nrf_delay_ms(1);
    
    cfg_board_gpio_set_default();
    nfc_uninit();
    nrf_delay_ms(1);
    if(m_module_parameter.boot_nfc_unlock)
    {
        if(m_module_parameter.wkup_gpio_enable==1)
        {
            nrf_gpio_cfg_sense_input(PIN_DEF_WKUP, NRF_GPIO_PIN_PULLDOWN, NRF_GPIO_PIN_SENSE_HIGH);
            nrf_delay_ms(1);
        }
        if(m_module_parameter.magnetic_gpio_enable)
        {
            nrf_gpio_cfg_sense_input(PIN_DEF_MAGNETIC_SIGNAL, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_SENSE_LOW);
            nrf_delay_ms(1);
        }
        NRF_NFCT->TASKS_SENSE = 1;
        NRF_NFCT->INTENCLR = 
            (NFCT_INTENCLR_RXFRAMEEND_Clear << NFCT_INTENCLR_RXFRAMEEND_Pos) |
            (NFCT_INTENCLR_RXERROR_Clear    << NFCT_INTENCLR_RXERROR_Pos);
    }
    nrf_delay_ms(1);
}

/**
 * @brief function for enter and wakeup deepsleep
 */
static void main_deepsleep_control(void)
{
    if(m_module_parameter.boot_nfc_unlock) 
    {
        bool magnetdet = false, wkupdet = false;
        cPrintLog(CDBG_MAIN_LOG, "wait boot unlock\n");
        if(m_module_parameter.magnetic_gpio_enable)nrf_gpio_cfg_input(PIN_DEF_MAGNETIC_SIGNAL, NRF_GPIO_PIN_NOPULL);
        if(m_module_parameter.wkup_gpio_enable==1)nrf_gpio_cfg_input(PIN_DEF_WKUP, NRF_GPIO_PIN_PULLDOWN);

        for (;; )
        {
            if(m_module_parameter.magnetic_gpio_enable){magnetdet = (nrf_gpio_pin_read(PIN_DEF_MAGNETIC_SIGNAL) == 0);}  //boot wake up level detect low for magnetic
            if(m_module_parameter.wkup_gpio_enable==1){wkupdet = (nrf_gpio_pin_read(PIN_DEF_WKUP) == 1);}  //boot wake up level detect high for wkup key
            if (mnfc_tag_on  //wake up condition
                || m_cfg_sw_reset_detected  //wake up condition
                || magnetdet  //wake up condition
                || wkupdet  //wake up condition
                || m_cfg_GPIO_wake_up_detected  //wake up condition
                || m_cfg_debug_interface_wake_up_detected
                || cTBC_check_host_connected()  //exception condition
                || m_module_parameter.wifi_testmode_enable  //exception condition
             )
            {
                if(m_module_parameter.magnetic_gpio_enable)nrf_gpio_cfg_default(PIN_DEF_MAGNETIC_SIGNAL);
                if(m_module_parameter.wkup_gpio_enable==1)nrf_gpio_cfg_default(PIN_DEF_WKUP);
                mnfc_tag_on = false;
                cPrintLog(CDBG_FCTRL_INFO, "wakeup reason swreset:%d, gpio:%d, nfc:%d %d, jtag:%d, magnet:%d, wkup:%d\n", 
                              m_cfg_sw_reset_detected, m_cfg_GPIO_wake_up_detected, m_cfg_NFC_wake_up_detected, mnfc_tag_on, m_cfg_debug_interface_wake_up_detected, magnetdet, wkupdet);
                break;
            }
            if(cTBC_is_busy())
            {
                power_manage();  //The wakeup event is generated by the cfg_twis_board_control
            }
            else
            {
                cPrintLog(CDBG_MAIN_LOG, "enter deep sleep mode\n");
                main_prepare_power_down();
                // Enter System OFF mode.
                sd_power_system_off();
                while(1);
            }
        }
    }
}

/**
 * @brief Callback function for handling iterrupt from accelerometer.
 */

void bma250_int_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    char display[20];
    int len;
    sprintf(display,"g-sensor shaking\n");
    len = strlen(display);
    if(!mnus_acc_report)
        main_wakeup_interrupt = true;
    cPrintLog(CDBG_FCTRL_INFO,"%s", display);
    cTBC_put_bypass_data((uint8_t *)display,len);
#ifdef CDEV_NUS_MODULE
    m_nus_service_parameter.accellometer_event = '1';
#endif
}

/**
 * @brief  function for setting iterrupt from accelerometer.
 */

void cfg_bma250_interrupt_init(void)
{
    uint32_t err_code;
    nrf_drv_gpiote_in_config_t in_config = GPIOTE_CONFIG_IN_SENSE_LOTOHI(false);

    if (!nrf_drv_gpiote_is_init())
    {
        err_code = nrf_drv_gpiote_init();
        APP_ERROR_CHECK(err_code);
    }

    err_code = nrf_drv_gpiote_in_init(PIN_DEF_ACC_INT1, &in_config, bma250_int_handler);
    APP_ERROR_CHECK(err_code);

    nrf_drv_gpiote_in_event_enable(PIN_DEF_ACC_INT1, true);
}

/**
 * @brief Callback function for handling NFC events.
 */

static void nfc_callback(void * p_context, nfc_t2t_event_t event, const uint8_t * p_data, size_t data_length)
{
    (void)p_context;

    switch (event)
    {
        case NFC_T2T_EVENT_FIELD_ON:
            mnfc_tag_on = true;
//            LEDS_ON(BSP_LED_0_MASK);
            break;

        case NFC_T2T_EVENT_FIELD_OFF:
 //           LEDS_OFF(BSP_LED_0_MASK);
            break;

        default:
            break;
    }
}

static void sigfox_id_record_add(nfc_ndef_msg_desc_t * p_ndef_msg_desc)
{
    /** @snippet [NFC text usage_1] */
    uint32_t             err_code;
    static  uint8_t en_payload[] =
                  {'I', 'D', ':', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',' '};
    static const uint8_t en_code[] = {'e', 'n'};

//  memcpy((char*)&en_payload[3],m_module_peripheral_ID.sigfox_device_ID,4);
    cfg_bin_2_hexadecimal(m_module_peripheral_ID.sigfox_device_ID,4,(char*)&en_payload[3]);
    NFC_NDEF_TEXT_RECORD_DESC_DEF(en_text_rec,
                                  UTF_8,
                                  en_code,
                                  sizeof(en_code),
                                  en_payload,
                                  sizeof(en_payload)-1);
   /** @snippet [NFC text usage_1] */

    err_code = nfc_ndef_msg_record_add(p_ndef_msg_desc,
                                       &NFC_NDEF_TEXT_RECORD_DESC(en_text_rec));
    APP_ERROR_CHECK(err_code);

}


/**
 * @brief Function for creating a record in Norwegian.
 */
static void sigfox_pac_record_add(nfc_ndef_msg_desc_t * p_ndef_msg_desc)
{
    uint32_t             err_code;
    static  uint8_t pl_payload[] =
                          {'P', 'A', 'C', ':', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    static const uint8_t pl_code[] = {'P', 'L'};

//  memcpy((char*)&no_payload[4],m_module_peripheral_ID.sigfox_pac_code,8);
    cfg_bin_2_hexadecimal(m_module_peripheral_ID.sigfox_pac_code,8,(char*)(char*)&pl_payload[4]);
    NFC_NDEF_TEXT_RECORD_DESC_DEF(no_text_rec,
                                  UTF_8,
                                  pl_code,
                                  sizeof(pl_code),
                                  pl_payload,
                                  sizeof(pl_payload)-1);

    err_code = nfc_ndef_msg_record_add(p_ndef_msg_desc,
                                       &NFC_NDEF_TEXT_RECORD_DESC(no_text_rec));
    APP_ERROR_CHECK(err_code);
}

/**
 * @brief Function for creating a record in Norwegian.
 */
static void ble_mac_record_add(nfc_ndef_msg_desc_t * p_ndef_msg_desc)
{
    uint32_t             err_code;
    static  uint8_t no_payload[17];
    uint8_t hex_digit[13];
    int i = 0;
    int j =0;
    static const uint8_t no_code[] = {'N', 'O'};

//  memcpy((char*)no_payload,m_module_peripheral_ID.ble_MAC,6);
    cfg_bin_2_hexadecimal(m_module_peripheral_ID.ble_MAC,6,(char*)(char*)hex_digit);
    for(i=0;i<12;)
    {
        no_payload[j++]=hex_digit[i++];
        no_payload[j++]=hex_digit[i++];
        if(j<17)
            no_payload[j++]=':';
    }
    NFC_NDEF_TEXT_RECORD_DESC_DEF(no_text_rec,
                                  UTF_8,
                                  no_code,
                                  sizeof(no_code),
                                  no_payload,
                                  sizeof(no_payload));

    err_code = nfc_ndef_msg_record_add(p_ndef_msg_desc,
                                       &NFC_NDEF_TEXT_RECORD_DESC(no_text_rec));
    APP_ERROR_CHECK(err_code);
}

uint32_t nus_send_id_pac_mac(uint8_t * send_buffer)
{
    uint32_t err_code;
    send_buffer[0]='S';
    memcpy(send_buffer+1,m_module_peripheral_ID.sigfox_device_ID,4);
    err_code = ble_nus_string_send(&m_nus, (uint8_t*)send_buffer, 5);

    memset(send_buffer,0xFF,20);
    send_buffer[0]='P';
    memcpy(send_buffer+1,m_module_peripheral_ID.sigfox_pac_code,8);
    err_code = ble_nus_string_send(&m_nus, (uint8_t*)send_buffer, 9);

    memset(send_buffer,0xFF,20);
    send_buffer[0]='M';
    memcpy(send_buffer+1,m_module_peripheral_ID.ble_MAC,6);
    err_code = ble_nus_string_send(&m_nus, (uint8_t*)send_buffer,7);

    return err_code;
}

#if 0
static void android_app_record_add(nfc_ndef_msg_desc_t * p_ndef_msg_desc)
{
    uint32_t             err_code;
    static const uint8_t no_code[] = {'N', 'O'};

    /* Create NFC NDEF message description, capacity - 2 records */
//    NFC_NDEF_MSG_DEF(nfc_launchapp_msg, 1);
    NFC_NDEF_TEXT_RECORD_DESC_DEF(no_text_rec,
                                  UTF_8,
                                  no_code,
                                 sizeof(no_code),
                                  m_android_package_name,
                                  sizeof(m_android_package_name));

//    p_android_rec = nfc_android_application_rec_declare(m_android_package_name,sizeof(m_android_package_name));

 
    /* Add Android App Record as second record to message */
    err_code = nfc_ndef_msg_record_add(p_ndef_msg_desc,&NFC_NDEF_TEXT_RECORD_DESC(no_text_rec));


    APP_ERROR_CHECK(err_code);
}
#endif

/**
 * @brief Function for encoding the welcome message.
 */
 
NFC_NDEF_MSG_DEF(welcome_msg, MAX_REC_COUNT);
static void welcome_msg_encode(uint8_t * p_buffer, uint32_t * p_len)
{
    sigfox_id_record_add(&NFC_NDEF_MSG(welcome_msg));
    sigfox_pac_record_add(&NFC_NDEF_MSG(welcome_msg));
    ble_mac_record_add(&NFC_NDEF_MSG(welcome_msg));

    /** @snippet [NFC text usage_2] */
    uint32_t err_code = nfc_ndef_msg_encode(&NFC_NDEF_MSG(welcome_msg),
                                            p_buffer,
                                            p_len);
    APP_ERROR_CHECK(err_code);
    /** @snippet [NFC text usage_2] */
}

void nfc_init()
{
    uint32_t err_code;

    /* Set up NFC */
    err_code = nfc_t2t_setup(nfc_callback, NULL);
    APP_ERROR_CHECK(err_code);

    /** @snippet [NFC URI usage_1] */
    /* Provide information about available buffer size to encoding function */
    uint32_t len = sizeof(m_ndef_msg_buf);
#if 0
    /* Encode URI message into buffer */
    err_code = nfc_uri_msg_encode( NFC_URI_HTTPS,
                                   m_url,
                                   sizeof(m_url),
                                   m_ndef_msg_buf,
                                   &len);
#else
    welcome_msg_encode(m_ndef_msg_buf, &len);
#endif

    /** @snippet [NFC URI usage_1] */

    /* Set created message as the NFC payload */
    err_code = nfc_t2t_payload_set(m_ndef_msg_buf, len);
    APP_ERROR_CHECK(err_code);

    /* Start sensing NFC field */
    err_code = nfc_t2t_emulation_start();
}

void nfc_uninit(void)
{
    nfc_t2t_emulation_stop();
}

void nfc_restart(void)
{
    nfc_t2t_emulation_stop();
    nfc_t2t_done();
    nfc_ndef_msg_clear(&NFC_NDEF_MSG(welcome_msg));
    nfc_init();
}

static void main_schedule_timeout_handler_examples(void * p_context)
{
    main_schedule_tick++;
}

void main_examples_prepare(void)
{
    volatile uint32_t      err_code;
    
    //timer Initialize
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);

    //sd init
    ble_stack_init();

    //ldo mode
    sd_power_dcdc_mode_set(1);

    //parameter init
    module_parameter_init();

    err_code = app_timer_create(&m_main_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                main_schedule_timeout_handler_examples);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_start(m_main_timer_id, APP_TIMER_TICKS(APP_MAIN_SCHEDULE_MS, APP_TIMER_PRESCALER), NULL);
    APP_ERROR_CHECK(err_code);
}

int main(void)
{
    bool    erase_bonds = 0;

    cPrintLog(CDBG_MAIN_LOG, "\n====== %s Module Started Ver:%s bdtype:%d======\n", m_cfg_model_name, m_cfg_sw_ver, m_cfg_board_type);
    cPrintLog(CDBG_MAIN_LOG, "build date:%s, %s\n", m_cfg_build_date, m_cfg_build_time);
    
    cfg_examples_check_enter();
    cfg_board_early_init();
    main_wakeup_interrupt = false;

    //state Initialize
    m_module_mode = ACC;
    m_init_excute = true;
    sigfox_set_state(SETUP_S);
    bma250_set_state(NONE_ACC);
    tmp102_set_state(NONE_TMP);

    // Initialize.
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);

    ble_stack_init();
    sd_power_dcdc_mode_set(1);
    module_parameter_init();

#ifdef CDEV_NUS_MODULE
    nus_data_init();
#endif
    cTBC_init();
    get_ble_mac_address();

    nfc_init();
    main_deepsleep_control();
    cfg_board_gpio_set_default_gps();

    cfg_board_init();
    if(module_parameter_ID_value_update())
    {
        cPrintLog(CDBG_FCTRL_INFO, "nfc reinit\n");
        nfc_restart();
    }

    if(main_get_param_val(module_parameter_item_magnetic_gpio_enable))cfg_magnetic_sensor_init(main_magnet_attach_callback);
    if(main_get_param_val(module_parameter_item_wkup_gpio_enable)==1)cfg_wkup_gpio_init(main_wkup_det_callback);

//    nfc_init();
//    sigfox_power_on(true);
//    nrf_delay_ms(1000);
    cPrintLog(CDBG_FCTRL_INFO, "%s started!\n", __func__);
//    cfg_bma250_spi_init();

    cfg_i2c_master_init();
    cfg_bma250_interrupt_init();

//    err_code = bsp_init(BSP_INIT_LED, APP_TIMER_TICKS(100, APP_TIMER_PRESCALER), NULL);
//    APP_ERROR_CHECK(err_code);
    peer_manager_init(erase_bonds);

    gap_params_init();
    services_init();


    advertising_init();
//    services_init();
    conn_params_init();

    main_timer_create();
//    cfg_sigfox_timer_create();
    cfg_bma250_timer_create();
    cfg_tmp102_timer_create();
#ifdef CDEV_NUS_MODULE
//    nus_timer_create();
#endif

#ifdef CDEV_BATT_CHECK_MODULE
    cfg_bas_timer_create();
#endif
    // Start execution.

    if(0){}  //dummy if
#if defined(CDEV_WIFI_MODULE)  //wifi testmode (wifi alway on only)
    else if((main_get_param_val(module_parameter_item_scenario_mode)== MODULE_SCENARIO_ASSET_TRACKER) 
        && main_get_param_val(module_parameter_item_wifi_testmode_enable))  //wifi_testmode_enable
    {
        main_set_module_state(WIFI_TEST_MODE);
#ifdef CDEV_GPS_MODULE
        cGps_power_control(false, true);
#endif
        cPrintLog(CDBG_FCTRL_INFO, "%s startup wifi test mode\n", __func__);
        // Enter main loop.
        main_timer_schedule_start();
    }
#endif
    else
    {
        cPrintLog(CDBG_FCTRL_INFO, "%s main schedule start! state:%d\n", __func__, m_module_mode);
        if(m_module_parameter.scenario_mode == MODULE_SCENARIO_ASSET_TRACKER)
        {
            cfg_sigfox_downlink_on_off(false);
#if (CDEV_BOARD_TYPE == CDEV_BOARD_IHERE) || (CDEV_BOARD_TYPE == CDEV_BOARD_IHERE_MINI)
            mnfc_tag_on_CB = main_timer_schedule_restart_check_idle;
#endif
        }
        else if(m_module_parameter.scenario_mode == MODULE_SCENARIO_MWC_DEMO)
        {            
            //set start state
            m_module_mode = WAIT_CTRL_MODE_CHANGE;
            cfg_sigfox_downlink_on_off(true);
        }
        else if(m_module_parameter.scenario_mode == MODULE_SCENARIO_IHERE_MINI)
        {
            cfg_sigfox_downlink_on_off(false);
            mnfc_tag_on_CB = ihere_mini_current_schedule_start;
            charging_indicator_init();
        }
        // Enter main loop.
        main_timer_schedule_start();
        advertising_start(true, true);
    }

    cPrintLog(CDBG_FCTRL_INFO, "BLE MAC:");
    cDataDumpPrintOut(CDBG_FCTRL_INFO, m_module_peripheral_ID.ble_MAC, 6);
    cPrintLog(CDBG_FCTRL_INFO, "SFX ID:");
    cDataDumpPrintOut(CDBG_FCTRL_INFO, m_module_peripheral_ID.sigfox_device_ID, 4);
    cPrintLog(CDBG_FCTRL_INFO, "SFX PAC CODE:");
    cDataDumpPrintOut(CDBG_FCTRL_INFO, m_module_peripheral_ID.sigfox_pac_code, 8);   
    cPrintLog(CDBG_FCTRL_INFO, "WIFI MAC:");
    cDataDumpPrintOut(CDBG_FCTRL_INFO, m_module_peripheral_ID.wifi_MAC_STA, 6);    

#ifdef CDEV_WIFI_MODULE
    {
        uint8_t wifi_app_ver;
        uint16_t initDataVer;
        
        cWifi_get_version_info(&wifi_app_ver, &initDataVer);
        cPrintLog(CDBG_FCTRL_INFO, "WIFI AppVer:%02x, InitDataVer:%04x\n", wifi_app_ver, initDataVer);
    }
#endif

    for (;; )
    {
        if(main_wakeup_interrupt == true)
        {
            main_wakeup_interrupt = false;
            main_timer_schedule_restart_check_idle();
        }

        if(mnfc_tag_on)
        {
            mnfc_tag_on= false;
//          advertising_start(false, led_control);
//          advertising_start(true, led_control);            
            if(mnfc_tag_on_CB)mnfc_tag_on_CB();
        }

        if(main_powerdown_request)
        {
            int i;
            main_powerdown_request = false;
            app_timer_stop_all();
            for(i=0;i<20;i++)
            {
                cfg_ble_led_control(((i%2==0)?true:false));
                nrf_delay_ms(100);
            }
            cfg_board_gpio_set_default_gps();
            main_prepare_power_down();
            sd_power_system_off();
            while(1);
        }

        power_manage();
    }
}


/**
 * @}
 */