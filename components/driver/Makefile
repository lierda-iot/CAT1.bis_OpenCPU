DRIVER_DIR ?= components/driver

ifeq ($(BUILD_DRIVER_LCD_EN), y)
include $(TOP)/$(DRIVER_DIR)/lcd/Makefile
endif

ifeq ($(BUILD_DRIVER_CAMERA_EN), y)
include $(TOP)/$(DRIVER_DIR)/camera/Makefile
endif

ifeq ($(BUILD_DRIVER_TP_EN), y)
include $(TOP)/$(DRIVER_DIR)/tp/Makefile
endif

ifeq ($(BUILD_DRIVER_WS2812B_EN), y)
include $(TOP)/$(DRIVER_DIR)/ws2812b/Makefile
endif

ifeq ($(BUILD_DRIVER_BATTERY_EN), y)
include $(TOP)/$(DRIVER_DIR)/battery/Makefile
endif

ifeq ($(BUILD_DRIVER_GX8006_EN), y)
include $(TOP)/$(DRIVER_DIR)/gx8006/Makefile
endif
