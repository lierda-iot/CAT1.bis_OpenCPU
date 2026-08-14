#include "app_display_port_lvgl_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if APP_DISPLAY_LVGL_ENABLE_LIOT_LCD
#include "app_boot_logo_liot.h"
#include "liot_gpio2.h"
#include "liot_lcd.h"
#include "liot_sleep.h"
#include "liot_tp.h"
#include "lvgl.h"

LIOT_ADD_DISPLAY(liot_st7789_dev);
LIOT_ADD_TP_DEV(g_liot_tp_ft6336);
#endif

#define APP_DISPLAY_LVGL_CMD_DRAIN_MAX 8U

typedef enum {
    APP_DISPLAY_LVGL_CMD_STATUS = 0,
    APP_DISPLAY_LVGL_CMD_NOTIFY,
    APP_DISPLAY_LVGL_CMD_EMOTION,
    APP_DISPLAY_LVGL_CMD_CHAT,
    APP_DISPLAY_LVGL_CMD_SCREEN,
    APP_DISPLAY_LVGL_CMD_STATUS_BAR,
    APP_DISPLAY_LVGL_CMD_POWER_SAVE,
    APP_DISPLAY_LVGL_CMD_CAMERA_FRAME,
    APP_DISPLAY_LVGL_CMD_PLAYER_FILES,
    APP_DISPLAY_LVGL_CMD_PLAYER_STATUS,
    APP_DISPLAY_LVGL_CMD_RECORDER_STATUS,
} app_display_lvgl_cmd_id_t;

typedef struct {
    app_display_lvgl_cmd_id_t id;
    app_display_screen_t screen;
    app_emotion_t emotion;
    app_display_role_t role;
    app_display_camera_frame_t camera_frame;
    app_display_player_file_t player_files[APP_DISPLAY_PLAYER_FILE_MAX];
    app_display_player_status_t player_status;
    app_display_recorder_status_t recorder_status;
    app_sem_t done_sem;
    uint32_t duration_ms;
    uint32_t player_file_count;
    bool enable;
    char text[APP_DISPLAY_LVGL_TEXT_MAX];
} app_display_lvgl_cmd_t;

static int display_lvgl_init(const app_display_caps_t *caps);
static int display_lvgl_set_status(const char *text);
static int display_lvgl_notify(const char *text, uint32_t duration_ms);
static int display_lvgl_set_emotion(app_emotion_t emotion);
static int display_lvgl_set_chat_message(app_display_role_t role, const char *text);
static int display_lvgl_show_screen(app_display_screen_t screen);
static int display_lvgl_update_status_bar(void);
static int display_lvgl_set_power_save(bool enable);
static int display_lvgl_present_camera_frame(const app_display_camera_frame_t *frame);
static int display_lvgl_set_player_files(const app_display_player_file_t *files, uint32_t count);
static int display_lvgl_set_player_status(const app_display_player_status_t *status);
static int display_lvgl_set_recorder_status(const app_display_recorder_status_t *status);

app_display_lvgl_ctx_t s_display_lvgl;

static const app_display_driver_t s_display_lvgl_driver = {
    .init = display_lvgl_init,
    .set_status = display_lvgl_set_status,
    .notify = display_lvgl_notify,
    .set_emotion = display_lvgl_set_emotion,
    .set_chat_message = display_lvgl_set_chat_message,
    .show_screen = display_lvgl_show_screen,
    .update_status_bar = display_lvgl_update_status_bar,
    .set_power_save = display_lvgl_set_power_save,
    .present_camera_frame = display_lvgl_present_camera_frame,
    .set_player_files = display_lvgl_set_player_files,
    .set_player_status = display_lvgl_set_player_status,
    .set_recorder_status = display_lvgl_set_recorder_status,
};

#if APP_DISPLAY_LVGL_ENABLE_LIOT_LCD
static liot_lcd_handle_t s_lcd;
static lv_disp_drv_t s_disp_drv;
static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t *s_lvgl_buf;
static lv_color_t *s_flush_line;
static liot_tp_handle_t s_tp;
static lv_indev_drv_t s_indev_drv;
static lv_indev_t *s_indev;
static volatile uint16_t s_touch_x;
static volatile uint16_t s_touch_y;
static volatile bool s_touch_pressed;
static volatile bool s_touch_valid;
static uint32_t s_touch_cb_count;
static uint32_t s_touch_read_count;
static uint16_t s_touch_log_x;
static uint16_t s_touch_log_y;
static bool s_touch_log_pressed;
static bool s_touch_log_valid;
static bool s_back_swipe_tracking;
static bool s_back_swipe_action_sent;
static uint16_t s_back_swipe_start_x;
static uint16_t s_back_swipe_start_y;
#endif

static void display_lvgl_copy_string(char *dst, uint32_t dst_size, const char *src)
{
    uint32_t i = 0U;

    if (dst == NULL || dst_size == 0U) {
        return;
    }
    dst[0] = '\0';
    if (src == NULL) {
        return;
    }
    while (i + 1U < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static bool display_lvgl_backend_available(app_display_lvgl_backend_t backend)
{
    switch (backend) {
    case APP_DISPLAY_LVGL_BACKEND_NONE:
        return true;
    case APP_DISPLAY_LVGL_BACKEND_LIOT_LCD:
        return APP_DISPLAY_LVGL_ENABLE_LIOT_LCD != 0;
    default:
        return false;
    }
}

static void display_lvgl_apply_default_caps(app_display_caps_t *caps)
{
    if (caps == NULL) {
        return;
    }
    if (caps->width == 0U) {
        caps->width = APP_DISPLAY_LVGL_DEFAULT_WIDTH;
    }
    if (caps->height == 0U) {
        caps->height = APP_DISPLAY_LVGL_DEFAULT_HEIGHT;
    }
}

static void display_lvgl_apply_default_lcd_config(app_display_lvgl_lcd_config_t *lcd)
{
    if (lcd == NULL) {
        return;
    }
    if (lcd->interface_type == 0U) {
        lcd->interface_type = APP_DISPLAY_LVGL_LCD_INTERFACE_LSPI;
    }
    if (lcd->lspi_num <= 0) {
        lcd->lspi_num = APP_DISPLAY_LVGL_DEFAULT_LSPI_NUM;
    }
    if (lcd->lspi_cs <= 0) {
        lcd->lspi_cs = APP_DISPLAY_LVGL_DEFAULT_LSPI_CS;
    }
    if (lcd->lspi_speed_hz == 0U) {
        lcd->lspi_speed_hz = APP_DISPLAY_LVGL_DEFAULT_LSPI_SPEED_HZ;
    }
    if (lcd->brightness == 0U) {
        lcd->brightness = 100U;
    }
}

static void display_lvgl_apply_default_config(app_display_lvgl_config_t *config)
{
    if (config == NULL) {
        return;
    }
    display_lvgl_apply_default_caps(&config->caps);
    display_lvgl_apply_default_lcd_config(&config->lcd);
    if (config->task_stack_bytes == 0U) {
        config->task_stack_bytes = APP_DISPLAY_LVGL_DEFAULT_STACK_BYTES;
    }
    if (config->task_priority == 0U) {
        config->task_priority = APP_DISPLAY_LVGL_DEFAULT_TASK_PRIORITY;
    }
    if (config->tick_ms == 0U) {
        config->tick_ms = APP_DISPLAY_LVGL_DEFAULT_TICK_MS;
    }
    if (config->handler_ms == 0U) {
        config->handler_ms = APP_DISPLAY_LVGL_DEFAULT_HANDLER_MS;
    }
    if (config->queue_depth == 0U) {
        config->queue_depth = APP_DISPLAY_LVGL_DEFAULT_QUEUE_DEPTH;
    }
    if (config->draw_buf_rows == 0U) {
        config->draw_buf_rows = APP_DISPLAY_LVGL_DEFAULT_DRAW_BUF_ROWS;
    }
}

int app_display_lvgl_get_default_config(app_display_lvgl_config_t *config)
{
    if (config == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(*config));
    config->backend = APP_DISPLAY_LVGL_BACKEND_LIOT_LCD;
    config->caps.has_screen = true;
    config->caps.has_status_bar = true;
    config->caps.has_emotion = true;
    config->caps.has_chat_text = true;
    config->caps.has_notification = true;
    config->caps.has_touch = true;
    config->caps.width = APP_DISPLAY_LVGL_DEFAULT_WIDTH;
    config->caps.height = APP_DISPLAY_LVGL_DEFAULT_HEIGHT;
    config->lcd.interface_type = APP_DISPLAY_LVGL_LCD_INTERFACE_LSPI;
    config->lcd.lspi_num = APP_DISPLAY_LVGL_DEFAULT_LSPI_NUM;
    config->lcd.lspi_cs = APP_DISPLAY_LVGL_DEFAULT_LSPI_CS;
    config->lcd.lspi_speed_hz = APP_DISPLAY_LVGL_DEFAULT_LSPI_SPEED_HZ;
    config->lcd.lspi_sync = true;
    config->lcd.backlight_type = APP_DISPLAY_LVGL_BACKLIGHT_GPIO;
    config->lcd.backlight_pin = APP_DISPLAY_LVGL_DEFAULT_BACKLIGHT_PIN;
    config->lcd.brightness = 100U;
    config->lcd.rst_pin = APP_DISPLAY_LVGL_DEFAULT_RST_PIN;
    config->lcd.rst_delay_ms = APP_DISPLAY_LVGL_DEFAULT_RST_DELAY_MS;
    config->touch.i2c_num = APP_DISPLAY_LVGL_DEFAULT_TOUCH_I2C_NUM;
    config->touch.i2c_addr = APP_DISPLAY_LVGL_DEFAULT_TOUCH_I2C_ADDR;
    config->touch.i2c_sda_pin = APP_DISPLAY_LVGL_DEFAULT_TOUCH_SDA_PIN;
    config->touch.i2c_scl_pin = APP_DISPLAY_LVGL_DEFAULT_TOUCH_SCL_PIN;
    config->touch.i2c_sda_func = APP_DISPLAY_LVGL_DEFAULT_TOUCH_SDA_FUNC;
    config->touch.i2c_scl_func = APP_DISPLAY_LVGL_DEFAULT_TOUCH_SCL_FUNC;
    config->touch.rst_pin = APP_DISPLAY_LVGL_DEFAULT_TOUCH_RST_PIN;
    config->touch.rst_delay_ms = APP_DISPLAY_LVGL_DEFAULT_RST_DELAY_MS;
    config->touch.rst_active_low = true;
    config->touch.int_pin = APP_DISPLAY_LVGL_DEFAULT_TOUCH_INT_PIN;
    config->enable_touch = true;
    config->task_stack_bytes = APP_DISPLAY_LVGL_DEFAULT_STACK_BYTES;
    config->task_priority = APP_DISPLAY_LVGL_DEFAULT_TASK_PRIORITY;
    config->tick_ms = APP_DISPLAY_LVGL_DEFAULT_TICK_MS;
    config->handler_ms = APP_DISPLAY_LVGL_DEFAULT_HANDLER_MS;
    config->queue_depth = APP_DISPLAY_LVGL_DEFAULT_QUEUE_DEPTH;
    config->draw_buf_rows = APP_DISPLAY_LVGL_DEFAULT_DRAW_BUF_ROWS;
    return APP_OK;
}

static int display_lvgl_require_ready(void)
{
    if (!s_display_lvgl.configured) {
        return APP_ERR_NOT_READY;
    }
    if (!s_display_lvgl.initialized) {
        return APP_ERR_NOT_READY;
    }
    return APP_OK;
}

static int display_lvgl_backend_not_supported(void)
{
    return APP_ERR_NOT_SUPPORTED;
}

static void display_lvgl_reset_runtime_state(void)
{
    s_display_lvgl.active_caps = s_display_lvgl.config.caps;
    s_display_lvgl.screen = APP_DISPLAY_SCREEN_HOME;
    s_display_lvgl.emotion = APP_EMOTION_NEUTRAL;
    s_display_lvgl.chat_role = APP_DISPLAY_ROLE_SYSTEM;
    s_display_lvgl.power_save = false;
    s_display_lvgl.notification_ticks = 0U;
    s_display_lvgl.camera_frame_count = 0U;
    s_display_lvgl.init_result = APP_ERR_NOT_READY;
    memset(&s_display_lvgl.player_status, 0, sizeof(s_display_lvgl.player_status));
    memset(&s_display_lvgl.recorder_status, 0, sizeof(s_display_lvgl.recorder_status));
    display_lvgl_copy_string(s_display_lvgl.status, sizeof(s_display_lvgl.status), "IDLE");
    display_lvgl_copy_string(s_display_lvgl.notification, sizeof(s_display_lvgl.notification), "");
    display_lvgl_copy_string(s_display_lvgl.chat_text, sizeof(s_display_lvgl.chat_text), "");
}

#if APP_DISPLAY_LVGL_ENABLE_LIOT_LCD
static void display_lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(s_display_lvgl.config.tick_ms);
}

static void display_lvgl_lcd_event_cb(void)
{
    //app_log("display lvgl display_lvgl_lcd_event_cb called");
}

static int display_lvgl_liot_power_on(void)
{
    LiotSleepModeCfg_t mode_cfg = {LIOT_SLEEP_MODE_NORMAL};

    app_log("display lvgl power: AON enable");
    if (Liot_AonPowerCtl(true) != L_GPIO_ERR_SUCCESS) {
        app_log("display lvgl power AON enable failed");
        return APP_ERR_FAIL;
    }
    app_log("display lvgl power: set 3.3V domain");
    if (Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V) != L_GPIO_ERR_SUCCESS) {
        app_log("display lvgl power voltage set failed");
        return APP_ERR_FAIL;
    }
    if (Liot_SleepSetMode(&mode_cfg) != LIOT_SLEEP_SUCCESS) {
        app_log("display lvgl power sleep mode set failed");
        return APP_ERR_FAIL;
    }
    app_log("display lvgl power ready");
    return APP_OK;
}

static int display_lvgl_liot_lcd_init(void)
{
    const app_display_lvgl_lcd_config_t *lcd = &s_display_lvgl.config.lcd;
    liot_lcd_config_t cfg;

    if (lcd->interface_type != APP_DISPLAY_LVGL_LCD_INTERFACE_LSPI) {
        app_log("display lvgl LCD interface unsupported: %u",
                (unsigned int)lcd->interface_type);
        return APP_ERR_NOT_SUPPORTED;
    }

    app_log("display lvgl LCD init: LSPI%d cs=%d speed=%lu rst=%d backlight=%d",
            lcd->lspi_num,
            lcd->lspi_cs,
            (unsigned long)lcd->lspi_speed_hz,
            lcd->rst_pin,
            lcd->backlight_pin);
    memset(&cfg, 0, sizeof(cfg));
    cfg.interface.type = LIOT_LCD_INTERFACE_LSPI;
    cfg.interface.lspi.num = (liot_lspi_port_e)lcd->lspi_num;
    cfg.interface.lspi.lcd_3_line_spi = lcd->lspi_3_line_spi;
    cfg.interface.lspi.lcd_2_data_lane = lcd->lspi_2_data_lane;
    cfg.interface.lspi.speed = lcd->lspi_speed_hz;
    cfg.interface.lspi.sync = lcd->lspi_sync;
    cfg.interface.lspi.cs = (liot_lspi_cs_e)lcd->lspi_cs;
    cfg.interface.lspi.cb = display_lvgl_lcd_event_cb;
    cfg.interface.blk.type = (liot_lcd_blk_type_e)lcd->backlight_type;
    cfg.interface.blk.pin = (int8_t)lcd->backlight_pin;
    cfg.interface.blk.pwm_num = (liot_pwm_sel_e)lcd->backlight_pwm_num;
    cfg.interface.rst.pin = (int8_t)lcd->rst_pin;
    cfg.interface.rst.delay = lcd->rst_delay_ms;
    cfg.lcdDev = &liot_st7789_dev;

    s_lcd = liot_lcd_init(&cfg);
    if (s_lcd == NULL) {
        app_log("display lvgl LCD init failed");
        return APP_ERR_FAIL;
    }

    (void)liot_lcd_clear_screen(s_lcd, BLACK);
    (void)liot_lcd_set_brightness(s_lcd, lcd->brightness);
    (void)app_boot_logo_show(s_lcd);
    app_log("display lvgl LCD ready: brightness=%u", (unsigned int)lcd->brightness);
    return APP_OK;
}

static void display_lvgl_flush_area(lv_disp_drv_t *disp,
                                    int16_t x1,
                                    int16_t y1,
                                    int16_t x2,
                                    int16_t y2,
                                    lv_color_t *color_p)
{
    liot_lcd_errcode_e ret;
    int16_t x;
    int16_t y;
    int16_t src_x1 = x1;
    int16_t src_y1 = y1;
    uint16_t src_width;
    int16_t width = (int16_t)s_display_lvgl.active_caps.width;
    int16_t height = (int16_t)s_display_lvgl.active_caps.height;

    if (s_lcd == NULL || s_flush_line == NULL || color_p == NULL || width <= 0 || height <= 0) {
        lv_disp_flush_ready(disp);
        return;
    }
    if (x2 < 0 || y2 < 0 || x1 >= width || y1 >= height || x2 < x1 || y2 < y1) {
        lv_disp_flush_ready(disp);
        return;
    }

    /*
     * The LIOT LSPI driver accepts LVGL's RGB565 buffer directly. Writing a
     * complete dirty rectangle in one transfer avoids visible seams between
     * the old 20-line draw-buffer chunks on this panel.
     */
    if (x1 >= 0 && y1 >= 0 && x2 < width && y2 < height) {
        //app_log("display lvgl lcd write start");
        ret = liot_lcd_write(s_lcd,
                             (uint16_t)x1,
                             (uint16_t)y1,
                             (uint16_t)x2,
                             (uint16_t)y2,
                             (uint8_t *)color_p);
        //app_log("display lvgl lcd write end");
        if (ret != LIOT_LCD_OK) {
            app_log("display lvgl flush failed: area=%d,%d,%d,%d ret=%d",
                    (int)x1,
                    (int)y1,
                    (int)x2,
                    (int)y2,
                    (int)ret);
        }
        lv_disp_flush_ready(disp);
        return;
    }

    src_width = (uint16_t)(x2 - x1 + 1);

    if (x1 < 0) {
        x1 = 0;
    }
    if (y1 < 0) {
        y1 = 0;
    }
    if (x2 >= width) {
        x2 = width - 1;
    }
    if (y2 >= height) {
        y2 = height - 1;
    }

    for (y = y1; y <= y2; y++) {
        for (x = x1; x <= x2; x++) {
            uint32_t src_index = (uint32_t)(y - src_y1) * src_width +
                                 (uint32_t)(x - src_x1);
            s_flush_line[x - x1] = color_p[src_index];
        }
        ret = liot_lcd_write(s_lcd,
                             (uint16_t)x1,
                             (uint16_t)y,
                             (uint16_t)x2,
                             (uint16_t)y,
                             (uint8_t *)s_flush_line);
        if (ret != LIOT_LCD_OK) {
            app_log("display lvgl clipped flush failed: y=%d ret=%d",
                    (int)y,
                    (int)ret);
            break;
        }
    }

    lv_disp_flush_ready(disp);
}

static void display_lvgl_flush_cb(lv_disp_drv_t *disp,
                                  const lv_area_t *area,
                                  lv_color_t *color_p)
{
    if (area == NULL) {
        lv_disp_flush_ready(disp);
        return;
    }
    display_lvgl_flush_area(disp, area->x1, area->y1, area->x2, area->y2, color_p);
}

static bool display_lvgl_touch_is_pressed(liot_tp_event_e event)
{
    return event == LIOT_TP_EVT_DOWN || event == LIOT_TP_EVT_MOVE;
}

static const char *display_lvgl_touch_event_name(liot_tp_event_e event)
{
    switch (event) {
    case LIOT_TP_EVT_DOWN:
        return "down";
    case LIOT_TP_EVT_UP:
        return "up";
    case LIOT_TP_EVT_MOVE:
        return "move";
    case LIOT_TP_EVT_NONE:
    default:
        return "none";
    }
}

static uint16_t display_lvgl_abs_diff_u16(uint16_t value_a, uint16_t value_b)
{
    return (value_a > value_b) ?
           (uint16_t)(value_a - value_b) :
           (uint16_t)(value_b - value_a);
}

static bool display_lvgl_touch_moved_enough(uint16_t x1,
                                            uint16_t y1,
                                            uint16_t x2,
                                            uint16_t y2)
{
    uint16_t dx = display_lvgl_abs_diff_u16(x1, x2);
    uint16_t dy = display_lvgl_abs_diff_u16(y1, y2);

    return dx >= APP_DISPLAY_LVGL_TOUCH_LOG_MOVE_DELTA ||
           dy >= APP_DISPLAY_LVGL_TOUCH_LOG_MOVE_DELTA;
}

void display_lvgl_back_swipe_reset(void)
{
    s_back_swipe_tracking = false;
    s_back_swipe_action_sent = false;
    s_back_swipe_start_x = 0U;
    s_back_swipe_start_y = 0U;
}

static void display_lvgl_back_swipe_emit(const char *source,
                                         const char *dir,
                                         uint16_t x,
                                         uint16_t y,
                                         uint16_t dx,
                                         uint16_t dy)
{
    s_back_swipe_action_sent = true;
    app_log("display back swipe: source=%s dir=%s screen=%d start=%u,%u end=%u,%u dx=%u dy=%u",
            (source != NULL) ? source : "unknown",
            (dir != NULL) ? dir : "unknown",
            (int)s_display_lvgl.screen,
            (unsigned int)s_back_swipe_start_x,
            (unsigned int)s_back_swipe_start_y,
            (unsigned int)x,
            (unsigned int)y,
            (unsigned int)dx,
            (unsigned int)dy);
    (void)app_display_emit_action(APP_DISPLAY_ACTION_BACK);
}

void display_lvgl_input_emit_back_swipe_from_gesture(int dir)
{
    const char *dir_name;

    app_log("display lvgl gesture: dir=%d screen=%d",
            dir,
            (int)s_display_lvgl.screen);
    if (dir != LV_DIR_LEFT && dir != LV_DIR_RIGHT) {
        return;
    }
    if (s_back_swipe_action_sent) {
        return;
    }

    dir_name = (dir == LV_DIR_RIGHT) ? "right" : "left";
    display_lvgl_back_swipe_emit("lvgl", dir_name, s_touch_x, s_touch_y, 0U, 0U);
}

static void display_lvgl_back_swipe_process(bool valid,
                                            bool pressed,
                                            uint16_t x,
                                            uint16_t y)
{
    uint16_t dx;
    uint16_t dy;
    const char *dir;

    if (!valid) {
        display_lvgl_back_swipe_reset();
        return;
    }

    if (pressed) {
        if (!s_back_swipe_tracking) {
            s_back_swipe_tracking = true;
            s_back_swipe_action_sent = false;
            s_back_swipe_start_x = x;
            s_back_swipe_start_y = y;
            app_log("display back swipe start: screen=%d xy=%u,%u",
                    (int)s_display_lvgl.screen,
                    (unsigned int)x,
                    (unsigned int)y);
        }
        return;
    }

    if (!s_back_swipe_tracking) {
        return;
    }

    dx = display_lvgl_abs_diff_u16(s_back_swipe_start_x, x);
    dy = display_lvgl_abs_diff_u16(s_back_swipe_start_y, y);
    if (!s_back_swipe_action_sent &&
        dx >= APP_DISPLAY_LVGL_BACK_SWIPE_MIN_DELTA &&
        dx > dy) {
        dir = (x >= s_back_swipe_start_x) ? "right" : "left";
        display_lvgl_back_swipe_emit("touch", dir, x, y, dx, dy);
    } else if (!s_back_swipe_action_sent &&
               display_lvgl_touch_moved_enough(s_back_swipe_start_x,
                                               s_back_swipe_start_y,
                                               x,
                                               y)) {
        app_log("display back swipe ignored: screen=%d start=%u,%u end=%u,%u dx=%u dy=%u",
                (int)s_display_lvgl.screen,
                (unsigned int)s_back_swipe_start_x,
                (unsigned int)s_back_swipe_start_y,
                (unsigned int)x,
                (unsigned int)y,
                (unsigned int)dx,
                (unsigned int)dy);
    }
    s_back_swipe_tracking = false;
}

static void display_lvgl_touch_save(uint16_t x, uint16_t y, bool pressed)
{
    s_touch_x = x;
    s_touch_y = y;
    s_touch_pressed = pressed;
    s_touch_valid = true;
}

static void display_lvgl_touch_map(uint16_t raw_x,
                                   uint16_t raw_y,
                                   uint16_t *mapped_x,
                                   uint16_t *mapped_y)
{
    uint16_t width = (uint16_t)s_display_lvgl.active_caps.width;
    uint16_t height = (uint16_t)s_display_lvgl.active_caps.height;

    if (mapped_x == NULL || mapped_y == NULL) {
        return;
    }

    *mapped_x = raw_x;
    *mapped_y = raw_y;
    if (liot_st7789_dev.info.direction != LIOT_LCD_DIR_180_ANGLE) {
        return;
    }

    if (width > 0U && raw_x < width) {
        *mapped_x = (uint16_t)(width - 1U - raw_x);
    }
    if (height > 0U && raw_y < height) {
        *mapped_y = (uint16_t)(height - 1U - raw_y);
    }
}

static void display_lvgl_touch_cb(liot_tp_touch_data_t *data, void *ctx)
{
    uint8_t i;
    uint8_t touch_cnt;
    uint16_t mapped_x;
    uint16_t mapped_y;
    bool should_log;

    (void)ctx;

    if (data == NULL) {
        return;
    }

    s_touch_cb_count++;
    if (data->touch_cnt == 0U) {
        should_log = s_touch_cb_count <= APP_DISPLAY_LVGL_TOUCH_LOG_FIRST_COUNT ||
                     (s_touch_cb_count % APP_DISPLAY_LVGL_TOUCH_LOG_INTERVAL) == 0U ||
                     s_touch_pressed;
        if (should_log) {
            app_log("display tp cb: seq=%lu cnt=0 release last=%u,%u pressed=%d screen=%d",
                    (unsigned long)s_touch_cb_count,
                    (unsigned int)s_touch_x,
                    (unsigned int)s_touch_y,
                    s_touch_pressed ? 1 : 0,
                    (int)s_display_lvgl.screen);
        }
        if (s_touch_pressed) {
            display_lvgl_touch_save(s_touch_x, s_touch_y, false);
        }
        return;
    }

    touch_cnt = data->touch_cnt;
    if (touch_cnt > APP_DISPLAY_LVGL_TOUCH_POINT_MAX) {
        touch_cnt = APP_DISPLAY_LVGL_TOUCH_POINT_MAX;
    }
    should_log = s_touch_cb_count <= APP_DISPLAY_LVGL_TOUCH_LOG_FIRST_COUNT ||
                 (s_touch_cb_count % APP_DISPLAY_LVGL_TOUCH_LOG_INTERVAL) == 0U ||
                 !s_touch_log_valid ||
                 s_touch_log_pressed != display_lvgl_touch_is_pressed(data->point[0].event) ||
                 display_lvgl_touch_moved_enough(s_touch_log_x,
                                                 s_touch_log_y,
                                                 data->point[0].x,
                                                 data->point[0].y);
    if (should_log) {
        display_lvgl_touch_map(data->point[0].x,
                               data->point[0].y,
                               &mapped_x,
                               &mapped_y);
        app_log("display tp cb: seq=%lu cnt=%u p0=%s(%d) raw=%u,%u mapped=%u,%u id=%u screen=%d",
                (unsigned long)s_touch_cb_count,
                (unsigned int)data->touch_cnt,
                display_lvgl_touch_event_name(data->point[0].event),
                (int)data->point[0].event,
                (unsigned int)data->point[0].x,
                (unsigned int)data->point[0].y,
                (unsigned int)mapped_x,
                (unsigned int)mapped_y,
                (unsigned int)data->point[0].id,
                (int)s_display_lvgl.screen);
        s_touch_log_x = data->point[0].x;
        s_touch_log_y = data->point[0].y;
        s_touch_log_pressed = display_lvgl_touch_is_pressed(data->point[0].event);
        s_touch_log_valid = true;
    }
    for (i = 0U; i < touch_cnt; i++) {
        display_lvgl_touch_map(data->point[i].x,
                               data->point[i].y,
                               &mapped_x,
                               &mapped_y);
        display_lvgl_touch_save(mapped_x,
                                mapped_y,
                                display_lvgl_touch_is_pressed(data->point[i].event));
    }
}

static void display_lvgl_gesture_cb(liot_tp_gesture_data_t *data, void *ctx)
{
    (void)ctx;

    if (data == NULL) {
        return;
    }
    app_log("display tp gesture cb: gesture=%d ts=%lu screen=%d",
            (int)data->gesture,
            (unsigned long)data->timestamp_ms,
            (int)s_display_lvgl.screen);
}

static void display_lvgl_touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    uint16_t x;
    uint16_t y;
    bool pressed;
    bool valid;
    static uint16_t last_read_x;
    static uint16_t last_read_y;
    static bool last_read_pressed;
    static bool last_read_valid;
    bool should_log;

    (void)indev_drv;

    if (data == NULL) {
        return;
    }

    if (s_tp != NULL && s_display_lvgl.config.touch.int_pin < 0) {
        liot_tp_touch_data_t touch_data;

        memset(&touch_data, 0, sizeof(touch_data));
        if (liot_tp_read_touch(s_tp, &touch_data) == LIOT_TP_SUCCESS) {
            display_lvgl_touch_cb(&touch_data, NULL);
        }
    }

    x = s_touch_x;
    y = s_touch_y;
    pressed = s_touch_pressed;
    valid = s_touch_valid;
    data->point.x = (lv_coord_t)x;
    data->point.y = (lv_coord_t)y;
    data->state = (valid && pressed) ?
                  LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->continue_reading = false;
    display_lvgl_back_swipe_process(valid, pressed, x, y);

    s_touch_read_count++;
    should_log = s_touch_read_count <= APP_DISPLAY_LVGL_TOUCH_LOG_FIRST_COUNT ||
                 (pressed &&
                  (s_touch_read_count % APP_DISPLAY_LVGL_TOUCH_LOG_INTERVAL) == 0U) ||
                 !last_read_valid ||
                 last_read_pressed != pressed ||
                 display_lvgl_touch_moved_enough(last_read_x, last_read_y, x, y);
    if (should_log) {
        app_log("display lvgl touch read: seq=%lu valid=%d pressed=%d xy=%u,%u state=%d screen=%d",
                (unsigned long)s_touch_read_count,
                valid ? 1 : 0,
                pressed ? 1 : 0,
                (unsigned int)x,
                (unsigned int)y,
                (int)data->state,
                (int)s_display_lvgl.screen);
        last_read_x = x;
        last_read_y = y;
        last_read_pressed = pressed;
        last_read_valid = true;
    }
}

static void display_lvgl_touch_reset_state(void)
{
    s_touch_x = 0U;
    s_touch_y = 0U;
    s_touch_pressed = false;
    s_touch_valid = false;
    s_touch_cb_count = 0U;
    s_touch_read_count = 0U;
    s_touch_log_x = 0U;
    s_touch_log_y = 0U;
    s_touch_log_pressed = false;
    s_touch_log_valid = false;
    display_lvgl_back_swipe_reset();
}

static int display_lvgl_liot_touch_init(void)
{
    const app_display_lvgl_touch_config_t *touch = &s_display_lvgl.config.touch;
    liot_tp_config_t cfg;
    uint8_t ic_info[4] = {0};
    int ret;

    if (!s_display_lvgl.config.enable_touch) {
        app_log("display lvgl touch disabled");
        return APP_OK;
    }
    if (touch->i2c_addr == 0U) {
        app_log("display lvgl touch init rejected: I2C address is zero");
        return APP_ERR_INVALID_ARG;
    }

    app_log("display lvgl touch init: I2C%d addr=0x%x sda=%d scl=%d int=%d",
            touch->i2c_num,
            touch->i2c_addr,
            touch->i2c_sda_pin,
            touch->i2c_scl_pin,
            touch->int_pin);
    display_lvgl_touch_reset_state();
    memset(&cfg, 0, sizeof(cfg));
    cfg.interface_type = LIOT_TP_IF_I2C;
    cfg.i2c.num = (liot_i2c_channel_e)touch->i2c_num;
    cfg.i2c.addr = touch->i2c_addr;
    cfg.i2c.sda = (int8_t)touch->i2c_sda_pin;
    cfg.i2c.scl = (int8_t)touch->i2c_scl_pin;
    cfg.i2c.sda_func = (uint8_t)touch->i2c_sda_func;
    cfg.i2c.scl_func = (uint8_t)touch->i2c_scl_func;
    cfg.rst.pin = (int8_t)touch->rst_pin;
    cfg.rst.delay_ms = touch->rst_delay_ms;
    cfg.rst.active_low = touch->rst_active_low;
    cfg.int_pin.pin = (int8_t)touch->int_pin;
    cfg.int_pin.signal = L_INT_EDGE_FALL;
    cfg.int_pin.pull = LIOT_FORCE_PULL_UP;
    cfg.sensor = &g_liot_tp_ft6336;
    cfg.fw_auto_update = touch->fw_auto_update;

    s_tp = liot_tp_init(&cfg);
    if (s_tp == NULL) {
        app_log("display lvgl touch device init failed");
        return APP_ERR_FAIL;
    }

    if (liot_tp_get_ic_info(s_tp, ic_info, sizeof(ic_info)) == LIOT_TP_SUCCESS) {
        app_log("display touch chip=0x%x fw=0x%x proj=0x%x lpm=0x%x",
                ic_info[0],
                ic_info[1],
                ic_info[2],
                ic_info[3]);
    }

    ret = liot_tp_register_int_callback(s_tp,
                                        display_lvgl_touch_cb,
                                        display_lvgl_gesture_cb,
                                        NULL,
                                        NULL);
    if (ret != LIOT_TP_SUCCESS) {
        app_log("display lvgl touch callback register failed: %d", ret);
        return APP_ERR_FAIL;
    }
    if (touch->int_pin >= 0) {
        ret = liot_tp_enable_int(s_tp, true);
        if (ret != LIOT_TP_SUCCESS) {
            app_log("display lvgl touch interrupt enable failed: %d", ret);
            return APP_ERR_FAIL;
        }
    }

    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = display_lvgl_touch_read_cb;
    s_indev_drv.disp = lv_disp_get_default();
    s_indev = lv_indev_drv_register(&s_indev_drv);
    if (s_indev == NULL) {
        app_log("display lvgl touch LVGL input register failed");
        return APP_ERR_FAIL;
    }

    app_log("display lvgl touch ready");
    return APP_OK;
}

static void display_lvgl_ui_set_power_save(bool enable)
{
    uint8_t brightness = enable ? 0U : s_display_lvgl.config.lcd.brightness;

    s_display_lvgl.power_save = enable;
    if (s_lcd != NULL) {
        (void)liot_lcd_set_brightness(s_lcd, brightness);
    }
}

static void display_lvgl_liot_release_buffers(void)
{
    display_lvgl_camera_release_preview_buffer();
    if (s_flush_line != NULL) {
        app_os_free(s_flush_line);
        s_flush_line = NULL;
    }
    if (s_lvgl_buf != NULL) {
        app_os_free(s_lvgl_buf);
        s_lvgl_buf = NULL;
    }
}

static void display_lvgl_apply_cmd(const app_display_lvgl_cmd_t *cmd)
{
    if (cmd == NULL) {
        return;
    }

    switch (cmd->id) {
    case APP_DISPLAY_LVGL_CMD_STATUS:
        display_lvgl_ui_set_status(cmd->text);
        break;
    case APP_DISPLAY_LVGL_CMD_NOTIFY:
        display_lvgl_ui_set_notification(cmd->text, cmd->duration_ms);
        break;
    case APP_DISPLAY_LVGL_CMD_EMOTION:
        display_lvgl_ui_set_emotion(cmd->emotion);
        break;
    case APP_DISPLAY_LVGL_CMD_CHAT:
        display_lvgl_ui_set_chat(cmd->role, cmd->text);
        break;
    case APP_DISPLAY_LVGL_CMD_SCREEN:
        display_lvgl_ui_set_screen(cmd->screen);
        break;
    case APP_DISPLAY_LVGL_CMD_STATUS_BAR:
        display_lvgl_ui_set_status(s_display_lvgl.status);
        break;
    case APP_DISPLAY_LVGL_CMD_POWER_SAVE:
        display_lvgl_ui_set_power_save(cmd->enable);
        break;
    case APP_DISPLAY_LVGL_CMD_CAMERA_FRAME:
        (void)display_lvgl_camera_present_frame(&cmd->camera_frame);
        break;
    case APP_DISPLAY_LVGL_CMD_PLAYER_FILES:
        display_lvgl_player_set_files(cmd->player_files, cmd->player_file_count);
        break;
    case APP_DISPLAY_LVGL_CMD_PLAYER_STATUS:
        display_lvgl_player_set_status(&cmd->player_status);
        break;
    case APP_DISPLAY_LVGL_CMD_RECORDER_STATUS:
        display_lvgl_recorder_set_status(&cmd->recorder_status);
        break;
    default:
        break;
    }
    if (cmd->done_sem != NULL) {
        (void)app_os_sem_release(cmd->done_sem);
    }
}

static int display_lvgl_liot_alloc_buffers(void)
{
    uint32_t width = s_display_lvgl.active_caps.width;
    uint32_t row_count = s_display_lvgl.active_caps.height;
    uint32_t pixel_count = width * row_count;

    if (width == 0U || row_count == 0U || pixel_count == 0U) {
        return APP_ERR_INVALID_ARG;
    }

    if (s_display_lvgl.config.draw_buf_rows != row_count) {
        app_log("display lvgl draw buffer promoted: rows=%u -> %lu",
                (unsigned int)s_display_lvgl.config.draw_buf_rows,
                (unsigned long)row_count);
        s_display_lvgl.config.draw_buf_rows = (uint16_t)row_count;
    }

    s_lvgl_buf = (lv_color_t *)app_os_malloc(pixel_count * sizeof(lv_color_t));
    if (s_lvgl_buf == NULL) {
        app_log("display lvgl buffer alloc failed: draw=%lu bytes",
                (unsigned long)(pixel_count * sizeof(lv_color_t)));
        return APP_ERR_NO_MEMORY;
    }

    s_flush_line = (lv_color_t *)app_os_malloc(width * sizeof(lv_color_t));
    if (s_flush_line == NULL) {
        app_log("display lvgl buffer alloc failed: flush=%lu bytes",
                (unsigned long)(width * sizeof(uint16_t)));
        app_os_free(s_lvgl_buf);
        s_lvgl_buf = NULL;
        return APP_ERR_NO_MEMORY;
    }
    app_log("display lvgl buffers ready: draw=%lu flush=%lu",
            (unsigned long)(pixel_count * sizeof(lv_color_t)),
            (unsigned long)(width * sizeof(lv_color_t)));
    return APP_OK;
}

static int display_lvgl_liot_register_lvgl_display(void)
{
    uint32_t width = s_display_lvgl.active_caps.width;
    uint32_t height = s_display_lvgl.active_caps.height;
    uint32_t pixel_count = width * s_display_lvgl.config.draw_buf_rows;

    if (s_display_lvgl.config.draw_buf_rows != height) {
        app_log("display lvgl driver register rejected: rows=%u height=%lu",
                (unsigned int)s_display_lvgl.config.draw_buf_rows,
                (unsigned long)height);
        return APP_ERR_INVALID_ARG;
    }

    lv_disp_draw_buf_init(&s_draw_buf, s_lvgl_buf, NULL, pixel_count);
    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = (lv_coord_t)width;
    s_disp_drv.ver_res = (lv_coord_t)height;
    s_disp_drv.flush_cb = display_lvgl_flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    if (lv_disp_drv_register(&s_disp_drv) == NULL) {
        app_log("display lvgl driver register failed");
        return APP_ERR_FAIL;
    }
    app_log("display lvgl driver registered: %lux%lu",
            (unsigned long)width,
            (unsigned long)height);
    return APP_OK;
}

static void display_lvgl_liot_finish_init(int result)
{
    s_display_lvgl.init_result = result;
    if (s_display_lvgl.ready_sem != NULL) {
        (void)app_os_sem_release(s_display_lvgl.ready_sem);
    }
}

static void display_lvgl_liot_cleanup_failed_init(void)
{
    app_log("display lvgl cleanup after failed initialization");
    if (s_display_lvgl.tick_timer != NULL) {
        (void)app_os_timer_stop(s_display_lvgl.tick_timer);
    }
    if (s_tp != NULL) {
        (void)liot_tp_deinit(s_tp);
        s_tp = NULL;
    }
    s_indev = NULL;
    display_lvgl_liot_release_buffers();
    s_lcd = NULL;
}

static void display_lvgl_liot_task(void *arg)
{
    app_display_lvgl_cmd_t cmd;
    int ret;

    (void)arg;

    app_log("display lvgl task started");
    app_os_log_current_task("display task start");
    ret = app_os_timer_create(&s_display_lvgl.tick_timer,
                              APP_TIMER_PERIODIC,
                              display_lvgl_tick_cb,
                              NULL);
    if (ret != APP_OK) {
        app_log("display lvgl tick timer create failed: %d", ret);
        display_lvgl_liot_cleanup_failed_init();
        display_lvgl_liot_finish_init(ret);
        return;
    }
    ret = app_os_timer_start(s_display_lvgl.tick_timer, s_display_lvgl.config.tick_ms);
    if (ret != APP_OK) {
        app_log("display lvgl tick timer start failed: %d", ret);
        display_lvgl_liot_cleanup_failed_init();
        display_lvgl_liot_finish_init(ret);
        return;
    }

    lv_init();
    app_log("display lvgl core ready");

    app_log("display lvgl task: power on");
    ret = display_lvgl_liot_power_on();
    if (ret == APP_OK) {
        app_log("display lvgl task: LCD init");
        ret = display_lvgl_liot_lcd_init();
    }
    if (ret == APP_OK) {
        app_log("display lvgl task: buffer allocation");
        ret = display_lvgl_liot_alloc_buffers();
    }
    if (ret == APP_OK) {
        app_log("display lvgl task: driver registration");
        ret = display_lvgl_liot_register_lvgl_display();
    }
    if (ret == APP_OK) {
        app_log("display lvgl task: touch init");
        ret = display_lvgl_liot_touch_init();
    }
    if (ret == APP_OK) {
        app_log("display lvgl task: UI create");
        ret = display_lvgl_ui_create();
    }
    if (ret != APP_OK) {
        app_log("display lvgl liot init failed: %d", ret);
        display_lvgl_liot_cleanup_failed_init();
        display_lvgl_liot_finish_init(ret);
        return;
    }

    display_lvgl_liot_finish_init(APP_OK);
    app_log("display lvgl task initialized");

    while (1) {
        uint32_t cmd_count = 0U;

        while (cmd_count < APP_DISPLAY_LVGL_CMD_DRAIN_MAX &&
               app_os_queue_recv(s_display_lvgl.queue,
                                 &cmd,
                                 sizeof(cmd),
                                 APP_OS_NO_WAIT) == APP_OK) {
            display_lvgl_apply_cmd(&cmd);
            cmd_count++;
        }

        display_lvgl_ui_update_notification_timer();
        (void)lv_task_handler();
        app_os_task_delay_ms(s_display_lvgl.config.handler_ms);
    }
}

static int display_lvgl_liot_start(void)
{
    int ret;

    if (s_display_lvgl.task_started) {
        app_log("display lvgl task already started: result=%d",
                s_display_lvgl.init_result);
        return s_display_lvgl.init_result;
    }

    app_log("display lvgl start: queue depth=%u item=%u",
            (unsigned int)s_display_lvgl.config.queue_depth,
            (unsigned int)sizeof(app_display_lvgl_cmd_t));
    ret = app_os_queue_create(&s_display_lvgl.queue,
                              s_display_lvgl.config.queue_depth,
                              sizeof(app_display_lvgl_cmd_t));
    if (ret != APP_OK) {
        app_log("display lvgl queue create failed: %d", ret);
        return ret;
    }

    ret = app_os_sem_create(&s_display_lvgl.ready_sem, 0U);
    if (ret != APP_OK) {
        app_log("display lvgl ready semaphore create failed: %d", ret);
        return ret;
    }
    ret = app_os_sem_create(&s_display_lvgl.camera_frame_done_sem, 0U);
    if (ret != APP_OK) {
        app_log("display lvgl camera frame semaphore create failed: %d", ret);
        return ret;
    }

    ret = app_os_task_create(&s_display_lvgl.task,
                             "display_lvgl",
                             display_lvgl_liot_task,
                             NULL,
                             s_display_lvgl.config.task_stack_bytes,
                             s_display_lvgl.config.task_priority);
    if (ret != APP_OK) {
        app_log("display lvgl task create failed: %d", ret);
        return ret;
    }

    s_display_lvgl.task_started = true;
    ret = app_os_sem_wait(s_display_lvgl.ready_sem, APP_OS_WAIT_FOREVER);
    if (ret != APP_OK) {
        app_log("display lvgl initial-ready wait failed: %d", ret);
        return ret;
    }
    app_log("display lvgl start complete: result=%d", s_display_lvgl.init_result);
    return s_display_lvgl.init_result;
}

static int display_lvgl_liot_post_cmd(const app_display_lvgl_cmd_t *cmd)
{
    int ret;

    if (cmd == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (s_display_lvgl.queue == NULL || !s_display_lvgl.task_started) {
        return APP_ERR_NOT_READY;
    }
    ret = app_os_queue_send(s_display_lvgl.queue, cmd, sizeof(*cmd), APP_OS_NO_WAIT);
    if (ret != APP_OK) {
        app_log("display lvgl command post failed: id=%d ret=%d", (int)cmd->id, ret);
    }
    return ret;
}
#endif

static int display_lvgl_start_backend(void)
{
    switch (s_display_lvgl.config.backend) {
    case APP_DISPLAY_LVGL_BACKEND_NONE:
        return APP_OK;
    case APP_DISPLAY_LVGL_BACKEND_LIOT_LCD:
#if APP_DISPLAY_LVGL_ENABLE_LIOT_LCD
        return display_lvgl_liot_start();
#else
        return APP_ERR_NOT_SUPPORTED;
#endif
    default:
        return APP_ERR_NOT_SUPPORTED;
    }
}

static int display_lvgl_post_cmd(const app_display_lvgl_cmd_t *cmd)
{
    if (s_display_lvgl.config.backend == APP_DISPLAY_LVGL_BACKEND_LIOT_LCD) {
#if APP_DISPLAY_LVGL_ENABLE_LIOT_LCD
        return display_lvgl_liot_post_cmd(cmd);
#else
        (void)cmd;
        return APP_ERR_NOT_SUPPORTED;
#endif
    }
    (void)cmd;
    return display_lvgl_backend_not_supported();
}

static int display_lvgl_init(const app_display_caps_t *caps)
{
    int ret;

    if (!s_display_lvgl.configured) {
        app_log("display lvgl init rejected: not configured");
        return APP_ERR_NOT_READY;
    }
    if (!display_lvgl_backend_available(s_display_lvgl.config.backend)) {
        app_log("display lvgl init rejected: backend=%d unavailable",
                (int)s_display_lvgl.config.backend);
        return APP_ERR_NOT_SUPPORTED;
    }

    display_lvgl_reset_runtime_state();
    if (caps != NULL) {
        s_display_lvgl.active_caps = *caps;
        display_lvgl_apply_default_caps(&s_display_lvgl.active_caps);
    }

    app_log("display lvgl init: backend=%d %ux%u",
            (int)s_display_lvgl.config.backend,
            (unsigned int)s_display_lvgl.active_caps.width,
            (unsigned int)s_display_lvgl.active_caps.height);
    ret = display_lvgl_start_backend();
    if (ret != APP_OK) {
        app_log("display lvgl backend start failed: %d", ret);
        return ret;
    }

    s_display_lvgl.initialized = true;
    app_log("display lvgl initialized: backend=%d %ux%u touch=%d",
            (int)s_display_lvgl.config.backend,
            (unsigned int)s_display_lvgl.active_caps.width,
            (unsigned int)s_display_lvgl.active_caps.height,
            s_display_lvgl.active_caps.has_touch ? 1 : 0);
    return APP_OK;
}

static int display_lvgl_set_status(const char *text)
{
    app_display_lvgl_cmd_t cmd;
    int ret = display_lvgl_require_ready();

    if (ret != APP_OK) {
        return ret;
    }
    if (s_display_lvgl.config.backend == APP_DISPLAY_LVGL_BACKEND_NONE) {
        display_lvgl_copy_string(s_display_lvgl.status, sizeof(s_display_lvgl.status), text);
        return display_lvgl_backend_not_supported();
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.id = APP_DISPLAY_LVGL_CMD_STATUS;
    display_lvgl_copy_string(cmd.text, sizeof(cmd.text), text);
    return display_lvgl_post_cmd(&cmd);
}

static int display_lvgl_notify(const char *text, uint32_t duration_ms)
{
    app_display_lvgl_cmd_t cmd;
    int ret = display_lvgl_require_ready();

    if (ret != APP_OK) {
        return ret;
    }
    if (s_display_lvgl.config.backend == APP_DISPLAY_LVGL_BACKEND_NONE) {
        display_lvgl_copy_string(s_display_lvgl.notification,
                                 sizeof(s_display_lvgl.notification),
                                 text);
        return display_lvgl_backend_not_supported();
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.id = APP_DISPLAY_LVGL_CMD_NOTIFY;
    cmd.duration_ms = duration_ms;
    display_lvgl_copy_string(cmd.text, sizeof(cmd.text), text);
    return display_lvgl_post_cmd(&cmd);
}

static int display_lvgl_set_emotion(app_emotion_t emotion)
{
    app_display_lvgl_cmd_t cmd;
    int ret = display_lvgl_require_ready();

    if (ret != APP_OK) {
        return ret;
    }
    if (s_display_lvgl.config.backend == APP_DISPLAY_LVGL_BACKEND_NONE) {
        s_display_lvgl.emotion = emotion;
        return display_lvgl_backend_not_supported();
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.id = APP_DISPLAY_LVGL_CMD_EMOTION;
    cmd.emotion = emotion;
    return display_lvgl_post_cmd(&cmd);
}

static int display_lvgl_set_chat_message(app_display_role_t role, const char *text)
{
    app_display_lvgl_cmd_t cmd;
    int ret = display_lvgl_require_ready();

    if (ret != APP_OK) {
        return ret;
    }
    if (s_display_lvgl.config.backend == APP_DISPLAY_LVGL_BACKEND_NONE) {
        s_display_lvgl.chat_role = role;
        display_lvgl_copy_string(s_display_lvgl.chat_text, sizeof(s_display_lvgl.chat_text), text);
        return display_lvgl_backend_not_supported();
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.id = APP_DISPLAY_LVGL_CMD_CHAT;
    cmd.role = role;
    display_lvgl_copy_string(cmd.text, sizeof(cmd.text), text);
    return display_lvgl_post_cmd(&cmd);
}

static int display_lvgl_show_screen(app_display_screen_t screen)
{
    app_display_lvgl_cmd_t cmd;
    int ret = display_lvgl_require_ready();

    if (ret != APP_OK) {
        return ret;
    }
    if (s_display_lvgl.config.backend == APP_DISPLAY_LVGL_BACKEND_NONE) {
        s_display_lvgl.screen = screen;
        return display_lvgl_backend_not_supported();
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.id = APP_DISPLAY_LVGL_CMD_SCREEN;
    cmd.screen = screen;
    return display_lvgl_post_cmd(&cmd);
}

static int display_lvgl_update_status_bar(void)
{
    app_display_lvgl_cmd_t cmd;
    int ret = display_lvgl_require_ready();

    if (ret != APP_OK) {
        return ret;
    }
    if (s_display_lvgl.config.backend == APP_DISPLAY_LVGL_BACKEND_NONE) {
        return display_lvgl_backend_not_supported();
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.id = APP_DISPLAY_LVGL_CMD_STATUS_BAR;
    return display_lvgl_post_cmd(&cmd);
}

static int display_lvgl_set_power_save(bool enable)
{
    app_display_lvgl_cmd_t cmd;
    int ret = display_lvgl_require_ready();

    if (ret != APP_OK) {
        return ret;
    }
    if (s_display_lvgl.config.backend == APP_DISPLAY_LVGL_BACKEND_NONE) {
        s_display_lvgl.power_save = enable;
        return display_lvgl_backend_not_supported();
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.id = APP_DISPLAY_LVGL_CMD_POWER_SAVE;
    cmd.enable = enable;
    return display_lvgl_post_cmd(&cmd);
}

static int display_lvgl_present_camera_frame(const app_display_camera_frame_t *frame)
{
    app_display_lvgl_cmd_t cmd;
    int ret = display_lvgl_require_ready();

    if (ret != APP_OK) {
        return ret;
    }
    if (!s_display_lvgl.active_caps.has_camera_preview) {
        return APP_ERR_NOT_SUPPORTED;
    }
    if (!display_lvgl_camera_frame_is_valid(frame)) {
        return APP_ERR_INVALID_ARG;
    }
    if (s_display_lvgl.screen != APP_DISPLAY_SCREEN_CAMERA) {
        return APP_OK;
    }
    if (s_display_lvgl.config.backend == APP_DISPLAY_LVGL_BACKEND_NONE) {
        return display_lvgl_backend_not_supported();
    }
    if (s_display_lvgl.camera_frame_done_sem == NULL) {
        return APP_ERR_NOT_READY;
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.id = APP_DISPLAY_LVGL_CMD_CAMERA_FRAME;
    cmd.camera_frame = *frame;
    cmd.done_sem = s_display_lvgl.camera_frame_done_sem;

    ret = display_lvgl_post_cmd(&cmd);
    if (ret != APP_OK) {
        return ret;
    }
    ret = app_os_sem_wait(s_display_lvgl.camera_frame_done_sem, APP_OS_WAIT_FOREVER);
    if (ret != APP_OK) {
        app_log("display camera frame wait failed: id=%lu ret=%d",
                (unsigned long)frame->frame_id,
                ret);
    }
    return ret;
}

static int display_lvgl_set_player_files(const app_display_player_file_t *files, uint32_t count)
{
    app_display_lvgl_cmd_t cmd;
    int ret = display_lvgl_require_ready();

    if (ret != APP_OK) {
        return ret;
    }
    if (count > APP_DISPLAY_PLAYER_FILE_MAX) {
        return APP_ERR_INVALID_ARG;
    }
    if (count > 0U && files == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (s_display_lvgl.config.backend == APP_DISPLAY_LVGL_BACKEND_NONE) {
        return display_lvgl_backend_not_supported();
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.id = APP_DISPLAY_LVGL_CMD_PLAYER_FILES;
    cmd.player_file_count = count;
    if (files != NULL && count > 0U) {
        memcpy(cmd.player_files, files, count * sizeof(files[0]));
    }
    return display_lvgl_post_cmd(&cmd);
}

static int display_lvgl_set_player_status(const app_display_player_status_t *status)
{
    app_display_lvgl_cmd_t cmd;
    int ret = display_lvgl_require_ready();

    if (ret != APP_OK) {
        return ret;
    }
    if (status == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (s_display_lvgl.config.backend == APP_DISPLAY_LVGL_BACKEND_NONE) {
        s_display_lvgl.player_status = *status;
        return display_lvgl_backend_not_supported();
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.id = APP_DISPLAY_LVGL_CMD_PLAYER_STATUS;
    cmd.player_status = *status;
    return display_lvgl_post_cmd(&cmd);
}

static int display_lvgl_set_recorder_status(const app_display_recorder_status_t *status)
{
    app_display_lvgl_cmd_t cmd;
    int ret = display_lvgl_require_ready();

    if (ret != APP_OK) {
        return ret;
    }
    if (status == NULL) {
        return APP_ERR_INVALID_ARG;
    }
    if (s_display_lvgl.config.backend == APP_DISPLAY_LVGL_BACKEND_NONE) {
        s_display_lvgl.recorder_status = *status;
        return display_lvgl_backend_not_supported();
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.id = APP_DISPLAY_LVGL_CMD_RECORDER_STATUS;
    cmd.recorder_status = *status;
    return display_lvgl_post_cmd(&cmd);
}

int app_display_lvgl_setup(const app_display_lvgl_config_t *config)
{
    app_display_lvgl_config_t effective_config;

    if (config == NULL) {
        app_log("display lvgl setup rejected: null config");
        return APP_ERR_INVALID_ARG;
    }
    if (!display_lvgl_backend_available(config->backend)) {
        app_log("display lvgl setup rejected: backend=%d unavailable", (int)config->backend);
        return APP_ERR_NOT_SUPPORTED;
    }

    effective_config = *config;
    display_lvgl_apply_default_config(&effective_config);

    memset(&s_display_lvgl, 0, sizeof(s_display_lvgl));
    s_display_lvgl.config = effective_config;
    s_display_lvgl.configured = true;
    app_log("display lvgl setup: backend=%d %ux%u tick=%lu handler=%lu queue=%u",
            (int)effective_config.backend,
            (unsigned int)effective_config.caps.width,
            (unsigned int)effective_config.caps.height,
            (unsigned long)effective_config.tick_ms,
            (unsigned long)effective_config.handler_ms,
            (unsigned int)effective_config.queue_depth);
    return APP_OK;
}

int app_display_lvgl_register(void)
{
    int ret;

    if (!s_display_lvgl.configured) {
        app_log("display lvgl register rejected: not configured");
        return APP_ERR_NOT_READY;
    }
    ret = app_display_register_driver(&s_display_lvgl_driver);
    if (ret != APP_OK) {
        app_log("display lvgl driver register failed: %d", ret);
        return ret;
    }
    app_log("display lvgl driver registered with display service");
    return APP_OK;
}

const app_display_driver_t *app_display_lvgl_driver(void)
{
    return &s_display_lvgl_driver;
}
