/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ai_app_config.h"

#include <stdio.h>
#include <string.h>

static const ai_app_config_t s_ai_app_config = {
    .device_type = AI_APP_DEVICE_TYPE,
    .device_id = AI_APP_DEVICE_ID,
    .ai_sdk_ver = AI_APP_AI_SDK_VER,
    .auth_secret_key = AI_APP_SECRET_KEY,
    .preset_ai_token = AI_APP_PRESET_AI_TOKEN,
    .allow_preset_token_fallback = (AI_APP_ALLOW_PRESET_TOKEN_FALLBACK != 0),

    .http_url = AI_APP_HTTP_URL,
    .http_ssl_verify_none = (AI_APP_HTTP_SSL_VERIFY_NONE != 0),

    .ws_host = AI_APP_WS_HOST,
    .ws_port = AI_APP_WS_PORT,
    .ws_path = AI_APP_WS_PATH,
    .ws_use_ssl = (AI_APP_WS_USE_SSL != 0),
    .ws_allow_insecure = (AI_APP_WS_ALLOW_INSECURE != 0),

    .sim_id = AI_APP_SIM_ID,
    .pdp_cid = AI_APP_PDP_CID,
    .ip_type = AI_APP_IP_TYPE,
    .apn = AI_APP_APN,
    .apn_user = AI_APP_APN_USER,
    .apn_password = AI_APP_APN_PASSWORD,
    .apn_auth_type = AI_APP_APN_AUTH_TYPE,
    .network_register_timeout_s = AI_APP_NETWORK_REGISTER_TIMEOUT_S,
    .ws_timeout_ms = AI_APP_WS_TIMEOUT_MS,
};

bool ai_app_config_string_empty(const char *value)
{
    return (value == NULL) || (value[0] == '\0');
}

const ai_app_config_t *ai_app_config_get(void)
{
    return &s_ai_app_config;
}

bool ai_app_config_validate(const ai_app_config_t *cfg, char *err, size_t err_size)
{
    if (cfg == NULL) {
        if (err != NULL && err_size > 0U) {
            snprintf(err, err_size, "config is NULL");
        }
        return false;
    }

    if (ai_app_config_string_empty(cfg->device_type)) {
        if (err != NULL && err_size > 0U) {
            snprintf(err, err_size, "AI_APP_DEVICE_TYPE is empty");
        }
        return false;
    }

    if (ai_app_config_string_empty(cfg->device_id)) {
        if (err != NULL && err_size > 0U) {
            snprintf(err, err_size, "AI_APP_DEVICE_ID is empty");
        }
        return false;
    }

    if (ai_app_config_string_empty(cfg->ai_sdk_ver)) {
        if (err != NULL && err_size > 0U) {
            snprintf(err, err_size, "AI_APP_AI_SDK_VER is empty");
        }
        return false;
    }

    if (ai_app_config_string_empty(cfg->auth_secret_key) &&
        (!cfg->allow_preset_token_fallback || ai_app_config_string_empty(cfg->preset_ai_token))) {
        if (err != NULL && err_size > 0U) {
            snprintf(err, err_size, "AI_APP_SECRET_KEY is empty");
        }
        return false;
    }

    if (ai_app_config_string_empty(cfg->http_url)) {
        if (err != NULL && err_size > 0U) {
            snprintf(err, err_size, "AI_APP_HTTP_URL is empty");
        }
        return false;
    }

    if (ai_app_config_string_empty(cfg->ws_host) || ai_app_config_string_empty(cfg->ws_path)) {
        if (err != NULL && err_size > 0U) {
            snprintf(err, err_size, "AI_APP_WS_HOST or AI_APP_WS_PATH is empty");
        }
        return false;
    }

    if (cfg->ws_port <= 0) {
        if (err != NULL && err_size > 0U) {
            snprintf(err, err_size, "AI_APP_WS_PORT is invalid");
        }
        return false;
    }

    return true;
}
