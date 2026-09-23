CUSTOM_DIR := examples/$(PROJECT)

# Resolve MODEMPKG before package-specific TiRTC options are evaluated.
# The common rules include this mapping again later without changing the result.
include $(TOP)/rules/Makefile.modem
include $(TOP)/$(CUSTOM_DIR)/config

IOINI := $(TOP)/$(CUSTOM_DIR)/tirtc/board/iodriver.ini

CFLAGS_INC += -I $(TOP)/$(CUSTOM_DIR)/inc

include $(TOP)/$(CUSTOM_DIR)/tirtc/tirtc_app.mk

CUSTOM_COBJS := $(addprefix $(BUILDDIR)/, $(CUSTOM_COBJSTEMP))

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_$(PROJECT).a

$(BUILDDIR)/lib/libusr/lib_$(PROJECT).a: $(CUSTOM_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif
