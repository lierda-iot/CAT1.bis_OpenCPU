/* Copyright (c) 2026 探鸽智能
 * SPDX-License-Identifier: MIT
 * Adapted from xiaotai-lierda f159bfd3: provisioning-only public surface. */
#ifndef TIRTC_APP_DEVICE_BINDING_H
#define TIRTC_APP_DEVICE_BINDING_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define DEMO_BIND_DEVICE_ID_MAX 64U
#define DEMO_BIND_DEVICE_KEY_MAX 256U
#define DEMO_BIND_CLIENT_ID_MAX 32U
#define DEMO_BIND_ICCID_MAX 32U
#define DEMO_BIND_TIRTC_ENDPOINT_MAX 111U
#define DEMO_BIND_AI_PEER_ID_MAX 768U
#define DEMO_BIND_AI_TOKEN_MAX 2048U
#define DEMO_BIND_AI_ROLE_ID_MAX 96U
#define DEMO_BIND_AI_APP_ID_MAX 64U
typedef struct {
    char device_id[DEMO_BIND_DEVICE_ID_MAX], device_key[DEMO_BIND_DEVICE_KEY_MAX];
    char client_id[DEMO_BIND_CLIENT_ID_MAX], iccid[DEMO_BIND_ICCID_MAX];
    char tirtc_endpoint[DEMO_BIND_TIRTC_ENDPOINT_MAX];
} demo_binding_tirtc_identity_t;
typedef struct {
    char device_id[DEMO_BIND_DEVICE_ID_MAX], device_key[DEMO_BIND_DEVICE_KEY_MAX];
    char tirtc_endpoint[DEMO_BIND_TIRTC_ENDPOINT_MAX];
    char peer_id[DEMO_BIND_AI_PEER_ID_MAX], token[DEMO_BIND_AI_TOKEN_MAX];
    char role_id[DEMO_BIND_AI_ROLE_ID_MAX], app_id[DEMO_BIND_AI_APP_ID_MAX];
    uint32_t credential_epoch; /* Nonzero binding lifetime that issued this access. */
} demo_binding_ai_access_t;
/* AI worker only. Credentials never enter UI or diagnostic logs. */
int demo_binding_fetch_ai_access(demo_binding_ai_access_t *out);
void demo_binding_clear_ai_access(demo_binding_ai_access_t *access);
typedef enum {
    DEMO_BIND_IDLE = 0, DEMO_BIND_WAIT_NETWORK, DEMO_BIND_DISCOVERING,
    DEMO_BIND_REPORTING, DEMO_BIND_WAIT_GRANT, DEMO_BIND_VERIFYING,
    DEMO_BIND_BOUND, DEMO_BIND_RESTARTING, DEMO_BIND_ERROR
} demo_binding_state_e;
typedef struct {
    demo_binding_state_e state;
    char code[7]; uint16_t seconds_left; int error;
    bool api_ready, binding_required;
    uint32_t generation, sample_ms;
    uint32_t credential_epoch; /* Independent of network generation; rejects same-route rebind ABA. */
    char device_id[64];
} demo_binding_snapshot_t;
void demo_binding_task(void *argument);
void demo_binding_request(void);
void demo_binding_get_snapshot(demo_binding_snapshot_t *out);
/* Secret-free identity/route lease for worker-only business requests. */
typedef struct {
    uint32_t binding_generation, credential_epoch, route_generation;
    int sim_id;
} demo_binding_service_context_t;
typedef enum {
    DEMO_BIND_SERVICE_DEVICE = 0, DEMO_BIND_SERVICE_AI,
    DEMO_BIND_SERVICE_VOIP, DEMO_BIND_SERVICE_CALL
} demo_binding_service_e;
typedef enum { DEMO_BIND_HTTP_GET = 0, DEMO_BIND_HTTP_POST } demo_binding_http_method_e;
/* RAM-only task-context snapshots. No tokens leave the platform. */
bool demo_binding_service_context(demo_binding_service_context_t *out);
bool demo_binding_service_context_current(const demo_binding_service_context_t *context);
/* Serialized asynchronous SDK HTTP lifecycle; worker only. Return zero means
 * transport completion, not HTTP/business success. Response is cleared on
 * stale identity/route, including changes while waiting for the HTTP mutex.
 * path must be a local /v1/... path without controls, fragment or authority.
 * Response capacity <=8192; body <2048 bytes; timeout 20..30000 ms. */
int demo_binding_service_request_guarded(
    demo_binding_service_e service, demo_binding_http_method_e method,
    const char *path, const char *json_body, char *response,
    size_t response_size, int *http_status, uint32_t timeout_ms,
    const demo_binding_service_context_t *expected);
/* Worker-only, may query SIM/IMEI. Secret output must never enter UI/logs. */
int demo_binding_get_tirtc_identity(demo_binding_tirtc_identity_t *out);
#endif
