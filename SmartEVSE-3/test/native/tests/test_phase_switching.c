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

    /* Phase 1: Trigger shortage -> 3P->1P */
    ctx.MainsMeterImeasured = 300;
    ctx.Isum = 200;
    ctx.SolarStopTimer = 2;  /* About to trigger */
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
    ctx.SolarStopTimer = 3;  /* About to trigger */
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(GOING_TO_SWITCH_3P, ctx.Switching_Phases_C2);

    /* Apply the switch */
    evse_set_state(&ctx, STATE_C);
    TEST_ASSERT_EQUAL_INT(3, ctx.Nr_Of_Phases_Charging);
    TEST_ASSERT_EQUAL_INT(NO_SWITCH, ctx.Switching_Phases_C2);
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

    TEST_SUITE_RESULTS();
}
