ROUTER_DIR := examples/router
ROUTER_SRC := $(ROUTER_DIR)/src

DEFINI:= $(TOP)/$(ROUTER_DIR)/default.ini

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
# web 各层头文件路径（项目内相对头：api/hal/resource + 根目录）
# 其余依赖(liot_router / liot_dev / mbedtls 等)由各自组件 Makefile 全局加入
CFLAGS_INC += -I $(TOP)/$(ROUTER_DIR)
CFLAGS_INC += -I $(TOP)/$(ROUTER_DIR)/api
CFLAGS_INC += -I $(TOP)/$(ROUTER_DIR)/hal
CFLAGS_INC += -I $(TOP)/$(ROUTER_DIR)/resource

# ── 前端资源自动生成(pre-build) ──
# Makefile 解析阶段执行：把 ui/ 下网页文件 gzip 后转成 resource/liot_router_resource.c。
# 改前端只需改 ui/ 后重新编译即自动刷新，无需手动跑脚本。
ROUTER_WEB_GEN := $(shell cd $(TOP)/$(ROUTER_DIR) && python3 tools/gen_web_asset.py ui resource/liot_router_resource.c resource/liot_router_resource.h 2>&1)
$(info router web asset: $(ROUTER_WEB_GEN))

# web 主入口 + 资源 + API 模块 + HAL 适配
ROUTER_COBJSTEMP += $(ROUTER_SRC)/liot_router_web_main.o
ROUTER_COBJSTEMP += $(ROUTER_DIR)/resource/liot_router_resource.o
ROUTER_COBJSTEMP += $(ROUTER_DIR)/hal/liot_router_hal_ec718.o
ROUTER_COBJSTEMP += $(ROUTER_DIR)/api/liot_router_auth.o
ROUTER_COBJSTEMP += $(ROUTER_DIR)/api/liot_router_sysinfo.o
ROUTER_COBJSTEMP += $(ROUTER_DIR)/api/liot_router_network.o
ROUTER_COBJSTEMP += $(ROUTER_DIR)/api/liot_router_lanconfig.o
ROUTER_COBJSTEMP += $(ROUTER_DIR)/api/liot_router_system.o
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
