/*=================================================================

                        EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

WHEN              WHO         WHAT, WHERE, WHY
------------     -------     -------------------------------------------------------------------------------

=================================================================*/

#ifndef LIOT_API_COMMON_H
#define LIOT_API_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/*========================================================================
 *  Macro Definition
 *========================================================================*/

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif
/*
//2022.05.18 add by Tomas.Qin EGICOMM-111  Query partition information
#define	LIOT_FLASH_MEM_HEADER1_ADDR       FLASH_MEM_HEADER1_ADDR
#define	LIOT_FLASH_MEM_HEADER2_ADDR       FLASH_MEM_HEADER2_ADDR
#define	LIOT_BOOTLOADER_FLASH_LOAD_ADDR   BOOTLOADER_FLASH_LOAD_ADDR
#define	LIOT_FLASH_MEM_RESERVED           BOOTLOADER_FLASH_LOAD_ADDR+BOOTLOADER_FLASH_LOAD_SIZE
#define	LIOT_FLASH_FOTA_REGION_START      FLASH_FOTA_REGION_START
#define	LIOT_NVRAM_FACTORY_PHYSICAL_BASE  NVRAM_FACTORY_PHYSICAL_BASE
#define	LIOT_NVRAM_PHYSICAL_BASE          NVRAM_PHYSICAL_BASE
#define	LIOT_AP_FLASH_LOAD_ADDR           AP_FLASH_LOAD_ADDR
#define	LIOT_FLASH_FS_REGION_START        FLASH_FS_REGION_START
#define	LIOT_UPDATER_FLASH_LOAD_ADDR      UPDATER_FLASH_LOAD_ADDR
#define	LIOT_FLASH_MEM_BACKUP_ADDR        FLASH_MEM_BACKUP_ADDR
#define	LIOT_FLASH_MEM_PLAT_INFO_ADDR     FLASH_MEM_PLAT_INFO_ADDR
*/
/*========================================================================
 *	Enumeration Definition
 *========================================================================*/
typedef enum
{
    LIOT_COMPONENT_NONE = 0,

    LIOT_COMPONENT_OSI = 0x8100,

    LIOT_COMPONENT_BSP          = 0x8200,
    LIOT_COMPONENT_BSP_GPIO     = 0x8201,
    LIOT_COMPONENT_BSP_PWM      = 0x8202,
    LIOT_COMPONENT_BSP_SPI      = 0x8203,
    LIOT_COMPONENT_BSP_I2C      = 0x8204,
    LIOT_COMPONENT_BSP_KEY      = 0x8205,
    LIOT_COMPONENT_BSP_UART     = 0x8206,
    LIOT_COMPONENT_BSP_USB      = 0x8207,
    LIOT_COMPONENT_BSP_CMUX     = 0x8208,
    LIOT_COMPONENT_BSP_LCD      = 0x8209,
    LIOT_COMPONENT_BSP_CAMERA   = 0x820A,
    LIOT_COMPONENT_BSP_BT       = 0x820B,
    LIOT_COMPONENT_BSP_GNSS     = 0x820C,
    LIOT_COMPONENT_BSP_WIFISCAN = 0x820D,
    LIOT_COMPONENT_BSP_ADC      = 0x820E,
    LIOT_COMPONENT_BSP_LED      = 0x820F,
    LIOT_COMPONENT_BSP_DECODER  = 0x8210,
    LIOT_COMPONENT_BSP_KEYPAD   = 0x8211,
    LIOT_COMPONENT_BSP_RTC      = 0x8212,
    LIOT_COMPONENT_BSP_LVGL     = 0x8213,
    LIOT_COMPONENT_BSP_CHARGE   = 0x8214,
    LIOT_COMPONENT_BSP_VIRT_AT  = 0x8215,
    LIOT_COMPONENT_BSP_OTHER    = 0x82FF,

    LIOT_COMPONENT_STORAGE                 = 0x8300,
    LIOT_COMPONENT_STORAGE_FLASH           = 0x8301,
    LIOT_COMPONENT_STORAGE_SDEMMC          = 0x8302,
    LIOT_COMPONENT_STORAGE_FILE            = 0x8303,
    LIOT_COMPONENT_STORAGE_EXTFLASH        = 0x8304, // for nor flash
    LIOT_COMPONENT_STORAGE_EXT_NANDFLASH   = 0x8305, // for nand flash
    LIOT_COMPONENT_STORAGE_ZIP             = 0x8306,
    LIOT_COMPONENT_STORAGE_FATFS           = 0x8307, // for fatfs
    LIOT_COMPONENT_STORAGE_EMBED_NOR_FLASH = 0x8308, // for embed flash
    LIOT_COMPONENT_STORAGE_QCFG            = 0x8309, // liot_cfg
    LIOT_COMPONENT_STORAGE_NV              = 0x8310, // nv

    LIOT_COMPONENT_AUDIO          = 0x8400,
    LIOT_COMPONENT_AUDIO_CODEC    = 0x8401,
    LIOT_COMPONENT_AUDIO_REC_PLAY = 0x8402,
    LIOT_COMPONENT_AUDIO_TTS      = 0x8403,
    LIOT_COMPONENT_AUDIO_DTMF     = 0x8404,
    LIOT_COMPONENT_AUDIO_SETTING  = 0x8405,
    LIOT_COMPONENT_AUDIO_HEADSET  = 0x8406,

    LIOT_COMPONENT_LWIP           = 0x8500,
    LIOT_COMPONENT_LWIP_SOCKET    = 0x8501, // tcp/udp/ping/dns
    LIOT_COMPONENT_LWIP_HTTP      = 0x8502,
    LIOT_COMPONENT_LWIP_MQTT      = 0x8503,
    LIOT_COMPONENT_LWIP_FTP       = 0x8504,
    LIOT_COMPONENT_LWIP_SSL       = 0x8505,
    LIOT_COMPONENT_LWIP_MMS       = 0x8506,
    LIOT_COMPONENT_LWIP_LBS       = 0x8507, // locator
    LIOT_COMPONENT_LWIP_LIOTTHING = 0x8508,
    LIOT_COMPONENT_LWIP_QNTP      = 0x8509,
    LIOT_COMPONENT_LWIP_SMTP      = 0x8510,

    LIOT_COMPONENT_NETWORK        = 0x8600,
    LIOT_COMPONENT_NETWORK_MANAGE = 0x8601,
    LIOT_COMPONENT_NETWORK_PPP    = 0x8602,
    LIOT_COMPONENT_NETWORK_USBNET = 0x8603,

    LIOT_COMPONENT_SIM = 0x8700,

    LIOT_COMPONENT_PM       = 0x8800,
    LIOT_COMPONENT_PM_SLEEP = 0x8801,

    LIOT_COMPONENT_SMS = 0x8900,

    LIOT_COMPONENT_VOICE_CALL = 0x8A00, // cs call (csfb)

    LIOT_COMPONENT_IMS = 0x8B00, // volte, ps sms

    LIOT_COMPONENT_FOTA = 0x8C00,

    LIOT_COMPONENT_STATE_INFO = 0x8D00, // basic info and state for magic product itself

    LIOT_COMPONENT_LOG = 0x8E00,

    LIOT_COMPONENT_NETLOG = 0x8F00, 
    //
    // next is reserved for app to add component
    //
    LIOT_COMPONENT_APP_START = 0x9F00, // app component start

    LIOT_COMPONENT_MAX = 0x9FFF, // component end
} liot_component_e;

/*
 *						errcode definition
 *
 * 4 bytes, little endian, signed type, as below:
 *
 *		----------------------------------------------------------------------------------------------------
 *		|			 4				|			  3 		   |		  2 		  | 		1		   |
 *		----------------------------------------------------------------------------------------------------
 *		| High byte of component ID | low byte of component ID | High byte of errcode | low byte of errcode|
 *		----------------------------------------------------------------------------------------------------
 *
 * component ID defined at enum type <liot_errcode_component_e>.
 * detail errcode defined by each component.
 *
 */
// common detail errcode, 2 bytes
typedef enum
{
    LIOT_NO_ERROR = 0,
    LIOT_SUCCESS  = 0,
    LIOT_INVALID_PARAM_ERR,
    LIOT_READ_NVM_ERR,
    LIOT_STORAGE_NVM_ERR,

    LIOT_GENERAL_ERROR = 0xFFFF,
} liot_errcode_e;

typedef enum
{
    LIOT_APP_EVENT_ID_MIN = 1 | (LIOT_COMPONENT_OSI << 16),
    LIOT_TEST_EVENT_ID_1,
    LIOT_TEST_EVENT_ID_2,
    LIOT_TEST_EVENT_ID_3,
    LIOT_KERNEL_FEED_DOG,

    // network
    LIOT_NW_SIGNAL_QUALITY_IND = 1 | (LIOT_COMPONENT_NETWORK << 16),
    LIOT_NW_VOICE_REG_STATUS_IND,
    LIOT_NW_DATA_REG_STATUS_IND,
    LIOT_NW_NITZ_TIME_UPDATE_IND,
    LIOT_NW_JAMMING_DETECT_IND,

    // datacall
    LIOT_DATACALL_ACT_RSP_IND = 1 | (LIOT_COMPONENT_NETWORK_MANAGE << 16),
    LIOT_DATACALL_DEACT_RSP_IND,
    LIOT_DATACALL_PDP_DEACTIVE_IND,

    // usbnet
    LIOT_USBNET_START_RSP_IND = 1 | (LIOT_COMPONENT_NETWORK_USBNET << 16), // response of usbnet start
    LIOT_USBNET_DEACTIVE_IND,     // usbnet down, The cause may be loss of network coverage or detached by the network
    LIOT_USBNET_PORT_CONNECT_IND, // usb plug in
    LIOT_USBNET_PORT_DISCONNECT_IND, // usb pull out

    // uart
    LIOT_UART_RX_RECV_DATA_IND = 1 | (LIOT_COMPONENT_BSP_UART << 16),
    LIOT_UART_RX_OVERFLOW_IND,
    LIOT_UART_TX_FIFO_COMPLETE_IND,
    LIOT_UART_TX_COMPLETE_APP_IND,
    LIOT_UART_RX_RECV_DATA_APP_IND,

    // usb
    LIOT_USB_LINESTATE_IND = 1 | (LIOT_COMPONENT_BSP_USB << 16), // only used for kernel, to handle usb linestate

    // uac
    LIOT_USB_AUDIO_CREATE_DONE = 2000 | (LIOT_COMPONENT_BSP_USB << 16), // UAC created ok, only used for kernel
    LIOT_USB_AUDIO_DATA_IN     = 2001 | (LIOT_COMPONENT_BSP_USB << 16), // UAC received data, only used for kernel
    LIOT_USB_AUDIO_START_PLAY  = 2002 | (LIOT_COMPONENT_BSP_USB << 16), // UAC start receive data, only used for kernel
    LIOT_USB_AUDIO_STOP_PLAY   = 2003 | (LIOT_COMPONENT_BSP_USB << 16), // UAC stop receive data, only used for kernel
    LIOT_USB_AUDIO_START_RECORD =
        2004 | (LIOT_COMPONENT_BSP_USB << 16), // UAC start send data to host, only used for kernel
    LIOT_USB_AUDIO_STOP_RECORD =
        2005 | (LIOT_COMPONENT_BSP_USB << 16), // UAC stop send data to host, only used for kernel

    // bt
    LIOT_BT_START_STATUS_IND = 1 | (LIOT_COMPONENT_BSP_BT << 16), // bt/ble start
    LIOT_BT_STOP_STATUS_IND,                                      // bt/ble stop
    LIOT_BT_BLE_RESET_IND,                                        // bt/ble reset.need to stop bt and restart
    LIOT_BT_VISIBILE_IND,                                         //
    LIOT_BT_HIDDEN_IND,
    LIOT_BT_SET_LOCALNAME_IND,
    LIOT_BT_SET_LOCALADDR_IND,
    LIOT_BT_INQURY_IND,
    LIOT_BT_INQURY_END_IND,
    LIOT_BT_CANCEL_INQURY_IND,
    LIOT_BT_BOND_IND,
    LIOT_BT_CANCELBOND_IND,
    LIOT_BT_CONNECT_IND,
    LIOT_BT_DISCONNECT_IND,
    LIOT_BT_SPP_SEND_IND,
    LIOT_BT_SPP_RECV_IND,

    LIOT_BT_HFP_CONNECTION_IND,
    // ble
    // all:server and client can recieve this event
    // server:only server can recieve this event
    // client:only client can recieve this event
    LIOT_BLE_CONNECT_IND = 100 | (LIOT_COMPONENT_BSP_BT << 16), // all:ble connect
    LIOT_BLE_DISCONNECT_IND,                                    // all:ble disconnect
    LIOT_BLE_UPDATE_CONN_PARAM_IND,                             // all:ble update connection parameter

    LIOT_BLESCAN_REPORT_IND, // client:ble gatt cliet scan and report other devices

    LIOT_BLE_GATT_MTU,      // all:ble connection mtu
    LIOT_BLE_GATT_RECV_IND, // server:when ble client write characteristic value or descriptor,server get the notice
    LIOT_BLE_GATT_RECV_READ_IND, // server:when ble client read characteristic value or descriptor,server get the notice
    LIOT_BLE_GATT_RECV_NOTIFICATION_IND, // client:client recieve notification
    LIOT_BLE_GATT_RECV_INDICATION_IND,   // client:client recieve indication
    LIOT_BLE_GATT_SEND_END,              // server send notification,and recieve send end notice

    LIOT_BLE_GATT_START_DISCOVER_SERVICE_IND,       // client:start discover service
    LIOT_BLE_GATT_DISCOVER_SERVICE_IND,             // client:discover service
    LIOT_BLE_GATT_DISCOVER_CHARACTERISTIC_DATA_IND, // client:discover characteristic
    LIOT_BLE_GATT_DISCOVER_CHARA_DESC_IND,          // client:discover characteristic descriptor
    LIOT_BLE_GATT_CHARA_WRITE_WITH_RSP_IND,         // client:write characterisctc value with response
    LIOT_BLE_GATT_CHARA_WRITE_WITHOUT_RSP_IND,      // client:write characteristic value without response
    LIOT_BLE_GATT_CHARA_READ_IND,                   // client:read characteristic value by hanlde
    LIOT_BLE_GATT_CHARA_READ_BY_UUID_IND,           // client:read characteristic value by uuid
    LIOT_BLE_GATT_CHARA_MULTI_READ_IND,             // client:read miltiple characteristic value
    LIOT_BLE_GATT_DESC_WRITE_WITH_RSP_IND,          // client:wirte characteristic descriptor
    LIOT_BLE_GATT_DESC_READ_IND,                    // client:read characteristic descriptor
    LIOT_BLE_GATT_ATT_ERROR_IND,                    // client:attribute error
    LIOT_BLE_GATT_STATE_CHANGE_IND,                 // server:通道改变
    LIOT_BLE_GATT_SEND_NODIFY_IND, // server:通知server线程从缓存里读取数据发送，只在标准版本使用

    LIOT_BT_HFP_CONNECT_IND = 200 | (LIOT_COMPONENT_BSP_BT << 16), // bt hfp connected
    LIOT_BT_HFP_DISCONNECT_IND,                                    // bt hfp disconnected
    LIOT_BT_HFP_CALL_IND,                                          // bt hfp call state callback
    LIOT_BT_HFP_CALL_SETUP_IND,                                    // bt hfp call setup state callback
    LIOT_BT_HFP_NETWORK_IND,                                       // bt hfp network state callback
    LIOT_BT_HFP_NETWORK_SIGNAL_IND,                                // bt hfp network signal callback
    LIOT_BT_HFP_BATTERY_IND,                                       // bt hfp battery level callback
    LIOT_BT_HFP_CALLHELD_IND,                                      // bt hfp callheld state callback
    LIOT_BT_HFP_AUDIO_IND,                                         // bt hfp audio state callback
    LIOT_BT_HFP_VOLUME_IND,                                        // bt hfp volume type callback
    LIOT_BT_HFP_NETWORK_TYPE_IND,                                  // bt hfp network type callback
    LIOT_BT_HFP_RING_IND,                                          // bt hfp ring indication callback
    LIOT_BT_HFP_CODEC_IND,                                         // bt hfp codec type callback

    LIOT_BT_A2DP_AUDIO_CONFIG_IND = 300 | (LIOT_COMPONENT_BSP_BT << 16), // bt a2dp audio config callback
    LIOT_BT_A2DP_AUDIO_STATE_START_IND,                                  // bt a2dp audio state start callback
    LIOT_BT_A2DP_AUDIO_STATE_STOPPED_IND,                                // bt a2dp audio state stop callback
    LIOT_BT_A2DP_CONNECTION_STATE_CONNECTED_IND,                         // bt a2dp connect callback
    LIOT_BT_A2DP_CONNECTION_STATE_DISCONNECTED_IND,                      // bt a2dp disconnect callback

    LIOT_BT_AVRCP_CONNECTION_STATE_CONNECTED_IND = 400 | (LIOT_COMPONENT_BSP_BT << 16), // bt avrcp connect callback
    LIOT_BT_AVRCP_CONNECTION_STATE_DISCONNECTED_IND,                                    // bt avrcp disconnect callback
    LIOT_BT_AVRCP_VOLUME_CHANGE_IND, // bt avrcp volume change callback

    // led_cfg(network status for netlight)
    LIOT_LEDCFG_EVENT_IND = 1 | (LIOT_COMPONENT_BSP_PWM << 16),
    LIOT_LEDCFG_CB_CREATE_IND,
    LIOT_VIRTUAL_PWM_EVENT,
    LIOT_LEDCFG_VIRT_PWM_EVENT,
    LIOT_HR_VIRT_PWM_EVENT,
    // sleep
    LIOT_SLEEP_ENETR_AUTO_SLEPP = 1 | (LIOT_COMPONENT_PM_SLEEP << 16),
    LIOT_SLEEP_EXIT_AUTO_SLEPP  = 2 | (LIOT_COMPONENT_PM_SLEEP << 16),

    // power
    LIOT_SLEEP_QUICK_POWER_DOWM  = 1 | (LIOT_COMPONENT_PM << 16),
    LIOT_SLEEP_NORMAL_POWER_DOWM = 2 | (LIOT_COMPONENT_PM << 16),
    LIOT_SLEEP_QUICK_RESET       = 3 | (LIOT_COMPONENT_PM << 16),
    LIOT_SLEEP_NORMAL_RESET      = 4 | (LIOT_COMPONENT_PM << 16),
    LIOT_SLEEP_FAST_POWEROFF     = 5 | (LIOT_COMPONENT_PM << 16),

    // sim
    LIOT_PBK_INIT_OK_IND     = 1 | (LIOT_COMPONENT_SIM << 16),
    LIOT_SIM_APREADY_CMD_IND = 2 | (LIOT_COMPONENT_SIM << 16),

    // pwrkey
    LIOT_PWRKEY_SHUTDOWN_START_IND = 1 | (LIOT_COMPONENT_BSP_KEYPAD << 16),
    LIOT_PWRKEY_PRESS_IND,
    LIOT_PWRKEY_RELEASE_IND,
    LIOT_PWRKEY_LONGPRESS_IND,

    // wifiscan
    LIOT_WIFISCAN_EVENT_DO        = 1 | (LIOT_COMPONENT_BSP_WIFISCAN << 16),
    LIOT_WIFISCAN_EVENT_ASYNC_IND = 5 | (LIOT_COMPONENT_BSP_WIFISCAN << 16),
    // gnss
    LIOT_GNSS_OK_CMD_IND         = 1 | (LIOT_COMPONENT_BSP_GNSS << 16),
    LIOT_GNSS_FAIL_0_CMD_IND     = 2 | (LIOT_COMPONENT_BSP_GNSS << 16),
    LIOT_GNSS_FAIL_1_CMD_IND     = 3 | (LIOT_COMPONENT_BSP_GNSS << 16),
    LIOT_GPS_INFO_CMD            = 4 | (LIOT_COMPONENT_BSP_GNSS << 16),
    LIOT_GNSS_POWER_EVENT_REQ    = 5 | (LIOT_COMPONENT_BSP_GNSS << 16),
    LIOT_GNSS_DISABLE_STATUS_IND = 6 | (LIOT_COMPONENT_BSP_GNSS << 16),

    // lvgl
    LIOT_LVGL_QUIT_IND = 1 | (LIOT_COMPONENT_BSP_LVGL << 16),

    // virt at
    LIOT_VIRT_AT_RX_RECV_DATA_IND = 1 | (LIOT_COMPONENT_BSP_VIRT_AT << 16),

    // sd
    LIOT_SDDET_EVENT_IND = 1 | (LIOT_COMPONENT_STORAGE_SDEMMC << 16),

    // headset_det
    LIOT_HEADSET_DET_EVENT_IND = 1 | (LIOT_COMPONENT_AUDIO_HEADSET << 16),

    // tcpip app
    LIOT_TCPIP_APP_PDP_EVT_IND = 1 | (LIOT_COMPONENT_LWIP << 16),

    // locator
    LIOT_LBS_EVENT_WIFISCAN_REC = 1 | (LIOT_COMPONENT_LWIP_LBS << 16),
    // BSP GPIO
    LIOT_GPIOCTRL_EVENT_IND       = 1 | (LIOT_COMPONENT_BSP_GPIO << 16),
    LIOT_GPIOCTRL_TIMER_EVENT_IND = 2 | (LIOT_COMPONENT_BSP_GPIO << 16),
    //
    // next is reserved for app to add event id!
    //
    LIOT_EVENT_APP_START = 1 | (LIOT_COMPONENT_APP_START << 16), // app event start

    LIOT_EVEN_MAX = 0xFFFF | (LIOT_COMPONENT_MAX << 16), // event max
} liot_api_event_id_e;

/*========================================================================
 *  Variable Definition
 *========================================================================*/
#define CHIP_SECTOR_SIZE (0x1000) // erase 4k each times
#define CHIP_BLOCK_SIZE  (0x10000) // erase 64k each times

/*========================================================================
 *  function Definition
 *========================================================================*/
typedef void (*liot_callback)(void *ctx);

/*
uint32_t liot_get_free_heap_size(void);
uint32_t liot_get_total_heap_size(void);
uint32_t liot_flash_erase(uint32_t SectorAddress, uint32_t size);
uint32_t liot_flash_read(uint8_t* pData, uint32_t ReadAddr, uint32_t Size);
uint32_t liot_flash_write(uint8_t* pData, uint32_t WriteAddr, uint32_t Size);
*/
#ifdef __cplusplus
} /*"C" */
#endif

#endif /* LIOT_API_COMMON_H */
