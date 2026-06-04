LWS_VERSION := libwebsockets-4.3-stable
LWS_DIR := $(TOP)/components/thirdparty/libwebsockets
include $(LWS_DIR)/config.inc

CFLAGS_INC += -I $(LWS_DIR)/inc \
			  -I $(LWS_DIR)/$(LWS_VERSION)/lib/core \
			  -I $(LWS_DIR)/$(LWS_VERSION)/lib/misc \
			  -I $(LWS_DIR)/$(LWS_VERSION)/lib/system \
			  -I $(LWS_DIR)/$(LWS_VERSION)/lib/system/metrics \
			  -I $(LWS_DIR)/$(LWS_VERSION)/include

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
	LWS_CFLAGS_DEFS += -DLWS_WITH_POLL
endif

ifeq ($(LWS_WITH_SYS_SMD),y) 
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/system/smd
	LWS_CFLAGS_DEFS += -DLWS_WITH_SYS_SMD
endif

ifeq ($(LWS_PLAT_FREERTOS),y) 
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/plat/freertos
	LWS_CFLAGS_DEFS += -DLWS_PLAT_FREERTOS
endif

ifeq ($(LWS_WITH_FILE_OPS),n)
	LWS_CFLAGS_DEFS += -DLWS_WITH_FILE_OPS
endif


ifeq ($(LWS_WITH_SYS_ASYNC_DNS)_$(LWS_WITH_SYS_NTPCLIENT),n_n)
	LWS_CFLAGS_DEFS += 
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
LWS_CFLAGS_DEFS += -DLWS_MAX_SMP=$(LWS_MAX_SMP)

CFLAGS_DEFS += $(LWS_CFLAGS_DEFS)

