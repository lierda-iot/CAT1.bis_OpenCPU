#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "cJSON.h"
#include "liot_os.h"
#include "liot_log.h"
#include "liot_dev.h"
#include "coze_action_tools.h"
#include "ws_ai.h"
#include "hw_audio.h"

typedef char *(*tools_func_t)(void *arg);

typedef struct {
    const char *name;
    tools_func_t func;
} tools_entry_t;

static char *action_volume_adjust(void *arg);

static const tools_entry_t g_tools_list[] = {
    {"VolumeAdjust", action_volume_adjust},
};
#define TOOLS_LIST_SIZE (sizeof(g_tools_list) / sizeof(g_tools_list[0]))

/* ---- helpers ---- */

static int extract_number(const char *s)
{
    int number = 0, sign = 1, i = 0;
    while (s[i] && !isdigit((int)s[i]) && s[i] != '-') i++;
    if (s[i] == '-') { sign = -1; i++; }
    while (s[i] && isdigit((int)s[i])) { number = number * 10 + (s[i] - '0'); i++; }
    return sign * number;
}

static void remove_quotes(char *str)
{
    if (!str) return;
    size_t len = strlen(str);
    if (len >= 2 && str[0] == '"' && str[len - 1] == '"') {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
    }
}

static void unescape_quotes(char *str)
{
    if (!str) return;
    char *r = str, *w = str;
    while (*r) {
        if (*r == '\\' && *(r + 1) == '"') { *w++ = '"'; r += 2; }
        else { *w++ = *r++; }
    }
    *w = '\0';
}

static void submit_tool_reply(const char *imei, const char *chat_id,
                              const char *tool_call_id, const char *result)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *rdata = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "id", imei);
    cJSON_AddStringToObject(root, "event_type", "conversation.chat.submit_tool_outputs");
    cJSON_AddItemToObject(root, "data", rdata);
    cJSON_AddStringToObject(rdata, "chat_id", chat_id);

    cJSON *outputs = cJSON_AddArrayToObject(rdata, "tool_outputs");
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "tool_call_id", tool_call_id);
    cJSON_AddStringToObject(item, "output", result);
    cJSON_AddItemToArray(outputs, item);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        ws_ai_send_raw((uint8_t *)json_str, strlen(json_str), 1, true);
        cJSON_free(json_str);
    }
    cJSON_Delete(root);
}

/* ---- VolumeAdjust ---- */

static char *action_volume_adjust(void *arg)
{
    uint32_t volume_value = audioModuleVolumeGet();
    cJSON *root = cJSON_Parse((char *)arg);
    if (!root) return NULL;

    cJSON *mode_item = cJSON_GetObjectItem(root, "VolMode");
    if (mode_item && cJSON_IsNumber(mode_item)) {
        int mode = mode_item->valueint;
        int cur = (int)volume_value;

        if (mode == 0) {
            cJSON *adj = cJSON_GetObjectItem(root, "CAdjust");
            if (adj && cJSON_IsNumber(adj)) {
                if (adj->valueint == 1) cur += 20;
                else if (adj->valueint == -1) cur -= 20;
            }
        } else if (mode == 1) {
            cJSON *adj = cJSON_GetObjectItem(root, "PAdjust");
            if (adj && cJSON_IsString(adj)) {
                int val = extract_number(adj->valuestring);
                if (strstr(adj->valuestring, "%"))
                    cur += (cur * val) / 100;
                else
                    cur += val;
            }
        } else if (mode == 2) {
            cJSON *set = cJSON_GetObjectItem(root, "VolSet");
            if (set && cJSON_IsNumber(set)) cur = set->valueint;
        }

        if (cur > 100) cur = 100;
        if (cur < 0) cur = 0;
        volume_value = (uint32_t)cur;
        audioModuleVolumeSet(volume_value);
        liot_trace("[TOOLS] volume set to %d", volume_value);
    }
    cJSON_Delete(root);

    char *rsp = liot_rtos_malloc(32);
    if (rsp) sprintf(rsp, "{\"VolNow\":%lu}", (unsigned long)volume_value);
    return rsp;
}

/* ---- public entry ---- */

void coze_action_tools_handle(uint8_t *data, uint32_t len)
{
    (void)len;
    cJSON *root = cJSON_Parse((char *)data);
    if (!root) return;

    char imei[16] = {0};
    char chat_id[64] = {0};
    char tool_call_id[64] = {0};

    cJSON *data_obj = cJSON_GetObjectItem(root, "data");
    if (!data_obj) { cJSON_Delete(root); return; }

    cJSON *cid = cJSON_GetObjectItem(data_obj, "chat_id");
    if (cid && cJSON_IsString(cid))
        strncpy(chat_id, cid->valuestring, sizeof(chat_id) - 1);

    cJSON *action = cJSON_GetObjectItem(data_obj, "required_action");
    if (!action) { cJSON_Delete(root); return; }

    cJSON *submit = cJSON_GetObjectItem(action, "submit_tool_outputs");
    if (!submit) { cJSON_Delete(root); return; }

    cJSON *calls = cJSON_GetObjectItem(submit, "tool_calls");
    if (!calls || !cJSON_IsArray(calls)) { cJSON_Delete(root); return; }

    liot_dev_get_imei(imei, sizeof(imei), 0);

    int count = cJSON_GetArraySize(calls);
    for (int i = 0; i < count; i++) {
        cJSON *call = cJSON_GetArrayItem(calls, i);
        cJSON *id_item = cJSON_GetObjectItem(call, "id");
        if (id_item && cJSON_IsString(id_item))
            strncpy(tool_call_id, id_item->valuestring, sizeof(tool_call_id) - 1);

        cJSON *func = cJSON_GetObjectItem(call, "function");
        if (!func) continue;

        cJSON *name = cJSON_GetObjectItem(func, "name");
        cJSON *arguments = cJSON_GetObjectItem(func, "arguments");
        if (!name || !cJSON_IsString(name) || !arguments || !cJSON_IsString(arguments))
            continue;

        char *name_str = cJSON_PrintUnformatted(name);
        char *arg_str = cJSON_PrintUnformatted(arguments);
        remove_quotes(name_str);
        remove_quotes(arg_str);
        unescape_quotes(arg_str);

        for (uint32_t j = 0; j < TOOLS_LIST_SIZE; j++) {
            if (strcmp(name_str, g_tools_list[j].name) == 0) {
                char *rsp = g_tools_list[j].func((void *)arg_str);
                if (rsp) {
                    submit_tool_reply(imei, chat_id, tool_call_id, rsp);
                    liot_rtos_free(rsp);
                }
                break;
            }
        }

        cJSON_free(name_str);
        cJSON_free(arg_str);
    }
    cJSON_Delete(root);
}
