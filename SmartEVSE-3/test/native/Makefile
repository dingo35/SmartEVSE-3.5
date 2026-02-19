# SmartEVSE-3 Native Test Suite
# Builds and runs state machine tests on macOS/Linux without embedded toolchain
#
# Usage:
#   make          - build all tests
#   make test     - build and run all tests
#   make clean    - remove build artifacts
#   make test_state_transitions  - build+run single suite

CC       ?= cc
CFLAGS   := -std=c11 -Wall -Wextra -Wpedantic -Werror -O0 -g -DEVSE_TESTING $(CFLAGS_EXTRA)
INCLUDES := -Iinclude -I../../src

SRC_DIR  := ../../src
TEST_DIR := tests
BUILD    := build

# Source files for state machine tests
EVSE_SRC := $(SRC_DIR)/evse_state_machine.c

# Source files for module-specific tests
MQTT_PARSER_SRC    := $(SRC_DIR)/mqtt_parser.c
HTTP_API_SRC       := $(SRC_DIR)/http_api.c
SERIAL_PARSER_SRC  := $(SRC_DIR)/serial_parser.c
LED_COLOR_SRC      := $(SRC_DIR)/led_color.c

# Discover all test files
TEST_SRCS := $(wildcard $(TEST_DIR)/test_*.c)
TEST_BINS := $(patsubst $(TEST_DIR)/%.c,$(BUILD)/%,$(TEST_SRCS))
TEST_NAMES := $(patsubst $(TEST_DIR)/test_%.c,%,$(TEST_SRCS))

.PHONY: all test clean help $(TEST_NAMES)

all: $(TEST_BINS)

# Module-specific build rules (must come before the generic rule)
$(BUILD)/test_mqtt_parser: $(TEST_DIR)/test_mqtt_parser.c $(MQTT_PARSER_SRC) $(SRC_DIR)/mqtt_parser.h include/*.h | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(TEST_DIR)/test_mqtt_parser.c $(MQTT_PARSER_SRC)

$(BUILD)/test_http_api: $(TEST_DIR)/test_http_api.c $(HTTP_API_SRC) $(SRC_DIR)/http_api.h include/*.h | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(TEST_DIR)/test_http_api.c $(HTTP_API_SRC)

$(BUILD)/test_serial_parser: $(TEST_DIR)/test_serial_parser.c $(SERIAL_PARSER_SRC) $(SRC_DIR)/serial_parser.h include/*.h | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(TEST_DIR)/test_serial_parser.c $(SERIAL_PARSER_SRC)

$(BUILD)/test_led_color: $(TEST_DIR)/test_led_color.c $(LED_COLOR_SRC) $(SRC_DIR)/led_color.h include/*.h | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(TEST_DIR)/test_led_color.c $(LED_COLOR_SRC)

# Build each state machine test binary (generic rule)
$(BUILD)/test_%: $(TEST_DIR)/test_%.c $(EVSE_SRC) include/*.h $(SRC_DIR)/*.h | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(TEST_DIR)/test_$*.c $(EVSE_SRC)

$(BUILD):
	mkdir -p $(BUILD)

# Run all tests
test: $(TEST_BINS)
	@echo ""
	@echo "============================================"
	@echo "  SmartEVSE-3 Native Test Suite"
	@echo "============================================"
	@total_pass=0; total_fail=0; total_suites=0; failed_suites=""; \
	for bin in $(TEST_BINS); do \
		total_suites=$$((total_suites + 1)); \
		if $$bin; then \
			total_pass=$$((total_pass + 1)); \
		else \
			total_fail=$$((total_fail + 1)); \
			failed_suites="$$failed_suites $$(basename $$bin)"; \
		fi; \
	done; \
	echo ""; \
	echo "============================================"; \
	echo "  TOTAL: $$total_suites suites, $$total_pass passed, $$total_fail failed"; \
	if [ $$total_fail -gt 0 ]; then \
		echo "  FAILED:$$failed_suites"; \
		echo "============================================"; \
		exit 1; \
	else \
		echo "  ALL SUITES PASSED"; \
		echo "============================================"; \
	fi

# Convenience targets: make <suite_name> runs that suite
$(TEST_NAMES): %: $(BUILD)/test_%
	@$(BUILD)/test_$*

clean:
	rm -rf $(BUILD)

help:
	@echo "Available targets:"
	@echo "  make          - build all tests"
	@echo "  make test     - build and run all tests"
	@echo "  make clean    - remove build artifacts"
	@echo ""
	@echo "Individual suites:"
	@for name in $(TEST_NAMES); do echo "  make $$name"; done
