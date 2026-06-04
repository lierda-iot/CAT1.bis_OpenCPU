/**
 * @File Name: liot_dev.h
 * @brief Device information and control API for Lierda modules
 * @author ljz email:ciot_iot_support@lierda.com
 * @version 1.1
 * @date 2025-07-08
 *
 * @copyright Copyright (c) 2023 Zhejiang Lierda Internet of Things Technology Co., Ltd.
 *
 */

#ifndef _LIOT_DEV_H_
#define _LIOT_DEV_H_

#ifdef __cplusplus // this area code will compile with C program
extern "C" {
#endif

#include "liot_api_common.h"
#include "liot_type.h"

/*===========================================================================
 * Macro Definition
 ===========================================================================*/

#define LIOT_DEV_ERRCODE_BASE (LIOT_COMPONENT_STATE_INFO << 16)

/*===========================================================================
 * Enum
 ===========================================================================*/

typedef enum
{
    LIOT_DEV_CFUN_MIN  = 0,
    LIOT_DEV_CFUN_FULL = 1,
    LIOT_DEV_CFUN_AIR  = 4,
} liot_dev_cfun_e;

typedef enum
{
    LIOT_DEV_SUCCESS = LIOT_SUCCESS,                                       ///< Operation successful
    LIOT_DEV_EXECUTE_ERR = 1 | LIOT_DEV_ERRCODE_BASE,                      ///< Execution error
    LIOT_DEV_MEM_ADDR_NULL_ERR,                                            ///< Memory address null error
    LIOT_DEV_INVALID_PARAM_ERR,                                            ///< Invalid parameter error
    LIOT_DEV_BUSY_ERR,                                                     ///< Device busy error
    LIOT_DEV_BUFF_TOO_SMALL_ERR,                                           ///< Buffer size too small error
    LIOT_DEV_SEMAPHORE_CREATE_ERR,                                         ///< Semaphore creation error
    LIOT_DEV_SEMAPHORE_TIMEOUT_ERR,                                        ///< Semaphore timeout error
    LIOT_DEV_HANDLE_INVALID_ERR,                                           ///< Invalid handle error
    LIOT_DEV_CFW_CFUN_GET_ERR = 15 | LIOT_DEV_ERRCODE_BASE,                ///< CFW CFUN get error
    LIOT_DEV_CFW_CFUN_SET_CURR_COMM_FLAG_ERR = 18 | LIOT_DEV_ERRCODE_BASE, ///< CFW CFUN set current comm flag error
    LIOT_DEV_CFW_CFUN_SET_COMM_ERR,                                        ///< CFW CFUN set comm error
    LIOT_DEV_CFW_CFUN_SET_COMM_RSP_ERR,                                    ///< CFW CFUN set comm response error
    LIOT_DEV_CFW_CFUN_RESET_BUSY = 25 | LIOT_DEV_ERRCODE_BASE,             ///< CFW CFUN reset busy
    LIOT_DEV_CFW_CFUN_RESET_CFW_CTRL_ERR,                                  ///< CFW CFUN reset CFW control error
    LIOT_DEV_CFW_CFUN_RESET_CFW_CTRL_RSP_ERR,                              ///< CFW CFUN reset CFW control response error
    LIOT_DEV_IMEI_GET_ERR = 33 | LIOT_DEV_ERRCODE_BASE,                    ///< IMEI get error
    LIOT_DEV_SN_GET_ERR = 36 | LIOT_DEV_ERRCODE_BASE,                      ///< Serial number get error
    LIOT_DEV_UID_READ_ERR = 39 | LIOT_DEV_ERRCODE_BASE,                    ///< UID read error
    LIOT_DEV_DNS_RAAD_ERR = 45 | LIOT_DEV_ERRCODE_BASE,                    ///< DNS read error
    LIOT_DEV_TEMP_GET_ERR = 50 | LIOT_DEV_ERRCODE_BASE,                    ///< Temperature get error
    LIOT_DEV_WDT_CFG_ERR = 53 | LIOT_DEV_ERRCODE_BASE,                     ///< Watchdog timer configuration error
    LIOT_DEV_HEAP_QUERY_ERR = 56 | LIOT_DEV_ERRCODE_BASE,                  ///< Heap query error
    LIOT_DEV_AUTHCODE_READ_ERR = 90 | LIOT_DEV_ERRCODE_BASE,               ///< Auth code read error
    LIOT_DEV_AUTHCODE_ADDR_NULL_ERR,                                       ///< Auth code address null error
    LIOT_DEV_READ_WIFI_MAC_ERR = 100 | LIOT_DEV_ERRCODE_BASE,              ///< Read WiFi MAC address NV error
} liot_errcode_dev_e;

/*===========================================================================
 * Struct
 ===========================================================================*/

typedef struct
{
    UINT32 total_size; ///< Total heap memory size
    UINT32 avail_size; ///< Available size. Actual allocatable size may be less than this
} liot_memory_heap_state_s;

/**
 * @brief Get IMEI number
 *
 * This function retrieves the International Mobile Equipment Identity (IMEI) number
 * of the device.
 *
 * @param p_imei    Pointer to the buffer for reading IMEI
 * @param imei_len  Size of the IMEI buffer (should be at least 16 bytes)
 * @param nSim      SIM card index, value: 0-1
 * @return Error code indicating success or failure
 *         - LIOT_DEV_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_errcode_dev_e liot_dev_get_imei(char *p_imei, size_t imei_len, uint8_t nSim);
typedef liot_errcode_dev_e (*_api_liot_dev_get_imei_t)(char *p_imei, size_t imei_len, uint8_t nSim);

/**
 * @brief Get firmware version of the device
 *
 * This function retrieves the firmware version of the device.
 *
 * @param p_version    Pointer to the buffer for reading version
 * @param version_len  Size of the version buffer
 * @return Error code indicating success or failure
 *         - LIOT_DEV_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_errcode_dev_e liot_dev_get_firmware_version(char *p_version, size_t version_len);
typedef liot_errcode_dev_e (*_api_liot_dev_get_firmware_version_t)(char *p_version, size_t version_len);

/**
 * @brief Get serial number (SN)
 *
 * This function retrieves the serial number (SN) of the device.
 *
 * @param p_sn    Pointer to the buffer for reading SN
 * @param sn_len  Size of the SN buffer
 * @param nSim    SIM card index, value: 0-1
 * @return Error code indicating success or failure
 *         - LIOT_DEV_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_errcode_dev_e liot_dev_get_sn(char *p_sn, size_t sn_len, uint8_t nSim);
typedef liot_errcode_dev_e (*_api_liot_dev_get_sn_t)(char *p_sn, size_t sn_len, uint8_t nSim);

/**
 * @brief Get product ID of the device
 *
 * This function retrieves the product ID of the device.
 *
 * @param p_product_id     Pointer to the buffer for reading product ID
 * @param product_id_len   Size of the product ID buffer
 * @return Error code indicating success or failure
 *         - LIOT_DEV_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_errcode_dev_e liot_dev_get_product_id(char *p_product_id, size_t product_id_len);
typedef liot_errcode_dev_e (*_api_liot_dev_get_product_id_t)(char *p_product_id, size_t product_id_len);

/**
 * @brief Get sub-firmware version of the device
 *
 * This function retrieves the sub-firmware version of the device.
 *
 * @param p_subversion     Pointer to the buffer for reading sub-version
 * @param subversion_len   Size of the sub-version buffer
 * @return Error code indicating success or failure
 *         - LIOT_DEV_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_errcode_dev_e liot_dev_get_firmware_subversion(char *p_subversion, size_t subversion_len);
typedef liot_errcode_dev_e (*_api_liot_dev_get_firmware_subversion_t)(char *p_subversion, size_t subversion_len);

/**
 * @brief Get model name of the device
 *
 * This function retrieves the model name of the device.
 *
 * @param p_model    Pointer to the buffer for reading device model
 * @param model_len  Size of the device model buffer
 * @return Error code indicating success or failure
 *         - LIOT_DEV_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_errcode_dev_e liot_dev_get_model(char *p_model, size_t model_len);
typedef liot_errcode_dev_e (*_api_liot_dev_get_model_t)(char *p_model, size_t model_len);

/**
 * @brief Set modem function mode
 *
 * This function sets the modem function mode of the device.
 *
 * @param at_dst_fun  Target modem function mode:
 *                    - LIOT_DEV_CFUN_MIN (0): Minimum functionality
 *                    - LIOT_DEV_CFUN_FULL (1): Full functionality
 *                    - LIOT_DEV_CFUN_AIR (4): Airplane mode
 * @param rst         Whether to reset modem before setting:
 *                    - 0: Do not reset
 *                    - 1: Reset before setting
 * @param nSim        SIM card index, value: 0-1
 * @return Error code indicating success or failure
 *         - LIOT_DEV_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_errcode_dev_e liot_dev_set_modem_fun(uint8_t at_dst_fun, uint8_t rst, uint8_t nSim);
typedef liot_errcode_dev_e (*_api_liot_dev_set_modem_fun_t)(uint8_t at_dst_fun, uint8_t rst, uint8_t nSim);

/**
 * @brief Get current modem function mode
 *
 * This function retrieves the current modem function mode of the device.
 *
 * @param p_function  Pointer to store current modem function mode:
 *                    - 0: Minimum functionality
 *                    - 1: Full functionality
 *                    - 4: Airplane mode
 * @param nSim        SIM card index, value: 0-1
 * @return Error code indicating success or failure
 *         - LIOT_DEV_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_errcode_dev_e liot_dev_get_modem_fun(uint8_t *p_function, uint8_t nSim);
typedef liot_errcode_dev_e (*_api_liot_dev_get_modem_fun_t)(uint8_t *p_function, uint8_t nSim);

/**
 * @brief Query heap memory state
 *
 * This function retrieves information about the heap memory state including
 * total size and available size.
 *
 * @param liot_heap_state  Pointer to structure to store heap memory state information
 *                         (refer to liot_memory_heap_state_s)
 * @return Error code indicating success or failure
 *         - LIOT_DEV_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_errcode_dev_e liot_dev_memory_size_query(liot_memory_heap_state_s *liot_heap_state);
typedef liot_errcode_dev_e (*_api_liot_dev_memory_size_query_t)(liot_memory_heap_state_s *liot_heap_state);

/**
 * @brief Configure watchdog timer
 *
 * This function configures the watchdog timer of the device.
 *
 * @param opt  Watchdog enable/disable:
 *             - 0: Disable watchdog
 *             - 1: Enable watchdog
 * @return Error code indicating success or failure
 *         - LIOT_DEV_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_errcode_dev_e liot_dev_cfg_wdt(uint8_t opt);
typedef liot_errcode_dev_e (*_api_liot_dev_cfg_wdt_t)(uint8_t opt);

/**
 * @brief Feed system watchdog (reset the timer)
 *
 * This function feeds the system watchdog timer to prevent it from triggering
 * a system reset.
 *
 * @return Error code indicating success or failure
 *         - LIOT_DEV_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_errcode_dev_e liot_dev_feed_wdt(void);
typedef liot_errcode_dev_e (*_api_liot_dev_feed_wdt_t)(void);


/**
 * @brief Enumeration for Band Mode Query Types
 * 
 * Defines the types of band mode queries that can be performed, including retrieving 
 * the list of used bands and the list of supported bands.
 */
typedef enum
{
    LIOT_DEV_GET_CAN_USED_BAND_LIST = 0,    ///< Query to get the list of currently used bands
    LIOT_DEV_GET_SUPPORT_BAND_LIST = 1,     ///< Query to get the list of supported bands
    LIOT_DEV_GET_BAND_MAX_NUM               ///< Placeholder for maximum number of band query types
} Liot_DevGetBandMode_e;

/**
 * @brief Set available band mode (equivalent to AT+ECBAND=3,5,8)
 *
 * This function is used to set the available band mode of the device by specifying 
 * the number of bands and the order of bands.
 *
 * @param bandNum    Number of bands
 * @param orderBand  Pointer to an array storing the order of bands
 * @return Error code indicating success or failure
 *         - LIOT_DEV_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_errcode_dev_e Liot_DevSetBandMode(uint8_t bandNum, uint8_t *orderBand);
typedef liot_errcode_dev_e (*_api_Liot_DevSetBandMode_t)(uint8_t bandNum, uint8_t *orderBand);
/**
 * @brief Query available band mode (equivalent to AT+ECBAND?)
 *
 * This function is used to query the current available band mode of the device, 
 * returning the number of bands and their order. The orderBand must be set to a size of 32 bytes.
 *
 * @param bandNum    Pointer to a variable storing the number of bands
 * @param orderBand  Pointer to an array storing the order of bands
 * @return Error code indicating success or failure
 *         - LIOT_DEV_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_errcode_dev_e Liot_DevGetBandMode(Liot_DevGetBandMode_e mode, uint8_t *bandNum, uint8_t *orderBand);
typedef liot_errcode_dev_e (*_api_Liot_DevGetBandMode_t)(Liot_DevGetBandMode_e mode, uint8_t *bandNum, uint8_t *orderBand);


#define  SUPPORT_MAX_FREQ_NUM   8       ///< Maximum number of supported frequencies

/**
 * @brief Frequency Operation Mode Enumeration
 * 
 * Defines the operation modes for frequency configuration, including unlocking cells, 
 * setting priority frequencies, locking frequencies or cells, etc.
 */
typedef enum
{
    LIOT_DEV_SET_UNLOCK = 0,                 ///< Unlock cell
    LIOT_DEV_SET_PRIORITY_FREQ = 1,          ///< Set priority frequency
    LIOT_DEV_SET_LOCK_FREQ_OR_CELLID = 2,    ///< Lock frequency or cell
    LIOT_DEV_SET_CLEAN_PRIORITY_FREQ = 3,    ///< Clear priority frequency
    LIOT_DEV_GET_FREQ = 4,                   ///< Get frequency information
    LIOT_DEV_SET_MAX_FREQ_MODE               ///< Maximum frequency mode (placeholder)
} Liot_DevFreqOpt_e;

/**
 * @brief Frequency Configuration Structure
 * 
 * Used to configure frequency-related parameters of the device, including operation mode, 
 * physical cell ID, number of frequencies, and frequency list.
 */
typedef struct 
{
    Liot_DevFreqOpt_e mode;       ///< Operation mode, refer to
    UINT16 phyCellId;             ///< Physical Cell ID, range: 0 - 503

    UINT8 arfcnNum;               ///< Number of frequencies:
                                  ///< - Must not be 0 when the mode is 
                                  ///< - Maximum value is

    UINT32 lockedArfcn;           ///< Locked EARFCN (E-UTRA Absolute Radio Frequency Channel Number)
    UINT32 arfcnList[SUPPORT_MAX_FREQ_NUM]; ///< Frequency list, supports up to 
} Liot_DevFreqConfig_t; 

/**
 * @brief Configure Device Frequency
 * 
 * This interface must be called in flight mode to configure frequency-related parameters of the device.
 * 
 * @param info Pointer to the frequency configuration structure [Liot_DevFreqConfig_t]
 * @return Error code:
 *         - LIOT_DEV_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_errcode_dev_e Liot_DevFreqConfig(Liot_DevFreqConfig_t *info);
typedef liot_errcode_dev_e (*_api_Liot_DevFreqConfig_t)(Liot_DevFreqConfig_t *info);

/**
 * @brief RRC Fast Release Configuration Structure
 * 
 * Used to configure the RRC fast release feature.
 */
typedef struct
{
    bool mode;            ///< Enable or disable RRC fast release:
                          ///< - true: Enable
                          ///< - false: Disable
    uint16_t idle_time;   ///< Time to wait before performing fast release (in seconds)
    uint16_t retry_time;  ///< Retry time (not currently in use)
} Liot_DevRRCRelease_t;

/**
 * @brief Configure RRC Fast Release
 * 
 * This function configures the RRC fast release feature. Due to the underlying implementation, 
 * it may cause a short period during which downlink data cannot be received. 
 * For applications requiring high real-time performance and reliability, use with caution.
 * 
 * @param cfg Pointer to the RRC fast release configuration structure [Liot_DevRRCRelease_t]
 * @return Error code:
 *         - LIOT_DEV_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_errcode_dev_e Liot_RRCRelease(Liot_DevRRCRelease_t *cfg);
typedef liot_errcode_dev_e (*_api_Liot_RRCRelease_t)(Liot_DevRRCRelease_t *cfg);

#define LIOT_WARE_DEFAULT_DNS_NUM            2
#define LIOT_WARE_ADDR_LEN                   64

/**
 * @brief DNS Server Address Structure
 * 
 * Used to store the device's DNS server addresses, including both IPv4 and IPv6 addresses.
 * example: ipv4Dns[0] = "192.168.1.1"
 *          ipv6Dns[0] = "2001:0db8:85a3:0000:0000:8a2e:0370:7334"
 */
typedef struct 
{
    UINT8 ipv4Dns[LIOT_WARE_DEFAULT_DNS_NUM][LIOT_WARE_ADDR_LEN + 1]; ///< IPv4 DNS address list
    UINT8 ipv6Dns[LIOT_WARE_DEFAULT_DNS_NUM][LIOT_WARE_ADDR_LEN + 1]; ///< IPv6 DNS address list
} Liot_DevDnsServer_t;


/**
 * @brief Set DNS Server Addresses
 * 
 * This function sets the device's DNS server addresses.
 * 
 * @param dns_servers Pointer to the DNS server address structure [Liot_DevDnsServer_t]
 * @return Error code:
 *         - LIOT_DEV_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_errcode_dev_e Liot_DevSetDnsServersAddr(Liot_DevDnsServer_t *dns_servers);
/**
 * @brief Get DNS Server Addresses
 * 
 * This function retrieves the currently configured DNS server addresses of the device.
 * 
 * @param dns_servers Pointer to the DNS server address structure [Liot_DevDnsServer_t]
 * @return Error code:
 *         - LIOT_DEV_SUCCESS: Operation successful
 *         - Other: Error code
 */
liot_errcode_dev_e Liot_DevGetDnsServersAddr(Liot_DevDnsServer_t *dns_servers);

/**
 * @brief Get Hardware Version Information
 * 
 * This function retrieves the hardware version information of the device.
 * 
 * @param[out] hdversion Pointer to the buffer where the hardware version string will be stored.
 *                       The buffer must be allocated by the caller with sufficient size.
 * @param[in] len        Length of the buffer pointed to by `hdversion`. 
 *                       Must be at least 32 bytes.
 * @return Error code:
 *         - LIOT_DEV_SUCCESS: Operation successful.
 *         - Other: Error code indicating failure (refer to `liot_errcode_dev_e` for details).
 *  
 */
liot_errcode_dev_e Liot_DevGetHardWareInfo(const char*hdversion, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif