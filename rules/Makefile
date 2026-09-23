PROJECT  		?= app
BASEPACKNAME	?= lierda_app.binpkg

ifeq ($(OS),Windows_NT)
    BUILD_ENV ?= windows
else
    BUILD_ENV ?= linux
endif

TOP			?= $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
GCC_PATH	?= $(TOP)/tools/toolchain/gcc
BUILDDIR	?= $(TOP)/gccout/$(PROJECT)
PATH		?= $(TOP)/tools/msys64/usr/bin;$(PATH)
BASEPACK	?= $(TOP)/baskpack/$(BASEPACK)
BINNAME	    ?= $(PROJECT)_$(MODEM)

TOOLCHAIN	?= GCC

include ./Makefile.rules