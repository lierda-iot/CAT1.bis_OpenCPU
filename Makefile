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
export TOOLCHAIN	:= GCC

###########################################################
# burn (flash) options, override on command line, e.g.
#   make burn pkg=firmware.binpkg type=full mode=usb
#   make burn pkg=app.binpkg type=app mode=uart port=COM7
###########################################################
FLASH_SCRIPT	:= $(TOP)/tools/flashtools/download.py
mode			?= usb
port			?=
erase			?=
# pkg and type are required, no default values
pkg				?=
type			?=
###########################################################
# tools
###########################################################
include $(TOP)/rules/Makefile.tools

###########################################################
# build rules
###########################################################
.PHONY: all showParams checkGcc build _build_core cleanall clean showtime burn
all: clean build
check: _checkGcc_
build:: check showParams _build_core showtime

# Pass user-supplied values via environment instead of splicing them into the
# recipe text, so quotes/spaces/shell metacharacters cannot alter the command.
burn: export BURN_PKG  := $(pkg)
burn: export BURN_TYPE := $(type)
burn: export BURN_MODE := $(mode)
burn: export BURN_PORT := $(port)
burn:
	@if ! command -v $(PYTHON) > $(NULL_DEVICE); then \
		echo "ERROR: $(PYTHON) not found, please install python."; exit 1; \
	fi
	@if ! $(PYTHON) -c "import sys; sys.exit(sys.version_info < (3, 6))" > $(NULL_DEVICE); then \
		echo "ERROR: Python 3.6+ is required, got: $$($(PYTHON) --version 2>&1). Use PYTHON=<python3 path> to override."; exit 1; \
	fi
	@if ! $(PYTHON) -c "import serial.tools.list_ports" > $(NULL_DEVICE); then \
		echo "ERROR: pyserial not found, please run: $(PYTHON) -m pip install pyserial"; exit 1; \
	fi
	@if [ -z "$$BURN_PKG" ]; then \
		echo "ERROR: pkg parameter is required. Usage: make burn pkg=<file.binpkg> type=<full|app> mode=<usb|uart> [port=<PORT>]"; exit 1; \
	fi
	@if [ -z "$$BURN_TYPE" ]; then \
		echo "ERROR: type parameter is required. Usage: make burn pkg=<file.binpkg> type=<full|app> mode=<usb|uart> [port=<PORT>]"; exit 1; \
	fi
	@if [ ! -f "$$BURN_PKG" ]; then \
		echo "ERROR: pkg file not found: $$BURN_PKG"; exit 1; \
	fi
	@case "$$BURN_PKG" in \
		*.binpkg) ;; \
		*) echo "ERROR: pkg file must end with .binpkg, got: $$BURN_PKG"; exit 1 ;; \
	esac
	@case "$$BURN_TYPE" in \
		full|app) ;; \
		*) echo "ERROR: type must be 'full' or 'app', got: $$BURN_TYPE"; exit 1 ;; \
	esac
	@case "$$BURN_MODE" in \
		usb|uart) ;; \
		*) echo "ERROR: mode must be 'usb' or 'uart', got: $$BURN_MODE"; exit 1 ;; \
	esac
	@case "$$BURN_PORT" in \
		*[!A-Za-z0-9_]*) echo "ERROR: invalid port: $$BURN_PORT (only letters, digits and '_' allowed, e.g. COM7)"; exit 1 ;; \
	esac
	@echo "== Burning: $$BURN_PKG (mode=$$BURN_MODE$${BURN_PORT:+ port=$$BURN_PORT}$(if $(filter 1,$(erase)), erase) type=$$BURN_TYPE)"
	$(PYTHON) $(FLASH_SCRIPT) "$$BURN_PKG" --type "$$BURN_TYPE" --mode "$$BURN_MODE" $${BURN_PORT:+--port "$$BURN_PORT"} $(if $(filter 1,$(erase)),--erase)

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
	@echo '-> make : default as "make all"'
	@echo '-> make all : clean and build'
	@echo '-> make clean : clean target Project'
	@echo '-> make cleanall : rm gccout/'
	@echo '-> make args=secboot : make and add params add "secboot"'
	@echo '-> make burn pkg=gccout/app/app_NT26F6D0_01.binpkg type=full mode=usb : full flash via USB (auto-detect port)'
	@echo '-> make burn pkg=gccout/app/app_NT26F6D0_01.binpkg type=full mode=usb port=COM7 : full flash via USB on COM7'
	@echo '-> make burn pkg=gccout/app/app_NT26F6D0_01.binpkg type=app mode=usb port=COM7 : app flash via USB on COM7'
	@echo '-> make burn pkg=gccout/app/app_NT26F6D0_01.binpkg type=full mode=uart port=COM7 : full flash via UART (port required)'
	@echo '-> make burn pkg=gccout/app/app_NT26F6D0_01.binpkg type=full mode=uart port=COM7 erase=1 : full flash with erase'