CUSTOM_DIR := examples/$(PROJECT)

include $(TOP)/$(CUSTOM_DIR)/config
include $(TOP)/rules/Makefile.modem

ifneq ($(filter $(APP_VARIANT),lite pro),$(APP_VARIANT))
$(error Unsupported APP_VARIANT "$(APP_VARIANT)"; use lite or pro)
endif

TIRTC_DIR := $(CUSTOM_DIR)/tirtc$(if $(filter pro,$(APP_VARIANT)),_pro,)

IOINI := $(TOP)/$(CUSTOM_DIR)/tirtc/board/iodriver.ini

CFLAGS_INC += -I $(TOP)/$(CUSTOM_DIR)/inc

ifeq ($(APP_VARIANT),pro)
include $(TOP)/$(TIRTC_DIR)/config
ifeq ($(BUILD_MODE),assets)
TIRTC_ASSET_INSTALLER := y
APP_VERSION := pro_assets
else ifeq ($(BUILD_MODE),full)
TIRTC_ASSET_INSTALLER := n
APP_VERSION := pro_app
else
TIRTC_ASSET_INSTALLER := n
APP_VERSION := pro_app
endif

else
TIRTC_ASSET_INSTALLER := n
endif

ifeq ($(APP_VARIANT),pro)
ifeq ($(BUILD_MODE),full)
TIRTC_FACTORY_PACKAGE := $(BUILDDIR)/$(BINNAME)_pro.binpkg
OUTTARGET += $(TIRTC_FACTORY_PACKAGE)

$(TIRTC_FACTORY_PACKAGE): $(BUILDDIR)/$(BINNAME).binpkg $(BUILDDIR)/$(BINNAME).bin \
	$(TOP)/$(TIRTC_DIR)/resource_store/package_factory.py \
	$(TOP)/$(TIRTC_DIR)/resource_store/build_factory_image.py
	$(Q)$(ECHO) "========> Generate: $@"
	$(Q)$(PYTHON) $(TOP)/$(TIRTC_DIR)/resource_store/package_factory.py \
		--repo $(TOP) --app $(TOP)/$(TIRTC_DIR) --build-dir $(BUILDDIR) \
		--normal-package $(BUILDDIR)/$(BINNAME)_$(APP_VERSION).binpkg \
		--app-binary $(BUILDDIR)/$(BINNAME).bin --output-package $@ \
		--fcelf $(FCELF)
endif
endif

include $(TOP)/$(TIRTC_DIR)/tirtc_app.mk

ifeq ($(APP_VARIANT),lite)
CUSTOM_COBJS := $(addprefix $(BUILDDIR)/, $(CUSTOM_COBJSTEMP))

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_$(PROJECT).a

$(BUILDDIR)/lib/libusr/lib_$(PROJECT).a: $(CUSTOM_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif
endif
