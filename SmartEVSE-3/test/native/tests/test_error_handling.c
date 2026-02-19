/*
 * test_error_handling.c - Error flags, recovery, safety mechanisms
 *
 * Tests:
 *   - Error flag set/clear
 *   - ChargeDelay countdown
 *   - Temperature protection with hysteresis
 *   - Meter communication timeouts
 *   - LESS_6A auto-recovery
 *   - RCM fault handling
 *   - Power unavailable graceful shutdown
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

static void setup_charging(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_SMART;
    ctx.LoadBl = 0;
    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.contactor1_state = true;
}

// ---- Error flag management ----

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-001
 * @scenario Setting an error flag stores it in ErrorFlags
 * @given The EVSE is initialised with no errors
 * @when evse_set_error_flags is called with TEMP_HIGH
 * @then TEMP_HIGH bit is set in ErrorFlags
 */
void test_set_error_flags(void) {
    evse_init(&ctx, NULL);
    evse_set_error_flags(&ctx, TEMP_HIGH);
    TEST_ASSERT_EQUAL_INT(TEMP_HIGH, ctx.ErrorFlags & TEMP_HIGH);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-002
 * @scenario Multiple error flags can be set simultaneously
 * @given The EVSE is initialised with no errors
 * @when evse_set_error_flags is called with TEMP_HIGH then CT_NOCOMM
 * @then Both TEMP_HIGH and CT_NOCOMM bits are set in ErrorFlags
 */
void test_set_multiple_error_flags(void) {
    evse_init(&ctx, NULL);
    evse_set_error_flags(&ctx, TEMP_HIGH);
    evse_set_error_flags(&ctx, CT_NOCOMM);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & TEMP_HIGH) != 0);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & CT_NOCOMM) != 0);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-003
 * @scenario Clearing an error flag removes only the specified flag
 * @given The EVSE has TEMP_HIGH and CT_NOCOMM error flags set
 * @when evse_clear_error_flags is called with TEMP_HIGH
 * @then TEMP_HIGH is cleared but CT_NOCOMM remains set
 */
void test_clear_error_flags(void) {
    evse_init(&ctx, NULL);
    ctx.ErrorFlags = TEMP_HIGH | CT_NOCOMM;
    evse_clear_error_flags(&ctx, TEMP_HIGH);
    TEST_ASSERT_FALSE(ctx.ErrorFlags & TEMP_HIGH);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & CT_NOCOMM) != 0);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-004
 * @scenario Clearing one flag preserves all other active flags
 * @given The EVSE has TEMP_HIGH, LESS_6A, and CT_NOCOMM error flags set
 * @when evse_clear_error_flags is called with LESS_6A
 * @then TEMP_HIGH and CT_NOCOMM remain set, LESS_6A is cleared
 */
void test_clear_preserves_other_flags(void) {
    evse_init(&ctx, NULL);
    ctx.ErrorFlags = TEMP_HIGH | LESS_6A | CT_NOCOMM;
    evse_clear_error_flags(&ctx, LESS_6A);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & TEMP_HIGH) != 0);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & CT_NOCOMM) != 0);
    TEST_ASSERT_FALSE(ctx.ErrorFlags & LESS_6A);
}

// ---- ChargeDelay countdown ----

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-005
 * @scenario ChargeDelay decrements each second
 * @given The EVSE has ChargeDelay set to 10
 * @when One second tick occurs
 * @then ChargeDelay decrements to 9
 */
void test_charge_delay_counts_down(void) {
    evse_init(&ctx, NULL);
    ctx.ChargeDelay = 10;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(9, ctx.ChargeDelay);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-006
 * @scenario ChargeDelay does not underflow past zero
 * @given The EVSE has ChargeDelay set to 1
 * @when Two second ticks occur
 * @then ChargeDelay reaches 0 and stays at 0
 */
void test_charge_delay_stops_at_zero(void) {
    evse_init(&ctx, NULL);
    ctx.ChargeDelay = 1;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(0, ctx.ChargeDelay);
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(0, ctx.ChargeDelay);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-007
 * @scenario Active ChargeDelay diverts A->B transition to STATE_B1
 * @given The EVSE is in STATE_A with ChargeDelay=5 and AccessStatus ON
 * @when A 9V pilot signal is received (vehicle connected)
 * @then The state transitions to STATE_B1 instead of STATE_B
 */
// Original line 3127-3128: ChargeDelay prevents A→B, goes to B1 instead
void test_charge_delay_blocks_A_to_B(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_NORMAL;
    ctx.ChargeDelay = 5;
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B1, ctx.State);
}

// ---- Temperature protection ----

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-008
 * @scenario Temperature exceeding maxTemp triggers TEMP_HIGH error
 * @given The EVSE is charging with TempEVSE=70 and maxTemp=65
 * @when One second tick occurs
 * @then TEMP_HIGH error flag is set in ErrorFlags
 */
void test_temp_high_triggers_error(void) {
    setup_charging();
    ctx.TempEVSE = 70;
    ctx.maxTemp = 65;
    evse_tick_1s(&ctx);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & TEMP_HIGH) != 0);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-009
 * @scenario Overtemperature shuts down active charging session
 * @given The EVSE is in STATE_C (charging) with TempEVSE=70 and maxTemp=65
 * @when One second tick occurs triggering temperature protection
 * @then The state transitions out of STATE_C (charging suspended)
 */
void test_temp_high_shuts_down_charging(void) {
    setup_charging();
    ctx.TempEVSE = 70;
    ctx.maxTemp = 65;
    evse_tick_1s(&ctx);
    // Should have called set_power_unavailable -> C1
    TEST_ASSERT_NOT_EQUAL(STATE_C, ctx.State);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-010
 * @scenario Temperature recovery requires 10-degree hysteresis below maxTemp
 * @given The EVSE has TEMP_HIGH error with maxTemp=65
 * @when Temperature drops to 60 (within hysteresis) then to 50 (below threshold)
 * @then TEMP_HIGH persists at 60 but clears at 50 (below maxTemp-10)
 */
void test_temp_recovery_with_hysteresis(void) {
    evse_init(&ctx, NULL);
    ctx.maxTemp = 65;
    ctx.ErrorFlags = TEMP_HIGH;

    // Still too hot (within 10 degrees)
    ctx.TempEVSE = 60;
    evse_tick_1s(&ctx);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & TEMP_HIGH) != 0);

    // Cooled down enough (below maxTemp - 10)
    ctx.TempEVSE = 50;
    evse_tick_1s(&ctx);
    TEST_ASSERT_FALSE(ctx.ErrorFlags & TEMP_HIGH);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-011
 * @scenario Temperature recovery boundary: exactly at threshold does not clear
 * @given The EVSE has TEMP_HIGH error with maxTemp=65
 * @when Temperature is exactly 55 (maxTemp-10) then drops to 54
 * @then TEMP_HIGH persists at 55 but clears at 54 (strictly below threshold)
 */
void test_temp_recovery_boundary(void) {
    evse_init(&ctx, NULL);
    ctx.maxTemp = 65;
    ctx.ErrorFlags = TEMP_HIGH;

    // Exactly at threshold: 65 - 10 = 55 — NOT below, so should NOT clear
    ctx.TempEVSE = 55;
    evse_tick_1s(&ctx);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & TEMP_HIGH) != 0);

    // One degree below threshold
    ctx.TempEVSE = 54;
    evse_tick_1s(&ctx);
    TEST_ASSERT_FALSE(ctx.ErrorFlags & TEMP_HIGH);
}

// ---- Mains meter timeout ----

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-012
 * @scenario Mains meter communication timeout sets CT_NOCOMM error
 * @given The EVSE is in MODE_SMART with MainsMeterTimeout=0 (timed out)
 * @when One second tick occurs
 * @then CT_NOCOMM error flag is set
 */
void test_mains_meter_timeout_sets_ct_nocomm(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterTimeout = 0;  // Already timed out
    ctx.LoadBl = 0;
    evse_tick_1s(&ctx);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & CT_NOCOMM) != 0);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-013
 * @scenario Mains meter timeout counter decrements each second
 * @given The EVSE is in MODE_SMART with MainsMeterTimeout=5
 * @when One second tick occurs
 * @then MainsMeterTimeout decrements to 4
 */
void test_mains_meter_timeout_counts_down(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterTimeout = 5;
    ctx.LoadBl = 0;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(4, ctx.MainsMeterTimeout);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-014
 * @scenario Normal mode ignores mains meter timeout (no CT_NOCOMM)
 * @given The EVSE is in MODE_NORMAL with MainsMeterTimeout=0
 * @when One second tick occurs
 * @then CT_NOCOMM error flag is NOT set
 */
void test_mains_meter_normal_mode_ignores_timeout(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterTimeout = 0;
    ctx.LoadBl = 0;
    evse_tick_1s(&ctx);
    // Normal mode - meter timeout should not set CT_NOCOMM
    TEST_ASSERT_FALSE(ctx.ErrorFlags & CT_NOCOMM);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-015
 * @scenario No mains meter configured resets timeout to COMM_TIMEOUT
 * @given The EVSE has MainsMeterType=0 (no meter) with MainsMeterTimeout=3
 * @when One second tick occurs
 * @then MainsMeterTimeout is reset to COMM_TIMEOUT
 */
void test_no_mains_meter_resets_timeout(void) {
    evse_init(&ctx, NULL);
    ctx.MainsMeterType = 0;
    ctx.MainsMeterTimeout = 3;
    ctx.LoadBl = 0;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(COMM_TIMEOUT, ctx.MainsMeterTimeout);
}

// ---- EV meter timeout ----

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-016
 * @scenario EV meter communication timeout sets EV_NOCOMM error
 * @given The EVSE has EVMeterType=1 with EVMeterTimeout=0 (timed out)
 * @when One second tick occurs
 * @then EV_NOCOMM error flag is set
 */
void test_ev_meter_timeout_sets_ev_nocomm(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.EVMeterType = 1;
    ctx.EVMeterTimeout = 0;
    evse_tick_1s(&ctx);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & EV_NOCOMM) != 0);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-017
 * @scenario No EV meter configured resets timeout to COMM_EVTIMEOUT
 * @given The EVSE has EVMeterType=0 (no meter) with EVMeterTimeout=3
 * @when One second tick occurs
 * @then EVMeterTimeout is reset to COMM_EVTIMEOUT
 */
void test_no_ev_meter_resets_timeout(void) {
    evse_init(&ctx, NULL);
    ctx.EVMeterType = 0;
    ctx.EVMeterTimeout = 3;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(COMM_EVTIMEOUT, ctx.EVMeterTimeout);
}

// ---- CT_NOCOMM / EV_NOCOMM recovery ----

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-018
 * @scenario CT_NOCOMM error clears when mains meter communication resumes
 * @given The EVSE has CT_NOCOMM error with MainsMeterTimeout=5 (restored)
 * @when One second tick occurs
 * @then CT_NOCOMM error flag is cleared
 */
void test_ct_nocomm_recovers_on_communication(void) {
    evse_init(&ctx, NULL);
    ctx.ErrorFlags = CT_NOCOMM;
    ctx.MainsMeterTimeout = 5;  // Communication restored
    evse_tick_1s(&ctx);
    TEST_ASSERT_FALSE(ctx.ErrorFlags & CT_NOCOMM);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-019
 * @scenario EV_NOCOMM error clears when EV meter communication resumes
 * @given The EVSE has EV_NOCOMM error with EVMeterTimeout=5 (restored)
 * @when One second tick occurs
 * @then EV_NOCOMM error flag is cleared
 */
void test_ev_nocomm_recovers_on_communication(void) {
    evse_init(&ctx, NULL);
    ctx.ErrorFlags = EV_NOCOMM;
    ctx.EVMeterTimeout = 5;  // Communication restored
    evse_tick_1s(&ctx);
    TEST_ASSERT_FALSE(ctx.ErrorFlags & EV_NOCOMM);
}

// ---- LESS_6A auto-recovery ----

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-020
 * @scenario LESS_6A error auto-recovers when sufficient current becomes available
 * @given The EVSE is in MODE_NORMAL standalone with LESS_6A error and AccessStatus ON
 * @when One second tick occurs (normal mode always has current available)
 * @then LESS_6A error flag is cleared
 */
void test_less_6a_recovers_when_current_available(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;
    ctx.ErrorFlags = LESS_6A;
    ctx.AccessStatus = ON;
    evse_tick_1s(&ctx);
    // Normal mode: current is always available
    TEST_ASSERT_FALSE(ctx.ErrorFlags & LESS_6A);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-021
 * @scenario LESS_6A error persists when current is still unavailable
 * @given The EVSE is in MODE_SMART with LESS_6A error and mains heavily loaded
 * @when One second tick occurs
 * @then LESS_6A error flag remains set
 */
void test_less_6a_stays_when_current_unavailable(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.LoadBl = 0;
    ctx.ErrorFlags = LESS_6A;
    ctx.MaxMains = 5;
    ctx.MainsMeterImeasured = 200;  // Way over
    ctx.MinCurrent = 6;
    evse_tick_1s(&ctx);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & LESS_6A) != 0);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-022
 * @scenario Node EVSEs (LoadBl >= 2) do not auto-recover LESS_6A
 * @given The EVSE is configured as a node (LoadBl=3) with LESS_6A error
 * @when One second tick occurs
 * @then LESS_6A error flag remains set (nodes rely on master for recovery)
 */
void test_less_6a_no_recovery_for_nodes(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 3;  // Node
    ctx.ErrorFlags = LESS_6A;
    evse_tick_1s(&ctx);
    // Nodes (LoadBl >= 2) don't auto-recover LESS_6A
    TEST_ASSERT_TRUE((ctx.ErrorFlags & LESS_6A) != 0);
}

// ---- Power unavailable graceful shutdown ----

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-023
 * @scenario Power unavailable during charging suspends to STATE_C1
 * @given The EVSE is in STATE_C (charging)
 * @when evse_set_power_unavailable is called
 * @then The state transitions to STATE_C1 (charging suspended)
 */
void test_power_unavailable_from_C_goes_C1(void) {
    setup_charging();
    evse_set_power_unavailable(&ctx);
    TEST_ASSERT_EQUAL_INT(STATE_C1, ctx.State);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-024
 * @scenario Power unavailable in STATE_B moves to waiting state B1
 * @given The EVSE is in STATE_B (connected)
 * @when evse_set_power_unavailable is called
 * @then The state transitions to STATE_B1 (waiting)
 */
void test_power_unavailable_from_B_goes_B1(void) {
    evse_init(&ctx, NULL);
    ctx.State = STATE_B;
    ctx.BalancedState[0] = STATE_B;
    evse_set_power_unavailable(&ctx);
    TEST_ASSERT_EQUAL_INT(STATE_B1, ctx.State);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-025
 * @scenario Power unavailable in STATE_A has no effect
 * @given The EVSE is in STATE_A (disconnected)
 * @when evse_set_power_unavailable is called
 * @then The state remains STATE_A
 */
void test_power_unavailable_from_A_stays_A(void) {
    evse_init(&ctx, NULL);
    ctx.State = STATE_A;
    evse_set_power_unavailable(&ctx);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-026
 * @scenario Power unavailable in STATE_B1 remains in B1 (already waiting)
 * @given The EVSE is in STATE_B1 (waiting)
 * @when evse_set_power_unavailable is called
 * @then The state remains STATE_B1
 */
void test_power_unavailable_from_B1_stays_B1(void) {
    evse_init(&ctx, NULL);
    evse_set_state(&ctx, STATE_B1);
    evse_set_power_unavailable(&ctx);
    TEST_ASSERT_EQUAL_INT(STATE_B1, ctx.State);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-027
 * @scenario Power unavailable in STATE_C1 remains in C1 (already suspended)
 * @given The EVSE is in STATE_C1 (charging suspended)
 * @when evse_set_power_unavailable is called
 * @then The state remains STATE_C1
 */
void test_power_unavailable_from_C1_stays_C1(void) {
    evse_init(&ctx, NULL);
    evse_set_state(&ctx, STATE_C1);
    evse_set_power_unavailable(&ctx);
    TEST_ASSERT_EQUAL_INT(STATE_C1, ctx.State);
}

// ---- Pilot disconnect/reconnect ----

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-028
 * @scenario Entering STATE_B1 disconnects pilot when authorized
 * @given The EVSE has AccessStatus ON and PilotDisconnected is false
 * @when The state is set to STATE_B1
 * @then PilotDisconnected is true and pilot_connected is false
 */
void test_pilot_disconnect_on_B1_entry(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.PilotDisconnected = false;
    evse_set_state(&ctx, STATE_B1);
    TEST_ASSERT_TRUE(ctx.PilotDisconnected);
    TEST_ASSERT_FALSE(ctx.pilot_connected);
}

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-029
 * @scenario Pilot reconnects after PilotDisconnectTime expires
 * @given The EVSE has PilotDisconnectTime=2 with pilot disconnected
 * @when Two second ticks occur
 * @then PilotDisconnected is cleared and pilot_connected is restored
 */
void test_pilot_reconnect_after_timer(void) {
    evse_init(&ctx, NULL);
    ctx.PilotDisconnectTime = 2;
    ctx.PilotDisconnected = true;
    ctx.pilot_connected = false;

    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(1, ctx.PilotDisconnectTime);
    TEST_ASSERT_TRUE(ctx.PilotDisconnected);  // Still disconnected

    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(0, ctx.PilotDisconnectTime);
    // tick_1s only decrements - reconnect happens in tick_10ms (matches original)
    TEST_ASSERT_TRUE(ctx.PilotDisconnected);  // Still disconnected after tick_1s

    // Reconnect happens on the next tick_10ms when PilotDisconnectTime==0
    ctx.State = STATE_B1;  // Must be in A/COMM_B/B1 for pilot check
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_FALSE(ctx.PilotDisconnected);
    TEST_ASSERT_TRUE(ctx.pilot_connected);
}

// ---- MaxSumMains timer ----

/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-030
 * @scenario MaxSumMains timer expiry stops charging with LESS_6A error
 * @given The EVSE is charging with MaxSumMainsTimer=1 and mains heavily loaded
 * @when One second tick occurs (timer expires)
 * @then The state transitions to STATE_C1 and LESS_6A error flag is set
 */
void test_maxsummains_timer_stops_charging(void) {
    setup_charging();
    ctx.MaxSumMainsTimer = 1;
    // Make current unavailable so LESS_6A persists after auto-recovery check
    ctx.MaxMains = 5;
    ctx.MainsMeterImeasured = 200;
    ctx.MinCurrent = 6;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(0, ctx.MaxSumMainsTimer);
    TEST_ASSERT_EQUAL_INT(STATE_C1, ctx.State);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & LESS_6A) != 0);
}

// ---- Main ----
int main(void) {
    TEST_SUITE_BEGIN("Error Handling & Safety");

    RUN_TEST(test_set_error_flags);
    RUN_TEST(test_set_multiple_error_flags);
    RUN_TEST(test_clear_error_flags);
    RUN_TEST(test_clear_preserves_other_flags);
    RUN_TEST(test_charge_delay_counts_down);
    RUN_TEST(test_charge_delay_stops_at_zero);
    RUN_TEST(test_charge_delay_blocks_A_to_B);
    RUN_TEST(test_temp_high_triggers_error);
    RUN_TEST(test_temp_high_shuts_down_charging);
    RUN_TEST(test_temp_recovery_with_hysteresis);
    RUN_TEST(test_temp_recovery_boundary);
    RUN_TEST(test_mains_meter_timeout_sets_ct_nocomm);
    RUN_TEST(test_mains_meter_timeout_counts_down);
    RUN_TEST(test_mains_meter_normal_mode_ignores_timeout);
    RUN_TEST(test_no_mains_meter_resets_timeout);
    RUN_TEST(test_ev_meter_timeout_sets_ev_nocomm);
    RUN_TEST(test_no_ev_meter_resets_timeout);
    RUN_TEST(test_ct_nocomm_recovers_on_communication);
    RUN_TEST(test_ev_nocomm_recovers_on_communication);
    RUN_TEST(test_less_6a_recovers_when_current_available);
    RUN_TEST(test_less_6a_stays_when_current_unavailable);
    RUN_TEST(test_less_6a_no_recovery_for_nodes);
    RUN_TEST(test_power_unavailable_from_C_goes_C1);
    RUN_TEST(test_power_unavailable_from_B_goes_B1);
    RUN_TEST(test_power_unavailable_from_A_stays_A);
    RUN_TEST(test_power_unavailable_from_B1_stays_B1);
    RUN_TEST(test_power_unavailable_from_C1_stays_C1);
    RUN_TEST(test_pilot_disconnect_on_B1_entry);
    RUN_TEST(test_pilot_reconnect_after_timer);
    RUN_TEST(test_maxsummains_timer_stops_charging);

    TEST_SUITE_RESULTS();
}
