BATTERYDRV_DIR := $(DRIVER_DIR)/battery
BATTERYDRV_SRC := $(BATTERYDRV_DIR)/src

CFLAGS_INC	+= -I $(TOP)/$(BATTERYDRV_DIR)/inc

BATTERYDRV_COBJS += $(BATTERYDRV_SRC)/battery.o

BATTERYDRV_COBJS := $(addprefix $(BUILDDIR)/, $(BATTERYDRV_COBJS))

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_batterydrv.a

$(BUILDDIR)/lib/libusr/lib_batterydrv.a: $(BATTERYDRV_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif
