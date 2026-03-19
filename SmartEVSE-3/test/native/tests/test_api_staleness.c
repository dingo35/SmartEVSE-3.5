/*
 * test_api_staleness.c - API/MQTT mains metering staleness detection
 *
 * Tests:
 *   - Staleness timer countdown and flag set on expiry
 *   - Fall back to MaxMains when stale
 *   - Timer reset clears stale flag (recovery)
 *   - Non-API metering modes skip staleness check
 *   - Staleness disabled when timeout is 0
 *   - CT_NOCOMM suppressed for API mode when staleness is active
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"
#include "mqtt_parser.h"

static evse_ctx_t ctx;

/* ---- Staleness timer countdown ---- */

/*
 * @feature API Mains Staleness Detection
 * @req REQ-MTR-020
 * @scenario Staleness timer counts down each second and sets stale flag on expiry
 * @given EVSE in Smart mode with MainsMeterType=EM_API_METER and api_mains_timeout=3
 * @when 3 seconds elapse with no data update
 * @then api_mains_stale is set to true
 */
void test_staleness_timer_countdown(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = EM_API_METER;
    ctx.LoadBl = 0;
    ctx.api_mains_timeout = 3;
    ctx.api_mains_staleness_timer = 3;
    ctx.api_mains_stale = false;
    ctx.MainsMeterTimeout = COMM_TIMEOUT;

    /* Tick 1: timer 3→2, not stale yet */
    evse_tick_1s(&ctx);
    TEST_ASSERT_FALSE(ctx.api_mains_stale);
    TEST_ASSERT_EQUAL_INT(2, ctx.api_mains_staleness_timer);

    /* Tick 2: timer 2→1, not stale yet */
    evse_tick_1s(&ctx);
    TEST_ASSERT_FALSE(ctx.api_mains_stale);
    TEST_ASSERT_EQUAL_INT(1, ctx.api_mains_staleness_timer);

    /* Tick 3: timer 1→0, stale flag set */
    evse_tick_1s(&ctx);
    TEST_ASSERT_TRUE(ctx.api_mains_stale);
    TEST_ASSERT_EQUAL_INT(0, ctx.api_mains_staleness_timer);
}

/* ---- Fall back to MaxMains on staleness ---- */

/*
 * @feature API Mains Staleness Detection
 * @req REQ-MTR-021
 * @scenario When API data goes stale, all phases fall back to MaxMains
 * @given EVSE in Smart mode with API metering, MaxMains=25, current readings are 10A/phase
 * @when Staleness timer expires
 * @then MainsMeterIrms for all 3 phases is set to MaxMains * 10 (250 dA)
 */
void test_staleness_fallback_to_maxmains(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = EM_API_METER;
    ctx.LoadBl = 0;
    ctx.MaxMains = 25;
    ctx.api_mains_timeout = 1;
    ctx.api_mains_staleness_timer = 1;
    ctx.api_mains_stale = false;
    ctx.MainsMeterTimeout = COMM_TIMEOUT;

    /* Set current readings to 10A per phase (100 dA) */
    ctx.MainsMeterIrms[0] = 100;
    ctx.MainsMeterIrms[1] = 100;
    ctx.MainsMeterIrms[2] = 100;

    /* Tick: timer expires, falls back to MaxMains */
    evse_tick_1s(&ctx);
    TEST_ASSERT_TRUE(ctx.api_mains_stale);
    TEST_ASSERT_EQUAL_INT(250, ctx.MainsMeterIrms[0]);
    TEST_ASSERT_EQUAL_INT(250, ctx.MainsMeterIrms[1]);
    TEST_ASSERT_EQUAL_INT(250, ctx.MainsMeterIrms[2]);
}

/* ---- Recovery when timer is reset ---- */

/*
 * @feature API Mains Staleness Detection
 * @req REQ-MTR-022
 * @scenario Stale flag is cleared when staleness timer is reset (data received)
 * @given EVSE in API mode with stale data (api_mains_stale=true, timer=0)
 * @when External code resets api_mains_staleness_timer to a positive value
 * @then api_mains_stale is cleared on the next tick
 */
void test_staleness_recovery_on_timer_reset(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = EM_API_METER;
    ctx.LoadBl = 0;
    ctx.api_mains_timeout = 120;
    ctx.api_mains_staleness_timer = 0;
    ctx.api_mains_stale = true;
    ctx.MainsMeterTimeout = COMM_TIMEOUT;

    /* Simulate data arrival: reset timer */
    ctx.api_mains_staleness_timer = 120;
    evse_tick_1s(&ctx);
    TEST_ASSERT_FALSE(ctx.api_mains_stale);
}

/* ---- Non-API metering skips staleness check ---- */

/*
 * @feature API Mains Staleness Detection
 * @req REQ-MTR-023
 * @scenario Staleness check is skipped for non-API metering modes
 * @given EVSE in Smart mode with MainsMeterType=1 (Sensorbox) and api_mains_timeout=120
 * @when Staleness timer is 0
 * @then api_mains_stale remains false
 */
void test_staleness_skipped_for_non_api(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1; /* Sensorbox, not API */
    ctx.LoadBl = 0;
    ctx.api_mains_timeout = 120;
    ctx.api_mains_staleness_timer = 0;
    ctx.api_mains_stale = false;
    ctx.MainsMeterTimeout = COMM_TIMEOUT;

    evse_tick_1s(&ctx);
    TEST_ASSERT_FALSE(ctx.api_mains_stale);
}

/* ---- Staleness disabled when timeout is 0 ---- */

/*
 * @feature API Mains Staleness Detection
 * @req REQ-MTR-024
 * @scenario Staleness detection is disabled when api_mains_timeout is 0
 * @given EVSE in Smart mode with API metering and api_mains_timeout=0
 * @when Staleness timer reaches 0
 * @then api_mains_stale remains false, normal CT_NOCOMM applies
 */
void test_staleness_disabled_when_timeout_zero(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = EM_API_METER;
    ctx.LoadBl = 0;
    ctx.api_mains_timeout = 0; /* Disabled */
    ctx.api_mains_staleness_timer = 0;
    ctx.api_mains_stale = false;
    ctx.MainsMeterTimeout = COMM_TIMEOUT;

    evse_tick_1s(&ctx);
    TEST_ASSERT_FALSE(ctx.api_mains_stale);
}

/* ---- CT_NOCOMM suppressed for API mode with staleness active ---- */

/*
 * @feature API Mains Staleness Detection
 * @req REQ-MTR-025
 * @scenario CT_NOCOMM is suppressed when API staleness detection is active
 * @given EVSE in Smart mode with API metering, staleness enabled, MainsMeterTimeout=0
 * @when A 1-second tick occurs
 * @then CT_NOCOMM is NOT set (staleness mechanism handles the timeout instead)
 */
void test_ct_nocomm_suppressed_for_api_with_staleness(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = EM_API_METER;
    ctx.LoadBl = 0;
    ctx.api_mains_timeout = 120;
    ctx.api_mains_staleness_timer = 60; /* Not yet stale */
    ctx.api_mains_stale = false;
    ctx.MainsMeterTimeout = 0; /* Would normally trigger CT_NOCOMM */

    evse_tick_1s(&ctx);
    TEST_ASSERT_FALSE((ctx.ErrorFlags & CT_NOCOMM) != 0);
}

/* ---- CT_NOCOMM still fires for API mode when staleness is disabled ---- */

/*
 * @feature API Mains Staleness Detection
 * @req REQ-MTR-026
 * @scenario CT_NOCOMM fires normally when staleness detection is disabled for API mode
 * @given EVSE in Smart mode with API metering, api_mains_timeout=0, MainsMeterTimeout=0
 * @when A 1-second tick occurs
 * @then CT_NOCOMM IS set (normal timeout behavior)
 */
void test_ct_nocomm_fires_when_staleness_disabled(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = EM_API_METER;
    ctx.LoadBl = 0;
    ctx.api_mains_timeout = 0; /* Staleness disabled */
    ctx.api_mains_staleness_timer = 0;
    ctx.api_mains_stale = false;
    ctx.MainsMeterTimeout = 0; /* Trigger CT_NOCOMM */

    evse_tick_1s(&ctx);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & CT_NOCOMM) != 0);
}

/* ---- Staleness does not fire repeatedly ---- */

/*
 * @feature API Mains Staleness Detection
 * @req REQ-MTR-027
 * @scenario Stale flag is set only once, not repeatedly overwriting Irms each tick
 * @given EVSE in API mode, already stale
 * @when Another tick occurs while stale
 * @then Irms values are not overwritten again (flag already set)
 */
void test_staleness_only_fires_once(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = EM_API_METER;
    ctx.LoadBl = 0;
    ctx.MaxMains = 25;
    ctx.api_mains_timeout = 1;
    ctx.api_mains_staleness_timer = 0;
    ctx.api_mains_stale = true; /* Already stale */
    ctx.MainsMeterTimeout = COMM_TIMEOUT;

    /* Set Irms to different values to verify they aren't overwritten */
    ctx.MainsMeterIrms[0] = 50;
    ctx.MainsMeterIrms[1] = 60;
    ctx.MainsMeterIrms[2] = 70;

    evse_tick_1s(&ctx);
    /* Values should NOT be overwritten since stale flag was already set */
    TEST_ASSERT_EQUAL_INT(50, ctx.MainsMeterIrms[0]);
    TEST_ASSERT_EQUAL_INT(60, ctx.MainsMeterIrms[1]);
    TEST_ASSERT_EQUAL_INT(70, ctx.MainsMeterIrms[2]);
}

/* ---- MQTT parser: staleness timeout setting ---- */

/*
 * @feature API Mains Staleness Detection
 * @req REQ-MQTT-030
 * @scenario Parse MainsMeterTimeout MQTT command with valid value
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/MainsMeterTimeout with payload "120"
 * @then Command type is MQTT_CMD_MAINS_METER_TIMEOUT with value 120
 */
void test_mqtt_parse_staleness_timeout_valid(void) {
    mqtt_command_t cmd;
    TEST_ASSERT_TRUE(mqtt_parse_command("SmartEVSE/1234", "SmartEVSE/1234/Set/MainsMeterTimeout", "120", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_MAINS_METER_TIMEOUT, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(120, cmd.mains_meter_timeout);
}

/*
 * @feature API Mains Staleness Detection
 * @req REQ-MQTT-031
 * @scenario Parse MainsMeterTimeout MQTT command with 0 (disabled)
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/MainsMeterTimeout with payload "0"
 * @then Command type is MQTT_CMD_MAINS_METER_TIMEOUT with value 0
 */
void test_mqtt_parse_staleness_timeout_disabled(void) {
    mqtt_command_t cmd;
    TEST_ASSERT_TRUE(mqtt_parse_command("SmartEVSE/1234", "SmartEVSE/1234/Set/MainsMeterTimeout", "0", &cmd));
    TEST_ASSERT_EQUAL_INT(MQTT_CMD_MAINS_METER_TIMEOUT, cmd.cmd);
    TEST_ASSERT_EQUAL_INT(0, cmd.mains_meter_timeout);
}

/*
 * @feature API Mains Staleness Detection
 * @req REQ-MQTT-032
 * @scenario Reject MainsMeterTimeout values outside valid range
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/MainsMeterTimeout with payload "5" (below minimum 10)
 * @then Parsing returns false
 */
void test_mqtt_parse_staleness_timeout_too_low(void) {
    mqtt_command_t cmd;
    TEST_ASSERT_FALSE(mqtt_parse_command("SmartEVSE/1234", "SmartEVSE/1234/Set/MainsMeterTimeout", "5", &cmd));
}

/*
 * @feature API Mains Staleness Detection
 * @req REQ-MQTT-033
 * @scenario Reject MainsMeterTimeout values above maximum
 * @given A valid MQTT prefix
 * @when Topic is prefix/Set/MainsMeterTimeout with payload "4000" (above maximum 3600)
 * @then Parsing returns false
 */
void test_mqtt_parse_staleness_timeout_too_high(void) {
    mqtt_command_t cmd;
    TEST_ASSERT_FALSE(mqtt_parse_command("SmartEVSE/1234", "SmartEVSE/1234/Set/MainsMeterTimeout", "4000", &cmd));
}

/* ---- Test runner ---- */

int main(void) {
    TEST_SUITE_BEGIN("API Mains Staleness Detection");

    /* State machine staleness tests */
    RUN_TEST(test_staleness_timer_countdown);
    RUN_TEST(test_staleness_fallback_to_maxmains);
    RUN_TEST(test_staleness_recovery_on_timer_reset);
    RUN_TEST(test_staleness_skipped_for_non_api);
    RUN_TEST(test_staleness_disabled_when_timeout_zero);
    RUN_TEST(test_ct_nocomm_suppressed_for_api_with_staleness);
    RUN_TEST(test_ct_nocomm_fires_when_staleness_disabled);
    RUN_TEST(test_staleness_only_fires_once);

    /* MQTT parser tests */
    RUN_TEST(test_mqtt_parse_staleness_timeout_valid);
    RUN_TEST(test_mqtt_parse_staleness_timeout_disabled);
    RUN_TEST(test_mqtt_parse_staleness_timeout_too_low);
    RUN_TEST(test_mqtt_parse_staleness_timeout_too_high);

    TEST_SUITE_RESULTS();
}
