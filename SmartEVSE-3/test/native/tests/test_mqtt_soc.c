/*
 * test_mqtt_soc.c — Native tests for MQTT SoC and energy topic parsing
 *
 * Tests the parsing of Set/InitialSoC, Set/FullSoC, Set/EnergyCapacity,
 * Set/EnergyRequest, and Set/EVCCID topics added to mqtt_parser.c.
 */

#include "test_framework.h"
#include "mqtt_parser.h"
#include <string.h>

#define PREFIX "SmartEVSE/123456"

static mqtt_command_t cmd;

// ---- InitialSoC ----

/*
 * @feature MQTT SoC Parsing
 * @req REQ-SOC-001
 * @scenario Parse Set/InitialSoC with valid percentage
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/InitialSoC with payload "80"
 * @then Command type is MQTT_CMD_INITIAL_SOC with initial_soc = 80
 */
void test_initial_soc_valid(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/InitialSoC", "80", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_INITIAL_SOC, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(80, cmd.initial_soc);
}

/*
 * @feature MQTT SoC Parsing
 * @req REQ-SOC-002
 * @scenario Parse Set/InitialSoC with -1 to reset/clear value
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/InitialSoC with payload "-1"
 * @then Command type is MQTT_CMD_INITIAL_SOC with initial_soc = -1
 */
void test_initial_soc_reset(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/InitialSoC", "-1", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_INITIAL_SOC, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(-1, cmd.initial_soc);
}

/*
 * @feature MQTT SoC Input Validation
 * @req REQ-SOC-003
 * @scenario Reject Set/InitialSoC with value above 100
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/InitialSoC with payload "101"
 * @then The parser returns false
 */
void test_initial_soc_above_max(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/InitialSoC", "101", &cmd));
}

/*
 * @feature MQTT SoC Input Validation
 * @req REQ-SOC-004
 * @scenario Reject Set/InitialSoC with value below -1
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/InitialSoC with payload "-2"
 * @then The parser returns false
 */
void test_initial_soc_below_min(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/InitialSoC", "-2", &cmd));
}

/*
 * @feature MQTT SoC Parsing
 * @req REQ-SOC-005
 * @scenario Parse Set/InitialSoC boundary value 0
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/InitialSoC with payload "0"
 * @then Command type is MQTT_CMD_INITIAL_SOC with initial_soc = 0
 */
void test_initial_soc_zero(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/InitialSoC", "0", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_INITIAL_SOC, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(0, cmd.initial_soc);
}

/*
 * @feature MQTT SoC Parsing
 * @req REQ-SOC-006
 * @scenario Parse Set/InitialSoC boundary value 100
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/InitialSoC with payload "100"
 * @then Command type is MQTT_CMD_INITIAL_SOC with initial_soc = 100
 */
void test_initial_soc_max(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/InitialSoC", "100", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_INITIAL_SOC, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(100, cmd.initial_soc);
}

// ---- FullSoC ----

/*
 * @feature MQTT SoC Parsing
 * @req REQ-SOC-007
 * @scenario Parse Set/FullSoC with valid percentage
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/FullSoC with payload "95"
 * @then Command type is MQTT_CMD_FULL_SOC with full_soc = 95
 */
void test_full_soc_valid(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/FullSoC", "95", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_FULL_SOC, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(95, cmd.full_soc);
}

/*
 * @feature MQTT SoC Parsing
 * @req REQ-SOC-008
 * @scenario Parse Set/FullSoC with -1 to reset/clear value
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/FullSoC with payload "-1"
 * @then Command type is MQTT_CMD_FULL_SOC with full_soc = -1
 */
void test_full_soc_reset(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/FullSoC", "-1", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_FULL_SOC, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(-1, cmd.full_soc);
}

/*
 * @feature MQTT SoC Input Validation
 * @req REQ-SOC-009
 * @scenario Reject Set/FullSoC with value above 100
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/FullSoC with payload "101"
 * @then The parser returns false
 */
void test_full_soc_above_max(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/FullSoC", "101", &cmd));
}

// ---- EnergyCapacity ----

/*
 * @feature MQTT SoC Parsing
 * @req REQ-SOC-010
 * @scenario Parse Set/EnergyCapacity with valid Wh value
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/EnergyCapacity with payload "64000"
 * @then Command type is MQTT_CMD_ENERGY_CAPACITY with energy_capacity = 64000
 */
void test_energy_capacity_valid(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/EnergyCapacity", "64000", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_ENERGY_CAPACITY, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(64000, cmd.energy_capacity);
}

/*
 * @feature MQTT SoC Input Validation
 * @req REQ-SOC-011
 * @scenario Reject Set/EnergyCapacity above 200000 Wh
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/EnergyCapacity with payload "200001"
 * @then The parser returns false
 */
void test_energy_capacity_above_max(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/EnergyCapacity", "200001", &cmd));
}

/*
 * @feature MQTT SoC Parsing
 * @req REQ-SOC-012
 * @scenario Parse Set/EnergyCapacity boundary value 200000
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/EnergyCapacity with payload "200000"
 * @then Command type is MQTT_CMD_ENERGY_CAPACITY with energy_capacity = 200000
 */
void test_energy_capacity_boundary_max(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/EnergyCapacity", "200000", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_ENERGY_CAPACITY, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(200000, cmd.energy_capacity);
}

/*
 * @feature MQTT SoC Parsing
 * @req REQ-SOC-013
 * @scenario Parse Set/EnergyCapacity with -1 to reset/clear value
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/EnergyCapacity with payload "-1"
 * @then Command type is MQTT_CMD_ENERGY_CAPACITY with energy_capacity = -1
 */
void test_energy_capacity_reset(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/EnergyCapacity", "-1", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_ENERGY_CAPACITY, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(-1, cmd.energy_capacity);
}

/*
 * @feature MQTT SoC Input Validation
 * @req REQ-SOC-014
 * @scenario Reject Set/EnergyCapacity below -1
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/EnergyCapacity with payload "-2"
 * @then The parser returns false
 */
void test_energy_capacity_below_min(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/EnergyCapacity", "-2", &cmd));
}

// ---- EnergyRequest ----

/*
 * @feature MQTT SoC Parsing
 * @req REQ-SOC-015
 * @scenario Parse Set/EnergyRequest with valid Wh value
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/EnergyRequest with payload "32000"
 * @then Command type is MQTT_CMD_ENERGY_REQUEST with energy_request = 32000
 */
void test_energy_request_valid(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/EnergyRequest", "32000", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_ENERGY_REQUEST, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(32000, cmd.energy_request);
}

/*
 * @feature MQTT SoC Input Validation
 * @req REQ-SOC-016
 * @scenario Reject Set/EnergyRequest above 200000 Wh
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/EnergyRequest with payload "200001"
 * @then The parser returns false
 */
void test_energy_request_above_max(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/EnergyRequest", "200001", &cmd));
}

// ---- EVCCID ----

/*
 * @feature MQTT SoC Parsing
 * @req REQ-SOC-017
 * @scenario Parse Set/EVCCID with valid identifier
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/EVCCID with payload "WBADE12345678901"
 * @then Command type is MQTT_CMD_EVCCID_SET with evccid matching the payload
 */
void test_evccid_set_valid(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/EVCCID", "WBADE12345678901", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_EVCCID_SET, cmd.cmd);
    TEST_ASSERT_TRUE(strcmp(cmd.evccid, "WBADE12345678901") == 0);
}

/*
 * @feature MQTT SoC Input Validation
 * @req REQ-SOC-018
 * @scenario Set/EVCCID truncated at 31 chars (32-byte buffer with NUL)
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/EVCCID with a 32-character payload
 * @then The parser returns false because payload >= sizeof(evccid)
 */
void test_evccid_set_too_long(void) {
    /* 32 chars = too long (no room for NUL in 32-byte buffer) */
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/EVCCID",
        "12345678901234567890123456789012", &cmd));
}

/*
 * @feature MQTT SoC Parsing
 * @req REQ-SOC-019
 * @scenario Set/EVCCID accepts exactly 31 characters
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/EVCCID with a 31-character payload
 * @then The parser returns true with the full string stored
 */
void test_evccid_set_max_length(void) {
    /* 31 chars = fits exactly (31 chars + NUL = 32 bytes) */
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/EVCCID",
        "1234567890123456789012345678901", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_EVCCID_SET, cmd.cmd);
    TEST_ASSERT_TRUE(strcmp(cmd.evccid, "1234567890123456789012345678901") == 0);
}

/*
 * @feature MQTT SoC Parsing
 * @req REQ-SOC-020
 * @scenario Set/EVCCID accepts empty string to clear
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/EVCCID with empty payload ""
 * @then The parser returns true with an empty evccid
 */
void test_evccid_set_empty(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/EVCCID", "", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_EVCCID_SET, cmd.cmd);
    TEST_ASSERT_TRUE(strcmp(cmd.evccid, "") == 0);
}

// ---- Empty payload rejection for numeric SoC topics ----

/*
 * @feature MQTT SoC Input Validation
 * @req REQ-SOC-021
 * @scenario Empty payload is rejected for InitialSoC
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/InitialSoC with empty payload ""
 * @then The parser returns false
 */
void test_initial_soc_empty_payload(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/InitialSoC", "", &cmd));
}

/*
 * @feature MQTT SoC Input Validation
 * @req REQ-SOC-022
 * @scenario Empty payload is rejected for FullSoC
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/FullSoC with empty payload ""
 * @then The parser returns false
 */
void test_full_soc_empty_payload(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/FullSoC", "", &cmd));
}

/*
 * @feature MQTT SoC Input Validation
 * @req REQ-SOC-023
 * @scenario Empty payload is rejected for EnergyCapacity
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/EnergyCapacity with empty payload ""
 * @then The parser returns false
 */
void test_energy_capacity_empty_payload(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/EnergyCapacity", "", &cmd));
}

/*
 * @feature MQTT SoC Input Validation
 * @req REQ-SOC-024
 * @scenario Empty payload is rejected for EnergyRequest
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/EnergyRequest with empty payload ""
 * @then The parser returns false
 */
void test_energy_request_empty_payload(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/EnergyRequest", "", &cmd));
}

// ---- Non-numeric payload rejection ----

/*
 * @feature MQTT SoC Input Validation
 * @req REQ-SOC-025
 * @scenario Non-numeric payload is rejected for InitialSoC
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/InitialSoC with payload "abc"
 * @then The parser returns false
 */
void test_initial_soc_non_numeric(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/InitialSoC", "abc", &cmd));
}

/*
 * @feature MQTT SoC Input Validation
 * @req REQ-SOC-026
 * @scenario Non-numeric payload is rejected for EnergyCapacity
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/EnergyCapacity with payload "abc"
 * @then The parser returns false
 */
void test_energy_capacity_non_numeric(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/EnergyCapacity", "abc", &cmd));
}

int main(void) {
    TEST_SUITE_BEGIN("MQTT SoC Parsing");

    // InitialSoC
    RUN_TEST(test_initial_soc_valid);
    RUN_TEST(test_initial_soc_reset);
    RUN_TEST(test_initial_soc_above_max);
    RUN_TEST(test_initial_soc_below_min);
    RUN_TEST(test_initial_soc_zero);
    RUN_TEST(test_initial_soc_max);

    // FullSoC
    RUN_TEST(test_full_soc_valid);
    RUN_TEST(test_full_soc_reset);
    RUN_TEST(test_full_soc_above_max);

    // EnergyCapacity
    RUN_TEST(test_energy_capacity_valid);
    RUN_TEST(test_energy_capacity_above_max);
    RUN_TEST(test_energy_capacity_boundary_max);
    RUN_TEST(test_energy_capacity_reset);
    RUN_TEST(test_energy_capacity_below_min);

    // EnergyRequest
    RUN_TEST(test_energy_request_valid);
    RUN_TEST(test_energy_request_above_max);

    // EVCCID
    RUN_TEST(test_evccid_set_valid);
    RUN_TEST(test_evccid_set_too_long);
    RUN_TEST(test_evccid_set_max_length);
    RUN_TEST(test_evccid_set_empty);

    // Empty payload rejection
    RUN_TEST(test_initial_soc_empty_payload);
    RUN_TEST(test_full_soc_empty_payload);
    RUN_TEST(test_energy_capacity_empty_payload);
    RUN_TEST(test_energy_request_empty_payload);

    // Non-numeric payload rejection
    RUN_TEST(test_initial_soc_non_numeric);
    RUN_TEST(test_energy_capacity_non_numeric);

    TEST_SUITE_RESULTS();
}
