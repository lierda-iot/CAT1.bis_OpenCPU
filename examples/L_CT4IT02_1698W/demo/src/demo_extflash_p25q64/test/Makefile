SHELL := /bin/bash

HOST_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
SEAM_DIR := $(abspath $(HOST_DIR)/..)
PROJECT_DIR := $(abspath $(SEAM_DIR)/../../..)
REPO_ROOT := $(abspath $(PROJECT_DIR)/../..)
OUT_DIR := $(HOST_DIR)/out
TARGET := $(OUT_DIR)/demo_extflash_p25q64_core_test

CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS += -DUNITY_USE_COMMAND_LINE_ARGS
CFLAGS += -I$(REPO_ROOT)/components/thirdparty/unity/unity/src
CFLAGS += -I$(SEAM_DIR)

UNITY_SRC := $(REPO_ROOT)/components/thirdparty/unity/unity/src/unity.c
TEST_SRC := $(HOST_DIR)/test_demo_extflash_p25q64_core.c
CORE_HDR := $(SEAM_DIR)/demo_extflash_p25q64_core.h

.PHONY: test clean list-tests smoke-test

SMOKE_TEST := test_tick_elapsed_forward_and_wraparound

test: $(TARGET)
	@$(TARGET)

smoke-test: $(TARGET)
	@out="$$($(TARGET))"; \
	echo "$$out"; \
	printf '%s' "$$out" | grep -q '$(SMOKE_TEST).*PASS' && \
		printf '%s' "$$out" | grep -q 'OK' || \
		{ echo "SMOKE FAIL: $(SMOKE_TEST)" >&2; exit 1; }

$(TARGET): $(UNITY_SRC) $(TEST_SRC) $(CORE_HDR)
	@mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) $(UNITY_SRC) $(TEST_SRC) -o $@

clean:
	@rm -rf $(OUT_DIR)

list-tests:
	@printf '%s\n' \
		"test_fill_pattern_sequence_and_wrap" \
		"test_tick_elapsed_forward_and_wraparound"
