/*
 * test_p1_parse.c - HomeWizard P1 meter JSON parsing tests
 *
 * Tests:
 *   - 3-phase JSON response parsing
 *   - 1-phase JSON response (only L1 present)
 *   - Negative power → negative current (feed-in)
 *   - Mixed positive/negative phases
 *   - Missing current keys → phases=0
 *   - Missing power keys → positive currents (default)
 *   - Empty/NULL JSON → invalid
 *   - JSON field extractor edge cases
 *   - Real-world P1 meter response
 */

#include "test_framework.h"
#include "p1_parse.h"

/* ---- JSON field extractor ---- */

/*
 * @feature P1 Meter Parsing
 * @req REQ-MTR-070
 * @scenario JSON number extractor finds integer value
 * @given JSON string containing "active_current_l1_a":12
 * @when p1_json_find_number is called with key "active_current_l1_a"
 * @then Returns 1 and value is 12.0
 */
void test_json_find_integer(void) {
    const char *json = "{\"active_current_l1_a\":12}";
    float val = 0;
    uint8_t found = p1_json_find_number(json, (uint16_t)strlen(json),
                                        "active_current_l1_a", &val);
    TEST_ASSERT_EQUAL_INT(1, found);
    TEST_ASSERT_EQUAL_INT(12, (int)val);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-MTR-071
 * @scenario JSON number extractor finds negative decimal value
 * @given JSON string containing "active_power_l1_w":-2725.5
 * @when p1_json_find_number is called with key "active_power_l1_w"
 * @then Returns 1 and value is approximately -2725.5
 */
void test_json_find_negative_decimal(void) {
    const char *json = "{\"active_power_l1_w\":-2725.5}";
    float val = 0;
    uint8_t found = p1_json_find_number(json, (uint16_t)strlen(json),
                                        "active_power_l1_w", &val);
    TEST_ASSERT_EQUAL_INT(1, found);
    TEST_ASSERT_EQUAL_INT(-2725, (int)val);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-MTR-072
 * @scenario JSON number extractor returns 0 for missing key
 * @given JSON string without the requested key
 * @when p1_json_find_number is called with key "nonexistent_key"
 * @then Returns 0
 */
void test_json_find_missing_key(void) {
    const char *json = "{\"active_current_l1_a\":12}";
    float val = -1;
    uint8_t found = p1_json_find_number(json, (uint16_t)strlen(json),
                                        "nonexistent_key", &val);
    TEST_ASSERT_EQUAL_INT(0, found);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-MTR-073
 * @scenario JSON extractor handles key that is a prefix of another key
 * @given JSON with "active_current_l1_a" and search for "active_current_l1"
 * @when p1_json_find_number is called
 * @then Returns 0 because the key must match exactly (followed by closing quote)
 */
void test_json_find_partial_key_no_match(void) {
    const char *json = "{\"active_current_l1_a\":12}";
    float val = 0;
    uint8_t found = p1_json_find_number(json, (uint16_t)strlen(json),
                                        "active_current_l1", &val);
    TEST_ASSERT_EQUAL_INT(0, found);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-MTR-074
 * @scenario JSON extractor handles whitespace around colon
 * @given JSON with spaces around the colon: "key" : 42
 * @when p1_json_find_number is called
 * @then Returns 1 and value is 42
 */
void test_json_find_whitespace_around_colon(void) {
    const char *json = "{\"mykey\" : 42}";
    float val = 0;
    uint8_t found = p1_json_find_number(json, (uint16_t)strlen(json),
                                        "mykey", &val);
    TEST_ASSERT_EQUAL_INT(1, found);
    TEST_ASSERT_EQUAL_INT(42, (int)val);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-MTR-075
 * @scenario JSON extractor NULL safety
 * @given NULL json, key, or out pointers
 * @when p1_json_find_number is called
 * @then Returns 0 without crashing
 */
void test_json_find_null_safety(void) {
    float val = 0;
    TEST_ASSERT_EQUAL_INT(0, p1_json_find_number(NULL, 10, "key", &val));
    TEST_ASSERT_EQUAL_INT(0, p1_json_find_number("{}", 2, NULL, &val));
    TEST_ASSERT_EQUAL_INT(0, p1_json_find_number("{}", 2, "key", NULL));
    TEST_ASSERT_EQUAL_INT(0, p1_json_find_number("{}", 0, "key", &val));
}

/* ---- Full P1 response parsing ---- */

/*
 * @feature P1 Meter Parsing
 * @req REQ-MTR-076
 * @scenario 3-phase P1 response with positive currents and positive power
 * @given JSON with L1=10.5A/2400W, L2=8.3A/1900W, L3=12.1A/2800W
 * @when p1_parse_response is called
 * @then phases=3, currents=[105, 83, 121] deci-amps (all positive)
 */
void test_parse_3phase_positive(void) {
    const char *json =
        "{\"active_current_l1_a\":10.5,"
        "\"active_current_l2_a\":8.3,"
        "\"active_current_l3_a\":12.1,"
        "\"active_power_l1_w\":2400,"
        "\"active_power_l2_w\":1900,"
        "\"active_power_l3_w\":2800}";
    p1_result_t r = p1_parse_response(json, (uint16_t)strlen(json));

    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(3, r.phases);
    TEST_ASSERT_EQUAL_INT(105, r.current_da[0]);
    TEST_ASSERT_EQUAL_INT(83, r.current_da[1]);
    TEST_ASSERT_EQUAL_INT(121, r.current_da[2]);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-MTR-077
 * @scenario 1-phase P1 response (only L1 present)
 * @given JSON with only L1 current and power fields
 * @when p1_parse_response is called
 * @then phases=1, current_da[0]=114 (11.43A * 10, rounded)
 */
void test_parse_1phase(void) {
    const char *json =
        "{\"active_current_l1_a\":11.43,"
        "\"active_power_l1_w\":2600}";
    p1_result_t r = p1_parse_response(json, (uint16_t)strlen(json));

    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(1, r.phases);
    TEST_ASSERT_EQUAL_INT(114, r.current_da[0]);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-MTR-078
 * @scenario Negative power causes negative current (solar feed-in)
 * @given JSON with L1=-11.43A current and -2725W power (feeding in)
 * @when p1_parse_response is called
 * @then current_da[0] = -114 (negative because power is negative)
 */
void test_parse_feedin_negative_power(void) {
    const char *json =
        "{\"active_current_l1_a\":-11.43,"
        "\"active_power_l1_w\":-2725}";
    p1_result_t r = p1_parse_response(json, (uint16_t)strlen(json));

    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(1, r.phases);
    TEST_ASSERT_EQUAL_INT(-114, r.current_da[0]);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-MTR-079
 * @scenario Mixed phases: L1 consuming, L2 feeding in
 * @given JSON with L1=5A/1150W (consuming) and L2=3A/-690W (feeding in)
 * @when p1_parse_response is called
 * @then current_da[0]=50, current_da[1]=-30
 */
void test_parse_mixed_direction(void) {
    const char *json =
        "{\"active_current_l1_a\":5.0,"
        "\"active_current_l2_a\":3.0,"
        "\"active_power_l1_w\":1150,"
        "\"active_power_l2_w\":-690}";
    p1_result_t r = p1_parse_response(json, (uint16_t)strlen(json));

    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(2, r.phases);
    TEST_ASSERT_EQUAL_INT(50, r.current_da[0]);
    TEST_ASSERT_EQUAL_INT(-30, r.current_da[1]);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-MTR-080
 * @scenario Missing all current keys returns invalid
 * @given JSON with only power fields, no current fields
 * @when p1_parse_response is called
 * @then valid=0, phases=0
 */
void test_parse_no_current_keys(void) {
    const char *json =
        "{\"active_power_l1_w\":1000,"
        "\"active_power_l2_w\":2000}";
    p1_result_t r = p1_parse_response(json, (uint16_t)strlen(json));

    TEST_ASSERT_EQUAL_INT(0, r.valid);
    TEST_ASSERT_EQUAL_INT(0, r.phases);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-MTR-081
 * @scenario Missing power keys defaults to positive current
 * @given JSON with current fields but no power fields
 * @when p1_parse_response is called
 * @then currents are positive (power defaults to 0, which is >= 0)
 */
void test_parse_missing_power_defaults_positive(void) {
    const char *json =
        "{\"active_current_l1_a\":10.0,"
        "\"active_current_l2_a\":8.0,"
        "\"active_current_l3_a\":6.0}";
    p1_result_t r = p1_parse_response(json, (uint16_t)strlen(json));

    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(3, r.phases);
    TEST_ASSERT_EQUAL_INT(100, r.current_da[0]);
    TEST_ASSERT_EQUAL_INT(80, r.current_da[1]);
    TEST_ASSERT_EQUAL_INT(60, r.current_da[2]);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-MTR-082
 * @scenario NULL JSON returns invalid result
 * @given NULL json pointer
 * @when p1_parse_response is called
 * @then valid=0, phases=0
 */
void test_parse_null_json(void) {
    p1_result_t r = p1_parse_response(NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, r.valid);
    TEST_ASSERT_EQUAL_INT(0, r.phases);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-MTR-083
 * @scenario Empty JSON string returns invalid result
 * @given Empty JSON string "{}"
 * @when p1_parse_response is called
 * @then valid=0, phases=0
 */
void test_parse_empty_json(void) {
    const char *json = "{}";
    p1_result_t r = p1_parse_response(json, (uint16_t)strlen(json));
    TEST_ASSERT_EQUAL_INT(0, r.valid);
    TEST_ASSERT_EQUAL_INT(0, r.phases);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-MTR-084
 * @scenario Real-world Kaifa single-phase P1 response
 * @given Actual P1 meter JSON response from a Kaifa meter with solar feed-in
 * @when p1_parse_response is called
 * @then Correctly extracts L1 current as -114 dA (11.43A feed-in)
 */
void test_parse_real_world_kaifa(void) {
    const char *json =
        "{\"wifi_ssid\":\"MyNetwork\",\"wifi_strength\":86,"
        "\"smr_version\":50,\"meter_model\":\"Kaifa AIFA-METER\","
        "\"active_tariff\":1,"
        "\"total_power_import_kwh\":7412.085,"
        "\"total_power_export_kwh\":6551.330,"
        "\"active_power_w\":-2725.000,"
        "\"active_power_l1_w\":-2725.000,"
        "\"active_voltage_l1_v\":238.400,"
        "\"active_current_a\":11.430,"
        "\"active_current_l1_a\":-11.430,"
        "\"voltage_sag_l1_count\":8.000,"
        "\"voltage_swell_l1_count\":0.000}";
    p1_result_t r = p1_parse_response(json, (uint16_t)strlen(json));

    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(1, r.phases);
    TEST_ASSERT_EQUAL_INT(-114, r.current_da[0]);
    TEST_ASSERT_EQUAL_INT(-2725, r.power_w[0]);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-MTR-085
 * @scenario Zero current and zero power
 * @given JSON with all currents and powers at 0
 * @when p1_parse_response is called
 * @then All current_da values are 0
 */
void test_parse_zero_values(void) {
    const char *json =
        "{\"active_current_l1_a\":0,"
        "\"active_current_l2_a\":0,"
        "\"active_current_l3_a\":0,"
        "\"active_power_l1_w\":0,"
        "\"active_power_l2_w\":0,"
        "\"active_power_l3_w\":0}";
    p1_result_t r = p1_parse_response(json, (uint16_t)strlen(json));

    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(3, r.phases);
    TEST_ASSERT_EQUAL_INT(0, r.current_da[0]);
    TEST_ASSERT_EQUAL_INT(0, r.current_da[1]);
    TEST_ASSERT_EQUAL_INT(0, r.current_da[2]);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-MTR-086
 * @scenario Power stores diagnostic values
 * @given JSON with L1=8.5A/1955W, L2=6.2A/1426W
 * @when p1_parse_response is called
 * @then power_w contains [1955, 1426, 0] for diagnostics
 */
void test_parse_power_diagnostics(void) {
    const char *json =
        "{\"active_current_l1_a\":8.5,"
        "\"active_current_l2_a\":6.2,"
        "\"active_power_l1_w\":1955,"
        "\"active_power_l2_w\":1426}";
    p1_result_t r = p1_parse_response(json, (uint16_t)strlen(json));

    TEST_ASSERT_EQUAL_INT(1, r.valid);
    TEST_ASSERT_EQUAL_INT(1955, r.power_w[0]);
    TEST_ASSERT_EQUAL_INT(1426, r.power_w[1]);
}

/* ---- Input validation hardening ---- */

/*
 * @feature P1 Meter Parsing
 * @req REQ-PWR-030
 * @scenario NaN current value is rejected by JSON extractor
 * @given JSON string containing "active_current_l1_a":"NaN"
 * @when p1_json_find_number is called
 * @then Returns 0 (parse failure) because NaN is not a valid number
 */
void test_json_find_nan_rejected(void) {
    const char *json = "{\"active_current_l1_a\":NaN}";
    float val = 0;
    uint8_t found = p1_json_find_number(json, (uint16_t)strlen(json),
                                        "active_current_l1_a", &val);
    TEST_ASSERT_EQUAL_INT(0, found);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-PWR-031
 * @scenario Infinity current value is rejected by JSON extractor
 * @given JSON string containing "active_current_l1_a":Infinity
 * @when p1_json_find_number is called
 * @then Returns 0 (parse failure) because Infinity is not a valid meter reading
 */
void test_json_find_infinity_rejected(void) {
    const char *json = "{\"active_current_l1_a\":Infinity}";
    float val = 0;
    uint8_t found = p1_json_find_number(json, (uint16_t)strlen(json),
                                        "active_current_l1_a", &val);
    TEST_ASSERT_EQUAL_INT(0, found);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-PWR-032
 * @scenario Negative infinity is rejected by JSON extractor
 * @given JSON string containing "active_current_l1_a":-Infinity
 * @when p1_json_find_number is called
 * @then Returns 0 (parse failure)
 */
void test_json_find_neg_infinity_rejected(void) {
    const char *json = "{\"active_current_l1_a\":-Infinity}";
    float val = 0;
    uint8_t found = p1_json_find_number(json, (uint16_t)strlen(json),
                                        "active_current_l1_a", &val);
    TEST_ASSERT_EQUAL_INT(0, found);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-PWR-033
 * @scenario Current value exceeding int16_t range marks result invalid
 * @given JSON with current 4000.0A (40000 dA exceeds INT16_MAX=32767)
 * @when p1_parse_response is called
 * @then Result is invalid because deci-amp value overflows int16_t
 */
void test_parse_current_overflow_invalid(void) {
    const char *json =
        "{\"active_current_l1_a\":4000.0,"
        "\"active_power_l1_w\":920000}";
    p1_result_t r = p1_parse_response(json, (uint16_t)strlen(json));
    TEST_ASSERT_EQUAL_INT(0, r.valid);
}

/*
 * @feature P1 Meter Parsing
 * @req REQ-PWR-034
 * @scenario NaN in full P1 response is rejected
 * @given JSON with NaN value for active_current_l1_a
 * @when p1_parse_response is called
 * @then Result is invalid because NaN cannot be parsed as a number
 */
void test_parse_nan_in_response_rejected(void) {
    const char *json =
        "{\"active_current_l1_a\":NaN,"
        "\"active_power_l1_w\":1000}";
    p1_result_t r = p1_parse_response(json, (uint16_t)strlen(json));
    /* NaN rejected by p1_json_find_number, so phases=0 → invalid */
    TEST_ASSERT_EQUAL_INT(0, r.valid);
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("P1 Meter Parsing");

    RUN_TEST(test_json_find_integer);
    RUN_TEST(test_json_find_negative_decimal);
    RUN_TEST(test_json_find_missing_key);
    RUN_TEST(test_json_find_partial_key_no_match);
    RUN_TEST(test_json_find_whitespace_around_colon);
    RUN_TEST(test_json_find_null_safety);
    RUN_TEST(test_parse_3phase_positive);
    RUN_TEST(test_parse_1phase);
    RUN_TEST(test_parse_feedin_negative_power);
    RUN_TEST(test_parse_mixed_direction);
    RUN_TEST(test_parse_no_current_keys);
    RUN_TEST(test_parse_missing_power_defaults_positive);
    RUN_TEST(test_parse_null_json);
    RUN_TEST(test_parse_empty_json);
    RUN_TEST(test_parse_real_world_kaifa);
    RUN_TEST(test_parse_zero_values);
    RUN_TEST(test_parse_power_diagnostics);

    // Input validation hardening
    RUN_TEST(test_json_find_nan_rejected);
    RUN_TEST(test_json_find_infinity_rejected);
    RUN_TEST(test_json_find_neg_infinity_rejected);
    RUN_TEST(test_parse_current_overflow_invalid);
    RUN_TEST(test_parse_nan_in_response_rejected);

    TEST_SUITE_RESULTS();
}
