#include "liot_os.h"
#include "liot_log.h"

void user_main(void)
{
    liot_trace("demoTestInit start");

    #ifdef APPDEMO_FS_EN
    void liot_fs_demo_task(void *argv);
    liot_task_t fstaskhandle = NULL;
    liot_rtos_task_create(&fstaskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_fs_demo_task", &liot_fs_demo_task, NULL);
    #endif

    #ifdef APPDEMO_CMSIS_EN
    void liot_cmsis_demo_thread(void *argv);
    liot_task_t cmsistaskhandle = NULL;
    liot_rtos_task_create(&cmsistaskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_cmsis_demo_thread", &liot_cmsis_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_ADC_EN
    void liot_adc_demo_thread(void *argv);
    liot_task_t adctaskhandle = NULL;
    liot_rtos_task_create(&adctaskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_adc_demo_thread", &liot_adc_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_DEV_EN
    void liot_dev_demo_thread(void *argv);
    liot_task_t devtaskhandle = NULL;
    liot_rtos_task_create(&devtaskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_dev_demo_thread", &liot_dev_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_UART2_EN
    void liot_uart2_demo_thread(void *argv);
    liot_task_t uart2taskhandle = NULL;
    liot_rtos_task_create(&uart2taskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_uart2_demo_thread", &liot_uart2_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_I2C_EN
    void liot_i2c_demo_thread(void *argv);
    liot_task_t i2ctaskhandle = NULL;
    liot_rtos_task_create(&i2ctaskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_i2c_demo_thread", &liot_i2c_demo_thread, NULL);
    #endif
   
    #ifdef APPDEMO_USB_EN
    void liot_usb_demo_thread(void *argv);
    liot_task_t usbtaskhandle = NULL;
    liot_rtos_task_create(&usbtaskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_usb_demo_thread", &liot_usb_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_NW_EN
    void liot_nw_demo_thread(void *argv);
    liot_task_t nwtaskhandle = NULL;
    liot_rtos_task_create(&nwtaskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_nw_demo_thread", &liot_nw_demo_thread, NULL);
    #endif
    
    #ifdef APPDEMO_HTTP_FOTA_EN
    void liot_fota_http_nvm_thread(void *argv);
    liot_task_t fotahttptskhandle = NULL;
    liot_rtos_task_create(&fotahttptskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_fota_http_nvm_thread", &liot_fota_http_nvm_thread, NULL);
    #endif

    #ifdef APPDEMO_SOCKET_EN
    void liot_sockets_demo_thread(void *argv);
    liot_task_t lwiptskhandle = NULL;
    liot_rtos_task_create(&lwiptskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_sockets_demo_thread", &liot_sockets_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_MQTT_ALI_EN
    void liot_mqtt_ali_app_thread(void *argv);
    liot_task_t mqttalitskhandle = NULL;
    liot_rtos_task_create(&mqttalitskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_mqtt_ali_app_thread", &liot_mqtt_ali_app_thread, NULL);
    #endif

    #ifdef APPDEMO_WIFISCAN_EN
    void liot_wifiscan_demo_thread(void *argv);
    liot_task_t wifiscantskhandle = NULL;
    liot_rtos_task_create(&wifiscantskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_wifiscan_demo_thread", &liot_wifiscan_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_PWM_EN
    void liot_pwm_demo_thread(void *argv);
    liot_task_t pwmtskhandle = NULL;
    liot_rtos_task_create(&pwmtskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_pwm_demo_thread", &liot_pwm_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_PING_EN
    void liot_ping_app_thread(void *argv);
    liot_task_t pingtskhandle = NULL;
    liot_rtos_task_create(&pingtskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_ping_app_thread", &liot_ping_app_thread, NULL);
    #endif

    #ifdef APPDEMO_USBNET_EN
    void liot_usbnet_demo_thread(void *argv);
    liot_task_t usbnettskhandle = NULL;
    liot_rtos_task_create(&usbnettskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_usbnet_demo_thread", &liot_usbnet_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_SPI_EN
    void liot_spi_demo_thread(void *argv);
    liot_task_t spitskhandle = NULL;
    liot_rtos_task_create(&spitskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_spi_demo_thread", &liot_spi_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_SIM_EN
    void liot_sim_demo_thread(void *argv);
    liot_task_t simtskhandle = NULL;
    liot_rtos_task_create(&simtskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_sim_demo_thread", &liot_sim_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_RTC_EN
    void liot_rtc_demo_thread(void *argv);
    liot_task_t rtctskhandle = NULL;
    liot_rtos_task_create(&rtctskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_rtc_demo_thread", &liot_rtc_demo_thread, NULL);
    #endif
    
    #ifdef APPDEMO_NTP_EN
    void liot_ntp_demo_thread(void *argv);
    liot_task_t ntptskhandle = NULL;
    liot_rtos_task_create(&ntptskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_ntp_demo_thread", &liot_ntp_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_LCD_EN
    void liot_lcd_demo_thread(void *argv);
    liot_task_t lcdtskhandle = NULL;
    liot_rtos_task_create(&lcdtskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_lcd_demo_thread", &liot_lcd_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_TP_EN
    void liot_tp_demo_thread(void *argv);
    liot_task_t tptskhandle = NULL;
    liot_rtos_task_create(&tptskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_tp_demo_thread", &liot_tp_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_CAMERA_EN
    void liot_camera_demo_thread(void *argv);
    liot_task_t cameratskhandle = NULL;
    liot_rtos_task_create(&cameratskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_camera_demo_thread", &liot_camera_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_LBS_EN
    void liot_lbs_demo_thread(void *argv);
    liot_task_t lbstskhandle = NULL;
    liot_rtos_task_create(&lbstskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_lbs_demo_thread", &liot_lbs_demo_thread, NULL);
    #endif
    
    #ifdef APPDEMO_FTP_EN
    void liot_ftp_demo_thread(void *argv);
    liot_task_t ftptskhandle = NULL;
    liot_rtos_task_create(&ftptskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_ftp_demo_thread", &liot_ftp_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_FLASH_EN
    void liot_flash_demo_task(void *argv);
    liot_task_t flashtskhandle = NULL;
    liot_rtos_task_create(&flashtskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_flash_demo_task", &liot_flash_demo_task, NULL);
    #endif

    #ifdef APPDEMO_KEYPAD_EN
    void liot_keypad_demo_thread(void *argv);
    liot_task_t keypadtskhandle = NULL;
    liot_rtos_task_create(&keypadtskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_keypad_demo_thread", &liot_keypad_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_PWMAUD_EN
    void liot_pwm_tts_demo_thread(void *argv);
    liot_task_t pwmAudtskhandle = NULL;
    liot_rtos_task_create(&pwmAudtskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_pwm_tts_demo_thread", &liot_pwm_tts_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_APWM_EN
    void liot_apwm_demo_thread(void *argv);
    liot_task_t apwmtskhandle = NULL;
    liot_rtos_task_create(&apwmtskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_apwm_app_thread", &liot_apwm_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_GPIO2_EN
    void demo_gpio2_thread(void *argv);
    liot_task_t gpio2tskhandle = NULL;
    liot_rtos_task_create(&gpio2tskhandle, 10240, LIOT_APP_TASK_PRIORITY, "demo_gpio2_thread", &demo_gpio2_thread, NULL);
    #endif

    #ifdef APPDEMO_APPFOTA_EN
    void liot_fota_demo_thread(void *argv);
    liot_task_t appfotatskhandle = NULL;
    liot_rtos_task_create(&appfotatskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_appfota_demo_thread", &liot_fota_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_FOTA2_EN
    void liot_fota2_demo_thread(void *argv);
    liot_task_t appfotatskhandle = NULL;
    liot_rtos_task_create(&appfotatskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_fota2_demo_thread", &liot_fota2_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_SLEEPEX_EN
    void liot_sleepex_demo_thread(void *argv);
    liot_task_t sleepexskhandle = NULL;
    liot_rtos_task_create(&sleepexskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_sleepex_demo_thread", &liot_sleepex_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_DATACALL_EN
    void liot_datacall_demo_thread(void *argv);
    liot_task_t datacalltskhandle = NULL;
    liot_rtos_task_create(&datacalltskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_datacall_demo_thread", &liot_datacall_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_SOCKET2_EN
    void liot_socket2_demo_thread(void *argv);
    liot_task_t socket2handle = NULL;
    liot_rtos_task_create(&socket2handle, 10240, LIOT_APP_TASK_PRIORITY, "liot_socket2_demo_thread", &liot_socket2_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_SSL2_EN
    void liot_ssl2_demo_thread(void *argv);
    liot_task_t ssl2handle = NULL;
    liot_rtos_task_create(&ssl2handle, 10240, LIOT_APP_TASK_PRIORITY, "liot_ssl2_demo_thread", &liot_ssl2_demo_thread, NULL);
    #endif
    
    #ifdef APPDEMO_VIRTUAL_EN
    void liot_virtual_demo_thread(void *argv);
    liot_task_t virtualtskhandle = NULL;
    liot_rtos_task_create(&virtualtskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_virtual_demo_thread", &liot_virtual_demo_thread, NULL);
    #endif
    
    #ifdef APPDEMO_ATCMD_EN
    void liot_atcmd_init_demo_thread(void *argv);
    liot_task_t atcmdtskhandle = NULL;
    liot_rtos_task_create(&atcmdtskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_atcmd_init_demo_thread", &liot_atcmd_init_demo_thread, NULL);    
    #endif

    #ifdef APPDEMO_OS_EN
    void liot_os_demo_thread(void *argv);
    liot_task_t ostaskhandle = NULL;
    liot_rtos_task_create(&ostaskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_os_demo_thread", &liot_os_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_SOUND_EN
    void liot_sound_demo_thread(void *argv);
    liot_task_t soundtaskhandle = NULL;
    liot_rtos_task_create(&soundtaskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_sound_demo_thread", &liot_sound_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_TTS_EN
    void liot_tts_demo_thread(void *argv);
    liot_task_t ttstaskhandle = NULL;
    liot_rtos_task_create(&ttstaskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_tts_demo_thread", &liot_tts_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_VOLTE_EN
    void liot_volte_demo_task(void *argv);
    liot_task_t voltetaskhandle = NULL;
    liot_rtos_task_create(&voltetaskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_volte_demo_task", &liot_volte_demo_task, NULL);
    #endif

    #ifdef APPDEMO_WS2812B_EN
    void liot_ws2812b_demo_thread(void *argv);
    liot_task_t ws2812btskhandle = NULL;
    liot_rtos_task_create(&ws2812btskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_ws2812b_demo_thread", &liot_ws2812b_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_SMS_EN
    void liot_sms_demo_task(void *argv);
    liot_task_t smstaskhandle = NULL;
    liot_rtos_task_create(&smstaskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_sms_demo_task", &liot_sms_demo_task, NULL);
    #endif
    
    #ifdef APPDEMO_WEBSOCKET_EN
    void liot_websocket_demo_thread(void *argv);
    liot_task_t websockettaskhandle = NULL;
    liot_rtos_task_create(&websockettaskhandle, 20*1024, LIOT_APP_TASK_PRIORITY, "liot_websocket_demo_thread", &liot_websocket_demo_thread, NULL);
    #endif
    
    #ifdef APPDEMO_HTTP_EN
    void liot_http_demo_thread(void *argv);
    liot_task_t httptaskhandle = NULL;
    liot_rtos_task_create(&httptaskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_http_demo_thread", &liot_http_demo_thread, NULL);
    #endif

    #ifdef APPDEMO_MOTOR_EN
    void liot_motor_demo_task(void *argv);
    liot_task_t motortskhandle = NULL;
    liot_rtos_task_create(&motortskhandle, 10240, LIOT_APP_TASK_PRIORITY, "liot_motor_demo_task", &liot_motor_demo_task, NULL);
    #endif

    #ifdef APPDEMO_LVGL_KEY_TCP_EN
    void demo_tgai_csq_start(void);
    void demo_tgai_key_init(void);
    void lvgl_init(void);

    lvgl_init();
    liot_rtos_task_sleep_ms(200);  
    demo_tgai_key_init();
    liot_rtos_task_sleep_ms(200);  
    demo_tgai_csq_start();
    #endif

    #ifdef APPDEMO_LVGL_SIMINFO_EN
    void demo_siminfo_csq_start(void);
    void lvgl_init(void);

    lvgl_init();
    liot_rtos_task_sleep_ms(200);
    demo_siminfo_csq_start();
    #endif
}