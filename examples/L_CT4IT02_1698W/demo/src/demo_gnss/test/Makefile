SHELL := /bin/bash

HOST_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
SEAM_DIR := $(abspath $(HOST_DIR)/..)
PROJECT_DIR := $(abspath $(SEAM_DIR)/../../..)
REPO_ROOT := $(abspath $(PROJECT_DIR)/../..)
OUT_DIR := $(HOST_DIR)/out
TARGET := $(OUT_DIR)/demo_gnss_core_test

CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS += -DUNITY_USE_COMMAND_LINE_ARGS
CFLAGS += -I$(REPO_ROOT)/components/thirdparty/unity/unity/src
CFLAGS += -I$(SEAM_DIR)

UNITY_SRC := $(REPO_ROOT)/components/thirdparty/unity/unity/src/unity.c
TEST_SRC := $(HOST_DIR)/test_demo_gnss_core.c
CORE_HDR := $(SEAM_DIR)/demo_gnss_core.h

.PHONY: test clean list-tests smoke-test

SMOKE_TEST := test_coordinate_null_empty_rejected

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
		"test_coordinate_positive_and_negative_hemisphere" \
		"test_coordinate_invalid_hemisphere_and_minutes" \
		"test_coordinate_null_empty_rejected" \
		"test_coordinate_fraction_truncated_to_7_digits" \
		"test_split_fields_nmea_sentence_and_empty"
