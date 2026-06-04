/*
 *the mbedtls configuration file of  libcoap
 */

#ifndef MBEDTLS_CONFIG_COMM_H
#define MBEDTLS_CONFIG_COMM_H

/* OS */
#define MBEDTLS_OS_FREERTOS

/*TCPIP STACK*/
#define MBEDTLS_TCPIP_LWIP

/* System support */
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_PLATFORM_MEMORY
#if defined(MBEDTLS_OS_FREERTOS)
#define MBEDTLS_PLATFORM_CALLOC_MACRO callocEc //mbedtls_calloc //
#define MBEDTLS_PLATFORM_FREE_MACRO   freeEc //mbedtls_free //
#endif

#define MBEDTLS_AES_ROM_TABLES

#define MBEDTLS_SSL_MAX_CONTENT_LEN         (16*1024)   /**< Size of the input buffer */

#define MBEDTLS_SSL_MAX_OUT_CONTENT_LEN         (4*1024)   /**< Size of the output buffer */


#define MBEDTLS_TIMING_ALT

#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_CIPHER_MODE_CTR
#define MBEDTLS_REMOVE_ARC4_CIPHERSUITES

#define MBEDTLS_ECP_DP_SECP192R1_ENABLED
#define MBEDTLS_ECP_DP_SECP224R1_ENABLED
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_SECP521R1_ENABLED
#define MBEDTLS_ECP_DP_SECP192K1_ENABLED
#define MBEDTLS_ECP_DP_SECP224K1_ENABLED
#define MBEDTLS_ECP_DP_SECP256K1_ENABLED
#define MBEDTLS_ECP_DP_BP256R1_ENABLED
#define MBEDTLS_ECP_DP_BP384R1_ENABLED
#define MBEDTLS_ECP_DP_BP512R1_ENABLED
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED
#define MBEDTLS_ECP_DP_CURVE448_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM//undeter


#define MBEDTLS_KEY_EXCHANGE_PSK_ENABLED
#define MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED
#define MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED
#define MBEDTLS_KEY_EXCHANGE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21


#define MBEDTLS_SSL_PROTO_TLS1_2

#define MBEDTLS_SSL_SESSION_TICKETS

#define MBEDTLS_SSL_EXPORT_KEYS//ecoptional

#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_X509_CHECK_KEY_USAGE//OPTIONAL

#define MBEDTLS_X509_CHECK_EXTENDED_KEY_USAGE//OPTIONAL

/**
 * \def MBEDTLS_X509_RSASSA_PSS_SUPPORT
 *
 * Enable parsing and verification of X.509 certificates, CRLs and CSRS
 * signed with RSASSA-PSS (aka PKCS#1 v2.1).
 *
 * Comment this macro to disallow using RSASSA-PSS in certificates.
 */
#define MBEDTLS_X509_RSASSA_PSS_SUPPORT//1215 enable

#define MBEDTLS_AES_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_BIGNUM_C

#define MBEDTLS_CCM_C
#define MBEDTLS_CHACHA20_C //1215 enable

#define MBEDTLS_CHACHAPOLY_C //1215 enable
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CTR_DRBG_C

#define MBEDTLS_DHM_C  //1215 enable
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ENTROPY_C

#define MBEDTLS_GCM_C
#define MBEDTLS_HKDF_C   //NEW FEATRUE 1215
#define MBEDTLS_MD_C

#define MBEDTLS_MD5_C
#define MBEDTLS_OID_C

#define MBEDTLS_PEM_PARSE_C

#define MBEDTLS_PK_C

#define MBEDTLS_PK_PARSE_C

#define MBEDTLS_PLATFORM_C

/**
 * \def MBEDTLS_POLY1305_C
 *
 * Enable the Poly1305 MAC algorithm.
 *
 * Module:  library/poly1305.c
 * Caller:  library/chachapoly.c
 */
#define MBEDTLS_POLY1305_C  //NEW FEATRUE 1215
#define MBEDTLS_RSA_C

#define MBEDTLS_SHA1_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA512_C

#define MBEDTLS_SSL_CLI_C

#define MBEDTLS_SSL_TLS_C

#define MBEDTLS_THREADING_C
#define MBEDTLS_THREADING_IMPL

#define MBEDTLS_USE_RAND_API_ENTROPY

#define MBEDTLS_TIMING_C

#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_CRL_PARSE_C
#define MBEDTLS_X509_CSR_PARSE_C

/* MPI / BIGNUM options */
#define MBEDTLS_MPI_WINDOW_SIZE            3 /**< Maximum windows size used. */
#define MBEDTLS_MPI_MAX_SIZE            512 /**< Maximum number of bytes for usable MPIs. */

/* ECP options */
#define MBEDTLS_ECP_MAX_BITS             521 /**< Maximum bit size of groups */
#define MBEDTLS_ECP_WINDOW_SIZE            2 /**< Maximum window size used */
//#define MBEDTLS_ECP_FIXED_POINT_OPTIM      1 /**< Enable fixed-point speed-up */

/* Entropy options */
#define MBEDTLS_ENTROPY_MAX_SOURCES                2 /**< Maximum number of sources supported */
//#define MBEDTLS_ENTROPY_MAX_GATHER                128 /**< Maximum amount requested from entropy sources */
#define MBEDTLS_TLS_DEFAULT_ALLOW_SHA1_IN_CERTIFICATES
#define MBEDTLS_TLS_DEFAULT_ALLOW_SHA1_IN_KEY_EXCHANGE

#include "mbedtls/check_config.h"

#endif /* MBEDTLS_CONFIG_H */
