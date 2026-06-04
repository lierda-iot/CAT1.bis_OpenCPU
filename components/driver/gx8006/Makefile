GX8006DRV_DIR := $(DRIVER_DIR)/gx8006
GX8006DRV_SRC := $(GX8006DRV_DIR)/src

CFLAGS_INC	+= -I $(TOP)/$(GX8006DRV_DIR)/inc

GX8006DRV_COBJS += $(GX8006DRV_SRC)/gx8006_hardware.o
GX8006DRV_COBJS += $(GX8006DRV_SRC)/gx8006_protocol.o
GX8006DRV_COBJS += $(GX8006DRV_SRC)/gx8006_mic.o
GX8006DRV_COBJS += $(GX8006DRV_SRC)/gx8006_data.o

GX8006DRV_COBJS := $(addprefix $(BUILDDIR)/, $(GX8006DRV_COBJS))

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_gx8006drv.a

$(BUILDDIR)/lib/libusr/lib_gx8006drv.a: $(GX8006DRV_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif
