/**
 * @File Name: liot_http.h
 * @brief
 * @Author : chenly email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-09-07
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */

#ifndef LIOT_HTTP_CLIENT_H
#define LIOT_HTTP_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "liot_api_common.h"
#include "liot_type.h"

typedef int liot_http_client_t;

/**
 * @brief HTTP client event callback function
 * @param[in] client  HTTP client handle
 * @param[in] evt     HTTP client event type
 * @param[in] evt_code HTTP client event code
 * @param[in] arg     HTTP client event argument
 * @return None
 */
typedef void (*liot_http_client_event_cb_t)(liot_http_client_t *client, int evt, int evt_code, void *arg);

/**
 * @brief HTTP client recv data callback function
 * @param[in] client  HTTP client handle
 * @param[in] arg     HTTP client event argument
 * @param[in] data    HTTP client recv data
 * @param[in] size    HTTP client recv data size
 * @param[in] end     HTTP client recv data end flag
 * @return None
 */
typedef int (*liot_http_client_write_data_cb_t)(
    liot_http_client_t *client, void *arg, char *data, int size, unsigned char end);

/**
 * @brief HTTP client send data callback function
 * @param[in] client  HTTP client handle
 * @param[in] arg     HTTP client event argument
 * @param[in] data    HTTP client send data
 * @param[in] size    HTTP client send data size
 * @return None
 */
typedef int (*liot_http_client_read_data_cb_t)(liot_http_client_t *client, void *arg, char *data, int size);


#define LIOT_HTTPC_READ   0x01      /*!< recv data*/
#define LIOT_HTTPC_WRITE  0x02      /*!<send data */ 
#define LIOT_MAXHOST_SIZE (512)     /*!< max host size*/
#define LIOT_MAXPATH_SIZE (512)     /*!< max path size*/

#define BASIC_AUTH_OPT_LENGTH    256                        /*!< basic auth opt length*/
#define HTTPSEND_TASK_STACK_SIZE (4096 * 2) // 2976         /*!< httpsend task stack size*/
#define LIOT_HTTPREQ_FILE_SIZE   (100 * 1024 +1) // 2976k   /*!< httpreq file size*/
#define LIOT_HTTPREQ_FILE_PATH_LENGTH   (255)               /*!< httpreq file path length*/

#define LIOT_HTTP_ERRCODE_BASE (LIOT_COMPONENT_LWIP_HTTP << 16)
typedef enum
{
    LIOT_HTTPC_SESSION_OPEN      = 1,                  /*!< HTTP session open event*/
    LIOT_HTTPC_UPLOAD_START      = 2,                   /*!< HTTP upload start event*/
    LIOT_HTTPC_UPLOAD_END        = 3,                   /*!< HTTP upload end event*/
    LIOT_HTTPC_RESPONSE_STATUS   = 4,                   /*!< HTTP response status event*/
    LIOT_HTTPC_RESPONSE_COMPLETE = 5,                   /*!< HTTP response complete event*/
    LIOT_HTTPC_RESPONSE_TIMEOUT  = 6,                   /*!< HTTP response timeout event*/
    LIOT_HTTPC_SESSION_CLOSE     = 7,                   /*!< HTTP session close event*/
    LIOT_HTTPC_RAW_REQ_START     = 8,                   /*!< HTTP raw request start event*/
    LIOT_HTTPC_RAW_REQ_END       = 9,                   /*!< HTTP raw request end event*/
} liot_httpc_event_type_e;

typedef enum
{
    LIOT_HTTP_EVENT_SESSION_ESTABLISH  = 0,
    LIOT_HTTP_EVENT_RESPONE_STATE_LINE = 1,
    LIOT_HTTP_EVENT_SESSION_DISCONNECT = 2,
} liot_httpc_event_id_e;

#define LIOT_HTTP_ERRCODE_BASE (LIOT_COMPONENT_LWIP_HTTP << 16)
typedef enum
{
    LIOT_HTTPC_SUCCESS            = LIOT_SUCCESS,                   /*!< HTTP success*/ 
    LIOT_HTTPC_ERR_INVALID_PARAM  = 1 | LIOT_HTTP_ERRCODE_BASE,     /*!< HTTP invalid param*/
    LIOT_HTTPC_ERR_UNKNOWN        = 2 | LIOT_HTTP_ERRCODE_BASE,     /*!< HTTP unknown error*/
    LIOT_HTTPC_ERR_OUT_OF_MEM     = 3 | LIOT_HTTP_ERRCODE_BASE,     /*!< HTTP out of memory*/
    LIOT_HTTPC_ERR_SOCKET_FAILURE = 4 | LIOT_HTTP_ERRCODE_BASE,     /*!< HTTP socket failure*/
    LIOT_HTTPC_ERR_NOT_SUPPORT    = 5 | LIOT_HTTP_ERRCODE_BASE,     /*!< HTTP not support*/
    LIOT_HTTPC_ERR_NOT_ALLOW      = 6 | LIOT_HTTP_ERRCODE_BASE,     /*!< HTTP not allow*/
    LIOT_HTTPC_ERR_NO_NETWORK     = 7 | LIOT_HTTP_ERRCODE_BASE,     /*!< HTTP no network*/
    LIOT_HTTPC_ERR_NO_SSL_CERT    = 8 | LIOT_HTTP_ERRCODE_BASE,     /*!< HTTP no ssl cert*/
    LIOT_HTTPC_ERR_TIMEOUT        = 9 | LIOT_HTTP_ERRCODE_BASE,     /*!< HTTP timeout*/ 
    LIOT_HTTPC_EMPTY_URL          = 10 | LIOT_HTTP_ERRCODE_BASE,    /*!< HTTP empty url*/
    LIOT_HTTPC_SSL_ERROR          = 11 | LIOT_HTTP_ERRCODE_BASE,    /*!< HTTP ssl error*/
} liot_httpc_result_code_e;

typedef enum
{
    LIOT_HTTPC_SCHEME_INVALID = 0,  /*!< HTTP invalid scheme*/
    LIOT_HTTPC_SCHEME_HTTP    = 1,  /*!< HTTP http scheme*/
    LIOT_HTTPC_SCHEME_HTTPS   = 2   /*!< HTTP https scheme*/
} liot_httpc_scheme_e;

typedef enum
{
    LIOT_HTTPC_METHOD_NONE   = 0,  /*!< HTTP none method*/
    LIOT_HTTPC_METHOD_GET    = 1,  /*!< HTTP get method*/
    LIOT_HTTPC_METHOD_POST   = 2,  /*!< HTTP post method*/
    LIOT_HTTPC_METHOD_PUT    = 3,  /*!< HTTP put method*/
    LIOT_HTTPC_METHOD_HEAD   = 4,  /*!< HTTP head method*/
    LIOT_HTTPC_METHOD_LAST   = 5,  /*!< HTTP last method*/
    LIOT_HTTPC_METHOD_DELETE = 6,  /*!< HTTP delete method*/
} liot_httpc_method_e;

typedef enum
{
    LIOT_HTTP_CLIENT_OPT_PDPCID = 1,            /*!< HTTP client option pdpcid*/
    LIOT_HTTP_CLIENT_OPT_BASIC_AUTH,            /*!< HTTP client option basic auth*/
    LIOT_HTTP_CLIENT_OPT_REQUEST_HEADER,        /*!< HTTP client option request header*/
    //LIOT_HTTP_CLIENT_OPT_REQUEST_BODY,
    LIOT_HTTP_CLIENT_OPT_WRITE_HEADER,          /*!< HTTP client option write header*/
    LIOT_HTTP_CLIENT_OPT_INTERVAL_TIME,         /*!< HTTP client option interval time*/
    LIOT_HTTP_CLIENT_OPT_METHOD,                /*!< HTTP client option method*/ 
    LIOT_HTTP_CLIENT_OPT_WRITE_FUNC,            /*!< HTTP client option recv function*/
    LIOT_HTTP_CLIENT_OPT_WRITE_DATA,            /*!< HTTP client option recv data*/
    LIOT_HTTP_CLIENT_OPT_READ_FUNC,             /*!< HTTP client option send function*/
    LIOT_HTTP_CLIENT_OPT_READ_DATA,             /*!< HTTP client option send data*/
    LIOT_HTTP_CLIENT_OPT_UPLOAD_LEN,            /*!< HTTP client option upload len*/    
    LIOT_HTTP_CLIENT_OPT_URL,                   /*!< HTTP client option url*/
    LIOT_HTTP_CLIENT_OPT_URI,                   /*!< HTTP client option uri*/
    LIOT_HTTP_CLIENT_OPT_SIM_ID,                /*!< HTTP client option sim id*/
    LIOT_HTTP_CLIENT_OPT_RAW_REQUEST,           /*!< HTTP client option raw request*/
    LIOT_HTTP_CLIENT_OPT_RAW_FILE,              /*!< HTTP client option raw file*/    
    LIOT_HTTP_CLIENT_OPT_AUTH_INFO,             /*!< HTTP client option auth info*/
    LIOT_HTTP_CLIENT_OPT_RESPONSE_TIME,         /*!< HTTP client option response time*/
    LIOT_HTTP_CLIENT_OPT_BODY_DATA_TYPE,        /*!< HTTP client option body data type*/
    LIOT_HTTP_CLIENT_OPT_FORM_OPTION,           /*!< HTTP client option form option*/
    LIOT_HTTP_CLIENT_OPT_CUSTOME_HEADER,        /*!< HTTP client option custom header*/
    LIOT_HTTP_CLIENT_OPT_SEND_TIMEOUT,          /*!< HTTP client option send timeout*/
    LIOT_HTTP_CLIENT_OPT_RECV_TIMEOUT,          /*!< HTTP client option recv timeout*/
#ifdef FEATURE_HTTP_TLS_ENABLE
    LIOT_HTTP_CLIENT_OPT_SSLCTXID,              /*!< HTTP client option ssl ctx id*/
    LIOT_HTTP_CLIENT_OPT_SSL_VERIFY_LEVEL,      /*!< HTTP client option ssl verify level*/
    LIOT_HTTP_CLIENT_OPT_SSL_CACERT_PATH,       /*!< HTTP client option ssl ca cert path*/
    LIOT_HTTP_CLIENT_OPT_SSL_CACERT_DATA,       /*!< HTTP client option ssl ca cert data*/
    LIOT_HTTP_CLIENT_OPT_SSL_OWNCERT_PATH,      /*/<! HTTP client option ssl own cert path*/
    LIOT_HTTP_CLIENT_OPT_SSL_OWNCERT_DATA,      /*/<! HTTP client option ssl own cert data*/
    LIOT_HTTP_CLIENT_OPT_SSL_OWNKEY_PATH,       /*!< HTTP client option ssl own key path*/
    LIOT_HTTP_CLIENT_OPT_SSL_OWNKEY_DATA,       /*/<! HTTP client option ssl own key data*/
    LIOT_HTTP_CLIENT_OPT_SSL_SNI,               /*!< HTTP client option ssl sni*/  
    LIOT_HTTP_CLIENT_OPT_SSL_VERSION,           /*!< HTTP client option ssl version*/
    LIOT_HTTP_CLIENT_OPT_SSL_HS_TIMEOUT,        /*!< HTTP client option ssl recv timeout*/
    LIOT_HTTP_CLIENT_OPT_SSL_IGNORE_LOCALTM,    /*!< HTTP client option ssl local time (not support)*/
    LIOT_HTTP_CLIENT_OPT_SSL_IGNORE_INVALID_CERT_SIGN,  /*!< HTTP client option ssl ignore cert sign*/
    LIOT_HTTP_CLIENT_OPT_SSL_IGNORE_CERT_ITEM,          /*!< HTTP client option ssl ignore cert time*/ 
    LIOT_HTTP_CLIENT_OPT_SSL_IGNORE_MULTI_CERTCHAIN_VERIFY, /*!< HTTP client option ssl ignore certchain verify*/
#endif
} liot_httpc_option_e;

typedef enum
{
    LIOT_HTTPC_STATUS_CODE    = 1,           /*!< HTTP status code*/
    LIOT_HTTPC_CHUNK_ENCODE   = 2,           /*!< HTTP chunk encode*/
    LIOT_HTTPC_CONTENT_LEN    = 3,           /*!< HTTP content length*/
    LIOT_HTTPC_CONTENT_RANGE  = 4,           /*!< HTTP content range*/
    LIOT_HTTPC_DATE           = 5,           /*!< HTTP date*/  
    LIOT_HTTPC_LOCATION       = 6,           /*!< HTTP location*/
    LIOT_HTTPC_NREAD_DATA_LEN = 7,           /*!< HTTP read data length*/
} liot_httpc_info_type_e;

typedef enum
{
    LIOT_HTTPC_RAW_DATA  = 0,
    LIOT_HTTPC_FORM_DATA = 1,
} liot_httpc_upload_data_type_e;

typedef enum
{
    LIOT_HTTPS_VERIFY_NONE          = 0,               /*!< HTTP ssl verify none*/
    LIOT_HTTPS_VERIFY_SERVER        = 1,               /*!< HTTP ssl verify server*/
    LIOT_HTTPS_VERIFY_SERVER_CLIENT = 2,               /*!< HTTP ssl verify server and client*/
} liot_https_verify_level_e;

typedef enum
{
    LIOT_HTTP_FORM_NAME         = 1,               /*!< HTTP form name*/    
    LIOT_HTTP_FORM_FILENAME     = 2,               /*!< HTTP form filename*/
    LIOT_HTTP_FORM_CONTENT_TYPE = 3,               /*!< HTTP form content type*/
} liot_httpc_formopt_e;

// typedef enum
// {
//     LIOT_APP_HTTP_OK                     = 0,
//     LIOT_APP_HTTP_UNKNOWN_ERR            = -1,
//     LIOT_APP_HTTP_TIMEOUT                = -2,
//     LIOT_APP_HTTP_BUSY                   = -3,
//     LIOT_APP_HTTP_SIO_BUSY               = -4,
//     LIOT_APP_HTTP_NO_REQ                 = -5,
//     LIOT_APP_HTTP_NETWORK_BUSY           = -6,
//     LIOT_APP_HTTP_PDP_ACTIVE_FAIL        = -7,
//     LIOT_APP_HTTP_PDP_PROFILE_NOT_EXIST  = -8,
//     LIOT_APP_HTTP_PDP_DEACTIVED          = -9,
//     LIOT_APP_HTTP_PDP_ERR                = -10,
//     LIOT_APP_HTTP_URL_ERR                = -11,
//     LIOT_APP_HTTP_EMPTY_URL              = -12,
//     LIOT_APP_HTTP_IP_ERR                 = -13,
//     LIOT_APP_HTTP_DNS_ERR                = -14,
//     LIOT_APP_HTTP_SOCKET_ALLOC_ERR       = -15,
//     LIOT_APP_HTTP_SOCKET_CONNECT_ERR     = -16,
//     LIOT_APP_HTTP_SOCKET_READ_ERR        = -17,
//     LIOT_APP_HTTP_SOCKET_WRITE_ERR       = -18,
//     LIOT_APP_HTTP_SOCKET_ABNORMAL_CLOSED = -19,
//     LIOT_APP_HTTP_DATA_ENCODE_ERR        = -20,
//     LIOT_APP_HTTP_DATA_DECODE_ERR        = -21,
//     LIOT_APP_HTTP_READ_TIMEOUT           = -22,
//     LIOT_APP_HTTP_RESPONSE_FAIL          = -23,
//     LIOT_APP_HTTP_INCOM_CALL_BUSY        = -24,
//     LIOT_APP_HTTP_VOICE_CALL_BUSY        = -25,
//     LIOT_APP_HTTP_INPUT_TIMEOUT          = -26,
//     LIOT_APP_HTTP_WAIT_DATA_TIMEOUT      = -27,
//     LIOT_APP_HTTP_RSP_TIMEOUT            = -28,
//     LIOT_APP_HTTP_OUT_OF_MEM             = -29,
//     LIOT_APP_HTTP_INVALID_PARAM          = -30,
//     LIOT_APP_HTTP_NOT_SUPPORT_PARAM      = -31,
//     LIOT_HTTPC_SSL_ERROR              = -32,
// } liot_atec_http_result_error_code_e;

typedef struct
{
    liot_httpc_scheme_e scheme;             /*!< HTTP client option scheme*/
    char *host;                             /*!< HTTP client option host*/
    unsigned short port;                    /*!< HTTP client option port*/
    char *uri;                              /*!< HTTP client option uri*/
} liot_httpc_url_s;

typedef struct liot_httpc_form_option_t
{
    struct liot_httpc_form_option_t *next;  /*!< next form option*/
    char *name;                             /*!< form name*/
    char *filename;                         /*!< form filename*/
    int content_length;                     /*!< form content length*/
    char *content_type;                     /*!< form content type*/
} liot_httpc_form_option_l;

/** 
 * @brief Create a new HTTP client session
 * @param[out] client Pointer to the HTTP client handle to be created
 * @param[in] cb Pointer to the HTTP client event callback function
 * @param[in] arg Pointer to the argument to be passed to the callback function
 * @return 0 on success, negative error code on failure
 * @note The client handle must be released by liot_httpc_release() when it is no longer needed 
*/
int liot_httpc_new(liot_http_client_t *client, liot_http_client_event_cb_t cb, void *arg);

/** 
 * @brief Perform an HTTP request
 * @param[in] client Pointer to the HTTP client handle
 * @return 0 on success, negative error code on failure
 * @note The client handle must be created by liot_httpc_new() before calling this function
*/
int liot_httpc_perform(liot_http_client_t *client);

/**
 * @brief Stop an ongoing HTTP request
 *  @param[in] http_hndl Pointer to the HTTP client handle
 * @return 0 on success, negative error code on failure
 * @note The client handle must be created by liot_httpc_new() before calling this function
 */
int liot_httpc_stop(liot_http_client_t *http_hndl);

/**
 * @brief  Release an HTTP client session
 * @param[in] client Pointer to the HTTP client handle
 * @return 0 on success, negative error code on failure
 * @note The client handle must be created by liot_httpc_new() before calling this function
 */
int liot_httpc_release(liot_http_client_t *client);

/**
 * @brief Set an HTTP client option
 * @param[in] client Pointer to the HTTP client handle
 * @param[in] opt_tag Option tag
 * @param[in] ... Option value
 * @return 0 on success, negative error code on failure
 * @note The client handle must be created by liot_httpc_new() before calling this function
 */
int liot_httpc_setopt(liot_http_client_t *client, int opt_tag, ...);

/**
 * @brief Get an HTTP client recv option info
 * @param[in] client Pointer to the HTTP client handle
 * @param[in] info Option tag
 * @param[in] ... Option value
 * @return 0 on success, negative error code on failure
 * @note The client handle must be created by liot_httpc_new() before calling this function
 */

int liot_httpc_getinfo(liot_http_client_t *client, int info, ...);

/**
 * @brief Continue a download from a previous HTTP request
 * @param[in] client Pointer to the HTTP client handle
 * @return 0 on success, negative error code on failure
 * @note The client handle must be created by liot_httpc_new() before calling this function
 */
int liot_httpc_continue_dload(liot_http_client_t *client);

/**
 * @brief Add a form option to an HTTP client session
 * @param[in] client Pointer to the HTTP client handle
 * @param[in] opt_tag Option tag
 * @param[in] ... Option value
 * @return 0 on success, negative error code on failure
 * @note The client handle must be created by liot_httpc_new() before calling this function
 */
int liot_httpc_formadd(liot_http_client_t *client, int opt_tag, ...);

/**
 * @brief User notify an HTTP client session
 * @param[in] http_hndl Pointer to the HTTP client handle
 * @param[in] type User notify type
 * @return 0 on success, negative error code on failure
 * @note The client handle must be created by liot_httpc_new() before calling this function
 */
int liot_httpc_user_notify(liot_http_client_t *http_hndl, unsigned char type);

/**
 * @brief   Check if an HTTP client session is running
 * @param[in] http_hndl Pointer to the HTTP client handle
 * @return true if the session is running, false otherwise
 */
bool liot_httpc_is_running(liot_http_client_t *http_hndl);

/**
 * @brief   Parse an HTTP URL string
 * @param[in] url_str Pointer to the HTTP URL string
 * @param[out] url Pointer to the HTTP URL structure
 * @return true if the URL is parsed successfully, false otherwise
 */
bool liot_httpc_url_parse(char *url_str, liot_httpc_url_s *url);

#ifdef __cplusplus
} /*"C" */
#endif

#endif
