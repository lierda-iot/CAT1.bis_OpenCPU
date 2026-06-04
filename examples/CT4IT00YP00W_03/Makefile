CUSTOM_DIR := examples/$(PROJECT)
CUSTOM_SRC := $(CUSTOM_DIR)/src

CFLAGS_INC	+= -I $(TOP)/$(CUSTOM_DIR)/inc

CUSTOM_COBJSTEMP := $(CUSTOM_SRC)/user_main.o

CUSTOM_COBJS := $(addprefix $(BUILDDIR)/, $(CUSTOM_COBJSTEMP))

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_CT4IT00YP00W_03.a

$(BUILDDIR)/lib/libusr/lib_CT4IT00YP00W_03.a: $(CUSTOM_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif