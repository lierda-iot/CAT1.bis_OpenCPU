CAMERADRV_DIR := $(DRIVER_DIR)/camera
CAMERADRV_SRC := $(CAMERADRV_DIR)/src

CFLAGS_INC	+= -I $(TOP)/$(CAMERADRV_DIR)/inc

CAMERADRV_COBJS += $(CAMERADRV_SRC)/liot_camdev_gc032a.o


CAMERADRV_COBJS := $(addprefix $(BUILDDIR)/, $(CAMERADRV_COBJS))

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_cameradrv.a

$(BUILDDIR)/lib/libusr/lib_cameradrv.a: $(CAMERADRV_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif