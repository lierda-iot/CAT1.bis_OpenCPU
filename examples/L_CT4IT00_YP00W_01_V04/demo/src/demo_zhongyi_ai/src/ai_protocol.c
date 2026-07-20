/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ai_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "liot_rtc.h"

static const cJSON *ai_protocol_get_item(const cJSON *object, const char *name)
{
    return cJSON_GetObjectItemCaseSensitive(object, name);
}

static const char *ai_protocol_get_string(const cJSON *object, const char *name)
{
    const cJSON *item = ai_protocol_get_item(object, name);
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        return item->valuestring;
    }
    return NULL;
}

static void ai_protocol_copy_string(const char *src, char *dst, size_t dst_size)
{
    if ((dst == NULL) || (dst_size == 0U)) {
        return;
    }

    dst[0] = '\0';
    if (src != NULL) {
        snprintf(dst, dst_size, "%s", src);
    }
}

static cJSON *ai_protocol_parse(const char *json, size_t len)
{
    if ((json == NULL) || (len == 0U)) {
        return NULL;
    }

    return cJSON_ParseWithLength(json, len);
}

static bool ai_protocol_type_or_state_is(const char *json, size_t len, const char *expected)
{
    cJSON *root = NULL;
    const char *type = NULL;
    const char *state = NULL;
    bool matched = false;

    root = ai_protocol_parse(json, len);
    if (root == NULL) {
        return false;
    }

    type = ai_protocol_get_string(root, "type");
    state = ai_protocol_get_string(root, "state");
    matched = ((type != NULL) && (strcmp(type, expected) == 0)) ||
              ((state != NULL) && (strcmp(state, expected) == 0));

    cJSON_Delete(root);
    return matched;
}

bool ai_protocol_extract_session_id(const char *json,
                                    size_t len,
                                    char *session_id,
                                    size_t session_id_size)
{
    cJSON *root = NULL;
    const cJSON *data = NULL;
    const char *type = NULL;
    const char *id = NULL;

    if ((session_id == NULL) || (session_id_size == 0U)) {
        return false;
    }
    session_id[0] = '\0';

    root = ai_protocol_parse(json, len);
    if (root == NULL) {
        return false;
    }

    type = ai_protocol_get_string(root, "type");
    if ((type != NULL) && (strcmp(type, "connection") != 0)) {
        cJSON_Delete(root);
        return false;
    }

    id = ai_protocol_get_string(root, "session_id");
    if (id == NULL) {
        id = ai_protocol_get_string(root, "sessionId");
    }

    if (id == NULL) {
        data = ai_protocol_get_item(root, "data");
        if (cJSON_IsObject(data)) {
            id = ai_protocol_get_string(data, "session_id");
            if (id == NULL) {
                id = ai_protocol_get_string(data, "sessionId");
            }
        }
    }

    if ((id != NULL) && (id[0] != '\0')) {
        ai_protocol_copy_string(id, session_id, session_id_size);
        cJSON_Delete(root);
        return true;
    }

    cJSON_Delete(root);
    return false;
}

bool ai_protocol_is_hello_ack(const char *json, size_t len)
{
    return ai_protocol_type_or_state_is(json, len, "hello") ||
           ai_protocol_type_or_state_is(json, len, "hello_ack");
}

bool ai_protocol_is_pong(const char *json, size_t len)
{
    return ai_protocol_type_or_state_is(json, len, "pong") ||
           ai_protocol_type_or_state_is(json, len, "ping");
}

bool ai_protocol_extract_error(const char *json,
                               size_t len,
                               char *state,
                               size_t state_size,
                               char *message,
                               size_t message_size,
                               int *code)
{
    cJSON *root = NULL;
    const char *type = NULL;
    const cJSON *code_item = NULL;

    if (state != NULL && state_size > 0U) {
        state[0] = '\0';
    }
    if (message != NULL && message_size > 0U) {
        message[0] = '\0';
    }
    if (code != NULL) {
        *code = 0;
    }

    root = ai_protocol_parse(json, len);
    if (root == NULL) {
        return false;
    }

    type = ai_protocol_get_string(root, "type");
    if ((type == NULL) || (strcmp(type, "error") != 0)) {
        cJSON_Delete(root);
        return false;
    }

    ai_protocol_copy_string(ai_protocol_get_string(root, "state"), state, state_size);
    ai_protocol_copy_string(ai_protocol_get_string(root, "message"), message, message_size);

    code_item = ai_protocol_get_item(root, "code");
    if (code != NULL) {
        if (cJSON_IsNumber(code_item)) {
            *code = code_item->valueint;
        } else if (cJSON_IsString(code_item) && code_item->valuestring != NULL) {
            *code = atoi(code_item->valuestring);
        }
    }

    cJSON_Delete(root);
    return true;
}

char *ai_protocol_build_hello(const char *session_id, const char *trace_id)
{
    cJSON *root = NULL;
    cJSON *audio = NULL;
    char *json = NULL;

    if ((session_id == NULL) || (trace_id == NULL)) {
        return NULL;
    }

    root = cJSON_CreateObject();
    audio = cJSON_CreateObject();
    if ((root == NULL) || (audio == NULL)) {
        goto exit;
    }

    if (!cJSON_AddStringToObject(root, "type", "hello") ||
        !cJSON_AddNumberToObject(root, "version", 1) ||
        !cJSON_AddStringToObject(root, "transport", "websocket") ||
        !cJSON_AddNumberToObject(audio, "sample_rate", 16000) ||
        !cJSON_AddNumberToObject(audio, "channels", 1) ||
        !cJSON_AddStringToObject(audio, "format", "opus") ||
        !cJSON_AddNumberToObject(audio, "frame_duration", 60) ||
        !cJSON_AddStringToObject(audio, "down_format", "opus")) {
        goto exit;
    }
    if (!cJSON_AddItemToObject(root, "audio_params", audio)) {
        goto exit;
    }
    audio = NULL;

    if (!cJSON_AddStringToObject(root, "session_id", session_id) ||
        !cJSON_AddStringToObject(root, "wake_type", "1") ||
        !cJSON_AddStringToObject(root, "wake_id", "") ||
        !cJSON_AddStringToObject(root, "msg_id", "") ||
        !cJSON_AddStringToObject(root, "trace_id", trace_id)) {
        goto exit;
    }

    json = cJSON_PrintUnformatted(root);

exit:
    if (audio != NULL) {
        cJSON_Delete(audio);
    }
    if (root != NULL) {
        cJSON_Delete(root);
    }
    return json;
}

char *ai_protocol_build_ping(const char *session_id)
{
    cJSON *root = NULL;
    uint64_t timestamp = 0;
    char *json = NULL;

    if (session_id == NULL) {
        return NULL;
    }

    if (Liot_GetTimestamp(&timestamp) != 0 || timestamp == 0ULL) {
        timestamp = 0ULL;
    }

    root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "ping") ||
        !cJSON_AddStringToObject(root, "session_id", session_id) ||
        !cJSON_AddNumberToObject(root, "timestamp", (double)timestamp)) {
        cJSON_Delete(root);
        return NULL;
    }

    json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

bool ai_protocol_is_tts_start(const char *json, size_t len)
{
    cJSON *root = NULL;
    const char *type = NULL;
    const char *state = NULL;
    bool matched = false;

    root = ai_protocol_parse(json, len);
    if (root == NULL) {
        return false;
    }

    type = ai_protocol_get_string(root, "type");
    state = ai_protocol_get_string(root, "state");
    matched = (type != NULL && strcmp(type, "tts") == 0 &&
               state != NULL && strcmp(state, "start") == 0);

    cJSON_Delete(root);
    return matched;
}

bool ai_protocol_is_tts_stop(const char *json, size_t len)
{
    cJSON *root = NULL;
    const char *type = NULL;
    const char *state = NULL;
    bool matched = false;

    root = ai_protocol_parse(json, len);
    if (root == NULL) {
        return false;
    }

    type = ai_protocol_get_string(root, "type");
    state = ai_protocol_get_string(root, "state");
    matched = (type != NULL && strcmp(type, "tts") == 0 &&
               state != NULL && strcmp(state, "stop") == 0);

    cJSON_Delete(root);
    return matched;
}

bool ai_protocol_is_stt_end(const char *json, size_t len)
{
    if (json == NULL || len == 0) {
        return false;
    }
    return (strstr(json, "\"type\"") != NULL &&
            strstr(json, "\"stt\"") != NULL &&
            strstr(json, "\"end\"") != NULL);
}

char *ai_protocol_build_wake(const char *session_id, const char *wake_word)
{
    cJSON *root = NULL;
    char *json = NULL;

    if (session_id == NULL || wake_word == NULL) {
        return NULL;
    }

    root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "listen") ||
        !cJSON_AddStringToObject(root, "state", "wake") ||
        !cJSON_AddStringToObject(root, "text", wake_word) ||
        !cJSON_AddStringToObject(root, "session_id", session_id) ||
        !cJSON_AddBoolToObject(root, "wake_reply", 0)) {
        cJSON_Delete(root);
        return NULL;
    }

    json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

char *ai_protocol_build_listen_start(const char *session_id, const char *mode)
{
    cJSON *root = NULL;
    char *json = NULL;

    if (session_id == NULL || mode == NULL) {
        return NULL;
    }

    root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "listen") ||
        !cJSON_AddStringToObject(root, "state", "start") ||
        !cJSON_AddStringToObject(root, "mode", mode) ||
        !cJSON_AddStringToObject(root, "session_id", session_id)) {
        cJSON_Delete(root);
        return NULL;
    }

    json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

char *ai_protocol_build_listen_stop(const char *session_id)
{
    cJSON *root = NULL;
    char *json = NULL;

    if (session_id == NULL) {
        return NULL;
    }

    root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "listen") ||
        !cJSON_AddStringToObject(root, "state", "stop") ||
        !cJSON_AddStringToObject(root, "session_id", session_id)) {
        cJSON_Delete(root);
        return NULL;
    }

    json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

void ai_protocol_free_json(char *json)
{
    if (json != NULL) {
        cJSON_free(json);
    }
}
