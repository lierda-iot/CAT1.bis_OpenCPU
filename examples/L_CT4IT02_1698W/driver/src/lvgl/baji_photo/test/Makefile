SHELL := /bin/bash

HOST_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
SEAM_DIR := $(abspath $(HOST_DIR)/..)
PROJECT_DIR := $(abspath $(SEAM_DIR)/../../../..)
REPO_ROOT := $(abspath $(PROJECT_DIR)/../..)
OUT_DIR := $(HOST_DIR)/out
TARGET := $(OUT_DIR)/baji_photo_core_utils_test

CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS += -DUNITY_USE_COMMAND_LINE_ARGS
CFLAGS += -DBAJI_PHOTO_ENABLE_GIF_SUPPORT=1
CFLAGS += -DBAJI_PHOTO_ENABLE_JPEG_SUPPORT=1
CFLAGS += -DBAJI_PHOTO_JPEG_MAX_COMPRESSED_SIZE=1048576u
CFLAGS += -DBAJI_PHOTO_ENABLE_PNG_SUPPORT=0
CFLAGS += -DBAJI_PHOTO_HTTP_RAW_RGB565_VERIFY=1
CFLAGS += -I$(REPO_ROOT)/components/thirdparty/unity/unity/src
CFLAGS += -I$(SEAM_DIR)

UNITY_SRC := $(REPO_ROOT)/components/thirdparty/unity/unity/src/unity.c
BAJI_UTILS_SRC := $(SEAM_DIR)/baji_photo_core_utils.c
TEST_SRC := $(HOST_DIR)/test_baji_photo_core_utils.c

.PHONY: test clean list-tests smoke-test

SMOKE_TEST := test_manifest_validators_reject_bad_values

test: $(TARGET)
	@$(TARGET)

smoke-test: $(TARGET)
	@out="$$($(TARGET))"; \
	echo "$$out"; \
	printf '%s' "$$out" | grep -q '$(SMOKE_TEST).*PASS' && \
		printf '%s' "$$out" | grep -q 'OK' || \
		{ echo "SMOKE FAIL: $(SMOKE_TEST)" >&2; exit 1; }

$(TARGET): $(UNITY_SRC) $(BAJI_UTILS_SRC) $(TEST_SRC)
	@mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	@rm -rf $(OUT_DIR)

list-tests:
	@printf '%s\n' \
		"test_skip_ws_and_find_key" \
		"test_parse_json_string_and_numbers" \
		"test_parse_manifest_format_matches_project_macros" \
		"test_manifest_validators_reject_bad_values" \
		"test_true_color_bounds_and_size_follow_project_limits"
