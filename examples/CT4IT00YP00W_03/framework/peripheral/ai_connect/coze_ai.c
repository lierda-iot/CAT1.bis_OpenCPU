#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "liot_type.h"
#include "libwebsockets.h"
#include "cJSON.h"
#include "mbedtls/base64.h"

#include "liot_os.h"
#include "aiLog.h"

#include "coze_ai.h"
#include "ws_ai.h"
#include "app_nv.h"
#include "hw_audio.h"
#include "hw_location.h"
#include "frameworkTypes.h"
#include "frameworkCore.h"
#include "coze_action_tools.h"

#define WS_AI_INPUT_AUDIO_FORMAT        "pcm"
#define WS_AI_INPUT_AUDIO_CODEC         "opus"
#define WS_AI_INPUT_AUDIO_SAMPLE_RATE   16000

#define WS_AI_OUTPUT_AUDIO_CODEC        "opus"
#define WS_AI_OUTPUT_AUDIO_BITRATE      16000
#define WS_AI_OUTPUT_AUDIO_FRAME_SIZE   40
#define WS_AI_OUTPUT_AUDIO_USE_CBR      false
#define WS_AI_OUTPUT_AUDIO_PERIOD       1
#define WS_AI_OUTPUT_AUDIO_MAX_FRAMES   ((WS_AI_OUTPUT_AUDIO_PERIOD * 1000 / WS_AI_OUTPUT_AUDIO_FRAME_SIZE))

#define AI_APP_VERSION  AI_APP_MAJOR "." AI_APP_MINOR "." AI_APP_PATCH

static const char *g_subscribe_events[] = {
    "chat.created", "chat.updated", "input_audio_buffer.completed",
    "conversation.chat.created", "conversation.audio.delta",
    "conversation.audio.completed", "conversation.chat.canceled",
    "conversation.chat.requires_action"
};
#define SUBSCRIBE_EVENT_COUNT (sizeof(g_subscribe_events) / sizeof(g_subscribe_events[0]))

static uint8_t g_cancel_flag = 0;

extern void ws_ai_connect_done_release(void);
extern void ws_ai_transcript_release(void);

static bool find_json_string_value(const char *json, const char *key,
                                   char **value_start, uint32_t *value_len)
{
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);

    char *pos = strstr(json, pattern);
    if (!pos) {
        return false;
    }

    pos += strlen(pattern);
    while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r') {
        pos++;
    }

    if (*pos != '"') {
        return false;
    }
    pos++;
    *value_start = pos;

    char *end = strchr(pos, '"');
    if (!end) {
        return false;
    }
    *value_len = (uint32_t)(end - pos);
    return true;
}

static int get_json_event_type(const char *json, char *out, uint32_t out_size)
{
    char *start = NULL;
    uint32_t len = 0;
    if (!find_json_string_value(json, "event_type", &start, &len)) {
        return -1;
    }
    if (len >= out_size) {
        len = out_size - 1;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return 0;
}

static void post_framework_event(eventId_E evtId)
{
    event_t evt = {
        .eventId = evtId,
        .arg1 = 0U,
        .arg2 = 0U,
        .data = NULL,
        .ownerJobId = 0U,
    };
    frameworkPostEvent(&evt);
}

static void handle_audio_delta(uint8_t *data, uint32_t len)
{
    char *content_start = NULL;
    uint32_t content_len = 0;

    if (!find_json_string_value((char *)data, "content", &content_start, &content_len)) {
        LOG_WARN("[COZE] audio delta: no content field");
        return;
    }

    size_t decoded_len = 0;
    uint8_t *decoded = liot_rtos_malloc(content_len);
    if (decoded == NULL) {
        return;
    }

    int ret = mbedtls_base64_decode(decoded, content_len, &decoded_len,
                                    (const unsigned char *)content_start, content_len);
    if (ret != 0) {
        liot_rtos_free(decoded);
        LOG_WARN("[COZE] base64 decode failed: %d", ret);
        return;
    }

    static uint32_t s_delta_count = 0;
    s_delta_count++;
    if (s_delta_count <= 3 || s_delta_count % 50 == 0)
        LOG_INFO("[COZE] audio delta #%u, decoded=%u bytes", s_delta_count, (unsigned)decoded_len);

    audioModuleWritePlayback(decoded, (uint32_t)decoded_len);
    liot_rtos_free(decoded);
}

int coze_ai_recv_message(uint8_t *data, uint32_t len)
{
    char event_type[64] = {0};

    if (get_json_event_type((char *)data, event_type, sizeof(event_type)) != 0) {
        LOG_WARN("coze recv: no event_type");
        return -1;
    }

    if (strcmp(event_type, "chat.created") == 0) {
        LOG_INFO("[COZE] chat.created");
        ws_ai_connect_done_release();
    } else if (strcmp(event_type, "chat.updated") == 0) {
        LOG_INFO("[COZE] chat.updated");
        ws_ai_connect_done_release();
    } else if (strcmp(event_type, "conversation.chat.created") == 0) {
        LOG_INFO("[COZE] conversation.chat.created (AI thinking)");
        post_framework_event(EVT_AI_RESPONSE_THINK);
        ws_ai_transcript_release();
    } else if (strcmp(event_type, "input_audio_buffer.completed") == 0) {
        LOG_INFO("[COZE] input_audio_buffer.completed");
        g_cancel_flag = 0;
    } else if (strcmp(event_type, "conversation.audio.delta") == 0) {
        if (!g_cancel_flag) {
            handle_audio_delta(data, len);
        }
    } else if (strcmp(event_type, "conversation.audio.completed") == 0) {
        LOG_INFO("[COZE] audio.completed → post EVT_AI_RESPONSE_DONE");
        post_framework_event(EVT_AI_RESPONSE_DONE);
    } else if (strcmp(event_type, "conversation.chat.canceled") == 0) {
        LOG_INFO("[COZE] chat.canceled");
        post_framework_event(EVT_AI_RESPONSE_DONE);
    } else if (strcmp(event_type, "error") == 0) {
        LOG_ERROR("[COZE] error event received");
        post_framework_event(EVT_AI_CONNECT_FAIL);
    } else if (strcmp(event_type, "conversation.chat.requires_action") == 0) {
        LOG_INFO("[COZE] requires_action");
        coze_action_tools_handle(data, len);
    }

    return 0;
}

int coze_ai_chat_update(const char *imei, const char *voiceid)
{
    location_info_t loc = {0};
    int loc_ok = locationModuleGetPosition(&loc);

    cJSON *root = cJSON_CreateObject();
    cJSON *data_obj = cJSON_CreateObject();
    cJSON *input_audio = cJSON_CreateObject();
    cJSON *output_audio = cJSON_CreateObject();
    cJSON *opus_config = cJSON_CreateObject();
    cJSON *limit_config = cJSON_CreateObject();
    cJSON *chat_config = cJSON_CreateObject();
    cJSON *parameters = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "id", imei);
    cJSON_AddStringToObject(root, "event_type", "chat.update");
    cJSON_AddItemToObject(root, "data", data_obj);

    cJSON_AddItemToObject(data_obj, "input_audio", input_audio);
    cJSON_AddStringToObject(input_audio, "format", WS_AI_INPUT_AUDIO_FORMAT);
    cJSON_AddStringToObject(input_audio, "codec", WS_AI_INPUT_AUDIO_CODEC);
    cJSON_AddNumberToObject(input_audio, "sample_rate", WS_AI_INPUT_AUDIO_SAMPLE_RATE);

    cJSON_AddItemToObject(data_obj, "output_audio", output_audio);
    cJSON_AddStringToObject(output_audio, "codec", WS_AI_OUTPUT_AUDIO_CODEC);
    cJSON_AddStringToObject(output_audio, "voice_id", voiceid);
    cJSON_AddItemToObject(output_audio, "opus_config", opus_config);
    cJSON_AddNumberToObject(opus_config, "bitrate", WS_AI_OUTPUT_AUDIO_BITRATE);
    cJSON_AddNumberToObject(opus_config, "frame_size_ms", WS_AI_OUTPUT_AUDIO_FRAME_SIZE);
    cJSON_AddBoolToObject(opus_config, "use_cbr", WS_AI_OUTPUT_AUDIO_USE_CBR);
    cJSON_AddItemToObject(opus_config, "limit_config", limit_config);
    cJSON_AddNumberToObject(limit_config, "period", WS_AI_OUTPUT_AUDIO_PERIOD);
    cJSON_AddNumberToObject(limit_config, "max_frame_num", WS_AI_OUTPUT_AUDIO_MAX_FRAMES);

    cJSON_AddItemToObject(data_obj, "chat_config", chat_config);
    cJSON_AddStringToObject(chat_config, "user_id", imei);
    cJSON_AddItemToObject(chat_config, "parameters", parameters);
    cJSON_AddStringToObject(parameters, "imei", imei);
    cJSON_AddStringToObject(parameters, "type", "4g");
    cJSON_AddStringToObject(parameters, "version", AI_APP_VERSION);
    cJSON_AddStringToObject(parameters, "hardware", AI_HARDWARE_VERSION);

    if (loc_ok == 0) {
        cJSON_AddStringToObject(parameters, "loc", loc.desc);
        cJSON *extra_params = cJSON_CreateObject();
        cJSON_AddStringToObject(extra_params, "longitude", loc.longitude);
        cJSON_AddStringToObject(extra_params, "latitude", loc.latitude);
        cJSON_AddItemToObject(chat_config, "extra_params", extra_params);
    }

    cJSON *events = cJSON_CreateArray();
    for (uint32_t i = 0; i < SUBSCRIBE_EVENT_COUNT; i++) {
        cJSON_AddItemToArray(events, cJSON_CreateString(g_subscribe_events[i]));
    }
    cJSON_AddItemToObject(data_obj, "event_subscriptions", events);

    char *json_str = cJSON_PrintUnformatted(root);
    int ret = ws_ai_send_raw((uint8_t *)json_str, (uint32_t)strlen(json_str),
                             LWS_WRITE_TEXT, false);

    LOG_INFO("chat_update: %s", json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
    return ret;
}

int coze_ai_chat_upload_audio(const char *imei, uint8_t *data, uint32_t len)
{
    size_t base64_len = 0;
    size_t buf_size = len * 4 / 3 + 4;
    uint8_t *base64_buf = liot_rtos_malloc(buf_size);
    if (base64_buf == NULL) {
        LOG_WARN("[COZE] upload_audio malloc failed, size=%u", (unsigned)buf_size);
        return WS_AI_NO_MEMORY;
    }

    mbedtls_base64_encode(base64_buf, buf_size, &base64_len, data, len);

    cJSON *root = cJSON_CreateObject();
    cJSON *rdata = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "id", imei);
    cJSON_AddStringToObject(root, "event_type", "input_audio_buffer.append");
    cJSON_AddItemToObject(root, "data", rdata);
    cJSON_AddStringToObject(rdata, "delta", (char *)base64_buf);

    char *json_str = cJSON_PrintUnformatted(root);
    int ret = ws_ai_send_raw((uint8_t *)json_str, (uint32_t)strlen(json_str),
                             LWS_WRITE_TEXT, true);

    liot_rtos_free(base64_buf);
    cJSON_free(json_str);
    cJSON_Delete(root);
    return ret;
}

int coze_ai_chat_upload_complete(const char *imei)
{
    LOG_INFO("[COZE] send input_audio_buffer.complete");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "id", imei);
    cJSON_AddStringToObject(root, "event_type", "input_audio_buffer.complete");

    char *json_str = cJSON_PrintUnformatted(root);
    int ret = ws_ai_send_raw((uint8_t *)json_str, (uint32_t)strlen(json_str),
                             LWS_WRITE_TEXT, true);

    cJSON_free(json_str);
    cJSON_Delete(root);
    return ret;
}

int coze_ai_chat_cancel(const char *imei)
{
    LOG_INFO("[COZE] send chat.cancel");
    g_cancel_flag = 1;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "id", imei);
    cJSON_AddStringToObject(root, "event_type", "conversation.chat.cancel");

    char *json_str = cJSON_PrintUnformatted(root);
    int ret = ws_ai_send_raw((uint8_t *)json_str, (uint32_t)strlen(json_str),
                             LWS_WRITE_TEXT, true);

    cJSON_free(json_str);
    cJSON_Delete(root);
    return ret;
}
