CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/misc
LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/misc

ifeq ($(LWS_WITH_NETWORK),y)
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/misc/cache-ttl
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/misc/cache-ttl

	ifneq ($(LWS_WITH_CACHE_NSCOOKIEJAR),y)
		LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/misc/cache-ttl/file.c
	endif
endif

ifeq ($(LWS_WITH_FTS),y)
	CFLAGS_INC += -I $(LWS_DIR)/$(LWS_VERSION)/lib/misc/fts
	LWS_SRC_DIRS += $(LWS_DIR)/$(LWS_VERSION)/lib/misc/fts
endif

ifneq ($(LWS_WITH_DISKCACHE),y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/misc/diskcache.c
endif

ifneq ($(LWS_WITH_STRUCT_JSON),y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/misc/lws-struct-lejp.c
endif

ifneq ($(LWS_WITH_STRUCT_SQLITE3),y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/misc/lws-struct-sqlite.c
endif

#linux exclude
LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/misc/fsmount.c

ifneq ($(LWS_WITH_DIR),y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/misc/dir.c
else
	LWS_CFLAGS_DEFS += -DLWS_WITH_DIR
endif

#if (LWS_WITH_THREADPOOL AND LWS_HAVE_PTHREAD_H)
#	list(APPEND SOURCES misc/threadpool/threadpool.c)
#endif()

ifneq ($(LWS_WITH_PEER_LIMITS),y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/misc/peer-limits.c
else
	LWS_CFLAGS_DEFS += -DLWS_WITH_PEER_LIMITS
endif

ifeq ($(LWS_WITHOUT_BUILTIN_SHA1), y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/misc/sha-1.c
endif

ifneq ($(LWS_WITH_LEJP),y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/misc/lejp.c
else
	LWS_CFLAGS_DEFS += -DLWS_WITH_LEJP
endif

ifneq ($(LWS_WITH_CBOR),y)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/misc/lecp.c
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/misc/ieeehalfprecision.c
else
	LWS_CFLAGS_DEFS += -DLWS_WITH_CBOR
endif

#unix exclude
LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/misc/getifaddrs.c
#other exclude
LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/misc/romfs.c

ifneq ($(LWS_WITHOUT_DAEMONIZE), n)
	LWS_EXCLUDE_FILES += $(LWS_DIR)/$(LWS_VERSION)/lib/misc/daemonize.c
endif