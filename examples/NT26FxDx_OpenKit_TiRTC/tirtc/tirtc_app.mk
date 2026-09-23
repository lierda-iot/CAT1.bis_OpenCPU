# SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai>
# SPDX-License-Identifier: Apache-2.0

# TiRTC application sources are kept outside the vendor demo tree so that the
# official demo can remain byte-for-byte restorable.
TIRTC_APP_ROOT := $(CUSTOM_DIR)/tirtc
TIRTC_SRC_ROOT := $(TIRTC_APP_ROOT)/src
TIRTC_SDK_ROOT := $(TOP)/$(TIRTC_APP_ROOT)/sdk

CFLAGS_INC += -I $(TOP)/$(TIRTC_APP_ROOT)/include
CFLAGS_INC += -I $(TIRTC_SDK_ROOT)/include

CUSTOM_COBJSTEMP := $(TIRTC_SRC_ROOT)/app/app_entry.o

BUILD_DRIVER_LCD_EN ?= y

# Fail at configuration time instead of producing a partly linked image.  The
# key task reports into the UI queue, the OLED service needs the LCD driver,
# and the managed TiRTC owner consumes the provisioned device identity.
ifeq ($(HWDEMO_KEY_EN),y)
ifneq ($(HWDEMO_LCD_SSD1306_EN),y)
$(error HWDEMO_KEY_EN=y requires HWDEMO_LCD_SSD1306_EN=y)
endif
endif

ifeq ($(HWDEMO_LCD_SSD1306_EN),y)
ifneq ($(BUILD_DRIVER_LCD_EN),y)
$(error HWDEMO_LCD_SSD1306_EN=y requires BUILD_DRIVER_LCD_EN=y)
endif
endif

ifeq ($(HWDEMO_TIRTC_EN),y)
ifneq ($(HWDEMO_BINDING_EN),y)
$(error HWDEMO_TIRTC_EN=y requires HWDEMO_BINDING_EN=y)
endif
endif

ifeq ($(HWDEMO_KEY_EN), y)
CUSTOM_COBJSTEMP += $(TIRTC_SRC_ROOT)/ui/key_input.o
CFLAGS_DEFS += -DHWDEMO_KEY_EN
endif

ifeq ($(HWDEMO_LCD_SSD1306_EN), y)
CUSTOM_COBJSTEMP += $(TIRTC_SRC_ROOT)/ui/ssd1306_display.o
CUSTOM_COBJSTEMP += $(TIRTC_SRC_ROOT)/ui/ui_controller.o
CFLAGS_DEFS += -DHWDEMO_LCD_SSD1306_EN
endif

ifeq ($(HWDEMO_NET_TIME_EN), y)
CUSTOM_COBJSTEMP += $(TIRTC_SRC_ROOT)/services/network_manager.o
CFLAGS_DEFS += -DHWDEMO_NET_TIME_EN
endif

ifeq ($(HWDEMO_BINDING_EN), y)
ifeq ($(HWDEMO_NET_TIME_EN), y)
CUSTOM_COBJSTEMP += $(TIRTC_SRC_ROOT)/services/device_binding.o
CUSTOM_COBJSTEMP += $(TIRTC_SRC_ROOT)/services/formal_mqtt.o
CFLAGS_DEFS += -DHWDEMO_BINDING_EN
ifeq ($(MODEMPKG),F6D_A)
CFLAGS_DEFS += -DDEMO_F6DA_LIOT_MQTT_RX_COMPAT
endif
else
$(error HWDEMO_BINDING_EN=y requires HWDEMO_NET_TIME_EN=y (SIM0/CID1 owner))
endif
endif

ifeq ($(HWDEMO_TIRTC_EN), y)
    CUSTOM_COBJSTEMP += $(TIRTC_SRC_ROOT)/core/tirtc_runtime.o
    CFLAGS_DEFS += -DHWDEMO_TIRTC_EN
    CFLAGS_DEFS += -DTIRTC_MAX_SEND_BUFFER_BYTES=$(TIRTC_MAX_SEND_BUFFER_BYTES)
    CFLAGS_DEFS += -DTIRTC_LIBRARY_LOG_LEVEL=$(TIRTC_LIBRARY_LOG_LEVEL)
    CFLAGS_DEFS += -DTIRTC_TRANSPORT_LOG_LEVEL=$(TIRTC_TRANSPORT_LOG_LEVEL)
    CFLAGS_DEFS += -DTIRTC_TRANSPORT_STATS_ENABLE=$(TIRTC_TRANSPORT_STATS_EN)
PREBUILDLIBS += $(TIRTC_SDK_ROOT)/lib/libTiRTC.a

# The vendor archive contains native Thumb code and LTO IR. Select its valid
# native objects with GCC 10 q4 and retain the ABI adapters in tirtc_runtime.c.
LDFLAGS += -fno-lto
LDFLAGS += -Wl,--wrap=Logf -Wl,--wrap=sha256_hmac \
    -Wl,--wrap=freertos_ThreadCreateWithStackSize \
    -Wl,--wrap=app_log_time -Wl,--wrap=printf

ifneq ($(filter y,$(TIRTC_SMOKE_RUNTIME_EN) $(HWDEMO_AI_CHAT_EN) $(HWDEMO_WECHAT_EN) $(HWDEMO_DEV_CHAT_EN) $(HWDEMO_LIVE_TALK_EN)),)
ifeq ($(TIRTC_SMOKE_RUNTIME_EN), y)
CFLAGS_DEFS += -DTIRTC_SMOKE_RUNTIME_EN
endif
TIRTC_BASE_ELF := $(TOP)/components/basePkg/$(MODEMPKG)/ap_lierda_app.elf
TIRTC_NETIF_LIST_ADDR := $(shell $(NM) -g --defined-only $(TIRTC_BASE_ELF) 2>/dev/null | grep -w "netif_list" | sed -n '1s/^\([0-9A-Fa-f]*\).*/0x\1/p')
ifeq ($(strip $(TIRTC_NETIF_LIST_ADDR)),)
$(error TiRTC requires netif_list, but it was not found in $(TIRTC_BASE_ELF))
endif
LDFLAGS += -Wl,--defsym=netif_list=$(TIRTC_NETIF_LIST_ADDR)
endif
endif

ifeq ($(HWDEMO_AI_CHAT_EN), y)
ifeq ($(HWDEMO_TIRTC_EN), y)
ifeq ($(HWDEMO_BINDING_EN), y)
ifeq ($(BUILD_THPART_OPUS_ENABLE), y)
CUSTOM_COBJSTEMP += $(TIRTC_SRC_ROOT)/features/ai_chat.o
CUSTOM_COBJSTEMP += $(TIRTC_SRC_ROOT)/media/opus_codec.o
CFLAGS_DEFS += -DHWDEMO_AI_CHAT_EN
CFLAGS_DEFS += -DTIRTC_AI_PREBUFFER_PACKETS=$(TIRTC_AI_PREBUFFER_PACKETS)
else
$(error HWDEMO_AI_CHAT_EN=y requires BUILD_THPART_OPUS_ENABLE=y)
endif
else
$(error HWDEMO_AI_CHAT_EN=y requires HWDEMO_BINDING_EN=y)
endif
else
$(error HWDEMO_AI_CHAT_EN=y requires HWDEMO_TIRTC_EN=y)
endif
endif

ifeq ($(HWDEMO_WECHAT_EN), y)
ifeq ($(HWDEMO_TIRTC_EN), y)
ifeq ($(HWDEMO_BINDING_EN), y)
ifeq ($(HWDEMO_LCD_SSD1306_EN), y)
ifeq ($(HWDEMO_KEY_EN), y)
CUSTOM_COBJSTEMP += $(TIRTC_SRC_ROOT)/features/wechat_call.o
CFLAGS_DEFS += -DHWDEMO_WECHAT_EN
else
$(error HWDEMO_WECHAT_EN=y requires HWDEMO_KEY_EN=y)
endif
else
$(error HWDEMO_WECHAT_EN=y requires HWDEMO_LCD_SSD1306_EN=y)
endif
else
$(error HWDEMO_WECHAT_EN=y requires HWDEMO_BINDING_EN=y)
endif
else
$(error HWDEMO_WECHAT_EN=y requires HWDEMO_TIRTC_EN=y)
endif
endif

ifeq ($(HWDEMO_DEV_CHAT_EN), y)
ifeq ($(HWDEMO_TIRTC_EN), y)
ifeq ($(HWDEMO_BINDING_EN), y)
ifeq ($(HWDEMO_LCD_SSD1306_EN), y)
ifeq ($(HWDEMO_KEY_EN), y)
CUSTOM_COBJSTEMP += $(TIRTC_SRC_ROOT)/features/device_call.o
CFLAGS_DEFS += -DHWDEMO_DEV_CHAT_EN
else
$(error HWDEMO_DEV_CHAT_EN=y requires HWDEMO_KEY_EN=y)
endif
else
$(error HWDEMO_DEV_CHAT_EN=y requires HWDEMO_LCD_SSD1306_EN=y)
endif
else
$(error HWDEMO_DEV_CHAT_EN=y requires HWDEMO_BINDING_EN=y)
endif
else
$(error HWDEMO_DEV_CHAT_EN=y requires HWDEMO_TIRTC_EN=y)
endif
endif

ifeq ($(HWDEMO_LIVE_TALK_EN), y)
ifeq ($(HWDEMO_TIRTC_EN), y)
ifeq ($(HWDEMO_LCD_SSD1306_EN), y)
CUSTOM_COBJSTEMP += $(TIRTC_SRC_ROOT)/features/platform_intercom.o
CFLAGS_DEFS += -DHWDEMO_LIVE_TALK_EN
else
$(error HWDEMO_LIVE_TALK_EN=y requires HWDEMO_LCD_SSD1306_EN=y (HOME policy owner))
endif
else
$(error HWDEMO_LIVE_TALK_EN=y requires HWDEMO_TIRTC_EN=y)
endif
endif

# AI, WeChat, device calling and platform intercom share one ES8311/I2S owner.
ifneq ($(filter y,$(HWDEMO_AI_CHAT_EN) $(HWDEMO_WECHAT_EN) $(HWDEMO_DEV_CHAT_EN) $(HWDEMO_LIVE_TALK_EN)),)
CUSTOM_COBJSTEMP += $(TIRTC_SRC_ROOT)/media/audio_device.o
endif

ifneq ($(filter y,$(HWDEMO_WECHAT_EN) $(HWDEMO_DEV_CHAT_EN) $(HWDEMO_LIVE_TALK_EN)),)
CUSTOM_COBJSTEMP += $(TIRTC_SRC_ROOT)/media/g711_codec.o
endif
