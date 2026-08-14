EXT_LFS_DIR 	:= $(PRESETFILE_DIR)/extflashlfs
#ext flash root dir
EXFLASH_FILES_ROOT_DIR ?= $(EXT_LFS_DIR)/files

# if customer not set EXT_LFS_BIN=customer_image, base EXFLASH_FILES_ROOT_DIR create file image.
ifeq ($(EXT_LFS_BIN), )
EXT_LFS_BIN 	   := $(BUILDDIR)/ext_lfs.bin
EXT_LFS_FILES_SIZE := $(shell stat -c %s $(EXFLASH_FILES_ROOT_DIR))
EXT_LFS_IMAGE_SIZE := $(shell echo $$(( ($(EXT_LFS_FILES_SIZE) / $(LFS_BLOCK_SIZE) + 3) * $(LFS_BLOCK_SIZE) )))

OUTTARGET += $(EXT_LFS_BIN)
$(EXT_LFS_BIN):
	$(Q)rm -f $@
	$(Q)$(LFSUTIL) -i $@ -c -S $(EXT_LFS_IMAGE_SIZE) -B $(LFS_BLOCK_SIZE)
	$(Q)$(LFSUTIL) -i $@ -w -A -D $(EXFLASH_FILES_ROOT_DIR)
	$(Q)$(ECHO) "GE $(EXT_LFS_BIN)"
endif

ifneq ($(EXFLASH_FILES_ROOT_DIR), )
OUTTARGET += pkgextfs
pkgextfs: $(EXT_LFS_BIN)
	$(Q)$(LFSUTIL) -i $< -w -A -D $(EXFLASH_FILES_ROOT_DIR)
	$(Q)$(ECHO) "PKGEXFS $(EXT_LFS_BIN)"
endif

EF_IMAGE_EN  ?= $(shell sed -n '/\[EXTFLASH IMAGE\]/,/\[/{s/^efImageEn=//p}' $(DEFINI))
EF_SPI_PORT  ?= $(shell sed -n '/\[EXTFLASH IMAGE\]/,/\[/{s/^efSpiPort=//p}' $(DEFINI))
EF_LFS_LNA   ?= $(shell sed -n '/\[EXTFLASH IMAGE\]/,/\[/{s/^efLfsLna=//p}' $(DEFINI))
EF_LFS_SIZE  ?= $(shell sed -n '/\[EXTFLASH IMAGE\]/,/\[/{s/^efLfsSize=//p}' $(DEFINI))

# Set addrname prefix based on SPI port (SPI0 -> EF_, SPI1 -> EFID01_)
ifeq ($(EF_SPI_PORT), 0)
EF_LFS_ADDR_NAME := EF_IMG_LFS_LNA
EF_LFS_SIZE_NAME := EF_IMG_LFS_SIZE
else
EF_LFS_ADDR_NAME := EFID01_IMG_LFS_LNA
EF_LFS_SIZE_NAME := EFID01_IMG_LFS_SIZE
endif

CFLAGS_DEFS += -D$(EF_LFS_ADDR_NAME)="($(EF_LFS_LNA))"
CFLAGS_DEFS += -D$(EF_LFS_SIZE_NAME)="($(EF_LFS_SIZE))"

BINPFK_ADD_ITEMS += -input $(EXT_LFS_BIN) -addrname $(EF_LFS_ADDR_NAME) -flashsize $(EF_LFS_SIZE_NAME)
