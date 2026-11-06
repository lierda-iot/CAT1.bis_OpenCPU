
###########################################################
# multi app ctrl
#
# 本文件仅在 BUILD_COMP_MULTIAPP_EN=y 时被 components/Makefile 引入
# APP_SLOT : 当前工程所在 slot，0 = APP0（默认，含多 APP 加载器）
# MULTIAPP_SLOTX_FLASH_START/SIZE : 子 APP 区的 flash 起始地址/大小
# MULTIAPP_SLOTX_RAM_START/SIZE   : 子 APP 区的 RAM 起始地址/大小
# SUBAPP_BIN : 子 APP 的 bin 文件路径（可选，用于自动打包）
###########################################################
MULTIAPPDIR := $(TOP)/components/multiapp
CFLAGS_INC += -I $(MULTIAPPDIR)

APP_SLOT ?= 0

###########################################################
# 多 APP 配置（子APP占用flash和RAM大小）
# 当 APP_SLOT=0 时，SIZE为所有APPx占用的flash和RAM总大小。即app1+app2+...
# 当 APP_SLOT≠0 时，SIZE为子APPx占用的flash和RAM大小。
###########################################################
MULTIAPP_SLOTX_FLASH_START ?= 0x00000000
MULTIAPP_SLOTX_FLASH_SIZE  ?= 0x00
MULTIAPP_SLOTX_RAM_START   ?= 0x00000000
MULTIAPP_SLOTX_RAM_SIZE    ?= 0x00

###########################################################
# 子APP烧录Bin文件路径
###########################################################
SUBAPP_BIN ?=

# 4k 对齐检查（仅当 slot 大小非 0 时检查）
# FLASH 需要 4k 对齐（擦除粒度），RAM 只需 4 字节对齐
ifneq ($(MULTIAPP_SLOTX_FLASH_SIZE),0x00)
    ifeq ($(shell printf '%d' $$(($(MULTIAPP_SLOTX_FLASH_SIZE) & 0xFFF))),0)
    else
        $(error MULTIAPP_SLOTX_FLASH_SIZE=$(MULTIAPP_SLOTX_FLASH_SIZE) 未 4k 对齐)
    endif
endif
ifneq ($(MULTIAPP_SLOTX_RAM_SIZE),0x00)
    ifeq ($(shell printf '%d' $$(($(MULTIAPP_SLOTX_RAM_SIZE) & 0x3))),0)
    else
        $(error MULTIAPP_SLOTX_RAM_SIZE=$(MULTIAPP_SLOTX_RAM_SIZE) 未 4 字节对齐)
    endif
endif

# 传递配置到 C 代码
CFLAGS_DEFS += -DCONFIG_MULTIAPP_ENABLE
CFLAGS_DEFS += -DCONFIG_MULTIAPP_SLOTX_FLASH_START=$(MULTIAPP_SLOTX_FLASH_START)
CFLAGS_DEFS += -DCONFIG_MULTIAPP_SLOTX_FLASH_SIZE=$(MULTIAPP_SLOTX_FLASH_SIZE)
CFLAGS_DEFS += -DCONFIG_MULTIAPP_SLOTX_RAM_START=$(MULTIAPP_SLOTX_RAM_START)
CFLAGS_DEFS += -DCONFIG_MULTIAPP_SLOTX_RAM_SIZE=$(MULTIAPP_SLOTX_RAM_SIZE)

# APP_SLOT=0 时编译 multiapp 加载器，APP_SLOT≠0 时不编译

ifeq ($(APP_SLOT), 0)
# 如果指定了 SUBAPP_BIN，将其打包进 binpkg
ifneq ($(wildcard $(SUBAPP_BIN)),)
    BINPFK_ADD_ITEMS += -input $(SUBAPP_BIN) -addrname PKGFLXAPP_APPX_LNA -flashsize PKGFLXAPP_APPX_SIZE
endif
endif

# APP0：编译 multiapp 加载器代码
obj-y += components/multiapp/liot_multiapp.o

ifeq ($(TOOLCHAIN),GCC)
lib-y += lib_multiapp.a

$(BUILDDIR)/lib/lib_multiapp.a: $(BUILDDIR)/components/multiapp/liot_multiapp.o
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif
