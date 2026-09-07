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

#ifdef HWDEMO_LCD_EN
void liot_lcd_demo_thread(void *argv);
#endif

#ifdef HWDEMO_SC7A20H_EN
void liot_sc7a20h_demo_thread(void *argv);
#endif

#ifdef HWDEMO_GNSS_EN
void liot_gnss_demo_thread(void *argv);
#endif

#ifdef HWDEMO_EXTFLASH_P25Q64_EN
void liot_extflash_p25q64_demo_thread(void *argv);
#endif

#ifdef HWDEMO_EXTFLASH_FS_P25Q64_EN
void liot_extflash_fs_p25q64_demo_thread(void *argv);
#endif

#ifdef HWDEMO_LVGL_PHOTO_FS_EN
void liot_lvgl_photo_fs_demo_thread(void *argv);
#endif

#ifdef HWDEMO_SOUND_ES8375_EN
void liot_sound_es8375_demo_thread(void *argv);
#endif

#ifdef HWDEMO_SALARY_CALCULATOR_EN
void liot_salary_calculator_demo_thread(void *argv);
#endif

#ifdef HWDEMO_MP3_EN
void liot_mp3_demo_thread(void *argv);
#endif

#ifdef HWDEMO_LOCATION_EN
void liot_location_demo_thread(void *argv);
#endif


void user_main(void)
{
    liot_trace("L_CT4IT02_1698W demo start");

#ifdef HWDEMO_LCD_EN
    liot_task_t lcd_demo_handle = NULL;
    liot_rtos_task_create(&lcd_demo_handle, 10240, LIOT_APP_TASK_PRIORITY,
                            "liot_lcd_demo_thread", liot_lcd_demo_thread, NULL);
#endif

#ifdef HWDEMO_SC7A20H_EN
    liot_task_t gsensor_demo_handle = NULL;
    liot_rtos_task_create(&gsensor_demo_handle, 10240, LIOT_APP_TASK_PRIORITY,
                            "liot_sc7a20h_demo_thread", liot_sc7a20h_demo_thread, NULL);
#endif

#ifdef HWDEMO_GNSS_EN
    liot_task_t gnss_demo_handle = NULL;
    liot_rtos_task_create(&gnss_demo_handle, 10240, LIOT_APP_TASK_PRIORITY,
                            "liot_gnss_demo_thread", liot_gnss_demo_thread, NULL);
#endif

#ifdef HWDEMO_EXTFLASH_P25Q64_EN
    liot_task_t extflash_demo_handle = NULL;
    liot_rtos_task_create(&extflash_demo_handle, 10240, LIOT_APP_TASK_PRIORITY,
                            "liot_extflash_p25q64_demo_thread", liot_extflash_p25q64_demo_thread, NULL);
#endif

#ifdef HWDEMO_EXTFLASH_FS_P25Q64_EN
    liot_task_t extflash_fs_demo_handle = NULL;
    liot_rtos_task_create(&extflash_fs_demo_handle, 10240, LIOT_APP_TASK_PRIORITY,
                            "liot_extflash_fs_p25q64_demo_thread", liot_extflash_fs_p25q64_demo_thread, NULL);
#endif

#ifdef HWDEMO_LVGL_PHOTO_FS_EN
    liot_task_t lvgl_photo_fs_demo_handle = NULL;
    liot_rtos_task_create(&lvgl_photo_fs_demo_handle, 16 * 1024, LIOT_APP_TASK_PRIORITY,
                            "liot_lvgl_photo_fs_demo_thread", liot_lvgl_photo_fs_demo_thread, NULL);
#endif

#ifdef HWDEMO_SALARY_CALCULATOR_EN
    liot_task_t salary_demo_handle = NULL;
    liot_rtos_task_create(&salary_demo_handle, 16 * 1024, LIOT_APP_TASK_PRIORITY,
                            "salary_calculator_demo", liot_salary_calculator_demo_thread, NULL);
#endif

#ifdef HWDEMO_SOUND_ES8375_EN
    liot_task_t sound_es8375_demo_handle = NULL;
    liot_rtos_task_create(&sound_es8375_demo_handle, 10240, LIOT_APP_TASK_PRIORITY,
                            "liot_sound_es8375_demo_thread", liot_sound_es8375_demo_thread, NULL);
#endif

#ifdef HWDEMO_MP3_EN
    liot_task_t mp3_demo_handle = NULL;
    liot_rtos_task_create(&mp3_demo_handle, 16 * 1024, LIOT_APP_TASK_PRIORITY,
                            "liot_mp3_demo_thread", liot_mp3_demo_thread, NULL);
#endif

#ifdef HWDEMO_LOCATION_EN
    liot_task_t location_demo_handle = NULL;
    liot_rtos_task_create(&location_demo_handle, 16 * 1024, LIOT_APP_TASK_PRIORITY,
                            "liot_location_demo", liot_location_demo_thread, NULL);
#endif
}
