AMRDECODER_DIR := $(TOP)/components/thirdparty/amr_decoder

CFLAGS_INC       	+=  -I $(AMRDECODER_DIR)/inc \
						-I $(AMRDECODER_DIR)/amr_common/dec/include \
						-I $(AMRDECODER_DIR)/amr_nb/common/include \
						-I $(AMRDECODER_DIR)/amr_nb/dec/include \
						-I $(AMRDECODER_DIR)/amr_nb/enc/include \
						-I $(AMRDECODER_DIR)/amr_wb/dec/include \
						-I $(AMRDECODER_DIR)/opencore-amrnb \
						-I $(AMRDECODER_DIR)/opencore-amrwb \
						-I $(AMRDECODER_DIR)/oscl \
						-I $(AMRDECODER_DIR)

AMRDECODER_SRC_DIRS += 	$(AMRDECODER_DIR) \
						$(AMRDECODER_DIR)/amr_nb/common/src \
						$(AMRDECODER_DIR)/amr_nb/dec/src \
						$(AMRDECODER_DIR)/amr_nb/enc/src \
						$(AMRDECODER_DIR)/amr_wb/dec/src \
						$(AMRDECODER_DIR)/src 

AMRDECODER_EXCLUDE_FILES += $(AMRDECODER_DIR)/amr_nb/enc/src/div_32.c \
						$(AMRDECODER_DIR)/amr_nb/enc/src/l_abs.c

AMRDECODER_CSRC = $(foreach dir, $(AMRDECODER_SRC_DIRS), $(wildcard $(dir)/*.c))
AMRDECODER_CFILES = $(filter-out $(AMRDECODER_EXCLUDE_FILES), $(AMRDECODER_CSRC))
AMRDECODER_CFILES_REL = $(patsubst $(TOP)/%, %, $(AMRDECODER_CFILES))
AMRDECODER_COBJSTEMP := $(patsubst %.c, %.o, $(AMRDECODER_CFILES_REL))
AMRDECODER_COBJS := $(addprefix $(BUILDDIR)/, $(AMRDECODER_COBJSTEMP))

AMRDECODER_PPFILES := $(patsubst %.o, %.pp, $(AMRDECODER_COBJS))
PPFILES += $(AMRDECODER_PPFILES)

ifeq ($(TOOLCHAIN),GCC)
libusr-y += libamr_decoder.a

$(BUILDDIR)/lib/libusr/libamr_decoder.a: $(AMRDECODER_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^

endif