PRESET_INSIDEDIR := $(PRESETFILE_DIR)/insidelfs

INSIDE_FILES_ROOT_DIR ?= $(PRESET_INSIDEDIR)/files

ifeq ($(INSIDE_LFS_BIN), )
# if customer not set INSIDE_LFS_BIN=customer_image, 
# base INSIDE_FILES_ROOT_DIR create file image.

INSIDE_LFS_BIN := $(BUILDDIR)/inside_lfs.bin
LFS_FILES_SIZE := $(shell stat -c %s $(INSIDE_FILES_ROOT_DIR))
LFS_IMAGE_SIZE := $(shell echo $$(( ($(LFS_FILES_SIZE) / $(LFS_BLOCK_SIZE) + 3) * $(LFS_BLOCK_SIZE) )))

OUTTARGET += $(INSIDE_LFS_BIN)
$(INSIDE_LFS_BIN):
	$(Q)rm -f $@
	$(Q)$(LFSUTIL) -i $@ -c -S $(LFS_IMAGE_SIZE) -B $(LFS_BLOCK_SIZE)
	$(Q)$(ECHO) "GE $(INSIDE_LFS_BIN)"
endif

ifneq ($(INSIDE_FILES_ROOT_DIR), )
OUTTARGET += pkginsidefs
pkginsidefs: $(INSIDE_LFS_BIN)
	$(Q)$(LFSUTIL) -i $< -w -A -D $(INSIDE_FILES_ROOT_DIR)
	$(Q)$(ECHO) "PKGFS $(INSIDE_LFS_BIN)"
endif

BINPFK_ADD_ITEMS += -input $(INSIDE_LFS_BIN) -addrname PKGFLXFS_IMG_LNA -flashsize PKGFLXFS_IMG_SIZE
