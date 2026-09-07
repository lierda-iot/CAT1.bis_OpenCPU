#include "demo_lvgl_photo_fs_lvgl_port.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "lierda_app_main.h"
#include "liot_gpio2.h"
#include "liot_lcd.h"
#include "liot_log.h"
#include "liot_os.h"
#include "liot_power.h"
#include "liot_sleep.h"
#include "liot_tp.h"
#include "lvgl.h"

static liot_task_t g_demo_lvgl_photo_fs_lvgl_task_handle = NULL;
static liot_sem_t g_demo_lvgl_photo_fs_lvgl_ready_sem = NULL;
static liot_timer_t g_demo_lvgl_photo_fs_lvgl_tick_timer = NULL;
static osMessageQueueId_t g_demo_lvgl_photo_fs_touch_msgid = NULL;

static liot_lcd_handle_t g_demo_lvgl_photo_fs_lcd = NULL;
static liot_tp_handle_t g_demo_lvgl_photo_fs_tp = NULL;

static uint8_t *g_demo_lvgl_photo_fs_lvgl_buf0 = NULL;
static uint8_t *g_demo_lvgl_photo_fs_lvgl_buf1 = NULL;

static lv_disp_drv_t g_demo_lvgl_photo_fs_disp_drv;
static lv_disp_draw_buf_t g_demo_lvgl_photo_fs_draw_buf;
static lv_indev_drv_t g_demo_lvgl_photo_fs_indev_drv;

LIOT_ADD_DISPLAY(liot_gc9b72_dev);
LIOT_ADD_TP_DEV(g_liot_tp_bl6178);

static void demo_lvgl_photo_fs_lvgl_tick_timer_callback(void *ctx)
{
    (void)ctx;
    lv_tick_inc(1u);
}

static void demo_lvgl_photo_fs_lvgl_tick_timer_init(void)
{
    liot_rtos_timer_create(&g_demo_lvgl_photo_fs_lvgl_tick_timer,
                           LIOT_TimerPeriodic,
                           demo_lvgl_photo_fs_lvgl_tick_timer_callback,
                           NULL);
    liot_rtos_timer_start(g_demo_lvgl_photo_fs_lvgl_tick_timer, 1u);
}

static void demo_lvgl_photo_fs_lcd_event_callback(void)
{
}

static void demo_lvgl_photo_fs_lcd_disp_init(void)
{
    liot_hal_lcdDev_t *lcd_dev = &liot_gc9b72_dev;
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
                .cb = demo_lvgl_photo_fs_lcd_event_callback,
            },
            .blk.type = LIOT_LCD_BACKLIGHT_PWM,
            .blk.pin = 100,
            .blk.pwm_num = LIOT_PWM_0,
            .rst.pin = 78,
            .rst.delay = 100,
        },
        .lcdDev = lcd_dev,
    };
    LiotSleepModeCfg_t mode_cfg = {LIOT_SLEEP_MODE_NORMAL};

    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    Liot_SleepSetMode(&mode_cfg);

    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);
    Liot_GpioInit(L_GPIO_27, L_IO_OUTPUT, L_IO_HIGH, NULL);

    g_demo_lvgl_photo_fs_lcd = liot_lcd_init(&cfg);
    liot_lcd_set_brightness(g_demo_lvgl_photo_fs_lcd, 90);
    liot_lcd_clear_screen(g_demo_lvgl_photo_fs_lcd, WHITE);
    liot_lcd_refresh(g_demo_lvgl_photo_fs_lcd);
}

static void demo_lvgl_photo_fs_lvgl_disp_flush(lv_disp_drv_t *disp_drv,
                                               const lv_area_t *area,
                                               lv_color_t *color_p)
{
    liot_lcd_write(g_demo_lvgl_photo_fs_lcd,
                   area->x1,
                   area->y1,
                   area->x2,
                   area->y2,
                   (uint8_t *)color_p);
    lv_disp_flush_ready(disp_drv);
}

static void demo_lvgl_photo_fs_lvgl_disp_init(void)
{
    demo_lvgl_photo_fs_lcd_disp_init();

    lv_disp_drv_init(&g_demo_lvgl_photo_fs_disp_drv);

    g_demo_lvgl_photo_fs_lvgl_buf0 = malloc(360u * 360u * 2u);
    g_demo_lvgl_photo_fs_lvgl_buf1 = malloc(360u * 360u * 2u);
    if ((g_demo_lvgl_photo_fs_lvgl_buf0 == NULL) || (g_demo_lvgl_photo_fs_lvgl_buf1 == NULL)) {
        liot_trace("[demo_lvgl_photo_fs_lvgl] alloc draw buffer failed");
    }

    lv_disp_draw_buf_init(&g_demo_lvgl_photo_fs_draw_buf,
                          g_demo_lvgl_photo_fs_lvgl_buf0,
                          g_demo_lvgl_photo_fs_lvgl_buf1,
                          360u * 360u);

    g_demo_lvgl_photo_fs_disp_drv.hor_res = 360;
    g_demo_lvgl_photo_fs_disp_drv.ver_res = 360;
    g_demo_lvgl_photo_fs_disp_drv.flush_cb = demo_lvgl_photo_fs_lvgl_disp_flush;
    g_demo_lvgl_photo_fs_disp_drv.draw_buf = &g_demo_lvgl_photo_fs_draw_buf;
    g_demo_lvgl_photo_fs_disp_drv.full_refresh = 1;
    g_demo_lvgl_photo_fs_disp_drv.direct_mode = 1;
    lv_disp_drv_register(&g_demo_lvgl_photo_fs_disp_drv);
}

static void demo_lvgl_photo_fs_tp_touch_callback(liot_tp_touch_data_t *data, void *ctx)
{
    uint8_t i;

    (void)ctx;
    for (i = 0u; i < data->touch_cnt; ++i) {
        osMessageQueuePut(g_demo_lvgl_photo_fs_touch_msgid, data, 0u, 0u);
    }
}

static void demo_lvgl_photo_fs_lvgl_touchpad_init(void)
{
    liot_tp_config_t tp_cfg = {
        .interface_type = LIOT_TP_IF_I2C,
        .i2c = {
            .num = 1,
            .sda = 66,
            .scl = 57,
            .addr = 0x2c,
            .scl_func = 3,
            .sda_func = 2,
        },
        .rst = {
            .pin = 28,
            .delay_ms = 100,
            .active_low = 1,
        },
        .int_pin = {
            .pin = 19,
            .signal = L_INT_EDGE_FALL,
            .pull = LIOT_FORCE_PULL_UP,
        },
        .sensor = &g_liot_tp_bl6178,
        .fw_auto_update = 0,
    };

    g_demo_lvgl_photo_fs_touch_msgid =
        osMessageQueueNew(1u, sizeof(liot_tp_touch_data_t), NULL);

    g_demo_lvgl_photo_fs_tp = liot_tp_init(&tp_cfg);
    if (g_demo_lvgl_photo_fs_tp == NULL) {
        liot_trace("[demo_lvgl_photo_fs_lvgl] tp init failed");
        return;
    }

    liot_tp_register_int_callback(g_demo_lvgl_photo_fs_tp,
                                  demo_lvgl_photo_fs_tp_touch_callback,
                                  NULL,
                                  NULL,
                                  NULL);
    liot_tp_enable_int(g_demo_lvgl_photo_fs_tp, true);
}

static bool demo_lvgl_photo_fs_lvgl_touchpad_is_pressed(liot_tp_touch_data_t *data)
{
    osStatus_t ret;

    ret = osMessageQueueGet(g_demo_lvgl_photo_fs_touch_msgid, data, NULL, 1u);
    if ((ret != 0) || (data->touch_cnt == 0u)) {
        return false;
    }

    return (data->point[0].event == LIOT_TP_EVT_DOWN) ||
           (data->point[0].event == LIOT_TP_EVT_MOVE);
}

static void demo_lvgl_photo_fs_lvgl_touchpad_get_xy(liot_tp_touch_data_t *data,
                                                    lv_coord_t *x,
                                                    lv_coord_t *y)
{
    *x = (lv_coord_t)data->point[0].x;
    *y = (lv_coord_t)data->point[0].y;
}

static void demo_lvgl_photo_fs_lvgl_touchpad_read(lv_indev_drv_t *indev_drv,
                                                  lv_indev_data_t *data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;
    static uint8_t release_cnt = 0u;
    liot_tp_touch_data_t tp_data = {0};

    (void)indev_drv;

    if (demo_lvgl_photo_fs_lvgl_touchpad_is_pressed(&tp_data)) {
        demo_lvgl_photo_fs_lvgl_touchpad_get_xy(&tp_data, &last_x, &last_y);
        data->state = LV_INDEV_STATE_PR;
        release_cnt = 0u;
    } else {
        if (release_cnt < 2u) {
            ++release_cnt;
            data->state = LV_INDEV_STATE_PR;
        } else {
            data->state = LV_INDEV_STATE_REL;
        }
    }

    data->point.x = last_x;
    data->point.y = last_y;
}

static void demo_lvgl_photo_fs_lvgl_indev_init(void)
{
    demo_lvgl_photo_fs_lvgl_touchpad_init();
    lv_indev_drv_init(&g_demo_lvgl_photo_fs_indev_drv);
    g_demo_lvgl_photo_fs_indev_drv.type = LV_INDEV_TYPE_POINTER;
    g_demo_lvgl_photo_fs_indev_drv.read_cb = demo_lvgl_photo_fs_lvgl_touchpad_read;
    lv_indev_drv_register(&g_demo_lvgl_photo_fs_indev_drv);
}

static void demo_lvgl_photo_fs_lvgl_task(void *argv)
{
    (void)argv;

    demo_lvgl_photo_fs_lvgl_tick_timer_init();
    lv_init();
    demo_lvgl_photo_fs_lvgl_disp_init();
    demo_lvgl_photo_fs_lvgl_indev_init();

    liot_rtos_semaphore_release(g_demo_lvgl_photo_fs_lvgl_ready_sem);

    while (1) {
        lv_task_handler();
        liot_rtos_task_sleep_ms(1u);
    }
}

void lvgl_init(void)
{
    if (g_demo_lvgl_photo_fs_lvgl_task_handle != NULL) {
        return;
    }

    liot_rtos_semaphore_create(&g_demo_lvgl_photo_fs_lvgl_ready_sem, 0u);
    liot_rtos_task_create(&g_demo_lvgl_photo_fs_lvgl_task_handle,
                          1024u * 50u,
                          APP_PRIORITY_HIGH,
                          "demo_lvgl_photo_fs_lvgl",
                          demo_lvgl_photo_fs_lvgl_task,
                          NULL);
    liot_rtos_semaphore_wait(g_demo_lvgl_photo_fs_lvgl_ready_sem, osWaitForever);
}
