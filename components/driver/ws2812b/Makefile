WS2812BDRV_DIR := $(DRIVER_DIR)/ws2812b
WS2812BDRV_SRC := $(WS2812BDRV_DIR)/src

CFLAGS_INC	+= -I $(TOP)/$(WS2812BDRV_DIR)/inc

WS2812BDRV_COBJS += $(WS2812BDRV_SRC)/ws2812b.o

WS2812BDRV_COBJS := $(addprefix $(BUILDDIR)/, $(WS2812BDRV_COBJS))

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_ws2812bdrv.a

$(BUILDDIR)/lib/libusr/lib_ws2812bdrv.a: $(WS2812BDRV_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif
