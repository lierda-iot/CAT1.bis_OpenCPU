SHELL := /bin/bash

TEST_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
MODULE ?= all
MODULE_MAP := watchface:driver/src/lvgl/watch_earth_2015/test
MODULE_MAP += baji_photo:driver/src/lvgl/baji_photo/test
MODULE_MAP += attitude:driver/src/lvgl/sc7a20h_page/test
MODULE_MAP += lvgl:driver/src/lvgl/test
MODULE_MAP += mp3:demo/src/demo_mp3/test
MODULE_MAP += salary:demo/src/demo_salary_calculator/test
MODULE_MAP += location:demo/src/demo_location/test
MODULE_MAP += gnss:demo/src/demo_gnss/test
MODULE_MAP += sc7a20h:demo/src/demo_sc7a20h/test
MODULE_MAP += mp3_standalone:demo/src/demo_mp3_standalone/test
MODULE_MAP += photo_lfs:demo/src/demo_lvgl_photo_lfs/test
MODULE_MAP += extflash_p25q64:demo/src/demo_extflash_p25q64/test
MODULE_MAP += xiaozhi:demo/src/demo_xiaozhi/test
MODULE_MAP += tone_1khz:demo/src/demo_1khz_tone/test
HOST_MODULES := $(foreach entry,$(MODULE_MAP),$(word 1,$(subst :, ,$(entry))))

.PHONY: host-test host-test-smoke list-tests

host-test:
	@modules="$(MODULE)"; \
	if [ "$$modules" = "all" ]; then \
		modules="$(HOST_MODULES)"; \
	fi; \
	status=0; \
	for module in $$modules; do \
		entry=""; \
		for candidate in $(MODULE_MAP); do \
			name="$${candidate%%:*}"; \
			path="$${candidate#*:}"; \
			if [ "$$name" = "$$module" ]; then entry="$$path"; break; fi; \
		done; \
		if [ -z "$$entry" ]; then \
			echo "unknown module: $$module" >&2; \
			status=2; \
			continue; \
		fi; \
		module_dir="$(TEST_ROOT)/../$$entry"; \
		if [ ! -d "$$module_dir" ]; then \
			echo "missing host test dir: $$module_dir" >&2; \
			status=2; \
			continue; \
		fi; \
		echo "== host-test $$module =="; \
		$(MAKE) -C "$$module_dir" test || status=$$?; \
	done; \
	exit $$status

host-test-smoke:
	@modules="$(MODULE)"; \
	if [ "$$modules" = "all" ]; then \
		modules="$(HOST_MODULES)"; \
	fi; \
	status=0; \
	for module in $$modules; do \
		entry=""; \
		for candidate in $(MODULE_MAP); do \
			name="$${candidate%%:*}"; \
			path="$${candidate#*:}"; \
			if [ "$$name" = "$$module" ]; then entry="$$path"; break; fi; \
		done; \
		if [ -z "$$entry" ]; then \
			echo "unknown module: $$module" >&2; \
			status=2; \
			continue; \
		fi; \
		module_dir="$(TEST_ROOT)/../$$entry"; \
		if [ ! -d "$$module_dir" ]; then \
			echo "missing host test dir: $$module_dir" >&2; \
			status=2; \
			continue; \
		fi; \
		echo "== host-test-smoke $$module =="; \
		$(MAKE) -C "$$module_dir" smoke-test || status=$$?; \
	done; \
	exit $$status

list-tests:
	@for module in $(HOST_MODULES); do \
		echo "$$module"; \
		for candidate in $(MODULE_MAP); do \
			name="$${candidate%%:*}"; \
			path="$${candidate#*:}"; \
			if [ "$$name" = "$$module" ]; then \
				$(MAKE) -C "$(TEST_ROOT)/../$$path" list-tests; \
				break; \
			fi; \
		done; \
	done
