ROUTER_WEB_DIR := $(ROUTER_DIR)/webServer

WEB_GEN_TOOL := $(TOP)/$(ROUTER_WEB_DIR)/tools/gen_web_asset.py
UI_DIR := $(TOP)/$(ROUTER_WEB_DIR)/ui

# web 各层头文件路径（项目内相对头：api/hal/resource + 根目录）
# 其余依赖(liot_router / liot_dev / mbedtls 等)由各自组件 Makefile 全局加入
CFLAGS_INC += -I $(TOP)/$(ROUTER_WEB_DIR)
CFLAGS_INC += -I $(TOP)/$(ROUTER_WEB_DIR)/api
CFLAGS_INC += -I $(TOP)/$(ROUTER_WEB_DIR)/hal

# web 主入口 + 资源 + API 模块 + HAL 适配
ROUTER_COBJSTEMP += $(ROUTER_WEB_DIR)/api/liot_router_web_main.o
ROUTER_COBJSTEMP += $(ROUTER_WEB_DIR)/liot_router_resource.o
ROUTER_COBJSTEMP += $(ROUTER_WEB_DIR)/hal/liot_router_hal_ec718.o
ROUTER_COBJSTEMP += $(ROUTER_WEB_DIR)/api/liot_router_auth.o
ROUTER_COBJSTEMP += $(ROUTER_WEB_DIR)/api/liot_router_sysinfo.o
ROUTER_COBJSTEMP += $(ROUTER_WEB_DIR)/api/liot_router_network.o
ROUTER_COBJSTEMP += $(ROUTER_WEB_DIR)/api/liot_router_lanconfig.o
ROUTER_COBJSTEMP += $(ROUTER_WEB_DIR)/api/liot_router_system.o

#将web/ui/ 下的前端文件 gzip 压缩后生成 C 数组，放在 liot_router_resource.c 中
$(BUILDDIR)/$(ROUTER_WEB_DIR)/liot_router_resource.c:
	$(Q)$(PYTHON) $(WEB_GEN_TOOL) $(UI_DIR) $@ >/dev/null
	$(Q)$(ECHO) "GE $@"