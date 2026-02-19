/*
 * test_dual_evse.c - Dual-EVSE master/slave load balancing scenarios
 *
 * Tests current distribution between two EVSEs in Normal, Smart, and Solar
 * modes with varying power availability, errors, and phase switching.
 *
 * All tests use a single evse_ctx_t configured as master (LoadBl=1) with
 * two EVSE nodes (BalancedState[0..1]).
 */

#include "test_framework.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

static void setup_dual_normal(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 1;              // Master
    ctx.AccessStatus = ON;
    ctx.ModemStage = 1;
    ctx.MaxCurrent = 16;
    ctx.MaxCapacity = 16;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 32;
    ctx.MaxMains = 32;
    ctx.ChargeCurrent = 160;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.EnableC2 = NOT_PRESENT;
    ctx.phasesLastUpdateFlag = true;
}

static void both_charging_at(uint16_t current_each, uint16_t charge_current) {
    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedState[1] = STATE_C;
    ctx.Balanced[0] = current_each;
    ctx.Balanced[1] = current_each;
    ctx.BalancedMax[0] = charge_current;
    ctx.BalancedMax[1] = charge_current;
    ctx.ChargeCurrent = charge_current;
}

// ===================================================================
// S1: Both EVSEs start charging simultaneously (Normal mode)
// ===================================================================

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S1A
 * @scenario Both EVSEs get equal share of MaxCircuit in Normal mode
 * @given Two EVSEs both in STATE_C, Normal mode, MaxCircuit=32A
 * @when evse_calc_balanced_current(mod=1) is called
 * @then Each EVSE gets 160 (16A), equal split of 320 IsetBalanced
 */
void test_s1_both_start_equal_split(void) {
    setup_dual_normal();
    both_charging_at(60, 160);
    ctx.EVMeterImeasured = 0;

    evse_calc_balanced_current(&ctx, 1);

    TEST_ASSERT_EQUAL_INT(160, ctx.Balanced[0]);
    TEST_ASSERT_EQUAL_INT(160, ctx.Balanced[1]);
}

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S1B
 * @scenario IsetBalanced equals MaxCircuit * 10 in Normal mode
 * @given Two EVSEs in Normal mode
 * @when evse_calc_balanced_current(mod=1)
 * @then IsetBalanced = MaxCircuit * 10 = 320
 */
void test_s1_isetbalanced_equals_max_circuit(void) {
    setup_dual_normal();
    both_charging_at(60, 160);
    ctx.EVMeterImeasured = 0;

    evse_calc_balanced_current(&ctx, 1);

    TEST_ASSERT_EQUAL_INT(320, ctx.IsetBalanced);
}

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S1C
 * @scenario EV meter baseload reduces available current
 * @given Two EVSEs, EV meter reads 200 (other loads on circuit)
 * @when evse_calc_balanced_current(mod=1)
 * @then IsetBalanced reduced by baseload
 */
void test_s1_ev_meter_baseload(void) {
    setup_dual_normal();
    ctx.EVMeterType = 1;
    both_charging_at(60, 160);
    ctx.EVMeterImeasured = 200;  // 20A on EV circuit

    evse_calc_balanced_current(&ctx, 1);

    // Baseload_EV = max(0, 200 - 120) = 80
    // IsetBalanced = 320 - 80 = 240
    // Each gets 120 (240 / 2)
    TEST_ASSERT_EQUAL_INT(120, ctx.Balanced[0]);
    TEST_ASSERT_EQUAL_INT(120, ctx.Balanced[1]);
}

// ===================================================================
// S2: Slave joins while master is already charging
// ===================================================================

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S2A
 * @scenario Master reduces current when slave joins
 * @given Master alone at 160, slave not active
 * @when Slave enters STATE_C and calc runs with mod=1
 * @then Both get equal share (160 each with MaxCircuit=32)
 */
void test_s2_slave_joins_master_reduces(void) {
    setup_dual_normal();
    ctx.EVMeterImeasured = 0;

    // Master alone
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedState[1] = STATE_A;
    ctx.Balanced[0] = 160;
    ctx.Balanced[1] = 0;
    ctx.BalancedMax[0] = 160;
    ctx.BalancedMax[1] = 160;

    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(160, ctx.Balanced[0]);  // Master gets full

    // Slave joins
    ctx.BalancedState[1] = STATE_C;
    ctx.Balanced[1] = 60;
    ctx.phasesLastUpdateFlag = true;

    evse_calc_balanced_current(&ctx, 1);
    TEST_ASSERT_EQUAL_INT(160, ctx.Balanced[0]);
    TEST_ASSERT_EQUAL_INT(160, ctx.Balanced[1]);
}

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S2B
 * @scenario Slave node sends COMM_B on car connect
 * @given Slave EVSE (LoadBl=2) in STATE_A with 9V pilot
 * @when tick_10ms with PILOT_9V
 * @then Transitions to STATE_COMM_B (requests master permission)
 */
void test_s2_slave_sends_comm_b(void) {
    evse_init(&ctx, NULL);
    ctx.LoadBl = 2;
    ctx.AccessStatus = ON;
    ctx.ModemStage = 1;
    ctx.MaxCurrent = 16;
    ctx.MaxCapacity = 16;
    ctx.MinCurrent = 6;

    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_COMM_B, ctx.State);
}

// ===================================================================
// S3: Power reduction while both charging
// ===================================================================

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S3A
 * @scenario MaxCircuit reduction redistributes current equally
 * @given Both at 160 with MaxCircuit=32
 * @when MaxCircuit reduced to 20
 * @then Each gets 100 (200 / 2)
 */
void test_s3_maxcircuit_reduction(void) {
    setup_dual_normal();
    both_charging_at(160, 160);
    ctx.EVMeterImeasured = 0;

    ctx.MaxCircuit = 20;
    evse_calc_balanced_current(&ctx, 0);

    // IsetBalanced = 200. Each gets 100.
    TEST_ASSERT_EQUAL_INT(100, ctx.Balanced[0]);
    TEST_ASSERT_EQUAL_INT(100, ctx.Balanced[1]);
}

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S3B
 * @scenario MaxCircuit at exactly 2 * MinCurrent gives each MinCurrent
 * @given Both in STATE_C, MaxCircuit=12
 * @when evse_calc_balanced_current
 * @then Each gets exactly 60 (MinCurrent * 10)
 */
void test_s3_maxcircuit_to_mincurrent(void) {
    setup_dual_normal();
    both_charging_at(60, 160);
    ctx.EVMeterImeasured = 0;

    ctx.MaxCircuit = 12;
    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_EQUAL_INT(60, ctx.Balanced[0]);
    TEST_ASSERT_EQUAL_INT(60, ctx.Balanced[1]);
}

// ===================================================================
// S4: One EVSE disconnects during dual charging
// ===================================================================

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S4A
 * @scenario Slave disconnects, master gets capped at BalancedMax
 * @given Both at 160, slave disconnects (STATE_A)
 * @when Recalculation runs
 * @then Master gets BalancedMax[0]=160, slave gets 0
 */
void test_s4_slave_disconnects(void) {
    setup_dual_normal();
    both_charging_at(160, 160);
    ctx.EVMeterImeasured = 0;

    // Slave disconnects
    ctx.BalancedState[1] = STATE_A;
    ctx.Balanced[1] = 0;
    ctx.phasesLastUpdateFlag = true;

    evse_calc_balanced_current(&ctx, 0);

    // Only 1 active. IsetBalanced=320. ActiveMax=160.
    // Master capped at BalancedMax[0]=160.
    TEST_ASSERT_EQUAL_INT(160, ctx.Balanced[0]);
}

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S4B
 * @scenario Master with higher MaxCurrent gets more after slave disconnect
 * @given Master MaxCurrent=32, MaxCircuit=40, slave disconnects
 * @when Recalculation with mod=0
 * @then Master gets up to 320 (BalancedMax[0]=ChargeCurrent=320)
 */
void test_s4_master_absorbs_full_capacity(void) {
    setup_dual_normal();
    ctx.MaxCurrent = 32;
    ctx.MaxCapacity = 32;
    ctx.MaxCircuit = 40;
    ctx.ChargeCurrent = 320;
    both_charging_at(160, 320);
    ctx.BalancedMax[0] = 320;
    ctx.EVMeterImeasured = 0;

    // Slave disconnects
    ctx.BalancedState[1] = STATE_A;
    ctx.Balanced[1] = 0;
    ctx.phasesLastUpdateFlag = true;

    evse_calc_balanced_current(&ctx, 0);

    // Only 1 active. IsetBalanced = 400. Capped at ActiveMax=320.
    TEST_ASSERT_EQUAL_INT(320, ctx.Balanced[0]);
}

// ===================================================================
// S5: Smart mode with varying grid import
// ===================================================================

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S5A
 * @scenario Smart mode: new EVSE joining recalculates from mains headroom
 * @given Master in MODE_SMART, both in STATE_C, MaxMains=32A
 * @when evse_calc_balanced_current(mod=1)
 * @then IsetBalanced based on (MaxMains*10) - Baseload
 */
void test_s5_smart_mode_new_join(void) {
    setup_dual_normal();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 200;
    ctx.EVMeterImeasured = 0;
    both_charging_at(80, 160);

    evse_calc_balanced_current(&ctx, 1);

    // Baseload = 200 - 160 = 40. IsetBalanced = min(320-40, 320-0) = 280
    // Each gets 140
    TEST_ASSERT_EQUAL_INT(140, ctx.Balanced[0]);
    TEST_ASSERT_EQUAL_INT(140, ctx.Balanced[1]);
}

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S5B
 * @scenario Smart mode: surplus increases IsetBalanced gradually
 * @given MODE_SMART, low mains load, IsetBalanced=200
 * @when evse_calc_balanced_current(mod=0)
 * @then IsetBalanced increases by Idifference/4
 */
void test_s5_smart_surplus_increases(void) {
    setup_dual_normal();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MaxMains = 32;
    both_charging_at(100, 160);
    ctx.EVMeterImeasured = 0;
    ctx.IsetBalanced = 200;
    ctx.MainsMeterImeasured = 100;

    int32_t before = ctx.IsetBalanced;
    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_GREATER_THAN(before, ctx.IsetBalanced);
}

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S5C
 * @scenario Smart mode: overload decreases IsetBalanced immediately
 * @given MODE_SMART, high mains load
 * @when evse_calc_balanced_current(mod=0)
 * @then IsetBalanced decreases by full Idifference
 */
void test_s5_smart_overload_decreases(void) {
    setup_dual_normal();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MaxMains = 32;
    both_charging_at(150, 160);
    ctx.EVMeterImeasured = 0;
    ctx.IsetBalanced = 300;
    ctx.MainsMeterImeasured = 400;

    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_TRUE(ctx.IsetBalanced < 300);
}

// ===================================================================
// S6: Solar mode with both EVSEs
// ===================================================================

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S6A
 * @scenario Solar mode: both EVSEs in startup get MinCurrent
 * @given MODE_SOLAR, both in STATE_C with IntTimer < SOLARSTARTTIME
 * @when evse_calc_balanced_current
 * @then Both receive exactly MinCurrent * 10 = 60
 */
void test_s6_solar_both_in_startup(void) {
    setup_dual_normal();
    ctx.Mode = MODE_SOLAR;
    ctx.MainsMeterType = 1;
    ctx.StartCurrent = 4;
    ctx.StopTime = 10;
    ctx.ImportCurrent = 0;
    both_charging_at(60, 160);
    ctx.Node[0].IntTimer = 5;
    ctx.Node[1].IntTimer = 5;
    ctx.IsetBalanced = 200;
    ctx.MainsMeterImeasured = -100;
    ctx.Isum = -50;

    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_EQUAL_INT(60, ctx.Balanced[0]);
    TEST_ASSERT_EQUAL_INT(60, ctx.Balanced[1]);
}

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S6B
 * @scenario Solar mode: insufficient solar starts SolarStopTimer
 * @given MODE_SOLAR, high grid import, past startup
 * @when evse_calc_balanced_current
 * @then SolarStopTimer is set
 */
void test_s6_solar_insufficient_starts_timer(void) {
    setup_dual_normal();
    ctx.Mode = MODE_SOLAR;
    ctx.MainsMeterType = 1;
    ctx.StartCurrent = 4;
    ctx.StopTime = 10;
    ctx.ImportCurrent = 0;
    ctx.EnableC2 = NOT_PRESENT;
    both_charging_at(60, 160);
    ctx.State = STATE_C;
    ctx.Node[0].IntTimer = SOLARSTARTTIME + 1;
    ctx.Node[1].IntTimer = SOLARSTARTTIME + 1;
    ctx.IsetBalanced = 100;
    ctx.MainsMeterImeasured = 300;
    // Isum must exceed (ActiveEVSE * MinCurrent * Nr_Of_Phases_Charging - StartCurrent) * 10
    // = (2 * 6 * 3 - 4) * 10 = 320 to trigger SolarStopTimer path
    ctx.Isum = 400;
    ctx.SolarStopTimer = 0;

    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_GREATER_THAN(0, (int)ctx.SolarStopTimer);
}

// ===================================================================
// S7: MinCurrent violation / current starvation
// ===================================================================

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S7A
 * @scenario Zero available power pauses all EVSEs via priority scheduling
 * @given Smart mode, MaxMains=5A, heavy mains load (IsetBalanced drops to 0)
 * @when evse_calc_balanced_current
 * @then Both EVSEs paused (Balanced=0), NoCurrent increments (true hard shortage)
 */
void test_s7_mincurrent_violation(void) {
    setup_dual_normal();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MaxMains = 5;
    ctx.MainsMeterImeasured = 200;
    both_charging_at(80, 160);
    ctx.EVMeterImeasured = 0;
    ctx.IsetBalanced = 50;
    ctx.NoCurrent = 0;

    evse_calc_balanced_current(&ctx, 0);

    /* With priority scheduling, available power = 0 (50 + (-150) clamped to 0).
     * Neither EVSE can get MinCurrent, so both are paused. NoCurrent increments. */
    TEST_ASSERT_EQUAL_INT(0, ctx.Balanced[0]);
    TEST_ASSERT_EQUAL_INT(0, ctx.Balanced[1]);
    TEST_ASSERT_GREATER_THAN(0, (int)ctx.NoCurrent);
}

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S7B
 * @scenario Exactly 2 * MinCurrent — no shortage
 * @given Normal mode, MaxCircuit=12
 * @when evse_calc_balanced_current
 * @then Each gets 60, NoCurrent stays 0
 */
void test_s7_barely_enough(void) {
    setup_dual_normal();
    ctx.MaxCircuit = 12;
    both_charging_at(60, 160);
    ctx.EVMeterImeasured = 0;

    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_EQUAL_INT(60, ctx.Balanced[0]);
    TEST_ASSERT_EQUAL_INT(60, ctx.Balanced[1]);
    TEST_ASSERT_EQUAL_INT(0, ctx.NoCurrent);
}

// ===================================================================
// S8: Error on slave during dual charging
// ===================================================================

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S8A
 * @scenario Slave error → master absorbs capacity
 * @given Both at 160 each, slave enters B1
 * @when Recalculation
 * @then Master gets 160 (capped by BalancedMax)
 */
void test_s8_slave_error_master_absorbs(void) {
    setup_dual_normal();
    both_charging_at(160, 160);
    ctx.EVMeterImeasured = 0;

    ctx.BalancedState[1] = STATE_B1;
    ctx.BalancedError[1] = RCM_TRIPPED;
    ctx.Balanced[1] = 0;
    ctx.phasesLastUpdateFlag = true;

    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_EQUAL_INT(160, ctx.Balanced[0]);
}

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S8B
 * @scenario Slave recovers, current redistributed
 * @given Master alone at 160, slave re-enters STATE_C
 * @when evse_calc_balanced_current(mod=1)
 * @then Both get equal share
 */
void test_s8_slave_recovers(void) {
    setup_dual_normal();
    ctx.EVMeterImeasured = 0;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedState[1] = STATE_B1;
    ctx.Balanced[0] = 160;
    ctx.Balanced[1] = 0;
    ctx.BalancedMax[0] = 160;
    ctx.BalancedMax[1] = 160;

    ctx.BalancedState[1] = STATE_C;
    ctx.Balanced[1] = 60;
    ctx.phasesLastUpdateFlag = true;

    evse_calc_balanced_current(&ctx, 1);

    TEST_ASSERT_EQUAL_INT(160, ctx.Balanced[0]);
    TEST_ASSERT_EQUAL_INT(160, ctx.Balanced[1]);
}

// ===================================================================
// S9: MaxSumMains limit enforcement
// ===================================================================

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S9A
 * @scenario MaxSumMains overridden Idifference limits IsetBalanced
 * @given MODE_SMART, MaxSumMains=30, Isum close to limit
 * @when evse_calc_balanced_current(mod=0)
 * @then IsetBalanced constrained by MaxSumMains
 */
void test_s9_maxsummains_limits(void) {
    setup_dual_normal();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MaxMains = 40;
    ctx.MaxSumMains = 30;
    ctx.MaxSumMainsTime = 5;
    both_charging_at(100, 160);
    ctx.EVMeterImeasured = 0;
    ctx.MainsMeterImeasured = 200;
    ctx.Isum = 350;
    ctx.IsetBalanced = 200;

    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_TRUE(ctx.IsetBalanced < 200);
}

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S9B
 * @scenario MaxSumMains timer expiry stops charging
 * @given STATE_C with MaxSumMainsTimer=1
 * @when evse_tick_1s (timer expires)
 * @then C → C1, LESS_6A set
 */
void test_s9_maxsummains_timer_expiry(void) {
    setup_dual_normal();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MaxMains = 25;
    ctx.MaxSumMains = 20;
    ctx.MainsMeterImeasured = 300;       // High mains load — current unavailable
    ctx.Isum = 300;
    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.Balanced[0] = 160;
    ctx.MaxSumMainsTimer = 1;

    evse_tick_1s(&ctx);

    TEST_ASSERT_EQUAL_INT(0, ctx.MaxSumMainsTimer);
    TEST_ASSERT_EQUAL_INT(STATE_C1, ctx.State);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & LESS_6A) != 0);
}

// ===================================================================
// S10: Phase switching during dual charging
// ===================================================================

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S10A
 * @scenario Normal mode forces 3P when currently on 1P
 * @given Normal mode, Nr_Of_Phases_Charging=1
 * @when evse_calc_balanced_current
 * @then Switching_Phases_C2 = GOING_TO_SWITCH_3P
 */
void test_s10_normal_forces_3p(void) {
    setup_dual_normal();
    both_charging_at(80, 160);
    ctx.EVMeterImeasured = 0;
    ctx.Nr_Of_Phases_Charging = 1;

    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_EQUAL_INT(GOING_TO_SWITCH_3P, ctx.Switching_Phases_C2);
}

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S10B
 * @scenario STATE_C entry applies 1P switch
 * @given Switching_Phases_C2=GOING_TO_SWITCH_1P, EnableC2=ALWAYS_OFF
 * @when evse_set_state(STATE_C)
 * @then Nr_Of_Phases_Charging=1, contactor2 off
 */
void test_s10_state_c_applies_1p(void) {
    setup_dual_normal();
    ctx.EnableC2 = ALWAYS_OFF;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.Switching_Phases_C2 = GOING_TO_SWITCH_1P;

    evse_set_state(&ctx, STATE_C);

    TEST_ASSERT_EQUAL_INT(1, ctx.Nr_Of_Phases_Charging);
    TEST_ASSERT_FALSE(ctx.contactor2_state);
}

/*
 * @feature Dual-EVSE Load Balancing
 * @req REQ-DUAL-S10C
 * @scenario Smart mode with AUTO forces back to 3P
 * @given MODE_SMART, EnableC2=AUTO, Nr_Of_Phases_Charging=1
 * @when evse_check_switching_phases
 * @then Switching_Phases_C2 = GOING_TO_SWITCH_3P
 */
void test_s10_smart_auto_forces_3p(void) {
    setup_dual_normal();
    ctx.Mode = MODE_SMART;
    ctx.EnableC2 = AUTO;
    ctx.Nr_Of_Phases_Charging = 1;
    ctx.State = STATE_B;
    ctx.BalancedState[0] = STATE_B;

    evse_check_switching_phases(&ctx);

    TEST_ASSERT_EQUAL_INT(GOING_TO_SWITCH_3P, ctx.Switching_Phases_C2);
}

int main(void) {
    TEST_SUITE_BEGIN("Dual-EVSE Master/Slave Scenarios");

    /* S1: Both start simultaneously */
    RUN_TEST(test_s1_both_start_equal_split);
    RUN_TEST(test_s1_isetbalanced_equals_max_circuit);
    RUN_TEST(test_s1_ev_meter_baseload);

    /* S2: Slave joins */
    RUN_TEST(test_s2_slave_joins_master_reduces);
    RUN_TEST(test_s2_slave_sends_comm_b);

    /* S3: Power reduction */
    RUN_TEST(test_s3_maxcircuit_reduction);
    RUN_TEST(test_s3_maxcircuit_to_mincurrent);

    /* S4: One disconnects */
    RUN_TEST(test_s4_slave_disconnects);
    RUN_TEST(test_s4_master_absorbs_full_capacity);

    /* S5: Smart mode */
    RUN_TEST(test_s5_smart_mode_new_join);
    RUN_TEST(test_s5_smart_surplus_increases);
    RUN_TEST(test_s5_smart_overload_decreases);

    /* S6: Solar mode */
    RUN_TEST(test_s6_solar_both_in_startup);
    RUN_TEST(test_s6_solar_insufficient_starts_timer);

    /* S7: MinCurrent violation */
    RUN_TEST(test_s7_mincurrent_violation);
    RUN_TEST(test_s7_barely_enough);

    /* S8: Error on slave */
    RUN_TEST(test_s8_slave_error_master_absorbs);
    RUN_TEST(test_s8_slave_recovers);

    /* S9: MaxSumMains */
    RUN_TEST(test_s9_maxsummains_limits);
    RUN_TEST(test_s9_maxsummains_timer_expiry);

    /* S10: Phase switching */
    RUN_TEST(test_s10_normal_forces_3p);
    RUN_TEST(test_s10_state_c_applies_1p);
    RUN_TEST(test_s10_smart_auto_forces_3p);

    TEST_SUITE_RESULTS();
}
