CH390DRV_DIR := $(DRIVER_DIR)/ch390
CH390DRV_SRC := $(CH390DRV_DIR)/src

CFLAGS_INC	+= -I $(TOP)/$(CH390DRV_DIR)/inc

CH390DRV_COBJS += $(CH390DRV_SRC)/liot_ch390.o

CH390DRV_COBJS := $(addprefix $(BUILDDIR)/, $(CH390DRV_COBJS))

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_ch390drv.a

$(BUILDDIR)/lib/libusr/lib_ch390drv.a: $(CH390DRV_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif
