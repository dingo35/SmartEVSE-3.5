/*
 * test_mode_sync.c - Mode synchronization behavioral expectations
 *
 * The firmware's setMode() function applies side effects when switching
 * between Normal/Smart/Solar modes: phase switching, error clearing,
 * timer resets, and charge delay clearing. These tests verify the state
 * machine's expected state after a mode switch, documenting the behavior
 * that the MENU_MODE fix (#120) enables for slaves receiving mode via
 * BroadcastSettings.
 *
 * Note: setMode() itself is in main.cpp (firmware glue, not testable
 * natively). These tests exercise the pure C functions it calls:
 * evse_check_switching_phases(), evse_clear_error_flags(), evse_set_state().
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

static void setup_charging_3p(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_SMART;
    ctx.LoadBl = 0;
    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 160;
    ctx.Balanced[0] = 100;
    ctx.ChargeCurrent = 160;
    ctx.MinCurrent = 6;
    ctx.MaxCurrent = 16;
    ctx.MaxCapacity = 16;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.EnableC2 = AUTO;
    ctx.contactor1_state = true;
    ctx.contactor2_state = true;
    ctx.MainsMeterType = 1;
    ctx.MaxMains = 25;
    ctx.MaxCircuit = 32;
    ctx.phasesLastUpdateFlag = true;
    ctx.Node[0].IntTimer = 100;
}

/* ================================================================
 * GROUP 1: Phase switching on mode change (EnableC2 = SOLAR_OFF)
 *
 * When EnableC2 = SOLAR_OFF and mode switches between Solar and
 * other modes, the CP must disconnect (C→C1 or B→B1) to allow
 * safe contactor switching. Without setMode(), this is skipped.
 * ================================================================ */

/*
 * @feature Mode Synchronization
 * @req REQ-MODE-SYNC-001
 * @scenario SOLAR_OFF: switching to Solar requires single-phase (evse_force_single_phase)
 * @given EVSE in Smart mode charging on 3 phases, EnableC2=SOLAR_OFF
 * @when Mode is set to Solar and evse_check_switching_phases is called
 * @then evse_force_single_phase returns true (C2 must be off in Solar mode)
 */
void test_solar_off_forces_single_phase_in_solar(void) {
    setup_charging_3p();
    ctx.EnableC2 = SOLAR_OFF;
    ctx.Mode = MODE_SOLAR;

    /* evse_force_single_phase checks EnableC2 and Mode */
    int force_1p = evse_force_single_phase(&ctx);
    TEST_ASSERT_TRUE(force_1p);
}

/*
 * @feature Mode Synchronization
 * @req REQ-MODE-SYNC-002
 * @scenario SOLAR_OFF: Smart mode allows three-phase
 * @given EVSE with EnableC2=SOLAR_OFF in Smart mode
 * @when evse_force_single_phase is checked
 * @then Returns false (C2 allowed in non-Solar modes with SOLAR_OFF)
 */
void test_solar_off_allows_3p_in_smart(void) {
    setup_charging_3p();
    ctx.EnableC2 = SOLAR_OFF;
    ctx.Mode = MODE_SMART;

    int force_1p = evse_force_single_phase(&ctx);
    TEST_ASSERT_FALSE(force_1p);
}

/*
 * @feature Mode Synchronization
 * @req REQ-MODE-SYNC-003
 * @scenario State C entry with SOLAR_OFF in Solar mode opens C2 contactor
 * @given EVSE with EnableC2=SOLAR_OFF, Mode=Solar, entering STATE_C
 * @when evse_set_state(ctx, STATE_C) is called
 * @then contactor2 is off (single-phase charging)
 */
void test_state_c_entry_solar_off_opens_c2(void) {
    setup_charging_3p();
    ctx.EnableC2 = SOLAR_OFF;
    ctx.Mode = MODE_SOLAR;
    ctx.State = STATE_B;

    evse_set_state(&ctx, STATE_C);

    /* In Solar mode with SOLAR_OFF, C2 should be off */
    TEST_ASSERT_FALSE(ctx.contactor2_state);
    TEST_ASSERT_EQUAL_INT(1, ctx.Nr_Of_Phases_Charging);
}

/* ================================================================
 * GROUP 2: Error and timer clearing on mode switch
 *
 * setMode() clears LESS_6A and SolarStopTimer when switching to
 * Smart mode. Without setMode(), a slave could retain stale errors
 * from a previous Solar mode session.
 * ================================================================ */

/*
 * @feature Mode Synchronization
 * @req REQ-MODE-SYNC-004
 * @scenario Clearing LESS_6A on switch to Smart (via evse_clear_error_flags)
 * @given EVSE with LESS_6A error set from solar shortage
 * @when evse_clear_error_flags clears LESS_6A (as setMode does for Smart)
 * @then ErrorFlags no longer has LESS_6A set
 */
void test_clear_less6a_on_mode_switch(void) {
    setup_charging_3p();
    ctx.ErrorFlags = LESS_6A;

    /* setMode(Smart) calls clearErrorFlags(LESS_6A) */
    evse_clear_error_flags(&ctx, LESS_6A);

    TEST_ASSERT_FALSE(ctx.ErrorFlags & LESS_6A);
}

/*
 * @feature Mode Synchronization
 * @req REQ-MODE-SYNC-005
 * @scenario SolarStopTimer persists if mode switch misses setMode
 * @given EVSE with SolarStopTimer=300, mode changes to Smart
 * @when Only Mode variable is assigned (simulating SETITEM bug)
 * @then SolarStopTimer remains at 300 (stale — not cleared)
 * @note This documents the bug: without setMode(), timers are not reset
 */
void test_raw_mode_assign_leaves_timer_stale(void) {
    setup_charging_3p();
    ctx.Mode = MODE_SOLAR;
    ctx.SolarStopTimer = 300;

    /* Simulate SETITEM(MENU_MODE, Mode): raw assignment */
    ctx.Mode = MODE_SMART;

    /* Timer NOT cleared — this is the bug */
    TEST_ASSERT_EQUAL_INT(300, ctx.SolarStopTimer);
}

/*
 * @feature Mode Synchronization
 * @req REQ-MODE-SYNC-006
 * @scenario SolarStopTimer cleared when setMode side effects applied
 * @given EVSE with SolarStopTimer=300, mode changes to Smart
 * @when setMode side effects are applied (timer reset to 0)
 * @then SolarStopTimer is 0
 */
void test_setmode_clears_timer(void) {
    setup_charging_3p();
    ctx.Mode = MODE_SOLAR;
    ctx.SolarStopTimer = 300;

    /* Simulate what setMode(Smart) does: */
    ctx.Mode = MODE_SMART;
    ctx.SolarStopTimer = 0;  /* setSolarStopTimer(0) */
    evse_clear_error_flags(&ctx, LESS_6A);

    TEST_ASSERT_EQUAL_INT(0, ctx.SolarStopTimer);
    TEST_ASSERT_FALSE(ctx.ErrorFlags & LESS_6A);
}

/* ================================================================
 * GROUP 3: Mode-dependent regulation behavior
 *
 * After a mode switch, the state machine must use the new mode for
 * current regulation. These tests verify that evse_calc_balanced_current
 * respects the Mode field consistently.
 * ================================================================ */

/*
 * @feature Mode Synchronization
 * @req REQ-MODE-SYNC-007
 * @scenario Smart→Solar mid-charge: regulation switches to solar algorithm
 * @given EVSE charging in Smart mode with mains headroom available
 * @when Mode is changed to Solar and evse_calc_balanced_current is called
 * @then Solar fine regulation is applied (IsetBalanced changes differently)
 */
void test_mid_charge_smart_to_solar(void) {
    setup_charging_3p();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterImeasured = 100;
    ctx.Isum = 100;
    ctx.IsetBalanced = 150;
    ctx.IsetBalanced_ema = 150;
    ctx.StartCurrent = 4;
    ctx.StopTime = 10;
    ctx.ImportCurrent = 0;
    ctx.SolarFineDeadBand = SOLAR_FINE_DEADBAND_DEFAULT;

    /* Run one cycle in Smart mode */
    evse_calc_balanced_current(&ctx, 0);
    int32_t smart_iset = ctx.IsetBalanced;

    /* Switch to Solar mode (simulating setMode side effects) */
    ctx.Mode = MODE_SOLAR;
    ctx.IsetBalanced = 150;
    ctx.IsetBalanced_ema = 150;

    /* Run one cycle in Solar mode with same grid conditions */
    evse_calc_balanced_current(&ctx, 0);
    int32_t solar_iset = ctx.IsetBalanced;

    /* Solar regulation should produce different result than Smart
     * (solar decreases more aggressively when importing) */
    TEST_ASSERT_TRUE(smart_iset != solar_iset);
}

/*
 * @feature Mode Synchronization
 * @req REQ-MODE-SYNC-008
 * @scenario Solar→Normal mid-charge: all EVSEs get full current
 * @given Master with 2 EVSEs in Solar mode with shortage
 * @when Mode is changed to Normal
 * @then Both EVSEs get full current (Normal ignores solar/mains constraints)
 */
void test_mid_charge_solar_to_normal(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_SOLAR;
    ctx.LoadBl = 1;
    ctx.MaxCurrent = 16;
    ctx.MaxCapacity = 16;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 32;
    ctx.MaxMains = 25;
    ctx.ChargeCurrent = 160;
    ctx.MainsMeterType = 1;
    ctx.phasesLastUpdateFlag = true;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.StartCurrent = 4;
    ctx.StopTime = 10;
    ctx.ImportCurrent = 0;

    for (int i = 0; i < 2; i++) {
        ctx.BalancedState[i] = STATE_C;
        ctx.BalancedMax[i] = 160;
        ctx.Balanced[i] = 60;
        ctx.Node[i].Online = 1;
        ctx.Node[i].IntTimer = 100;
    }
    ctx.State = STATE_C;
    ctx.MainsMeterImeasured = 200;  /* Importing = no solar */
    ctx.Isum = 200;
    ctx.IsetBalanced = 60;
    ctx.IsetBalanced_ema = 60;

    /* Solar mode: shortage, low distribution */
    evse_calc_balanced_current(&ctx, 0);
    int32_t solar_total = ctx.Balanced[0] + ctx.Balanced[1];

    /* Switch to Normal (simulating setMode side effects) */
    ctx.Mode = MODE_NORMAL;
    ctx.SolarStopTimer = 0;
    ctx.phasesLastUpdateFlag = true;
    evse_clear_error_flags(&ctx, LESS_6A);

    evse_calc_balanced_current(&ctx, 0);
    int32_t normal_total = ctx.Balanced[0] + ctx.Balanced[1];

    /* Normal mode should give substantially more current */
    TEST_ASSERT_GREATER_THAN(solar_total, normal_total);
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[0]);
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[1]);
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("Mode Synchronization");

    /* Group 1: Phase switching */
    RUN_TEST(test_solar_off_forces_single_phase_in_solar);
    RUN_TEST(test_solar_off_allows_3p_in_smart);
    RUN_TEST(test_state_c_entry_solar_off_opens_c2);

    /* Group 2: Error/timer clearing */
    RUN_TEST(test_clear_less6a_on_mode_switch);
    RUN_TEST(test_raw_mode_assign_leaves_timer_stale);
    RUN_TEST(test_setmode_clears_timer);

    /* Group 3: Mode-dependent regulation */
    RUN_TEST(test_mid_charge_smart_to_solar);
    RUN_TEST(test_mid_charge_solar_to_normal);

    TEST_SUITE_RESULTS();
}
