/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AI_HTTP_TOKEN_H
#define AI_HTTP_TOKEN_H

#include <stdint.h>

#include "ai_app_config.h"

#define AI_HTTP_TOKEN_MAX 512
#define AI_HTTP_URL_MAX 256
#define AI_HTTP_TIMEZONE_MAX 32

typedef struct {
    char ai_token[AI_HTTP_TOKEN_MAX];
    char ai_url[AI_HTTP_URL_MAX];
    int resp_code;
    int http_status;
    uint64_t server_timestamp;
    char timezone[AI_HTTP_TIMEZONE_MAX];
} ai_http_token_result_t;

int ai_http_fetch_token(const ai_app_config_t *cfg, ai_http_token_result_t *out);

#endif /* AI_HTTP_TOKEN_H */
