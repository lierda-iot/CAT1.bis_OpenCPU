SHELL := /bin/bash

HOST_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
SEAM_DIR := $(abspath $(HOST_DIR)/..)
PROJECT_DIR := $(abspath $(SEAM_DIR)/../../..)
REPO_ROOT := $(abspath $(PROJECT_DIR)/../..)
OUT_DIR := $(HOST_DIR)/out
TARGET := $(OUT_DIR)/location_core_utils_test

CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS += -DUNITY_USE_COMMAND_LINE_ARGS
CFLAGS += -I$(REPO_ROOT)/components/thirdparty/unity/unity/src
CFLAGS += -I$(SEAM_DIR)

UNITY_SRC := $(REPO_ROOT)/components/thirdparty/unity/unity/src/unity.c
CORE_SRC := $(SEAM_DIR)/location_core_utils.c
TEST_SRC := $(HOST_DIR)/test_location_core_utils.c

.PHONY: test clean list-tests smoke-test

SMOKE_TEST := test_coordinate_positive_negative_direction

test: $(TARGET)
	@$(TARGET)

smoke-test: $(TARGET)
	@out="$$($(TARGET))"; \
	echo "$$out"; \
	printf '%s' "$$out" | grep -q '$(SMOKE_TEST).*PASS' && \
		printf '%s' "$$out" | grep -q 'OK' || \
		{ echo "SMOKE FAIL: $(SMOKE_TEST)" >&2; exit 1; }

$(TARGET): $(UNITY_SRC) $(CORE_SRC) $(TEST_SRC)
	@mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	@rm -rf $(OUT_DIR)

list-tests:
	@printf '%s\n' \
		"test_coordinate_positive_negative_direction" \
		"test_compact_format_no_space_negative" \
		"test_boundary_and_truncation"
