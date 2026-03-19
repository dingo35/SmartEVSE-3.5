/*
 * test_solar_debug_json.c - Tests for solar debug JSON formatter
 */

#include "test_framework.h"
#include "solar_debug_json.h"
#include <string.h>

/*
 * @feature Solar Debug Telemetry
 * @req REQ-SOL-020
 * @scenario Format solar debug snapshot as JSON with all fields
 * @given A solar debug snapshot with known values
 * @when solar_debug_to_json is called with a sufficiently large buffer
 * @then All 14 fields appear in the JSON output with correct values
 */
void test_solar_debug_to_json_all_fields(void)
{
    evse_solar_debug_t snap = {
        .IsetBalanced = -150,
        .IsetBalanced_ema = -120,
        .Idifference = 300,
        .IsumImport = 450,
        .Isum = -80,
        .MainsMeterImeasured = 200,
        .Balanced0 = 160,
        .SolarStopTimer = 30,
        .PhaseSwitchTimer = 10,
        .PhaseSwitchHoldDown = 5,
        .NoCurrent = 2,
        .SettlingTimer = 3,
        .Nr_Of_Phases_Charging = 1,
        .ErrorFlags = 0
    };

    char buf[512];
    int n = solar_debug_to_json(&snap, buf, sizeof(buf));

    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_TRUE(strstr(buf, "\"IsetBalanced\":-150") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"IsetBalanced_ema\":-120") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"Idifference\":300") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"IsumImport\":450") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"Isum\":-80") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"MainsMeterImeasured\":200") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"Balanced0\":160") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"SolarStopTimer\":30") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"PhaseSwitchTimer\":10") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"PhaseSwitchHoldDown\":5") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"NoCurrent\":2") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"SettlingTimer\":3") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"Nr_Of_Phases_Charging\":1") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"ErrorFlags\":0") != NULL);
}

/*
 * @feature Solar Debug Telemetry
 * @req REQ-SOL-020
 * @scenario JSON output starts with { and ends with }
 * @given A solar debug snapshot
 * @when solar_debug_to_json is called
 * @then The output is valid JSON object framing
 */
void test_solar_debug_to_json_valid_framing(void)
{
    evse_solar_debug_t snap = {0};
    char buf[512];
    int n = solar_debug_to_json(&snap, buf, sizeof(buf));

    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL((int)'{', (int)buf[0]);
    TEST_ASSERT_EQUAL((int)'}', (int)buf[n - 1]);
}

/*
 * @feature Solar Debug Telemetry
 * @req REQ-SOL-020
 * @scenario Buffer too small for JSON output
 * @given A solar debug snapshot
 * @when solar_debug_to_json is called with a buffer that is too small
 * @then The function returns -1
 */
void test_solar_debug_to_json_buffer_too_small(void)
{
    evse_solar_debug_t snap = {0};
    char buf[10];
    int n = solar_debug_to_json(&snap, buf, sizeof(buf));

    TEST_ASSERT_EQUAL(-1, n);
}

/*
 * @feature Solar Debug Telemetry
 * @req REQ-SOL-020
 * @scenario Null pointer arguments
 * @given NULL snap or buf pointer
 * @when solar_debug_to_json is called
 * @then The function returns -1
 */
void test_solar_debug_to_json_null_args(void)
{
    evse_solar_debug_t snap = {0};
    char buf[512];

    TEST_ASSERT_EQUAL(-1, solar_debug_to_json(NULL, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(-1, solar_debug_to_json(&snap, NULL, sizeof(buf)));
    TEST_ASSERT_EQUAL(-1, solar_debug_to_json(&snap, buf, 0));
}

/*
 * @feature Solar Debug Telemetry
 * @req REQ-SOL-020
 * @scenario Zero-initialized snapshot produces valid JSON
 * @given A zero-initialized solar debug snapshot
 * @when solar_debug_to_json is called
 * @then All fields are zero in the output
 */
void test_solar_debug_to_json_zeroed(void)
{
    evse_solar_debug_t snap = {0};
    char buf[512];
    int n = solar_debug_to_json(&snap, buf, sizeof(buf));

    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_TRUE(strstr(buf, "\"IsetBalanced\":0") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"ErrorFlags\":0") != NULL);
}

/*
 * @feature Solar Debug Telemetry
 * @req REQ-SOL-020
 * @scenario Negative values are correctly represented
 * @given A snapshot with negative Isum and IsetBalanced
 * @when solar_debug_to_json is called
 * @then Negative values appear with minus sign
 */
void test_solar_debug_to_json_negative_values(void)
{
    evse_solar_debug_t snap = {0};
    snap.IsetBalanced = -999;
    snap.Isum = -320;

    char buf[512];
    int n = solar_debug_to_json(&snap, buf, sizeof(buf));

    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_TRUE(strstr(buf, "\"IsetBalanced\":-999") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"Isum\":-320") != NULL);
}

/*
 * @feature Solar Debug Telemetry
 * @req REQ-SOL-020
 * @scenario Return value matches actual string length
 * @given A solar debug snapshot
 * @when solar_debug_to_json is called
 * @then The return value equals strlen of the output
 */
void test_solar_debug_to_json_return_value_matches_strlen(void)
{
    evse_solar_debug_t snap = {0};
    snap.IsetBalanced = 100;
    snap.Balanced0 = 160;
    snap.Nr_Of_Phases_Charging = 3;

    char buf[512];
    int n = solar_debug_to_json(&snap, buf, sizeof(buf));

    TEST_ASSERT_EQUAL((int)strlen(buf), n);
}

int main(void)
{
    TEST_SUITE_BEGIN("Solar Debug JSON Formatter");
    RUN_TEST(test_solar_debug_to_json_all_fields);
    RUN_TEST(test_solar_debug_to_json_valid_framing);
    RUN_TEST(test_solar_debug_to_json_buffer_too_small);
    RUN_TEST(test_solar_debug_to_json_null_args);
    RUN_TEST(test_solar_debug_to_json_zeroed);
    RUN_TEST(test_solar_debug_to_json_negative_values);
    RUN_TEST(test_solar_debug_to_json_return_value_matches_strlen);
    TEST_SUITE_RESULTS();
}
