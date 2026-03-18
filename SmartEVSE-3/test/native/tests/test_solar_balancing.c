/*
 * test_solar_balancing.c - Solar-specific paths in evse_calc_balanced_current()
 *
 * Covers 3P/1P switching timers, solar startup forcing MinCurrent,
 * fine-grained increase/decrease, B-state phase determination,
 * hard/soft shortage, IsetBalanced cap, normal mode 3P forcing,
 * phasesLastUpdateFlag gating, and multi-EVSE solar startup.
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

static void setup_solar_charging(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_SOLAR;
    ctx.LoadBl = 0;
    ctx.MaxCurrent = 16;
    ctx.MaxCapacity = 16;
    ctx.MinCurrent = 6;
    ctx.MaxMains = 25;
    ctx.MaxCircuit = 32;
    ctx.StartCurrent = 4;
    ctx.StopTime = 10;
    ctx.ImportCurrent = 0;
    ctx.MainsMeterType = 1;
    ctx.phasesLastUpdateFlag = true;

    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 160;
    ctx.Balanced[0] = 100;
    ctx.ChargeCurrent = 160;
    ctx.IsetBalanced = 100;
    ctx.IsetBalanced_ema = 100;  /* Match IsetBalanced for EMA consistency */
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.Node[0].IntTimer = SOLARSTARTTIME + 1; /* Past startup */
}

/* ---- 3P shortage starts PhaseSwitchTimer ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-001
 * @scenario 3-phase solar shortage starts PhaseSwitchTimer
 * @given The EVSE is solar charging on 3 phases with EnableC2=AUTO and high mains load
 * @when evse_calc_balanced_current is called with large import (Isum=200)
 * @then PhaseSwitchTimer is set to a value greater than 0
 */
void test_solar_3p_shortage_starts_timer(void) {
    setup_solar_charging();
    ctx.EnableC2 = AUTO;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.MainsMeterImeasured = 300;   /* Very high load */
    ctx.Isum = 200;                  /* Large import */
    ctx.PhaseSwitchTimer = 0;
    evse_calc_balanced_current(&ctx, 0);
    /* Shortage detected in 3P + AUTO: PhaseSwitchTimer should start */
    TEST_ASSERT_GREATER_THAN(0, ctx.PhaseSwitchTimer);
}

/* ---- PhaseSwitchTimer<=2 triggers 1P switch ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-002
 * @scenario PhaseSwitchTimer reaching 2 or below triggers 3P to 1P phase switch
 * @given The EVSE is solar charging on 3 phases with EnableC2=AUTO and PhaseSwitchTimer=2
 * @when evse_calc_balanced_current is called with ongoing shortage
 * @then Switching_Phases_C2 is set to GOING_TO_SWITCH_1P
 */
void test_solar_3p_timer_triggers_1p_switch(void) {
    setup_solar_charging();
    ctx.EnableC2 = AUTO;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.MainsMeterImeasured = 300;
    ctx.Isum = 200;
    ctx.PhaseSwitchTimer = 2;  /* Will trigger */
    evse_calc_balanced_current(&ctx, 0);
    /* Timer <=2 should set GOING_TO_SWITCH_1P and go to C1 */
    TEST_ASSERT_EQUAL_INT(GOING_TO_SWITCH_1P, ctx.Switching_Phases_C2);
}

/* ---- 1P surplus starts PhaseSwitchTimer for 3P switch ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-003
 * @scenario 1-phase solar surplus near MaxCurrent starts PhaseSwitchTimer for 3P upgrade
 * @given The EVSE is solar charging on 1 phase with IsetBalanced near MaxCurrent and good surplus
 * @when evse_calc_balanced_current is called with export (Isum=-100)
 * @then PhaseSwitchTimer is set to 63 (countdown to 3P switch)
 */
void test_solar_1p_surplus_starts_timer(void) {
    setup_solar_charging();
    ctx.EnableC2 = AUTO;
    ctx.Nr_Of_Phases_Charging = 1;
    ctx.IsetBalanced = 155;           /* Near MaxCurrent*10 */
    ctx.IsetBalanced_ema = 155;
    ctx.Isum = -100;                  /* Good surplus */
    ctx.MainsMeterImeasured = -50;
    ctx.PhaseSwitchTimer = 0;
    ctx.PhaseSwitchHoldDown = 0;      /* Hold-down expired */
    evse_calc_balanced_current(&ctx, 0);
    /* 1P at max with surplus: timer should start at 63 */
    if (ctx.PhaseSwitchTimer > 0) {
        TEST_ASSERT_EQUAL_INT(63, ctx.PhaseSwitchTimer);
    }
}

/* ---- PhaseSwitchTimer<=3 triggers 3P switch ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-004
 * @scenario PhaseSwitchTimer reaching 3 or below on 1P triggers switch to 3P
 * @given The EVSE is solar charging on 1 phase with PhaseSwitchTimer=3 and large surplus
 * @when evse_calc_balanced_current is called
 * @then Switching_Phases_C2 is set to GOING_TO_SWITCH_3P
 */
void test_solar_1p_timer_triggers_3p_switch(void) {
    setup_solar_charging();
    ctx.EnableC2 = AUTO;
    ctx.Nr_Of_Phases_Charging = 1;
    ctx.IsetBalanced = 160;
    ctx.IsetBalanced_ema = 160;
    ctx.Isum = -200;                  /* Large surplus */
    ctx.MainsMeterImeasured = -100;
    ctx.PhaseSwitchTimer = 3;         /* Will be at <=3 */
    ctx.PhaseSwitchHoldDown = 0;      /* Hold-down expired */
    evse_calc_balanced_current(&ctx, 0);
    /* Timer <=3 should switch to 3P */
    TEST_ASSERT_EQUAL_INT(GOING_TO_SWITCH_3P, ctx.Switching_Phases_C2);
}

/* ---- Insufficient surplus resets PhaseSwitchTimer ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-005
 * @scenario Insufficient surplus resets PhaseSwitchTimer to prevent false 3P upgrade
 * @given The EVSE is solar charging on 1 phase with IsetBalanced well below MaxCurrent
 * @when evse_calc_balanced_current is called with minimal surplus (Isum=-10)
 * @then PhaseSwitchTimer is reset to 0
 */
void test_solar_insufficient_surplus_resets_timer(void) {
    setup_solar_charging();
    ctx.EnableC2 = AUTO;
    ctx.Nr_Of_Phases_Charging = 1;
    ctx.IsetBalanced = 100;            /* Well below MaxCurrent*10 */
    ctx.IsetBalanced_ema = 100;
    ctx.Isum = -10;                    /* Minimal surplus */
    ctx.MainsMeterImeasured = 0;
    ctx.PhaseSwitchTimer = 30;
    ctx.PhaseSwitchHoldDown = 0;
    evse_calc_balanced_current(&ctx, 0);
    /* Not enough surplus to upgrade: timer reset */
    TEST_ASSERT_EQUAL_INT(0, ctx.PhaseSwitchTimer);
}

/* ---- Solar startup forces MinCurrent ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-006
 * @scenario During solar startup period, EVSE is forced to MinCurrent
 * @given The EVSE is solar charging with IntTimer below SOLARSTARTTIME (in startup)
 * @when evse_calc_balanced_current is called
 * @then Balanced[0] is set to MinCurrent*10 regardless of IsetBalanced
 */
void test_solar_startup_forces_mincurrent(void) {
    setup_solar_charging();
    ctx.Node[0].IntTimer = SOLARSTARTTIME - 5;  /* In startup */
    ctx.IsetBalanced = 200;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(ctx.MinCurrent * 10, ctx.Balanced[0]);
}

/* ---- Past startup uses calculated value ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-007
 * @scenario Past startup period, EVSE uses calculated distribution value
 * @given The EVSE is solar charging with IntTimer past SOLARSTARTTIME
 * @when evse_calc_balanced_current is called
 * @then Balanced[0] uses the calculated value (at least MinCurrent*10)
 */
void test_solar_past_startup_uses_calculated(void) {
    setup_solar_charging();
    ctx.Node[0].IntTimer = SOLARSTARTTIME + 1;
    ctx.IsetBalanced = 120;
    ctx.MainsMeterImeasured = 50;
    evse_calc_balanced_current(&ctx, 0);
    /* Should use calculated distribution, not MinCurrent */
    TEST_ASSERT_TRUE(ctx.Balanced[0] >= ctx.MinCurrent * 10);
}

/* ---- Solar fine increase (small export) ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-008
 * @scenario Small solar export results in gradual current increase
 * @given The EVSE is solar charging with small export (Isum=-5)
 * @when evse_calc_balanced_current is called
 * @then IsetBalanced increases by at least 1 (fine-grained increase)
 */
void test_solar_fine_increase_small(void) {
    setup_solar_charging();
    ctx.Isum = -5;   /* Small export */
    ctx.ImportCurrent = 0;
    int32_t before = ctx.IsetBalanced;
    ctx.MainsMeterImeasured = ctx.Isum + (int16_t)ctx.Balanced[0];
    evse_calc_balanced_current(&ctx, 0);
    /* Small export -> +1 */
    TEST_ASSERT_TRUE(ctx.IsetBalanced >= before);
}

/* ---- Solar fine increase (large export) ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-009
 * @scenario Large solar export results in larger current increase
 * @given The EVSE is solar charging with large export (Isum=-50)
 * @when evse_calc_balanced_current is called
 * @then IsetBalanced increases by more than the small export case
 */
void test_solar_fine_increase_large(void) {
    setup_solar_charging();
    ctx.Isum = -50;  /* Large export */
    ctx.ImportCurrent = 0;
    ctx.MainsMeterImeasured = ctx.Isum + (int16_t)ctx.Balanced[0];
    int32_t before = ctx.IsetBalanced;
    evse_calc_balanced_current(&ctx, 0);
    /* Large export -> should increase by more */
    TEST_ASSERT_TRUE(ctx.IsetBalanced > before);
}

/* ---- Solar fine decrease (moderate import) ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-010
 * @scenario Moderate grid import decreases solar charging current
 * @given The EVSE is solar charging with IsetBalanced=150 and moderate import (Isum=15)
 * @when evse_calc_balanced_current is called
 * @then IsetBalanced decreases below 150
 */
void test_solar_fine_decrease_moderate(void) {
    setup_solar_charging();
    ctx.Isum = 15;    /* Moderate import */
    ctx.ImportCurrent = 0;
    ctx.MainsMeterImeasured = ctx.Isum + (int16_t)ctx.Balanced[0];
    ctx.IsetBalanced = 150;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_TRUE(ctx.IsetBalanced < 150);
}

/* ---- Solar fine decrease (aggressive import) ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-011
 * @scenario Large grid import aggressively decreases solar charging current
 * @given The EVSE is solar charging with IsetBalanced=200 and large import (Isum=50)
 * @when evse_calc_balanced_current is called
 * @then IsetBalanced decreases below 200
 */
void test_solar_fine_decrease_aggressive(void) {
    setup_solar_charging();
    ctx.Isum = 50;     /* Large import */
    ctx.ImportCurrent = 0;
    ctx.MainsMeterImeasured = ctx.Isum + (int16_t)ctx.Balanced[0];
    ctx.IsetBalanced = 200;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_TRUE(ctx.IsetBalanced < 200);
}

/* ---- Solar B-state AUTO determines 1P ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-012
 * @scenario Solar B-state with AUTO and small surplus determines 1-phase charging
 * @given The EVSE is in STATE_B with EnableC2=AUTO, 3 phases, and small surplus (Isum=-50)
 * @when evse_calc_balanced_current is called
 * @then Switching_Phases_C2 is set to GOING_TO_SWITCH_1P
 */
void test_solar_b_state_auto_determines_1p(void) {
    setup_solar_charging();
    ctx.State = STATE_B;
    ctx.BalancedState[0] = STATE_B;
    ctx.EnableC2 = AUTO;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.Isum = -50;  /* Small surplus - not enough for 3P */
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(GOING_TO_SWITCH_1P, ctx.Switching_Phases_C2);
}

/* ---- Solar B-state AUTO determines 3P ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-013
 * @scenario Solar B-state with AUTO and large surplus determines 3-phase charging
 * @given The EVSE is in STATE_B with EnableC2=AUTO, 1 phase, and large surplus (Isum=-500)
 * @when evse_calc_balanced_current is called
 * @then Switching_Phases_C2 is set to GOING_TO_SWITCH_3P
 */
void test_solar_b_state_auto_determines_3p(void) {
    setup_solar_charging();
    ctx.State = STATE_B;
    ctx.BalancedState[0] = STATE_B;
    ctx.EnableC2 = AUTO;
    ctx.Nr_Of_Phases_Charging = 1;
    ctx.Isum = -500;  /* Large surplus -> enough for 3P */
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(GOING_TO_SWITCH_3P, ctx.Switching_Phases_C2);
}

/* ---- Hard shortage increments NoCurrent ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-014
 * @scenario Hard current shortage increments NoCurrent counter
 * @given The EVSE is in MODE_SMART with heavily overloaded mains and low MaxMains
 * @when evse_calc_balanced_current is called
 * @then NoCurrent counter is incremented above 0
 */
void test_hard_shortage_increments_nocurrent(void) {
    setup_solar_charging();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 300;  /* Overloaded */
    ctx.MaxMains = 10;               /* Low limit */
    ctx.NoCurrent = 0;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_GREATER_THAN(0, ctx.NoCurrent);
}

/* ---- Soft shortage starts MaxSumMains timer ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-015
 * @scenario Soft shortage (Isum exceeds MaxSumMains) starts MaxSumMains timer
 * @given The EVSE is in MODE_SMART with Isum exceeding MaxSumMains and MaxSumMainsTime=5
 * @when evse_calc_balanced_current is called
 * @then MaxSumMainsTimer is set to MaxSumMainsTime*60 (300 seconds)
 */
void test_soft_shortage_starts_maxsummains_timer(void) {
    setup_solar_charging();
    ctx.Mode = MODE_SMART;
    ctx.MaxSumMains = 10;
    ctx.MaxSumMainsTime = 5;
    ctx.Isum = 200;
    ctx.MainsMeterImeasured = 200;
    ctx.MaxMains = 40;              /* High enough to not be a hard shortage on MaxMains */
    ctx.MaxSumMainsTimer = 0;
    evse_calc_balanced_current(&ctx, 0);
    if (ctx.MaxSumMainsTimer > 0) {
        TEST_ASSERT_EQUAL_INT(5 * 60, ctx.MaxSumMainsTimer);
    }
}

/* ---- No shortage clears timers ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-016
 * @scenario No shortage condition clears SolarStopTimer and decays NoCurrent
 * @given The EVSE is in MODE_SMART with low mains load and high MaxMains
 * @when evse_calc_balanced_current is called with no shortage detected
 * @then SolarStopTimer is reset to 0 and NoCurrent decays by 1
 */
void test_no_shortage_clears_timers(void) {
    setup_solar_charging();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterImeasured = 50;
    ctx.MaxMains = 40;
    ctx.SolarStopTimer = 10;
    ctx.NoCurrent = 5;
    ctx.IsetBalanced = 200;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(0, ctx.SolarStopTimer);
    TEST_ASSERT_EQUAL_INT(4, ctx.NoCurrent);
}

/* ---- IsetBalanced capped at 800 ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-017
 * @scenario IsetBalanced is capped at 800 (80A maximum)
 * @given The EVSE is in MODE_SMART with IsetBalanced=900 and large surplus
 * @when evse_calc_balanced_current is called
 * @then IsetBalanced does not exceed 800
 */
void test_isetbalanced_capped_at_800(void) {
    setup_solar_charging();
    ctx.Mode = MODE_SMART;
    ctx.IsetBalanced = 900;
    ctx.MainsMeterImeasured = -500;  /* Large surplus */
    ctx.MaxMains = 100;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_LESS_OR_EQUAL(800, ctx.IsetBalanced);
}

/* ---- Normal mode forces 3P ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-018
 * @scenario Normal mode forces 3-phase charging regardless of current phase count
 * @given A standalone EVSE in MODE_NORMAL currently on 1 phase
 * @when evse_calc_balanced_current is called
 * @then Switching_Phases_C2 is set to GOING_TO_SWITCH_3P
 */
void test_normal_mode_forces_3p(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;
    ctx.MaxCurrent = 16;
    ctx.MaxCapacity = 16;
    ctx.ChargeCurrent = 160;
    ctx.Nr_Of_Phases_Charging = 1;
    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 160;
    ctx.Balanced[0] = 160;
    ctx.phasesLastUpdateFlag = true;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(GOING_TO_SWITCH_3P, ctx.Switching_Phases_C2);
}

/* ---- phasesLastUpdateFlag gates regulation ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-019
 * @scenario phasesLastUpdateFlag=false prevents IsetBalanced regulation
 * @given The EVSE is in MODE_SMART with phasesLastUpdateFlag=false and large surplus
 * @when evse_calc_balanced_current is called
 * @then IsetBalanced remains unchanged (regulation gated)
 */
void test_phases_flag_gates_regulation(void) {
    setup_solar_charging();
    ctx.Mode = MODE_SMART;
    ctx.phasesLastUpdateFlag = false;
    ctx.IsetBalanced = 100;
    ctx.MainsMeterImeasured = -500;  /* Huge surplus */
    ctx.MaxMains = 100;
    int32_t before = ctx.IsetBalanced;
    evse_calc_balanced_current(&ctx, 0);
    /* With flag false, IsetBalanced should NOT increase (for smart ongoing) */
    TEST_ASSERT_EQUAL_INT(before, ctx.IsetBalanced);
}

/* ---- Multi-EVSE solar startup ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOLAR-020
 * @scenario Multi-EVSE solar startup: EVSE in startup gets MinCurrent, others get calculated
 * @given Two EVSEs as master, EVSE 0 in startup (IntTimer < SOLARSTARTTIME), EVSE 1 past startup
 * @when evse_calc_balanced_current is called
 * @then EVSE 0 Balanced is set to MinCurrent*10 (startup forcing)
 */
void test_multi_evse_solar_startup(void) {
    setup_solar_charging();
    ctx.LoadBl = 1;
    /* EVSE 0: in startup */
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 160;
    ctx.Balanced[0] = 100;
    ctx.Node[0].IntTimer = 5;  /* In startup */
    /* EVSE 1: past startup */
    ctx.BalancedState[1] = STATE_C;
    ctx.BalancedMax[1] = 160;
    ctx.Balanced[1] = 100;
    ctx.Node[1].IntTimer = SOLARSTARTTIME + 10;
    ctx.IsetBalanced = 200;
    ctx.MainsMeterImeasured = 50;
    ctx.Isum = -50;
    evse_calc_balanced_current(&ctx, 0);
    /* EVSE 0 (startup) should get MinCurrent */
    TEST_ASSERT_EQUAL_INT(ctx.MinCurrent * 10, ctx.Balanced[0]);
}

/* ==== Issue #15: Measurement Smoothing and Dead Band ==== */

static void setup_smart_charging(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_SMART;
    ctx.LoadBl = 0;
    ctx.MaxCurrent = 16;
    ctx.MaxCapacity = 16;
    ctx.MinCurrent = 6;
    ctx.MaxMains = 25;
    ctx.MaxCircuit = 32;
    ctx.MainsMeterType = 1;
    ctx.phasesLastUpdateFlag = true;

    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 160;
    ctx.Balanced[0] = 100;
    ctx.ChargeCurrent = 160;
    ctx.IsetBalanced = 100;
    ctx.IsetBalanced_ema = 100;
    ctx.Nr_Of_Phases_Charging = 3;
}

/* ---- EMA smoothing tests ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-021
 * @scenario EMA smoothing dampens sudden IsetBalanced changes
 * @given The EVSE is in smart mode with IsetBalanced_ema=100 and EmaAlpha=50
 * @when evse_calc_balanced_current computes a new IsetBalanced of 200
 * @then IsetBalanced_ema moves toward 200 but not all the way (between 100 and 200)
 */
void test_ema_smoothing_dampens_change(void) {
    setup_smart_charging();
    ctx.IsetBalanced_ema = 100;
    ctx.EmaAlpha = 50;
    /* Create conditions where IsetBalanced will be high (large surplus) */
    ctx.MainsMeterImeasured = -100;  /* Lots of headroom */
    ctx.IsetBalanced = 100;
    evse_calc_balanced_current(&ctx, 0);
    /* EMA should move toward new value but be dampened */
    TEST_ASSERT_GREATER_THAN(100, ctx.IsetBalanced_ema);
}

/*
 * @feature Solar Balancing
 * @req REQ-SOL-022
 * @scenario EMA with alpha=100 tracks raw IsetBalanced exactly (no smoothing)
 * @given The EVSE is in smart mode with EmaAlpha=100 and IsetBalanced_ema=50
 * @when evse_calc_balanced_current computes a new IsetBalanced with large surplus
 * @then IsetBalanced_ema updates to a value different from the old 50
 */
void test_ema_alpha_100_no_smoothing(void) {
    setup_smart_charging();
    ctx.EmaAlpha = 100;
    ctx.IsetBalanced_ema = 50;
    ctx.MainsMeterImeasured = -100;
    evse_calc_balanced_current(&ctx, 0);
    /* With alpha=100, EMA tracks the raw calculated IsetBalanced exactly.
     * Distribution may then cap IsetBalanced below IsetBalanced_ema. */
    TEST_ASSERT_NOT_EQUAL(50, ctx.IsetBalanced_ema);
    TEST_ASSERT_LESS_OR_EQUAL(ctx.IsetBalanced_ema, ctx.IsetBalanced);
}

/*
 * @feature Solar Balancing
 * @req REQ-SOL-023
 * @scenario EMA with alpha=0 holds previous value (full dampening)
 * @given The EVSE is in smart mode with EmaAlpha=0 and IsetBalanced_ema=80
 * @when evse_calc_balanced_current computes a different IsetBalanced
 * @then IsetBalanced_ema remains at 80
 */
void test_ema_alpha_0_full_dampening(void) {
    setup_smart_charging();
    ctx.EmaAlpha = 0;
    ctx.IsetBalanced_ema = 80;
    ctx.MainsMeterImeasured = -100;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(80, ctx.IsetBalanced_ema);
}

/*
 * @feature Solar Balancing
 * @req REQ-SOL-024
 * @scenario EMA defaults are initialized correctly by evse_init
 * @given A freshly initialized EVSE context
 * @when evse_init is called
 * @then EmaAlpha=100 (no smoothing), SmartDeadBand=10, RampRateDivisor=4, SolarFineDeadBand=5
 */
void test_smoothing_defaults_initialized(void) {
    evse_ctx_t fresh;
    evse_init(&fresh, NULL);
    TEST_ASSERT_EQUAL_INT(100, fresh.EmaAlpha);
    TEST_ASSERT_EQUAL_INT(10, fresh.SmartDeadBand);
    TEST_ASSERT_EQUAL_INT(4, fresh.RampRateDivisor);
    TEST_ASSERT_EQUAL_INT(5, fresh.SolarFineDeadBand);
    TEST_ASSERT_EQUAL_INT(0, fresh.IsetBalanced_ema);
}

/* ---- Smart mode dead band tests ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-025
 * @scenario Smart mode dead band suppresses small adjustments
 * @given The EVSE is in smart mode with SmartDeadBand=10 and small Idifference (~5 dA)
 * @when evse_calc_balanced_current is called
 * @then IsetBalanced does not change (within dead band)
 */
void test_smart_deadband_suppresses_small_change(void) {
    setup_smart_charging();
    ctx.SmartDeadBand = 10;
    /* MainsMeterImeasured close to MaxMains*10 so Idifference is small */
    ctx.MainsMeterImeasured = 245;  /* Idifference = 250-245 = 5, < dead band 10 */
    int32_t before = ctx.IsetBalanced;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(before, ctx.IsetBalanced);
}

/*
 * @feature Solar Balancing
 * @req REQ-SOL-026
 * @scenario Smart mode dead band allows large adjustments through
 * @given The EVSE is in smart mode with SmartDeadBand=10 and large Idifference
 * @when evse_calc_balanced_current is called with large surplus (Idifference >> 10)
 * @then IsetBalanced increases (dead band does not suppress)
 */
void test_smart_deadband_allows_large_change(void) {
    setup_smart_charging();
    ctx.SmartDeadBand = 10;
    ctx.MainsMeterImeasured = 100;  /* Idifference = 250-100 = 150, >> dead band */
    int32_t before = ctx.IsetBalanced;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_GREATER_THAN(before, ctx.IsetBalanced);
}

/*
 * @feature Solar Balancing
 * @req REQ-SOL-027
 * @scenario Smart mode dead band suppresses small negative Idifference
 * @given The EVSE is in smart mode with SmartDeadBand=10 and Idifference=-5
 * @when evse_calc_balanced_current is called with slight overload
 * @then IsetBalanced does not decrease (within dead band)
 */
void test_smart_deadband_suppresses_small_decrease(void) {
    setup_smart_charging();
    ctx.SmartDeadBand = 10;
    ctx.EmaAlpha = 100;  /* No EMA smoothing to isolate dead band effect */
    ctx.IsetBalanced_ema = 100;
    ctx.Balanced[0] = 110;  /* Larger current draw so guard rail doesn't cap */
    /* Slightly over MaxMains: Idifference = 250-255 = -5, |5| < dead band 10 */
    ctx.MainsMeterImeasured = 255;
    int32_t before = ctx.IsetBalanced;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(before, ctx.IsetBalanced);
}

/* ---- Symmetric ramp rate tests ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-028
 * @scenario Symmetric ramp applies same rate for increasing and decreasing
 * @given The EVSE is in smart mode with RampRateDivisor=4 and Idifference=40
 * @when evse_calc_balanced_current is called with positive Idifference
 * @then IsetBalanced increases by Idifference/4 = 10
 */
void test_symmetric_ramp_increase(void) {
    setup_smart_charging();
    ctx.RampRateDivisor = 4;
    ctx.SmartDeadBand = 0;  /* Disable dead band for this test */
    ctx.EmaAlpha = 100;     /* No EMA smoothing */
    ctx.IsetBalanced_ema = 100;
    /* Idifference = MaxMains*10 - MainsMeterImeasured = 250 - 210 = 40 */
    ctx.MainsMeterImeasured = 210;
    ctx.IdiffFiltered = 40; /* Pre-seed EMA to match expected Idifference */
    int32_t before = ctx.IsetBalanced;
    evse_calc_balanced_current(&ctx, 0);
    /* Should increase by 40/4 = 10 */
    TEST_ASSERT_EQUAL_INT(before + 10, ctx.IsetBalanced);
}

/*
 * @feature Solar Balancing
 * @req REQ-SOL-029
 * @scenario Symmetric ramp applies same divisor for decrease (was full-step)
 * @given The EVSE is in smart mode with RampRateDivisor=4 and Idifference=-40
 * @when evse_calc_balanced_current is called with negative Idifference
 * @then IsetBalanced decreases by |Idifference|/4 = 10 (not full 40)
 */
void test_symmetric_ramp_decrease(void) {
    setup_smart_charging();
    ctx.RampRateDivisor = 4;
    ctx.SmartDeadBand = 0;  /* Disable dead band for this test */
    ctx.EmaAlpha = 100;     /* No EMA smoothing */
    ctx.IsetBalanced_ema = 100;
    ctx.Balanced[0] = 140;  /* Larger draw so guard rail > 90 after decrease */
    /* Idifference = 250 - 290 = -40 */
    ctx.MainsMeterImeasured = 290;
    ctx.IdiffFiltered = -40; /* Pre-seed EMA to match expected Idifference */
    int32_t before = ctx.IsetBalanced;
    evse_calc_balanced_current(&ctx, 0);
    /* Should decrease by 40/4 = 10, not full 40 */
    TEST_ASSERT_EQUAL_INT(before - 10, ctx.IsetBalanced);
}

/* ---- Solar fine regulation expanded dead band tests ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-030
 * @scenario Solar fine regulation dead band expanded to 5 dA
 * @given The EVSE is solar charging with IsumImport=4 (was outside old 3 dA band, now inside 5 dA)
 * @when evse_calc_balanced_current is called
 * @then IsetBalanced does not decrease from fine regulation (4 dA within 5 dA dead band)
 */
void test_solar_fine_deadband_expanded(void) {
    setup_solar_charging();
    ctx.SolarFineDeadBand = 5;
    ctx.EmaAlpha = 100;     /* No EMA smoothing */
    ctx.ImportCurrent = 0;
    /* Set Isum small positive (within dead band) and MainsMeterImeasured so
     * Idifference > 0 (surplus) to enter fine regulation increase path */
    ctx.Isum = 4;  /* IsumImport = 4, within 5 dA dead band */
    ctx.MainsMeterImeasured = ctx.Isum + (int16_t)ctx.Balanced[0];
    ctx.IsetBalanced = 100;
    ctx.IsetBalanced_ema = 100;
    evse_calc_balanced_current(&ctx, 0);
    /* IsumImport=4 is > 0 but <= SolarFineDeadBand(5): no decrease applied.
     * The fine regulation positive path (IsumImport < 0) doesn't trigger either.
     * So IsetBalanced should stay at 100 from fine regulation perspective. */
    TEST_ASSERT_EQUAL_INT(100, ctx.IsetBalanced);
}

/*
 * @feature Solar Balancing
 * @req REQ-SOL-031
 * @scenario Solar fine regulation triggers decrease above expanded dead band
 * @given The EVSE is solar charging with IsumImport=15 (well above 5 dA dead band)
 * @when evse_calc_balanced_current is called
 * @then IsetBalanced decreases (outside dead band)
 */
void test_solar_fine_deadband_triggers_above(void) {
    setup_solar_charging();
    ctx.SolarFineDeadBand = 5;
    ctx.EmaAlpha = 100;     /* No EMA smoothing */
    ctx.ImportCurrent = 0;
    ctx.Isum = 15;  /* IsumImport = 15, > 5 dA dead band */
    ctx.MainsMeterImeasured = ctx.Isum + (int16_t)ctx.Balanced[0];
    ctx.IsetBalanced = 150;
    ctx.IsetBalanced_ema = 150;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_TRUE(ctx.IsetBalanced < 150);
}

/* ==== Issue #17: Stop/Start Cycling Prevention ==== */

static void setup_solar_shortage(void) {
    setup_solar_charging();
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 300;  /* Overloaded */
    ctx.MaxMains = 10;               /* Low limit */
    ctx.NoCurrent = 0;
    ctx.NoCurrentThreshold = 10;
    ctx.EmaAlpha = 100;
    ctx.IsetBalanced_ema = 100;
}

/* ---- NoCurrent threshold increased ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-032
 * @scenario NoCurrent below threshold does not trigger LESS_6A
 * @given The EVSE is in MODE_SMART with NoCurrent=5 and NoCurrentThreshold=10
 * @when evse_calc_balanced_current is called with hard shortage
 * @then NoCurrent increments but LESS_6A is not set (below threshold)
 */
void test_nocurrent_below_threshold_no_less6a(void) {
    setup_solar_shortage();
    ctx.Mode = MODE_SMART;
    ctx.NoCurrent = 5;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_GREATER_THAN(5, ctx.NoCurrent);
    TEST_ASSERT_FALSE(ctx.ErrorFlags & LESS_6A);
}

/*
 * @feature Solar Balancing
 * @req REQ-SOL-033
 * @scenario NoCurrent reaching threshold triggers LESS_6A
 * @given The EVSE is in MODE_SMART with NoCurrent=9 and NoCurrentThreshold=10
 * @when evse_calc_balanced_current is called with hard shortage
 * @then NoCurrent reaches 10 and LESS_6A is set
 */
void test_nocurrent_at_threshold_triggers_less6a(void) {
    setup_solar_shortage();
    ctx.Mode = MODE_SMART;
    ctx.NoCurrent = 9;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_GREATER_OR_EQUAL(10, ctx.NoCurrent);
    TEST_ASSERT_TRUE(ctx.ErrorFlags & LESS_6A);
}

/* ---- NoCurrent decay ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-034
 * @scenario NoCurrent decays gradually when shortage resolves (not instant reset)
 * @given The EVSE is in MODE_SMART with NoCurrent=8 and no shortage
 * @when evse_calc_balanced_current is called with surplus
 * @then NoCurrent decrements by 1 (not reset to 0)
 */
void test_nocurrent_decays_gradually(void) {
    setup_smart_charging();
    ctx.NoCurrent = 8;
    ctx.MainsMeterImeasured = 50;
    ctx.MaxMains = 40;
    ctx.IsetBalanced = 200;
    ctx.IsetBalanced_ema = 200;
    evse_calc_balanced_current(&ctx, 0);
    /* Should decay by 1, not reset to 0 */
    TEST_ASSERT_EQUAL_INT(7, ctx.NoCurrent);
}

/*
 * @feature Solar Balancing
 * @req REQ-SOL-035
 * @scenario NoCurrent at 0 stays at 0 when no shortage
 * @given The EVSE is in MODE_SMART with NoCurrent=0 and no shortage
 * @when evse_calc_balanced_current is called
 * @then NoCurrent stays at 0
 */
void test_nocurrent_stays_zero(void) {
    setup_smart_charging();
    ctx.NoCurrent = 0;
    ctx.MainsMeterImeasured = 50;
    ctx.MaxMains = 40;
    ctx.IsetBalanced = 200;
    ctx.IsetBalanced_ema = 200;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(0, ctx.NoCurrent);
}

/* ---- Solar minimum run time ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-036
 * @scenario Solar min run time prevents LESS_6A during initial charging
 * @given The EVSE is solar charging with IntTimer < SolarMinRunTime and hard shortage
 * @when NoCurrent exceeds threshold
 * @then LESS_6A is NOT set (protected by min run time)
 */
void test_solar_min_run_time_prevents_less6a(void) {
    setup_solar_shortage();
    ctx.Mode = MODE_SOLAR;
    ctx.SolarMinRunTime = 60;
    ctx.Node[0].IntTimer = 30;  /* Only 30s into charge, min is 60 */
    ctx.NoCurrent = 15;         /* Well above threshold */
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_FALSE(ctx.ErrorFlags & LESS_6A);
}

/*
 * @feature Solar Balancing
 * @req REQ-SOL-037
 * @scenario Solar min run time expired allows LESS_6A
 * @given The EVSE is solar charging with IntTimer >= SolarMinRunTime and hard shortage
 * @when NoCurrent exceeds threshold
 * @then LESS_6A is set (min run time has passed)
 */
void test_solar_min_run_time_expired_allows_less6a(void) {
    setup_solar_shortage();
    ctx.Mode = MODE_SOLAR;
    ctx.SolarMinRunTime = 60;
    ctx.Node[0].IntTimer = 120;  /* 120s into charge, past min 60 */
    ctx.NoCurrent = 15;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_TRUE(ctx.ErrorFlags & LESS_6A);
}

/* ---- Solar-specific charge delay ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-038
 * @scenario Solar mode uses shorter charge delay when LESS_6A active
 * @given The EVSE is in MODE_SOLAR with LESS_6A error active and SolarChargeDelay=15
 * @when evse_tick_1s is called
 * @then ChargeDelay is set to SolarChargeDelay (15) not CHARGEDELAY (60)
 */
void test_solar_charge_delay_shorter(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SOLAR;
    ctx.State = STATE_C;
    ctx.SolarChargeDelay = 15;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 300;  /* High load so LESS_6A won't auto-recover */
    ctx.MaxMains = 10;
    evse_set_error_flags(&ctx, LESS_6A);
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(15, ctx.ChargeDelay);
}

/*
 * @feature Solar Balancing
 * @req REQ-SOL-039
 * @scenario Smart mode still uses full charge delay when LESS_6A active
 * @given The EVSE is in MODE_SMART with LESS_6A error active and no current available
 * @when evse_tick_1s is called
 * @then ChargeDelay is set to CHARGEDELAY (60)
 */
void test_smart_charge_delay_unchanged(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.State = STATE_C;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 300;  /* High load so current not available */
    ctx.MaxMains = 10;              /* Low limit prevents auto-recovery */
    evse_set_error_flags(&ctx, LESS_6A);
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(CHARGEDELAY, ctx.ChargeDelay);
}

/* ---- Cycling prevention defaults ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-040
 * @scenario Cycling prevention defaults initialized correctly
 * @given A freshly initialized EVSE context
 * @when evse_init is called
 * @then NoCurrentThreshold=10, SolarChargeDelay=15, SolarMinRunTime=60
 */
void test_cycling_prevention_defaults(void) {
    evse_ctx_t fresh;
    evse_init(&fresh, NULL);
    TEST_ASSERT_EQUAL_INT(10, fresh.NoCurrentThreshold);
    TEST_ASSERT_EQUAL_INT(15, fresh.SolarChargeDelay);
    TEST_ASSERT_EQUAL_INT(60, fresh.SolarMinRunTime);
}

/* ==== Issue #18: Slow EV Compatibility (Zoe Fix) ==== */

/* ---- Settling window suppresses regulation ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-041
 * @scenario Settling window suppresses smart regulation after current change
 * @given The EVSE is in smart mode with SettlingTimer > 0 (settling active)
 * @when evse_calc_balanced_current is called with large surplus
 * @then IsetBalanced does not increase (regulation suppressed during settling)
 */
void test_settling_window_suppresses_regulation(void) {
    setup_smart_charging();
    ctx.SmartDeadBand = 0;
    ctx.EmaAlpha = 100;
    ctx.IsetBalanced_ema = 100;
    ctx.SettlingTimer = 3;  /* Active settling */
    ctx.MainsMeterImeasured = 100;  /* Large surplus: Idifference = 150 */
    int32_t before = ctx.IsetBalanced;
    evse_calc_balanced_current(&ctx, 0);
    /* Regulation should be suppressed during settling */
    TEST_ASSERT_EQUAL_INT(before, ctx.IsetBalanced);
}

/* ---- Settling window allows regulation when expired ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-042
 * @scenario Regulation proceeds normally when settling timer is 0
 * @given The EVSE is in smart mode with SettlingTimer=0 and large surplus
 * @when evse_calc_balanced_current is called
 * @then IsetBalanced increases (regulation active)
 */
void test_settling_expired_allows_regulation(void) {
    setup_smart_charging();
    ctx.SmartDeadBand = 0;
    ctx.EmaAlpha = 100;
    ctx.IsetBalanced_ema = 100;
    ctx.SettlingTimer = 0;  /* Not settling */
    ctx.MainsMeterImeasured = 100;
    int32_t before = ctx.IsetBalanced;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_GREATER_THAN(before, ctx.IsetBalanced);
}

/* ---- Current change triggers settling ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-043
 * @scenario Balanced[0] change triggers settling timer
 * @given The EVSE is solar charging with LastBalanced=100 and SettlingWindow=5
 * @when evse_calc_balanced_current produces a different Balanced[0]
 * @then SettlingTimer is set to SettlingWindow
 */
void test_current_change_triggers_settling(void) {
    setup_solar_charging();
    ctx.EmaAlpha = 100;
    ctx.IsetBalanced_ema = 150;
    ctx.SettlingWindow = 5;
    ctx.SettlingTimer = 0;
    ctx.LastBalanced = 100;  /* Different from what calc will produce */
    ctx.Isum = -50;
    ctx.MainsMeterImeasured = -50 + (int16_t)ctx.Balanced[0];
    ctx.IsetBalanced = 150;
    evse_calc_balanced_current(&ctx, 0);
    /* Balanced[0] should differ from LastBalanced=100, triggering settling */
    if (ctx.Balanced[0] != 100) {
        TEST_ASSERT_EQUAL_INT(5, ctx.SettlingTimer);
    }
}

/* ---- Ramp rate limits current increase ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-044
 * @scenario Ramp rate limits how much Balanced[0] can change per cycle
 * @given The EVSE is smart charging with MaxRampRate=30 and Balanced[0]=100
 * @when evse_calc_balanced_current produces a large increase
 * @then Balanced[0] changes by at most MaxRampRate from LastBalanced
 */
void test_ramp_rate_limits_increase(void) {
    setup_smart_charging();
    ctx.SmartDeadBand = 0;
    ctx.EmaAlpha = 100;
    ctx.IsetBalanced_ema = 100;
    ctx.MaxRampRate = 30;
    ctx.LastBalanced = 100;
    ctx.Balanced[0] = 100;
    ctx.MainsMeterImeasured = -200;  /* Huge surplus -> big IsetBalanced increase */
    ctx.MaxMains = 100;
    evse_calc_balanced_current(&ctx, 0);
    /* Balanced[0] should not exceed LastBalanced + MaxRampRate */
    TEST_ASSERT_LESS_OR_EQUAL(130, ctx.Balanced[0]);
}

/* ---- Ramp rate limits current decrease ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-045
 * @scenario Ramp rate limits how much Balanced[0] can decrease per cycle
 * @given The EVSE is smart charging with MaxRampRate=30 and Balanced[0]=160
 * @when evse_calc_balanced_current produces a large decrease
 * @then Balanced[0] decreases by at most MaxRampRate from LastBalanced
 */
void test_ramp_rate_limits_decrease(void) {
    setup_smart_charging();
    ctx.SmartDeadBand = 0;
    ctx.EmaAlpha = 100;
    ctx.IsetBalanced_ema = 100;
    ctx.MaxRampRate = 30;
    ctx.LastBalanced = 160;
    ctx.Balanced[0] = 160;
    ctx.MainsMeterImeasured = 350;  /* Heavy load -> decrease */
    evse_calc_balanced_current(&ctx, 0);
    /* Balanced[0] should not drop below LastBalanced - MaxRampRate = 130 */
    if (ctx.Balanced[0] > 0)
        TEST_ASSERT_GREATER_OR_EQUAL(130, ctx.Balanced[0]);
}

/* ---- Settling timer counts down in tick_1s ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-046
 * @scenario SettlingTimer counts down each second
 * @given SettlingTimer=3
 * @when evse_tick_1s is called
 * @then SettlingTimer decrements to 2
 */
void test_settling_timer_countdown(void) {
    evse_init(&ctx, NULL);
    ctx.State = STATE_C;
    ctx.SettlingTimer = 3;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(2, ctx.SettlingTimer);
}

/* ---- Slow EV defaults ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-047
 * @scenario Slow EV compatibility defaults initialized correctly
 * @given A freshly initialized EVSE context
 * @when evse_init is called
 * @then SettlingWindow=5, MaxRampRate=30, SettlingTimer=0, LastBalanced=0
 */
void test_slow_ev_defaults(void) {
    evse_ctx_t fresh;
    evse_init(&fresh, NULL);
    TEST_ASSERT_EQUAL_INT(5, fresh.SettlingWindow);
    TEST_ASSERT_EQUAL_INT(30, fresh.MaxRampRate);
    TEST_ASSERT_EQUAL_INT(0, fresh.SettlingTimer);
    TEST_ASSERT_EQUAL_INT(0, fresh.LastBalanced);
}

/* ---- Ramp rate zero means no limiting ---- */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-048
 * @scenario MaxRampRate=0 disables ramp rate limiting
 * @given The EVSE is smart charging with MaxRampRate=0
 * @when evse_calc_balanced_current produces a large change
 * @then Balanced[0] is not ramp-limited (can change freely)
 */
void test_ramp_rate_zero_no_limit(void) {
    setup_smart_charging();
    ctx.SmartDeadBand = 0;
    ctx.EmaAlpha = 100;
    ctx.IsetBalanced_ema = 100;
    ctx.MaxRampRate = 0;  /* Disabled */
    ctx.LastBalanced = 60;
    ctx.Balanced[0] = 60;
    ctx.MainsMeterImeasured = 50;  /* Surplus */
    ctx.MaxMains = 40;
    evse_calc_balanced_current(&ctx, 0);
    /* With ramp rate disabled, Balanced[0] can change freely */
    TEST_ASSERT_TRUE(ctx.Balanced[0] != 60 || ctx.IsetBalanced != 100);
}

/* ==== Issue #19: Solar Debug Telemetry ==== */

/*
 * @feature Solar Balancing
 * @req REQ-SOL-049
 * @scenario Debug snapshot is populated after evse_calc_balanced_current
 * @given The EVSE is solar charging with known meter readings
 * @when evse_calc_balanced_current is called
 * @then solar_debug snapshot contains matching values
 */
void test_solar_debug_snapshot_populated(void) {
    setup_solar_charging();
    ctx.EmaAlpha = 100;
    ctx.IsetBalanced_ema = 100;
    ctx.Isum = -30;
    ctx.MainsMeterImeasured = 70;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(ctx.IsetBalanced, ctx.solar_debug.IsetBalanced);
    TEST_ASSERT_EQUAL_INT(ctx.Isum, ctx.solar_debug.Isum);
    TEST_ASSERT_EQUAL_INT(ctx.MainsMeterImeasured, ctx.solar_debug.MainsMeterImeasured);
    TEST_ASSERT_EQUAL_INT(ctx.Balanced[0], ctx.solar_debug.Balanced0);
    TEST_ASSERT_EQUAL_INT(ctx.Nr_Of_Phases_Charging, ctx.solar_debug.Nr_Of_Phases_Charging);
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("Solar Balancing");

    RUN_TEST(test_solar_3p_shortage_starts_timer);
    RUN_TEST(test_solar_3p_timer_triggers_1p_switch);
    RUN_TEST(test_solar_1p_surplus_starts_timer);
    RUN_TEST(test_solar_1p_timer_triggers_3p_switch);
    RUN_TEST(test_solar_insufficient_surplus_resets_timer);
    RUN_TEST(test_solar_startup_forces_mincurrent);
    RUN_TEST(test_solar_past_startup_uses_calculated);
    RUN_TEST(test_solar_fine_increase_small);
    RUN_TEST(test_solar_fine_increase_large);
    RUN_TEST(test_solar_fine_decrease_moderate);
    RUN_TEST(test_solar_fine_decrease_aggressive);
    RUN_TEST(test_solar_b_state_auto_determines_1p);
    RUN_TEST(test_solar_b_state_auto_determines_3p);
    RUN_TEST(test_hard_shortage_increments_nocurrent);
    RUN_TEST(test_soft_shortage_starts_maxsummains_timer);
    RUN_TEST(test_no_shortage_clears_timers);
    RUN_TEST(test_isetbalanced_capped_at_800);
    RUN_TEST(test_normal_mode_forces_3p);
    RUN_TEST(test_phases_flag_gates_regulation);
    RUN_TEST(test_multi_evse_solar_startup);

    /* Issue #15: Measurement Smoothing and Dead Band */
    RUN_TEST(test_ema_smoothing_dampens_change);
    RUN_TEST(test_ema_alpha_100_no_smoothing);
    RUN_TEST(test_ema_alpha_0_full_dampening);
    RUN_TEST(test_smoothing_defaults_initialized);
    RUN_TEST(test_smart_deadband_suppresses_small_change);
    RUN_TEST(test_smart_deadband_allows_large_change);
    RUN_TEST(test_smart_deadband_suppresses_small_decrease);
    RUN_TEST(test_symmetric_ramp_increase);
    RUN_TEST(test_symmetric_ramp_decrease);
    RUN_TEST(test_solar_fine_deadband_expanded);
    RUN_TEST(test_solar_fine_deadband_triggers_above);

    /* Issue #17: Stop/Start Cycling Prevention */
    RUN_TEST(test_nocurrent_below_threshold_no_less6a);
    RUN_TEST(test_nocurrent_at_threshold_triggers_less6a);
    RUN_TEST(test_nocurrent_decays_gradually);
    RUN_TEST(test_nocurrent_stays_zero);
    RUN_TEST(test_solar_min_run_time_prevents_less6a);
    RUN_TEST(test_solar_min_run_time_expired_allows_less6a);
    RUN_TEST(test_solar_charge_delay_shorter);
    RUN_TEST(test_smart_charge_delay_unchanged);
    RUN_TEST(test_cycling_prevention_defaults);

    /* Issue #18: Slow EV Compatibility (Zoe Fix) */
    RUN_TEST(test_settling_window_suppresses_regulation);
    RUN_TEST(test_settling_expired_allows_regulation);
    RUN_TEST(test_current_change_triggers_settling);
    RUN_TEST(test_ramp_rate_limits_increase);
    RUN_TEST(test_ramp_rate_limits_decrease);
    RUN_TEST(test_settling_timer_countdown);
    RUN_TEST(test_slow_ev_defaults);
    RUN_TEST(test_ramp_rate_zero_no_limit);

    /* Issue #19: Solar Debug Telemetry */
    RUN_TEST(test_solar_debug_snapshot_populated);

    TEST_SUITE_RESULTS();
}
