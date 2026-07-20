
TGTSDK_DIR        := $(VSIM_DIR)/TGT/src

CFLAGS_INC += -I $(VSIM_DIR)/TGT/inc
CFLAGS_INC += -I$(TOP)/components/thirdparty/mbedtls/include

TGT_SDK_SRC_DIRS += $(TGTSDK_DIR)

TGT_SDK_EXCLUDE_FILES :=                                   

TGT_SDK_CSRC = $(foreach dir, $(TGT_SDK_SRC_DIRS), $(wildcard $(dir)/*.c))
TGT_SDK_CFILES = $(filter-out $(TGT_SDK_EXCLUDE_FILES), $(TGT_SDK_CSRC))
TGT_SDK_COBJSTEMP := $(patsubst %.c, %.o, $(TGT_SDK_CFILES))

TGT_SDK_COBJSTEMP :=$(subst $(TGTSDK_DIR),components/thirdparty/vsim/TGT/src,$(TGT_SDK_COBJSTEMP))

TGT_SDK_COBJS := $(addprefix $(BUILDDIR)/, $(TGT_SDK_COBJSTEMP))
TGT_SDK_PPFILES := $(patsubst %.o, %.pp, $(TGT_SDK_COBJS))
PPFILES += $(TGT_SDK_PPFILES)

ifneq ($(MAKECMDGOALS), unilog)
-include $(TGT_SDK_COBJS:.o=.d)
endif
-include $(TGT_SDK_COBJS:.o=.dd)

ifeq ($(TOOLCHAIN),GCC)


lib-y += libvsim_tgt.a

$(BUILDDIR)/lib/libvsim_tgt.a: $(TGT_SDK_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^

endif

ifeq ($(TOOLCHAIN),ARMCC)

lib-y += libvsim_tgt.lib

$(BUILDDIR)/lib/libvsim_tgt.lib: $(UC_SDK_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) $(ARFLAGS) $@ $^

endif
