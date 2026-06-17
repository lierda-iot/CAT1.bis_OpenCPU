LIOT_AI_PATH := $(TOP)/components/thirdparty/liot_ai

LIOT_PROJECT_NAME :=

CFLAGS_INC	+=	-I $(LIOT_AI_PATH)/utils \
				-I $(LIOT_AI_PATH)/framework

LIOT_AI_APP_SRC_DIRS += $(LIOT_AI_PATH)/utils \
						$(LIOT_AI_PATH)/framework

ifneq ($(LIOT_PROJECT_NAME),)
CFLAGS_INC	+=	-I $(LIOT_AI_PATH)/projects/$(LIOT_PROJECT_NAME)/app \
				-I $(LIOT_AI_PATH)/projects/$(LIOT_PROJECT_NAME)/modules \
				-I $(LIOT_AI_PATH)/projects/$(LIOT_PROJECT_NAME)/drivers

LIOT_AI_APP_SRC_DIRS += $(LIOT_AI_PATH)/projects/$(LIOT_PROJECT_NAME)/app \
						$(LIOT_AI_PATH)/projects/$(LIOT_PROJECT_NAME)/modules \
						$(LIOT_AI_PATH)/projects/$(LIOT_PROJECT_NAME)/drivers
endif				

LIOT_AI_APP_CSRC = $(foreach dir, $(LIOT_AI_APP_SRC_DIRS), $(wildcard $(dir)/*.c))
LIOT_AI_APP_COBJSTEMP := $(patsubst $(TOP)/%.c, %.o, $(LIOT_AI_APP_CSRC))
LIOT_AI_APP_COBJS := $(addprefix $(BUILDDIR)/, $(LIOT_AI_APP_COBJSTEMP))
PP_FILES += $(LIOT_AI_APP_COBJS:.o=.pp)

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_liot_ai.a

$(BUILDDIR)/lib/libusr/lib_liot_ai.a: $(LIOT_AI_APP_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif
