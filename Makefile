# default target
build::
###########################################################
# User Configurable Options
###########################################################
export TOP						:= $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
export PROJECT 					?= app
export MODEM 					?= NT26FCNB60WNA
export MODEMPKG 				?= 
export BUILD_COMP_SECBOOT_EN	?= 
export BUILD_COMP_OTA_EN     	?= n

###########################################################
# build Environment
###########################################################
ifeq ($(OS),Windows_NT)
    export BUILD_ENV := windows
else
    export BUILD_ENV := linux
endif

export GCC_PATH		:= $(TOP)/tools/toolchain/gcc
export BUILDDIR		:= $(TOP)/gccout/$(PROJECT)
export BINNAME		:= $(PROJECT)_$(MODEM)
export TOOLCHAIN	:= GCC
###########################################################
# tools
###########################################################
include $(TOP)/rules/Makefile.tools

###########################################################
# build rules
###########################################################
.PHONY: all showParams checkGcc build _build_core cleanall clean showtime
all: clean build
check: _checkGcc_
build:: check showParams _build_core showtime

_build_core:
	@mkdir -p $(BUILDDIR)
	@(cd $(TOP)/rules && $(MAKE) -j4 -f Makefile all)

showParams:
	@echo ============== Build start at: $(shell $(GET_DATE)) ==================
	@echo Project is: $(PROJECT)
	@echo OS: $(BUILD_ENV)
	@echo GCC_PATH: $(GCC_PATH)
	@echo =====================================================================

cleanall:
	@rm -rf $(TOP)/gccout

clean:
	@rm -rf $(BUILDDIR)

_checkGcc_:
	@$(eval BUILD_START_TIME := $(shell $(GET_TIMESTAMP)))
	@if [ ! -d "$(GCC_PATH)" ];then \
    	mkdir -p $(GCC_PATH) && $(UNZIP_GCC) ; \
	fi

_packPythonTools_:
	@if ! command -v $(PYTHON) > $(NULL_DEVICE); then \
		echo "ERROR: $(PYTHON) not found, please install $(PYTHON). "; \
		exit 1; \
	fi
	pyinstaller -F $(TOP)/tools/appota/create_ota_package.py
	pyinstaller -F $(TOP)/tools/precfg/defaultCfg.py
	pyinstaller -F $(TOP)/tools/precfg/ioResource.py
	mv dist/create_ota_package* $(TOP)/tools/appota/
	mv dist/defaultCfg* $(TOP)/tools/precfg/
	mv dist/ioResource* $(TOP)/tools/precfg/
	rm build/ -rf
	rm dist/ -rf
	rm *.spec

showtime:
	$(eval BUILD_STOP_TIME := $(shell $(GET_TIMESTAMP)))
	$(eval RUN_TIME := $(shell echo $$(( $(BUILD_STOP_TIME) - $(BUILD_START_TIME) )) ))
	$(eval RUN_TIME_SEC := $(shell echo $$(( $(RUN_TIME) / 1000 )) ))
	$(eval RUN_TIME_MS  := $(shell echo $$(( $(RUN_TIME) % 1000 )) ))
	@echo
	@echo "=========== Build end : Time-used $(RUN_TIME_SEC).$(RUN_TIME_MS)s ==========="

help:
	@echo "======================================================"
	@echo "================= LSDK build command ================="
	@echo "======================================================"
	@echo -》 make : default as "make all"
	@echo -》 make all : clean and build
	@echo -》 make clean : clean target Project
	@echo -》 make cleanall : rm gccout/
	@echo -》 make args=secboot	: make and add params add \"secboot\"