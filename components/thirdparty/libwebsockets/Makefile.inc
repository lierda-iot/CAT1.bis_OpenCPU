LWS_VERSION := libwebsockets-4.3-stable
LWS_DIR := $(TOP)/components/thirdparty/libwebsockets
include $(LWS_DIR)/config.inc

CFLAGS_INC += -I $(LWS_DIR)/inc \
			  -I $(LWS_DIR)/$(LWS_VERSION)/lib/core \
			  -I $(LWS_DIR)/$(LWS_VERSION)/lib/misc \
			  -I $(LWS_DIR)/$(LWS_VERSION)/lib/system \
			  -I $(LWS_DIR)/$(LWS_VERSION)/lib/system/metrics \
			  -I $(LWS_DIR)/$(LWS_VERSION)/include

LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/core \
				$(LWS_DIR)/$(LWS_VERSION)/lib/system \
				$(LWS_DIR)


LWS_EXCLUDE_FILES := 
LWS_CFLAGS_DEFS	:=

ifeq ($(LWS_WITH_NETWORK),y)
LWS_CFLAGS_DEFS += -DLWS_WITH_NETWORK
ifeq ($(LWS_WITH_CLIENT),y)
	LWS_CFLAGS_DEFS += -DLWS_WITH_CLIENT
endif
include $(LWS_DIR)/core-net.inc
include $(LWS_DIR)/roles.inc
endif

ifeq ($(LWS_WITH_TLS),y)
LWS_CFLAGS_DEFS += -DLWS_WITH_TLS
include $(LWS_DIR)/tls.inc
endif

include $(LWS_DIR)/misc.inc

ifeq ($(LWS_WITH_POLL),y)
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/event-libs
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/event-libs/poll
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/event-libs/poll
	LWS_CFLAGS_DEFS += -DLWS_WITH_POLL
endif

ifeq ($(LWS_WITH_SYS_SMD),y) 
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/system/smd
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/system/smd
	LWS_CFLAGS_DEFS += -DLWS_WITH_SYS_SMD
endif

ifeq ($(LWS_PLAT_FREERTOS),y) 
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/plat/freertos
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/plat/freertos
	LWS_CFLAGS_DEFS += -DLWS_PLAT_FREERTOS
endif

ifneq ($(LWS_WITH_FILE_OPS),y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/core/vfs.c
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/plat/freertos/freertos-file.c
else
	LWS_CFLAGS_DEFS += -DLWS_WITH_FILE_OPS
endif



ifeq ($(LWS_WITH_SYS_ASYNC_DNS)_$(LWS_WITH_SYS_NTPCLIENT),n_n)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/plat/freertos/freertos-resolv.c
else
ifeq ($(LWS_WITH_SYS_ASYNC_DNS),y)
	LWS_CFLAGS_DEFS += -DLWS_WITH_SYS_ASYNC_DNS
endif
ifeq ($(LWS_WITH_SYS_NTPCLIENT),y)
	LWS_CFLAGS_DEFS += -DLWS_WITH_SYS_NTPCLIENT
endif
endif

LWS_CFLAGS_DEFS += -DLWS_BUILD_HASH=\"$(LWS_BUILD_HASH)\"
LWS_CFLAGS_DEFS += -DLWS_LIBRARY_VERSION=\"$(LWS_LIBRARY_VERSION)\"
#LWS_CFLAGS_DEFS += -DLWS_LOGGING_BITFIELD_CLEAR=$(LWS_LOGGING_BITFIELD_CLEAR)
#LWS_CFLAGS_DEFS += -DLWS_LOGGING_BITFIELD_SET=$(LWS_LOGGING_BITFIELD_SET)
LWS_CFLAGS_DEFS += -DLWS_MAX_SMP=$(LWS_MAX_SMP)

CFLAGS_DEFS += $(LWS_CFLAGS_DEFS)

LWS_CSRC = $(foreach dir, $(LWS_SRC_DIRS), $(wildcard $(dir)/*.c))
LWS_CFILES = $(filter-out $(LWS_EXCLUDE_FILES), $(LWS_CSRC))
LWS_COBJSTEMP := $(patsubst $(TOP)/%.c, %.o, $(LWS_CFILES))
LWS_COBJS := $(addprefix $(BUILDDIR)/, $(LWS_COBJSTEMP))

PP_FILES += $(LWS_COBJS:.o=.pp)

-include $(LWS_DDFILES)

ifeq ($(TOOLCHAIN),GCC)
libusr-y += libwebsockets.a

$(BUILDDIR)/lib/libusr/libwebsockets.a: $(LWS_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif