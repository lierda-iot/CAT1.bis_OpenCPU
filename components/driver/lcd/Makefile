LCDDRV_DIR := $(DRIVER_DIR)/lcd
LCDDRV_SRC := $(LCDDRV_DIR)/src

CFLAGS_INC	+= -I $(TOP)/$(LCDDRV_DIR)/inc

LCDDRV_COBJS += $(LCDDRV_SRC)/liot_lcdDev_GC9A01.o
LCDDRV_COBJS += $(LCDDRV_SRC)/liot_lcdDev_GC9D01.o
LCDDRV_COBJS += $(LCDDRV_SRC)/liot_lcdDev_SSD1306.o
LCDDRV_COBJS += $(LCDDRV_SRC)/liot_lcdDev_ST7567.o
LCDDRV_COBJS += $(LCDDRV_SRC)/liot_lcdDev_ST7735.o
LCDDRV_COBJS += $(LCDDRV_SRC)/liot_lcdDev_ST7789.o
LCDDRV_COBJS += $(LCDDRV_SRC)/liot_lcdDev_ST77916.o

LCDDRV_COBJS := $(addprefix $(BUILDDIR)/, $(LCDDRV_COBJS))

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_lcddrv.a

$(BUILDDIR)/lib/libusr/lib_lcddrv.a: $(LCDDRV_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif