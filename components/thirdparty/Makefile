THIRDPART_DIR := $(TOP)/components/thirdparty

ifeq ($(BUILD_THPART_CJSON_ENABLE), y)
include $(THIRDPART_DIR)/CJSON/Makefile
endif

ifeq ($(BUILD_THPART_FREERTOS_ENABLE), y)
include $(THIRDPART_DIR)/freertos/Makefile
endif

ifeq ($(BUILD_THPART_LIB1_ENABLE), y)
include $(THIRDPART_DIR)/lib1/Makefile
endif

ifeq ($(BUILD_THPART_LWIP_ENABLE), y)
include $(THIRDPART_DIR)/lwip/Makefile
endif

ifeq ($(BUILD_THPART_MBEDTLS_ENABLE), y)
include $(THIRDPART_DIR)/mbedtls/Makefile
endif

ifeq ($(BUILD_THPART_WEBSOCKET_ENABLE), y)
include $(THIRDPART_DIR)/libwebsockets/Makefile.inc
endif

ifeq ($(BUILD_THPART_LVGL_ENABLE), y)
include $(THIRDPART_DIR)/lvgl/Makefile
endif

ifeq ($(BUILD_THPART_LIOT_AI_ENABLE), y)
include $(THIRDPART_DIR)/liot_ai/Makefile
endif

ifeq ($(BUILD_THPART_AMR_DECODER_ENABLE), y)
include $(THIRDPART_DIR)/amr_decoder/Makefile
endif

ifeq ($(BUILD_THPART_OPUS_ENABLE), y)
include $(THIRDPART_DIR)/opus/Makefile
endif

ifeq ($(BUILD_THPART_MP3_ENABLE), y)
include $(THIRDPART_DIR)/mp3/Makefile
endif
