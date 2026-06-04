LIOTAPI_DIR := $(KERNELDIR)/lierda_api

CFLAGS_INC	+= -I $(LIOTAPI_DIR)/inc

# liot api inc
LIOT_INC_DIRS := $(wildcard $(LIOTAPI_DIR)/liot_*)
CFLAGS_INC += $(patsubst %, -I%, $(LIOT_INC_DIRS))