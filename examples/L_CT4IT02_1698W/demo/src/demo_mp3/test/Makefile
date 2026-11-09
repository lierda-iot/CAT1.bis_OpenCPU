SHELL := /bin/bash

HOST_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
SEAM_DIR := $(abspath $(HOST_DIR)/..)
PROJECT_DIR := $(abspath $(SEAM_DIR)/../../..)
REPO_ROOT := $(abspath $(PROJECT_DIR)/../..)
OUT_DIR := $(HOST_DIR)/out
TARGET := $(OUT_DIR)/demo_mp3_core_test

CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS += -DUNITY_USE_COMMAND_LINE_ARGS
CFLAGS += -I$(REPO_ROOT)/components/thirdparty/unity/unity/src
CFLAGS += -I$(SEAM_DIR)

UNITY_SRC := $(REPO_ROOT)/components/thirdparty/unity/unity/src/unity.c
CORE_SRC := $(SEAM_DIR)/demo_mp3_core.c
TEST_SRC := $(HOST_DIR)/test_demo_mp3_core.c

.PHONY: test clean list-tests smoke-test

SMOKE_TEST := test_duration_estimate_single_frame_cbr

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
		"test_frame_offset_skips_id3_junk_no_sync_returns_zero" \
		"test_sample_rate_and_bitrate_table_bounds" \
		"test_duration_estimate_single_frame_cbr" \
		"test_duration_estimate_walks_frames_skips_bad" \
		"test_current_ms_state_machine" \
		"test_format_time_and_state_name"
