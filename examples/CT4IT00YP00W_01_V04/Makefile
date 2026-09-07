CUSTOM_DIR := examples/app
CUSTOM_SRC := $(CUSTOM_DIR)/src

CFLAGS_INC	+= -I $(TOP)/$(CUSTOM_DIR)/inc

#Customize your configuration
include  $(TOP)/$(CUSTOM_DIR)/config

CUSTOM_COBJSTEMP := $(CUSTOM_SRC)/user_main.o

CUSTOM_COBJS := $(addprefix $(BUILDDIR)/, $(CUSTOM_COBJSTEMP))

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_customapp.a

$(BUILDDIR)/lib/libusr/lib_customapp.a: $(CUSTOM_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif