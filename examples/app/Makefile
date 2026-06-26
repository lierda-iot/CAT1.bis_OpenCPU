CUSTOM_DIR := examples/$(PROJECT)

include $(TOP)/$(CUSTOM_DIR)/config

CFLAGS_INC += -I $(TOP)/$(CUSTOM_DIR)/inc
CUSTOM_SRC_DIRS := $(CUSTOM_DIR)/src

CUSTOM_CSRC = $(foreach dir, $(CUSTOM_SRC_DIRS), $(wildcard $(TOP)/$(dir)/*.c))
CUSTOM_COBJSTEMP := $(patsubst $(TOP)/%.c, %.o, $(CUSTOM_CSRC))
CUSTOM_COBJS := $(addprefix $(BUILDDIR)/, $(CUSTOM_COBJSTEMP))
PP_FILES += $(CUSTOM_COBJS:.o=.pp)

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_custom_$(PROJECT).a

$(BUILDDIR)/lib/libusr/lib_custom_$(PROJECT).a: $(CUSTOM_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif
