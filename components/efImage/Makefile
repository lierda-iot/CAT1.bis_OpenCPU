EF_IMAGE_DIR := $(MID_DIR)/efImage

EF_LFS_BIN ?= $(EF_IMAGE_DIR)/ext_lfs.bin

# Validate ext_lfs.bin exists
ifeq ($(wildcard $(EF_LFS_BIN)),)
$(error ext_lfs.bin not found at $(EF_LFS_BIN))
endif

# Set addrname prefix based on SPI port (SPI0 -> EF_, SPI1 -> EFID01_)
ifeq ($(EF_SPI_PORT), 0)
EF_PREFIX := EF
else
EF_PREFIX := EFID01
endif

EF_LFS_ADDR_NAME := $(EF_PREFIX)_IMG_LFS_LNA
EF_LFS_SIZE_NAME := $(EF_PREFIX)_IMG_LFS_SIZE

# Validate efLfsLna starts from 0x80000000
EF_LFS_LNA_VAL := $(shell printf "%d" $(EF_LFS_LNA))
EF_LFS_LNA_BASE := $(shell printf "%d" 0x80000000)
ifneq ($(shell [ $(EF_LFS_LNA_VAL) -ge $(EF_LFS_LNA_BASE) ] && echo ok),ok)
$(error efLfsLna must start from 0x80000000, current value: $(EF_LFS_LNA))
endif

# Validate and align efLfsSize to 4KB
EF_LFS_SIZE_VAL := $(shell printf "%d" $(EF_LFS_SIZE))
EF_LFS_SIZE_MOD := $(shell echo $$(($(EF_LFS_SIZE_VAL) % 4096)))
ifeq ($(EF_LFS_SIZE_MOD), 0)
EF_LFS_SIZE_ALIGNED := $(EF_LFS_SIZE)
else
EF_LFS_SIZE_ALIGNED := $(shell printf "0x%X" $$(((($(EF_LFS_SIZE_VAL) + 4095) / 4096) * 4096)))
$(warning efLfsSize not 4KB aligned, adjusted from $(EF_LFS_SIZE) to $(EF_LFS_SIZE_ALIGNED))
endif

# Validate ext_lfs.bin size does not exceed configured size
EF_LFS_BIN_SIZE := $(shell stat -c %s $(EF_LFS_BIN) 2>/dev/null || echo 0)
EF_LFS_SIZE_ALIGNED_VAL := $(shell printf "%d" $(EF_LFS_SIZE_ALIGNED))
ifneq ($(shell [ $(EF_LFS_BIN_SIZE) -le $(EF_LFS_SIZE_ALIGNED_VAL) ] && echo ok),ok)
$(error ext_lfs.bin size ($(EF_LFS_BIN_SIZE) bytes) exceeds efLfsSize ($(EF_LFS_SIZE_ALIGNED)))
endif

CFLAGS_DEFS += -DCONFIG_EXT_LFS_ENABLE
CFLAGS_DEFS += -DCONFIG_EXT_LFS_SIZE="($(EF_LFS_SIZE_ALIGNED))"
CFLAGS_DEFS += -D$(EF_LFS_ADDR_NAME)="($(EF_LFS_LNA))"
CFLAGS_DEFS += -D$(EF_LFS_SIZE_NAME)="($(EF_LFS_SIZE_ALIGNED))"

BINPFK_ADD_ITEMS += -input $(EF_LFS_BIN) -addrname $(EF_LFS_ADDR_NAME) -flashsize $(EF_LFS_SIZE_NAME)
