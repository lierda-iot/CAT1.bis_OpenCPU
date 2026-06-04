LWIP_HOST_DIRS := $(TOP)/components/thirdparty/lwip/src

CFLAGS_INC += -I $(LWIP_HOST_DIRS)/include \
              -I $(LWIP_HOST_DIRS)/include/posix

LWIPCONFIG_CFLAGS ?= -DLWIP_CONFIG_FILE=\"lwip_config_cat.h\"
LWIPCONFIG_CFLAGS += -DDEBUG_LOG_HEADER_FILE=\"debug_trace.h\"

CFLAGS += $(LWIPCONFIG_CFLAGS)
