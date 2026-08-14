/**
 * @file 0demo_main.c
 * @brief Demo mode entry point - hardware validation
 *
 * Starts all hardware-ready modules as independent tasks.
 * Each module is guarded by HW_XXX_READY, set in the config file.
 * A module marked 'n' is excluded from compilation entirely,
 * so an unstable module cannot affect the rest of the system.
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-01-01
 * @version 1.0
 */

#include "liot_log.h"
#include "liot_os.h"

#ifdef HWDEMO_KEY_EN
#include "demo_key.h"
#endif

#ifdef HWDEMO_LCD_SSD1306_EN
#include "demo_lcd_ssd1306.h"
#endif

#ifdef HWDEMO_LCD_EN
void liot_lcd_demo_thread(void *argv);
#endif

#ifdef HWDEMO_CAMERA_EN
void liot_camera_demo_thread(void *argv);
#endif

#ifdef HWDEMO_TP_EN
void liot_tp_demo_thread(void *argv);
#endif

#ifdef HWDEMO_SOUND_EN
void liot_sound_demo_thread(void *argv);
#endif

#ifdef HWDEMO_WS2812B_EN
#include "demo_ws2812b.h"
#endif

void user_main(void)
{
    liot_trace("NT26FxDx_OpenKit demo start");

#ifdef HWDEMO_KEY_EN
    liot_task_t key_handle = NULL;
    liot_rtos_task_create(&key_handle, 4096, LIOT_APP_TASK_PRIORITY,
                          "demo_key", demo_key_task, NULL);
#endif

#ifdef HWDEMO_LCD_SSD1306_EN
    liot_task_t lcd_handle = NULL;
    liot_rtos_task_create(&lcd_handle, 10240, LIOT_APP_TASK_PRIORITY,
                          "demo_lcd_ssd1306", demo_lcd_ssd1306_task, NULL);
#endif

#ifdef HWDEMO_LCD_EN
    liot_task_t lcd_demo_handle = NULL;
    liot_rtos_task_create(&lcd_demo_handle, 10240, LIOT_APP_TASK_PRIORITY,
                          "liot_lcd_demo_thread", liot_lcd_demo_thread, NULL);
#endif

#ifdef HWDEMO_CAMERA_EN
    liot_task_t camera_demo_handle = NULL;
    liot_rtos_task_create(&camera_demo_handle, 10240, LIOT_APP_TASK_PRIORITY,
                                                         "liot_camera_demo_thread", liot_camera_demo_thread, NULL);
#endif

#ifdef HWDEMO_TP_EN
    liot_task_t tp_demo_handle = NULL;
    liot_rtos_task_create(&tp_demo_handle, 10240, LIOT_APP_TASK_PRIORITY,
                          "liot_tp_demo_thread", liot_tp_demo_thread, NULL);
#endif

#ifdef HWDEMO_SOUND_EN
    liot_task_t sound_demo_handle = NULL;
    liot_rtos_task_create(&sound_demo_handle, 10240, LIOT_APP_TASK_PRIORITY,
                          "liot_sound_demo_thread", liot_sound_demo_thread, NULL);
#endif

#ifdef HWDEMO_WS2812B_EN
    liot_task_t ws2812b_handle = NULL;
    liot_rtos_task_create(&ws2812b_handle, 4096, LIOT_APP_TASK_PRIORITY,
                          "demo_ws2812b", demo_ws2812b_task, NULL);
#endif
}
