/**
 * @brief LVGL GUI layout for tgai application (LSDK port)
 *
 * Ported from PLAT liot_tgai_demo/hardware/src/lcd_gui.c.
 * Replaced PLAT-only headers (lierda_app_main.h, atcmd.h local path)
 * with LSDK equivalents.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cmsis_os2.h"
#include "liot_os.h"
#include "liot_log.h"

#include "lvgl.h"
#include "lcd_gui.h"
#include "font_awesome_symbols.h"

LV_FONT_DECLARE(font_puhui_14_1)
LV_FONT_DECLARE(font_awesome_30_1)
LV_FONT_DECLARE(font_awesome_14_1)

/* ------------------------------------------------------------------ */
/* Reply text queue                                                    */
/* ------------------------------------------------------------------ */
#define REPLY_SENTENCE_MAX_LEN 256
#define REPLY_TEXT_QUEUE_MAX   16
#define AUTO_DISPLAY_CHAR_COUNT 11

int reply_sentence_buffer_len = 0;
int queue_front = 0, queue_rear = 0, queue_count = 0;
int reply_text_show_enable = 0;
liot_timer_t reply_text_show_timer = NULL;

char reply_group_queue[REPLY_TEXT_QUEUE_MAX][REPLY_SENTENCE_MAX_LEN] = {{0}};
char reply_sentence_buffer[REPLY_SENTENCE_MAX_LEN] = {0};

/* ------------------------------------------------------------------ */
/* Label shot timer                                                    */
/* ------------------------------------------------------------------ */
liot_timer_t lvgl_label_shot_timer = NULL;
char *lvgl_label_shot_old_str = NULL;
char *lvgl_label_shot_new_str = NULL;
uint8_t lvgl_label_shot_str_status = 0;

char network_str[8] = {0};
char battery_str[8] = {0};
char usbInsert_str[8] = {0};

static void lvgl_label_shot_timer_callback(void *arg);

/* ------------------------------------------------------------------ */
/* GUI info struct                                                     */
/* ------------------------------------------------------------------ */
typedef struct
{
    lv_style_t *style;

    lv_obj_t *screen;
    lv_obj_t *container;
    lv_obj_t *content;
    lv_obj_t *emotion_label;
    lv_obj_t *status_bar;
    lv_obj_t *side_bar;
    lv_obj_t *network_label;
    lv_obj_t *status_label;
    lv_obj_t *text_label;

    lv_obj_t *logo_img;
} gui_info_t;

gui_info_t gGuiInfo = {0};

LV_IMG_DECLARE(Lierda_logo)

/* ------------------------------------------------------------------ */
/* UTF-8 helpers                                                       */
/* ------------------------------------------------------------------ */
static int is_sentence_end(const char *str)
{
    if (str == NULL) return 0;
    const char *endings[] = {"。", "！", "？", "!", "?"};
    size_t n = sizeof(endings) / sizeof(endings[0]);
    int slen = strlen(str);
    for (size_t i = 0; i < n; ++i)
    {
        size_t elen = strlen(endings[i]);
        if (slen >= (int)elen && strcmp(str + slen - elen, endings[i]) == 0)
            return 1;
    }
    return 0;
}

static int utf8_strlen(const char *s)
{
    int len = 0;
    while (*s)
    {
        if ((*s & 0x80) == 0)
            s += 1;
        else if ((*s & 0xE0) == 0xC0)
            s += 2;
        else if ((*s & 0xF0) == 0xE0)
            s += 3;
        else if ((*s & 0xF8) == 0xF0)
            s += 4;
        else
            s += 1;
        len++;
    }
    return len;
}

/* ------------------------------------------------------------------ */
/* Reply group queue                                                   */
/* ------------------------------------------------------------------ */
void enqueue_reply_group(const char *group)
{
    if (queue_count < REPLY_TEXT_QUEUE_MAX)
    {
        strncpy(reply_group_queue[queue_rear], group, REPLY_SENTENCE_MAX_LEN - 1);
        reply_group_queue[queue_rear][REPLY_SENTENCE_MAX_LEN - 1] = 0;
        queue_rear = (queue_rear + 1) % REPLY_TEXT_QUEUE_MAX;
        queue_count++;
    }
    else
    {
        queue_front = (queue_front + 1) % REPLY_TEXT_QUEUE_MAX;
        strncpy(reply_group_queue[queue_rear], group, REPLY_SENTENCE_MAX_LEN - 1);
        reply_group_queue[queue_rear][REPLY_SENTENCE_MAX_LEN - 1] = 0;
        queue_rear = (queue_rear + 1) % REPLY_TEXT_QUEUE_MAX;
    }
}

void flush_reply_buffer_as_groups(void)
{
    char *p = reply_sentence_buffer;
    int remain = strlen(reply_sentence_buffer);

    while (remain > 0)
    {
        char part[REPLY_SENTENCE_MAX_LEN] = {0};
        int char_count = 0;
        int byte_count = 0;
        const char *s = p;
        int break_on_newline = 0;

        while (*s && char_count < AUTO_DISPLAY_CHAR_COUNT)
        {
            if (s[0] == '\n' && s[1] == '\n')
            {
                break_on_newline = 2;
                break;
            }
            if (s[0] == '\n')
            {
                break_on_newline = 1;
                break;
            }

            int this_bytes = 1;
            if ((*s & 0x80) == 0)
                this_bytes = 1;
            else if ((*s & 0xE0) == 0xC0)
                this_bytes = 2;
            else if ((*s & 0xF0) == 0xE0)
                this_bytes = 3;
            else if ((*s & 0xF8) == 0xF0)
                this_bytes = 4;
            else
                this_bytes = 1;

            if ((byte_count + this_bytes) > REPLY_SENTENCE_MAX_LEN - 2)
                break;
            memcpy(part + byte_count, s, this_bytes);
            s += this_bytes;
            byte_count += this_bytes;
            char_count++;
        }
        part[byte_count] = '\0';
        if (byte_count > 0)
            enqueue_reply_group(part);

        int skip = byte_count;
        if (break_on_newline > 0)
            skip += break_on_newline;
        remain -= skip;
        p += skip;
    }

    memset(reply_sentence_buffer, 0, sizeof(reply_sentence_buffer));
    reply_sentence_buffer_len = 0;
}

static uint32_t calc_reply_show_time(const char *str)
{
    int len = utf8_strlen(str);
    if (len >= AUTO_DISPLAY_CHAR_COUNT)
        return 2950;
    if (len <= 5)
        return 1200;
    return 1200 + (3000 - 1200) * len / AUTO_DISPLAY_CHAR_COUNT;
}

static void lvgl_reply_group_step_show(void *arg)
{
    static int waiting_clear = 0;
    if (queue_count > 0)
    {
        lvgl_text_label_update(reply_group_queue[queue_front]);
        uint32_t show_time = calc_reply_show_time(reply_group_queue[queue_front]);
        queue_front = (queue_front + 1) % REPLY_TEXT_QUEUE_MAX;
        queue_count--;
        waiting_clear = 0;

        if (queue_count > 0)
        {
            liot_rtos_timer_start(reply_text_show_timer, show_time);
            reply_text_show_enable = 1;
        }
        else
        {
            waiting_clear = 1;
            liot_rtos_timer_start(reply_text_show_timer, 1500);
        }
    }
    else if (waiting_clear)
    {
        reply_text_show_enable = 0;
        waiting_clear = 0;
        lvgl_text_label_update("");
    }
}

/* ------------------------------------------------------------------ */
/* GUI setup                                                           */
/* ------------------------------------------------------------------ */
void liot_lvgl_gui_setup(void)
{
    gGuiInfo.container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(gGuiInfo.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(gGuiInfo.container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(gGuiInfo.container, 0, 0);
    lv_obj_set_style_border_width(gGuiInfo.container, 0, 0);
    lv_obj_set_style_pad_column(gGuiInfo.container, 0, 0);

    gGuiInfo.side_bar = lv_obj_create(gGuiInfo.container);
    lv_obj_set_flex_grow(gGuiInfo.side_bar, 1);
    lv_obj_set_flex_flow(gGuiInfo.side_bar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(gGuiInfo.side_bar, 0, 0);
    lv_obj_set_style_border_width(gGuiInfo.side_bar, 0, 0);
    lv_obj_set_style_radius(gGuiInfo.side_bar, 0, 0);
    lv_obj_set_style_pad_row(gGuiInfo.side_bar, 0, 0);

    gGuiInfo.content = lv_obj_create(gGuiInfo.container);
    lv_obj_set_size(gGuiInfo.content, 32, 32);
    lv_obj_set_style_pad_all(gGuiInfo.content, 0, 0);
    lv_obj_set_style_border_width(gGuiInfo.content, 0, 0);
    lv_obj_set_style_radius(gGuiInfo.content, 0, 0);

    gGuiInfo.emotion_label = lv_label_create(gGuiInfo.content);
    lv_obj_set_style_text_font(gGuiInfo.emotion_label, &font_awesome_30_1, LV_PART_MAIN);
    lv_label_set_text(gGuiInfo.emotion_label, FONT_AWESOME_AI_CHIP);
    lv_obj_center(gGuiInfo.emotion_label);

    gGuiInfo.status_bar = lv_obj_create(gGuiInfo.side_bar);
    lv_obj_set_size(gGuiInfo.status_bar, LV_SIZE_CONTENT, 16);
    lv_obj_set_style_radius(gGuiInfo.status_bar, 0, 0);
    lv_obj_set_flex_flow(gGuiInfo.status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(gGuiInfo.status_bar, 0, 0);
    lv_obj_set_style_border_width(gGuiInfo.status_bar, 0, 0);
    lv_obj_set_style_pad_column(gGuiInfo.status_bar, 0, 0);

    gGuiInfo.network_label = lv_label_create(gGuiInfo.status_bar);
    lv_obj_set_style_text_font(gGuiInfo.network_label, &font_awesome_14_1, LV_PART_MAIN);
    lv_label_set_text(gGuiInfo.network_label, FONT_AWESOME_SIGNAL_OFF);

    gGuiInfo.status_label = lv_label_create(gGuiInfo.side_bar);
    lv_obj_set_style_text_font(gGuiInfo.status_label, &font_puhui_14_1, LV_PART_MAIN);
    lv_obj_set_flex_grow(gGuiInfo.status_label, 0);
    lv_obj_set_width(gGuiInfo.status_label, 128 - 32);
    lv_label_set_long_mode(gGuiInfo.status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(gGuiInfo.status_label, "正在初始化");

    gGuiInfo.text_label = lv_label_create(gGuiInfo.side_bar);
    lv_obj_set_style_text_font(gGuiInfo.text_label, &font_puhui_14_1, LV_PART_MAIN);
    lv_obj_set_flex_grow(gGuiInfo.text_label, 1);
    lv_obj_set_width(gGuiInfo.text_label, 128 - 32);
    lv_label_set_long_mode(gGuiInfo.text_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(gGuiInfo.text_label, "");

    if (reply_text_show_timer == NULL)
    {
        liot_rtos_timer_create(&reply_text_show_timer, 1, lvgl_reply_group_step_show, NULL);
    }

    liot_rtos_timer_create(&lvgl_label_shot_timer, 0, lvgl_label_shot_timer_callback, NULL);
}

/* ------------------------------------------------------------------ */
/* GUI update functions                                                */
/* ------------------------------------------------------------------ */
void lvgl_status_label_update(char *str)
{
    lv_label_set_text(gGuiInfo.status_label, str);
}

void lvgl_text_label_update(char *str)
{
    lv_label_set_text(gGuiInfo.text_label, str);
}

void lvgl_emotion_label_update(char *str)
{
    lv_label_set_text(gGuiInfo.emotion_label, str);
}

void lvgl_network_label_update(char *str)
{
    char status_bar_str[32] = {0};
    strncpy(network_str, str, sizeof(network_str) - 1);
    snprintf(status_bar_str, sizeof(status_bar_str), "%s%s%s", battery_str, network_str, usbInsert_str);
    lv_label_set_text(gGuiInfo.network_label, status_bar_str);
}

void lvgl_battery_label_update(char *str)
{
    char status_bar_str[32] = {0};
    strncpy(battery_str, str, sizeof(battery_str) - 1);
    snprintf(status_bar_str, sizeof(status_bar_str), "%s%s%s", battery_str, network_str, usbInsert_str);
    lv_label_set_text(gGuiInfo.network_label, status_bar_str);
}

void lvgl_usbInsert_label_update(char *str)
{
    char status_bar_str[32] = {0};
    strncpy(usbInsert_str, str, sizeof(usbInsert_str) - 1);
    snprintf(status_bar_str, sizeof(status_bar_str), "%s%s%s", battery_str, network_str, usbInsert_str);
    lv_label_set_text(gGuiInfo.network_label, status_bar_str);
}

static void lvgl_label_shot_timer_callback(void *arg)
{
    if (strcmp(lvgl_label_shot_new_str, lv_label_get_text(gGuiInfo.status_label)) == 0)
    {
        lv_label_set_text(gGuiInfo.status_label, lvgl_label_shot_old_str);
    }
    else
    {
        lv_label_set_text(gGuiInfo.status_label, lv_label_get_text(gGuiInfo.status_label));
    }
    free(lvgl_label_shot_old_str);
    free(lvgl_label_shot_new_str);
    lvgl_label_shot_str_status = 0;
}

void lvgl_status_label_shot_time(char *str, uint32_t time)
{
    if (lvgl_label_shot_str_status == 0)
        lvgl_label_shot_old_str = strdup(lv_label_get_text(gGuiInfo.status_label));
    lvgl_label_shot_str_status = 1;
    lvgl_label_shot_new_str = strdup(str);
    lv_label_set_text(gGuiInfo.status_label, str);
    liot_rtos_timer_start(lvgl_label_shot_timer, time);
}

void lvgl_display_logo(void)
{
    gGuiInfo.logo_img = lv_img_create(lv_scr_act());
    lv_img_set_src(gGuiInfo.logo_img, &Lierda_logo);
    lv_obj_center(gGuiInfo.logo_img);
    lv_img_set_zoom(gGuiInfo.logo_img, 128);
    osDelay(3000);
    lv_obj_del(gGuiInfo.logo_img);
}

void lvgl_display_software_info(void)
{
}

void lvgl_display_add_reply_text(const char *str){
    if (str == NULL || strlen(str) == 0)
        return;
    int slen = strlen(str);
    if (reply_sentence_buffer_len + slen < REPLY_SENTENCE_MAX_LEN)
    {
        strcat(reply_sentence_buffer, str);
        reply_sentence_buffer_len += slen;
    }
    else
    {
        flush_reply_buffer_as_groups();
        strcat(reply_sentence_buffer, str);
        reply_sentence_buffer_len += slen;
    }

    char *seg_start = reply_sentence_buffer;
    while (*seg_start)
    {
        char *newline = strstr(seg_start, "\n");
        if (newline)
        {
            int is_double_newline = (newline[1] == '\n') ? 1 : 0;
            int part_len = newline - seg_start;
            if (part_len > 0 && part_len < REPLY_SENTENCE_MAX_LEN)
            {
                char part[REPLY_SENTENCE_MAX_LEN] = {0};
                strncpy(part, seg_start, part_len);
                part[part_len] = '\0';
                if (strlen(part) > 0)
                    enqueue_reply_group(part);
            }
            seg_start = newline + 1 + is_double_newline;
            continue;
        }
        break;
    }
    if (seg_start != reply_sentence_buffer)
    {
        int left = strlen(seg_start);
        memmove(reply_sentence_buffer, seg_start, left + 1);
        reply_sentence_buffer_len = left;
    }

    if (is_sentence_end(reply_sentence_buffer) || utf8_strlen(reply_sentence_buffer) >= AUTO_DISPLAY_CHAR_COUNT)
    {
        if (strlen(reply_sentence_buffer) > 0)
        {
            enqueue_reply_group(reply_sentence_buffer);
            memset(reply_sentence_buffer, 0, sizeof(reply_sentence_buffer));
            reply_sentence_buffer_len = 0;
        }
    }

    if (queue_count > 0 && !reply_text_show_enable)
    {
        lvgl_text_label_update(reply_group_queue[queue_front]);
        queue_front = (queue_front + 1) % REPLY_TEXT_QUEUE_MAX;
        queue_count--;
        reply_text_show_enable = 1;
        liot_rtos_timer_start(reply_text_show_timer, 2000);
    }
}

/* ------------------------------------------------------------------ */
/* Signal info GUI (CSQ / RSRP / SNR three-line layout)               */
/* ------------------------------------------------------------------ */
typedef struct
{
    lv_obj_t *container;
    lv_obj_t *csq_label;
    lv_obj_t *rsrp_label;
    lv_obj_t *snr_label;
} signal_gui_t;

static signal_gui_t gSignalGui = {0};

void liot_lvgl_signal_gui_setup(void)
{
    /* 全屏容器，垂直排列三行 */
    gSignalGui.container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(gSignalGui.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(gSignalGui.container, 2, 0);
    lv_obj_set_style_border_width(gSignalGui.container, 0, 0);
    lv_obj_set_style_radius(gSignalGui.container, 0, 0);
    lv_obj_set_flex_flow(gSignalGui.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(gSignalGui.container, 2, 0);

    /* 第1行：CSQ */
    gSignalGui.csq_label = lv_label_create(gSignalGui.container);
    lv_obj_set_style_text_font(gSignalGui.csq_label, &font_puhui_14_1, LV_PART_MAIN);
    lv_obj_set_width(gSignalGui.csq_label, LV_HOR_RES - 4);
    lv_label_set_text(gSignalGui.csq_label, "CSQ: --");

    /* 第2行：RSRP */
    gSignalGui.rsrp_label = lv_label_create(gSignalGui.container);
    lv_obj_set_style_text_font(gSignalGui.rsrp_label, &font_puhui_14_1, LV_PART_MAIN);
    lv_obj_set_width(gSignalGui.rsrp_label, LV_HOR_RES - 4);
    lv_label_set_text(gSignalGui.rsrp_label, "RSRP: --dBm");

    /* 第3行：SNR */
    gSignalGui.snr_label = lv_label_create(gSignalGui.container);
    lv_obj_set_style_text_font(gSignalGui.snr_label, &font_puhui_14_1, LV_PART_MAIN);
    lv_obj_set_width(gSignalGui.snr_label, LV_HOR_RES - 4);
    lv_label_set_text(gSignalGui.snr_label, "SNR: --dB");
}

void lvgl_signal_update(uint8_t csq, int rsrp, int snr)
{
    char buf[32];

    snprintf(buf, sizeof(buf), "CSQ: %d", csq);
    lv_label_set_text(gSignalGui.csq_label, buf);

    snprintf(buf, sizeof(buf), "RSRP: -%ddBm", rsrp);
    lv_label_set_text(gSignalGui.rsrp_label, buf);

    snprintf(buf, sizeof(buf), "SNR: %ddB", snr);
    lv_label_set_text(gSignalGui.snr_label, buf);
}

/* ------------------------------------------------------------------ */
/* TCP info GUI (Status / Recv bytes two-line layout)                  */
/* ------------------------------------------------------------------ */
typedef struct
{
    lv_obj_t *container;
    lv_obj_t *status_label;
    lv_obj_t *recv_label;
} tcp_gui_t;

static tcp_gui_t gTcpGui = {0};

void liot_lvgl_tcp_gui_setup(void)
{
    gTcpGui.container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(gTcpGui.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(gTcpGui.container, 2, 0);
    lv_obj_set_style_border_width(gTcpGui.container, 0, 0);
    lv_obj_set_style_radius(gTcpGui.container, 0, 0);
    lv_obj_set_flex_flow(gTcpGui.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(gTcpGui.container, 4, 0);

    /* 第1行：TCP 连接状态 */
    gTcpGui.status_label = lv_label_create(gTcpGui.container);
    lv_obj_set_style_text_font(gTcpGui.status_label, &font_puhui_14_1, LV_PART_MAIN);
    lv_obj_set_width(gTcpGui.status_label, LV_HOR_RES - 4);
    lv_label_set_text(gTcpGui.status_label, "TCP: --");

    /* 第2行：累计收到字节数 */
    gTcpGui.recv_label = lv_label_create(gTcpGui.container);
    lv_obj_set_style_text_font(gTcpGui.recv_label, &font_puhui_14_1, LV_PART_MAIN);
    lv_obj_set_width(gTcpGui.recv_label, LV_HOR_RES - 4);
    lv_label_set_text(gTcpGui.recv_label, "Data Recv: 0Bytes");
}

void lvgl_tcp_update(int socket_status, uint32_t recv_bytes)
{
    char buf[32];
    const char *status_str;

    switch (socket_status) {
        case 0:
            status_str = "Connected";
            break;
        case 4:
            status_str = "Disconnected";
            break;
        case 2:
            status_str = "Failed";
            break;
        default:
            status_str = "Disconnected";
            break;
    }

    snprintf(buf, sizeof(buf), "TCP: %s", status_str);
    lv_label_set_text(gTcpGui.status_label, buf);

    snprintf(buf, sizeof(buf), "Data Recv: %luBytes", recv_bytes);
    lv_label_set_text(gTcpGui.recv_label, buf);
}
