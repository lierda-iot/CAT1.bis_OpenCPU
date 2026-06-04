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

#ifdef __cplusplus
extern "C" {
#endif

#define LIOT_SOCKET_SUPPORT

#ifndef LIOT_SSL_SUPPORT
#define LIOT_SSL_SUPPORT 1
#endif
#ifndef LIOT_SOCKET_SUPPORT
#define LIOT_SOCKET_SUPPORT 1
#endif

#if defined(LIOT_SOCKET_SUPPORT) && defined(LIOT_SSL_SUPPORT)
#include "liot_os.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/net.h"
#include "mbedtls/ssl.h"
#include "mbedtls/ssl_internal.h"
#include "mbedtls/timing.h"

typedef int liot_ssl_session_t;
typedef void *liot_ssl_ctx;
typedef void *liot_ssl_config;

#define LIOT_SSL_CA_CERT_SIZE     6144  /*!< CA certificate size 6KB */
#define LIOT_SSL_FILE_PATH_LENGTH   128 /*!< Maximum certificate path length (128 bytes) */
#define LIOT_SSL_CLIENT_CERT_SIZE 4096 /*!< Client certificate size 4KB */
#define LIOT_SSL_RECV_BUF_SIZE    8192 /*!< SSL receive buffer size 8KB */
#define LIOT_SSL_HANDSHARK_MAX_TIMEOUT     (5 * 60) /*!< Maximum handshake or receive timeout (5 minutes) */


#define LIOT_SSL_CTX_ID_MIN  0     /*!< Minimum SSL context ID */
#define LIOT_SSL_CTX_ID_MAX  5     /*!< Maximum SSL context ID */
#define LIOT_MAX_SSL_CTX_NUM 6     /*!< Maximum number of SSL contexts */

#define LIOT_SSL_NEGOTIATE_TIME_MIN 10     /*!< Minimum negotiation time (10 seconds) */
#define LIOT_SSL_NEGOTIATE_TIME_MAX 300    /*!< Maximum negotiation time (300 seconds) */

#define LIOT_SSL_CA_CERT_MAX_CNT 20     /*!< Maximum number of CA certificates */

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
 * @brief SSL context configuration options
 * Enumeration of all available SSL configuration options
 */
typedef enum
{
    LIOT_SSL_CTX_SSL_VERSION                        = 1,  /*!< SSL/TLS version */
    LIOT_SSL_CTX_AUTH_TYPE                          = 2,  /*!< Authentication type */
    LIOT_SSL_CTX_CIPHERSUITE                        = 3,  /*!< Cipher suite */
    LIOT_SSL_CTX_HS_TIMEOUT                         = 4,  /*!< Handshake timeout */
    LIOT_SSL_CTX_ENABLE_SNI                         = 5,  /*!< Server Name Indication (SNI) */
    LIOT_SSL_CTX_TRANSPORT                          = 6,  /*!< Transport protocol (TLS/DTLS) */
    LIOT_SSL_CTX_CA_CHAIN                           = 7,  /*!< CA certificate chain */
    LIOT_SSL_CTX_USER_CERT                          = 8,  /*!< User certificate */
    LIOT_SSL_CTX_PSK_INFO                           = 9,  /*!< PSK information */
    LIOT_SSL_CTX_IGNORE_MULTI_CERT_CHAIN_VERIFY_OPT = 10, /*!< Ignore multi-cert chain verification */
    LIOT_SSL_CTX_IGNORE_INVALID_CERT_SIGN_OPT       = 11, /*!< Ignore invalid certificate signature */
    LIOT_SSL_CTX_IGNORE_CERT_ITEM_OPT               = 12, /*!< Ignore specific certificate items */
    LIOT_SSL_CTX_IGNORE_LOCALTIME_OPT               = 13, /*!< Ignore local time verification */
    LIOT_SSL_CTX_IGNORE_SESSION_CACHE_OPT           = 14, /*!< Ignore session cache */
    LIOT_SSL_CTX_IGNORE_CLOSE_TIME_MODE_OPT         = 15, /*!< Ignore close timing mode */
    LIOT_SSL_CTX_RENEGOTIATION_EXT_OPT              = 16, /*!< Renegotiation extension */
    LIOT_SSL_CTX_ALPN_EXT_OPT                       = 17, /*!< ALPN extension */
    LIOT_SSL_CTX_SESSION_MULTIPEX                   = 18, /*!< Session multiplexing */
    LIOT_SSL_CTX_CA_BUFFER_OPT                      = 19, /*!< CA certificate buffer option */
    LIOT_SSL_CTX_HOST                               = 20, /*!< Hostname */
} liot_ssl_ctx_option_e;

/**
 * @brief Client certificate type
 * Enumeration of client certificate storage types
 */
typedef enum
{
    LIOT_SSL_CLIENT_CERT_FILE   = 0, /*!< Certificate stored in file */
    LIOT_SSL_CLIENT_CERT_BUFFER = 1, /*!< Certificate stored in memory buffer */
} liot_ssl_client_cert_type_e;

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
} liot_ssl_error_code_e;

typedef struct
{
    char *ca_cert_list[LIOT_SSL_CA_CERT_MAX_CNT]; /*!< CA certificate list */
    int ca_depth; /*!< CA certificate depth */

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
 * @brief SSL session handshake timeout callback function
 * @param ssl_session SSL session pointer
 * @param arg Callback argument
 */
typedef void (*liot_ssl_handshake_timeout_cb)(liot_ssl_session_t *ssl_session, void *arg);

typedef struct
{
    int session_cache_enable;      /*!< SSL session cache enable */
    char server[LIOT_SSL_SESSION_CACHE_URL_MAX_LEN + 1]; /*!< Server hostname */
    uint16_t port;              /*!< Server port */
    mbedtls_ssl_session ssl_session;   /*!< SSL session */
} liot_ssl_session_cache_context_s;

typedef struct
{
    liot_mutex_t mutex;                      /*!< Mutex */
    unsigned char ssl_version;              /*!< SSL version */
    unsigned char auth_type;               /*!< Authentication type */
    int ciphersuite[2];                     /*!< Cipher suite */
    char *alpn_list[LIOT_SSL_CA_CERT_MAX_CNT];   /*!< ca cert list */
    char *ssl_recv_buf;              /*!< SSL receive buffer */
    unsigned short ssl_recv_len;              /*!< SSL receive buffer length */
    unsigned char sni_support;                /*!< SNI support */
    unsigned char cert_buffer_type;           /*!< Certificate buffer type */
    unsigned short hs_timeout;              /*!< Handshake timeout */
    liot_ssl_handshake_timeout_cb handshake_timeout_cb; /*!< Handshake timeout callback */
    void *handshake_timeout_cb_arg; /*!< Argument for timeout callback */
    unsigned char transport_protocol;  /*!< Transport protocol */
    char *ca_cert[LIOT_SSL_CA_CERT_MAX_CNT];    /*!< CA cert */
    unsigned short ca_cert_length[LIOT_SSL_CA_CERT_MAX_CNT];    /*!< CA cert length */
    unsigned char ca_cert_index;        /*!< CA cert index */
    char *user_cert;        /*!< User cert */
    unsigned short user_cert_length;        /*!< User cert length */
    char *user_key;                        /*!< User key */
    unsigned short user_key_length;        /*!< User key length */
    char *user_key_pwd;                    /*!< User key password */
    char *psk;                            /*!< Pre-shared key */
    unsigned short psk_length;            /*!< Pre-shared key length */
    char *psk_identity;                   /*!< Pre-shared key identity */
    unsigned short psk_identity_length;   /*!< Pre-shared key identity length */
    int ignore_cert_flag;                /*!< Ignore certificate verification */
    int ignore_certitem;                /*!< Ignore certificate item */
    unsigned char is_ignore_multicertchain_verify; /*!< Ignore multi-cert chain verification */
    int clase_time_mode;                /*!< Class time mode */
    liot_ssl_session_cache_context_s ssl_session_cache_ctx;   /*!< SSL session cache context */
} liot_ssl_context_s;

#ifdef LIOT_SSL_SUPPORT
/**
 * @brief SSL session structure
 * Contains all mbedTLS context and resources for an SSL session
 */
typedef struct
{
    int fd;                     /*!< Socket file descriptor */
    mbedtls_ssl_context mbedSslCtx; /*!< mbedTLS SSL context */
    mbedtls_ssl_config mbedSslConf; /*!< mbedTLS SSL configuration */
    mbedtls_ctr_drbg_context mbedDrbgCtx; /*!< mbedTLS random generator context */
    mbedtls_entropy_context mbedEtpyCtx; /*!< mbedTLS entropy context */
    mbedtls_x509_crt mbedX509Ca; /*!< mbedTLS CA certificate chain */
    mbedtls_pk_context mbedPkUser; /*!< mbedTLS user private key */
    mbedtls_x509_crt mbedX509User; /*!< mbedTLS user certificate */
    mbedtls_x509_crt_profile crtProfile; /*!< mbedTLS certificate profile */
    mbedtls_timing_delay_context timer; /*!< mbedTLS timing context */
    liot_timer_t dtls_retransmit_timer; /*!< DTLS retransmission timer */
    liot_timer_t handshark_timer; /*!< Handshake timer */
    liot_ssl_context_s *ssl_context; /*!< Pointer to SSL configuration (zfw) */
} liot_ssl_session_s, *ssl_session_ptr_s;
#endif

/**
 * @brief Initialize SSL configuration context.
 * This function initializes the SSL configuration context for establishing secure connections.
 * 
 * @param ssl_ctx Pointer to the SSL configuration context
 * @return int Returns LIOT_SSL_SUCCESS on success, or error code on failure
 */
int liot_ssl_conf_init(liot_ssl_config *ssl_ctx);

/**
 * @brief Set SSL configuration parameters.
 * This function sets various SSL configuration parameters based on the specified type.
 * 
 * @param ssl_ctx Pointer to the SSL configuration context
 * @param type Configuration parameter type (liot_ssl_ctx_option_e)
 * @param ... Variable arguments depending on the parameter type
 * @return int Returns LIOT_SSL_SUCCESS on success, or error code on failure
 */
int liot_ssl_conf_set(liot_ssl_config *ssl_ctx, int type, ...);

/**
 * @brief Get SSL configuration parameters.
 * This function retrieves various SSL configuration parameters based on the specified type.
 * 
 * @param ssl_ctx Pointer to the SSL configuration context
 * @param type Configuration parameter type (liot_ssl_ctx_option_e)
 * @param ... Variable arguments depending on the parameter type
 * @return int Returns LIOT_SSL_SUCCESS on success, or error code on failure
 */
int liot_ssl_conf_get(liot_ssl_config *ssl_ctx, int type, ...);

/**
 * @brief Create a new SSL session.
 * This function initializes a new SSL session for secure communication.
 * 
 * @param ssl Pointer to the SSL session handle
 * @return int Returns LIOT_SSL_SUCCESS on success, or error code on failure
 */
int liot_ssl_new(liot_ssl_session_t *ssl);

/**
 * @brief Set socket file descriptor for SSL session.
 * This function associates a socket file descriptor with an SSL session.
 * 
 * @param ssl Pointer to the SSL session handle
 * @param sock_fd Socket file descriptor
 * @return int Returns LIOT_SSL_SUCCESS on success, or error code on failure
 */
int liot_ssl_set_socket_fd(liot_ssl_session_t *ssl, int sock_fd);

/**
 * @brief Set hostname for SSL session.
 * This function sets the hostname for Server Name Indication (SNI) extension.
 * 
 * @param ssl Pointer to the SSL session handle
 * @param hostname Hostname string
 * @return int Returns LIOT_SSL_SUCCESS on success, or error code on failure
 */
int liot_ssl_set_hostname(liot_ssl_session_t *ssl, const char *hostname);

/**
 * @brief Setup SSL session with configuration.
 * This function configures an SSL session with the provided SSL context.
 * 
 * @param ssl Pointer to the SSL session handle
 * @param ssl_ctx Pointer to the SSL configuration context
 * @return int Returns LIOT_SSL_SUCCESS on success, or error code on failure
 */
int liot_ssl_setup(liot_ssl_session_t *ssl, liot_ssl_config *ssl_ctx);

/**
 * @brief Write data over SSL connection.
 * This function sends encrypted data over an established SSL connection.
 * 
 * @param ssl Pointer to the SSL session handle
 * @param buf Pointer to the data buffer to send
 * @param len Length of data to send
 * @return int Returns number of bytes sent on success, or error code on failure
 */
int liot_ssl_write(liot_ssl_session_t *ssl, const unsigned char *buf, size_t len);

/**
 * @brief Read data from SSL connection.
 * This function receives decrypted data from an established SSL connection.
 * 
 * @param ssl Pointer to the SSL session handle
 * @param buf Pointer to the buffer to store received data
 * @param len Maximum length of data to receive
 * @return int Returns number of bytes received on success, or error code on failure
 */
int liot_ssl_read(liot_ssl_session_t *ssl, unsigned char *buf, size_t len);

/**
 * @brief Perform SSL handshake.
 * This function performs the SSL/TLS handshake to establish a secure connection.
 * 
 * @param ssl Pointer to the SSL session handle
 * @return int Returns LIOT_SSL_SUCCESS on success, or error code on failure
 */
int liot_ssl_handshake(liot_ssl_session_t *ssl);

/**
 * @brief Send SSL close notification.
 * This function sends a proper SSL/TLS close notification to the peer.
 * 
 * @param ssl Pointer to the SSL session handle
 * @return int Returns LIOT_SSL_SUCCESS on success, or error code on failure
 */
int liot_ssl_close_notify(liot_ssl_session_t *ssl);

/**
 * @brief Free SSL configuration resources.
 * This function releases all resources associated with an SSL configuration context.
 * 
 * @param ssl_ctx Pointer to the SSL configuration context
 * @return int Returns LIOT_SSL_SUCCESS on success, or error code on failure
 */
int liot_ssl_conf_free(liot_ssl_config *ssl_ctx);

/**
 * @brief Free SSL session resources.
 * This function releases all resources associated with an SSL session.
 * 
 * @param ssl Pointer to the SSL session handle
 */
void liot_ssl_free(liot_ssl_session_t *ssl);

#endif

#ifdef __cplusplus
} /*"C" */

#endif

#endif
