ifeq ($(BUILD_COMP_SECBOOT_EN), y)
BASEPKG_OBJ := $(BUILDDIR)/$(BINNAME)Sigend.bin
else
BASEPKG_OBJ := $(BUILDDIR)/$(BINNAME).bin
endif

MODEMPKG ?= F6B_A
BINPFK_ADD_ITEMS	:= 

BASEPKG_DIR			:= $(TOP)/components/basePkg
BASEPKG_MDM_DIR		:= $(BASEPKG_DIR)/$(MODEMPKG)

BASEPKG_PARTITION	:= $(BASEPKG_MDM_DIR)/partition_info.txt
BASEPKG_MEMMAP		:= $(BASEPKG_MDM_DIR)/mem_map.txt
BASEPKG_COMDB		:= $(BASEPKG_MDM_DIR)/comdb.txt
BASEPKG_ELF			:= $(BASEPKG_MDM_DIR)/ap_lierda_app.elf

BASEPKG_LIB			:= $(foreach dir, $(BASEPKG_MDM_DIR)/lib, $(wildcard $(dir)/*.a))
ifneq ($(origin EXDEMO_LVGL_USE_SOURCE_LIB), undefined)
BASEPKG_LIB			:= $(filter-out %/liblvgl.a,$(BASEPKG_LIB))
endif

BASEPKG_BL_BIN		:= $(BASEPKG_MDM_DIR)/ap_bootloader.bin
BASEPKG_AP_BIN		:= $(BASEPKG_MDM_DIR)/ap_lierda_app.bin
BASEPKG_CP_BIN		:= $(BASEPKG_MDM_DIR)/cp-demo-flash.bin

BASEPKG_APP_ADDR 	:= $(shell grep -a -w "appsdk" $(BASEPKG_PARTITION) | sed -n 's/[^│]*│[^│]*│[^│]*│[^│]*│[^│]*│ *\([0-9A-Fa-fx]*\).*/\1/p')
BASEPKG_APP_SIZE 	:= $(shell grep -a -w "appsdk" $(BASEPKG_PARTITION) | sed -n 's/[^│]*│[^│]*│[^│]*│[^│]*│[^│]*│[^│]*│ *\([0-9A-Fa-fx]*\).*/\1/p')
BASEPKG_RAM_ADDR 	:= $(shell grep -a -w "appsdkRam" $(BASEPKG_PARTITION) | sed -n 's/[^│]*│[^│]*│[^│]*│[^│]*│[^│]*│ *\([0-9A-Fa-fx]*\).*/\1/p')
BASEPKG_RAM_SIZE 	:= $(shell grep -a -w "appsdkRam" $(BASEPKG_PARTITION) | sed -n 's/[^│]*│[^│]*│[^│]*│[^│]*│[^│]*│[^│]*│ *\([0-9A-Fa-fx]*\).*/\1/p')
BASEPKG_FS_ADDR 	:= $(shell grep -a -w "fs" $(BASEPKG_PARTITION) | sed -n 's/.*raw *│ *\([0-9A-Fa-fx]*\).*/\1/p')
BASEPKG_FS_SIZE 	:= $(shell grep -a -w "fs" $(BASEPKG_PARTITION) | sed -n 's/[^│]*│[^│]*│[^│]*│[^│]*│ *\([0-9A-Fa-fx]*\).*/\1/p')
BASEPKG_FS_END		:= $(shell printf "0x%X" $$(($(BASEPKG_FS_ADDR) + $(BASEPKG_FS_SIZE))))
BASEPKG_PRODUCT 	:= $(shell grep -a -w "^\#define\ PKG_PRODUCT" $(BASEPKG_MEMMAP) | sed 's/^\#define\ PKG_PRODUCT[[:space:]]*//')

BASEPKG_BL_PKGIMG_LNA := $(shell grep -a -w "^\#define\ BL_PKGIMG_LNA" $(BASEPKG_MEMMAP) | sed -e 's/^\#define\ BL_PKGIMG_LNA[[:space:]]*//' -e 's/[()]//g' -e 's/[[:space:]]*$$//')
BASEPKG_BOOTLOADER_PKGIMG_LIMIT_SIZE := $(shell grep -a -w "^\#define\ BOOTLOADER_PKGIMG_LIMIT_SIZE" $(BASEPKG_MEMMAP) | sed -e 's/^\#define\ BOOTLOADER_PKGIMG_LIMIT_SIZE[[:space:]]*//' -e 's/[()]//g' -e 's/[[:space:]]*$$//')
BASEPKG_AP_PKGIMG_LNA := $(shell grep -a -w "^\#define\ AP_PKGIMG_LNA" $(BASEPKG_MEMMAP) | sed -e 's/^\#define\ AP_PKGIMG_LNA[[:space:]]*//' -e 's/[()]//g' -e 's/[[:space:]]*$$//')
BASEPKG_AP_PKGIMG_LIMIT_SIZE := $(shell grep -a -w "^\#define\ AP_PKGIMG_LIMIT_SIZE" $(BASEPKG_MEMMAP) | sed -e 's/^\#define\ AP_PKGIMG_LIMIT_SIZE[[:space:]]*//' -e 's/[()]//g' -e 's/[[:space:]]*$$//')
BASEPKG_CP_PKGIMG_LNA := $(shell grep -a -w "^\#define\ CP_PKGIMG_LNA" $(BASEPKG_MEMMAP) | sed -e 's/^\#define\ CP_PKGIMG_LNA[[:space:]]*//' -e 's/[()]//g' -e 's/[[:space:]]*$$//')
BASEPKG_CP_PKGIMG_LIMIT_SIZE := $(shell grep -a -w "^\#define\ CP_PKGIMG_LIMIT_SIZE" $(BASEPKG_MEMMAP) | sed -e 's/^\#define\ CP_PKGIMG_LIMIT_SIZE[[:space:]]*//' -e 's/[()]//g' -e 's/[[:space:]]*$$//')
BASEPKG_LIERDA_MAP_FLASH_AP_SIZE := $(shell grep -a -w "^\#define\ LIERDA_MAP_FLASH_AP_SIZE" $(BASEPKG_MEMMAP) | sed -e 's/^\#define\ LIERDA_MAP_FLASH_AP_SIZE[[:space:]]*//' -e 's/[()]//g' -e 's/[[:space:]]*$$//')

CFLAGS_DEFS += -DLIERDA_MAP_FLASH_AP_SIZE="($(BASEPKG_LIERDA_MAP_FLASH_AP_SIZE))"
CFLAGS_DEFS += -DBL_PKGIMG_LNA="($(BASEPKG_BL_PKGIMG_LNA))"
CFLAGS_DEFS += -DBOOTLOADER_PKGIMG_LIMIT_SIZE="($(BASEPKG_BOOTLOADER_PKGIMG_LIMIT_SIZE))"
CFLAGS_DEFS += -DAP_PKGIMG_LNA="($(BASEPKG_AP_PKGIMG_LNA))"
CFLAGS_DEFS += -DAP_PKGIMG_LIMIT_SIZE="($(BASEPKG_AP_PKGIMG_LIMIT_SIZE))"
CFLAGS_DEFS += -DCP_PKGIMG_LNA="($(BASEPKG_CP_PKGIMG_LNA))"
CFLAGS_DEFS += -DCP_PKGIMG_LIMIT_SIZE="($(BASEPKG_CP_PKGIMG_LIMIT_SIZE))"


# Add chip type definition based on PKG_PRODUCT
ifneq ($(findstring EC716E, $(BASEPKG_PRODUCT)),)
CFLAGS_DEFS += -DCHIP_EC716
CFLAGS_DEFS += -DTYPE_EC716E
else ifneq ($(findstring EC718PM, $(BASEPKG_PRODUCT)),)
CFLAGS_DEFS += -DCHIP_EC718
CFLAGS_DEFS += -DTYPE_EC718M
CFLAGS_DEFS += -DTYPE_EC718PM
endif


ifneq ($(BASEPKG_APP_ADDR),)
CFLAGS_DEFS += -DCONFIG_APP_BURN_ADDR=$(BASEPKG_APP_ADDR)
endif

ifneq ($(BASEPKG_APP_SIZE),)
CFLAGS_DEFS += -DCONFIG_APP_BURN_SIZE=$(BASEPKG_APP_SIZE)
endif

ifneq ($(BASEPKG_RAM_ADDR),)
CFLAGS_DEFS += -DCONFIG_APP_RAM_ORIGIN_ADDR=$(BASEPKG_RAM_ADDR)
endif

ifneq ($(BASEPKG_RAM_SIZE),)
CFLAGS_DEFS += -DCONFIG_APP_RAM_LENGTH_ADDR=$(BASEPKG_RAM_SIZE)
endif

ifneq ($(BASEPKG_FS_ADDR),)
CFLAGS_DEFS += -DCONFIG_APP_DEFAULT_FS_ADDR=$(BASEPKG_FS_ADDR)
endif

ifneq ($(BASEPKG_FS_SIZE),)
CFLAGS_DEFS += -DCONFIG_APP_DEFAULT_FS_SIZE=$(BASEPKG_FS_SIZE)
endif

ifneq ($(BASEPKG_FS_END),)
CFLAGS_DEFS += -DCONFIG_APP_DEFAULT_FS_END_ADDR=$(BASEPKG_FS_END)
endif

PREBUILDLIBS += $(BASEPKG_LIB)
OUTTARGET += $(BUILDDIR)/$(BINNAME).binpkg

$(BUILDDIR)/$(BINNAME).binpkg: _Generate_basebinpkg_ _Generate_binpkg_
	$(Q)$(COPY) $(BASEPKG_COMDB) $(BUILDDIR)/
	$(Q)$(ECHO) "========> Generate: $(BUILDDIR)/$(BINNAME)_$(APP_VERSION).binpkg"
	$(Q)$(ECHO) "========> Generate: $(BUILDDIR)/$(MODEMPKG)_base.binpkg"

_Generate_basebinpkg_:
	$(Q)$(FCELF) -M -input $(BASEPKG_BL_BIN) -addrname BL_PKGIMG_LNA -flashsize BOOTLOADER_PKGIMG_LIMIT_SIZE \
	-input $(BASEPKG_AP_BIN) -addrname AP_PKGIMG_LNA -flashsize AP_PKGIMG_LIMIT_SIZE \
	-input $(BASEPKG_CP_BIN) -addrname CP_PKGIMG_LNA -flashsize CP_PKGIMG_LIMIT_SIZE \
	-pkgmode 1 \
	-banoldtool 1 \
	-productname $(BASEPKG_PRODUCT) \
	-def $(BASEPKG_MEMMAP) \
	-outfile $(BUILDDIR)/$(MODEMPKG)_base.binpkg
	
_Generate_binpkg_: $(BASEPKG_OBJ) $(BUILDDIR)/mem_map.txt
	$(Q)$(FCELF) -M -input $(BASEPKG_BL_BIN) -addrname BL_PKGIMG_LNA -flashsize BOOTLOADER_PKGIMG_LIMIT_SIZE \
	-input $(BASEPKG_AP_BIN) -addrname AP_PKGIMG_LNA -flashsize AP_PKGIMG_LIMIT_SIZE \
	-input $(BASEPKG_CP_BIN) -addrname CP_PKGIMG_LNA -flashsize CP_PKGIMG_LIMIT_SIZE \
	-input $< -addrname PKGFLXAPP_APP0_LNA -flashsize PKGFLXAPP_APP0_SIZE \
	$(BINPFK_ADD_ITEMS) \
	-pkgmode 1 \
	-banoldtool 1 \
	-productname $(BASEPKG_PRODUCT) \
	-def $(BUILDDIR)/mem_map.txt \
	-outfile $(BUILDDIR)/$(BINNAME)_$(APP_VERSION).binpkg

