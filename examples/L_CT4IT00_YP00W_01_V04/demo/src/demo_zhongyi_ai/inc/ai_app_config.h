/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Hangyan AI platform application configuration.
 */

#ifndef AI_APP_CONFIG_H
#define AI_APP_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__has_include)
#if __has_include("ai_app_private_config.h")
#include "ai_app_private_config.h"
#endif
#endif

#ifndef AI_APP_DEVICE_TYPE
#ifdef AI_DEVICE_TYPE
#define AI_APP_DEVICE_TYPE AI_DEVICE_TYPE
#else
#define AI_APP_DEVICE_TYPE ""
#endif
#endif

#ifndef AI_APP_DEVICE_ID
#ifdef AI_DEVICE_ID
#define AI_APP_DEVICE_ID AI_DEVICE_ID
#else
#define AI_APP_DEVICE_ID ""
#endif
#endif

#ifndef AI_APP_AI_SDK_VER
#ifdef AI_SDK_VER
#define AI_APP_AI_SDK_VER AI_SDK_VER
#elif defined(SDK_VERSION)
#define AI_APP_AI_SDK_VER SDK_VERSION
#else
#define AI_APP_AI_SDK_VER "V0.0.0"
#endif
#endif

#ifndef AI_APP_SECRET_KEY
#ifdef AI_SECRET_KEY
#define AI_APP_SECRET_KEY AI_SECRET_KEY
#else
#define AI_APP_SECRET_KEY ""
#endif
#endif

#ifndef AI_APP_PRESET_AI_TOKEN
#ifdef AI_TOKEN
#define AI_APP_PRESET_AI_TOKEN AI_TOKEN
#else
#define AI_APP_PRESET_AI_TOKEN ""
#endif
#endif

#ifndef AI_APP_ALLOW_PRESET_TOKEN_FALLBACK
#define AI_APP_ALLOW_PRESET_TOKEN_FALLBACK 0
#endif

#ifndef AI_APP_HTTP_URL
#define AI_APP_HTTP_URL "https://a2link.komect.com:1443/cloud-device/ai/get"
#endif

#ifndef AI_APP_HTTP_SSL_VERIFY_NONE
#define AI_APP_HTTP_SSL_VERIFY_NONE 1
#endif

#ifndef AI_APP_WS_HOST
#define AI_APP_WS_HOST "demo.hjq.komect.com"
#endif

#ifndef AI_APP_WS_PORT
#define AI_APP_WS_PORT 443
#endif

#ifndef AI_APP_WS_PATH
#define AI_APP_WS_PATH "/speech/ws/"
#endif

#ifndef AI_APP_WS_USE_SSL
#define AI_APP_WS_USE_SSL 1
#endif

#ifndef AI_APP_WS_ALLOW_INSECURE
#define AI_APP_WS_ALLOW_INSECURE 1
#endif

#ifndef AI_APP_SIM_ID
#define AI_APP_SIM_ID 0
#endif

#ifndef AI_APP_PDP_CID
#define AI_APP_PDP_CID 1
#endif

#ifndef AI_APP_IP_TYPE
#define AI_APP_IP_TYPE 1
#endif

#ifndef AI_APP_APN
#define AI_APP_APN ""
#endif

#ifndef AI_APP_APN_USER
#define AI_APP_APN_USER ""
#endif

#ifndef AI_APP_APN_PASSWORD
#define AI_APP_APN_PASSWORD ""
#endif

#ifndef AI_APP_APN_AUTH_TYPE
#define AI_APP_APN_AUTH_TYPE 0
#endif

#ifndef AI_APP_NETWORK_REGISTER_TIMEOUT_S
#define AI_APP_NETWORK_REGISTER_TIMEOUT_S 120
#endif

#ifndef AI_APP_WS_TIMEOUT_MS
#define AI_APP_WS_TIMEOUT_MS 60000
#endif

typedef struct {
    const char *device_type;
    const char *device_id;
    const char *ai_sdk_ver;
    const char *auth_secret_key;
    const char *preset_ai_token;
    bool allow_preset_token_fallback;

    const char *http_url;
    bool http_ssl_verify_none;

    const char *ws_host;
    int ws_port;
    const char *ws_path;
    bool ws_use_ssl;
    bool ws_allow_insecure;

    uint8_t sim_id;
    int pdp_cid;
    int ip_type;
    const char *apn;
    const char *apn_user;
    const char *apn_password;
    int apn_auth_type;
    unsigned int network_register_timeout_s;
    unsigned int ws_timeout_ms;
} ai_app_config_t;

const ai_app_config_t *ai_app_config_get(void);
bool ai_app_config_validate(const ai_app_config_t *cfg, char *err, size_t err_size);
bool ai_app_config_string_empty(const char *value);

#endif /* AI_APP_CONFIG_H */
