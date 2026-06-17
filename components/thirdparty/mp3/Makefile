MP3_DIR := $(TOP)/components/thirdparty/mp3

CFLAGS_INC += -I $(MP3_DIR)/libmad-0.15.1b/msvc++ \
              -I $(MP3_DIR)/libmad-0.15.1b

MP3_SRC_DIRS += $(MP3_DIR)/libmad-0.15.1b \
                $(MP3_DIR)

CFLAGS += -DFEATURE_MP3_ENABLED


MP3_EXCLUDE_FILES :=
MP3_CSRC = $(foreach dir, $(MP3_SRC_DIRS), $(wildcard $(dir)/*.c))
MP3_CFILES = $(filter-out $(MP3_EXCLUDE_FILES), $(MP3_CSRC))
MP3_COBJSTEMP := $(patsubst $(TOP)/%.c, %.o, $(MP3_CFILES))
MP3_COBJS := $(addprefix $(BUILDDIR)/, $(MP3_COBJSTEMP))
PP_FILES += $(MP3_COBJS:.o=.pp)

ifeq ($(TOOLCHAIN),GCC)
libusr-y += libmp3.a
$(BUILDDIR)/lib/libusr/libmp3.a: $(MP3_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif