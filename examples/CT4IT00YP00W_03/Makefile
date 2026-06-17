CUSTOM_DIR := examples/$(PROJECT)

include $(TOP)/$(CUSTOM_DIR)/config
include $(TOP)/$(CUSTOM_DIR)/framework/Makefile

CFLAGS_INC += -I $(TOP)/$(CUSTOM_DIR)/inc
CUSTOM_SRC_DIRS := $(CUSTOM_DIR)/src

CUSTOM_CSRC = $(foreach dir, $(CUSTOM_SRC_DIRS), $(wildcard $(TOP)/$(dir)/*.c))
CUSTOM_COBJSTEMP := $(patsubst $(TOP)/%.c, %.o, $(CUSTOM_CSRC))
CUSTOM_COBJS := $(addprefix $(BUILDDIR)/, $(CUSTOM_COBJSTEMP))

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_$(PROJECT).a

$(BUILDDIR)/lib/libusr/lib_$(PROJECT).a: $(CUSTOM_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif
