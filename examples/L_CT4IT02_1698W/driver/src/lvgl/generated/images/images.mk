LVGL_GENERATED_IMAGE_DIR ?= $(PRJ_DIR)/generated/images

GEN_CSRCS += $(notdir $(wildcard $(LVGL_GENERATED_IMAGE_DIR)/*.c))
-include $(LVGL_GENERATED_IMAGE_DIR)/lottie_list.mk

DEPPATH += --dep-path $(LVGL_GENERATED_IMAGE_DIR)
VPATH += :$(LVGL_GENERATED_IMAGE_DIR)

CFLAGS += "-I$(LVGL_GENERATED_IMAGE_DIR)"
GEN_CSRCS += _watch_360x360.c _baji_360x360.c _attitude_360x360.c _attitudefun_360x360.c _salary_360x360.c _positioning_360x360.c _mp3_360x360.c
