/*
 * Copyright (c) 2026 探鸽智能
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: MIT AND Apache-2.0
 *
 * Public snapshots and serialized cloud-service requests for provisioning.
 */

#ifndef TIRTC_APP_DEVICE_BINDING_H
#define TIRTC_APP_DEVICE_BINDING_H

#include <stddef.h>
#include <stdint.h>

#define DEMO_BIND_DEVICE_ID_MAX       64U
#define DEMO_BIND_DEVICE_KEY_MAX      256U
#define DEMO_BIND_CLIENT_ID_MAX       32U
#define DEMO_BIND_ICCID_MAX           32U
/* 110-byte endpoint + 17-byte WX reject path + NUL fit the SDK's 128-byte URL. */
#define DEMO_BIND_TIRTC_ENDPOINT_MAX  111U
#define DEMO_BIND_AI_PEER_ID_MAX      768U
#define DEMO_BIND_AI_TOKEN_MAX        2048U
#define DEMO_BIND_AI_ROLE_ID_MAX      96U
#define DEMO_BIND_AI_APP_ID_MAX       64U

typedef struct
{
    char device_id[DEMO_BIND_DEVICE_ID_MAX];
    char device_key[DEMO_BIND_DEVICE_KEY_MAX];
    char client_id[DEMO_BIND_CLIENT_ID_MAX];
    char iccid[DEMO_BIND_ICCID_MAX];
    char tirtc_endpoint[DEMO_BIND_TIRTC_ENDPOINT_MAX];
} demo_binding_tirtc_identity_t;

typedef struct
{
    char device_id[DEMO_BIND_DEVICE_ID_MAX];
    char device_key[DEMO_BIND_DEVICE_KEY_MAX];
    char tirtc_endpoint[DEMO_BIND_TIRTC_ENDPOINT_MAX];
    char peer_id[DEMO_BIND_AI_PEER_ID_MAX];
    char token[DEMO_BIND_AI_TOKEN_MAX];
    char role_id[DEMO_BIND_AI_ROLE_ID_MAX];
    char app_id[DEMO_BIND_AI_APP_ID_MAX];
} demo_binding_ai_access_t;

typedef enum
{
    DEMO_BIND_IDLE = 0,
    DEMO_BIND_WAIT_NETWORK,
    DEMO_BIND_DISCOVERING,
    DEMO_BIND_REPORTING,
    DEMO_BIND_WAIT_GRANT,
    DEMO_BIND_VERIFYING,
    DEMO_BIND_BOUND,
    DEMO_BIND_RESTARTING,
    DEMO_BIND_ERROR,
} demo_binding_state_e;

typedef struct
{
    demo_binding_state_e state;
    char code[7];
    uint16_t seconds_left;
    int error;
} demo_binding_snapshot_t;

/* Discovered ThingConnect services.  Feature modules use this narrow API so
 * all requests share binding's serialized libliot_https transport. */
typedef enum
{
    DEMO_BIND_SERVICE_DEVICE = 0,
    DEMO_BIND_SERVICE_AI,
    DEMO_BIND_SERVICE_VOIP,
    DEMO_BIND_SERVICE_CALL,
} demo_binding_service_e;

typedef enum
{
    DEMO_BIND_HTTP_GET = 0,
    DEMO_BIND_HTTP_POST,
} demo_binding_http_method_e;

/* Dedicated worker task.  It never accesses the OLED or key GPIOs. */
void demo_binding_task(void *argv);

/* Start/retry provisioning.  Safe to call from the UI task. */
void demo_binding_request(void);

/* Non-blocking copy of the state currently visible to the UI. */
void demo_binding_get_snapshot(demo_binding_snapshot_t *out);

/* Copy the long-lived identity used to start the single device TiRTC
 * runtime.  client_id is the same stable pseudo-MAC reported at binding;
 * iccid identifies the active 4G route.  Secret values are never logged. */
int demo_binding_get_tirtc_identity(demo_binding_tirtc_identity_t *out);

/* Fetch short-lived AI Chat connection credentials from the discovered
 * ThingConnect ai-server.  Secrets are copied into out and never logged. */
int demo_binding_fetch_ai_access(demo_binding_ai_access_t *out);

/* Zero every credential field after a connect attempt. */
void demo_binding_clear_ai_access(demo_binding_ai_access_t *access);

/* Return 0 when the HTTP transaction completed.  Business success is
 * determined by http_status and the JSON response.  This call blocks and is
 * intended for feature worker tasks, never MQTT/TiRTC callbacks or the UI. */
int demo_binding_service_request(demo_binding_service_e service,
                                 demo_binding_http_method_e method,
                                 const char *path,
                                 const char *json_body,
                                 char *response,
                                 size_t response_size,
                                 int *http_status);

/* Variant for latency-sensitive feature workers.  timeout_ms controls the
 * response/send/receive deadline; cleanup still follows the vendor HTTP
 * singleton's close/release contract. */
int demo_binding_service_request_timeout(
    demo_binding_service_e service,
    demo_binding_http_method_e method,
    const char *path,
    const char *json_body,
    char *response,
    size_t response_size,
    int *http_status,
    uint32_t timeout_ms);

#endif /* TIRTC_APP_DEVICE_BINDING_H */
