/*
 * Copyright (c) 2026 探鸽智能
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: MIT AND Apache-2.0
 *
 * Permanent post-binding MQTT transport and bounded message fan-out.
 */

#ifndef TIRTC_APP_FORMAL_MQTT_H
#define TIRTC_APP_FORMAL_MQTT_H

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"

typedef enum
{
    DEMO_FORMAL_MQTT_COMMAND = 0,
    DEMO_FORMAL_MQTT_NOTIFY,
} demo_formal_mqtt_message_kind_e;

/* type/channel/payload are valid only for the duration of the callback.
 * Handlers must only make bounded copies or post a non-blocking event; HTTP,
 * TiRTC and audio work belongs in the feature worker task. */
typedef void (*demo_formal_mqtt_message_handler_t)(
    demo_formal_mqtt_message_kind_e kind,
    const char *type,
    const char *channel,
    const cJSON *payload,
    void *user);

/*
 * Permanent ThingConnect MQTT connection.
 *
 * This module owns only the post-binding MQTT transport.  Provisioning and
 * credential persistence remain in device_binding.c, while future AI/WX/DEV
 * message routing can be added here without coupling it to the OLED UI.
 */
int demo_formal_mqtt_connect(const char *mqtt_url,
                             const char *device_id,
                             const char *mqtt_token);

/* Bounded disconnect/deinit.  Safe to call when already stopped.  LIOT's
 * asynchronous callbacks are drained by the implementation; a failed
 * deinit submission retains its handle and is retried by maintenance or the
 * next lifecycle request. */
void demo_formal_mqtt_disconnect(void);

/* True only after CONNECT and both cmd/notify subscriptions complete. */
bool demo_formal_mqtt_is_online(void);

/* Atomically consume a remote {"type":"unbind"} notification. */
bool demo_formal_mqtt_take_unbind(void);

/* Copy the current device JWT without exposing it in logs.  The token is
 * needed by ThingConnect business APIs such as GET /v1/ai/token. */
bool demo_formal_mqtt_copy_token(char *output, size_t output_size);

/* Fixed-capacity, allocation-free fan-out for AI/WX/DEV business messages.
 * Registering the same handler/user pair twice is idempotent. */
int demo_formal_mqtt_register_handler(
    demo_formal_mqtt_message_handler_t handler,
    void *user);
void demo_formal_mqtt_unregister_handler(
    demo_formal_mqtt_message_handler_t handler,
    void *user);

/* Lightweight heartbeat; call periodically from the existing binding task. */
void demo_formal_mqtt_maintenance(void);

#endif /* TIRTC_APP_FORMAL_MQTT_H */
