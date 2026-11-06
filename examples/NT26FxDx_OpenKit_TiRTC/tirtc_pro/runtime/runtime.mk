# Managed TiRTC integration, pinned to the official SDK 1.7.1 F6D_D package.
TIRTC_RUNTIME_DIR := $(TIRTC_DIR)/runtime
TIRTC_SDK_DIR := $(TIRTC_DIR)/sdk
CFLAGS_INC += -I $(TOP)/$(TIRTC_RUNTIME_DIR) -I $(TOP)/$(TIRTC_SDK_DIR)/include
CFLAGS_DEFS += -DHWDEMO_BINDING_EN -DHWDEMO_TIRTC_EN
CFLAGS_DEFS += -DTIRTC_MAX_SEND_BUFFER_BYTES=131072 -DTIRTC_LIBRARY_LOG_LEVEL=3
CFLAGS_DEFS += -DTIRTC_TRANSPORT_LOG_LEVEL=3 -DTIRTC_TRANSPORT_STATS_ENABLE=0
TIRTC_RUNTIME_CFILES := $(TOP)/$(TIRTC_RUNTIME_DIR)/tirtc_runtime.c
PREBUILDLIBS += $(TOP)/$(TIRTC_SDK_DIR)/lib/libTiRTC.a

# Native ARM objects are present alongside LTO IR. The bundled GCC10 must
# select native code; its LTO plugin cannot consume arbitrary producer IR.
LDFLAGS += -fno-lto
LDFLAGS += -Wl,--wrap=Logf -Wl,--wrap=sha256_hmac \
           -Wl,--wrap=tgtrp_connection_set_on_error \
           -Wl,--wrap=freertos_ThreadCreateWithStackSize \
           -Wl,--wrap=app_log_time -Wl,--wrap=printf

# Keep the native SDK and source-reviewed base integration on exact artifacts.
# RTOS/MQTT guards remain in the project Makefile; their wrappers are unchanged.
TIRTC_SDK_SHA256 := $(word 1,$(shell sha256sum "$(TOP)/$(TIRTC_SDK_DIR)/lib/libTiRTC.a"))
ifneq ($(TIRTC_SDK_SHA256),31c72553a082104b28eb8a2baa3e837b87afd25a54c2330eadc640dd0f287a5d)
$(error libTiRTC.a changed; review the native SDK ABI adapters)
endif
TIRTC_RUNTIME_BASE_ELF := $(TIRTC_BOARD_BASEPKG)/ap_lierda_app.elf
TIRTC_RUNTIME_BASE_SHA256 := $(word 1,$(shell sha256sum "$(TIRTC_RUNTIME_BASE_ELF)"))
ifneq ($(TIRTC_RUNTIME_BASE_SHA256),b2db1f908b409648e358f5211d50410a87b8b9381d9b8bcedeb802409860fa7f)
$(error F6D_D base ELF changed; review TiRTC FreeRTOS/lwIP/crypto compatibility)
endif
TIRTC_NETIF_LIST_ADDR := $(shell $(NM) -g --defined-only "$(TIRTC_RUNTIME_BASE_ELF)" 2>/dev/null | grep -w "netif_list" | sed -n '1s/^\([0-9A-Fa-f]*\).*/0x\1/p')
ifeq ($(strip $(TIRTC_NETIF_LIST_ADDR)),)
$(error TiRTC requires netif_list exported by the verified base ELF)
endif
LDFLAGS += -Wl,--defsym=netif_list=$(TIRTC_NETIF_LIST_ADDR)
