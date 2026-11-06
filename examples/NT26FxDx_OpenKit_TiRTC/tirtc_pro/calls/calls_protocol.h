/* ThingConnect call protocol, adapted from official Lierda/ESP32 examples.
 * Copyright (c) 2026 Tange Intelligent Technology.
 * SPDX-License-Identifier: MIT */
#ifndef TIRTC_CALLS_PROTOCOL_H
#define TIRTC_CALLS_PROTOCOL_H
#include "../contacts/tirtc_contacts.h"
#include "../platform/formal_mqtt.h"

enum { CALLS_PROTO_INVALID=-5601, CALLS_PROTO_SCHEMA=-5602,
       CALLS_PROTO_STALE=-5603, CALLS_PROTO_HTTP=-5604, CALLS_PROTO_MEMORY=-5605 };
typedef enum { CALLS_PROTO_NONE=0, CALLS_PROTO_INCOMING,
               CALLS_PROTO_CANCEL, CALLS_PROTO_ANSWERED, CALLS_PROTO_REJECT } calls_proto_event_type_t;
typedef struct {
    calls_proto_event_type_t type;
    bool wechat, video;
    char room[129], peer[1152], token[1152], contact_id[96], name[96];
    char app_id[64], model_id[64], session_token[256], payload[512];
    char call_id[65], from[64];
} calls_proto_event_t;
typedef struct { char room[129], call_id[65]; } calls_proto_dial_t;
typedef struct { char peer[65], token[601]; } calls_proto_access_t;
typedef struct {
    bool present, caller_role, answered, video;
    char room[129], caller[96];
} calls_proto_room_t;
typedef enum { CALLS_PROTO_ROOM_REJECT=0, CALLS_PROTO_ROOM_CANCEL,
               CALLS_PROTO_ROOM_HANGUP } calls_proto_room_action_t;

/* Synchronous worker-only HTTP. No calls/UI/audio state is mutated. Return 0
 * is checked HTTP + exact service success code + valid schema. Positive
 * values preserve business errors; negative values are transport/validation.
 * Output is cleared on errors except dial.room may contain a 40202 hint;
 * verify that hint with room_get before any room mutation. */
int calls_proto_device_dial(const tirtc_contact_t *contact, bool video,
    const demo_binding_service_context_t *context, calls_proto_dial_t *out);
int calls_proto_device_info(const char *caller, const char *room,
    const demo_binding_service_context_t *context, calls_proto_access_t *out);
int calls_proto_room_get(const demo_binding_service_context_t *context,
    calls_proto_room_t *out);
int calls_proto_room_action(calls_proto_room_action_t action,
    const char *room, const char *reason, const demo_binding_service_context_t *context);
int calls_proto_wx_dial(const tirtc_contact_t *contact, const char *device_id, bool video,
    const demo_binding_service_context_t *context, calls_proto_dial_t *out);
int calls_proto_profile(const char *json_body,
    const demo_binding_service_context_t *context);
/* Worker-only rejection, fields from the exact saved call_incoming event. */
int calls_proto_wx_reject(const calls_proto_event_t *event, unsigned reason,
    const demo_binding_service_context_t *context);

/* MQTT borrowed cJSON input; only bounded copies, no I/O or allocation. All
 * selected fields are checked for duplicate keys, controls and truncation.
 * Returns 1 recognized, 0 unrelated, negative malformed. */
int calls_proto_parse_mqtt(demo_formal_mqtt_message_kind_e kind,
    const char *type, const char *channel, const cJSON *payload, calls_proto_event_t *out);
void calls_proto_clear_event(calls_proto_event_t *event);
#endif
