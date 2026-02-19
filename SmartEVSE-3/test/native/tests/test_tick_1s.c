/*
 * test_tick_1s.c - Tests for the 1-second timer tick (evse_tick_1s)
 *
 * Covers solar stop timer, charge timers per node, mains meter timeout,
 * LESS_6A enforcement, MaxSumMains timer, access timer, EV meter timeout,
 * activation countdown, and charge delay interactions.
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

static void setup_base(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;
}

/* ---- SolarStopTimer ---- */

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-001
 * @scenario SolarStopTimer decrements by one each second
 * @given EVSE is in normal mode with SolarStopTimer=3
 * @when A 1-second tick occurs
 * @then SolarStopTimer decrements to 2
 */
void test_solar_stop_timer_countdown(void) {
    setup_base();
    ctx.SolarStopTimer = 3;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(2, ctx.SolarStopTimer);
}

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-002
 * @scenario SolarStopTimer expiry triggers STATE_C to STATE_C1 transition
 * @given EVSE is in Smart mode in STATE_C with high mains load and SolarStopTimer=1
 * @when A 1-second tick decrements SolarStopTimer to 0
 * @then The EVSE transitions to STATE_C1 (charging suspended) and LESS_6A error flag is set
 */
void test_solar_stop_timer_triggers_c1(void) {
    setup_base();
    /* Use SMART mode with high load so LESS_6A won't auto-recover */
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 300;
    ctx.MaxMains = 10;
    evse_set_state(&ctx, STATE_C);
    /* Set timer AFTER set_state since STATE_C entry resets SolarStopTimer */
    ctx.SolarStopTimer = 1;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(STATE_C1, ctx.State);
    TEST_ASSERT_TRUE(ctx.ErrorFlags & LESS_6A);
}

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-003
 * @scenario SolarStopTimer expiry does not trigger C1 when not in STATE_C
 * @given EVSE is in Smart mode in STATE_B (not charging) with SolarStopTimer=1
 * @when A 1-second tick decrements SolarStopTimer to 0
 * @then The EVSE does not transition to STATE_C1 but LESS_6A error flag is still set
 */
void test_solar_stop_timer_not_in_c(void) {
    setup_base();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 300;
    ctx.MaxMains = 10;
    evse_set_state(&ctx, STATE_B);
    ctx.SolarStopTimer = 1;
    evse_tick_1s(&ctx);
    /* Not in STATE_C, so no C1 transition, but LESS_6A still set */
    TEST_ASSERT_NOT_EQUAL(STATE_C1, ctx.State);
    TEST_ASSERT_TRUE(ctx.ErrorFlags & LESS_6A);
}

/* ---- Node charge timers ---- */

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-004
 * @scenario Node charge timers increment when node is in STATE_C
 * @given Node 0 is in STATE_C with IntTimer=5 and Timer=100
 * @when A 1-second tick occurs
 * @then IntTimer increments to 6 and Timer increments to 101
 */
void test_node_charge_timer_increments(void) {
    setup_base();
    ctx.BalancedState[0] = STATE_C;
    ctx.Node[0].IntTimer = 5;
    ctx.Node[0].Timer = 100;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(6, ctx.Node[0].IntTimer);
    TEST_ASSERT_EQUAL_INT(101, ctx.Node[0].Timer);
}

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-005
 * @scenario Node charge timer resets when node is not in STATE_C
 * @given Node 0 is in STATE_B (connected but not charging) with IntTimer=20
 * @when A 1-second tick occurs
 * @then IntTimer is reset to 0
 */
void test_node_charge_timer_resets(void) {
    setup_base();
    ctx.BalancedState[0] = STATE_B;
    ctx.Node[0].IntTimer = 20;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(0, ctx.Node[0].IntTimer);
}

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-006
 * @scenario Multiple node charge timers update independently based on each node state
 * @given Nodes 0 and 2 are in STATE_C (charging) and node 1 is in STATE_B, all with IntTimer=10
 * @when A 1-second tick occurs
 * @then Nodes 0 and 2 increment to 11 while node 1 resets to 0
 */
void test_multi_node_timers(void) {
    setup_base();
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedState[1] = STATE_B;
    ctx.BalancedState[2] = STATE_C;
    ctx.Node[0].IntTimer = 10;
    ctx.Node[1].IntTimer = 10;
    ctx.Node[2].IntTimer = 10;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(11, ctx.Node[0].IntTimer);
    TEST_ASSERT_EQUAL_INT(0, ctx.Node[1].IntTimer);
    TEST_ASSERT_EQUAL_INT(11, ctx.Node[2].IntTimer);
}

/* ---- MainsMeter timeout (node) ---- */

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-007
 * @scenario MainsMeter timeout sets CT_NOCOMM error on node
 * @given EVSE is a node (LoadBl=2) with MainsMeterTimeout=0
 * @when A 1-second tick occurs
 * @then CT_NOCOMM error flag is set indicating mains meter communication lost
 */
void test_mains_meter_timeout_node(void) {
    setup_base();
    ctx.LoadBl = 2;
    ctx.MainsMeterTimeout = 0;
    ctx.ErrorFlags = NO_ERROR;
    evse_tick_1s(&ctx);
    TEST_ASSERT_TRUE(ctx.ErrorFlags & CT_NOCOMM);
}

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-008
 * @scenario MainsMeter timeout counter decrements on node each second
 * @given EVSE is a node (LoadBl=3) with MainsMeterTimeout=5
 * @when A 1-second tick occurs
 * @then MainsMeterTimeout decrements to 4
 */
void test_mains_meter_node_countdown(void) {
    setup_base();
    ctx.LoadBl = 3;
    ctx.MainsMeterTimeout = 5;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(4, ctx.MainsMeterTimeout);
}

/* ---- LESS_6A enforcement ---- */

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-009
 * @scenario LESS_6A error forces STATE_C to STATE_C1 via power unavailable
 * @given EVSE is in Smart mode in STATE_C with LESS_6A error flag set and high mains load
 * @when A 1-second tick occurs
 * @then The EVSE transitions to STATE_C1 (charging suspended due to insufficient power)
 */
void test_less_6a_enforces_power_unavailable(void) {
    setup_base();
    /* Use SMART mode with high mains load so LESS_6A can't auto-recover */
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 300;
    ctx.MaxMains = 10;
    evse_set_state(&ctx, STATE_C);
    ctx.ErrorFlags = LESS_6A;
    evse_tick_1s(&ctx);
    /* LESS_6A should cause transition from C to C1 via set_power_unavailable */
    TEST_ASSERT_EQUAL_INT(STATE_C1, ctx.State);
}

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-010
 * @scenario LESS_6A error sets ChargeDelay to CHARGEDELAY (60 seconds)
 * @given EVSE is in Smart mode in STATE_B1 with LESS_6A error flag set and ChargeDelay=0
 * @when A 1-second tick occurs
 * @then ChargeDelay is set to CHARGEDELAY (60 seconds) to prevent rapid retry
 */
void test_less_6a_sets_charge_delay(void) {
    setup_base();
    /* Use SMART mode with high mains load so LESS_6A can't auto-recover */
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 300;
    ctx.MaxMains = 10;
    ctx.ErrorFlags = LESS_6A;
    ctx.ChargeDelay = 0;
    ctx.State = STATE_B1;
    ctx.BalancedState[0] = STATE_B1;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(CHARGEDELAY, ctx.ChargeDelay);
}

/* ---- MaxSumMains timer ---- */

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-011
 * @scenario MaxSumMains timer decrements each second
 * @given EVSE has MaxSumMainsTimer=5
 * @when A 1-second tick occurs
 * @then MaxSumMainsTimer decrements to 4
 */
void test_maxsummains_timer_countdown(void) {
    setup_base();
    ctx.MaxSumMainsTimer = 5;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(4, ctx.MaxSumMainsTimer);
}

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-012
 * @scenario MaxSumMains timer expiry triggers STATE_C to STATE_C1 transition
 * @given EVSE is in Smart mode in STATE_C with high mains load and MaxSumMainsTimer=1
 * @when A 1-second tick decrements MaxSumMainsTimer to 0
 * @then The EVSE transitions to STATE_C1 and LESS_6A error flag is set
 */
void test_maxsummains_timer_triggers_c1(void) {
    setup_base();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 300;
    ctx.MaxMains = 10;
    evse_set_state(&ctx, STATE_C);
    /* Set timer AFTER set_state since STATE_C entry resets MaxSumMainsTimer */
    ctx.MaxSumMainsTimer = 1;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(STATE_C1, ctx.State);
    TEST_ASSERT_TRUE(ctx.ErrorFlags & LESS_6A);
}

/* ---- AccessTimer ---- */

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-013
 * @scenario AccessTimer is cleared when EVSE is not in STATE_A
 * @given EVSE is in STATE_B with AccessTimer=30
 * @when A 1-second tick occurs
 * @then AccessTimer is cleared to 0 because it is only relevant in STATE_A
 */
void test_access_timer_cleared_not_in_a(void) {
    setup_base();
    evse_set_state(&ctx, STATE_B);
    ctx.AccessTimer = 30;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(0, ctx.AccessTimer);
}

/* ---- EVMeter timeout ---- */

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-014
 * @scenario EV meter timeout counter decrements each second
 * @given EVMeterType=1 (meter installed) with EVMeterTimeout=5
 * @when A 1-second tick occurs
 * @then EVMeterTimeout decrements to 4
 */
void test_ev_meter_timeout_countdown(void) {
    setup_base();
    ctx.EVMeterType = 1;
    ctx.EVMeterTimeout = 5;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(4, ctx.EVMeterTimeout);
}

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-015
 * @scenario EV meter timeout reaching zero sets EV_NOCOMM error
 * @given EVMeterType=1 with EVMeterTimeout=0 and no existing errors in Smart mode
 * @when A 1-second tick occurs
 * @then EV_NOCOMM error flag is set indicating EV meter communication lost
 */
void test_ev_meter_timeout_triggers_error(void) {
    setup_base();
    ctx.EVMeterType = 1;
    ctx.EVMeterTimeout = 0;
    ctx.ErrorFlags = NO_ERROR;
    ctx.Mode = MODE_SMART;
    evse_tick_1s(&ctx);
    TEST_ASSERT_TRUE(ctx.ErrorFlags & EV_NOCOMM);
}

/* ---- Activation timer ---- */

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-016
 * @scenario Activation timer decrements each second
 * @given EVSE has ActivationTimer=3
 * @when A 1-second tick occurs
 * @then ActivationTimer decrements to 2
 */
void test_activation_timer_countdown(void) {
    setup_base();
    ctx.ActivationTimer = 3;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(2, ctx.ActivationTimer);
}

/* ---- ActivationMode countdown ---- */

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-017
 * @scenario ActivationMode counter decrements each second
 * @given EVSE has ActivationMode=10
 * @when A 1-second tick occurs
 * @then ActivationMode decrements to 9
 */
void test_activation_mode_countdown(void) {
    setup_base();
    ctx.ActivationMode = 10;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(9, ctx.ActivationMode);
}

/* ---- ChargeDelay overridden by LESS_6A ---- */

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-018
 * @scenario ChargeDelay is overridden by LESS_6A enforcement after decrementing to zero
 * @given EVSE is in Smart mode in STATE_B1 with ChargeDelay=1 and LESS_6A error flag set
 * @when A 1-second tick decrements ChargeDelay to 0 then LESS_6A enforcement re-sets it
 * @then ChargeDelay is set back to CHARGEDELAY (60 seconds) by LESS_6A enforcement
 */
void test_charge_delay_overridden_by_less_6a(void) {
    setup_base();
    /* Use SMART mode with high mains load so LESS_6A can't auto-recover */
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 300;
    ctx.MaxMains = 10;
    ctx.ChargeDelay = 1;  /* Decrements to 0, then LESS_6A enforcement sets CHARGEDELAY */
    ctx.ErrorFlags = LESS_6A;
    ctx.State = STATE_B1;
    ctx.BalancedState[0] = STATE_B1;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(CHARGEDELAY, ctx.ChargeDelay);
}

/* ---- LESS_6A ChargeDelay unconditional reset (F2 fidelity fix) ---- */

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-F2A
 * @scenario LESS_6A resets ChargeDelay to CHARGEDELAY every tick, even when non-zero
 * @given EVSE is in Smart mode in STATE_B1 with LESS_6A set and ChargeDelay=30
 * @when A 1-second tick occurs
 * @then ChargeDelay is reset to CHARGEDELAY (60), not decremented to 29
 */
void test_less_6a_resets_charge_delay_every_tick(void) {
    setup_base();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 300;
    ctx.MaxMains = 10;
    ctx.ErrorFlags = LESS_6A;
    ctx.ChargeDelay = 30;
    ctx.State = STATE_B1;
    ctx.BalancedState[0] = STATE_B1;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(CHARGEDELAY, ctx.ChargeDelay);
}

/*
 * @feature 1-Second Tick Processing
 * @req REQ-TICK1S-F2B
 * @scenario LESS_6A prevents ChargeDelay from ever reaching zero
 * @given EVSE is in Smart mode in STATE_B1 with LESS_6A set and ChargeDelay=1
 * @when A 1-second tick occurs (ChargeDelay decrements to 0, then LESS_6A resets it)
 * @then ChargeDelay is CHARGEDELAY (60), not 0
 */
void test_less_6a_charge_delay_never_reaches_zero(void) {
    setup_base();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 300;
    ctx.MaxMains = 10;
    ctx.ErrorFlags = LESS_6A;
    ctx.ChargeDelay = 1;
    ctx.State = STATE_B1;
    ctx.BalancedState[0] = STATE_B1;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(CHARGEDELAY, ctx.ChargeDelay);
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("Tick 1s");

    RUN_TEST(test_solar_stop_timer_countdown);
    RUN_TEST(test_solar_stop_timer_triggers_c1);
    RUN_TEST(test_solar_stop_timer_not_in_c);
    RUN_TEST(test_node_charge_timer_increments);
    RUN_TEST(test_node_charge_timer_resets);
    RUN_TEST(test_multi_node_timers);
    RUN_TEST(test_mains_meter_timeout_node);
    RUN_TEST(test_mains_meter_node_countdown);
    RUN_TEST(test_less_6a_enforces_power_unavailable);
    RUN_TEST(test_less_6a_sets_charge_delay);
    RUN_TEST(test_maxsummains_timer_countdown);
    RUN_TEST(test_maxsummains_timer_triggers_c1);
    RUN_TEST(test_access_timer_cleared_not_in_a);
    RUN_TEST(test_ev_meter_timeout_countdown);
    RUN_TEST(test_ev_meter_timeout_triggers_error);
    RUN_TEST(test_activation_timer_countdown);
    RUN_TEST(test_activation_mode_countdown);
    RUN_TEST(test_charge_delay_overridden_by_less_6a);
    RUN_TEST(test_less_6a_resets_charge_delay_every_tick);
    RUN_TEST(test_less_6a_charge_delay_never_reaches_zero);

    TEST_SUITE_RESULTS();
}
