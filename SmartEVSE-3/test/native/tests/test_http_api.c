/*
 * test_http_api.c — Native tests for HTTP API validation logic
 *
 * Tests the pure validation and parsing logic extracted from
 * handle_URI() into http_api.c.
 */

#include "test_framework.h"
#include "http_api.h"
#include "evse_ctx.h"
#include <string.h>

// ---- Color Parsing ----

/*
 * @feature HTTP API Color Parsing
 * @req REQ-API-001
 * @scenario Valid RGB color values are accepted
 * @given Integer values for R, G, B
 * @when All values are in 0..255
 * @then http_api_parse_color returns true with correct output
 */
void test_color_valid(void) {
    uint8_t r, g, b;
    TEST_ASSERT_TRUE(http_api_parse_color(255, 128, 0, &r, &g, &b));
    TEST_ASSERT_EQUAL_INT(255, r);
    TEST_ASSERT_EQUAL_INT(128, g);
    TEST_ASSERT_EQUAL_INT(0, b);
}

/*
 * @feature HTTP API Color Parsing
 * @req REQ-API-001
 * @scenario Zero RGB values are accepted
 */
void test_color_zero(void) {
    uint8_t r, g, b;
    TEST_ASSERT_TRUE(http_api_parse_color(0, 0, 0, &r, &g, &b));
    TEST_ASSERT_EQUAL_INT(0, r);
}

/*
 * @feature HTTP API Color Parsing
 * @req REQ-API-001
 * @scenario Maximum RGB values are accepted
 */
void test_color_max(void) {
    uint8_t r, g, b;
    TEST_ASSERT_TRUE(http_api_parse_color(255, 255, 255, &r, &g, &b));
    TEST_ASSERT_EQUAL_INT(255, r);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-002
 * @scenario RGB value above 255 is rejected
 */
void test_color_out_of_range(void) {
    uint8_t r, g, b;
    TEST_ASSERT_FALSE(http_api_parse_color(256, 0, 0, &r, &g, &b));
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-002
 * @scenario Negative RGB value is rejected
 */
void test_color_negative(void) {
    uint8_t r, g, b;
    TEST_ASSERT_FALSE(http_api_parse_color(-1, 0, 0, &r, &g, &b));
}

// ---- Override Current Validation ----

/*
 * @feature HTTP API Validation
 * @req REQ-API-003
 * @scenario Override current zero is always valid (disables override)
 */
void test_override_current_zero(void) {
    TEST_ASSERT_TRUE(http_api_validate_override_current(0, 6, 32, 0) == NULL);
}

/*
 * @feature HTTP API Validation
 * @req REQ-API-003
 * @scenario Override current within range is valid
 */
void test_override_current_valid(void) {
    // min=6A, max=32A -> valid range 60..320 dA
    TEST_ASSERT_TRUE(http_api_validate_override_current(160, 6, 32, 0) == NULL);
}

/*
 * @feature HTTP API Validation
 * @req REQ-API-003
 * @scenario Override current at minimum boundary is valid
 */
void test_override_current_at_min(void) {
    TEST_ASSERT_TRUE(http_api_validate_override_current(60, 6, 32, 0) == NULL);
}

/*
 * @feature HTTP API Validation
 * @req REQ-API-003
 * @scenario Override current at maximum boundary is valid
 */
void test_override_current_at_max(void) {
    TEST_ASSERT_TRUE(http_api_validate_override_current(320, 6, 32, 0) == NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-004
 * @scenario Override current below minimum is rejected
 */
void test_override_current_below_min(void) {
    TEST_ASSERT_TRUE(http_api_validate_override_current(50, 6, 32, 0) != NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-004
 * @scenario Override current above maximum is rejected
 */
void test_override_current_above_max(void) {
    TEST_ASSERT_TRUE(http_api_validate_override_current(330, 6, 32, 0) != NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-004
 * @scenario Override current on slave is rejected
 */
void test_override_current_slave(void) {
    TEST_ASSERT_TRUE(http_api_validate_override_current(160, 6, 32, 2) != NULL);
}

// ---- Current Min Validation ----

/*
 * @feature HTTP API Validation
 * @req REQ-API-005
 * @scenario Current min at boundary (6A) is valid
 */
void test_current_min_valid(void) {
    TEST_ASSERT_TRUE(http_api_validate_current_min(6, 0) == NULL);
}

/*
 * @feature HTTP API Validation
 * @req REQ-API-005
 * @scenario Current min at 16A is valid
 */
void test_current_min_max(void) {
    TEST_ASSERT_TRUE(http_api_validate_current_min(16, 0) == NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-005
 * @scenario Current min below 6A is rejected
 */
void test_current_min_too_low(void) {
    TEST_ASSERT_TRUE(http_api_validate_current_min(5, 0) != NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-005
 * @scenario Current min above 16A is rejected
 */
void test_current_min_too_high(void) {
    TEST_ASSERT_TRUE(http_api_validate_current_min(17, 0) != NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-005
 * @scenario Current min on slave is rejected
 */
void test_current_min_slave(void) {
    TEST_ASSERT_TRUE(http_api_validate_current_min(10, 2) != NULL);
}

// ---- Max Sum Mains Validation ----

/*
 * @feature HTTP API Validation
 * @req REQ-API-006
 * @scenario Max sum mains zero disables limit
 */
void test_max_sum_mains_zero(void) {
    TEST_ASSERT_TRUE(http_api_validate_max_sum_mains(0, 0) == NULL);
}

/*
 * @feature HTTP API Validation
 * @req REQ-API-006
 * @scenario Max sum mains at minimum (10A) is valid
 */
void test_max_sum_mains_min(void) {
    TEST_ASSERT_TRUE(http_api_validate_max_sum_mains(10, 0) == NULL);
}

/*
 * @feature HTTP API Validation
 * @req REQ-API-006
 * @scenario Max sum mains at maximum (600A) is valid
 */
void test_max_sum_mains_max(void) {
    TEST_ASSERT_TRUE(http_api_validate_max_sum_mains(600, 0) == NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-006
 * @scenario Max sum mains between 1-9 is rejected
 */
void test_max_sum_mains_gap(void) {
    TEST_ASSERT_TRUE(http_api_validate_max_sum_mains(5, 0) != NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-006
 * @scenario Max sum mains above 600 is rejected
 */
void test_max_sum_mains_too_high(void) {
    TEST_ASSERT_TRUE(http_api_validate_max_sum_mains(601, 0) != NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-006
 * @scenario Max sum mains on slave is rejected
 */
void test_max_sum_mains_slave(void) {
    TEST_ASSERT_TRUE(http_api_validate_max_sum_mains(100, 2) != NULL);
}

// ---- Stop Timer Validation ----

/*
 * @feature HTTP API Validation
 * @req REQ-API-007
 * @scenario Stop timer at zero is valid
 */
void test_stop_timer_zero(void) {
    TEST_ASSERT_TRUE(http_api_validate_stop_timer(0) == NULL);
}

/*
 * @feature HTTP API Validation
 * @req REQ-API-007
 * @scenario Stop timer at max (60) is valid
 */
void test_stop_timer_max(void) {
    TEST_ASSERT_TRUE(http_api_validate_stop_timer(60) == NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-007
 * @scenario Stop timer above 60 is rejected
 */
void test_stop_timer_too_high(void) {
    TEST_ASSERT_TRUE(http_api_validate_stop_timer(61) != NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-007
 * @scenario Negative stop timer is rejected
 */
void test_stop_timer_negative(void) {
    TEST_ASSERT_TRUE(http_api_validate_stop_timer(-1) != NULL);
}

// ---- Solar Start Current Validation ----

/*
 * @feature HTTP API Validation
 * @req REQ-API-008
 * @scenario Solar start current at 0 is valid
 */
void test_solar_start_zero(void) {
    TEST_ASSERT_TRUE(http_api_validate_solar_start(0) == NULL);
}

/*
 * @feature HTTP API Validation
 * @req REQ-API-008
 * @scenario Solar start current at 48 is valid
 */
void test_solar_start_max(void) {
    TEST_ASSERT_TRUE(http_api_validate_solar_start(48) == NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-008
 * @scenario Solar start current above 48 is rejected
 */
void test_solar_start_too_high(void) {
    TEST_ASSERT_TRUE(http_api_validate_solar_start(49) != NULL);
}

// ---- Solar Max Import Validation ----

/*
 * @feature HTTP API Validation
 * @req REQ-API-009
 * @scenario Solar max import at 0 is valid
 */
void test_solar_import_zero(void) {
    TEST_ASSERT_TRUE(http_api_validate_solar_max_import(0) == NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-009
 * @scenario Solar max import above 48 is rejected
 */
void test_solar_import_too_high(void) {
    TEST_ASSERT_TRUE(http_api_validate_solar_max_import(49) != NULL);
}

// ---- PrioStrategy Validation ----

/*
 * @feature HTTP API Validation
 * @req REQ-API-011
 * @scenario PrioStrategy MODBUS_ADDR (0) is valid on master
 * @given A master EVSE (load_bl=0)
 * @when prio_strategy is 0
 * @then Validation passes
 */
void test_prio_strategy_valid_0(void) {
    TEST_ASSERT_TRUE(http_api_validate_prio_strategy(0, 0) == NULL);
}

/*
 * @feature HTTP API Validation
 * @req REQ-API-011
 * @scenario PrioStrategy FIRST_CONNECTED (1) is valid
 */
void test_prio_strategy_valid_1(void) {
    TEST_ASSERT_TRUE(http_api_validate_prio_strategy(1, 1) == NULL);
}

/*
 * @feature HTTP API Validation
 * @req REQ-API-011
 * @scenario PrioStrategy LAST_CONNECTED (2) is valid
 */
void test_prio_strategy_valid_2(void) {
    TEST_ASSERT_TRUE(http_api_validate_prio_strategy(2, 0) == NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-011
 * @scenario PrioStrategy value 3 is rejected
 */
void test_prio_strategy_too_high(void) {
    TEST_ASSERT_TRUE(http_api_validate_prio_strategy(3, 0) != NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-011
 * @scenario PrioStrategy negative value is rejected
 */
void test_prio_strategy_negative(void) {
    TEST_ASSERT_TRUE(http_api_validate_prio_strategy(-1, 0) != NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-011
 * @scenario PrioStrategy on slave is rejected
 * @given A slave EVSE (load_bl=2)
 * @when prio_strategy is 0 (valid value)
 * @then Validation fails because slaves cannot set scheduling
 */
void test_prio_strategy_slave(void) {
    TEST_ASSERT_TRUE(http_api_validate_prio_strategy(0, 2) != NULL);
}

// ---- RotationInterval Validation ----

/*
 * @feature HTTP API Validation
 * @req REQ-API-012
 * @scenario RotationInterval 0 (disabled) is valid
 * @given A master EVSE (load_bl=0)
 * @when rotation_interval is 0
 * @then Validation passes
 */
void test_rotation_interval_zero(void) {
    TEST_ASSERT_TRUE(http_api_validate_rotation_interval(0, 0) == NULL);
}

/*
 * @feature HTTP API Validation
 * @req REQ-API-012
 * @scenario RotationInterval at minimum (30) is valid
 */
void test_rotation_interval_min(void) {
    TEST_ASSERT_TRUE(http_api_validate_rotation_interval(30, 0) == NULL);
}

/*
 * @feature HTTP API Validation
 * @req REQ-API-012
 * @scenario RotationInterval at maximum (1440) is valid
 */
void test_rotation_interval_max(void) {
    TEST_ASSERT_TRUE(http_api_validate_rotation_interval(1440, 0) == NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-012
 * @scenario RotationInterval in gap (1-29) is rejected
 */
void test_rotation_interval_gap(void) {
    TEST_ASSERT_TRUE(http_api_validate_rotation_interval(15, 0) != NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-012
 * @scenario RotationInterval above maximum is rejected
 */
void test_rotation_interval_too_high(void) {
    TEST_ASSERT_TRUE(http_api_validate_rotation_interval(1441, 0) != NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-012
 * @scenario RotationInterval on slave is rejected
 */
void test_rotation_interval_slave(void) {
    TEST_ASSERT_TRUE(http_api_validate_rotation_interval(60, 2) != NULL);
}

// ---- IdleTimeout Validation ----

/*
 * @feature HTTP API Validation
 * @req REQ-API-013
 * @scenario IdleTimeout at minimum (30) is valid
 * @given A master EVSE (load_bl=0)
 * @when idle_timeout is 30
 * @then Validation passes
 */
void test_idle_timeout_min(void) {
    TEST_ASSERT_TRUE(http_api_validate_idle_timeout(30, 0) == NULL);
}

/*
 * @feature HTTP API Validation
 * @req REQ-API-013
 * @scenario IdleTimeout at default (60) is valid
 */
void test_idle_timeout_default(void) {
    TEST_ASSERT_TRUE(http_api_validate_idle_timeout(60, 0) == NULL);
}

/*
 * @feature HTTP API Validation
 * @req REQ-API-013
 * @scenario IdleTimeout at maximum (300) is valid
 */
void test_idle_timeout_max(void) {
    TEST_ASSERT_TRUE(http_api_validate_idle_timeout(300, 0) == NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-013
 * @scenario IdleTimeout below minimum (29) is rejected
 */
void test_idle_timeout_too_low(void) {
    TEST_ASSERT_TRUE(http_api_validate_idle_timeout(29, 0) != NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-013
 * @scenario IdleTimeout above maximum (301) is rejected
 */
void test_idle_timeout_too_high(void) {
    TEST_ASSERT_TRUE(http_api_validate_idle_timeout(301, 0) != NULL);
}

/*
 * @feature HTTP API Input Validation
 * @req REQ-API-013
 * @scenario IdleTimeout on slave is rejected
 */
void test_idle_timeout_slave(void) {
    TEST_ASSERT_TRUE(http_api_validate_idle_timeout(60, 2) != NULL);
}

// ---- Combined Settings Validation ----

/*
 * @feature HTTP API Settings Validation
 * @req REQ-API-010
 * @scenario Valid settings request passes validation
 * @given A settings request with valid current_min and override_current
 * @when Validated against current limits
 * @then No errors are returned
 */
void test_validate_settings_valid(void) {
    http_settings_request_t req;
    memset(&req, 0, sizeof(req));
    req.has_current_min = true;
    req.current_min = 10;
    req.has_override_current = true;
    req.override_current = 160; // 16A in dA

    http_validation_error_t errors[10];
    int count = http_api_validate_settings(&req, 6, 32, 0, 0, errors, 10);
    TEST_ASSERT_EQUAL_INT(0, count);
}

/*
 * @feature HTTP API Settings Validation
 * @req REQ-API-010
 * @scenario Invalid current_min in combined request
 */
void test_validate_settings_invalid_min(void) {
    http_settings_request_t req;
    memset(&req, 0, sizeof(req));
    req.has_current_min = true;
    req.current_min = 3; // too low

    http_validation_error_t errors[10];
    int count = http_api_validate_settings(&req, 6, 32, 0, 0, errors, 10);
    TEST_ASSERT_EQUAL_INT(1, count);
    TEST_ASSERT_TRUE(strcmp(errors[0].field, "current_min") == 0);
}

/*
 * @feature HTTP API Settings Validation
 * @req REQ-API-010
 * @scenario Multiple invalid fields
 */
void test_validate_settings_multiple_errors(void) {
    http_settings_request_t req;
    memset(&req, 0, sizeof(req));
    req.has_current_min = true;
    req.current_min = 3; // too low
    req.has_stop_timer = true;
    req.stop_timer = 99; // too high

    http_validation_error_t errors[10];
    int count = http_api_validate_settings(&req, 6, 32, 0, 0, errors, 10);
    TEST_ASSERT_EQUAL_INT(2, count);
}

/*
 * @feature HTTP API Settings Validation
 * @req REQ-API-010
 * @scenario Empty request passes validation
 */
void test_validate_settings_empty(void) {
    http_settings_request_t req;
    memset(&req, 0, sizeof(req));

    http_validation_error_t errors[10];
    int count = http_api_validate_settings(&req, 6, 32, 0, 0, errors, 10);
    TEST_ASSERT_EQUAL_INT(0, count);
}

/*
 * @feature HTTP API Settings Validation
 * @req REQ-API-010
 * @scenario Slave restrictions applied
 */
void test_validate_settings_slave_restrictions(void) {
    http_settings_request_t req;
    memset(&req, 0, sizeof(req));
    req.has_current_min = true;
    req.current_min = 10;

    http_validation_error_t errors[10];
    int count = http_api_validate_settings(&req, 6, 32, 2, 0, errors, 10); // LoadBl=2 (slave)
    TEST_ASSERT_EQUAL_INT(1, count);
}

/*
 * @feature HTTP API Settings Validation
 * @req REQ-API-014
 * @scenario Valid scheduling settings in combined request
 * @given A settings request with valid scheduling fields
 * @when Validated on master (load_bl=1)
 * @then No errors are returned
 */
void test_validate_settings_scheduling_valid(void) {
    http_settings_request_t req;
    memset(&req, 0, sizeof(req));
    req.has_prio_strategy = true;
    req.prio_strategy = 1;
    req.has_rotation_interval = true;
    req.rotation_interval = 60;
    req.has_idle_timeout = true;
    req.idle_timeout = 120;

    http_validation_error_t errors[10];
    int count = http_api_validate_settings(&req, 6, 32, 1, 0, errors, 10);
    TEST_ASSERT_EQUAL_INT(0, count);
}

/*
 * @feature HTTP API Settings Validation
 * @req REQ-API-014
 * @scenario Invalid scheduling settings on slave
 * @given A settings request with scheduling fields
 * @when Validated on slave (load_bl=2)
 * @then All three scheduling fields produce errors
 */
void test_validate_settings_scheduling_slave(void) {
    http_settings_request_t req;
    memset(&req, 0, sizeof(req));
    req.has_prio_strategy = true;
    req.prio_strategy = 0;
    req.has_rotation_interval = true;
    req.rotation_interval = 60;
    req.has_idle_timeout = true;
    req.idle_timeout = 60;

    http_validation_error_t errors[10];
    int count = http_api_validate_settings(&req, 6, 32, 2, 0, errors, 10);
    TEST_ASSERT_EQUAL_INT(3, count);
}

// ---- IEC 61851 State Mapping ----

/*
 * @feature EVCC IEC 61851 State Mapping
 * @req REQ-API-020
 * @scenario STATE_A maps to IEC 61851 state A (standby)
 * @given The EVSE is in STATE_A with no errors
 * @when evse_state_to_iec61851 is called
 * @then It returns 'A'
 */
void test_iec61851_state_a(void) {
    TEST_ASSERT_EQUAL_INT('A', evse_state_to_iec61851(STATE_A, NO_ERROR));
}

/*
 * @feature EVCC IEC 61851 State Mapping
 * @req REQ-API-020
 * @scenario STATE_B maps to IEC 61851 state B (vehicle detected)
 * @given The EVSE is in STATE_B with no errors
 * @when evse_state_to_iec61851 is called
 * @then It returns 'B'
 */
void test_iec61851_state_b(void) {
    TEST_ASSERT_EQUAL_INT('B', evse_state_to_iec61851(STATE_B, NO_ERROR));
}

/*
 * @feature EVCC IEC 61851 State Mapping
 * @req REQ-API-020
 * @scenario STATE_C maps to IEC 61851 state C (charging)
 * @given The EVSE is in STATE_C with no errors
 * @when evse_state_to_iec61851 is called
 * @then It returns 'C'
 */
void test_iec61851_state_c(void) {
    TEST_ASSERT_EQUAL_INT('C', evse_state_to_iec61851(STATE_C, NO_ERROR));
}

/*
 * @feature EVCC IEC 61851 State Mapping
 * @req REQ-API-020
 * @scenario STATE_D maps to IEC 61851 state D (ventilation required)
 * @given The EVSE is in STATE_D with no errors
 * @when evse_state_to_iec61851 is called
 * @then It returns 'D'
 */
void test_iec61851_state_d(void) {
    TEST_ASSERT_EQUAL_INT('D', evse_state_to_iec61851(STATE_D, NO_ERROR));
}

/*
 * @feature EVCC IEC 61851 State Mapping
 * @req REQ-API-021
 * @scenario STATE_B1 maps to IEC 61851 state B (connected, EVSE not ready)
 * @given The EVSE is in STATE_B1 (no PWM signal) with no errors
 * @when evse_state_to_iec61851 is called
 * @then It returns 'B' because the vehicle is connected
 */
void test_iec61851_state_b1(void) {
    TEST_ASSERT_EQUAL_INT('B', evse_state_to_iec61851(STATE_B1, NO_ERROR));
}

/*
 * @feature EVCC IEC 61851 State Mapping
 * @req REQ-API-021
 * @scenario STATE_C1 maps to IEC 61851 state C (charge stopping)
 * @given The EVSE is in STATE_C1 (stopping) with no errors
 * @when evse_state_to_iec61851 is called
 * @then It returns 'C' because charging session is still active
 */
void test_iec61851_state_c1(void) {
    TEST_ASSERT_EQUAL_INT('C', evse_state_to_iec61851(STATE_C1, NO_ERROR));
}

/*
 * @feature EVCC IEC 61851 State Mapping
 * @req REQ-API-021
 * @scenario Communication and modem states map to B (connected)
 * @given The EVSE is in various communication/modem states
 * @when evse_state_to_iec61851 is called for each
 * @then All return 'B' because the vehicle is connected but not yet charging
 */
void test_iec61851_comm_modem_states(void) {
    TEST_ASSERT_EQUAL_INT('B', evse_state_to_iec61851(STATE_COMM_B, NO_ERROR));
    TEST_ASSERT_EQUAL_INT('B', evse_state_to_iec61851(STATE_COMM_B_OK, NO_ERROR));
    TEST_ASSERT_EQUAL_INT('C', evse_state_to_iec61851(STATE_COMM_C, NO_ERROR));
    TEST_ASSERT_EQUAL_INT('C', evse_state_to_iec61851(STATE_COMM_C_OK, NO_ERROR));
    TEST_ASSERT_EQUAL_INT('B', evse_state_to_iec61851(STATE_ACTSTART, NO_ERROR));
    TEST_ASSERT_EQUAL_INT('B', evse_state_to_iec61851(STATE_MODEM_REQUEST, NO_ERROR));
    TEST_ASSERT_EQUAL_INT('B', evse_state_to_iec61851(STATE_MODEM_WAIT, NO_ERROR));
    TEST_ASSERT_EQUAL_INT('B', evse_state_to_iec61851(STATE_MODEM_DONE, NO_ERROR));
}

/*
 * @feature EVCC IEC 61851 State Mapping
 * @req REQ-API-022
 * @scenario Modem denied maps to E (error)
 * @given The EVSE is in STATE_MODEM_DENIED
 * @when evse_state_to_iec61851 is called
 * @then It returns 'E' because access was denied
 */
void test_iec61851_modem_denied(void) {
    TEST_ASSERT_EQUAL_INT('E', evse_state_to_iec61851(STATE_MODEM_DENIED, NO_ERROR));
}

/*
 * @feature EVCC IEC 61851 State Mapping
 * @req REQ-API-022
 * @scenario Hard error flags override state to E (error)
 * @given The EVSE is in STATE_C (charging) with RCM_TRIPPED error
 * @when evse_state_to_iec61851 is called
 * @then It returns 'E' because a hard error takes priority
 */
void test_iec61851_hard_error_overrides_state(void) {
    TEST_ASSERT_EQUAL_INT('E', evse_state_to_iec61851(STATE_C, RCM_TRIPPED));
    TEST_ASSERT_EQUAL_INT('E', evse_state_to_iec61851(STATE_B, CT_NOCOMM));
    TEST_ASSERT_EQUAL_INT('E', evse_state_to_iec61851(STATE_C, TEMP_HIGH));
    TEST_ASSERT_EQUAL_INT('E', evse_state_to_iec61851(STATE_C, EV_NOCOMM));
}

/*
 * @feature EVCC IEC 61851 State Mapping
 * @req REQ-API-022
 * @scenario Soft errors (LESS_6A, NO_SUN) do NOT override state
 * @given The EVSE is in STATE_C with LESS_6A or STATE_A with NO_SUN
 * @when evse_state_to_iec61851 is called
 * @then It returns the state-based letter, not 'E'
 */
void test_iec61851_soft_errors_no_override(void) {
    TEST_ASSERT_EQUAL_INT('C', evse_state_to_iec61851(STATE_C, LESS_6A));
    TEST_ASSERT_EQUAL_INT('A', evse_state_to_iec61851(STATE_A, NO_SUN));
    TEST_ASSERT_EQUAL_INT('B', evse_state_to_iec61851(STATE_B, LESS_6A | NO_SUN));
}

/*
 * @feature EVCC IEC 61851 State Mapping
 * @req REQ-API-023
 * @scenario NOSTATE and unknown values map to F (not available)
 * @given The EVSE is in NOSTATE or an unrecognized state value
 * @when evse_state_to_iec61851 is called
 * @then It returns 'F' indicating EVSE not available
 */
void test_iec61851_nostate_and_unknown(void) {
    TEST_ASSERT_EQUAL_INT('F', evse_state_to_iec61851(NOSTATE, NO_ERROR));
    TEST_ASSERT_EQUAL_INT('F', evse_state_to_iec61851(99, NO_ERROR));
}

// ---- Charging Enabled Derivation ----

/*
 * @feature EVCC Charging Enabled
 * @req REQ-API-025
 * @scenario STATE_C means charging is enabled
 * @given The EVSE is in STATE_C (charging)
 * @when evse_charging_enabled is called
 * @then It returns true
 */
void test_charging_enabled_state_c(void) {
    TEST_ASSERT_TRUE(evse_charging_enabled(STATE_C));
}

/*
 * @feature EVCC Charging Enabled
 * @req REQ-API-025
 * @scenario STATE_C1 means charging is enabled (stopping phase)
 * @given The EVSE is in STATE_C1 (charge stopping)
 * @when evse_charging_enabled is called
 * @then It returns true because energy is still being delivered
 */
void test_charging_enabled_state_c1(void) {
    TEST_ASSERT_TRUE(evse_charging_enabled(STATE_C1));
}

/*
 * @feature EVCC Charging Enabled
 * @req REQ-API-025
 * @scenario Non-charging states return false
 * @given The EVSE is in STATE_A, STATE_B, or other non-charging states
 * @when evse_charging_enabled is called
 * @then It returns false
 */
void test_charging_enabled_non_charging_states(void) {
    TEST_ASSERT_FALSE(evse_charging_enabled(STATE_A));
    TEST_ASSERT_FALSE(evse_charging_enabled(STATE_B));
    TEST_ASSERT_FALSE(evse_charging_enabled(STATE_B1));
    TEST_ASSERT_FALSE(evse_charging_enabled(STATE_D));
    TEST_ASSERT_FALSE(evse_charging_enabled(STATE_COMM_B));
    TEST_ASSERT_FALSE(evse_charging_enabled(STATE_ACTSTART));
    TEST_ASSERT_FALSE(evse_charging_enabled(STATE_MODEM_REQUEST));
    TEST_ASSERT_FALSE(evse_charging_enabled(STATE_MODEM_DENIED));
    TEST_ASSERT_FALSE(evse_charging_enabled(NOSTATE));
}

// ---- Phase Switch Validation ----

/*
 * @feature EVCC Phase Switch Validation
 * @req REQ-API-024
 * @scenario Valid 1-phase switch request on standalone with C2 contactor
 * @given A standalone EVSE (load_bl=0) with EnableC2=AUTO
 * @when A phase switch to 1 phase is requested
 * @then Validation passes (returns NULL)
 */
void test_phase_switch_valid_1p(void) {
    http_phase_switch_request_t req = { .phases = 1 };
    TEST_ASSERT_TRUE(http_api_validate_phase_switch(&req, AUTO, 0) == NULL);
}

/*
 * @feature EVCC Phase Switch Validation
 * @req REQ-API-024
 * @scenario Valid 3-phase switch request on master with C2 contactor
 * @given A master EVSE (load_bl=1) with EnableC2=ALWAYS_ON
 * @when A phase switch to 3 phases is requested
 * @then Validation passes (returns NULL)
 */
void test_phase_switch_valid_3p(void) {
    http_phase_switch_request_t req = { .phases = 3 };
    TEST_ASSERT_TRUE(http_api_validate_phase_switch(&req, ALWAYS_ON, 1) == NULL);
}

/*
 * @feature EVCC Phase Switch Validation
 * @req REQ-API-025
 * @scenario Invalid phase count (2) is rejected
 * @given A standalone EVSE with EnableC2=AUTO
 * @when A phase switch to 2 phases is requested
 * @then Validation fails with error message
 */
void test_phase_switch_invalid_phase_count(void) {
    http_phase_switch_request_t req = { .phases = 2 };
    TEST_ASSERT_TRUE(http_api_validate_phase_switch(&req, AUTO, 0) != NULL);
}

/*
 * @feature EVCC Phase Switch Validation
 * @req REQ-API-025
 * @scenario Phase switch rejected when C2 contactor not present
 * @given An EVSE with EnableC2=NOT_PRESENT (no C2 hardware)
 * @when A phase switch to 1 phase is requested
 * @then Validation fails because hardware cannot switch phases
 */
void test_phase_switch_no_c2_hardware(void) {
    http_phase_switch_request_t req = { .phases = 1 };
    TEST_ASSERT_TRUE(http_api_validate_phase_switch(&req, NOT_PRESENT, 0) != NULL);
}

/*
 * @feature EVCC Phase Switch Validation
 * @req REQ-API-025
 * @scenario Phase switch rejected on slave node
 * @given A slave EVSE (load_bl=2) with EnableC2=AUTO
 * @when A phase switch to 3 phases is requested
 * @then Validation fails because slaves cannot initiate phase switching
 */
void test_phase_switch_slave_rejected(void) {
    http_phase_switch_request_t req = { .phases = 3 };
    TEST_ASSERT_TRUE(http_api_validate_phase_switch(&req, AUTO, 2) != NULL);
}

/*
 * @feature EVCC Phase Switch Validation
 * @req REQ-API-025
 * @scenario Phase switch with zero phases is rejected
 * @given A standalone EVSE with EnableC2=AUTO
 * @when A phase switch to 0 phases is requested
 * @then Validation fails
 */
void test_phase_switch_zero_phases(void) {
    http_phase_switch_request_t req = { .phases = 0 };
    TEST_ASSERT_TRUE(http_api_validate_phase_switch(&req, AUTO, 0) != NULL);
}

/*
 * @feature EVCC Phase Switch Validation
 * @req REQ-API-024
 * @scenario Phase switch valid with all non-NOT_PRESENT EnableC2 values
 * @given A standalone EVSE with various EnableC2 settings (ALWAYS_OFF, SOLAR_OFF, ALWAYS_ON, AUTO)
 * @when A phase switch to 1 phase is requested
 * @then Validation passes for all C2 configurations that have hardware present
 */
void test_phase_switch_all_c2_configs(void) {
    http_phase_switch_request_t req = { .phases = 1 };
    TEST_ASSERT_TRUE(http_api_validate_phase_switch(&req, ALWAYS_OFF, 0) == NULL);
    TEST_ASSERT_TRUE(http_api_validate_phase_switch(&req, SOLAR_OFF, 0) == NULL);
    TEST_ASSERT_TRUE(http_api_validate_phase_switch(&req, ALWAYS_ON, 0) == NULL);
    TEST_ASSERT_TRUE(http_api_validate_phase_switch(&req, AUTO, 0) == NULL);
}

int main(void) {
    TEST_SUITE_BEGIN("HTTP API");

    // Color parsing
    RUN_TEST(test_color_valid);
    RUN_TEST(test_color_zero);
    RUN_TEST(test_color_max);
    RUN_TEST(test_color_out_of_range);
    RUN_TEST(test_color_negative);

    // Override current
    RUN_TEST(test_override_current_zero);
    RUN_TEST(test_override_current_valid);
    RUN_TEST(test_override_current_at_min);
    RUN_TEST(test_override_current_at_max);
    RUN_TEST(test_override_current_below_min);
    RUN_TEST(test_override_current_above_max);
    RUN_TEST(test_override_current_slave);

    // Current min
    RUN_TEST(test_current_min_valid);
    RUN_TEST(test_current_min_max);
    RUN_TEST(test_current_min_too_low);
    RUN_TEST(test_current_min_too_high);
    RUN_TEST(test_current_min_slave);

    // Max sum mains
    RUN_TEST(test_max_sum_mains_zero);
    RUN_TEST(test_max_sum_mains_min);
    RUN_TEST(test_max_sum_mains_max);
    RUN_TEST(test_max_sum_mains_gap);
    RUN_TEST(test_max_sum_mains_too_high);
    RUN_TEST(test_max_sum_mains_slave);

    // Stop timer
    RUN_TEST(test_stop_timer_zero);
    RUN_TEST(test_stop_timer_max);
    RUN_TEST(test_stop_timer_too_high);
    RUN_TEST(test_stop_timer_negative);

    // Solar
    RUN_TEST(test_solar_start_zero);
    RUN_TEST(test_solar_start_max);
    RUN_TEST(test_solar_start_too_high);
    RUN_TEST(test_solar_import_zero);
    RUN_TEST(test_solar_import_too_high);

    // PrioStrategy
    RUN_TEST(test_prio_strategy_valid_0);
    RUN_TEST(test_prio_strategy_valid_1);
    RUN_TEST(test_prio_strategy_valid_2);
    RUN_TEST(test_prio_strategy_too_high);
    RUN_TEST(test_prio_strategy_negative);
    RUN_TEST(test_prio_strategy_slave);

    // RotationInterval
    RUN_TEST(test_rotation_interval_zero);
    RUN_TEST(test_rotation_interval_min);
    RUN_TEST(test_rotation_interval_max);
    RUN_TEST(test_rotation_interval_gap);
    RUN_TEST(test_rotation_interval_too_high);
    RUN_TEST(test_rotation_interval_slave);

    // IdleTimeout
    RUN_TEST(test_idle_timeout_min);
    RUN_TEST(test_idle_timeout_default);
    RUN_TEST(test_idle_timeout_max);
    RUN_TEST(test_idle_timeout_too_low);
    RUN_TEST(test_idle_timeout_too_high);
    RUN_TEST(test_idle_timeout_slave);

    // Combined validation
    RUN_TEST(test_validate_settings_valid);
    RUN_TEST(test_validate_settings_invalid_min);
    RUN_TEST(test_validate_settings_multiple_errors);
    RUN_TEST(test_validate_settings_empty);
    RUN_TEST(test_validate_settings_slave_restrictions);
    RUN_TEST(test_validate_settings_scheduling_valid);
    RUN_TEST(test_validate_settings_scheduling_slave);

    // IEC 61851 state mapping
    RUN_TEST(test_iec61851_state_a);
    RUN_TEST(test_iec61851_state_b);
    RUN_TEST(test_iec61851_state_c);
    RUN_TEST(test_iec61851_state_d);
    RUN_TEST(test_iec61851_state_b1);
    RUN_TEST(test_iec61851_state_c1);
    RUN_TEST(test_iec61851_comm_modem_states);
    RUN_TEST(test_iec61851_modem_denied);
    RUN_TEST(test_iec61851_hard_error_overrides_state);
    RUN_TEST(test_iec61851_soft_errors_no_override);
    RUN_TEST(test_iec61851_nostate_and_unknown);

    // Charging enabled derivation
    RUN_TEST(test_charging_enabled_state_c);
    RUN_TEST(test_charging_enabled_state_c1);
    RUN_TEST(test_charging_enabled_non_charging_states);

    // Phase switch validation
    RUN_TEST(test_phase_switch_valid_1p);
    RUN_TEST(test_phase_switch_valid_3p);
    RUN_TEST(test_phase_switch_invalid_phase_count);
    RUN_TEST(test_phase_switch_no_c2_hardware);
    RUN_TEST(test_phase_switch_slave_rejected);
    RUN_TEST(test_phase_switch_zero_phases);
    RUN_TEST(test_phase_switch_all_c2_configs);

    TEST_SUITE_RESULTS();
}
