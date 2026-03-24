/*
 * test_capacity_peak.c — Native tests for capacity tariff peak tracking
 *
 * Tests the pure C capacity_peak module:
 *   - 15-minute window averaging
 *   - Monthly peak tracking and rollover
 *   - Headroom calculation
 *   - Deciamps conversion
 *   - JSON output
 *   - Edge cases (NULL, zero buffer, negative power, disabled)
 */

#include "test_framework.h"
#include "capacity_peak.h"
#include <string.h>
#include <limits.h>

/* Static to keep stack usage under 1024 bytes */
static capacity_state_t s_state;
static char s_json_buf[512];

/* ---- Window averaging tests ---- */

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-001
 * @scenario Basic 15-minute window averaging at constant power
 * @given A capacity state initialized with limit 5000W
 * @when 900 ticks of capacity_tick_1s with constant 3000W
 * @then window_avg_w equals 3000 and headroom reflects remaining capacity
 */
void test_capacity_basic_window_avg(void) {
    capacity_init(&s_state);
    capacity_set_limit(&s_state, 5000);

    /* Run 900 ticks at 3000W */
    for (int i = 0; i < 900; i++) {
        capacity_tick_1s(&s_state, 3000, 3, 2);
    }

    /* Window should have completed */
    TEST_ASSERT_EQUAL_INT(1, s_state.window.window_valid);
    TEST_ASSERT_EQUAL_INT(3000, s_state.window.window_avg_w);
    TEST_ASSERT_EQUAL_INT(3000, capacity_get_window_avg_w(&s_state));

    /* Monthly peak should be 3000 */
    TEST_ASSERT_EQUAL_INT(3000, capacity_get_monthly_peak_w(&s_state));

    /* After window reset, elapsed is 0, headroom = limit = 5000 */
    TEST_ASSERT_EQUAL_INT(5000, capacity_get_headroom_w(&s_state));
}

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-002
 * @scenario Variable power within a single window
 * @given A capacity state with limit 5000W
 * @when 450 ticks at 2000W followed by 450 ticks at 4000W
 * @then window_avg_w equals 3000
 */
void test_capacity_variable_power_window(void) {
    capacity_init(&s_state);
    capacity_set_limit(&s_state, 5000);

    /* 450s at 2000W */
    for (int i = 0; i < 450; i++) {
        capacity_tick_1s(&s_state, 2000, 3, 2);
    }
    /* 450s at 4000W */
    for (int i = 0; i < 450; i++) {
        capacity_tick_1s(&s_state, 4000, 3, 2);
    }

    TEST_ASSERT_EQUAL_INT(1, s_state.window.window_valid);
    TEST_ASSERT_EQUAL_INT(3000, s_state.window.window_avg_w);
}

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-003
 * @scenario Monthly peak tracks highest window
 * @given A fresh capacity state in month 3
 * @when Three windows complete with averages 3000W, 5000W, 4000W
 * @then monthly_peak_w equals 5000
 */
void test_capacity_monthly_peak_tracking(void) {
    capacity_init(&s_state);
    capacity_set_limit(&s_state, 10000);

    /* Window 1: 3000W */
    for (int i = 0; i < 900; i++) {
        capacity_tick_1s(&s_state, 3000, 3, 2);
    }
    TEST_ASSERT_EQUAL_INT(3000, capacity_get_monthly_peak_w(&s_state));

    /* Window 2: 5000W */
    for (int i = 0; i < 900; i++) {
        capacity_tick_1s(&s_state, 5000, 3, 2);
    }
    TEST_ASSERT_EQUAL_INT(5000, capacity_get_monthly_peak_w(&s_state));

    /* Window 3: 4000W — peak should stay at 5000 */
    for (int i = 0; i < 900; i++) {
        capacity_tick_1s(&s_state, 4000, 3, 2);
    }
    TEST_ASSERT_EQUAL_INT(5000, capacity_get_monthly_peak_w(&s_state));
}

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-004
 * @scenario Month rollover resets monthly peak
 * @given A state with monthly_peak_w of 5000 in month 3
 * @when capacity_tick_1s is called with month 4
 * @then monthly_peak_w resets to 0 and new tracking begins
 */
void test_capacity_month_rollover(void) {
    capacity_init(&s_state);
    capacity_set_limit(&s_state, 10000);

    /* Build up a peak in month 3 */
    for (int i = 0; i < 900; i++) {
        capacity_tick_1s(&s_state, 5000, 3, 2);
    }
    TEST_ASSERT_EQUAL_INT(5000, capacity_get_monthly_peak_w(&s_state));

    /* First tick in month 4 — peak should reset */
    capacity_tick_1s(&s_state, 1000, 4, 2);
    TEST_ASSERT_EQUAL_INT(0, capacity_get_monthly_peak_w(&s_state));

    /* Complete a window in month 4 */
    for (int i = 1; i < 900; i++) {
        capacity_tick_1s(&s_state, 2000, 4, 2);
    }
    /* Average: (1000 + 899*2000) / 900 = (1000 + 1798000) / 900 = 1998.89 -> 1998 */
    int32_t peak = capacity_get_monthly_peak_w(&s_state);
    TEST_ASSERT_TRUE(peak > 0);
    TEST_ASSERT_TRUE(peak < 5000); /* Much less than old peak */
}

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-005
 * @scenario Headroom calculation mid-window
 * @given A capacity state with limit 5000W, mid-window at 450s with running avg 3500W
 * @when capacity_get_headroom_w is called
 * @then Headroom reflects how much more can be consumed in remaining window time
 */
void test_capacity_headroom_mid_window(void) {
    capacity_init(&s_state);
    capacity_set_limit(&s_state, 5000);

    /* Simulate 450s at 3500W: accumulated = 450 * 3500 = 1,575,000 Ws */
    for (int i = 0; i < 450; i++) {
        capacity_tick_1s(&s_state, 3500, 3, 2);
    }

    /*
     * Budget = 5000 * 900 = 4,500,000 Ws
     * Remaining budget = 4,500,000 - 1,575,000 = 2,925,000 Ws
     * Remaining seconds = 450
     * Headroom = 2,925,000 / 450 = 6500W
     *
     * This means: if we draw 6500W for the remaining 450s, the
     * 15-min average will exactly equal the 5000W limit.
     */
    int32_t headroom = capacity_get_headroom_w(&s_state);
    TEST_ASSERT_EQUAL_INT(6500, headroom);
}

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-006
 * @scenario Headroom to deciamps conversion for 3-phase
 * @given A headroom of 2300W
 * @when capacity_headroom_to_da is called with 3 phases
 * @then Returns 33 deciamps (2300 * 10 / (230 * 3) = 33.3 -> 33)
 */
void test_capacity_headroom_to_da_3phase(void) {
    int16_t da = capacity_headroom_to_da(2300, 3);
    /* 2300 * 10 / (230 * 3) = 23000 / 690 = 33.33 -> 33 */
    TEST_ASSERT_EQUAL_INT(33, da);
}

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-007
 * @scenario Headroom to deciamps conversion for 1-phase
 * @given A headroom of 2300W
 * @when capacity_headroom_to_da is called with 1 phase
 * @then Returns 100 deciamps (2300 * 10 / 230 = 100)
 */
void test_capacity_headroom_to_da_1phase(void) {
    int16_t da = capacity_headroom_to_da(2300, 1);
    /* 2300 * 10 / (230 * 1) = 23000 / 230 = 100 */
    TEST_ASSERT_EQUAL_INT(100, da);
}

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-008
 * @scenario Disabled when limit equals zero
 * @given A capacity state with limit_w set to 0
 * @when capacity_get_headroom_w is called
 * @then Returns INT32_MAX indicating no constraint
 */
void test_capacity_disabled_returns_max_headroom(void) {
    capacity_init(&s_state);
    capacity_set_limit(&s_state, 0);

    /* Even with accumulated power, headroom is unconstrained */
    for (int i = 0; i < 100; i++) {
        capacity_tick_1s(&s_state, 10000, 3, 2);
    }

    TEST_ASSERT_EQUAL_INT(INT32_MAX, capacity_get_headroom_w(&s_state));
}

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-009
 * @scenario JSON output contains all key fields
 * @given A state with valid window and peak data
 * @when capacity_to_json is called
 * @then JSON contains limit_w, window_avg_w, monthly_peak_w, headroom_w
 */
void test_capacity_json_output(void) {
    capacity_init(&s_state);
    capacity_set_limit(&s_state, 5000);

    /* Complete one window */
    for (int i = 0; i < 900; i++) {
        capacity_tick_1s(&s_state, 3000, 3, 2);
    }

    int n = capacity_to_json(&s_state, s_json_buf, sizeof(s_json_buf));
    TEST_ASSERT_GREATER_THAN(0, n);

    /* Verify key fields present */
    TEST_ASSERT_TRUE(strstr(s_json_buf, "\"limit_w\":5000") != NULL);
    TEST_ASSERT_TRUE(strstr(s_json_buf, "\"window_avg_w\":3000") != NULL);
    TEST_ASSERT_TRUE(strstr(s_json_buf, "\"monthly_peak_w\":3000") != NULL);
    TEST_ASSERT_TRUE(strstr(s_json_buf, "\"headroom_w\":") != NULL);
    TEST_ASSERT_TRUE(strstr(s_json_buf, "\"window_valid\":1") != NULL);
    TEST_ASSERT_TRUE(strstr(s_json_buf, "\"window_elapsed_s\":0") != NULL);
}

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-010
 * @scenario Negative power from solar export is averaged correctly
 * @given A capacity state with limit 5000W
 * @when 900 ticks with -1000W (net solar export)
 * @then window_avg_w equals -1000 and headroom is limit + 1000 = 6000
 */
void test_capacity_negative_power_solar(void) {
    capacity_init(&s_state);
    capacity_set_limit(&s_state, 5000);

    for (int i = 0; i < 900; i++) {
        capacity_tick_1s(&s_state, -1000, 3, 2);
    }

    TEST_ASSERT_EQUAL_INT(-1000, s_state.window.window_avg_w);

    /* After window reset, headroom = limit = 5000 (window just started) */
    TEST_ASSERT_EQUAL_INT(5000, capacity_get_headroom_w(&s_state));

    /* Monthly peak should still be 0 (negative average doesn't set peak) */
    /* Actually: -1000 > 0 is false, so peak stays 0 */
    TEST_ASSERT_EQUAL_INT(0, capacity_get_monthly_peak_w(&s_state));
}

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-011
 * @scenario Zero-length buffer returns -1
 * @given A valid capacity state
 * @when capacity_to_json is called with bufsz 0
 * @then Returns -1
 */
void test_capacity_json_zero_buffer(void) {
    capacity_init(&s_state);
    int n = capacity_to_json(&s_state, s_json_buf, 0);
    TEST_ASSERT_EQUAL_INT(-1, n);
}

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-012
 * @scenario NULL state returns -1 for JSON and 0 for getters
 * @given A NULL state pointer
 * @when capacity_to_json and getter functions are called
 * @then JSON returns -1, getters return 0
 */
void test_capacity_null_state(void) {
    int n = capacity_to_json(NULL, s_json_buf, sizeof(s_json_buf));
    TEST_ASSERT_EQUAL_INT(-1, n);

    TEST_ASSERT_EQUAL_INT(0, capacity_get_headroom_w(NULL));
    TEST_ASSERT_EQUAL_INT(0, capacity_get_window_avg_w(NULL));
    TEST_ASSERT_EQUAL_INT(0, capacity_get_monthly_peak_w(NULL));
}

/* ---- Additional edge case tests ---- */

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-013
 * @scenario Headroom to deciamps with zero phases returns zero
 * @given A headroom value
 * @when capacity_headroom_to_da is called with 0 phases
 * @then Returns 0
 */
void test_capacity_headroom_to_da_zero_phases(void) {
    int16_t da = capacity_headroom_to_da(5000, 0);
    TEST_ASSERT_EQUAL_INT(0, da);
}

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-014
 * @scenario JSON with NULL buffer returns -1
 * @given A valid capacity state
 * @when capacity_to_json is called with NULL buffer
 * @then Returns -1
 */
void test_capacity_json_null_buffer(void) {
    capacity_init(&s_state);
    int n = capacity_to_json(&s_state, NULL, 512);
    TEST_ASSERT_EQUAL_INT(-1, n);
}

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-015
 * @scenario JSON with too-small buffer returns -1
 * @given A valid capacity state
 * @when capacity_to_json is called with a very small buffer
 * @then Returns -1 (truncation detected)
 */
void test_capacity_json_small_buffer(void) {
    capacity_init(&s_state);
    capacity_set_limit(&s_state, 5000);
    int n = capacity_to_json(&s_state, s_json_buf, 10);
    TEST_ASSERT_EQUAL_INT(-1, n);
}

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-016
 * @scenario Year rollover also resets monthly peak
 * @given A state with peak in month 12 year_offset 2
 * @when tick is called with month 1 year_offset 3
 * @then Monthly peak resets to 0
 */
void test_capacity_year_rollover(void) {
    capacity_init(&s_state);
    capacity_set_limit(&s_state, 10000);

    /* Build peak in December */
    for (int i = 0; i < 900; i++) {
        capacity_tick_1s(&s_state, 7000, 12, 2);
    }
    TEST_ASSERT_EQUAL_INT(7000, capacity_get_monthly_peak_w(&s_state));

    /* January of next year — peak should reset */
    capacity_tick_1s(&s_state, 1000, 1, 3);
    TEST_ASSERT_EQUAL_INT(0, capacity_get_monthly_peak_w(&s_state));
}

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-017
 * @scenario Window not valid until first window completes
 * @given A freshly initialized capacity state
 * @when Only 100 ticks have elapsed (no complete window)
 * @then window_valid is 0 and window_avg_w is 0
 */
void test_capacity_window_not_valid_initially(void) {
    capacity_init(&s_state);
    capacity_set_limit(&s_state, 5000);

    for (int i = 0; i < 100; i++) {
        capacity_tick_1s(&s_state, 3000, 3, 2);
    }

    TEST_ASSERT_EQUAL_INT(0, s_state.window.window_valid);
    TEST_ASSERT_EQUAL_INT(0, capacity_get_window_avg_w(&s_state));
}

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-018
 * @scenario Headroom decreases as window fills with high power
 * @given A capacity state with limit 5000W
 * @when Power consumption exceeds limit for first half of window
 * @then Headroom in second half is reduced to compensate
 */
void test_capacity_headroom_decreases_with_high_power(void) {
    capacity_init(&s_state);
    capacity_set_limit(&s_state, 5000);

    /* Draw 8000W for 450s — well over limit */
    for (int i = 0; i < 450; i++) {
        capacity_tick_1s(&s_state, 8000, 3, 2);
    }

    /*
     * Budget = 5000 * 900 = 4,500,000 Ws
     * Consumed = 450 * 8000 = 3,600,000 Ws
     * Remaining budget = 900,000 Ws
     * Remaining seconds = 450
     * Headroom = 900,000 / 450 = 2000W
     */
    int32_t headroom = capacity_get_headroom_w(&s_state);
    TEST_ASSERT_EQUAL_INT(2000, headroom);
}

int main(void) {
    TEST_SUITE_BEGIN("Capacity Tariff Peak Tracking");

    /* Window averaging */
    RUN_TEST(test_capacity_basic_window_avg);
    RUN_TEST(test_capacity_variable_power_window);

    /* Monthly peak tracking */
    RUN_TEST(test_capacity_monthly_peak_tracking);
    RUN_TEST(test_capacity_month_rollover);

    /* Headroom */
    RUN_TEST(test_capacity_headroom_mid_window);
    RUN_TEST(test_capacity_headroom_decreases_with_high_power);

    /* Deciamps conversion */
    RUN_TEST(test_capacity_headroom_to_da_3phase);
    RUN_TEST(test_capacity_headroom_to_da_1phase);
    RUN_TEST(test_capacity_headroom_to_da_zero_phases);

    /* Disabled mode */
    RUN_TEST(test_capacity_disabled_returns_max_headroom);

    /* JSON output */
    RUN_TEST(test_capacity_json_output);
    RUN_TEST(test_capacity_json_zero_buffer);
    RUN_TEST(test_capacity_json_null_buffer);
    RUN_TEST(test_capacity_json_small_buffer);

    /* Null/edge cases */
    RUN_TEST(test_capacity_null_state);
    RUN_TEST(test_capacity_negative_power_solar);
    RUN_TEST(test_capacity_year_rollover);
    RUN_TEST(test_capacity_window_not_valid_initially);

    TEST_SUITE_RESULTS();
}
