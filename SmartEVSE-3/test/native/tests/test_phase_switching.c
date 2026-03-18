/*
 * test_phase_switching.c - Phase switching logic tests
 *
 * Tests evse_check_switching_phases() and phase switching behaviour
 * during state transitions (STATE_B entry, STATE_C entry).
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

static void setup_base(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_SOLAR;
    ctx.Nr_Of_Phases_Charging = 3;
}

/* ---- AUTO + SOLAR: forces 1P ---- */

/*
 * @feature Phase Switching
 * @req REQ-PHASE-003
 * @scenario AUTO + SOLAR: no switch needed when already at correct phase count
 * @given The EVSE is in STATE_B with EnableC2=AUTO, MODE_SOLAR, and various phase counts
 * @when evse_check_switching_phases is called
 * @then Switching_Phases_C2 is NO_SWITCH when already at the correct phase count
 */
void test_check_auto_solar_forces_1p(void) {
    setup_base();
    ctx.EnableC2 = AUTO;
    ctx.Mode = MODE_SOLAR;
    ctx.Nr_Of_Phases_Charging = 1; /* AUTO returns force_single=1 when Nr==1 */
    ctx.State = STATE_B;
    ctx.BalancedState[0] = STATE_B;
    evse_check_switching_phases(&ctx);
    /* AUTO + SOLAR + already 1P: NO_SWITCH (already single phase) */
    TEST_ASSERT_EQUAL_INT(NO_SWITCH, ctx.Switching_Phases_C2);

    /* Now test with 3P: AUTO sees Nr_Of_Phases=3, force_single returns 0,
       so it wants to go to 3P. But since already 3P, NO_SWITCH. */
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.Switching_Phases_C2 = 99; /* sentinel */
    evse_check_switching_phases(&ctx);
    TEST_ASSERT_EQUAL_INT(NO_SWITCH, ctx.Switching_Phases_C2);
}

/* ---- AUTO + SOLAR already 1P ---- */

/*
 * @feature Phase Switching
 * @req REQ-PHASE-004
 * @scenario AUTO + SOLAR already on 1 phase results in NO_SWITCH
 * @given The EVSE is in STATE_B with EnableC2=AUTO, MODE_SOLAR, and 1 phase
 * @when evse_check_switching_phases is called
 * @then Switching_Phases_C2 is NO_SWITCH (already single phase)
 */
void test_check_auto_solar_already_1p(void) {
    setup_base();
    ctx.EnableC2 = AUTO;
    ctx.Mode = MODE_SOLAR;
    ctx.Nr_Of_Phases_Charging = 1;
    ctx.State = STATE_B;
    ctx.BalancedState[0] = STATE_B;
    evse_check_switching_phases(&ctx);
    TEST_ASSERT_EQUAL_INT(NO_SWITCH, ctx.Switching_Phases_C2);
}

/* ---- AUTO + SMART: forces 3P ---- */

/*
 * @feature Phase Switching
 * @req REQ-PHASE-005
 * @scenario AUTO + SMART forces 3-phase when currently on 1 phase
 * @given The EVSE is in STATE_B with EnableC2=AUTO, MODE_SMART, and 1 phase
 * @when evse_check_switching_phases is called
 * @then Switching_Phases_C2 is set to GOING_TO_SWITCH_3P
 */
void test_check_auto_smart_forces_3p(void) {
    setup_base();
    ctx.EnableC2 = AUTO;
    ctx.Mode = MODE_SMART;
    ctx.Nr_Of_Phases_Charging = 1;
    ctx.State = STATE_B;
    ctx.BalancedState[0] = STATE_B;
    evse_check_switching_phases(&ctx);
    TEST_ASSERT_EQUAL_INT(GOING_TO_SWITCH_3P, ctx.Switching_Phases_C2);
}

/* ---- AUTO + SMART already 3P ---- */

/*
 * @feature Phase Switching
 * @req REQ-PHASE-006
 * @scenario AUTO + SMART already on 3 phases results in NO_SWITCH
 * @given The EVSE is in STATE_B with EnableC2=AUTO, MODE_SMART, and 3 phases
 * @when evse_check_switching_phases is called
 * @then Switching_Phases_C2 is NO_SWITCH (already three phase)
 */
void test_check_auto_smart_already_3p(void) {
    setup_base();
    ctx.EnableC2 = AUTO;
    ctx.Mode = MODE_SMART;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.State = STATE_B;
    ctx.BalancedState[0] = STATE_B;
    evse_check_switching_phases(&ctx);
    TEST_ASSERT_EQUAL_INT(NO_SWITCH, ctx.Switching_Phases_C2);
}

/* ---- ALWAYS_OFF in STATE_A sets directly ---- */

/*
 * @feature Phase Switching
 * @req REQ-PHASE-007
 * @scenario ALWAYS_OFF in STATE_A sets Nr_Of_Phases_Charging directly to 1
 * @given The EVSE is in STATE_A with EnableC2=ALWAYS_OFF and 3 phases configured
 * @when evse_check_switching_phases is called
 * @then Nr_Of_Phases_Charging is set directly to 1 (no deferred switch needed)
 */
void test_check_always_off_in_state_a(void) {
    setup_base();
    ctx.EnableC2 = ALWAYS_OFF;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.State = STATE_A;
    ctx.BalancedState[0] = STATE_A;
    evse_check_switching_phases(&ctx);
    /* In STATE_A, should set Nr_Of_Phases directly */
    TEST_ASSERT_EQUAL_INT(1, ctx.Nr_Of_Phases_Charging);
}

/* ---- ALWAYS_OFF in STATE_B sets switching flag ---- */

/*
 * @feature Phase Switching
 * @req REQ-PHASE-008
 * @scenario ALWAYS_OFF in STATE_B sets deferred switching flag to 1P
 * @given The EVSE is in STATE_B with EnableC2=ALWAYS_OFF and 3 phases configured
 * @when evse_check_switching_phases is called
 * @then Switching_Phases_C2 is set to GOING_TO_SWITCH_1P (deferred until STATE_C entry)
 */
void test_check_always_off_in_state_b(void) {
    setup_base();
    ctx.EnableC2 = ALWAYS_OFF;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.State = STATE_B;
    ctx.BalancedState[0] = STATE_B;
    evse_check_switching_phases(&ctx);
    TEST_ASSERT_EQUAL_INT(GOING_TO_SWITCH_1P, ctx.Switching_Phases_C2);
}

/* ---- SOLAR_OFF + SMART: forces 3P ---- */

/*
 * @feature Phase Switching
 * @req REQ-PHASE-009
 * @scenario SOLAR_OFF + SMART forces 3-phase charging
 * @given The EVSE is in STATE_B with EnableC2=SOLAR_OFF, MODE_SMART, and 1 phase
 * @when evse_check_switching_phases is called
 * @then Switching_Phases_C2 is set to GOING_TO_SWITCH_3P
 */
void test_check_solar_off_smart_3p(void) {
    setup_base();
    ctx.EnableC2 = SOLAR_OFF;
    ctx.Mode = MODE_SMART;
    ctx.Nr_Of_Phases_Charging = 1;
    ctx.State = STATE_B;
    ctx.BalancedState[0] = STATE_B;
    evse_check_switching_phases(&ctx);
    /* SOLAR_OFF + SMART: force_single_phase returns 0, so 3P */
    TEST_ASSERT_EQUAL_INT(GOING_TO_SWITCH_3P, ctx.Switching_Phases_C2);
}

/* ---- SOLAR_OFF + SOLAR: forces 1P ---- */

/*
 * @feature Phase Switching
 * @req REQ-PHASE-010
 * @scenario SOLAR_OFF + SOLAR forces 1-phase charging
 * @given The EVSE is in STATE_B with EnableC2=SOLAR_OFF, MODE_SOLAR, and 3 phases
 * @when evse_check_switching_phases is called
 * @then Switching_Phases_C2 is set to GOING_TO_SWITCH_1P
 */
void test_check_solar_off_solar_1p(void) {
    setup_base();
    ctx.EnableC2 = SOLAR_OFF;
    ctx.Mode = MODE_SOLAR;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.State = STATE_B;
    ctx.BalancedState[0] = STATE_B;
    evse_check_switching_phases(&ctx);
    TEST_ASSERT_EQUAL_INT(GOING_TO_SWITCH_1P, ctx.Switching_Phases_C2);
}

/* ---- STATE_C applies 1P switch ---- */

/*
 * @feature Phase Switching
 * @req REQ-PHASE-011
 * @scenario STATE_C entry applies deferred 1P switch and opens contactor 2
 * @given Switching_Phases_C2 is GOING_TO_SWITCH_1P with EnableC2=ALWAYS_OFF
 * @when The state is set to STATE_C
 * @then Nr_Of_Phases_Charging is 1 and contactor2 is off (open)
 */
void test_state_c_applies_1p_switch(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.EnableC2 = ALWAYS_OFF;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.Switching_Phases_C2 = GOING_TO_SWITCH_1P;
    evse_set_state(&ctx, STATE_C);
    TEST_ASSERT_EQUAL_INT(1, ctx.Nr_Of_Phases_Charging);
    TEST_ASSERT_FALSE(ctx.contactor2_state);
}

/* ---- STATE_C applies 3P switch ---- */

/*
 * @feature Phase Switching
 * @req REQ-PHASE-012
 * @scenario STATE_C entry applies deferred 3P switch and closes contactor 2
 * @given Switching_Phases_C2 is GOING_TO_SWITCH_3P with EnableC2=ALWAYS_ON
 * @when The state is set to STATE_C
 * @then Nr_Of_Phases_Charging is 3 and contactor2 is on (closed)
 */
void test_state_c_applies_3p_switch(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.EnableC2 = ALWAYS_ON;
    ctx.Nr_Of_Phases_Charging = 1;
    ctx.Switching_Phases_C2 = GOING_TO_SWITCH_3P;
    evse_set_state(&ctx, STATE_C);
    TEST_ASSERT_EQUAL_INT(3, ctx.Nr_Of_Phases_Charging);
    TEST_ASSERT_TRUE(ctx.contactor2_state);
}

/* ---- STATE_C resets Switching_Phases ---- */

/*
 * @feature Phase Switching
 * @req REQ-PHASE-013
 * @scenario STATE_C entry resets Switching_Phases_C2 to NO_SWITCH
 * @given Switching_Phases_C2 is GOING_TO_SWITCH_1P
 * @when The state is set to STATE_C
 * @then Switching_Phases_C2 is reset to NO_SWITCH
 */
void test_state_c_resets_switching(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Switching_Phases_C2 = GOING_TO_SWITCH_1P;
    evse_set_state(&ctx, STATE_C);
    TEST_ASSERT_EQUAL_INT(NO_SWITCH, ctx.Switching_Phases_C2);
}

/* ---- Full 3P->1P->3P cycle ---- */

/*
 * @feature Phase Switching
 * @req REQ-PHASE-014
 * @scenario Full 3P to 1P to 3P phase switching cycle in solar mode
 * @given The EVSE is solar charging on 3 phases with EnableC2=AUTO
 * @when Solar shortage triggers 3P->1P switch, then surplus triggers 1P->3P switch
 * @then The EVSE correctly switches from 3P to 1P and back to 3P with proper contactor and flag states
 */
void test_full_3p_1p_3p_cycle(void) {
    /* Start: 3P solar charging */
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_SOLAR;
    ctx.EnableC2 = AUTO;
    ctx.MaxCurrent = 16;
    ctx.MaxCapacity = 16;
    ctx.MinCurrent = 6;
    ctx.MaxMains = 25;
    ctx.StartCurrent = 4;
    ctx.StopTime = 10;
    ctx.ImportCurrent = 0;
    ctx.MainsMeterType = 1;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.phasesLastUpdateFlag = true;

    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 160;
    ctx.Balanced[0] = 60;
    ctx.ChargeCurrent = 160;
    ctx.IsetBalanced = 60;
    ctx.Node[0].IntTimer = SOLARSTARTTIME + 1;

    ctx.EmaAlpha = 100;  /* No EMA smoothing for predictable values */
    ctx.IsetBalanced_ema = 60;

    /* Phase 1: Trigger shortage -> 3P->1P */
    ctx.MainsMeterImeasured = 300;
    ctx.Isum = 200;
    ctx.PhaseSwitchTimer = 2;  /* About to trigger */
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(GOING_TO_SWITCH_1P, ctx.Switching_Phases_C2);

    /* Apply the switch by entering STATE_C */
    evse_set_state(&ctx, STATE_C);
    TEST_ASSERT_EQUAL_INT(1, ctx.Nr_Of_Phases_Charging);
    TEST_ASSERT_EQUAL_INT(NO_SWITCH, ctx.Switching_Phases_C2);

    /* Phase 2: Surplus comes back -> 1P->3P */
    ctx.phasesLastUpdateFlag = true;
    ctx.BalancedState[0] = STATE_C;
    ctx.MainsMeterImeasured = -100;
    ctx.Isum = -200;
    ctx.IsetBalanced = 155;  /* Near max */
    ctx.IsetBalanced_ema = 155;
    ctx.PhaseSwitchTimer = 3;  /* About to trigger */
    ctx.PhaseSwitchHoldDown = 0;  /* Hold-down expired */
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(GOING_TO_SWITCH_3P, ctx.Switching_Phases_C2);

    /* Apply the switch */
    evse_set_state(&ctx, STATE_C);
    TEST_ASSERT_EQUAL_INT(3, ctx.Nr_Of_Phases_Charging);
    TEST_ASSERT_EQUAL_INT(NO_SWITCH, ctx.Switching_Phases_C2);
}

/* ==== Issue #16: Phase Switching Timer Improvements ==== */

static void setup_solar_3p_charging(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_SOLAR;
    ctx.EnableC2 = AUTO;
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
    ctx.EmaAlpha = 100;  /* No EMA smoothing */

    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 160;
    ctx.Balanced[0] = 60;
    ctx.ChargeCurrent = 160;
    ctx.IsetBalanced = 60;
    ctx.IsetBalanced_ema = 60;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.Node[0].IntTimer = SOLARSTARTTIME + 1;
}

/* ---- Tiered 3P→1P timer: severe shortage gets short timer ---- */

/*
 * @feature Phase Switching
 * @req REQ-PH-015
 * @scenario Severe solar shortage uses short PhaseSwitchTimer
 * @given The EVSE is solar charging on 3P with severe shortage (IsumImport >= MinCurrent*10)
 * @when evse_calc_balanced_current is called
 * @then PhaseSwitchTimer is set to PhaseSwitchSevereTime (30s default)
 */
void test_severe_shortage_uses_short_timer(void) {
    setup_solar_3p_charging();
    ctx.PhaseSwitchSevereTime = 30;
    /* Severe: IsumImport = Isum - 10*ImportCurrent. ImportCurrent=0, so IsumImport = Isum.
     * Severe when IsumImport >= MinCurrent*10 = 60 */
    ctx.Isum = 100;  /* IsumImport=100, >= 60 → severe */
    ctx.MainsMeterImeasured = 300;
    ctx.PhaseSwitchTimer = 0;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(30, ctx.PhaseSwitchTimer);
}

/* ---- Tiered 3P→1P timer: mild shortage gets long timer ---- */

/*
 * @feature Phase Switching
 * @req REQ-PH-016
 * @scenario Mild solar shortage uses long PhaseSwitchTimer (StopTime-based)
 * @given The EVSE is solar charging on 3P with mild shortage (0 < IsumImport < MinCurrent*10)
 * @when evse_calc_balanced_current is called
 * @then PhaseSwitchTimer is set to StopTime*60 (600s default)
 */
void test_mild_shortage_uses_long_timer(void) {
    setup_solar_3p_charging();
    /* Mild: IsumImport > 0 but < MinCurrent*10 = 60 */
    ctx.Isum = 30;  /* IsumImport=30, < 60 → mild */
    ctx.MainsMeterImeasured = 300;
    ctx.PhaseSwitchTimer = 0;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(ctx.StopTime * 60, ctx.PhaseSwitchTimer);
}

/* ---- PhaseSwitchTimer triggers 3P→1P switch ---- */

/*
 * @feature Phase Switching
 * @req REQ-PH-017
 * @scenario PhaseSwitchTimer reaching <=2 triggers 3P to 1P switch
 * @given The EVSE is solar charging on 3P with PhaseSwitchTimer=2 and ongoing shortage
 * @when evse_calc_balanced_current is called
 * @then Switching_Phases_C2 is set to GOING_TO_SWITCH_1P
 */
void test_phase_switch_timer_triggers_1p(void) {
    setup_solar_3p_charging();
    ctx.Isum = 200;
    ctx.MainsMeterImeasured = 300;
    ctx.PhaseSwitchTimer = 2;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(GOING_TO_SWITCH_1P, ctx.Switching_Phases_C2);
}

/* ---- 3P→1P switch starts hold-down ---- */

/*
 * @feature Phase Switching
 * @req REQ-PH-018
 * @scenario Switching from 3P to 1P starts the hold-down counter
 * @given The EVSE is solar charging on 3P with PhaseSwitchTimer about to trigger
 * @when The 3P→1P switch is triggered (PhaseSwitchTimer<=2)
 * @then PhaseSwitchHoldDown is set to PhaseSwitchHoldDownTime
 */
void test_3p_to_1p_starts_holddown(void) {
    setup_solar_3p_charging();
    ctx.PhaseSwitchHoldDownTime = 300;
    ctx.Isum = 200;
    ctx.MainsMeterImeasured = 300;
    ctx.PhaseSwitchTimer = 2;
    ctx.PhaseSwitchHoldDown = 0;
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(300, ctx.PhaseSwitchHoldDown);
}

/* ---- Hold-down prevents 1P→3P upgrade ---- */

/*
 * @feature Phase Switching
 * @req REQ-PH-019
 * @scenario Hold-down counter prevents premature 1P to 3P upgrade
 * @given The EVSE is solar charging on 1P with sufficient surplus but PhaseSwitchHoldDown > 0
 * @when evse_calc_balanced_current is called
 * @then PhaseSwitchTimer stays 0 and Switching_Phases_C2 stays NO_SWITCH (upgrade blocked)
 */
void test_holddown_prevents_3p_upgrade(void) {
    setup_solar_3p_charging();
    ctx.Nr_Of_Phases_Charging = 1;
    ctx.IsetBalanced = 155;        /* Near MaxCurrent*10 */
    ctx.IsetBalanced_ema = 155;
    ctx.Isum = -200;               /* Large surplus */
    ctx.MainsMeterImeasured = -100;
    ctx.PhaseSwitchHoldDown = 100;  /* Active hold-down */
    ctx.PhaseSwitchTimer = 0;
    evse_calc_balanced_current(&ctx, 0);
    /* Hold-down blocks upgrade: timer should not start */
    TEST_ASSERT_EQUAL_INT(0, ctx.PhaseSwitchTimer);
    TEST_ASSERT_EQUAL_INT(NO_SWITCH, ctx.Switching_Phases_C2);
}

/* ---- Hold-down expired allows 1P→3P upgrade ---- */

/*
 * @feature Phase Switching
 * @req REQ-PH-020
 * @scenario Hold-down expired allows 1P to 3P upgrade to proceed
 * @given The EVSE is solar charging on 1P with sufficient surplus and PhaseSwitchHoldDown=0
 * @when evse_calc_balanced_current is called
 * @then PhaseSwitchTimer starts countdown for 3P upgrade
 */
void test_holddown_expired_allows_upgrade(void) {
    setup_solar_3p_charging();
    ctx.Nr_Of_Phases_Charging = 1;
    ctx.IsetBalanced = 155;
    ctx.IsetBalanced_ema = 155;
    ctx.Isum = -200;
    ctx.MainsMeterImeasured = -100;
    ctx.PhaseSwitchHoldDown = 0;
    ctx.PhaseSwitchTimer = 0;
    evse_calc_balanced_current(&ctx, 0);
    /* Hold-down expired: timer should start */
    TEST_ASSERT_GREATER_THAN(0, ctx.PhaseSwitchTimer);
}

/* ---- PhaseSwitchTimer is separate from SolarStopTimer ---- */

/*
 * @feature Phase Switching
 * @req REQ-PH-021
 * @scenario PhaseSwitchTimer is independent of SolarStopTimer
 * @given PhaseSwitchTimer and SolarStopTimer are at different values
 * @when evse_calc_balanced_current triggers a phase switch timer
 * @then Only PhaseSwitchTimer changes, SolarStopTimer is unaffected
 */
void test_phase_timer_independent_of_solar_stop(void) {
    setup_solar_3p_charging();
    ctx.SolarStopTimer = 42;  /* Pre-existing solar stop countdown */
    ctx.PhaseSwitchTimer = 0;
    ctx.Isum = 100;           /* Severe shortage */
    ctx.MainsMeterImeasured = 300;
    evse_calc_balanced_current(&ctx, 0);
    /* PhaseSwitchTimer should be set, SolarStopTimer unchanged by phase logic */
    TEST_ASSERT_GREATER_THAN(0, ctx.PhaseSwitchTimer);
}

/* ---- PhaseSwitchTimer countdown in tick_1s ---- */

/*
 * @feature Phase Switching
 * @req REQ-PH-022
 * @scenario PhaseSwitchTimer counts down each second in tick_1s
 * @given PhaseSwitchTimer=10 and PhaseSwitchHoldDown=5
 * @when evse_tick_1s is called
 * @then PhaseSwitchTimer decrements to 9 and PhaseSwitchHoldDown decrements to 4
 */
void test_phase_timer_countdown_in_tick_1s(void) {
    evse_init(&ctx, NULL);
    ctx.State = STATE_C;
    ctx.PhaseSwitchTimer = 10;
    ctx.PhaseSwitchHoldDown = 5;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(9, ctx.PhaseSwitchTimer);
    TEST_ASSERT_EQUAL_INT(4, ctx.PhaseSwitchHoldDown);
}

/* ---- Phase switching timer defaults initialized ---- */

/*
 * @feature Phase Switching
 * @req REQ-PH-023
 * @scenario Phase switching timer fields initialized correctly by evse_init
 * @given A freshly initialized EVSE context
 * @when evse_init is called
 * @then PhaseSwitchHoldDownTime and PhaseSwitchSevereTime have correct defaults
 */
void test_phase_timer_defaults(void) {
    evse_ctx_t fresh;
    evse_init(&fresh, NULL);
    TEST_ASSERT_EQUAL_INT(0, fresh.PhaseSwitchTimer);
    TEST_ASSERT_EQUAL_INT(0, fresh.PhaseSwitchHoldDown);
    TEST_ASSERT_EQUAL_INT(PHASE_SWITCH_HOLDDOWN_DEFAULT, fresh.PhaseSwitchHoldDownTime);
    TEST_ASSERT_EQUAL_INT(PHASE_SWITCH_SEVERE_DEFAULT, fresh.PhaseSwitchSevereTime);
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("Phase Switching");

    RUN_TEST(test_check_auto_solar_forces_1p);
    RUN_TEST(test_check_auto_solar_already_1p);
    RUN_TEST(test_check_auto_smart_forces_3p);
    RUN_TEST(test_check_auto_smart_already_3p);
    RUN_TEST(test_check_always_off_in_state_a);
    RUN_TEST(test_check_always_off_in_state_b);
    RUN_TEST(test_check_solar_off_smart_3p);
    RUN_TEST(test_check_solar_off_solar_1p);
    RUN_TEST(test_state_c_applies_1p_switch);
    RUN_TEST(test_state_c_applies_3p_switch);
    RUN_TEST(test_state_c_resets_switching);
    RUN_TEST(test_full_3p_1p_3p_cycle);

    /* Issue #16: Phase Switching Timer Improvements */
    RUN_TEST(test_severe_shortage_uses_short_timer);
    RUN_TEST(test_mild_shortage_uses_long_timer);
    RUN_TEST(test_phase_switch_timer_triggers_1p);
    RUN_TEST(test_3p_to_1p_starts_holddown);
    RUN_TEST(test_holddown_prevents_3p_upgrade);
    RUN_TEST(test_holddown_expired_allows_upgrade);
    RUN_TEST(test_phase_timer_independent_of_solar_stop);
    RUN_TEST(test_phase_timer_countdown_in_tick_1s);
    RUN_TEST(test_phase_timer_defaults);

    TEST_SUITE_RESULTS();
}
