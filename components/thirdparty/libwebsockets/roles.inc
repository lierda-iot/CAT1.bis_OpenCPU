CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/roles
LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/roles \
				$(LWS_DIR)/$(LWS_VERSION)/lib/roles/pipe

ifeq ($(LWS_ROLE_MQTT),y)
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/roles/mqtt
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/mqtt
	LWS_CFLAGS_DEFS += -DLWS_ROLE_MQTT
endif

ifeq ($(LWS_ROLE_H1),y)
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/roles/h1
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/h1
	LWS_CFLAGS_DEFS += -DLWS_ROLE_H1
endif

ifeq ($(LWS_ROLE_H2),y)
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/roles/h2
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/h2
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/h2/minihuf.c
	LWS_CFLAGS_DEFS += -DLWS_ROLE_H2
endif

ifeq ($(LWS_ROLE_WS),y)
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/roles/ws
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/ws
	LWS_CFLAGS_DEFS += -DLWS_ROLE_WS
	ifeq ($(LWS_WITHOUT_SERVER),y)
		LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/ws/server-ws.c
	endif
	ifeq ($(LWS_WITHOUT_CLIENT), y)
		LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/ws/client-ws.c \
							 $(LWS_DIR)/$(LWS_VERSION)/lib/roles/ws/client-parser-ws.c
	endif
endif

ifeq ($(LWS_ROLE_RAW),y)
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/roles/raw-skt
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/raw-skt
	LWS_CFLAGS_DEFS += -DLWS_ROLE_RAW
endif

ifeq ($(LWS_ROLE_RAW_FILE),y)
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/roles/raw-file
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/raw-file
	LWS_CFLAGS_DEFS += -DLWS_ROLE_RAW_FILE
endif

ifeq ($(LWS_WITH_CGI),y)
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/roles/cgi
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/cgi
	LWS_CFLAGS_DEFS += -DLWS_WITH_CGI
endif

ifeq ($(LWS_ROLE_RAW_PROXY),y)
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/roles/raw-proxy
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/raw-proxy
	LWS_CFLAGS_DEFS += -DLWS_ROLE_RAW_PROXY
endif

ifeq ($(LWS_WITH_NETLINK),y)
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/roles/netlink
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/netlink
	LWS_CFLAGS_DEFS += -DLWS_WITH_NETLINK
endif

#http
ifneq ($(LWS_ROLE_H1)_$(LWS_ROLE_H2), n_n)
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/roles/http
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/http

	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/http/minilex.c

	ifeq ($(LWS_CLIENT_HTTP_PROXYING),y)
		LWS_CFLAGS_DEFS += -DLWS_CLIENT_HTTP_PROXYING
	endif

	ifeq ($(LWS_WITH_CLIENT),y)
		CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/roles/http/client
		LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/http/client
	endif

	ifneq ($(LWS_WITH_CLIENT)_$(LWS_WITH_CACHE_NSCOOKIEJAR),y_y)
		LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/http/cookie.c
	else
		LWS_CFLAGS_DEFS += -DLWS_WITH_CACHE_NSCOOKIEJAR
	endif

	ifneq ($(LWS_WITHOUT_SERVER), y)
		LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/http/server

		ifneq ($(LWS_WITH_HTTP_PROXY)_$(LWS_WITH_HUBBUB),y_y)
			LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/http/server/rewrite.c
		else
			ifeq ($(LWS_WITH_HTTP_PROXY),y)
				LWS_CFLAGS_DEFS += -DLWS_WITH_HTTP_PROXY
			endif
			ifeq ($(LWS_WITH_HUBBUB),y)
				LWS_CFLAGS_DEFS += -DLWS_WITH_HUBBUB
			endif
		endif

		ifneq ($(LWS_WITH_ACCESS_LOG),y)
			LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/http/server/access-log.c
		else
			LWS_CFLAGS_DEFS += -DLWS_WITH_ACCESS_LOG
		endif

		ifneq ($(LWS_WITH_RANGES),y)
			LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/http/server/ranges.c
		else
			LWS_CFLAGS_DEFS += -DLWS_WITH_RANGES
		endif

		LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/roles/http/server/fops-zip.c
	endif
endif
