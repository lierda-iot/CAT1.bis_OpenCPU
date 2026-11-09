UNITY_ROOT := $(TOP)/components/thirdparty/unity/unity
UNITY_SRC_DIR := $(UNITY_ROOT)/src

CFLAGS_INC += -I $(UNITY_SRC_DIR)

UNITY_CSRC := $(UNITY_SRC_DIR)/unity.c
UNITY_COBJSTEMP := $(patsubst $(TOP)/%.c, %.o, $(UNITY_CSRC))
UNITY_COBJS := $(addprefix $(BUILDDIR)/, $(UNITY_COBJSTEMP))

PP_FILES += $(UNITY_COBJS:.o=.pp)

ifeq ($(TOOLCHAIN),GCC)
libusr-y += libunity.a

$(BUILDDIR)/lib/libusr/libunity.a: $(UNITY_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)printf '%s\n' $^ | sed 's#^/cygdrive/\([a-zA-Z]\)/#\1:/#' > $@.rsp
	$(Q)$(AR) $(ARFLAGS) $@ @$@.rsp
	$(Q)rm -f $@.rsp
endif
