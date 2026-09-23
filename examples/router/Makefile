ROUTER_DIR := examples/router
ROUTER_SRC := $(ROUTER_DIR)/src

DEFINI := $(TOP)/$(ROUTER_DIR)/default.ini

include $(TOP)/$(ROUTER_DIR)/config

CFLAGS_INC += -I $(TOP)/$(ROUTER_DIR)/inc

# Web 管理服务开关（y=启用）
LIOT_ROUTER_WEB ?= y
# 示例入口 + 用户层/持久化（始终编译）
ROUTER_COBJSTEMP += $(ROUTER_SRC)/user_main.o
ROUTER_COBJSTEMP += $(ROUTER_SRC)/liot_router_user.o
ROUTER_COBJSTEMP += $(ROUTER_SRC)/liot_router_nv.o

# LAN口条件编译
ifeq ($(LIOT_ROUTER_LAN0),y)
CFLAGS_DEFS += -DLIOT_ROUTER_LAN0_ENABLE
ROUTER_COBJSTEMP += $(ROUTER_SRC)/liot_router_atlan0.o
endif

ifeq ($(LIOT_ROUTER_LAN1),y)
CFLAGS_DEFS += -DLIOT_ROUTER_LAN1_ENABLE
ROUTER_COBJSTEMP += $(ROUTER_SRC)/liot_router_atlan1.o
endif

# 指示灯条件编译
ifeq ($(LIOT_ROUTER_LIGHT),y)
CFLAGS_DEFS += -DLIOT_ROUTER_LIGHT_ENABLE
ROUTER_COBJSTEMP += $(ROUTER_SRC)/liot_router_light.o
endif

# 复位按键条件编译
ifeq ($(LIOT_ROUTER_RESET),y)
CFLAGS_DEFS += -DLIOT_ROUTER_RESET_ENABLE
ROUTER_COBJSTEMP += $(ROUTER_SRC)/liot_router_reset.o
endif

# Web 管理服务条件编译
ifeq ($(LIOT_ROUTER_WEB),y)
CFLAGS_DEFS += -DLIOT_ROUTER_WEB_ENABLE
include $(TOP)/$(ROUTER_DIR)/webServer/Makefile
endif

ROUTER_COBJS := $(addprefix $(BUILDDIR)/, $(ROUTER_COBJSTEMP))
PP_FILES += $(ROUTER_COBJS:.o=.pp)

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_router.a

$(BUILDDIR)/lib/libusr/lib_router.a: $(ROUTER_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif
