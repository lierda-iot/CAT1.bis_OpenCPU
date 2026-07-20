VSIM_DIR:= $(TOP)/components/thirdparty/vsim

include $(VSIM_DIR)/TGT/Makefile.inc
CFLAGS += -DFEATURE_TGT_VSIM_ENABLE
