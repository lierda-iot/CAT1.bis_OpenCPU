CUSTOM_SRC := $(CUSTOM_DIR)/demo/src/demo_3a
CUSTOM_COBJSTEMP := $(CUSTOM_SRC)/demo_3a.o

ifeq ($(MODEMPKG),)
$(error demo_3a requires MODEMPKG=F6D_D or MODEMPKG=F9D_D)
endif

ifneq ($(filter $(MODEMPKG),F6D_D F9D_D),$(MODEMPKG))
$(error demo_3a only supports MODEMPKG=F6D_D or MODEMPKG=F9D_D, got "$(MODEMPKG)")
endif

CFLAGS_DEFS += -DFEATURE_LIERDA_AUDIO_3A
CFLAGS_DEFS += -DFEATURE_SUPPORT_APP_PCM_MEM_POOL
