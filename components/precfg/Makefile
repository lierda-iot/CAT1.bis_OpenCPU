PRECFG_DIR := components/precfg
PRECFG_SRC := $(PRECFG_DIR)/src

CFLAGS_INC	+= -I $(TOP)/$(PRECFG_DIR)/inc
CFLAGS_INC  += -I $(BUILDDIR)/components/precfg/src

PRECFG_COBJSTEMP := $(PRECFG_SRC)/driveio.o
PRECFG_COBJSTEMP += $(PRECFG_SRC)/pin.o
PRECFG_COBJSTEMP += $(PRECFG_SRC)/defconfig.o

PRECFG_COBJS := $(addprefix $(BUILDDIR)/, $(PRECFG_COBJSTEMP))
PP_FILES += $(PRECFG_COBJS:.o=.pp)

# ini config: project-level overrides root-level
DEFINI ?= $(TOP)/config/default.ini
IOINI  ?= $(TOP)/config/iodriver.ini

$(BUILDDIR)/components/precfg/src/defaultini.h:
	$(Q)$(GEN_DEFCFG) -i $(DEFINI) -o $@
	$(Q)$(ECHO) "GE $@"

$(BUILDDIR)/components/precfg/src/pin.c:
	$(Q)$(GEN_IORESOURCE) -i $(IOINI) -o $@
	$(Q)$(ECHO) "GE $@"

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_precfg.a

$(TOP)/$(PRECFG_SRC)/defconfig.c: $(BUILDDIR)/components/precfg/src/defaultini.h

$(BUILDDIR)/lib/libusr/lib_precfg.a:  $(PRECFG_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif