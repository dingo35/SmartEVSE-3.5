/*
 * test_serial_parser.c — Native tests for serial message parsing and
 * current calculation logic extracted from main.cpp.
 */

#include "test_framework.h"
#include "serial_parser.h"
#include <string.h>

/* Constants matching firmware values */
#define EM_API        9
#define MODE_SOLAR    2
#define MODE_NORMAL   0
#define MODE_SMART    1
#define ALWAYS_OFF    1
#define NOT_PRESENT   0
#define ALWAYS_ON     3

/* ================================================================
 * Irms Parsing Tests
 * ================================================================ */

/*
 * @feature Serial Message Parsing
 * @req REQ-SERIAL-001
 * @scenario Valid three-phase Irms message with address 011
 * @given A serial buffer containing "Irms:011,312,123,124"
 * @when serial_parse_irms is called
 * @then Address is 11 and three phase currents are parsed correctly
 */
void test_irms_valid_three_phase(void) {
    serial_irms_t out;
    TEST_ASSERT_TRUE(serial_parse_irms("Irms:011,312,123,124", &out));
    TEST_ASSERT_EQUAL_INT(11, out.address);
    TEST_ASSERT_EQUAL_INT(312, out.irms[0]);
    TEST_ASSERT_EQUAL_INT(123, out.irms[1]);
    TEST_ASSERT_EQUAL_INT(124, out.irms[2]);
}

/*
 * @feature Serial Message Parsing
 * @req REQ-SERIAL-001
 * @scenario Irms message with negative current values
 * @given A serial buffer with negative Irms values (solar injection)
 * @when serial_parse_irms is called
 * @then Negative values are parsed correctly
 */
void test_irms_negative_values(void) {
    serial_irms_t out;
    TEST_ASSERT_TRUE(serial_parse_irms("Irms:010,-150,-200,-100", &out));
    TEST_ASSERT_EQUAL_INT(10, out.address);
    TEST_ASSERT_EQUAL_INT(-150, out.irms[0]);
    TEST_ASSERT_EQUAL_INT(-200, out.irms[1]);
    TEST_ASSERT_EQUAL_INT(-100, out.irms[2]);
}

/*
 * @feature Serial Message Parsing
 * @req REQ-SERIAL-001
 * @scenario Irms message with zero values
 * @given A serial buffer with all zero Irms values
 * @when serial_parse_irms is called
 * @then All values parsed as zero
 */
void test_irms_zero_values(void) {
    serial_irms_t out;
    TEST_ASSERT_TRUE(serial_parse_irms("Irms:001,0,0,0", &out));
    TEST_ASSERT_EQUAL_INT(1, out.address);
    TEST_ASSERT_EQUAL_INT(0, out.irms[0]);
    TEST_ASSERT_EQUAL_INT(0, out.irms[1]);
    TEST_ASSERT_EQUAL_INT(0, out.irms[2]);
}

/*
 * @feature Serial Message Parsing
 * @req REQ-SERIAL-001
 * @scenario Irms message embedded in larger buffer with extra text
 * @given A serial buffer with text before and after the Irms token
 * @when serial_parse_irms is called
 * @then The Irms message is found and parsed correctly
 */
void test_irms_embedded_in_buffer(void) {
    serial_irms_t out;
    TEST_ASSERT_TRUE(serial_parse_irms("some prefix Irms:005,100,200,300 trailing", &out));
    TEST_ASSERT_EQUAL_INT(5, out.address);
    TEST_ASSERT_EQUAL_INT(100, out.irms[0]);
    TEST_ASSERT_EQUAL_INT(200, out.irms[1]);
    TEST_ASSERT_EQUAL_INT(300, out.irms[2]);
}

/*
 * @feature Serial Input Validation
 * @req REQ-SERIAL-004
 * @scenario Irms message with missing fields returns false
 * @given A serial buffer with only 2 of 4 expected Irms fields
 * @when serial_parse_irms is called
 * @then Returns false
 */
void test_irms_missing_fields(void) {
    serial_irms_t out;
    TEST_ASSERT_FALSE(serial_parse_irms("Irms:011,312", &out));
}

/*
 * @feature Serial Input Validation
 * @req REQ-SERIAL-004
 * @scenario Irms token not found in buffer
 * @given A serial buffer without the Irms token
 * @when serial_parse_irms is called
 * @then Returns false
 */
void test_irms_token_not_found(void) {
    serial_irms_t out;
    TEST_ASSERT_FALSE(serial_parse_irms("PowerMeasured:011,500", &out));
}

/*
 * @feature Serial Input Validation
 * @req REQ-SERIAL-004
 * @scenario NULL buffer passed to Irms parser
 * @given A NULL buffer pointer
 * @when serial_parse_irms is called
 * @then Returns false without crashing
 */
void test_irms_null_buffer(void) {
    serial_irms_t out;
    TEST_ASSERT_FALSE(serial_parse_irms(NULL, &out));
}

/*
 * @feature Serial Input Validation
 * @req REQ-SERIAL-004
 * @scenario Empty buffer passed to Irms parser
 * @given An empty string buffer
 * @when serial_parse_irms is called
 * @then Returns false
 */
void test_irms_empty_buffer(void) {
    serial_irms_t out;
    TEST_ASSERT_FALSE(serial_parse_irms("", &out));
}

/* ================================================================
 * PowerMeasured Parsing Tests
 * ================================================================ */

/*
 * @feature Serial Message Parsing
 * @req REQ-SERIAL-002
 * @scenario Valid power measurement with address 010
 * @given A serial buffer containing "PowerMeasured:010,500"
 * @when serial_parse_power is called
 * @then Address is 10 and power is 500
 */
void test_power_valid(void) {
    serial_power_t out;
    TEST_ASSERT_TRUE(serial_parse_power("PowerMeasured:010,500", &out));
    TEST_ASSERT_EQUAL_INT(10, out.address);
    TEST_ASSERT_EQUAL_INT(500, out.power);
}

/*
 * @feature Serial Message Parsing
 * @req REQ-SERIAL-002
 * @scenario Power measurement with negative value (export)
 * @given A serial buffer with negative power value
 * @when serial_parse_power is called
 * @then Negative power is parsed correctly
 */
void test_power_negative(void) {
    serial_power_t out;
    TEST_ASSERT_TRUE(serial_parse_power("PowerMeasured:011,-1500", &out));
    TEST_ASSERT_EQUAL_INT(11, out.address);
    TEST_ASSERT_EQUAL_INT(-1500, out.power);
}

/*
 * @feature Serial Input Validation
 * @req REQ-SERIAL-004
 * @scenario Power message with missing field returns false
 * @given A serial buffer with only the address, no power value
 * @when serial_parse_power is called
 * @then Returns false
 */
void test_power_missing_field(void) {
    serial_power_t out;
    TEST_ASSERT_FALSE(serial_parse_power("PowerMeasured:011", &out));
}

/*
 * @feature Serial Input Validation
 * @req REQ-SERIAL-004
 * @scenario Power token not found in buffer
 * @given A serial buffer without the PowerMeasured token
 * @when serial_parse_power is called
 * @then Returns false
 */
void test_power_token_not_found(void) {
    serial_power_t out;
    TEST_ASSERT_FALSE(serial_parse_power("Irms:011,100,200,300", &out));
}

/*
 * @feature Serial Input Validation
 * @req REQ-SERIAL-004
 * @scenario NULL buffer passed to power parser
 * @given A NULL buffer pointer
 * @when serial_parse_power is called
 * @then Returns false without crashing
 */
void test_power_null_buffer(void) {
    serial_power_t out;
    TEST_ASSERT_FALSE(serial_parse_power(NULL, &out));
}

/* ================================================================
 * Node Status Parsing Tests
 * ================================================================ */

/*
 * @feature Serial Message Parsing
 * @req REQ-SERIAL-003
 * @scenario Valid 16-byte node status with state B and no errors
 * @given A 16-byte buffer with state=1 (B), error=0, mode=0 (Normal)
 * @when serial_parse_node_status is called
 * @then All fields parsed correctly
 */
void test_node_status_valid(void) {
    uint8_t buf[16] = {0};
    buf[1] = 1;    /* STATE_B */
    buf[3] = 0;    /* No error */
    buf[7] = 0;    /* MODE_NORMAL */
    buf[8] = 0;    /* Solar timer high byte */
    buf[9] = 120;  /* Solar timer low byte = 120 */
    buf[13] = 0;   /* Config not changed */
    buf[15] = 32;  /* Max current = 32 * 10 = 320 (32A) */

    serial_node_status_t out;
    TEST_ASSERT_TRUE(serial_parse_node_status(buf, 16, &out));
    TEST_ASSERT_EQUAL_INT(1, out.state);
    TEST_ASSERT_EQUAL_INT(0, out.error);
    TEST_ASSERT_EQUAL_INT(0, out.mode);
    TEST_ASSERT_EQUAL_INT(120, out.solar_timer);
    TEST_ASSERT_EQUAL_INT(0, out.config_changed);
    TEST_ASSERT_EQUAL_INT(320, out.max_current);
}

/*
 * @feature Serial Message Parsing
 * @req REQ-SERIAL-003
 * @scenario Node status with error flags and solar timer
 * @given A buffer with RCM_TRIPPED error and large solar timer
 * @when serial_parse_node_status is called
 * @then Error and solar timer are parsed correctly
 */
void test_node_status_error_and_timer(void) {
    uint8_t buf[16] = {0};
    buf[1] = 2;    /* STATE_C */
    buf[3] = 16;   /* RCM_TRIPPED */
    buf[7] = 2;    /* MODE_SOLAR */
    buf[8] = 1;    /* Solar timer high byte */
    buf[9] = 44;   /* Solar timer = 256 + 44 = 300 */
    buf[13] = 1;   /* Config changed */
    buf[15] = 16;  /* Max current = 160 (16A) */

    serial_node_status_t out;
    TEST_ASSERT_TRUE(serial_parse_node_status(buf, 16, &out));
    TEST_ASSERT_EQUAL_INT(2, out.state);
    TEST_ASSERT_EQUAL_INT(16, out.error);
    TEST_ASSERT_EQUAL_INT(2, out.mode);
    TEST_ASSERT_EQUAL_INT(300, out.solar_timer);
    TEST_ASSERT_EQUAL_INT(1, out.config_changed);
    TEST_ASSERT_EQUAL_INT(160, out.max_current);
}

/*
 * @feature Serial Message Parsing
 * @req REQ-SERIAL-003
 * @scenario Node status with mode Smart and max current boundary
 * @given A buffer with mode=1 (Smart) and max current 255 (max byte value)
 * @when serial_parse_node_status is called
 * @then Max current is 255 * 10 = 2550
 */
void test_node_status_max_current_boundary(void) {
    uint8_t buf[16] = {0};
    buf[1] = 0;    /* STATE_A */
    buf[7] = 1;    /* MODE_SMART */
    buf[15] = 255; /* Max current = 2550 */

    serial_node_status_t out;
    TEST_ASSERT_TRUE(serial_parse_node_status(buf, 16, &out));
    TEST_ASSERT_EQUAL_INT(0, out.state);
    TEST_ASSERT_EQUAL_INT(1, out.mode);
    TEST_ASSERT_EQUAL_INT(2550, out.max_current);
}

/*
 * @feature Serial Input Validation
 * @req REQ-SERIAL-004
 * @scenario Node status buffer too short
 * @given A buffer shorter than 16 bytes
 * @when serial_parse_node_status is called
 * @then Returns false
 */
void test_node_status_buffer_too_short(void) {
    uint8_t buf[10] = {0};
    serial_node_status_t out;
    TEST_ASSERT_FALSE(serial_parse_node_status(buf, 10, &out));
}

/*
 * @feature Serial Input Validation
 * @req REQ-SERIAL-004
 * @scenario NULL buffer passed to node status parser
 * @given A NULL buffer pointer
 * @when serial_parse_node_status is called
 * @then Returns false without crashing
 */
void test_node_status_null_buffer(void) {
    serial_node_status_t out;
    TEST_ASSERT_FALSE(serial_parse_node_status(NULL, 16, &out));
}

/*
 * @feature Serial Input Validation
 * @req REQ-SERIAL-004
 * @scenario NULL output passed to node status parser
 * @given A valid buffer but NULL output pointer
 * @when serial_parse_node_status is called
 * @then Returns false without crashing
 */
void test_node_status_null_output(void) {
    uint8_t buf[16] = {0};
    TEST_ASSERT_FALSE(serial_parse_node_status(buf, 16, NULL));
}

/* ================================================================
 * Battery Current Calculation Tests
 * ================================================================ */

/*
 * @feature Battery Current Calculation
 * @req REQ-CALC-001
 * @scenario Fresh battery data in solar mode
 * @given Battery update 30s ago, solar mode, current = 1000
 * @when calc_battery_current is called
 * @then Returns 1000 (battery current value)
 */
void test_battery_current_fresh_solar_api(void) {
    int16_t result = calc_battery_current(30, MODE_SOLAR, EM_API, 1000);
    TEST_ASSERT_EQUAL_INT(1000, result);
}

/*
 * @feature Battery Current Calculation
 * @req REQ-CALC-001
 * @scenario Stale battery data is ignored after 60 seconds
 * @given Battery update 61s ago
 * @when calc_battery_current is called
 * @then Returns 0 (stale data ignored)
 */
void test_battery_current_stale_data(void) {
    int16_t result = calc_battery_current(61, MODE_SOLAR, EM_API, 1000);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/*
 * @feature Battery Current Calculation
 * @req REQ-CALC-001
 * @scenario Battery data exactly at 60 second boundary
 * @given Battery update exactly 60s ago
 * @when calc_battery_current is called
 * @then Returns battery current (60s is not stale)
 */
void test_battery_current_boundary_60s(void) {
    int16_t result = calc_battery_current(60, MODE_SOLAR, EM_API, 500);
    TEST_ASSERT_EQUAL_INT(500, result);
}

/*
 * @feature Battery Current Calculation
 * @req REQ-CALC-001
 * @scenario Non-solar mode returns zero
 * @given Normal mode with fresh battery data
 * @when calc_battery_current is called
 * @then Returns 0 (battery only used in solar mode)
 */
void test_battery_current_normal_mode(void) {
    int16_t result = calc_battery_current(10, MODE_NORMAL, EM_API, 1000);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/*
 * @feature Battery Current Calculation
 * @req REQ-CALC-001
 * @scenario Non-API meter in solar mode still returns battery current
 * @given Solar mode with non-API meter type
 * @when calc_battery_current is called
 * @then Returns battery current (battery used with any meter in solar mode)
 */
void test_battery_current_non_api_meter(void) {
    int16_t result = calc_battery_current(10, MODE_SOLAR, 1, 1000);
    TEST_ASSERT_EQUAL_INT(1000, result);
}

/*
 * @feature Battery Current Calculation
 * @req REQ-CALC-001
 * @scenario Never-updated battery returns zero
 * @given time_since_update is 0 (never updated)
 * @when calc_battery_current is called
 * @then Returns 0
 */
void test_battery_current_never_updated(void) {
    int16_t result = calc_battery_current(0, MODE_SOLAR, EM_API, 1000);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/*
 * @feature Battery Current Calculation
 * @req REQ-CALC-001
 * @scenario Negative battery current (discharging) in solar mode
 * @given Battery discharging with negative current value
 * @when calc_battery_current is called
 * @then Returns the negative value
 */
void test_battery_current_negative_discharge(void) {
    int16_t result = calc_battery_current(5, MODE_SOLAR, EM_API, -500);
    TEST_ASSERT_EQUAL_INT(-500, result);
}

/* ================================================================
 * CalcIsum Tests
 * ================================================================ */

/*
 * @feature Current Sum Calculation
 * @req REQ-CALC-002
 * @scenario Three-phase system distributes battery current equally
 * @given Three-phase mains at 100,200,300 dA and battery current 300 dA
 * @when calc_isum is called with enable_c2 = NOT_PRESENT
 * @then Battery current divided by 3 (100) subtracted from each phase, Isum = 300
 */
void test_isum_three_phase_battery(void) {
    calc_isum_input_t input = {
        .mains_irms = {100, 200, 300},
        .battery_current = 300,
        .enable_c2 = NOT_PRESENT
    };
    calc_isum_result_t result = calc_isum(&input);
    TEST_ASSERT_EQUAL_INT(0, result.adjusted_irms[0]);    /* 100 - 100 */
    TEST_ASSERT_EQUAL_INT(100, result.adjusted_irms[1]);  /* 200 - 100 */
    TEST_ASSERT_EQUAL_INT(200, result.adjusted_irms[2]);  /* 300 - 100 */
    TEST_ASSERT_EQUAL_INT(300, result.isum);              /* 0+100+200 */
}

/*
 * @feature Current Sum Calculation
 * @req REQ-CALC-003
 * @scenario Single-phase battery adjustment uses L1 only
 * @given EnableC2 ALWAYS_OFF, battery current 300 dA
 * @when calc_isum is called
 * @then Full battery current subtracted from L1, L2 and L3 unchanged
 */
void test_isum_single_phase_battery(void) {
    calc_isum_input_t input = {
        .mains_irms = {100, 200, 300},
        .battery_current = 300,
        .enable_c2 = ALWAYS_OFF
    };
    calc_isum_result_t result = calc_isum(&input);
    TEST_ASSERT_EQUAL_INT(-200, result.adjusted_irms[0]); /* 100 - 300 */
    TEST_ASSERT_EQUAL_INT(200, result.adjusted_irms[1]);  /* unchanged */
    TEST_ASSERT_EQUAL_INT(300, result.adjusted_irms[2]);  /* unchanged */
    TEST_ASSERT_EQUAL_INT(300, result.isum);              /* -200+200+300 */
}

/*
 * @feature Current Sum Calculation
 * @req REQ-CALC-002
 * @scenario Zero battery current leaves mains unchanged
 * @given Zero battery current
 * @when calc_isum is called
 * @then Adjusted currents equal original mains currents
 */
void test_isum_zero_battery(void) {
    calc_isum_input_t input = {
        .mains_irms = {100, 200, 300},
        .battery_current = 0,
        .enable_c2 = NOT_PRESENT
    };
    calc_isum_result_t result = calc_isum(&input);
    TEST_ASSERT_EQUAL_INT(100, result.adjusted_irms[0]);
    TEST_ASSERT_EQUAL_INT(200, result.adjusted_irms[1]);
    TEST_ASSERT_EQUAL_INT(300, result.adjusted_irms[2]);
    TEST_ASSERT_EQUAL_INT(600, result.isum);
}

/*
 * @feature Current Sum Calculation
 * @req REQ-CALC-002
 * @scenario Negative mains currents (solar injection) with battery
 * @given Negative mains currents and positive battery current
 * @when calc_isum is called
 * @then Battery adjustment makes values more negative
 */
void test_isum_negative_mains(void) {
    calc_isum_input_t input = {
        .mains_irms = {-500, -400, -300},
        .battery_current = 300,
        .enable_c2 = ALWAYS_ON
    };
    calc_isum_result_t result = calc_isum(&input);
    TEST_ASSERT_EQUAL_INT(-600, result.adjusted_irms[0]); /* -500 - 100 */
    TEST_ASSERT_EQUAL_INT(-500, result.adjusted_irms[1]); /* -400 - 100 */
    TEST_ASSERT_EQUAL_INT(-400, result.adjusted_irms[2]); /* -300 - 100 */
    TEST_ASSERT_EQUAL_INT(-1500, result.isum);
}

/*
 * @feature Current Sum Calculation
 * @req REQ-CALC-002
 * @scenario Battery current not evenly divisible by 3
 * @given Battery current of 100 (100/3 = 33 per phase, integer division)
 * @when calc_isum is called
 * @then Each phase reduced by 33 (truncated integer division)
 */
void test_isum_battery_rounding(void) {
    calc_isum_input_t input = {
        .mains_irms = {200, 200, 200},
        .battery_current = 100,
        .enable_c2 = NOT_PRESENT
    };
    calc_isum_result_t result = calc_isum(&input);
    /* 100/3 = 33 (integer truncation) */
    TEST_ASSERT_EQUAL_INT(167, result.adjusted_irms[0]);
    TEST_ASSERT_EQUAL_INT(167, result.adjusted_irms[1]);
    TEST_ASSERT_EQUAL_INT(167, result.adjusted_irms[2]);
    TEST_ASSERT_EQUAL_INT(501, result.isum);
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    TEST_SUITE_BEGIN("Serial Parser & Current Calculations");

    /* Irms parsing */
    RUN_TEST(test_irms_valid_three_phase);
    RUN_TEST(test_irms_negative_values);
    RUN_TEST(test_irms_zero_values);
    RUN_TEST(test_irms_embedded_in_buffer);
    RUN_TEST(test_irms_missing_fields);
    RUN_TEST(test_irms_token_not_found);
    RUN_TEST(test_irms_null_buffer);
    RUN_TEST(test_irms_empty_buffer);

    /* PowerMeasured parsing */
    RUN_TEST(test_power_valid);
    RUN_TEST(test_power_negative);
    RUN_TEST(test_power_missing_field);
    RUN_TEST(test_power_token_not_found);
    RUN_TEST(test_power_null_buffer);

    /* Node status parsing */
    RUN_TEST(test_node_status_valid);
    RUN_TEST(test_node_status_error_and_timer);
    RUN_TEST(test_node_status_max_current_boundary);
    RUN_TEST(test_node_status_buffer_too_short);
    RUN_TEST(test_node_status_null_buffer);
    RUN_TEST(test_node_status_null_output);

    /* Battery current */
    RUN_TEST(test_battery_current_fresh_solar_api);
    RUN_TEST(test_battery_current_stale_data);
    RUN_TEST(test_battery_current_boundary_60s);
    RUN_TEST(test_battery_current_normal_mode);
    RUN_TEST(test_battery_current_non_api_meter);
    RUN_TEST(test_battery_current_never_updated);
    RUN_TEST(test_battery_current_negative_discharge);

    /* CalcIsum */
    RUN_TEST(test_isum_three_phase_battery);
    RUN_TEST(test_isum_single_phase_battery);
    RUN_TEST(test_isum_zero_battery);
    RUN_TEST(test_isum_negative_mains);
    RUN_TEST(test_isum_battery_rounding);

    TEST_SUITE_RESULTS();
}
