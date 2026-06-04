/**
 * @file lcd_gui.c
 * @brief LVGL GUI for SIM signal info display demo.
 *
 * Displays CSQ, RSRP and SNR on a 128x64 SSD1306 OLED.
 * Extracted from demo_lvgl_key_tcp/lcd_gui.c — only the signal
 * info section is kept; all other GUI elements are removed.
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-01-01
 * @version 1.0
 */

#include <stdio.h>
#include <string.h>

#include "liot_os.h"
#include "liot_log.h"
#include "lvgl.h"
#include "lcd_gui.h"

LV_FONT_DECLARE(font_puhui_14_1)

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

static signal_gui_t s_signal_gui = {0};

/**
 * @brief Initialize the signal info GUI layout.
 *
 * Creates a full-screen flex container with three rows:
 * CSQ, RSRP and SNR. Must be called after lv_init() and
 * display driver registration.
 */
void liot_lvgl_signal_gui_setup(void)
{
    s_signal_gui.container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_signal_gui.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(s_signal_gui.container, 2, 0);
    lv_obj_set_style_border_width(s_signal_gui.container, 0, 0);
    lv_obj_set_style_radius(s_signal_gui.container, 0, 0);
    lv_obj_set_flex_flow(s_signal_gui.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_signal_gui.container, 2, 0);

    s_signal_gui.csq_label = lv_label_create(s_signal_gui.container);
    lv_obj_set_style_text_font(s_signal_gui.csq_label,
                               &font_puhui_14_1, LV_PART_MAIN);
    lv_obj_set_width(s_signal_gui.csq_label, LV_HOR_RES - 4);
    lv_label_set_text(s_signal_gui.csq_label, "CSQ: --");

    s_signal_gui.rsrp_label = lv_label_create(s_signal_gui.container);
    lv_obj_set_style_text_font(s_signal_gui.rsrp_label,
                               &font_puhui_14_1, LV_PART_MAIN);
    lv_obj_set_width(s_signal_gui.rsrp_label, LV_HOR_RES - 4);
    lv_label_set_text(s_signal_gui.rsrp_label, "RSRP: --dBm");

    s_signal_gui.snr_label = lv_label_create(s_signal_gui.container);
    lv_obj_set_style_text_font(s_signal_gui.snr_label,
                               &font_puhui_14_1, LV_PART_MAIN);
    lv_obj_set_width(s_signal_gui.snr_label, LV_HOR_RES - 4);
    lv_label_set_text(s_signal_gui.snr_label, "SNR: --dB");
}

/**
 * @brief Update the signal info labels on screen.
 *
 * @param[in] csq   CSQ value (0-31, 99 = unknown)
 * @param[in] rsrp  RSRP value in dBm (negative, stored as positive)
 * @param[in] snr   SNR value in dB
 */
void lvgl_signal_update(uint8_t csq, int rsrp, int snr)
{
    char buf[32];

    snprintf(buf, sizeof(buf), "CSQ: %d", csq);
    lv_label_set_text(s_signal_gui.csq_label, buf);

    snprintf(buf, sizeof(buf), "RSRP: -%ddBm", rsrp);
    lv_label_set_text(s_signal_gui.rsrp_label, buf);

    snprintf(buf, sizeof(buf), "SNR: %ddB", snr);
    lv_label_set_text(s_signal_gui.snr_label, buf);
}
