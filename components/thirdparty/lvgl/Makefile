
LVGL_DIR := $(TOP)/components/thirdparty/lvgl/lvgl-8.3.10

CFLAGS_INC += -I $(LVGL_DIR)

LVGL_SRC_DIRS += $(LVGL_DIR)	\
				$(LVGL_DIR)/src		\
				$(LVGL_DIR)/src/*	\
				$(LVGL_DIR)/src/*/*		\
				$(LVGL_DIR)/src/*/*/*	

BUILD_THPART_LVGL_DEMOS_ENABLE ?= n
ifeq ($(BUILD_THPART_LVGL_DEMOS_ENABLE), y)
CFLAGS_INC += -I $(LVGL_DIR)/demos
LVGL_SRC_DIRS += $(LVGL_DIR)/demos		\
				$(LVGL_DIR)/demos/*		\
				$(LVGL_DIR)/demos/*/*
endif

CFLAGS += -DFEATURE_LVGL_ENABLED
CFLAGS += -DLV_LVGL_H_INCLUDE_SIMPLE

LVGL_EXCLUDE_FILES := $(LVGL_DIR)/lv_conf.h \
				$(LVGL_DIR)/lv_port_disp.c \
				$(LVGL_DIR)/lv_port_indev.c \
				$(LVGL_DIR)/lv_port_fs.c
LVGL_CSRC = $(foreach dir, $(LVGL_SRC_DIRS), $(wildcard $(dir)/*.c))
LVGL_CFILES = $(filter-out $(LVGL_EXCLUDE_FILES), $(LVGL_CSRC))
LVGL_COBJSTEMP := $(patsubst $(TOP)/%.c, %.o, $(LVGL_CFILES))
LVGL_COBJS := $(addprefix $(BUILDDIR)/, $(LVGL_COBJSTEMP))

PP_FILES += $(LVGL_COBJS:.o=.pp)

ifeq ($(TOOLCHAIN),GCC)
libusr-y += liblvgl.a

$(BUILDDIR)/lib/libusr/liblvgl.a: $(LVGL_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)printf '%s\n' $^ | sed 's#^/cygdrive/\([a-zA-Z]\)/#\1:/#' > $@.rsp
	$(Q)$(AR) $(ARFLAGS) $@ @$@.rsp
	$(Q)rm -f $@.rsp
endif
