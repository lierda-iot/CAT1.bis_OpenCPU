/**
 * @File Name: liot_lbs_client.h
 * @brief
 * @Author : Lxh email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-09-07
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */
#ifndef LIOT_LBS_CLIENT_H
#define LIOT_LBS_CLIENT_H

#include "liot_api_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int liot_lbs_client_hndl;

#define LBS_MAX_CELL_NUM 7   
#define LBS_MAX_POS_NUM  6
#define LBS_MAX_WIFI_NUM 6

typedef struct
{
    uint8_t type;           /*!<loction type 1:LBS,0:WIFI*/
    uint8_t encrypt;        /*!<loction encrypt type*/
    uint8_t key_index;      /*!<loction key index*/
    uint8_t pos_format;     /*!<loction format*/
    uint8_t loc_method;     /*!<loction method*/
} liot_lbs_basic_info_t;

typedef struct
{
    char user_name[64];     /*!<user name*/
    char user_pwd[64];      /*!<user password*/
    char token[128];        /*!<LBS token*/
    char imei[64];          /*!<device imei*/
    uint16_t rand;          /*!< random number*/
} liot_lbs_auth_info_t;

typedef struct
{
    uint8_t radio;           /*!<radio type*/
    uint16_t mcc;            /*!<mobile country code*/
    uint16_t mnc;            /*!<mobile network code*/
    int lac_id;              /*!<location area code*/
    int cell_id;             /*!<cell id*/
    int16_t signal;          /*!<signal strength*/
    uint16_t tac;            /*!<tracking area code*/
    uint16_t bcch;           /*!<base channel*/
    uint8_t bsic;            /*!<base station identity code*/
    uint16_t uarfcndl;       /*!<uplink absolute radio frequency channel number*/
    uint16_t psc;            /*!<physical cell id*/
    uint16_t rsrq;           /*!<reference signal received quality*/
    uint16_t pci;            /*!<physical cell id*/
    uint16_t earfcn;         /*!<uplink absolute radio frequency channel number*/
    uint16_t reserve;        /*!<reserve*/
} liot_lbs_cell_info_t;

typedef struct
{
    char wifi_mac[18];             /*!<wifi mac*/
    int wifi_rssi;                 /*!<wifi rssi*/
    char wifi_ssid[32];            /*!<wifi ssid*/
} liot_lbs_wifi_mac_info_t;

typedef struct
{
    char longitude[32];             /*!<longitude information*/
    char latitude[32];              /*!<latitude information*/
    char desc[1024];                /*!<Chinese location*/
    uint16_t accuracy;             /*!<accuracy */
    uint8_t flag;                  /*!<flag */
} liot_lbs_postion_info_t;

typedef struct
{
    int pdp_cid;                                /*!<pdp context id*/
    int sim_id;                                 /*!<sim id*/
    int req_timeout;                            /*!<request timeout*/
    liot_lbs_basic_info_t *basic_info;          /*!<lbs basic information*/
    liot_lbs_auth_info_t *auth_info;            /*!<lbs authentication information*/
    int cell_num;                               /*!<cell number*/
    liot_lbs_cell_info_t *cell_info;            /*!<cell information*/
    int wifi_num;                               /*!<wifi number*/
    liot_lbs_wifi_mac_info_t *wifi_info;        /*!<wifi information*/
} liot_lbs_option_t;

typedef enum
{
    LIOT_LBS_OK                          = 0,
    LIOT_LBS_LOC_FAIL                    = (LIOT_COMPONENT_LWIP_LBS << 16) | 10000,
    LIOT_LBS_IMEI_ILLEGAL                = (LIOT_COMPONENT_LWIP_LBS << 16) | 10001,
    LIOT_LBS_TOKEN_NOT_EXIST             = (LIOT_COMPONENT_LWIP_LBS << 16) | 10002,
    LIOT_LBS_TOKEN_LOC_EXCEED_MAX        = (LIOT_COMPONENT_LWIP_LBS << 16) | 10003,
    LIOT_LBS_IMEI_LOC_EXCEED_DAY_MAX     = (LIOT_COMPONENT_LWIP_LBS << 16) | 10004,
    LIOT_LBS_IMEI_LOC_VISIT_EXCEED_MAX   = (LIOT_COMPONENT_LWIP_LBS << 16) | 10005,
    LIOT_LBS_TOKEN_EXPIRED               = (LIOT_COMPONENT_LWIP_LBS << 16) | 10006,
    LIOT_LBS_IMEI_NO_AUTHORITY           = (LIOT_COMPONENT_LWIP_LBS << 16) | 10007,
    LIOT_LBS_TOKEN_LOC_VISIT_EXCEED_MAX  = (LIOT_COMPONENT_LWIP_LBS << 16) | 10008,
    LIOT_LBS_TOKEN_LOC_EXCEED_PERIOD_MAX = (LIOT_COMPONENT_LWIP_LBS << 16) | 10009,
    LIOT_LBS_DNS_FAIL                    = (LIOT_COMPONENT_LWIP_LBS << 16) | 10101,
    LIOT_LBS_MD5_FAIL                    = (LIOT_COMPONENT_LWIP_LBS << 16) | 10102,
    LIOT_LBS_MEMORY_FAIL                 = (LIOT_COMPONENT_LWIP_LBS << 16) | 10103,
    LIOT_LBS_NET_FAIL                    = (LIOT_COMPONENT_LWIP_LBS << 16) | 10104,
    LIOT_LBS_PARAM_FORMAT_FAIL           = (LIOT_COMPONENT_LWIP_LBS << 16) | 10105,
} liot_lbs_result_code_e;

typedef struct
{
    liot_lbs_client_hndl hndl;              /*!< lbs client handle */
    liot_lbs_result_code_e result;          /*!< lbs result code */
    int pos_num;                            /*!< lbs position number */
    liot_lbs_postion_info_t *pos_info;      /*!< lbs position information */
    char *date;                             /*!< lbs date */
    void *arg;                              /*!< lbs argument */
} liot_lbs_response_data_t;

/**
 * @brief Location service response callback function
 * @param[in] response_data response data
 * @return liot_lbs_result_code_e result code
 */
typedef void (*liot_lbs_response_callback)(liot_lbs_response_data_t *response_data);


/**
 * @brief Initialize lbs client
 * @param[in] lbs_hndl lbs client handle
 * @param[in] host lbs server host
 * @param[in] user_opts lbs user options
 * @param[in] cb lbs response callback function
 * @param[in] arg lbs argument
 * @return liot_lbs_result_code_e result code
 */
liot_lbs_result_code_e liot_lbs_get_position(
    liot_lbs_client_hndl *lbs_hndl, char *host, liot_lbs_option_t *user_opts, liot_lbs_response_callback cb, void *arg);

#ifdef __cplusplus
} /*"C" */
#endif

#endif
