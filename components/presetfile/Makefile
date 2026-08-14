PRESETFILE_DIR := $(TOP)/components/presetfile

# LFS image parameters
LFS_BLOCK_SIZE := 4096

ifeq ($(BUILD_PRESET_FILE_INSIDELFS_EN), y)
include $(PRESETFILE_DIR)/insidelfs/Makefile
endif

ifeq ($(BUILD_PRESET_FILE_EXTFLASHLFS_EN), y)
include $(PRESETFILE_DIR)/extflashlfs/Makefile
endif
