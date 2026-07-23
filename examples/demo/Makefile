DEMO_DIR := examples/demo
DEMO_SRC := $(DEMO_DIR)/src

CFLAGS_INC	+= -I $(TOP)/$(DEMO_DIR)/inc

include $(TOP)/$(DEMO_DIR)/config

DEMO_COBJSTEMP += $(DEMO_SRC)/0demo_main.o

ifeq ($(EXDEMO_ADC_EN), y)
CFLAGS_DEFS += -DAPPDEMO_ADC_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_adc.o
endif

ifeq ($(EXDEMO_DEV_EN), y)
CFLAGS_DEFS += -DAPPDEMO_DEV_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_dev.o
endif

ifeq ($(EXDEMO_FS_EN), y)
CFLAGS_DEFS += -DAPPDEMO_FS_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_fs.o
endif

ifeq ($(EXDEMO_I2C_EN), y)
CFLAGS_DEFS += -DAPPDEMO_I2C_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_i2c.o
endif

ifeq ($(EXDEMO_USB_EN), y)
CFLAGS_DEFS += -DAPPDEMO_USB_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_usb.o
endif

ifeq ($(EXDEMO_CMSIS_EN), y)
CFLAGS_DEFS += -DAPPDEMO_CMSIS_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_cmsis.o
endif

ifeq ($(EXDEMO_NW_EN), y)
CFLAGS_DEFS += -DAPPDEMO_NW_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_nw.o
endif

ifeq ($(EXDEMO_HTTP_FOTA_EN), y)
CFLAGS_DEFS += -DAPPDEMO_HTTP_FOTA_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_http_fota.o
endif

ifeq ($(EXDEMO_SOCKET_EN), y)
CFLAGS_DEFS += -DAPPDEMO_SOCKET_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_sockets.o
endif

ifeq ($(EXDEMO_MQTT_ALI_EN), y)
CFLAGS_DEFS += -DAPPDEMO_MQTT_ALI_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_mqtt_ali.o
endif

ifeq ($(EXDEMO_WIFISCAN_EN), y)
CFLAGS_DEFS += -DAPPDEMO_WIFISCAN_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_wifiscan.o
endif

ifeq ($(EXDEMO_PWM_EN), y)
CFLAGS_DEFS += -DAPPDEMO_PWM_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_pwm.o
endif

ifeq ($(EXDEMO_PING_EN), y)
CFLAGS_DEFS += -DAPPDEMO_PING_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_ping.o
endif

ifeq ($(EXDEMO_USBNET_EN), y)
CFLAGS_DEFS += -DAPPDEMO_USBNET_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_usbnet.o
endif

ifeq ($(EXDEMO_SPI_EN), y)
CFLAGS_DEFS += -DAPPDEMO_SPI_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_spi.o
endif

ifeq ($(EXDEMO_SIM_EN), y)
CFLAGS_DEFS += -DAPPDEMO_SIM_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_sim.o
endif

ifeq ($(EXDEMO_RTC_EN), y)
CFLAGS_DEFS += -DAPPDEMO_RTC_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_rtc.o
endif

ifeq ($(EXDEMO_NTP_EN), y)
CFLAGS_DEFS += -DAPPDEMO_NTP_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_ntp.o
endif

ifeq ($(EXDEMO_LCD_EN), y)
CFLAGS_DEFS += -DAPPDEMO_LCD_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_lcd.o
endif

ifeq ($(EXDEMO_CAMERA_EN), y)
CFLAGS_DEFS += -DAPPDEMO_CAMERA_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_camera.o
endif

ifeq ($(EXDEMO_TP_EN), y)
CFLAGS_DEFS += -DAPPDEMO_TP_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_tp.o
endif

ifeq ($(EXDEMO_LBS_EN), y)
CFLAGS_DEFS += -DAPPDEMO_LBS_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_lbs.o
endif

ifeq ($(EXDEMO_GNSS_EN), y)
CFLAGS_DEFS += -DAPPDEMO_GNSS_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_gnss.o
endif

ifeq ($(EXDEMO_FTP_EN), y)
CFLAGS_DEFS += -DAPPDEMO_FTP_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_ftp.o
endif

ifeq ($(EXDEMO_FLASH_EN), y)
CFLAGS_DEFS += -DAPPDEMO_FLASH_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_flash.o
endif

ifeq ($(EXDEMO_KEYPAD_EN), y)
CFLAGS_DEFS += -DAPPDEMO_KEYPAD_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_keypad.o
endif

ifeq ($(EXDEMO_PWMAUD_EN), y)
CFLAGS_DEFS += -DAPPDEMO_PWMAUD_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_pwmaud.o
endif

ifeq ($(EXDEMO_APWM_EN), y)
CFLAGS_DEFS += -DAPPDEMO_APWM_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_apwm.o
endif

ifeq ($(EXDEMO_GPIO2_EN), y)
CFLAGS_DEFS += -DAPPDEMO_GPIO2_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_gpio2.o
endif

ifeq ($(EXDEMO_APPFOTA_EN), y)
CFLAGS_DEFS += -DAPPDEMO_APPFOTA_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_app_fota.o
endif

ifeq ($(EXDEMO_FOTA2_EN), y)
CFLAGS_DEFS += -DAPPDEMO_FOTA2_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_fota2.o
endif

ifeq ($(EXDEMO_UART2_EN), y)
CFLAGS_DEFS += -DAPPDEMO_UART2_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_uart2.o
endif

ifeq ($(EXDENO_SLEEPEX_EN), y)
CFLAGS_DEFS += -DAPPDEMO_SLEEPEX_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_sleepex.o
endif

ifeq ($(EXDEMO_DATACALL_EN), y)
CFLAGS_DEFS += -DAPPDEMO_DATACALL_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_datacall.o
endif

ifeq ($(EXDEMO_SOCKET2_EN), y)
CFLAGS_DEFS += -DAPPDEMO_SOCKET2_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_sockets2.o
endif

ifeq ($(EXDEMO_SSL2_EN), y)
CFLAGS_DEFS += -DAPPDEMO_SSL2_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_ssl2.o
endif

ifeq ($(EXDEMO_VIRTUAL_EN), y)
CFLAGS_DEFS += -DAPPDEMO_VIRTUAL_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_virtual.o
endif

ifeq ($(EXDEMO_ATCMD_EN), y)
CFLAGS_DEFS += -DAPPDEMO_ATCMD_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_atcmd.o
endif

ifeq ($(EXDEMO_OS_EN), y)
CFLAGS_DEFS += -DAPPDEMO_OS_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_os.o
endif

ifeq ($(EXDEMO_SOUND_EN), y)
CFLAGS_DEFS += -DAPPDEMO_SOUND_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_sound.o
endif

ifeq ($(EXDEMO_TTS_EN), y)
CFLAGS_DEFS += -DAPPDEMO_TTS_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_tts.o
endif

ifeq ($(EXDEMO_VOLTE_EN), y)
CFLAGS_DEFS += -DAPPDEMO_VOLTE_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_volte.o
endif

ifeq ($(EXDEMO_WS2812B_EN), y)
CFLAGS_DEFS += -DAPPDEMO_WS2812B_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_ws2812b.o
endif

ifeq ($(EXDEMO_SMS_EN), y)
CFLAGS_DEFS += -DAPPDEMO_SMS_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_sms.o
endif

ifeq ($(EXDEMO_WEBSOCKET_EN), y)
CFLAGS_DEFS += -DAPPDEMO_WEBSOCKET_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_websocket.o
endif

ifeq ($(EXDEMO_HTTP_EN), y)
CFLAGS_DEFS += -DAPPDEMO_HTTP_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_http.o
endif

ifeq ($(EXDEMO_MOTOR_EN), y)
CFLAGS_DEFS += -DAPPDEMO_MOTOR_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_motor.o
endif

ifeq ($(EXDEMO_LVGL_KEY_TCP_EN), y)
include $(TOP)/$(DEMO_SRC)/demo_lvgl_key_tcp/Makefile
endif

ifeq ($(EXDEMO_LVGL_SIMINFO_EN), y)
include $(TOP)/$(DEMO_SRC)/demo_lvgl_siminfo/Makefile
endif

ifeq ($(EXDEMO_LVGL_DUAL_EYE_EN), y)
include $(TOP)/$(DEMO_SRC)/demo_lvgl_dual_eye/Makefile
endif

ifeq ($(EXDEMO_EXTFLASH_FS_EN), y)
CFLAGS_DEFS += -DAPPDEMO_EXTFLASH_FS_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_extflash_fs.o
endif

ifeq ($(EXDEMO_EXTFLASH_EN), y)
CFLAGS_DEFS += -DAPPDEMO_EXTFLASH_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_extflash.o
endif

ifeq ($(EXDEMO_NVM_EN), y)
CFLAGS_DEFS += -DAPPDEMO_NVM_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_nvm.o
endif

ifeq ($(EXDEMO_VSIM_TGT_EN), y)
CFLAGS_DEFS += -DAPPDEMO_VSIM_TGT_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_vsim_tgt.o
endif

ifeq ($(EXDEMO_TIMER_EN), y)
CFLAGS_DEFS += -DAPPDEMO_TIMER_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_timer.o
endif

ifeq ($(EXDEMO_ZIP_EN), y)
CFLAGS_DEFS += -DAPPDEMO_ZIP_EN
DEMO_COBJSTEMP += $(DEMO_SRC)/demo_zip.o
endif

DEMO_COBJS := $(addprefix $(BUILDDIR)/, $(DEMO_COBJSTEMP))
PP_FILES += $(DEMO_COBJS:.o=.pp)

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_demo.a

$(BUILDDIR)/lib/libusr/lib_demo.a: $(DEMO_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif
