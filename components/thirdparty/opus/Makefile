OPUS_DIR := $(TOP)/components/thirdparty/opus

CFLAGS_INC += -I $(OPUS_DIR)/opus-1.4/include \
			  -I $(OPUS_DIR)/opus-1.4/celt \
			  -I $(OPUS_DIR)/opus-1.4/silk \
			  -I $(OPUS_DIR)/opus-1.4/silk/fixed \
			  -I $(OPUS_DIR)/opus-1.4/src

OPUS_SRC_DIRS += $(OPUS_DIR)/opus-1.4/src \
				 $(OPUS_DIR)/opus-1.4/celt \
				 $(OPUS_DIR)/opus-1.4/silk \
				 $(OPUS_DIR)/opus-1.4/silk/fixed \
				 $(OPUS_DIR)

OPUS_EXCLUDE_FILES := $(OPUS_DIR)/opus-1.4/src/repacketizer_demo.c \
					  $(OPUS_DIR)/opus-1.4/src/opus_demo.c \
					  $(OPUS_DIR)/opus-1.4/src/opus_custom_demo.c \
					  $(OPUS_DIR)/opus-1.4/src/opus_compare.c

OPUS_CSRC = $(foreach dir, $(OPUS_SRC_DIRS), $(wildcard $(dir)/*.c))
OPUS_CFILES = $(filter-out $(OPUS_EXCLUDE_FILES), $(OPUS_CSRC))
OPUS_COBJSTEMP := $(patsubst $(TOP)/%.c, %.o, $(OPUS_CFILES))
OPUS_COBJS := $(addprefix $(BUILDDIR)/, $(OPUS_COBJSTEMP))
PP_FILES += $(OPUS_COBJS:.o=.pp)

CFLAGS += -DOPUS_BUILD -DHAVE_LRINT -DFIXED_POINT=1 -DDISABLE_FLOAT_API -DVAR_ARRAYS #-DUSE_ALLOCA

ifeq ($(TOOLCHAIN),GCC)
libusr-y += libopus.a

$(BUILDDIR)/lib/libusr/libopus.a: $(OPUS_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif