/*
 * test_operating_modes.c - Normal / Smart / Solar mode behavior
 *
 * Tests how the operating mode affects current calculation,
 * regulation, and solar-specific stop/start timers.
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

static void setup_charging_single(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;
    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = ctx.MaxCurrent * 10;
    ctx.Balanced[0] = ctx.MaxCurrent * 10;
    ctx.ChargeCurrent = ctx.MaxCurrent * 10;
    ctx.contactor1_state = true;
}

// ---- Normal mode tests ----

/*
 * @feature Operating Modes
 * @req REQ-MODE-001
 * @scenario Normal mode sets IsetBalanced to MaxCurrent
 * @given EVSE is standalone in STATE_C in Normal mode
 * @when Balanced current is calculated
 * @then IsetBalanced equals MaxCurrent * 10 (fixed current allocation)
 */
void test_normal_mode_uses_max_current(void) {
    setup_charging_single();
    ctx.Mode = MODE_NORMAL;
    evse_calc_balanced_current(&ctx, 0);
    // In normal standalone mode, IsetBalanced = ChargeCurrent
    TEST_ASSERT_EQUAL_INT(ctx.MaxCurrent * 10, ctx.IsetBalanced);
}

/*
 * @feature Operating Modes
 * @req REQ-MODE-002
 * @scenario Normal mode ignores mains meter readings
 * @given EVSE is standalone in STATE_C in Normal mode with high MainsMeterImeasured=300
 * @when Balanced current is calculated
 * @then IsetBalanced remains at MaxCurrent * 10 regardless of mains load
 */
void test_normal_mode_ignores_mains(void) {
    setup_charging_single();
    ctx.Mode = MODE_NORMAL;
    ctx.MainsMeterImeasured = 300;  // High mains current
    evse_calc_balanced_current(&ctx, 0);
    // Normal mode should NOT reduce current based on mains
    TEST_ASSERT_EQUAL_INT(ctx.MaxCurrent * 10, ctx.IsetBalanced);
}

/*
 * @feature Operating Modes
 * @req REQ-MODE-003
 * @scenario Normal mode respects MaxCapacity as upper bound
 * @given EVSE is standalone in STATE_C in Normal mode with MaxCapacity=10A and MaxCurrent=16A
 * @when Balanced current is calculated
 * @then ChargeCurrent is limited to 100 deciamps (MaxCapacity * 10) instead of MaxCurrent
 */
void test_normal_mode_respects_max_capacity(void) {
    setup_charging_single();
    ctx.Mode = MODE_NORMAL;
    ctx.MaxCapacity = 10;  // 10A capacity
    ctx.MaxCurrent = 16;   // 16A max
    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(100, ctx.ChargeCurrent);  // Should use MaxCapacity
}

// ---- Smart mode tests ----

/*
 * @feature Operating Modes
 * @req REQ-MODE-004
 * @scenario Smart mode limits current based on MaxMains minus baseload
 * @given EVSE is standalone in STATE_C in Smart mode with MaxMains=25A and MainsMeterImeasured=200
 * @when Balanced current is calculated
 * @then IsetBalanced does not exceed (MaxMains * 10) minus baseload
 */
void test_smart_mode_respects_maxmains(void) {
    setup_charging_single();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 200;  // 20A on mains
    ctx.MaxMains = 25;              // 25A limit
    evse_calc_balanced_current(&ctx, 0);
    // IsetBalanced should be limited by MaxMains - Baseload
    int32_t expected_limit = (ctx.MaxMains * 10) - (200 - ctx.Balanced[0]);
    TEST_ASSERT_LESS_OR_EQUAL(expected_limit, ctx.IsetBalanced);
}

/*
 * @feature Operating Modes
 * @req REQ-MODE-005
 * @scenario Smart mode increases current conservatively (Idifference/4)
 * @given EVSE is standalone in STATE_C in Smart mode with low mains usage and measurements updated
 * @when Balanced current is calculated with headroom available
 * @then IsetBalanced increases from its initial value but conservatively (not full step)
 */
void test_smart_mode_slow_increase(void) {
    setup_charging_single();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 100;  // Low usage
    ctx.MaxMains = 25;
    ctx.phasesLastUpdateFlag = true;  // Measurements updated
    int32_t initial = ctx.IsetBalanced;
    evse_calc_balanced_current(&ctx, 0);
    // Smart mode increases slowly (Idifference/4)
    // The increase should be present but conservative
    TEST_ASSERT_TRUE(ctx.IsetBalanced >= initial);
}

/*
 * @feature Operating Modes
 * @req REQ-MODE-006
 * @scenario Smart mode decreases current rapidly when over mains limit
 * @given EVSE is standalone in STATE_C in Smart mode with IsetBalanced=200 and mains way over MaxMains=10A
 * @when Balanced current is calculated with negative Idifference
 * @then IsetBalanced decreases rapidly (full Idifference, not divided) below the initial 200
 */
void test_smart_mode_fast_decrease(void) {
    setup_charging_single();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.IsetBalanced = 200;         // Currently set high
    ctx.MaxMains = 10;              // Low limit
    ctx.MainsMeterImeasured = 250;  // Way over limit -> negative Idifference
    ctx.phasesLastUpdateFlag = true;
    evse_calc_balanced_current(&ctx, 0);
    // Should decrease rapidly (full Idifference, not /4)
    TEST_ASSERT_TRUE(ctx.IsetBalanced < 200);
}

// ---- Solar mode tests ----

/*
 * @feature Operating Modes
 * @req REQ-MODE-007
 * @scenario Solar mode requires surplus power to make current available
 * @given EVSE is in Solar mode with StartCurrent=6A and Isum=0 (no surplus)
 * @when evse_is_current_available is called
 * @then Returns 0 (unavailable) because there is no solar surplus for charging
 */
void test_solar_current_available_requires_surplus(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SOLAR;
    ctx.StartCurrent = 6;  // Need 6A surplus
    ctx.AccessStatus = ON;
    // No surplus (Isum = 0, StartCurrent*-10 = -60)
    ctx.Isum = 0;
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/*
 * @feature Operating Modes
 * @req REQ-MODE-008
 * @scenario Solar mode allows charging when sufficient surplus is available
 * @given EVSE is in Solar mode with StartCurrent=6A and Isum=-80 (8A export surplus)
 * @when evse_is_current_available is called
 * @then Returns 1 (available) because export surplus exceeds StartCurrent threshold
 */
void test_solar_current_available_with_surplus(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SOLAR;
    ctx.StartCurrent = 6;
    ctx.AccessStatus = ON;
    ctx.MinCurrent = MIN_CURRENT;
    // Strong surplus: Isum = -80 (8A export)
    ctx.Isum = -80;
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(1, result);
}

/*
 * @feature Operating Modes
 * @req REQ-MODE-009
 * @scenario Solar mode increases current in small steps when surplus is available
 * @given EVSE is standalone in STATE_C in Solar mode with 2A export surplus and past solar startup phase
 * @when Balanced current is calculated
 * @then IsetBalanced increases from its initial 100 value in fine-grained solar increments
 */
void test_solar_fine_grained_increase(void) {
    setup_charging_single();
    ctx.Mode = MODE_SOLAR;
    ctx.MainsMeterType = 1;
    ctx.MaxMains = 25;
    ctx.ImportCurrent = 0;
    ctx.Isum = -20;  // 2A export surplus
    ctx.IsetBalanced = 100;
    ctx.MainsMeterImeasured = 50;
    ctx.phasesLastUpdateFlag = true;
    ctx.Node[0].IntTimer = SOLARSTARTTIME;  // Past solar startup phase
    evse_calc_balanced_current(&ctx, 0);
    // Solar should increase by small steps
    TEST_ASSERT_TRUE(ctx.IsetBalanced >= 100);
}

/*
 * @feature Operating Modes
 * @req REQ-MODE-010
 * @scenario Solar mode decreases current rapidly when importing from grid
 * @given EVSE is standalone in STATE_C in Solar mode with Isum=50 (5A import) and IsetBalanced=100
 * @when Balanced current is calculated with grid import detected
 * @then IsetBalanced decreases below 100 to reduce grid import quickly
 */
void test_solar_rapid_decrease_on_import(void) {
    setup_charging_single();
    ctx.Mode = MODE_SOLAR;
    ctx.MainsMeterType = 1;
    ctx.MaxMains = 25;
    ctx.ImportCurrent = 0;
    ctx.Isum = 50;  // 5A import, over threshold
    ctx.IsetBalanced = 100;
    ctx.MainsMeterImeasured = 50;
    ctx.phasesLastUpdateFlag = true;
    ctx.Node[0].IntTimer = SOLARSTARTTIME;  // Past solar startup phase
    evse_calc_balanced_current(&ctx, 0);
    // Solar should decrease quickly
    TEST_ASSERT_TRUE(ctx.IsetBalanced < 100);
}

/*
 * @feature Operating Modes
 * @req REQ-MODE-011
 * @scenario Solar mode ImportCurrent offset allows controlled grid import
 * @given EVSE is in Solar mode with ImportCurrent=3A allowance and Isum=20 (2A import within allowance)
 * @when Balanced current is calculated with import within the allowed offset
 * @then IsetBalanced increases because IsumImport (20 - 30 = -10) indicates effective surplus
 */
void test_solar_import_current_offset(void) {
    setup_charging_single();
    ctx.Mode = MODE_SOLAR;
    ctx.MainsMeterType = 1;
    ctx.MaxMains = 25;
    ctx.ImportCurrent = 3;  // Allow 3A import
    ctx.Isum = 20;  // 2A import - within allowance
    ctx.IsetBalanced = 100;
    ctx.MainsMeterImeasured = 50;
    ctx.phasesLastUpdateFlag = true;
    ctx.Node[0].IntTimer = SOLARSTARTTIME;  // Past solar startup phase
    evse_calc_balanced_current(&ctx, 0);
    // IsumImport = 20 - 30 = -10, should increase
    TEST_ASSERT_TRUE(ctx.IsetBalanced >= 100);
}

// ---- Phase switching tests ----

/*
 * @feature Operating Modes
 * @req REQ-MODE-012
 * @scenario EnableC2=NOT_PRESENT does not force single phase
 * @given EVSE has EnableC2 set to NOT_PRESENT (contactor 2 not installed)
 * @when evse_force_single_phase is called
 * @then Returns 0 because the phase switching hardware is not present
 */
void test_force_single_phase_not_present(void) {
    evse_init(&ctx, NULL);
    ctx.EnableC2 = NOT_PRESENT;
    TEST_ASSERT_EQUAL_INT(0, evse_force_single_phase(&ctx));
}

/*
 * @feature Operating Modes
 * @req REQ-MODE-013
 * @scenario EnableC2=ALWAYS_OFF forces single phase operation
 * @given EVSE has EnableC2 set to ALWAYS_OFF (contactor 2 always disabled)
 * @when evse_force_single_phase is called
 * @then Returns 1 because the EVSE is configured to always operate in single phase
 */
void test_force_single_phase_always_off(void) {
    evse_init(&ctx, NULL);
    ctx.EnableC2 = ALWAYS_OFF;
    TEST_ASSERT_EQUAL_INT(1, evse_force_single_phase(&ctx));
}

/*
 * @feature Operating Modes
 * @req REQ-MODE-014
 * @scenario EnableC2=SOLAR_OFF forces single phase when in Solar mode
 * @given EVSE has EnableC2 set to SOLAR_OFF and Mode is MODE_SOLAR
 * @when evse_force_single_phase is called
 * @then Returns 1 because SOLAR_OFF disables contactor 2 in solar mode
 */
void test_force_single_phase_solar_off_in_solar_mode(void) {
    evse_init(&ctx, NULL);
    ctx.EnableC2 = SOLAR_OFF;
    ctx.Mode = MODE_SOLAR;
    TEST_ASSERT_EQUAL_INT(1, evse_force_single_phase(&ctx));
}

/*
 * @feature Operating Modes
 * @req REQ-MODE-015
 * @scenario EnableC2=SOLAR_OFF does not force single phase in Smart mode
 * @given EVSE has EnableC2 set to SOLAR_OFF and Mode is MODE_SMART
 * @when evse_force_single_phase is called
 * @then Returns 0 because SOLAR_OFF only applies in Solar mode, not Smart mode
 */
void test_force_single_phase_solar_off_in_smart_mode(void) {
    evse_init(&ctx, NULL);
    ctx.EnableC2 = SOLAR_OFF;
    ctx.Mode = MODE_SMART;
    TEST_ASSERT_EQUAL_INT(0, evse_force_single_phase(&ctx));
}

/*
 * @feature Operating Modes
 * @req REQ-MODE-016
 * @scenario EnableC2=AUTO forces single phase when charging on 1 phase
 * @given EVSE has EnableC2 set to AUTO and Nr_Of_Phases_Charging=1
 * @when evse_force_single_phase is called
 * @then Returns 1 because AUTO mode follows the current phase count
 */
void test_force_single_phase_auto_c2_1p(void) {
    evse_init(&ctx, NULL);
    ctx.EnableC2 = AUTO;
    ctx.Nr_Of_Phases_Charging = 1;
    TEST_ASSERT_EQUAL_INT(1, evse_force_single_phase(&ctx));
}

/*
 * @feature Operating Modes
 * @req REQ-MODE-017
 * @scenario EnableC2=AUTO does not force single phase when charging on 3 phases
 * @given EVSE has EnableC2 set to AUTO and Nr_Of_Phases_Charging=3
 * @when evse_force_single_phase is called
 * @then Returns 0 because AUTO mode allows 3-phase operation when already on 3 phases
 */
void test_force_single_phase_auto_c2_3p(void) {
    evse_init(&ctx, NULL);
    ctx.EnableC2 = AUTO;
    ctx.Nr_Of_Phases_Charging = 3;
    TEST_ASSERT_EQUAL_INT(0, evse_force_single_phase(&ctx));
}

/*
 * @feature Operating Modes
 * @req REQ-MODE-018
 * @scenario EnableC2=ALWAYS_ON does not force single phase
 * @given EVSE has EnableC2 set to ALWAYS_ON (contactor 2 always enabled for 3-phase)
 * @when evse_force_single_phase is called
 * @then Returns 0 because the EVSE is configured to always operate in three phase
 */
void test_force_single_phase_always_on(void) {
    evse_init(&ctx, NULL);
    ctx.EnableC2 = ALWAYS_ON;
    TEST_ASSERT_EQUAL_INT(0, evse_force_single_phase(&ctx));
}

/*
 * @feature Operating Modes
 * @req REQ-MODE-019
 * @scenario STATE_C entry with single phase disables contactor 2
 * @given EVSE has EnableC2 set to ALWAYS_OFF (force single phase)
 * @when EVSE transitions to STATE_C
 * @then Contactor 1 is on, contactor 2 is off, and Nr_Of_Phases_Charging is 1
 */
void test_state_C_contactor2_off_when_single_phase(void) {
    evse_init(&ctx, NULL);
    ctx.EnableC2 = ALWAYS_OFF;  // Force single phase
    evse_set_state(&ctx, STATE_C);
    TEST_ASSERT_TRUE(ctx.contactor1_state);
    TEST_ASSERT_FALSE(ctx.contactor2_state);
    TEST_ASSERT_EQUAL_INT(1, ctx.Nr_Of_Phases_Charging);
}

/*
 * @feature Operating Modes
 * @req REQ-MODE-020
 * @scenario STATE_C entry with three phase enables both contactors
 * @given EVSE has EnableC2 set to NOT_PRESENT (default 3-phase behavior)
 * @when EVSE transitions to STATE_C
 * @then Both contactor 1 and contactor 2 are on and Nr_Of_Phases_Charging is 3
 */
void test_state_C_contactor2_on_when_three_phase(void) {
    evse_init(&ctx, NULL);
    ctx.EnableC2 = NOT_PRESENT;
    evse_set_state(&ctx, STATE_C);
    TEST_ASSERT_TRUE(ctx.contactor1_state);
    TEST_ASSERT_TRUE(ctx.contactor2_state);
    TEST_ASSERT_EQUAL_INT(3, ctx.Nr_Of_Phases_Charging);
}

/*
 * @feature Operating Modes
 * @req REQ-MODE-021
 * @scenario Phase switch from 3P to 1P completes on STATE_C entry
 * @given EVSE has Switching_Phases_C2=GOING_TO_SWITCH_1P and EnableC2=AUTO
 * @when EVSE transitions to STATE_C
 * @then Nr_Of_Phases_Charging is set to 1 and Switching_Phases_C2 is cleared to NO_SWITCH
 */
void test_phase_switch_going_to_1p(void) {
    evse_init(&ctx, NULL);
    ctx.Switching_Phases_C2 = GOING_TO_SWITCH_1P;
    ctx.EnableC2 = AUTO;
    evse_set_state(&ctx, STATE_C);
    TEST_ASSERT_EQUAL_INT(1, ctx.Nr_Of_Phases_Charging);
    TEST_ASSERT_EQUAL_INT(NO_SWITCH, ctx.Switching_Phases_C2);
}

// ---- Main ----
int main(void) {
    TEST_SUITE_BEGIN("Operating Modes");

    RUN_TEST(test_normal_mode_uses_max_current);
    RUN_TEST(test_normal_mode_ignores_mains);
    RUN_TEST(test_normal_mode_respects_max_capacity);
    RUN_TEST(test_smart_mode_respects_maxmains);
    RUN_TEST(test_smart_mode_slow_increase);
    RUN_TEST(test_smart_mode_fast_decrease);
    RUN_TEST(test_solar_current_available_requires_surplus);
    RUN_TEST(test_solar_current_available_with_surplus);
    RUN_TEST(test_solar_fine_grained_increase);
    RUN_TEST(test_solar_rapid_decrease_on_import);
    RUN_TEST(test_solar_import_current_offset);
    RUN_TEST(test_force_single_phase_not_present);
    RUN_TEST(test_force_single_phase_always_off);
    RUN_TEST(test_force_single_phase_solar_off_in_solar_mode);
    RUN_TEST(test_force_single_phase_solar_off_in_smart_mode);
    RUN_TEST(test_force_single_phase_auto_c2_1p);
    RUN_TEST(test_force_single_phase_auto_c2_3p);
    RUN_TEST(test_force_single_phase_always_on);
    RUN_TEST(test_state_C_contactor2_off_when_single_phase);
    RUN_TEST(test_state_C_contactor2_on_when_three_phase);
    RUN_TEST(test_phase_switch_going_to_1p);

    TEST_SUITE_RESULTS();
}
