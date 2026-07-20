/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ai_hello_smoke.h"

#include <string.h>

#include "ai_app_config.h"
#include "ai_http_token.h"
#include "ai_net.h"
#include "ai_ws_client.h"
#include "ai_app_log.h"
#include "liot_os.h"

#define AI_HELLO_TASK_STACK_SIZE (96 * 1024)
#define AI_HELLO_TASK_PRIORITY 5

static liot_task_t s_ai_hello_task = NULL;

static void ai_hello_smoke_task(void *arg)
{
    const ai_app_config_t *cfg = NULL;
    ai_http_token_result_t token_result;
    ai_ws_client_result_t ws_result;
    const char *ws_token = NULL;
    char err[128] = {0};
    int ret = 0;
    (void)arg;

    memset(&token_result, 0, sizeof(token_result));
    memset(&ws_result, 0, sizeof(ws_result));

    liot_trace("ai_hello_smoke start");

    cfg = ai_app_config_get();
    if (!ai_app_config_validate(cfg, err, sizeof(err))) {
        liot_trace("ai_hello_smoke config invalid: %s", err);
        liot_trace("ai_hello_smoke set local credentials via demo_zhongyi_ai/inc/ai_app_private_config.h or build macros");
        goto exit;
    }

    ret = ai_net_start(cfg);
    if (ret != 0) {
        liot_trace("ai_hello_smoke network failed ret=%d; SIM card/APN/signal/data service may be the cause",
                   ret);
        goto exit;
    }

    ret = ai_http_fetch_token(cfg, &token_result);
    if (ret != 0) {
        liot_trace("ai_hello_smoke HTTP token failed ret=%d", ret);
        if (cfg->allow_preset_token_fallback && !ai_app_config_string_empty(cfg->preset_ai_token)) {
            liot_trace("ai_hello_smoke using preset token fallback");
            ws_token = cfg->preset_ai_token;
        } else {
            goto exit;
        }
    } else {
        ws_token = token_result.ai_token;
        liot_trace("ai_hello_smoke token ok, starting ws token_len=%d", (int)strlen(ws_token));
    }

    ret = ai_ws_client_connect(cfg, ws_token, &ws_result);
    if (ret != 0) {
        liot_trace("ai_hello_smoke WSS connect failed ret=%d", ret);
        goto exit;
    }

    liot_trace("ai_hello_smoke done trace_id=%s session_id=%s",
               ws_result.trace_id,
               ws_result.session_id);
    ai_ws_client_close();

exit:
    s_ai_hello_task = NULL;
    liot_trace("ai_hello_smoke task exit");
    liot_rtos_task_delete(NULL);
}

int ai_hello_smoke_start(void)
{
    if (s_ai_hello_task != NULL) {
        liot_trace("ai_hello_smoke already started");
        return 0;
    }

    if (liot_rtos_task_create(&s_ai_hello_task,
                              AI_HELLO_TASK_STACK_SIZE,
                              AI_HELLO_TASK_PRIORITY,
                              "ai_hello",
                              ai_hello_smoke_task,
                              NULL) != LIOT_OSI_SUCCESS) {
        s_ai_hello_task = NULL;
        liot_trace("ai_hello_smoke task create failed");
        return -1;
    }

    return 0;
}
