MID_DIR := $(TOP)/components

include $(MID_DIR)/kernel/Makefile
include $(MID_DIR)/basePkg/Makefile
include $(MID_DIR)/precfg/Makefile
include $(MID_DIR)/driver/Makefile

ifeq ($(BUILD_COMP_SECBOOT_EN), y)
include $(MID_DIR)/secboot/Makefile
endif

ifeq ($(BUILD_COMP_THIRDPART_EN), y)
include $(MID_DIR)/thirdparty/Makefile
endif

ifeq ($(BUILD_COMP_OTA_EN), y)
include $(MID_DIR)/ota/Makefile
endif

ifeq ($(BUILD_COMP_TTS_EN), y)
include $(MID_DIR)/tts/Makefile
endif

ifeq ($(BUILD_COMP_PRESETFILE_EN), y)
include $(MID_DIR)/presetfile/Makefile
endif