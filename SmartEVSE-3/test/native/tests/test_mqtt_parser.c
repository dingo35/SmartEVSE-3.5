/*
 * test_mqtt_parser.c — Native tests for MQTT command parsing
 *
 * Tests the pure parsing and validation logic extracted from
 * mqtt_receive_callback() into mqtt_parser.c.
 */

#include "test_framework.h"
#include "mqtt_parser.h"
#include <string.h>

#define PREFIX "SmartEVSE/123456"

static mqtt_command_t cmd;

// ---- Mode commands ----

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-001
 * @scenario Set mode to Normal via MQTT
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/Mode with payload "Normal"
 * @then Command type is MQTT_CMD_MODE with mode MQTT_MODE_NORMAL
 */
void test_mode_normal(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/Mode", "Normal", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_MODE, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(MQTT_MODE_NORMAL, cmd.mode);
}

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-001
 * @scenario Set mode to Solar via MQTT
 */
void test_mode_solar(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/Mode", "Solar", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_MODE, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(MQTT_MODE_SOLAR, cmd.mode);
}

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-001
 * @scenario Set mode to Smart via MQTT
 */
void test_mode_smart(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/Mode", "Smart", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_MODE, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(MQTT_MODE_SMART, cmd.mode);
}

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-001
 * @scenario Set mode to Off via MQTT
 */
void test_mode_off(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/Mode", "Off", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_MODE, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(MQTT_MODE_OFF, cmd.mode);
}

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-001
 * @scenario Set mode to Pause via MQTT
 */
void test_mode_pause(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/Mode", "Pause", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_MODE, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(MQTT_MODE_PAUSE, cmd.mode);
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-002
 * @scenario Invalid mode string is rejected
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/Mode with payload "Invalid"
 * @then The parser returns false
 */
void test_mode_invalid(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/Mode", "Invalid", &cmd));
}

// ---- CustomButton ----

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-003
 * @scenario CustomButton set to On
 */
void test_custom_button_on(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/CustomButton", "On", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_CUSTOM_BUTTON, cmd.cmd);
    TEST_ASSERT_TRUE(cmd.custom_button);
}

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-003
 * @scenario CustomButton set to Off
 */
void test_custom_button_off(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/CustomButton", "Off", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_CUSTOM_BUTTON, cmd.cmd);
    TEST_ASSERT_FALSE(cmd.custom_button);
}

// ---- CurrentOverride ----

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-004
 * @scenario Current override with valid value
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/CurrentOverride with payload "100"
 * @then Command has current_override = 100
 */
void test_current_override_valid(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/CurrentOverride", "100", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_CURRENT_OVERRIDE, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(100, cmd.current_override);
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-004
 * @scenario Current override zero resets override
 */
void test_current_override_zero(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/CurrentOverride", "0", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_CURRENT_OVERRIDE, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(0, cmd.current_override);
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-004
 * @scenario Current override with max value
 */
void test_current_override_max(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/CurrentOverride", "320", &cmd));
    TEST_ASSERT_EQUAL_INT(320, cmd.current_override);
}

// ---- CurrentMaxSumMains ----

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-005
 * @scenario Max sum mains valid value
 */
void test_max_sum_mains_valid(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/CurrentMaxSumMains", "100", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_MAX_SUM_MAINS, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(100, cmd.max_sum_mains);
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-005
 * @scenario Max sum mains zero disables
 */
void test_max_sum_mains_zero(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/CurrentMaxSumMains", "0", &cmd));
    TEST_ASSERT_EQUAL_INT(0, cmd.max_sum_mains);
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-005
 * @scenario Max sum mains below minimum rejected
 */
void test_max_sum_mains_below_min(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/CurrentMaxSumMains", "5", &cmd));
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-005
 * @scenario Max sum mains above maximum rejected
 */
void test_max_sum_mains_above_max(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/CurrentMaxSumMains", "601", &cmd));
}

// ---- CPPWMOverride ----

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-006
 * @scenario CP PWM override normal mode (-1)
 */
void test_cp_pwm_normal(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/CPPWMOverride", "-1", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_CP_PWM_OVERRIDE, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(-1, cmd.cp_pwm);
}

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-006
 * @scenario CP PWM override disconnect (0)
 */
void test_cp_pwm_disconnect(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/CPPWMOverride", "0", &cmd));
    TEST_ASSERT_EQUAL_INT(0, cmd.cp_pwm);
}

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-006
 * @scenario CP PWM override max value (1024)
 */
void test_cp_pwm_max(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/CPPWMOverride", "1024", &cmd));
    TEST_ASSERT_EQUAL_INT(1024, cmd.cp_pwm);
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-006
 * @scenario CP PWM override out of range rejected
 */
void test_cp_pwm_out_of_range(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/CPPWMOverride", "1025", &cmd));
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-006
 * @scenario CP PWM override below -1 rejected
 */
void test_cp_pwm_below_neg1(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/CPPWMOverride", "-2", &cmd));
}

// ---- Mains Meter Parsing ----

/*
 * @feature MQTT Meter Parsing
 * @req REQ-MQTT-007
 * @scenario Mains meter format L1:L2:L3 is parsed correctly
 * @given Valid three-phase mains meter data
 * @when Payload is "100:200:300"
 * @then L1=100, L2=200, L3=300
 */
void test_mains_meter_valid(void) {
    int32_t L1, L2, L3;
    TEST_ASSERT_TRUE(mqtt_parse_mains_meter("100:200:300", &L1, &L2, &L3));
    TEST_ASSERT_EQUAL_INT(100, L1);
    TEST_ASSERT_EQUAL_INT(200, L2);
    TEST_ASSERT_EQUAL_INT(300, L3);
}

/*
 * @feature MQTT Meter Parsing
 * @req REQ-MQTT-007
 * @scenario Mains meter with negative values
 */
void test_mains_meter_negative(void) {
    int32_t L1, L2, L3;
    TEST_ASSERT_TRUE(mqtt_parse_mains_meter("-100:-200:-300", &L1, &L2, &L3));
    TEST_ASSERT_EQUAL_INT(-100, L1);
    TEST_ASSERT_EQUAL_INT(-200, L2);
    TEST_ASSERT_EQUAL_INT(-300, L3);
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-007
 * @scenario Mains meter out of range rejected (>=2000)
 */
void test_mains_meter_out_of_range(void) {
    int32_t L1, L2, L3;
    TEST_ASSERT_FALSE(mqtt_parse_mains_meter("2000:0:0", &L1, &L2, &L3));
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-007
 * @scenario Mains meter out of range rejected (<=-2000)
 */
void test_mains_meter_out_of_range_neg(void) {
    int32_t L1, L2, L3;
    TEST_ASSERT_FALSE(mqtt_parse_mains_meter("-2000:0:0", &L1, &L2, &L3));
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-007
 * @scenario Mains meter missing fields rejected
 */
void test_mains_meter_missing_fields(void) {
    int32_t L1, L2, L3;
    TEST_ASSERT_FALSE(mqtt_parse_mains_meter("100:200", &L1, &L2, &L3));
}

/*
 * @feature MQTT Meter Parsing
 * @req REQ-MQTT-007
 * @scenario Mains meter via full command parse
 */
void test_mains_meter_command(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/MainsMeter", "50:60:70", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_MAINS_METER, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(50, cmd.mains_meter.L1);
    TEST_ASSERT_EQUAL_INT(60, cmd.mains_meter.L2);
    TEST_ASSERT_EQUAL_INT(70, cmd.mains_meter.L3);
}

// ---- EV Meter Parsing ----

/*
 * @feature MQTT Meter Parsing
 * @req REQ-MQTT-008
 * @scenario EV meter format L1:L2:L3:W:WH is parsed correctly
 */
void test_ev_meter_valid(void) {
    int32_t L1, L2, L3, W, Wh;
    TEST_ASSERT_TRUE(mqtt_parse_ev_meter("10:20:30:500:1000", &L1, &L2, &L3, &W, &Wh));
    TEST_ASSERT_EQUAL_INT(10, L1);
    TEST_ASSERT_EQUAL_INT(20, L2);
    TEST_ASSERT_EQUAL_INT(30, L3);
    TEST_ASSERT_EQUAL_INT(500, W);
    TEST_ASSERT_EQUAL_INT(1000, Wh);
}

/*
 * @feature MQTT Meter Parsing
 * @req REQ-MQTT-008
 * @scenario EV meter with unknown values (-1)
 */
void test_ev_meter_unknown_values(void) {
    int32_t L1, L2, L3, W, Wh;
    TEST_ASSERT_TRUE(mqtt_parse_ev_meter("-1:-1:-1:-1:-1", &L1, &L2, &L3, &W, &Wh));
    TEST_ASSERT_EQUAL_INT(-1, L1);
    TEST_ASSERT_EQUAL_INT(-1, W);
    TEST_ASSERT_EQUAL_INT(-1, Wh);
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-008
 * @scenario EV meter partial data rejected
 */
void test_ev_meter_partial(void) {
    int32_t L1, L2, L3, W, Wh;
    TEST_ASSERT_FALSE(mqtt_parse_ev_meter("10:20:30", &L1, &L2, &L3, &W, &Wh));
}

/*
 * @feature MQTT Meter Parsing
 * @req REQ-MQTT-008
 * @scenario EV meter via full command parse
 */
void test_ev_meter_command(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/EVMeter", "10:20:30:500:1000", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_EV_METER, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(10, cmd.ev_meter.L1);
    TEST_ASSERT_EQUAL_INT(500, cmd.ev_meter.W);
    TEST_ASSERT_EQUAL_INT(1000, cmd.ev_meter.Wh);
}

// ---- HomeBatteryCurrent ----

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-009
 * @scenario Home battery current set
 */
void test_home_battery_current(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/HomeBatteryCurrent", "50", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_HOME_BATTERY_CURRENT, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(50, cmd.home_battery_current);
}

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-009
 * @scenario Home battery current negative (discharging)
 */
void test_home_battery_current_negative(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/HomeBatteryCurrent", "-30", &cmd));
    TEST_ASSERT_EQUAL_INT(-30, cmd.home_battery_current);
}

// ---- Color Parsing ----

/*
 * @feature MQTT Color Parsing
 * @req REQ-MQTT-010
 * @scenario Valid RGB color parsed
 */
void test_rgb_valid(void) {
    uint8_t r, g, b;
    TEST_ASSERT_TRUE(mqtt_parse_rgb("255,128,0", &r, &g, &b));
    TEST_ASSERT_EQUAL_INT(255, r);
    TEST_ASSERT_EQUAL_INT(128, g);
    TEST_ASSERT_EQUAL_INT(0, b);
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-010
 * @scenario RGB color out of range rejected
 */
void test_rgb_out_of_range(void) {
    uint8_t r, g, b;
    TEST_ASSERT_FALSE(mqtt_parse_rgb("256,0,0", &r, &g, &b));
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-010
 * @scenario RGB color negative rejected
 */
void test_rgb_negative(void) {
    uint8_t r, g, b;
    TEST_ASSERT_FALSE(mqtt_parse_rgb("-1,0,0", &r, &g, &b));
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-010
 * @scenario RGB color missing component rejected
 */
void test_rgb_missing(void) {
    uint8_t r, g, b;
    TEST_ASSERT_FALSE(mqtt_parse_rgb("255,128", &r, &g, &b));
}

/*
 * @feature MQTT Color Parsing
 * @req REQ-MQTT-010
 * @scenario ColorOff topic parsed correctly
 */
void test_color_off_command(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/ColorOff", "10,20,30", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_COLOR, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(MQTT_COLOR_OFF, cmd.color.index);
    TEST_ASSERT_EQUAL_INT(10, cmd.color.r);
    TEST_ASSERT_EQUAL_INT(20, cmd.color.g);
    TEST_ASSERT_EQUAL_INT(30, cmd.color.b);
}

/*
 * @feature MQTT Color Parsing
 * @req REQ-MQTT-010
 * @scenario ColorSolar topic parsed correctly
 */
void test_color_solar_command(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/ColorSolar", "0,255,0", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_COLOR_SOLAR, cmd.color.index);
    TEST_ASSERT_EQUAL_INT(255, cmd.color.g);
}

/*
 * @feature MQTT Color Parsing
 * @req REQ-MQTT-010
 * @scenario ColorCustom topic parsed correctly
 */
void test_color_custom_command(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/ColorCustom", "100,100,100", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_COLOR_CUSTOM, cmd.color.index);
}

// ---- CableLock ----

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-011
 * @scenario Cable lock enabled
 */
void test_cable_lock_enable(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/CableLock", "1", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_CABLE_LOCK, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(1, cmd.cable_lock);
}

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-011
 * @scenario Cable lock disabled
 */
void test_cable_lock_disable(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/CableLock", "0", &cmd));
    TEST_ASSERT_EQUAL_INT(0, cmd.cable_lock);
}

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-011
 * @scenario Cable lock any non-"1" disables
 */
void test_cable_lock_any_other(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/CableLock", "abc", &cmd));
    TEST_ASSERT_EQUAL_INT(0, cmd.cable_lock);
}

// ---- EnableC2 ----

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-012
 * @scenario EnableC2 numeric value
 */
void test_enable_c2_numeric(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/EnableC2", "3", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_ENABLE_C2, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(3, cmd.enable_c2);
}

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-012
 * @scenario EnableC2 string value
 */
void test_enable_c2_string(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/EnableC2", "Always On", &cmd));
    TEST_ASSERT_EQUAL_INT(3, cmd.enable_c2);
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-012
 * @scenario EnableC2 out of range rejected
 */
void test_enable_c2_out_of_range(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/EnableC2", "5", &cmd));
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-012
 * @scenario EnableC2 invalid string rejected
 */
void test_enable_c2_invalid_string(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/EnableC2", "InvalidMode", &cmd));
}

// ---- RequiredEVCCID ----

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-013
 * @scenario RequiredEVCCID set
 */
void test_required_evccid(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/RequiredEVCCID", "ABC123", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_REQUIRED_EVCCID, cmd.cmd);
    TEST_ASSERT_TRUE(strcmp(cmd.evccid, "ABC123") == 0);
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-013
 * @scenario RequiredEVCCID too long rejected
 */
void test_required_evccid_too_long(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/RequiredEVCCID",
        "12345678901234567890123456789012", &cmd)); // 32 chars = too long (no room for null)
}

// ---- PrioStrategy ----

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-015
 * @scenario PrioStrategy set to MODBUS_ADDR (0) via MQTT
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/PrioStrategy with payload "0"
 * @then Command type is MQTT_CMD_PRIO_STRATEGY with value 0
 */
void test_prio_strategy_modbus_addr(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/PrioStrategy", "0", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_PRIO_STRATEGY, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(0, cmd.prio_strategy);
}

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-015
 * @scenario PrioStrategy set to FIRST_CONNECTED (1)
 */
void test_prio_strategy_first_connected(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/PrioStrategy", "1", &cmd));
    TEST_ASSERT_EQUAL_INT(1, cmd.prio_strategy);
}

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-015
 * @scenario PrioStrategy set to LAST_CONNECTED (2)
 */
void test_prio_strategy_last_connected(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/PrioStrategy", "2", &cmd));
    TEST_ASSERT_EQUAL_INT(2, cmd.prio_strategy);
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-015
 * @scenario PrioStrategy value 3 is rejected (out of range)
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/PrioStrategy with payload "3"
 * @then The parser returns false
 */
void test_prio_strategy_out_of_range(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/PrioStrategy", "3", &cmd));
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-015
 * @scenario PrioStrategy negative value is rejected
 */
void test_prio_strategy_negative(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/PrioStrategy", "-1", &cmd));
}

// ---- RotationInterval ----

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-016
 * @scenario RotationInterval set to 0 (disabled) via MQTT
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/RotationInterval with payload "0"
 * @then Command type is MQTT_CMD_ROTATION_INTERVAL with value 0
 */
void test_rotation_interval_zero(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/RotationInterval", "0", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_ROTATION_INTERVAL, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(0, cmd.rotation_interval);
}

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-016
 * @scenario RotationInterval set to minimum (30 minutes)
 */
void test_rotation_interval_min(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/RotationInterval", "30", &cmd));
    TEST_ASSERT_EQUAL_INT(30, cmd.rotation_interval);
}

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-016
 * @scenario RotationInterval set to maximum (1440 minutes = 24h)
 */
void test_rotation_interval_max(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/RotationInterval", "1440", &cmd));
    TEST_ASSERT_EQUAL_INT(1440, cmd.rotation_interval);
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-016
 * @scenario RotationInterval in gap (1-29) is rejected
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/RotationInterval with payload "15"
 * @then The parser returns false
 */
void test_rotation_interval_gap(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/RotationInterval", "15", &cmd));
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-016
 * @scenario RotationInterval above maximum is rejected
 */
void test_rotation_interval_too_high(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/RotationInterval", "1441", &cmd));
}

// ---- IdleTimeout ----

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-017
 * @scenario IdleTimeout set to minimum (30 seconds) via MQTT
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/IdleTimeout with payload "30"
 * @then Command type is MQTT_CMD_IDLE_TIMEOUT with value 30
 */
void test_idle_timeout_min(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/IdleTimeout", "30", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_IDLE_TIMEOUT, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(30, cmd.idle_timeout);
}

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-017
 * @scenario IdleTimeout set to default (60 seconds)
 */
void test_idle_timeout_default(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/IdleTimeout", "60", &cmd));
    TEST_ASSERT_EQUAL_INT(60, cmd.idle_timeout);
}

/*
 * @feature MQTT Command Parsing
 * @req REQ-MQTT-017
 * @scenario IdleTimeout set to maximum (300 seconds)
 */
void test_idle_timeout_max(void) {
    TEST_ASSERT_TRUE(mqtt_parse_command(PREFIX, PREFIX "/Set/IdleTimeout", "300", &cmd));
    TEST_ASSERT_EQUAL_INT(300, cmd.idle_timeout);
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-017
 * @scenario IdleTimeout below minimum (29) is rejected
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/IdleTimeout with payload "29"
 * @then The parser returns false
 */
void test_idle_timeout_too_low(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/IdleTimeout", "29", &cmd));
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-017
 * @scenario IdleTimeout above maximum (301) is rejected
 */
void test_idle_timeout_too_high(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/IdleTimeout", "301", &cmd));
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-017
 * @scenario IdleTimeout zero is rejected (minimum is 30)
 */
void test_idle_timeout_zero(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/IdleTimeout", "0", &cmd));
}

// ---- Unrecognized topic ----

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-014
 * @scenario Unrecognized topic returns false
 */
void test_unrecognized_topic(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, PREFIX "/Set/Unknown", "value", &cmd));
}

/*
 * @feature MQTT Input Validation
 * @req REQ-MQTT-014
 * @scenario Wrong prefix returns false
 */
void test_wrong_prefix(void) {
    TEST_ASSERT_FALSE(mqtt_parse_command(PREFIX, "OtherDevice/Set/Mode", "Normal", &cmd));
}

int main(void) {
    TEST_SUITE_BEGIN("MQTT Parser");

    // Mode
    RUN_TEST(test_mode_normal);
    RUN_TEST(test_mode_solar);
    RUN_TEST(test_mode_smart);
    RUN_TEST(test_mode_off);
    RUN_TEST(test_mode_pause);
    RUN_TEST(test_mode_invalid);

    // CustomButton
    RUN_TEST(test_custom_button_on);
    RUN_TEST(test_custom_button_off);

    // CurrentOverride
    RUN_TEST(test_current_override_valid);
    RUN_TEST(test_current_override_zero);
    RUN_TEST(test_current_override_max);

    // MaxSumMains
    RUN_TEST(test_max_sum_mains_valid);
    RUN_TEST(test_max_sum_mains_zero);
    RUN_TEST(test_max_sum_mains_below_min);
    RUN_TEST(test_max_sum_mains_above_max);

    // CPPWMOverride
    RUN_TEST(test_cp_pwm_normal);
    RUN_TEST(test_cp_pwm_disconnect);
    RUN_TEST(test_cp_pwm_max);
    RUN_TEST(test_cp_pwm_out_of_range);
    RUN_TEST(test_cp_pwm_below_neg1);

    // MainsMeter
    RUN_TEST(test_mains_meter_valid);
    RUN_TEST(test_mains_meter_negative);
    RUN_TEST(test_mains_meter_out_of_range);
    RUN_TEST(test_mains_meter_out_of_range_neg);
    RUN_TEST(test_mains_meter_missing_fields);
    RUN_TEST(test_mains_meter_command);

    // EVMeter
    RUN_TEST(test_ev_meter_valid);
    RUN_TEST(test_ev_meter_unknown_values);
    RUN_TEST(test_ev_meter_partial);
    RUN_TEST(test_ev_meter_command);

    // HomeBatteryCurrent
    RUN_TEST(test_home_battery_current);
    RUN_TEST(test_home_battery_current_negative);

    // Color parsing
    RUN_TEST(test_rgb_valid);
    RUN_TEST(test_rgb_out_of_range);
    RUN_TEST(test_rgb_negative);
    RUN_TEST(test_rgb_missing);
    RUN_TEST(test_color_off_command);
    RUN_TEST(test_color_solar_command);
    RUN_TEST(test_color_custom_command);

    // CableLock
    RUN_TEST(test_cable_lock_enable);
    RUN_TEST(test_cable_lock_disable);
    RUN_TEST(test_cable_lock_any_other);

    // EnableC2
    RUN_TEST(test_enable_c2_numeric);
    RUN_TEST(test_enable_c2_string);
    RUN_TEST(test_enable_c2_out_of_range);
    RUN_TEST(test_enable_c2_invalid_string);

    // RequiredEVCCID
    RUN_TEST(test_required_evccid);
    RUN_TEST(test_required_evccid_too_long);

    // PrioStrategy
    RUN_TEST(test_prio_strategy_modbus_addr);
    RUN_TEST(test_prio_strategy_first_connected);
    RUN_TEST(test_prio_strategy_last_connected);
    RUN_TEST(test_prio_strategy_out_of_range);
    RUN_TEST(test_prio_strategy_negative);

    // RotationInterval
    RUN_TEST(test_rotation_interval_zero);
    RUN_TEST(test_rotation_interval_min);
    RUN_TEST(test_rotation_interval_max);
    RUN_TEST(test_rotation_interval_gap);
    RUN_TEST(test_rotation_interval_too_high);

    // IdleTimeout
    RUN_TEST(test_idle_timeout_min);
    RUN_TEST(test_idle_timeout_default);
    RUN_TEST(test_idle_timeout_max);
    RUN_TEST(test_idle_timeout_too_low);
    RUN_TEST(test_idle_timeout_too_high);
    RUN_TEST(test_idle_timeout_zero);

    // Unrecognized
    RUN_TEST(test_unrecognized_topic);
    RUN_TEST(test_wrong_prefix);

    TEST_SUITE_RESULTS();
}
