CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/core-net
LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/core-net

ifneq ($(LWS_WITH_SYS_STATE),y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/core-net/state.c
else
	LWS_CFLAGS_DEFS += -DLWS_WITH_SYS_STATE
endif

ifneq ($(LWS_WITH_NETLINK),y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/core-net/route.c
else
	LWS_CFLAGS_DEFS += -DLWS_WITH_NETLINK
endif

ifneq ($(LWS_WITH_LWS_DSH), 0)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/core-net/lws-dsh.c
endif

ifneq ($(LWS_WITH_SEQUENCER),y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/core-net/sequencer.c
else
	LWS_CFLAGS_DEFS += -DLWS_WITH_SEQUENCER
endif

ifeq ($(LWS_WITH_CLIENT),y)
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/core-net/client
	ifneq ($(LWS_WITH_CONMON),y)
		LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/core-net/client/conmon.c
	else
		LWS_CFLAGS_DEFS += -DLWS_WITH_CONMON
	endif
endif

ifneq ($(LWS_WITH_SOCKS5),y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/core-net/socks5-client.c
else
	LWS_CFLAGS_DEFS += -DLWS_WITH_SOCKS5
endif

