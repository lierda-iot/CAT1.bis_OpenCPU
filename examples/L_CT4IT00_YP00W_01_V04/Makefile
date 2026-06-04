CUSTOM_DIR := examples/$(PROJECT)

# Load project config (BUILD_MODE, HW_XXX_READY flags)
include $(TOP)/$(CUSTOM_DIR)/config

# Common include paths
CFLAGS_INC += -I $(TOP)/$(CUSTOM_DIR)/inc
CFLAGS_INC += -I $(TOP)/$(CUSTOM_DIR)/demo/inc

ifeq ($(BUILD_MODE), demo)

CUSTOM_SRC := $(CUSTOM_DIR)/demo/src
CUSTOM_COBJSTEMP := $(CUSTOM_SRC)/0demo_main.o

ifeq ($(HWDEMO_KEY_EN), y)
CUSTOM_COBJSTEMP += $(CUSTOM_SRC)/demo_key.o
CFLAGS_DEFS += -DHWDEMO_KEY_EN
endif

ifeq ($(HWDEMO_LCD_SSD1306_EN), y)
CUSTOM_COBJSTEMP += $(CUSTOM_SRC)/demo_lcd_ssd1306.o
CFLAGS_DEFS += -DHWDEMO_LCD_SSD1306_EN
endif

ifeq ($(HWDEMO_WS2812B_EN), y)
CUSTOM_COBJSTEMP += $(CUSTOM_SRC)/demo_ws2812b.o
CFLAGS_DEFS += -DHWDEMO_WS2812B_EN
endif

else

CUSTOM_SRC := $(CUSTOM_DIR)/app/src
CUSTOM_COBJSTEMP := $(CUSTOM_SRC)/user_main.o

endif

CUSTOM_COBJS := $(addprefix $(BUILDDIR)/, $(CUSTOM_COBJSTEMP))

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_L_CT4IT00_YP00W_01_V04.a

$(BUILDDIR)/lib/libusr/lib_L_CT4IT00_YP00W_01_V04.a: $(CUSTOM_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif
