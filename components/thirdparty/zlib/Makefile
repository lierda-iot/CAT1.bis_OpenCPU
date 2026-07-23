ZLIB_DIR := $(TOP)/components/thirdparty/zlib/zlib-1.3.1

CFLAGS_INC += -I $(ZLIB_DIR) \
			  -I $(ZLIB_DIR)/contrib/minizip

CFLAGS += -DFEATURE_ZLIB_ENABLED

ZLIB_SRC_DIRS += $(ZLIB_DIR) \
				 $(ZLIB_DIR)/contrib/minizip

ZLIB_EXCLUDE_FILES :=   $(ZLIB_DIR)/gzclose.c \
                        $(ZLIB_DIR)/gzlib.c \
                        $(ZLIB_DIR)/gzread.c \
                        $(ZLIB_DIR)/gzwrite.c

ZLIB_CSRC = $(foreach dir, $(ZLIB_SRC_DIRS), $(wildcard $(dir)/*.c))
ZLIB_CFILES = $(filter-out $(ZLIB_EXCLUDE_FILES), $(ZLIB_CSRC))
ZLIB_COBJSTEMP := $(patsubst $(TOP)/%.c, %.o, $(ZLIB_CFILES))
ZLIB_COBJS := $(addprefix $(BUILDDIR)/, $(ZLIB_COBJSTEMP))
PP_FILES += $(ZLIB_COBJS:.o=.pp)

ifeq ($(TOOLCHAIN),GCC)
libusr-y += libzlib.a
$(BUILDDIR)/lib/libusr/libzlib.a: $(ZLIB_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif