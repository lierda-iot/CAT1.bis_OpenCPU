DEMO_WATCH_DIR := $(CUSTOM_DIR)/demo/src/demo_watch

DEMO_WATCH_INC_DIRS := \
	$(DEMO_WATCH_DIR)/apps/watch_ai_app \
	$(DEMO_WATCH_DIR)/apps/watch_ai_app/modules \
	$(DEMO_WATCH_DIR)/framework/core \
	$(DEMO_WATCH_DIR)/framework/services/camera \
	$(DEMO_WATCH_DIR)/framework/services/display \
	$(DEMO_WATCH_DIR)/framework/services/player \
	$(DEMO_WATCH_DIR)/framework/services/recorder \
	$(DEMO_WATCH_DIR)/framework/services/scan \
	$(DEMO_WATCH_DIR)/framework/platform/board \
	$(DEMO_WATCH_DIR)/framework/platform/camera \
	$(DEMO_WATCH_DIR)/framework/platform/display \
	$(DEMO_WATCH_DIR)/framework/platform/player \
	$(DEMO_WATCH_DIR)/framework/platform/recorder \
	$(DEMO_WATCH_DIR)/framework/platform/scan \
	$(DEMO_WATCH_DIR)/framework/platform/osal \
	$(DEMO_WATCH_DIR)/framework/ui

DEMO_WATCH_CFILES := \
	$(TOP)/$(DEMO_WATCH_DIR)/apps/watch_ai_app/app_main.c \
	$(TOP)/$(DEMO_WATCH_DIR)/apps/watch_ai_app/user_main.c \
	$(TOP)/$(DEMO_WATCH_DIR)/apps/watch_ai_app/modules/watch_action.c \
	$(TOP)/$(DEMO_WATCH_DIR)/apps/watch_ai_app/modules/watch_audio.c \
	$(TOP)/$(DEMO_WATCH_DIR)/apps/watch_ai_app/modules/watch_camera.c \
	$(TOP)/$(DEMO_WATCH_DIR)/apps/watch_ai_app/modules/watch_camera_profile.c \
	$(TOP)/$(DEMO_WATCH_DIR)/apps/watch_ai_app/modules/watch_camera_session.c \
	$(TOP)/$(DEMO_WATCH_DIR)/apps/watch_ai_app/modules/watch_display.c \
	$(TOP)/$(DEMO_WATCH_DIR)/apps/watch_ai_app/modules/watch_feature.c \
	$(TOP)/$(DEMO_WATCH_DIR)/apps/watch_ai_app/modules/watch_gif.c \
	$(TOP)/$(DEMO_WATCH_DIR)/apps/watch_ai_app/modules/watch_network.c \
	$(TOP)/$(DEMO_WATCH_DIR)/apps/watch_ai_app/modules/watch_page.c \
	$(TOP)/$(DEMO_WATCH_DIR)/apps/watch_ai_app/modules/watch_player.c \
	$(TOP)/$(DEMO_WATCH_DIR)/apps/watch_ai_app/modules/watch_protocol.c \
	$(TOP)/$(DEMO_WATCH_DIR)/apps/watch_ai_app/modules/watch_recorder.c \
	$(TOP)/$(DEMO_WATCH_DIR)/apps/watch_ai_app/modules/watch_scan.c \
	$(TOP)/$(DEMO_WATCH_DIR)/apps/watch_ai_app/modules/watch_session.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/core/app_core.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/core/app_event.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/core/app_state.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/services/camera/app_camera_service.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/services/display/app_display_service.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/services/player/app_player_service.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/services/recorder/app_recorder_service.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/services/scan/app_scan_service.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/board/board_stub.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/board/board_lierda_watch.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/camera/app_camera_port_liot.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/display/app_display_port_lvgl.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/display/app_display_port_lvgl_ui.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/display/app_display_port_lvgl_home.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/display/app_display_port_lvgl_camera.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/display/app_display_port_lvgl_gif.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/display/app_display_port_lvgl_player.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/display/app_display_port_lvgl_recorder.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/display/app_boot_logo_liot.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/player/app_player_port_liot.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/recorder/app_recorder_port_liot.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/scan/app_scan_port_liot.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/osal/osal_liot.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/ui/ui_ai_basic.c

CFLAGS_INC += $(patsubst %,-I $(TOP)/%,$(DEMO_WATCH_INC_DIRS))
CFLAGS_DEFS += -DAPP_DISPLAY_LVGL_ENABLE_LIOT_LCD=1
CFLAGS_DEFS += -DAPP_CAMERA_LIOT_ENABLE_GC032A=1

CFLAGS_DEFS += -DAPP_DISPLAY_LVGL_ENABLE_GIF=1
CFLAGS_DEFS += -DWATCH_AI_ENABLE_GIF=1
DEMO_WATCH_CFILES += \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/display/app_display_port_lvgl_gif_player.c \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/display/assets/watch_gif_demo.c

BUILD_COMP_THIRDPART_EN := y
BUILD_THPART_CJSON_ENABLE := n
BUILD_THPART_LWIP_ENABLE := n
BUILD_THPART_MBEDTLS_ENABLE := n
BUILD_THPART_WEBSOCKET_ENABLE := n
BUILD_THPART_OPUS_ENABLE := n
BUILD_THPART_LVGL_ENABLE := y
LVGL_CFILES_REL = $(LVGL_CFILES)
EXDEMO_LVGL_USE_SOURCE_LIB := y
DEMO_WATCH_LV_CONF := \
	$(TOP)/$(DEMO_WATCH_DIR)/framework/platform/display/lvgl/lv_conf.h
CFLAGS_DEFS += -DLV_CONF_PATH=$(DEMO_WATCH_LV_CONF)

BUILD_DRIVER_LCD_EN := y
BUILD_DRIVER_CAMERA_EN := y
BUILD_DRIVER_TP_EN := y
BUILD_DRIVER_GX8006_EN := n
BUILD_DRIVER_WS2812B_EN := n
BUILD_COMP_TTS_EN := y
TTS_RESOURCE_TYPE ?= 8k_zh
CODE_REGION_ADJUST ?= 0x9A000
BUILD_PRESET_FILE_EXTFLASHLFS_EN := y
EXT_LFS_BIN ?= $(TOP)/components/presetfile/extflashlfs/ext_lfs.bin
EXFLASH_FILES_ROOT_DIR :=


CUSTOM_COBJSTEMP := $(patsubst $(TOP)/%.c,%.o,$(DEMO_WATCH_CFILES))
PP_FILES += $(addprefix $(BUILDDIR)/,$(CUSTOM_COBJSTEMP:.o=.pp))
