/**
 * @File Name: liot_datacall.h
 * @brief
 * @Version : 1.0
 * @Creat Date : 2025-06-26
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */

#ifndef _LIOT_DATACALL_H_
#define _LIOT_DATACALL_H_

#ifdef __cplusplus // this area code will compile with c program
extern "C" {
#endif

#include "liot_api_common.h"
#include "liot_type.h"

#define LIOT_DATACALL_ERRCODE_BASE (LIOT_COMPONENT_NETWORK_MANAGE << 16)

#define LIOT_PROFILE_IDX_MIN 1
#define LIOT_PROFILE_IDX_MAX 7

#define LIOT_APN_LEN_MAX      100
#define LIOT_USERNAME_LEN_MAX 64
#define LIOT_PASSWORD_LEN_MAX 64

#define LIOT_DATA_TYPE_INVALID -1
#define LIOT_DATA_TYPE_IP      1
#define LIOT_DATA_TYPE_IPV6    2
#define LIOT_DATA_TYPE_IPV4V6  3

#define LIOT_DATA_CID_DEFAULT 1
#define LIOT_DATA_CID_MIN     1
#define LIOT_DATA_CID_MAX     15

#define LIOT_DATACALL_REGISTER_ALL_SIM 0XFF
#define LIOT_DATACALL_REGISTER_ALL_PDP 0XFF

typedef enum
{
    LIOT_DATA_AUTH_TYPE_AUTO = 0xFF,
    LIOT_DATA_AUTH_TYPE_NONE = 0, // no authentication protocol is used for this PDP context. Username and password are
                                  // removed if previously specified
    LIOT_DATA_AUTH_TYPE_PAP,
    LIOT_DATA_AUTH_TYPE_CHAP,
} liot_data_auth_type_e;

typedef struct
{
    UINT32 addr;
} liot_ip4_addr_t;

typedef struct
{
    UINT32 addr[4];
} liot_ip6_addr_t;

typedef struct
{
    union
    {
        liot_ip4_addr_t v4;
        liot_ip6_addr_t v6;
    } addr;

    UINT8 type;
} liot_ip_addr_t;

typedef struct
{
    INT32 cid;
    INT32 ip_type;
    BOOL apn_valid;
    INT32 apn_len;
    CHAR apn[LIOT_APN_LEN_MAX];
    BOOL user_valid;
    INT32 user_len;
    CHAR username[LIOT_USERNAME_LEN_MAX];
    BOOL pwd_valid;
    INT32 pwd_len;
    char password[LIOT_PASSWORD_LEN_MAX];
    CHAR auth_type_valid;
    INT32 auth_type;
} liot_data_profile_info_t;

typedef struct
{
    liot_ip4_addr_t ip;
    liot_ip4_addr_t pri_dns;
    liot_ip4_addr_t sec_dns;
} liot_v4_address_status;

typedef struct
{
    INT32 state;                 // dial status
    liot_v4_address_status addr; // IPv4 address information
} liot_v4_info;

typedef struct
{
    liot_ip6_addr_t ip;
    liot_ip6_addr_t pri_dns;
    liot_ip6_addr_t sec_dns;
} liot_v6_address_status;

typedef struct
{
    INT32 state;                 // dial status
    liot_v6_address_status addr; // IPv6 address information
} liot_v6_info;

typedef struct
{
    INT32 cid;
    INT32 ip_version;
    liot_v4_info v4;
    liot_v6_info v6;
    char apn_name[LIOT_APN_LEN_MAX];
} liot_data_call_info_t;

typedef struct
{
    uint8_t ip_version;
    liot_ip4_addr_t ipv4;
    liot_ip6_addr_t ipv6;
    char apn_name[LIOT_APN_LEN_MAX];
} liot_data_call_default_pdn_info_s;

typedef enum
{
    LIOT_DATACALL_CONNECT_EVENT         = 1,
    LIOT_DATACALL_DISCONNECT_EVENT      = 2,
    LIOT_DATACALL_AUTO_DISCONNECT_EVENT = 3,
} liot_datacall_event_id_e;

typedef enum
{
    LIOT_DATACALL_STATE_IDLE = 0,
    LIOT_DATACALL_STATE_ACTIVING,
    LIOT_DATACALL_STATE_ACTIVED,
    LIOT_DATACALL_STATE_DEACTIVING,
} liot_datacall_state_e;

typedef enum
{
    LIOT_DATACALL_SUCCESS     = 0,
    LIOT_DATACALL_EXECUTE_ERR = 1 | LIOT_DATACALL_ERRCODE_BASE,
    LIOT_DATACALL_MEM_ADDR_NULL_ERR,
    LIOT_DATACALL_INVALID_PARAM_ERR,
    LIOT_DATACALL_NW_REGISTER_TIMEOUT_ERR,
    LIOT_DATACALL_CFW_ACT_STATE_GET_ERR = 5 | LIOT_DATACALL_ERRCODE_BASE,
    LIOT_DATACALL_REPEAT_ACTIVE_ERR,
    LIOT_DATACALL_REPEAT_DEACTIVE_ERR,
    LIOT_DATACALL_CFW_PDP_CTX_SET_ERR,
    LIOT_DATACALL_CFW_PDP_CTX_GET_ERR,
    LIOT_DATACALL_CS_CALL_ERR = 10 | LIOT_DATACALL_ERRCODE_BASE,
    LIOT_DATACALL_CFW_CFUN_GET_ERR,
    LIOT_DATACALL_CFUN_DISABLE_ERR,
    LIOT_DATACALL_NW_STATUS_GET_ERR,
    LIOT_DATACALL_NOT_REGISTERED_ERR,
    LIOT_DATACALL_NO_MEM_ERR = 15 | LIOT_DATACALL_ERRCODE_BASE,
    LIOT_DATACALL_CFW_ATTACH_STATUS_GET_ERR,
    LIOT_DATACALL_SEMAPHORE_CREATE_ERR,
    LIOT_DATACALL_SEMAPHORE_TIMEOUT_ERR,
    LIOT_DATACALL_CFW_ATTACH_REQUEST_ERR,
    LIOT_DATACALL_CFW_ACTIVE_REQUEST_ERR = 20 | LIOT_DATACALL_ERRCODE_BASE,
    LIOT_DATACALL_ACTIVE_FAIL_ERR,
    LIOT_DATACALL_CFW_DEACTIVE_REQUEST_ERR,
    LIOT_DATACALL_NO_DFTPDN_CFG_CONTEXT,
    LIOT_DATACALL_NO_DFTPDN_INFO_CONTEXT,
} liot_datacall_errcode_e;

typedef void (*liot_datacall_callback)(uint8_t nSim, unsigned int ind_type, int profile_idx, bool result, void *ctx);

/**
 * @brief Register a callback function for data call events
 * @details Registers a callback to receive notifications about data call state changes
 * 
 * @param nSim SIM card index (0-1 or 0xFF for all SIMs)
 * @param profile_idx PDP context ID (1-7 or 0xFF for all contexts)
 * @param datacall_cb Callback function pointer
 * @param ctx User context pointer passed to callback
 * @return liot_datacall_errcode_e Error code
 */
liot_datacall_errcode_e liot_datacall_register_cb(uint8_t nSim,
                                                         int profile_idx,
                                                         liot_datacall_callback datacall_cb,
                                                         void *ctx);

/**
 * @brief Unregister a data call callback function
 * @details Removes a previously registered callback function
 * 
 * @param nSim SIM card index (0-1 or 0xFF for all SIMs)
 * @param profile_idx PDP context ID (1-7 or 0xFF for all contexts)
 * @param datacall_cb Callback function pointer to unregister
 * @param ctx User context pointer
 * @return liot_datacall_errcode_e Error code
 */
liot_datacall_errcode_e liot_datacall_unregister_cb(uint8_t nSim,
                                                           int profile_idx,
                                                           liot_datacall_callback datacalliot_cb,
                                                           void *ctx);

/**
 * @brief Get data call information
 * @details Retrieves detailed information about an active data call
 * 
 * @param nSim SIM card index (0-1)
 * @param profile_idx PDP context ID (1-7)
 * @param call_info Pointer to store the retrieved data call info
 * @return liot_datacall_errcode_e Error code
 */
liot_datacall_errcode_e liot_get_data_call_info(UINT8 nSim, INT32 profile_idx, liot_data_call_info_t *call_info);

/**
 * @brief Start a data call
 * @details Initiates a data call with specified parameters
 * 
 * @param nSim SIM card index (0-1)
 * @param cid PDP context ID (1-7)
 * @param ip_version IP type (1=IPv4, 2=IPv6, 3=IPv4v6)
 * @param apn_name APN string
 * @param username Authentication username
 * @param password Authentication password
 * @param auth_type Authentication type
 * @return liot_datacall_errcode_e Error code
 */
liot_datacall_errcode_e liot_start_data_call(
    UINT8 nSim, INT32 cid, INT32 ip_version, CHAR *apn_name, CHAR *username, CHAR *password, INT32 auth_type);

/**
 * @brief Get default PDN information
 * @details Retrieves information about the default Packet Data Network
 * 
 * @param nSim SIM card index (0-1)
 * @param ctx Pointer to store the default PDN info
 * @return liot_datacall_errcode_e Error code
 */
liot_datacall_errcode_e liot_datacall_get_default_pdn_info(uint8_t nSim, liot_data_call_default_pdn_info_s *ctx);

/**
 * @brief Check if a PDP context is active
 * @details Determines whether the specified PDP context is currently active
 * 
 * @param nSim SIM card index (0-1)
 * @param profile_idx PDP context ID (1-7)
 * @return bool true if active, false otherwise
 */
bool liot_datacall_get_sim_profile_is_active(uint8_t nSim, int profile_idx);

/**
 * @brief Stop an active data call
 * @details Terminates an active data call for the specified PDP context
 * 
 * @param nSim SIM card index (0-1)
 * @param profile_idx PDP context ID (1-7)
 * @return liot_datacall_errcode_e Error code
 */
liot_datacall_errcode_e liot_stop_data_call(UINT8 nSim, INT32 profile_idx);

/**
 * @brief Set data call async mode
 * @details Configures synchronous or asynchronous mode for data call operations
 * 
 * @param nSim SIM card index (0-1)
 * @param profile_idx PDP context ID (1-7)
 * @param enable true for async mode, false for sync mode
 * @return liot_datacall_errcode_e Error code
 */
liot_datacall_errcode_e liot_set_data_call_asyn_mode(uint8_t nSim, int cid, bool enable);

/*
 * @brief Wait for network registration (seconds)
 * @details Blocks until network registration succeeds or timeout occurs
 * 
 * @param nSim SIM card index (0-1)
 * @param timeout_s Timeout in seconds
 * @return liot_datacall_errcode_e Error code
 */
liot_datacall_errcode_e liot_network_register_wait(uint8_t nSim, unsigned int timeout_s);

/**
 * @brief Wait for network registration (milliseconds)
 * @details Blocks until network registration succeeds or timeout occurs
 * 
 * @param nSim SIM card index (0-1)
 * @param timeout_ms Timeout in milliseconds
 * @return liot_datacall_errcode_e Error code
 */
liot_datacall_errcode_e liot_network_register_wait_ms(uint8_t nSim, unsigned long timeout_ms);

/**
 * @brief Set NAT mode
 * @details Configures NAT mode for the specified SIM card
 * 
 * @param nSim SIM card index (0-1)
 * @param natmode NAT mode (0=enable, 1=disable)
 * @return liot_datacall_errcode_e Error code
 */
liot_datacall_errcode_e liot_datacall_set_nat(uint8_t nSim, UINT32 natmode);

/**
 * @brief Get NAT mode
 * @details Retrieves the current NAT mode for the specified SIM card
 * 
 * @param nSim SIM card index (0-1)
 * @param natmode Pointer to store the NAT mode
 * @return liot_datacall_errcode_e Error code
 */
liot_datacall_errcode_e liot_datacall_get_nat(uint8_t nSim, UINT32 *natmode);

/**
 * @brief Convert IPv4 address to string
 * @details Converts an IPv4 address structure to a dotted-decimal string
 * 
 * @param addr Pointer to IPv4 address structure
 * @return CHAR* Pointer to the resulting string
 */
CHAR *liot_ip4addr_ntoa(liot_ip4_addr_t *addr);

/**
 * @brief Convert IPv6 address to string
 * @details Converts an IPv6 address structure to a hexadecimal string
 * 
 * @param addr Pointer to IPv6 address structure
 * @return CHAR* Pointer to the resulting string
 */
CHAR *liot_ip6addr_ntoa(liot_ip6_addr_t *addr);

/**
 * @brief Convert string to IPv4 address
 * @details Parses a dotted-decimal string into an IPv4 address structure
 * 
 * @param cp Pointer to the input string
 * @param addr Pointer to store the parsed address
 * @return BOOL true if successful, false otherwise
 */
BOOL liot_ip4addr_aton(CHAR *cp, liot_ip4_addr_t *addr);

/**
 * @brief Convert string to IPv6 address
 * @details Parses a hexadecimal string into an IPv6 address structure
 * 
 * @param cp Pointer to the input string
 * @param addr Pointer to store the parsed address
 * @return BOOL true if successful, false otherwise
 */
BOOL liot_ip6addr_aton(CHAR *cp, liot_ip6_addr_t *addr);

/**
 * @brief Get network registration CEREG status
 * @details Retrieves the CEREG registration status for the specified SIM card
 * 
 * @param nSim SIM card index (0-1)
 * @return liot_datacall_errcode_e Error code
 */
liot_datacall_errcode_e liot_network_register_cereg_get(uint8_t nSim);

/**
 * @brief Defines the enumeration type for bearer status and network events.
 *
 * @details This enumeration is used to describe event types related to bearer status, 
 *           network attachment status, and Out of Service (OOS) events.
 */
typedef enum
{
  LIOT_PS_EVENT_BEARER_ACTED = 0,   ///< Network registration successful
  LIOT_PS_EVENT_BEARER_DEACTED,     ///< Network registration failed
  LIOT_PS_EVENT_NETIF_OOS,          ///< Out of Service
  LIOT_PS_EVENT_NETIF_ACTIVATED,    ///< Network interface attachment successful
  LIOT_PS_EVENT_NETIF_DEACTIVATED,  ///< Network interface attachment failed
  LIOT_PS_EVENT_MAX                 ///< Maximum value of the enumeration
} Liot_PsEvent_e;

typedef void (*Liot_PsEventCallback_t)(Liot_PsEvent_e eventId, void *param, UINT32 paramLen);

/**
 * @brief Registers a system interface to notify the application layer of changes 
 *        in bearer status, OOS events, or registration status.
 *
 * @param callback A pointer to the callback function that will be invoked when the bearer status changes.
 *                 - eventId: Event type, see [Liot_PsEvent_e] enumeration.
 *                 - param: Parameters related to the event.
 *                 - paramLen: Length of the parameters.
 * @return liot_datacall_errcode_e Error code:
 *         - LIOT_DATACALL_SUCCESS: Success.
 *         - LIOT_DATACALL_INVALID_PARAM_ERR: Invalid parameter.
 *         - LIOT_DATACALL_EXECUTE_ERR: Execution failed.
 */
liot_datacall_errcode_e Liot_PsEventCb(Liot_PsEventCallback_t callback);


/**
 * @brief Defines the enumeration for IP types.
 */
typedef enum
{
    LIOT_PS_PDN_TYPE_IP_V4 = 1,  ///< IPv4 type
    LIOT_PS_PDN_TYPE_IP_V6,      ///< IPv6 type
    LIOT_PS_PDN_TYPE_IP_V4V6,    ///< Dual-stack IPv4 and IPv6 type
    LIOT_PS_PDN_TYPE_NUM         ///< Number of IP types
} Liot_DataCallIpType_e;


/**
 * @brief Defines the enumeration for APN operation methods.
 */
typedef enum
{
    LIOT_DATACALL_APN_SET,  ///< Set APN configuration
    LIOT_DATACALL_APN_GET,  ///< Query APN configuration
    LIOT_DATACALL_APN_MAX   ///< Maximum value for boundary checking
} Liot_DataCallMethod_e;


/**
 * @brief Defines the structure for APN and IP type configuration.
 *
 * @details This structure is used to set or query the default bearer's APN and IP type configuration.
 */
typedef struct
{
    Liot_DataCallMethod_e method;       ///< Operation method, either set or query configuration
    Liot_DataCallIpType_e ip_version;   ///< IP type
    char apn[LIOT_APN_LEN_MAX];         ///< APN string
    uint16_t apn_len;                   ///< Actual length of the APN string
} Liot_DataCallCFG_t;

/**
 * @brief Sets or queries the APN and IP type for the default bearer CID 1.
 * 
 * Main use cases include:
 * - When the device is overseas and AUTO APN is not enabled, 
 *   or when a specific SIM card has its own APN, setting a specific APN ensures successful 
 *   network registration.
 * - When AUTO APN is enabled, the platform strategy automatically matches the APN, 
 *   so manual settings are unnecessary.
 *
 * @param cfg A pointer to the configuration structure containing the following fields:
 *            - method: Operation method, see [Liot_DataCallMethod_e] enumeration:
 *                      - LIOT_DATACALL_APN_SET: Set APN.
 *                      - LIOT_DATACALL_APN_GET: Query APN.
 *            - ip_version: IP type, see [Liot_DataCallIpType_e] enumeration:
 *                         - LIOT_PS_PDN_TYPE_IP_V4: IPv4.
 *                         - LIOT_PS_PDN_TYPE_IP_V6: IPv6.
 *                         - LIOT_PS_PDN_TYPE_IP_V4V6: IPv4v6.
 *            - apn: APN string, with a maximum length of [LIOT_APN_LEN_MAX].
 *            - apn_len: Actual length of the APN string.
 * @return liot_datacall_errcode_e Error code:
 *         - LIOT_DATACALL_SUCCESS: Success.
 *         - LIOT_DATACALL_INVALID_PARAM_ERR: Invalid parameter.
 *         - LIOT_DATACALL_EXECUTE_ERR: Execution failed.
 */
liot_datacall_errcode_e Liot_DataCallCfgDefaultEpsBearer(Liot_DataCallCFG_t *cfg);
#ifdef __cplusplus
}
#endif
#endif