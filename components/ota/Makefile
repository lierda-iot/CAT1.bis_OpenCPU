ECSECURE_DIR := $(TOP)/components/ota

OUTTARGET 	+= $(BUILDDIR)/AppOTA.bin

$(BUILDDIR)/AppOTA.bin: $(BUILDDIR)/$(BINNAME).bin
	$(Q)$(OTASCRIPT) $< $(BASEPKG_APP_ADDR) $@
	$(Q)$(ECHO) "make APP OTA package -> $@"