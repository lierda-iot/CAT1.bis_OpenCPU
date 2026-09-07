#include <stdio.h>
#include <string.h>
#include "math.h"
#include "lierda_app_main.h"
#include "app_features.h"
#include "liot_os.h"
#include "liot_power.h"
#include "baji_photo/baji_photo_page.h"
#include "sc7a20h_page.h"
#if APP_SALARY_EN
#include "demo_salary_calculator.h"
#endif
#if APP_MAP_EN
#include "demo_location.h"
#endif
#if APP_MP3_EN
#include "demo_mp3.h"
#endif
#include "liot_gpio2.h"
#include "liot_lcd.h"
#include "liot_sleep.h"
#include "liot_tp.h"
#include "liot_gpio2.h"
#include "lvgl.h"

#include "gui_guider.h"

liot_task_t lvgl_task_handle = NULL;
liot_sem_t lvgl_ready_sem = NULL;
liot_timer_t lvgl_tick_timer = NULL;
osMessageQueueId_t touchMsgid = NULL;
static volatile bool g_touch_release_pending = false;

liot_lcd_handle_t lv_lcd = NULL;
liot_tp_handle_t lv_tp = NULL;


uint8_t *lvgl_buf0 = NULL;
uint8_t *lvgl_buf1 = NULL;

static lv_disp_drv_t disp_drv;  
static lv_disp_draw_buf_t draw_buf;
static lv_indev_drv_t indev_drv;
lv_indev_t * indev_touchpad;



lv_ui guider_ui = {0};

LIOT_ADD_DISPLAY(liot_gc9b72_dev); 
LIOT_ADD_TP_DEV(g_liot_tp_bl6178);


static void lvgl_tick_timer_callback(void *ctx)
{
    lv_tick_inc(1);
}

void lvgl_tick_timer_init(void)
{
    liot_rtos_timer_create(&lvgl_tick_timer, LIOT_TimerPeriodic, lvgl_tick_timer_callback, NULL);
    liot_rtos_timer_start(lvgl_tick_timer, 1);
}

static void lcd_event_callback(void) 
{
    // liot_trace("lcd_event_callback");
}

void lcd_disp_init(void)
{
    liot_hal_lcdDev_t *lcdDev = &liot_gc9b72_dev;

    liot_lcd_config_t cfg = {
        .interface = {
            .type = LIOT_LCD_INTERFACE_QSPI,
            .qspi = {
                .num = LIOT_LSPI_PORT2,
                .cmd_line = LIOT_QSPI_DATA_LINE_1,
                .data_line = LIOT_QSPI_DATA_LINE_4,
                .instruction = 0x32,
                .speed = LIOT_LSPI_51MHZ,
                .sync = true,
                .cs = LIOT_LSPI_CS0,
                .cb = lcd_event_callback,
            },
            .blk.type = LIOT_LCD_BACKLIGHT_PWM,
            .blk.pin = 100,
            .blk.pwm_num = LIOT_PWM_0,
            .rst.pin = 78,
            .rst.delay = 100,
        },
        .lcdDev = lcdDev,
    };

    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    LiotSleepModeCfg_t mode_cfg = {LIOT_SLEEP_MODE_NORMAL};
    Liot_SleepSetMode(&mode_cfg);

    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);
    Liot_GpioInit(L_GPIO_27, L_IO_OUTPUT, L_IO_HIGH, NULL);

    lv_lcd = liot_lcd_init(&cfg);

    liot_lcd_set_brightness(lv_lcd, 80);

    liot_lcd_clear_screen(lv_lcd, WHITE);
    liot_lcd_refresh(lv_lcd);
}

void lvgl_lcd_disp_flush(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t *buf)
{
    liot_lcd_write(lv_lcd, x1, y1, x2, y2, (uint8_t *)buf);
    // osDelay(1);
    // liot_trace("flush %d %d %d %d", x1, y1, x2, y2);
    // liot_trace("%X %X %X %X %X %X %X %X", buf[0], buf[1], buf[2], buf[3],buf[4], buf[5], buf[6], buf[7]);
    lv_disp_flush_ready(&disp_drv);
    
}

void lvgl_disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    lvgl_lcd_disp_flush(area->x1, area->y1, area->x2, area->y2, (uint8_t *)color_p);
}

void lvgl_disp_init()
{
    lcd_disp_init();
    lv_disp_drv_init(&disp_drv); 

    lvgl_buf0 = malloc(360 * 360 * 2);
    lvgl_buf1 = malloc(360 * 360 * 2);
    if(!lvgl_buf1 || !lvgl_buf0)
    {
        liot_trace("malloc lvgl buf failed");
    }
    lv_disp_draw_buf_init(&draw_buf, lvgl_buf0, lvgl_buf1, 360 * 360);

    disp_drv.hor_res = 360;
    disp_drv.ver_res = 360;

    disp_drv.flush_cb = lvgl_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.full_refresh = 0;
    disp_drv.direct_mode = 1;
    lv_disp_drv_register(&disp_drv);
}

void tp_touch_callback(liot_tp_touch_data_t *data, void *ctx)
{
    liot_tp_touch_data_t stale_data;

    (void)ctx;
    if (data == NULL || touchMsgid == NULL) return;

    /* Keep MOVE as a latest-value sample instead of a FIFO. Feeding LVGL
     * historical coordinates makes short/fast swipes look slow or too short. */
    if (data->touch_cnt == 0) {
        g_touch_release_pending = true;
        return;
    }
    g_touch_release_pending = false;

    while (osMessageQueueGet(touchMsgid, &stale_data, NULL, 0) == osOK) {}
    osMessageQueuePut(touchMsgid, data, 0, 0);
}
void lvgl_touchpad_init(void)
{
    liot_tp_config_t tp_cfg = {
        .interface_type = LIOT_TP_IF_I2C,
        .i2c = {
            .num      = 1,
            .sda      = 66,
            .scl      = 57,
            .addr     = (0x2c),
            .scl_func = 3,
            .sda_func = 2,
        },
        .rst = {
            .pin        = 28,
            .delay_ms   = 100,
            .active_low = 1,
        },
        .int_pin = {
            .pin    = 19,
            .signal = L_INT_EDGE_FALL,
            .pull   = LIOT_FORCE_PULL_UP,
        },
        .sensor = &g_liot_tp_bl6178,
        .fw_auto_update = 0,
    };

    touchMsgid = osMessageQueueNew(8, sizeof(liot_tp_touch_data_t), NULL);

    lv_tp = liot_tp_init(&tp_cfg);
    if (!lv_tp) {
        liot_trace("[TP] init failed!");
    }
    liot_trace("[TP] init ok");

    liot_tp_register_int_callback(lv_tp,
                                    tp_touch_callback,
                                    NULL,
                                    NULL, NULL);

    liot_tp_enable_int(lv_tp, true);
    liot_trace("[TP] INT enabled, waiting for touch...");
}

/*Get the x and y coordinates if the touchpad is pressed*/
static void lvgl_touchpad_get_xy(liot_tp_touch_data_t *data, lv_coord_t * x, lv_coord_t * y)
{
    *x = (lv_coord_t)data->point[0].x;
    *y = (lv_coord_t)data->point[0].y;
}

void lvgl_touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;
    static lv_indev_state_t last_state = LV_INDEV_STATE_REL;
    static bool release_deferred = false;

    liot_tp_touch_data_t tp_data = {0};
    osStatus_t ret;

    if (release_deferred) {
        release_deferred = false;
        last_state = LV_INDEV_STATE_REL;
        data->state = last_state;
        data->point.x = last_x;
        data->point.y = last_y;
        return;
    }

    if (g_touch_release_pending) {
        bool final_sample = false;

        /* A fast swipe can finish between two LVGL reads. Feed its final
         * pressed coordinate first and defer RELEASE until the next read. */
        while (osMessageQueueGet(touchMsgid, &tp_data, NULL, 0) == osOK) {
            if (tp_data.touch_cnt > 0) {
                lvgl_touchpad_get_xy(&tp_data, &last_x, &last_y);
                final_sample = true;
            }
        }
        g_touch_release_pending = false;
        if (final_sample) {
            last_state = LV_INDEV_STATE_PR;
            release_deferred = true;
        } else {
            last_state = LV_INDEV_STATE_REL;
        }
        data->state = last_state;
        data->point.x = last_x;
        data->point.y = last_y;
        return;
    }
    ret = osMessageQueueGet(touchMsgid, &tp_data, NULL, 1);
    if (ret == osOK) {
        if (tp_data.touch_cnt > 0 &&
            (tp_data.point[0].event == LIOT_TP_EVT_DOWN ||
             tp_data.point[0].event == LIOT_TP_EVT_MOVE)) {
            lvgl_touchpad_get_xy(&tp_data, &last_x, &last_y);
            last_state = LV_INDEV_STATE_PR;
        } else {
            last_state = LV_INDEV_STATE_REL;
        }
    }
    data->state = last_state;

    // liot_trace("x=%d y=%d data->state %d", last_x, last_y, data->state);

    data->point.x = last_x;
    data->point.y = last_y;
}



void lvgl_index_init()
{
    lvgl_touchpad_init();
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touchpad_read;
    indev_drv.long_press_time = 600;
    indev_drv.gesture_limit = 40;
    indev_drv.gesture_min_velocity = 1;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
}

void lvgl_gui_setup(void)
{
    if (APP_SINGLE_MODE && (strcmp(APP_BUILD_MODE, "app") == 0)) {
        init_scr_del_flag(&guider_ui);
        init_keyboard(&guider_ui);
#if APP_WATCHFACE_EN
        setup_scr_time(&guider_ui);
        lv_scr_load(guider_ui.time);
#elif APP_BAJI_EN
        setup_scr_baji(&guider_ui);
        baji_photo_page_enter();
#elif APP_ATTITUDE_EN
        setup_scr_attuition(&guider_ui);
        sc7a20h_page_enter_async();
#elif APP_SALARY_EN
        setup_scr_salary(&guider_ui);
        salary_calculator_page_enter_async();
#elif APP_MAP_EN
        setup_scr_positioning(&guider_ui);
        demo_location_page_enter_async();
#elif APP_MP3_EN
        setup_scr_mp3(&guider_ui);
        demo_mp3_page_enter_async();
#elif APP_ATTFUN_EN
        setup_scr_attfun(&guider_ui);
        lv_scr_load(guider_ui.attfun);
#endif
    } else {
        setup_ui(&guider_ui);
    }
    // lv_demo_benchmark();
    // lv_demo_music();
}

void lvgl_task()
{
    lvgl_tick_timer_init();
    // lv_log_register_print_cb(lv_log_print);
    lv_init();
    lvgl_disp_init();
#ifdef HWDEMO_MP3_EN
    liot_trace("[audio] MP3 preinit before touch begin");
    demo_mp3_audio_preinit();
#endif

    lvgl_index_init();
    {
        UINT8 powerup_reason = LIOT_PWRUP_UNKNOWN;
        liot_power_errcode_e power_ret = liot_get_powerup_reason(&powerup_reason);
        liot_trace("[boot-late] powerup reason=%u ret=%d (wdg=%u panic=%u)",
                   (unsigned int)powerup_reason, (int)power_ret,
                   (unsigned int)LIOT_PWRUP_WDG, (unsigned int)LIOT_PWRUP_PANIC);
    }
    // lv_port_fs_init();
    lvgl_gui_setup();


    liot_rtos_semaphore_release(lvgl_ready_sem);
    while(1)
    {
        lv_task_handler();
        liot_rtos_task_sleep_ms(1);
    }
}

void lvgl_init()
{
    liot_rtos_semaphore_create(&lvgl_ready_sem, 0);
    liot_rtos_task_create(&lvgl_task_handle, 1024 * 50, APP_PRIORITY_HIGH, "lvgl_task", &lvgl_task, NULL);
    liot_rtos_semaphore_wait(lvgl_ready_sem, osWaitForever);
}
