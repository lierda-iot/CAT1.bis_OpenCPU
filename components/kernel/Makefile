KERNELDIR := $(TOP)/components/kernel

CFLAGS_INC	+= -I $(KERNELDIR)/include

include $(KERNELDIR)/ecapi/Makefile
include $(KERNELDIR)/lierda_api/Makefile

KERNEL_COBJSTEMP 	:= core/appram.o

KERNEL_COBJS := $(addprefix $(BUILDDIR)/components/kernel/, $(KERNEL_COBJSTEMP))

obj-y	+= components/kernel/core/main.o
obj-y	+= components/kernel/core/syscalls.o

ifeq ($(TOOLCHAIN),GCC)
lib-y	+= lib_Kernel.a

$(BUILDDIR)/lib/lib_Kernel.a: $(KERNEL_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif