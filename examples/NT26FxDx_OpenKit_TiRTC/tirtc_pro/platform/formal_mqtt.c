/*
 * Copyright (c) 2026 探鸽智能
 * SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
 * SPDX-License-Identifier: MIT
 */

/**
 * @file formal_mqtt.c
 * @brief Minimal permanent ThingConnect MQTT transport.
 *
 * Protocol:
 *   ClientID = sn_{device_id}, Username = device_id, Password = mqtt_token
 *   subscribe QoS1: device/sn_{device_id}/cmd and /notify
 *   ACK every /cmd on device/sn_{device_id}/ack
 *
 * MQTT callbacks never destroy the client or reboot the device.  They only
 * record events; demo_binding_task owns lifecycle and recovery decisions.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "json_guard.h"
#include "liot_log.h"
#include "../tirtc_log.h"
#include "liot_mqtt_client.h"
#include "liot_os.h"

#include "formal_mqtt.h"
#include "platform_internal.h"
#include "tirtc_mqtt_compat.h"

#define FORMAL_MQTT_PDP_CID          1
#define FORMAL_MQTT_CONNECT_MS       30000U
#define FORMAL_MQTT_SUBSCRIBE_MS     10000U
#define FORMAL_MQTT_DISCONNECT_MS    10000U
#define FORMAL_MQTT_SETTLE_MS        5000U
#define FORMAL_MQTT_CALLBACK_DRAIN_MS 2000U
#define FORMAL_MQTT_ASYNC_DEINIT_MS   5000U
#define FORMAL_MQTT_HEARTBEAT_MS     30000U
#define FORMAL_MQTT_RESUB_RETRY_MS    5000U
#define FORMAL_MQTT_PAYLOAD_MAX       6144U

#define FORMAL_DEVICE_ID_MAX         64
#define FORMAL_MQTT_TOKEN_MAX        1024
#define FORMAL_CLIENT_ID_MAX         72
#define FORMAL_TOPIC_MAX             128
#define FORMAL_HANDLER_MAX           4U
#define FORMAL_CLIENT_SLOT_COUNT      2U

typedef struct
{
    demo_formal_mqtt_message_handler_t handler;
    void *user;
} formal_handler_slot_t;

typedef enum
{
    FORMAL_LIFECYCLE_IDLE = 0,
    FORMAL_LIFECYCLE_INITIALIZING,
    FORMAL_LIFECYCLE_ACTIVE,
    FORMAL_LIFECYCLE_CLOSING,
    FORMAL_LIFECYCLE_DEINIT_RETRY,
} formal_lifecycle_e;

typedef struct
{
    uint32_t generation;
} formal_callback_context_t;

/* Client-handle storage and callback contexts have static lifetime.  This is
 * deliberate: an asynchronous LIOT callback must never observe reclaimed
 * stack storage, even while a former client is being deinitialized. */
static liot_mqtt_client_t s_client_slots[FORMAL_CLIENT_SLOT_COUNT];
static formal_callback_context_t
    s_callback_contexts[FORMAL_CLIENT_SLOT_COUNT];
static liot_mqtt_client_t *s_client;
static liot_mqtt_client_t *s_retired_client;
static formal_callback_context_t *s_callback_context;
static uint32_t s_client_slot_cursor;
static uint32_t s_generation;
static volatile formal_lifecycle_e s_lifecycle = FORMAL_LIFECYCLE_IDLE;
static volatile uint32_t s_callbacks_inflight;
static volatile bool s_accept_incoming;
static bool s_accept_events;
static liot_sem_t s_sem;
static liot_mutex_t s_lifecycle_mutex;
static volatile bool s_connect_done;
static volatile bool s_connect_ok;
static volatile bool s_sub_done;
static volatile bool s_sub_ok;
static volatile bool s_disconnect_done;
static volatile bool s_lost;
static volatile bool s_online;
/* The LIOT client can reconnect internally without recreating this owner.
 * Keep clean_session enabled for this port (persistent sessions cause a
 * CONNECT-event loop), and restore both subscriptions after a later CONNECT. */
static volatile bool s_subscriptions_ready;
static volatile bool s_resubscribe_pending;
static volatile bool s_unbind_pending;
static uint32_t s_resubscribe_retry_at;

static char s_device_id[FORMAL_DEVICE_ID_MAX];
static char s_mqtt_token[FORMAL_MQTT_TOKEN_MAX];
static char s_client_id[FORMAL_CLIENT_ID_MAX];
static char s_cmd_topic[FORMAL_TOPIC_MAX];
static char s_notify_topic[FORMAL_TOPIC_MAX];
static char s_ack_topic[FORMAL_TOPIC_MAX];
static char s_up_topic[FORMAL_TOPIC_MAX];
static uint32_t s_heartbeat_tick;
static uint32_t s_heartbeat_seq;
static formal_handler_slot_t s_handlers[FORMAL_HANDLER_MAX];
static liot_mqtt_client_option s_options;

static void formal_signal(void)
{
    if (s_sem != NULL)
    {
        liot_rtos_semaphore_release(s_sem);
    }
}

static bool formal_deadline_pending(uint32_t deadline)
{
    return (int32_t)(deadline - liot_rtos_get_running_time()) > 0;
}

static int formal_ensure_sync_objects(void)
{
    liot_sem_t new_sem = NULL;
    liot_mutex_t new_mutex = NULL;

    /* These two small RTOS objects live for the process lifetime.  Deleting
     * and recreating them per connection is unsafe because LIOT completes
     * disconnect/deinit on worker tasks and may deliver a final callback. */
    if (s_sem == NULL &&
        liot_rtos_semaphore_create(&new_sem, 0) != LIOT_OSI_SUCCESS)
    {
        return -1;
    }
    if (s_lifecycle_mutex == NULL &&
        liot_rtos_mutex_create(&new_mutex) != LIOT_OSI_SUCCESS)
    {
        if (new_sem != NULL)
        {
            tirtc_platform_sem_delete_idle(new_sem);
        }
        return -1;
    }

    liot_rtos_enter_critical();
    if (s_sem == NULL && new_sem != NULL)
    {
        s_sem = new_sem;
        new_sem = NULL;
    }
    if (s_lifecycle_mutex == NULL && new_mutex != NULL)
    {
        s_lifecycle_mutex = new_mutex;
        new_mutex = NULL;
    }
    liot_rtos_exit_critical();

    /* A concurrent first caller may have installed the object first. */
    if (new_sem != NULL)
    {
        tirtc_platform_sem_delete_idle(new_sem);
    }
    if (new_mutex != NULL)
    {
        liot_rtos_mutex_delete(new_mutex);
    }
    return (s_sem != NULL && s_lifecycle_mutex != NULL) ? 0 : -1;
}

static bool formal_callback_enter(liot_mqtt_client_t *client,
                                  void *arg,
                                  bool require_context,
                                  bool incoming)
{
    formal_callback_context_t *context =
        (formal_callback_context_t *)arg;
    bool accepted = false;

    liot_rtos_enter_critical();
    if (client != NULL && s_client != NULL && *client != 0 &&
        *client == *s_client && s_accept_events && s_lifecycle != FORMAL_LIFECYCLE_IDLE &&
        (!require_context ||
         (context == s_callback_context && context != NULL &&
          context->generation == s_generation)) &&
        (!incoming ||
         (s_lifecycle == FORMAL_LIFECYCLE_ACTIVE && s_accept_incoming)))
    {
        s_callbacks_inflight++;
        accepted = true;
    }
    liot_rtos_exit_critical();
    return accepted;
}

static void formal_callback_leave(void)
{
    liot_rtos_enter_critical();
    if (s_callbacks_inflight > 0U)
    {
        s_callbacks_inflight--;
    }
    liot_rtos_exit_critical();
    formal_signal();
}

static bool formal_wait_callbacks_drained(uint32_t timeout_ms)
{
    uint32_t deadline = liot_rtos_get_running_time() + timeout_ms;

    while (s_callbacks_inflight != 0U && formal_deadline_pending(deadline))
    {
        liot_rtos_semaphore_wait(s_sem, 100);
    }
    return s_callbacks_inflight == 0U;
}

static void formal_reset_transport_flags(void)
{
    s_connect_done = false;
    s_connect_ok = false;
    s_sub_done = false;
    s_sub_ok = false;
    s_disconnect_done = false;
    s_lost = false;
    s_online = false;
    s_subscriptions_ready = false;
    s_resubscribe_pending = false;
    s_resubscribe_retry_at = 0U;
}

static void formal_clear_connection_storage(void)
{
    memset(&s_options, 0, sizeof(s_options));
    memset(s_device_id, 0, sizeof(s_device_id));
    memset(s_mqtt_token, 0, sizeof(s_mqtt_token));
    memset(s_client_id, 0, sizeof(s_client_id));
    memset(s_cmd_topic, 0, sizeof(s_cmd_topic));
    memset(s_notify_topic, 0, sizeof(s_notify_topic));
    memset(s_ack_topic, 0, sizeof(s_ack_topic));
    memset(s_up_topic, 0, sizeof(s_up_topic));
}

static int formal_reap_retired_client_locked(bool wait)
{
    uint32_t deadline = liot_rtos_get_running_time() + FORMAL_MQTT_ASYNC_DEINIT_MS;
    if (s_retired_client == NULL)
    {
        return 0;
    }
    while (wait && !tirtc_mqtt_cleanup_complete(s_retired_client) && formal_deadline_pending(deadline))
    {
        liot_rtos_task_sleep_ms(100);
    }
    if (!tirtc_mqtt_cleanup_complete(s_retired_client))
    {
        return -1;
    }

    /* The fixed SDK ABI helper verifies all terminal fields below its worker
     * priority. Elapsed time alone must never make this storage reusable. */
    *s_retired_client = 0;
    s_retired_client = NULL;
    formal_clear_connection_storage();
    return 0;
}

static void formal_begin_client_generation_locked(void)
{
    uint32_t slot = s_client_slot_cursor % FORMAL_CLIENT_SLOT_COUNT;

    s_client_slot_cursor++;
    memset(&s_client_slots[slot], 0, sizeof(s_client_slots[slot]));
    s_generation++;
    if (s_generation == 0U)
    {
        s_generation = 1U;
    }
    s_callback_contexts[slot].generation = s_generation;

    liot_rtos_enter_critical();
    s_client = &s_client_slots[slot];
    s_callback_context = &s_callback_contexts[slot];
    s_callbacks_inflight = 0U;
    s_accept_incoming = false;
    s_accept_events = true;
    s_lifecycle = FORMAL_LIFECYCLE_INITIALIZING;
    liot_rtos_exit_critical();
}

static bool formal_state_is_stopped(int state)
{
    return state == MQTT_CONN_DEFAULT ||
           state == MQTT_CONN_NOT_OPEN ||
           state == MQTT_CONN_OPEN_FAIL ||
           state == MQTT_CONN_CONNECT_FAIL ||
           state == MQTT_CONN_CLOSED ||
           state == MQTT_CONN_CLOSED_FAIL ||
           state == MQTT_CONN_DISCONNECTED ||
           state == MQTT_CONN_DISCONNECTED_FAIL ||
           state == MQTT_CONN_RECONNECTING_FAIL;
}

static void formal_event_cb(liot_mqtt_client_t *client,
                            int event,
                            void *arg,
                            void *data)
{
    int value;

    if (!formal_callback_enter(client, arg, true, false))
    {
        return;
    }
    value = (data != NULL) ? *(int *)data : -1;
    switch (event)
    {
    case LIOT_MQTT_CONNECT_EVENT:
        liot_trace("[FORMAL-MQTT] connect status=%d\r\n", value);
        s_connect_ok = (value == 0);
        s_connect_done = true;
        if (value == 0)
        {
            s_lost = false;
            /* Initial subscriptions are owned by demo_formal_mqtt_connect().
             * A later CONNECT is the SDK's reconnect completion.  Queue the
             * blocking SUBACK waits for maintenance, but keep an already
             * healthy transport online so active AI/WX sessions are not
             * aborted during this short restoration window. */
            if (s_subscriptions_ready)
            {
                s_resubscribe_pending = true;
                s_resubscribe_retry_at = 0U;
            }
        }
        formal_signal();
        break;

    case LIOT_MQTT_RECONNECT_EVENT:
        liot_trace("[FORMAL-MQTT] reconnecting\r\n");
        formal_signal();
        break;

    case LIOT_MQTT_SUB_EVENT:
        /* F6D_A reports completion result, not packet id. */
        liot_trace("[FORMAL-MQTT] SUBACK result=%d\r\n", value);
        s_sub_ok = (value == 0);
        s_sub_done = true;
        formal_signal();
        break;

    case LIOT_MQTT_PUB_EVENT:
        /* QoS 0 reports local completion, not broker acknowledgment. */
        if (TIRTC_ENABLE_DEBUG_LOG || value != 0) liot_trace("[FORMAL-MQTT] publish complete result=%d\r\n", value);
        formal_signal();
        break;

    case LIOT_MQTT_DISCONNECT_EVENT:
        liot_trace("[FORMAL-MQTT] disconnect event result=%d\r\n", value);
        s_disconnect_done = true;
        s_lost = true;
        s_online = false;
        s_accept_incoming = false;
        formal_signal();
        break;

    case LIOT_MQTT_CLOSE_EVENT:
        liot_trace("[FORMAL-MQTT] close event result=%d\r\n", value);
        s_lost = true;
        s_online = false;
        s_accept_incoming = false;
        formal_signal();
        break;

    default:
        break;
    }
    formal_callback_leave();
}

static void formal_exception_cb(liot_mqtt_client_t *client)
{
    if (!formal_callback_enter(client, NULL, false, false))
    {
        return;
    }
    liot_trace("[FORMAL-MQTT] state exception\r\n");
    s_lost = true;
    s_online = false;
    s_accept_incoming = false;
    formal_signal();
    formal_callback_leave();
}

static void formal_publish_cmd_ack(liot_mqtt_client_t *client)
{
    static const char ack[] = "{\"ack\":true}";
    int ret;

    ret = liot_mqtt_publish(client, s_ack_topic, ack,
                            (unsigned short)strlen(ack), 1, 0,
                            NULL, NULL);
    liot_trace("[FORMAL-MQTT] cmd ACK submit ret=%d\r\n", ret);
}

/* MQTT supplies a byte span, not necessarily a C string. Reject suffixes,
 * embedded NUL/escaped NUL and ambiguous envelope keys before any router
 * decides which account-bound operation the message represents. */
static cJSON *formal_parse_envelope(const unsigned char *payload, size_t length)
{
    cJSON *root;
    const cJSON *a,*b;
    const char *end=NULL;
    size_t i;
    if (!payload || !length || memchr(payload,0,length)) return NULL;
    for(i=0U;i<length;++i) {
        if(payload[i]=='\\' && i+1U<length) {
            if(length-i>=6U && !memcmp(payload+i,"\\u0000",6U))return NULL;
            ++i;
        }
    }
    root=demo_json_parse_with_length_opts((const char*)payload,length,&end,false);
    if(!cJSON_IsObject(root) || !end) { cJSON_Delete(root);return NULL; }
    for(i=(size_t)(end-(const char*)payload);i<length;++i) {
        if(payload[i]!=' ' && payload[i]!='\t' && payload[i]!='\r' && payload[i]!='\n') {
            cJSON_Delete(root);return NULL;
        }
    }
    for(a=root->child;a;a=a->next)
        for(b=a->next;b;b=b->next)
            if(a->string && b->string && !strcmp(a->string,b->string)) {
                cJSON_Delete(root);return NULL;
            }
    return root;
}

static void formal_incoming_cb(liot_mqtt_client_t *client,
                               void *arg,
                               int packet_id,
                               const char *topic,
                               const unsigned char *payload,
                               unsigned short payload_len)
{
    cJSON *root = NULL;
    const cJSON *type;
    const cJSON *channel;
    const cJSON *business_payload;
    formal_handler_slot_t handlers[FORMAL_HANDLER_MAX];
    demo_formal_mqtt_message_kind_e kind;
    uint32_t i;

    if (!formal_callback_enter(client, arg, true, true))
    {
        return;
    }
    if (!tirtc_platform_attempt_current() || topic == NULL || payload == NULL || payload_len == 0)
    {
        goto done;
    }

    if (payload_len > FORMAL_MQTT_PAYLOAD_MAX) {
        liot_trace("[FORMAL-MQTT] rejected oversize bytes=%u limit=%u\r\n",
                   (unsigned)payload_len,(unsigned)FORMAL_MQTT_PAYLOAD_MAX);
        goto done;
    }

    TIRTC_LOG_DEBUG("[FORMAL-MQTT] RX packet_id=%d topic=%s bytes=%u\r\n",
               packet_id, topic, (unsigned int)payload_len);

    if (strcmp(topic, s_cmd_topic) == 0)
    {
        /* The server treats unbind delivery as best effort, but all cmd
         * messages still follow the common ACK convention. */
        formal_publish_cmd_ack(client);
        kind = DEMO_FORMAL_MQTT_COMMAND;
    }
    else if (strcmp(topic, s_notify_topic) != 0)
    {
        liot_trace("[FORMAL-MQTT] ignored unexpected topic\r\n");
        goto done;
    }
    else
    {
        kind = DEMO_FORMAL_MQTT_NOTIFY;
    }

    root = formal_parse_envelope(payload, payload_len);
    type = (root != NULL)
               ? cJSON_GetObjectItemCaseSensitive(root, "type")
               : NULL;
    if (!cJSON_IsString(type) || type->valuestring == NULL)
    {
        liot_trace("[FORMAL-MQTT] invalid command JSON\r\n");
        goto done;
    }

    channel = cJSON_GetObjectItemCaseSensitive(root, "channel");
    business_payload = cJSON_GetObjectItemCaseSensitive(root, "payload");
#ifdef HWDEMO_GROUP_ROOM_EN
    /* Room assignment hints use top-level metadata in the server protocol. */
    if (strcmp(type->valuestring, "room_assignment_changed") == 0 &&
        !cJSON_IsObject(business_payload))
    {
        business_payload = root;
    }
#endif
    TIRTC_LOG_DEBUG("[FORMAL-MQTT] validated message received");
    if (strcmp(topic, s_cmd_topic) == 0 &&
        strcmp(type->valuestring, "unbind") == 0)
    {
        liot_rtos_enter_critical();
        s_unbind_pending = true;
        liot_rtos_exit_critical();
        formal_signal();
    }

    liot_rtos_enter_critical();
    memcpy(handlers, s_handlers, sizeof(handlers));
    liot_rtos_exit_critical();
    for (i = 0U; i < FORMAL_HANDLER_MAX; ++i)
    {
        if (handlers[i].handler != NULL)
        {
            handlers[i].handler(
                kind,
                type->valuestring,
                cJSON_IsString(channel) && channel->valuestring != NULL ?
                    channel->valuestring : "",
                cJSON_IsObject(business_payload) ? business_payload : NULL,
                handlers[i].user);
        }
    }
done:
    if (root != NULL)
    {
        cJSON_Delete(root);
    }
    formal_callback_leave();
}

static int formal_wait_connected(void)
{
    uint32_t deadline = liot_rtos_get_running_time() +
                        FORMAL_MQTT_CONNECT_MS;

    while (tirtc_platform_attempt_current() && formal_deadline_pending(deadline))
    {
        if (s_client != NULL &&
            liot_mqtt_client_state(s_client) == MQTT_CONN_CONNECTED)
        {
            return 0;
        }
        if ((s_connect_done && !s_connect_ok) || s_lost)
        {
            return -1;
        }
        liot_rtos_semaphore_wait(s_sem, 250);
    }
    return -1;
}

static int formal_subscribe(const char *topic)
{
    uint32_t deadline;
    int ret;

    s_sub_done = false;
    s_sub_ok = false;
    if (s_client == NULL || s_lost)
    {
        return -1;
    }
    ret = liot_mqtt_sub_unsub(s_client, topic, 1, NULL, NULL, 1);
    liot_trace("[FORMAL-MQTT] subscribe submit topic=%s ret=%d\r\n",
               topic, ret);
    if (ret != LIOT_MQTTCLIENT_SUCCESS && ret != LIOT_MQTTCLIENT_WOUNDBLOCK)
    {
        return -1;
    }

    deadline = liot_rtos_get_running_time() + FORMAL_MQTT_SUBSCRIBE_MS;
    while (tirtc_platform_attempt_current() && formal_deadline_pending(deadline))
    {
        if (s_lost)
        {
            return -1;
        }
        if (s_sub_done)
        {
            return s_sub_ok ? 0 : -1;
        }
        liot_rtos_semaphore_wait(s_sem, 250);
    }
    return -1;
}

static void formal_finish_current_client_locked(bool async_deinit)
{
    liot_mqtt_client_t *old_client = s_client;

    liot_rtos_enter_critical();
    s_client = NULL;
    s_callback_context = NULL;
    s_accept_incoming = false;
    s_accept_events = false;
    s_lifecycle = FORMAL_LIFECYCLE_IDLE;
    if (async_deinit)
    {
        s_retired_client = old_client;
    }
    liot_rtos_exit_critical();
    formal_reset_transport_flags();

    if (!async_deinit)
    {
        if (old_client != NULL)
        {
            *old_client = 0;
        }
        formal_clear_connection_storage();
    }
}

static int formal_cleanup_client_locked(void)
{
    uint32_t deadline;
    int state;
    int ret = LIOT_MQTTCLIENT_SUCCESS;

    if (formal_reap_retired_client_locked(true) != 0)
    {
        return -1;
    }
    if (s_client == NULL)
    {
        formal_reset_transport_flags();
        return 0;
    }

    liot_rtos_enter_critical();
    s_online = false;
    s_accept_incoming = false;
    s_lifecycle = FORMAL_LIFECYCLE_CLOSING;
    liot_rtos_exit_critical();
    if (!formal_wait_callbacks_drained(FORMAL_MQTT_CALLBACK_DRAIN_MS))
    {
        liot_trace("[FORMAL-MQTT] cleanup deferred: callback still active\r\n");
        s_lifecycle = FORMAL_LIFECYCLE_DEINIT_RETRY;
        return -1;
    }

    if (*s_client == 0)
    {
        formal_finish_current_client_locked(false);
        return 0;
    }

    state = liot_mqtt_client_state(s_client);
    TIRTC_LOG_DEBUG("[FORMAL-MQTT] cleanup begin state=%d\r\n", state);
    s_disconnect_done = false;

    if (!formal_state_is_stopped(state) &&
        state != MQTT_CONN_IS_DISCONNECTING &&
        state != MQTT_CONN_IS_CLOSING)
    {
        ret = liot_mqtt_disconnect(s_client, NULL, NULL);
        liot_trace("[FORMAL-MQTT] disconnect submit ret=%d\r\n", ret);
        if (ret != LIOT_MQTTCLIENT_SUCCESS &&
            ret != LIOT_MQTTCLIENT_WOUNDBLOCK)
        {
            s_lifecycle = FORMAL_LIFECYCLE_DEINIT_RETRY;
            return -1;
        }
    }

    state = liot_mqtt_client_state(s_client);
    if (ret == LIOT_MQTTCLIENT_WOUNDBLOCK ||
        state == MQTT_CONN_IS_DISCONNECTING ||
        state == MQTT_CONN_IS_CLOSING)
    {
        deadline = liot_rtos_get_running_time() +
                   FORMAL_MQTT_DISCONNECT_MS;
        while (formal_deadline_pending(deadline))
        {
            state = liot_mqtt_client_state(s_client);
            if (s_disconnect_done || formal_state_is_stopped(state))
            {
                break;
            }
            liot_rtos_semaphore_wait(s_sem, 250);
        }
    }

    state = liot_mqtt_client_state(s_client);
    TIRTC_LOG_DEBUG("[FORMAL-MQTT] disconnect settled state=%d done=%d\r\n",
               state, s_disconnect_done ? 1 : 0);
    if (!formal_state_is_stopped(state))
    {
        liot_trace("[FORMAL-MQTT] cleanup deferred: client still running\r\n");
        s_lifecycle = FORMAL_LIFECYCLE_DEINIT_RETRY;
        return -1;
    }

    /* Match the ordering used by Lierda's MQTT example: the disconnect
     * worker is allowed to settle before the asynchronous deinit request. */
    liot_rtos_task_sleep_ms(FORMAL_MQTT_SETTLE_MS);
    liot_rtos_enter_critical(); s_accept_events = false; liot_rtos_exit_critical();
    if (!formal_wait_callbacks_drained(FORMAL_MQTT_CALLBACK_DRAIN_MS))
    {
        liot_trace("[FORMAL-MQTT] deinit deferred: callback still active\r\n");
        s_lifecycle = FORMAL_LIFECYCLE_DEINIT_RETRY;
        return -1;
    }

    TIRTC_LOG_DEBUG("[FORMAL-MQTT] deinit begin\r\n");
    ret = liot_mqtt_client_deinit(s_client);
    liot_trace("[FORMAL-MQTT] deinit submit ret=%d\r\n", ret);
    if (ret == LIOT_MQTTCLIENT_SUCCESS || ret == LIOT_MQTTCLIENT_WOUNDBLOCK)
    {
        /* Both 0 and -2 require the actual terminal-state check. A partial
         * init can return 0 while queue/mutex resources still exist. */
        formal_finish_current_client_locked(true);
        return formal_reap_retired_client_locked(true);
    }

    /* A real submission failure keeps the handle and all callback/sync
     * storage intact.  A later disconnect/connect/maintenance call retries
     * cleanup rather than leaking the context or creating a second owner. */
    s_lifecycle = FORMAL_LIFECYCLE_DEINIT_RETRY;
    liot_trace("[FORMAL-MQTT] deinit failed; handle retained for retry\r\n");
    return -1;
}

int demo_formal_mqtt_prepare_binding(void)
{
    int ret;
    if (s_lifecycle_mutex == NULL) return 0;
    if (liot_rtos_mutex_lock(s_lifecycle_mutex, LIOT_WAIT_FOREVER) != LIOT_OSI_SUCCESS) return -1;
    ret = formal_cleanup_client_locked();
    liot_rtos_mutex_unlock(s_lifecycle_mutex);
    return ret;
}

void demo_formal_mqtt_disconnect(void)
{
    int ret;

    if (s_lifecycle_mutex == NULL)
    {
        return;
    }
    if (liot_rtos_mutex_lock(s_lifecycle_mutex, LIOT_WAIT_FOREVER) !=
        LIOT_OSI_SUCCESS)
    {
        liot_trace("[FORMAL-MQTT] cleanup lock failed\r\n");
        return;
    }
    ret = formal_cleanup_client_locked();
    liot_rtos_mutex_unlock(s_lifecycle_mutex);
    if (ret != 0)
    {
        liot_trace("[FORMAL-MQTT] cleanup pending; will retry\r\n");
    }
}

int demo_formal_mqtt_connect(const char *mqtt_url,
                             const char *device_id,
                             const char *mqtt_token)
{
    const char *log_broker;
    size_t id_len;
    size_t token_len;
    int cleanup_ret;
    int ret;

    if (!tirtc_platform_temp_mqtt_idle() || mqtt_url == NULL || device_id == NULL || mqtt_token == NULL)
    {
        return -1;
    }
    id_len = strlen(device_id);
    token_len = strlen(mqtt_token);
    if (id_len == 0 || id_len >= sizeof(s_device_id) ||
        token_len == 0 || token_len >= sizeof(s_mqtt_token))
    {
        return -1;
    }

    if (formal_ensure_sync_objects() != 0 ||
        liot_rtos_mutex_lock(s_lifecycle_mutex, LIOT_WAIT_FOREVER) !=
            LIOT_OSI_SUCCESS)
    {
        liot_trace("[FORMAL-MQTT] lifecycle sync unavailable\r\n");
        return -1;
    }
    if (formal_cleanup_client_locked() != 0)
    {
        liot_trace("[FORMAL-MQTT] previous client cleanup still pending\r\n");
        liot_rtos_mutex_unlock(s_lifecycle_mutex);
        return -1;
    }

    formal_clear_connection_storage();
    memcpy(s_device_id, device_id, id_len + 1U);
    memcpy(s_mqtt_token, mqtt_token, token_len + 1U);
    if (snprintf(s_client_id, sizeof(s_client_id), "sn_%s", device_id) >=
            (int)sizeof(s_client_id) ||
        snprintf(s_cmd_topic, sizeof(s_cmd_topic), "device/sn_%s/cmd",
                 device_id) >= (int)sizeof(s_cmd_topic) ||
        snprintf(s_notify_topic, sizeof(s_notify_topic),
                 "device/sn_%s/notify", device_id) >=
            (int)sizeof(s_notify_topic) ||
        snprintf(s_ack_topic, sizeof(s_ack_topic), "device/sn_%s/ack",
                 device_id) >= (int)sizeof(s_ack_topic) ||
        snprintf(s_up_topic, sizeof(s_up_topic), "device/sn_%s/up",
                 device_id) >= (int)sizeof(s_up_topic))
    {
        memset(s_mqtt_token, 0, sizeof(s_mqtt_token));
        liot_rtos_mutex_unlock(s_lifecycle_mutex);
        return -1;
    }

    formal_reset_transport_flags();
    s_unbind_pending = false;
    s_heartbeat_seq = 0;
    s_heartbeat_tick = liot_rtos_get_running_time();
    formal_begin_client_generation_locked();

    ret = liot_mqtt_client_init_ex(s_client, FORMAL_MQTT_PDP_CID,
                                   formal_event_cb, s_callback_context);
    if (ret != LIOT_MQTTCLIENT_SUCCESS)
    {
        liot_trace("[FORMAL-MQTT] init failed ret=%d\r\n", ret);
        goto fail;
    }

    ret = liot_mqtt_set_inpub_callback(s_client, formal_incoming_cb,
                                       s_callback_context);
    if (ret != LIOT_MQTTCLIENT_SUCCESS)
    {
        liot_trace("[FORMAL-MQTT] incoming callback setup failed ret=%d\r\n",
                   ret);
        goto fail;
    }

    memset(&s_options, 0, sizeof(s_options));
    s_options.version = LIOT_MQTT_VERSION_4;
    s_options.pdp_cid = FORMAL_MQTT_PDP_CID;
    s_options.client_id = s_client_id;
    s_options.client_user = s_device_id;
    s_options.client_pass = s_mqtt_token;
    /* The NT26/LIOT MQTT port repeatedly emits CONNECT and never completes
     * SUBACK with a persistent session.  Use the vendor-supported clean
     * session mode and explicitly restore cmd/notify after reconnect. */
    s_options.clean_session = 1;
    s_options.kalive_time = 60;
    s_options.delivery_time = 5;
    s_options.delivery_cnt = 3;
    /* Public F6D option: SDK validates 512..16384 and allocates this size.
     * 8192 holds the bounded 6144-byte JSON plus topic and MQTT framing. */
    s_options.mqtt_read_buf_len = 8192;
    s_options.ping_timeout = 5;

    /* Keep authentication fields private while identifying the real broker. */
    log_broker = (strncmp(mqtt_url, "mqtt://", 7U) == 0 &&
                  strpbrk(mqtt_url + 7U, "/@?#\r\n") == NULL)
                     ? mqtt_url : "<broker URL redacted>";
    liot_trace("[FORMAL-MQTT] connecting broker=%s cid=%d "
               "client_len=%u token_len=%u\r\n",
               log_broker, FORMAL_MQTT_PDP_CID,
               (unsigned int)strlen(s_client_id), (unsigned int)token_len);
    ret = liot_mqtt_connect(s_client, mqtt_url, NULL, NULL,
                            &s_options, formal_exception_cb);
    liot_trace("[FORMAL-MQTT] connect submit ret=%d "
               "(0=accepted,-2=queued; wait for connect status)\r\n", ret);
    if ((ret != LIOT_MQTTCLIENT_SUCCESS &&
         ret != LIOT_MQTTCLIENT_WOUNDBLOCK) ||
        formal_wait_connected() != 0 ||
        formal_subscribe(s_cmd_topic) != 0 ||
        formal_subscribe(s_notify_topic) != 0)
    {
        liot_trace("[FORMAL-MQTT] connect/subscribe failed ret=%d "
                   "broker=%s\r\n", ret, log_broker);
        goto fail;
    }

    liot_rtos_enter_critical();
    /* SUBACK can be followed by DISCONNECT before the owner runs again.
     * Never overwrite the callback's offline state when committing online. */
    if (s_lost)
    {
        liot_rtos_exit_critical();
        goto fail;
    }
    s_subscriptions_ready = true;
    s_resubscribe_pending = false;
    s_online = true;
    s_accept_incoming = true;
    s_lifecycle = FORMAL_LIFECYCLE_ACTIVE;
    liot_rtos_exit_critical();
    liot_trace("[FORMAL-MQTT] online; cmd+notify subscribed broker=%s\r\n",
               log_broker);
    liot_rtos_mutex_unlock(s_lifecycle_mutex);
    return 0;

fail:
    cleanup_ret = formal_cleanup_client_locked();
    liot_rtos_mutex_unlock(s_lifecycle_mutex);
    if (cleanup_ret != 0)
    {
        liot_trace("[FORMAL-MQTT] failed client retained for cleanup retry\r\n");
    }
    return -1;
}

bool demo_formal_mqtt_is_online(void)
{
    bool online;

    /* Do not query the opaque LIOT handle here: this accessor is called by
     * feature tasks while the lifecycle owner may be retiring that handle. */
    liot_rtos_enter_critical();
    online = s_online && s_client != NULL &&
             s_lifecycle == FORMAL_LIFECYCLE_ACTIVE;
    liot_rtos_exit_critical();
    return online;
}

bool demo_formal_mqtt_take_unbind(void)
{
    bool pending;

    liot_rtos_enter_critical();
    pending = s_unbind_pending;
    s_unbind_pending = false;
    liot_rtos_exit_critical();
    return pending;
}

bool demo_formal_mqtt_copy_token(char *output, size_t output_size)
{
    size_t length;

    if (output == NULL || output_size == 0)
    {
        return false;
    }
    output[0] = '\0';

    liot_rtos_enter_critical();
    if (!s_online || s_client == NULL ||
        s_lifecycle != FORMAL_LIFECYCLE_ACTIVE)
    {
        liot_rtos_exit_critical();
        return false;
    }
    length = strlen(s_mqtt_token);
    if (length == 0 || length >= output_size)
    {
        liot_rtos_exit_critical();
        return false;
    }
    memcpy(output, s_mqtt_token, length + 1U);
    liot_rtos_exit_critical();
    return true;
}

int demo_formal_mqtt_register_handler(
    demo_formal_mqtt_message_handler_t handler,
    void *user)
{
    uint32_t i;
    int free_slot = -1;

    if (handler == NULL)
    {
        return -1;
    }
    liot_rtos_enter_critical();
    for (i = 0U; i < FORMAL_HANDLER_MAX; ++i)
    {
        if (s_handlers[i].handler == handler && s_handlers[i].user == user)
        {
            liot_rtos_exit_critical();
            return 0;
        }
        if (free_slot < 0 && s_handlers[i].handler == NULL)
        {
            free_slot = (int)i;
        }
    }
    if (free_slot >= 0)
    {
        s_handlers[free_slot].handler = handler;
        s_handlers[free_slot].user = user;
    }
    liot_rtos_exit_critical();
    return free_slot >= 0 ? 0 : -1;
}

void demo_formal_mqtt_unregister_handler(
    demo_formal_mqtt_message_handler_t handler,
    void *user)
{
    uint32_t i;

    liot_rtos_enter_critical();
    for (i = 0U; i < FORMAL_HANDLER_MAX; ++i)
    {
        if (s_handlers[i].handler == handler && s_handlers[i].user == user)
        {
            memset(&s_handlers[i], 0, sizeof(s_handlers[i]));
        }
    }
    liot_rtos_exit_critical();
}

void demo_formal_mqtt_maintenance(void)
{
    char payload[96];
    uint32_t now;
    bool was_online;
    int length;
    int ret;
    int native_state;

    if (s_lifecycle_mutex == NULL ||
        liot_rtos_mutex_lock(s_lifecycle_mutex, LIOT_WAIT_FOREVER) !=
            LIOT_OSI_SUCCESS)
    {
        return;
    }
    if (s_lifecycle == FORMAL_LIFECYCLE_DEINIT_RETRY)
    {
        (void)formal_cleanup_client_locked();
        goto done;
    }
    if (s_lifecycle == FORMAL_LIFECYCLE_ACTIVE && s_client != NULL)
    {
        /* F6D_A can exhaust its internal reconnect count and retire its
         * static context without CLOSE/exception callbacks. Only this owner
         * holds the lifecycle mutex and may inspect that native handle.
         * Opening/connecting/reconnecting are transient, not terminal. */
        native_state = liot_mqtt_client_state(s_client);
        if (native_state < 0 || formal_state_is_stopped(native_state))
        {
            liot_rtos_enter_critical();
            s_lost = true;
            s_online = false;
            s_accept_incoming = false;
            liot_rtos_exit_critical();
            liot_trace("[FORMAL-MQTT] native transport stopped state=%d; retiring\r\n",
                       native_state);
            /* Retain options and callback storage until the existing exact
             * completion guard proves the SDK no longer references them. */
            (void)formal_cleanup_client_locked();
            goto done;
        }
    }

    if (s_resubscribe_pending && s_client != NULL &&
        s_lifecycle == FORMAL_LIFECYCLE_ACTIVE &&
        liot_mqtt_client_state(s_client) == MQTT_CONN_CONNECTED)
    {
        now = liot_rtos_get_running_time();
        if (s_resubscribe_retry_at != 0U &&
            formal_deadline_pending(s_resubscribe_retry_at))
        {
            goto done;
        }
        liot_rtos_enter_critical();
        was_online = s_online;
        s_resubscribe_pending = false;
        liot_rtos_exit_critical();
        liot_trace("[FORMAL-MQTT] restoring cmd+notify subscriptions\r\n");
        if (formal_subscribe(s_cmd_topic) == 0 &&
            formal_subscribe(s_notify_topic) == 0 && tirtc_platform_attempt_current())
        {
            liot_rtos_enter_critical();
            if (s_lost)
            {
                s_online = false;
                s_accept_incoming = false;
                s_resubscribe_pending = false;
                s_resubscribe_retry_at = 0U;
                liot_rtos_exit_critical();
                goto done;
            }
            s_subscriptions_ready = true;
            s_resubscribe_pending = false;
            s_resubscribe_retry_at = 0U;
            s_accept_incoming = true;
            s_online = true;
            liot_rtos_exit_critical();
            liot_trace("[FORMAL-MQTT] subscriptions restored\r\n");
        }
        else
        {
            liot_trace("[FORMAL-MQTT] subscription restore failed\r\n");
            liot_rtos_enter_critical();
            /* If the old connection was still usable, retain it and retry;
             * one delayed SUBACK must not tear down binding and every feature.
             * A real disconnect already set s_online=false and is recovered
             * by demo_binding_task through the existing token flow. */
            s_online = was_online && s_online && !s_lost;
            if (!s_online) s_accept_incoming = false;
            s_resubscribe_pending = s_online;
            s_resubscribe_retry_at = s_online
                ? liot_rtos_get_running_time() + FORMAL_MQTT_RESUB_RETRY_MS
                : 0U;
            liot_rtos_exit_critical();
            goto done;
        }
    }

    if (!demo_formal_mqtt_is_online())
    {
        goto done;
    }
    now = liot_rtos_get_running_time();
    if ((int32_t)(now - s_heartbeat_tick) <
        (int32_t)FORMAL_MQTT_HEARTBEAT_MS)
    {
        goto done;
    }
    s_heartbeat_tick = now;
    s_heartbeat_seq++;
    length = snprintf(payload, sizeof(payload),
                      "{\"type\":\"heartbeat\",\"seq\":%u}",
                      (unsigned int)s_heartbeat_seq);
    if (length <= 0 || length >= (int)sizeof(payload))
    {
        goto done;
    }
    ret = liot_mqtt_publish(s_client, s_up_topic, payload,
                            (unsigned short)length, 0, 0, NULL, NULL);
    if (ret == LIOT_MQTTCLIENT_WOUNDBLOCK)
    {
        /* In this SDK -2 means the request was queued to the MQTT worker. */
        TIRTC_LOG_DEBUG("[FORMAL-MQTT] heartbeat seq=%u queued\r\n",
                   (unsigned int)s_heartbeat_seq);
    }
    else
    {
        liot_trace("[FORMAL-MQTT] heartbeat seq=%u ret=%d\r\n",
                   (unsigned int)s_heartbeat_seq, ret);
    }

done:
    liot_rtos_mutex_unlock(s_lifecycle_mutex);
}
