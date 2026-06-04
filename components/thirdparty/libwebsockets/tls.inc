CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/tls
LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/tls


ifneq ($(LWS_WITH_NETWORK),y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/tls-network.c
endif

ifneq ($(LWS_WITH_TLS_SESSIONS),y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/tls-sessions.c
else
	LWS_CFLAGS_DEFS += -DLWS_WITH_TLS_SESSIONS
endif

ifneq ($(LWS_WITH_TLS_JIT_TRUST),y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/tls-jit-trust.c
else
	LWS_CFLAGS_DEFS += -DLWS_WITH_TLS_JIT_TRUST
endif

ifeq ($(LWS_WITHOUT_SERVER), y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/tls-server.c
endif

ifeq ($(LWS_WITHOUT_CLIENT), y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/tls-client.c
endif

ifneq ($(LWS_WITH_TLS)_$(LWS_WITH_GENCRYPTO), y_y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/lws-gencrypto-common.c
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/lws-genec-common.c
endif

ifeq ($(LWS_WITH_MBEDTLS), y)
	LWS_CFLAGS_DEFS += -DLWS_WITH_MBEDTLS
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls

	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls

	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls/wrapper/include
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls/wrapper/include/platform
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls/wrapper/include/internal
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls/wrapper/include/openssl

	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls/wrapper/library
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls/wrapper/platform

	ifeq ($(LWS_WITHOUT_SERVER), y)
		LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls/mbedtls-server.c
	endif
	
	ifneq ($(LWS_WITH_GENCRYPTO), y)
		LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls/lws-genrsa.c
		LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls/lws-genaes.c
		LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls/lws-genec.c
		LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls/lws-gencrypto.c
	endif

	ifneq ($(LWS_WITH_TLS_JIT_TRUST), y)
		LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls/mbedtls-extensions.c
	endif

	ifneq ($(LWS_WITH_TLS_SESSIONS), y)
		LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/tls/mbedtls/mbedtls-session.c
	endif
endif