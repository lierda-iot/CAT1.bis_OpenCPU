TPDRV_DIR := $(DRIVER_DIR)/tp
TPDRV_AXS5106_SRC := $(TPDRV_DIR)/axs5106/axs5106
TPDRV_CST816D_SRC := $(TPDRV_DIR)/axs5106/cst816d

CFLAGS_INC += -I $(TOP)/$(TPDRV_AXS5106_SRC)
CFLAGS_INC += -I $(TOP)/$(TPDRV_CST816D_SRC)

TPDRV_COBJS += $(TPDRV_AXS5106_SRC)/liot_tpDev_AXS5106.o
TPDRV_COBJS += $(TPDRV_CST816D_SRC)/liot_tpDev_CST816D.o

TPDRV_COBJS := $(addprefix $(BUILDDIR)/, $(TPDRV_COBJS))

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_tpdrv.a

$(BUILDDIR)/lib/libusr/lib_tpdrv.a: $(TPDRV_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif
