/**
 * @File Name: liot_ssl.h
 * @brief
 * @Author : chenly email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-09-07
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */
#ifndef LIOT_SSL_CTX_H
#define LIOT_SSL_CTX_H
#include "liot_type.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef int liot_ssl_session_t;
typedef void *liot_ssl_client_t;
typedef void *liot_ssl_config;


#define LIOT_SSL_CTX_ID_MIN  0     /*!< Minimum SSL context ID */
#define LIOT_SSL_CTX_ID_MAX  5     /*!< Maximum SSL context ID */
#define LIOT_MAX_SSL_CTX_NUM 6     /*!< Maximum number of SSL contexts */

#define LIOT_SSL_NEGOTIATE_TIME_MIN 10     /*!< Minimum negotiation time (10 seconds) */
#define LIOT_SSL_NEGOTIATE_TIME_MAX 300    /*!< Maximum negotiation time (300 seconds) */

#define LIOT_SSL_CA_CERT_MAX_CNT 20     /*!< Maximum number of CA certificates */
#define LIOT_SSL_ALPN_NAME_LEN 64

#define LIOT_SSL_VERSION_0   0               /*!< SSL version v3.0 */
#define LIOT_SSL_VERSION_1   1               /*!< SSL version v1.0 */
#define LIOT_SSL_VERSION_2   2               /*!< SSL version v1.1 */
#define LIOT_SSL_VERSION_3   3               /*!< SSL version v1.2 */
#define LIOT_SSL_VERSION_ALL 4

#define LIOT_SSL_VERIFY_NONE          0      /*!< No verification */
#define LIOT_SSL_VERIFY_SERVER        1      /*!< Verify server certificate */
#define LIOT_SSL_VERIFY_CLIENT_SERVER 2      /*!< Verify client and server certificate */

#define LIOT_SSL_TLS_PROTOCOL  0        /*!< TLS protocol */
#define LIOT_SSL_DTLS_PROTOCOL 1        /*!< DTLS protocol */


#define LIOT_SSL_MULTI_CERT_CHAIN_VERIFY_NOT_IGNORE 0  /*!< Do not ignore multi-cert chain verification */
#define LIOT_SSL_MULTI_CERT_CHAIN_VERIFY_IGNORE     1  /*!< Ignore multi-cert chain verification */ 

#define LIOT_SSL_INVALID_CERT_SIGN_NOT_IGNORE 0        /*!< Do not ignore invalid certificate signature */
#define LIOT_SSL_INVALID_CERT_SIGN_IGNORE     1        /*!< Ignore invalid certificate signature */

#define LIOT_SSL_INVALID_CERT_SIGN_NOT_IGNORE 0        /*!< Do not ignore invalid certificate signature */
#define LIOT_SSL_INVALID_CERT_SIGN_IGNORE     1        /*!< Ignore invalid certificate signature */ 

#define LIOT_SSL_SESSION_CACHE_DISABLE 0        /*!< Disable session cache */
#define LIOT_SSL_SESSION_CACHE_ENABLE  1        /*!< Enable session cache */

#define LIOT_SSL_CLOSE_TIME_MODE_DISABLE 0        /*!< Disable close timing mode */ 
#define LIOT_SSL_CLOSE_TIME_MODE_ENABLE  1        /*!< Enable close timing mode */

#define LIOT_SSL_SESSION_CACHE_MAX_LEN     800     /*!< Maximum number of cached sessions */    
#define LIOT_SSL_SESSION_CACHE_URL_MAX_LEN 255    /*!< Maximum length of URL */

/* Number of certificate checking items */
#define LIOT_SSL_TLS_CERT_ITEM_NUM 20

#define LIOT_SSL_SESSION_MULTIPLEX_WITH_ADDR      0x01    /*!< Session multiplexing with address */
#define LIOT_SSL_SESSION_MULTIPLEX_WITH_ADDR_PORT 0x02    /*!< Session multiplexing with address and port */
#define LIOT_SSL_LWIP_PS_INVALID_CID              (0xFF)    /*!< Invalid CID */


/**
 * @brief SSL error codes
 * Enumeration of all possible SSL error codes
 */
typedef enum
{
    LIOT_SSL_SUCCESS               = 0,  /*!< Operation successful */
    LIOT_SSL_ERROR_UNKOWN          = -1, /*!< Unknown error */
    LIOT_SSL_ERROR_WOUNDBLOCK      = -2, /*!< Would block error */
    LIOT_SSL_ERROR_INVALID_PARAM   = -3, /*!< Invalid parameter */
    LIOT_SSL_ERROR_OUT_OF_MEM      = -4, /*!< Out of memory */
    LIOT_SSL_ERROR_NOT_SUPPORT     = -5, /*!< Feature not supported */
    LIOT_SSL_ERROR_HS_FAILURE      = -6, /*!< Handshake failure */
    LIOT_SSL_ERROR_DECRYPT_FAILURE = -7, /*!< Decryption failure */
    LIOT_SSL_ERROR_ENCRYPT_FAILURE = -8, /*!< Encryption failure */
    LIOT_SSL_ERROR_HS_INPROGRESS   = -9, /*!< Handshake in progress */
    LIOT_SSL_ERROR_BAD_REQUEST     = -10, /*!< Bad request */
    LIOT_SSL_ERROR_WANT_READ       = -11, /*!< Need to read more data */
    LIOT_SSL_ERROR_WANT_WRITE      = -12, /*!< Need to write more data */
    LIOT_SSL_ERROR_SOCKET_RESET    = -13, /*!< Socket reset */
    LIOT_SSL_ERROR_TIMEOUT         = -14, /*!< Operation timeout */
    LIOT_SSL_ERROR_SOCKET_CLOSED   = -15, /*!< Socket closed */
    LIOT_SSL_ERROR_SOCKET_ERROR    = -16, /*!< Socket error */
    LIOT_SSL_ERROR_INVALID_CERT    = -17, /*!< Invalid certificate */
    LIOT_SSL_ERROR_VERIFY_FAILED   = -18, /*!< Verification failed */
    LIOT_SSL_ERROR_SEND_FAILURE    = -19, /*!< Send data failure */
    LIOT_SSL_ERROR_STATUS_ERROR    = -20, /*!< Invalid status for operation */
} liot_ssl_error_code_e;


typedef struct
{
    char *ca_cert_list[LIOT_SSL_CA_CERT_MAX_CNT];   /*!< CA certificate list */
    int ca_depth;                                   /*!< CA certificate depth */

} liot_ssl_ca_chain_s;

typedef struct
{
    char *cert_file;     /*!< Certificate file */
    char *key_file;      /*!< Private key file */
    char *key_password;  /*!< Private key password */
} liot_ssl_user_cert_s;

typedef struct
{
    unsigned short psk_len;     /*!< PSK length */
    char *psk;                  /*!< Pre-shared key */
    unsigned short psk_identity_len; /*!< PSK identity length */
    char *psk_identity;        /*!< PSK identity */
} liot_ssl_psk_info_s;


/**
 * @brief SSL client receive callback function type
 * SSL client receive callback function
 * @param clientId SSL client ID
 * @param data Received data
 * @param dataLen Received data length
 * @param arg User argument
 * @return None
 */
typedef void (*liot_sslClient_recvCb_t)(uint8_t clientId,  void* data, uint32_t dataLen, void *arg);

typedef struct
{
    uint8_t ssl_version;                /*!< SSL version */
    uint8_t auth_type;                  /*!< Authentication type */
    int32_t ciphersuite[2];               /*!< Ciphersuite */
    uint8_t *alpn_list[LIOT_SSL_ALPN_NAME_LEN];  /*!< ALPN list */
    uint8_t sni_support;                 /*!< SNI support */
    uint8_t cert_buffer_type;           /*!< Certificate buffer type */
    uint32_t r_timeout;                 /*!< Receive timeout */
    uint32_t s_timeout;                  /*!< Send timeout */
    uint8_t transport_protocol;      /*!< Transport protocol */
    uint8_t *ca_cert;                   /*!< CA certificate */  
    uint32_t ca_cert_length;            /*!< CA certificate length */
    uint8_t ca_cert_index;              /*!< CA certificate index */
    uint8_t *user_cert;                 /*!< User certificate */
    uint32_t user_cert_length;          /*!< User certificate length */
    uint8_t *user_key;                  /*!< User key */
    uint32_t user_key_length;           /*!< User key length */
    uint8_t *user_key_pwd;              /*!< User key password */
    uint8_t *psk;                       /*!< Pre-shared key */
    uint32_t psk_length;                /*!< Pre-shared key length */
    uint8_t *psk_identity;              /*!< Pre-shared key identity */
    uint32_t psk_identity_length;       /*!< Pre-shared key identity length */ 
    uint32_t ignore_cert_flag;          /*!< Ignore certificate flag */
    uint32_t ignore_certitem;           /*!< Ignore certificate item */ 
    uint8_t is_ignore_multicertchain_verify;    /*!< Is ignore multi-certificate chain verify */
    uint8_t cache_enable;               /*!< SSL cache enable */  //not support  
}Liot_sslContext_t;

typedef struct
{
    uint8_t sslClientId;        /*!<SSL client ID */
    uint8_t cid;                /*!<CID */
    uint8_t socket_type;         /*!<Socket type */
    uint8_t* host;          /*!<Host */
    uint16_t port;          /*!<remote Port */
    uint16_t local_port;     /*!<Local Port */
    uint8_t KeepaliveEn;     /*!<Keepalive Enable */
    uint8_t keepidle;       /*!<Keepalive Idle Time */
    uint8_t keepinterval;   /*!<Keepalive Interval */
    uint8_t keepcount;      /*!<Keepalive Count */
    Liot_sslContext_t *ssl_context;     /*!<SSL context */
    liot_sslClient_recvCb_t sslClient_cb;   /*!<SSL client receive callback function */
}Liot_sslClientInfo_t;


/**
 * @brief SSL client status
 * SSL client status
 */
typedef enum
{
    LIOT_SSL_CLIENT_STATUS_INIT = 0,        /*!< SSL client status init */
    LIOT_SSL_CLIENT_STATUS_CONNECTING,      /*!< SSL client status connecting */
    LIOT_SSL_CLIENT_STATUS_CONNECTED,       /*!< SSL client status connected */
    LIOT_SSL_CLIENT_STATUS_DISCONNECTING,   /*!< SSL client status disconnecting */
    LIOT_SSL_CLIENT_STATUS_DISCONNECTED,    /*!< SSL client status disconnected */
    LIOT_SSL_CLIENT_STATUS_CLOSED,          /*!< SSL client status closed */    
}liot_ssl_client_status_e;


/**
 * @brief SSL client configuration
 * SSL client configuration
 * @param ssl_cfg SSL client configuration
 * @return 0 if successful, otherwise error code
 */
int32_t Liot_SSLSetCfg(void* ssl_cfg);

/**
 * @brief SSL client open
 * SSL client open
 * @param sslCLientID SSL client ID
 * @return 0 if successful, otherwise error code
 */
int32_t Liot_SSLSocketOpen(int32_t sslClientID);

/**
 * @brief SSL client send
 * SSL client send
 * @param sslCLientID SSL client ID
 * @param data Data to be sent
 * @param len Length of data to be sent
 * @return 0 if successful, otherwise error code
 */
int32_t Liot_SSLSocketSend(int32_t sslClientID, uint8_t *data, uint16_t len);

/**
 * @brief SSL client get status
 * SSL client get status
 * @param sslCLientID SSL client ID
 * @return SSL client status
 */
int32_t Liot_SSLSocketGetStatus(int32_t sslClientID);


/**
 * @brief SSL client close
 * SSL client close
 * @param sslCLientID SSL client ID
 * @return 0 if successful, otherwise error code
 */
int32_t Liot_SSLSocketClose(int32_t sslClientID);

#ifdef __cplusplus
} /*"C" */

#endif

#endif