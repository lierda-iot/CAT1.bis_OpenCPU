# V04 product-only overrides. Vendor firmware, libraries and ABI headers all
# come from the official SDK; never keep another SDK copy under this folder.
TIRTC_BOARD_COMPAT := $(TOP)/$(TIRTC_DIR)/board_compat
TIRTC_BOARD_BASEPKG := $(TOP)/components/basePkg/$(MODEMPKG)
LD_FILE := $(TIRTC_BOARD_COMPAT)/app.ld
# Assets are installed explicitly; never embed/erase a filesystem by default.
EF_IMAGE_EN := 0

# Explicit project recipes replace only these two objects in the V4 build.
# Keep the SDK sources unchanged for other projects. The startup replacement
# retains the fix for the original SDK's byte-count/word-count zeroing bug.
$(BUILDDIR)/components/kernel/core/appram.o: $(TIRTC_BOARD_COMPAT)/appram.c
	@mkdir -p $(dir $@)
	$(ECHO) CC $<
	$(Q)$(CC) $(CFLAGS) $(CFLAGS_CPU) $(CFLAGS_INC) $(CFLAGS_DEFS) $(DEPFLAGS) -c $< -o $@

$(BUILDDIR)/components/driver/lcd/src/liot_lcdDev_ST7789.o: $(TIRTC_BOARD_COMPAT)/liot_lcdDev_ST7789.c
	@mkdir -p $(dir $@)
	$(ECHO) CC $<
	$(Q)$(CC) $(CFLAGS) $(CFLAGS_CPU) $(CFLAGS_INC) $(CFLAGS_DEFS) $(DEPFLAGS) -c $< -o $@


# Preserve the tested archive search order: UI, board drivers, then LVGL.
# This affects only this application, and retains any additional project libs.
override LIBURS = $(addprefix $(BUILDDIR)/lib/libusr/,$(filter-out liblvgl.a,$(libusr-y)) $(filter liblvgl.a,$(libusr-y)))
$(BUILDDIR)/$(BINNAME).elf: $(TIRTC_BOARD_COMPAT)/board_compat.mk $(TIRTC_BOARD_COMPAT)/app.ld
