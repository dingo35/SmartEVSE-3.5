/*
 * test_power_availability.c - IsCurrentAvailable() checks
 *
 * Tests the multi-layer power availability gate:
 *   - MaxMains / MaxCircuit / MaxSumMains limits
 *   - Solar surplus requirements
 *   - OCPP current limit
 *   - Mode-specific behavior
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

static void setup_normal_standalone(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;
    ctx.AccessStatus = ON;
    ctx.MinCurrent = MIN_CURRENT;
    ctx.MaxMains = MAX_MAINS;
    ctx.MaxCircuit = MAX_CIRCUIT;
}

// ---- Normal mode availability ----

/*
 * @feature Power Availability
 * @req REQ-PWR-001
 * @scenario Normal mode always reports current as available regardless of mains load
 * @given EVSE is standalone in Normal mode with very high MainsMeterImeasured=999
 * @when evse_is_current_available is called
 * @then Returns 1 (available) because Normal mode does not check mains
 */
void test_normal_mode_always_available(void) {
    setup_normal_standalone();
    // Normal mode doesn't check mains
    ctx.MainsMeterImeasured = 999;
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(1, result);
}

/*
 * @feature Power Availability
 * @req REQ-PWR-002
 * @scenario Normal mode available even with high mains load
 * @given EVSE is standalone in Normal mode with MainsMeterImeasured=400
 * @when evse_is_current_available is called
 * @then Returns 1 (available) because Normal mode ignores mains measurements
 */
void test_normal_mode_available_with_high_load(void) {
    setup_normal_standalone();
    ctx.MainsMeterImeasured = 400;
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(1, result);
}

// ---- Smart mode MaxMains ----

/*
 * @feature Power Availability
 * @req REQ-PWR-003
 * @scenario Smart mode allows current when mains load plus MinCurrent is under MaxMains
 * @given EVSE is standalone in Smart mode with MaxMains=25A and MainsMeterImeasured=100 (10A)
 * @when evse_is_current_available is called
 * @then Returns 1 (available) because baseload (10A) + MinCurrent (6A) = 16A < MaxMains (25A)
 */
void test_smart_maxmains_allows_under_limit(void) {
    setup_normal_standalone();
    ctx.Mode = MODE_SMART;
    ctx.MaxMains = 25;             // 25A limit
    ctx.MainsMeterImeasured = 100; // 10A current load
    ctx.MinCurrent = 6;
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(1, result);
}

/*
 * @feature Power Availability
 * @req REQ-PWR-004
 * @scenario Smart mode blocks current when mains load plus MinCurrent exceeds MaxMains
 * @given EVSE is standalone in Smart mode with MaxMains=10A and MainsMeterImeasured=200 (20A baseload)
 * @when evse_is_current_available is called
 * @then Returns 0 (unavailable) because baseload (20A) + MinCurrent (6A) = 26A > MaxMains (10A)
 */
void test_smart_maxmains_blocks_over_limit(void) {
    setup_normal_standalone();
    ctx.Mode = MODE_SMART;
    ctx.MaxMains = 10;             // 10A limit
    ctx.MainsMeterImeasured = 200; // 20A current -> baseload 20A
    ctx.MinCurrent = 6;
    // Adding MinCurrent (6A) to 20A baseload = 26A > 10A limit
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(0, result);
}

// ---- Smart mode MaxCircuit ----

/*
 * @feature Power Availability
 * @req REQ-PWR-005
 * @scenario Smart mode allows current when circuit load is under MaxCircuit limit
 * @given EVSE is master (LoadBl=1) in Smart mode with MaxCircuit=20A and EVMeterImeasured=50
 * @when evse_is_current_available is called
 * @then Returns 1 (available) because circuit load (5A) + MinCurrent (6A) is under MaxCircuit
 */
void test_smart_maxcircuit_allows_under_limit(void) {
    setup_normal_standalone();
    ctx.Mode = MODE_SMART;
    ctx.LoadBl = 1;  // Master, so MaxCircuit check applies
    ctx.MaxCircuit = 20;
    ctx.MaxMains = 40;  // High mains limit
    ctx.EVMeterImeasured = 50;
    ctx.MinCurrent = 6;
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(1, result);
}

/*
 * @feature Power Availability
 * @req REQ-PWR-006
 * @scenario Smart mode blocks current when circuit load exceeds MaxCircuit limit
 * @given EVSE is master (LoadBl=1) in Smart mode with MaxCircuit=8A and EVMeterImeasured=100 (10A)
 * @when evse_is_current_available is called
 * @then Returns 0 (unavailable) because circuit load (10A) already exceeds MaxCircuit (8A)
 */
void test_smart_maxcircuit_blocks_over_limit(void) {
    setup_normal_standalone();
    ctx.Mode = MODE_SMART;
    ctx.LoadBl = 1;
    ctx.MaxCircuit = 8;           // Very low circuit limit
    ctx.MaxMains = 40;
    ctx.EVMeterImeasured = 100;   // Already at 10A on circuit
    ctx.MinCurrent = 6;
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(0, result);
}

// ---- MaxSumMains ----

/*
 * @feature Power Availability
 * @req REQ-PWR-007
 * @scenario MaxSumMains allows current when sum of phase currents is under limit
 * @given EVSE is in Smart mode with MaxSumMains=50 and Isum=100 (10A total)
 * @when evse_is_current_available is called
 * @then Returns 1 (available) because Isum plus MinCurrent is under MaxSumMains limit
 */
void test_maxsummains_allows_under_limit(void) {
    setup_normal_standalone();
    ctx.Mode = MODE_SMART;
    ctx.MaxSumMains = 50;
    ctx.Isum = 100;  // 10A total across all phases
    ctx.MinCurrent = 6;
    ctx.MaxMains = 40;
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(1, result);
}

/*
 * @feature Power Availability
 * @req REQ-PWR-008
 * @scenario MaxSumMains blocks current when sum of phase currents exceeds limit
 * @given EVSE is in Smart mode with MaxSumMains=10 and Isum=200 (way over limit)
 * @when evse_is_current_available is called
 * @then Returns 0 (unavailable) because total phase current sum exceeds MaxSumMains
 */
void test_maxsummains_blocks_over_limit(void) {
    setup_normal_standalone();
    ctx.Mode = MODE_SMART;
    ctx.MaxSumMains = 10;
    ctx.Isum = 200;  // Way over sum limit
    ctx.MinCurrent = 6;
    ctx.MaxMains = 40;
    ctx.MainsMeterImeasured = 50;
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/*
 * @feature Power Availability
 * @req REQ-PWR-009
 * @scenario MaxSumMains=0 disables the sum-of-mains check entirely
 * @given EVSE is in Smart mode with MaxSumMains=0 (disabled) and Isum=9999 (extremely high)
 * @when evse_is_current_available is called
 * @then Returns 1 (available) because MaxSumMains=0 means the check is skipped
 */
void test_maxsummains_zero_disables_check(void) {
    setup_normal_standalone();
    ctx.Mode = MODE_SMART;
    ctx.MaxSumMains = 0;  // Disabled
    ctx.Isum = 9999;      // Would exceed any limit
    ctx.MaxMains = 40;
    ctx.MinCurrent = 6;
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(1, result);
}

// ---- Solar mode surplus ----

/*
 * @feature Power Availability
 * @req REQ-PWR-010
 * @scenario Solar mode blocks current when no surplus is available
 * @given EVSE is in Solar mode with StartCurrent=6A and Isum=0 (no export)
 * @when evse_is_current_available is called
 * @then Returns 0 (unavailable) because there is no solar surplus for charging
 */
void test_solar_no_surplus_blocks(void) {
    setup_normal_standalone();
    ctx.Mode = MODE_SOLAR;
    ctx.StartCurrent = 6;
    ctx.Isum = 0;  // No surplus
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/*
 * @feature Power Availability
 * @req REQ-PWR-011
 * @scenario Solar mode allows current when surplus exceeds StartCurrent
 * @given EVSE is in Solar mode with StartCurrent=6A and Isum=-80 (8A export surplus)
 * @when evse_is_current_available is called
 * @then Returns 1 (available) because 8A surplus exceeds 6A StartCurrent threshold
 */
void test_solar_surplus_allows(void) {
    setup_normal_standalone();
    ctx.Mode = MODE_SOLAR;
    ctx.StartCurrent = 6;
    ctx.Isum = -80;  // 8A export
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(1, result);
}

/*
 * @feature Power Availability
 * @req REQ-PWR-012
 * @scenario Solar mode blocks current when surplus is below StartCurrent threshold
 * @given EVSE is in Solar mode with StartCurrent=10A and Isum=-80 (only 8A surplus)
 * @when evse_is_current_available is called
 * @then Returns 0 (unavailable) because 8A surplus is below the 10A StartCurrent threshold
 */
void test_solar_insufficient_surplus_blocks(void) {
    setup_normal_standalone();
    ctx.Mode = MODE_SOLAR;
    ctx.StartCurrent = 10;        // Need 10A surplus
    ctx.Isum = -80;               // Only 8A surplus
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/*
 * @feature Power Availability
 * @req REQ-PWR-013
 * @scenario Solar mode with active EVSE checks fair share before allowing more
 * @given EVSE is in Solar mode with one active EVSE at MinCurrent and Isum=10 (1A import)
 * @when evse_is_current_available is called
 * @then Returns 0 (unavailable) because grid import indicates insufficient surplus for another EVSE
 */
void test_solar_with_active_evse_checks_fair_share(void) {
    setup_normal_standalone();
    ctx.Mode = MODE_SOLAR;
    ctx.StartCurrent = 6;
    ctx.MinCurrent = 6;
    ctx.BalancedState[0] = STATE_C;
    ctx.Balanced[0] = 60;  // 6A charging
    ctx.Isum = 10;         // 1A import
    // One active EVSE at min current, trying to add another
    // Active * MinCurrent * 10 = 1 * 60 = 60, TotalCurrent = 60
    // Check: Isum > ImportCurrent*10 + TotalCurrent - ActiveEVSE * MinCurrent * 10
    // 10 > 0 + 60 - 60 = 0? Yes -> blocked
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(0, result);
}

// ---- OCPP availability ----

/*
 * @feature Power Availability
 * @req REQ-PWR-014
 * @scenario OCPP limit below MinCurrent blocks power availability
 * @given EVSE is standalone with OcppMode enabled and OcppCurrentLimit=4.0A (below MinCurrent=6A)
 * @when evse_is_current_available is called
 * @then Returns 0 (unavailable) because OCPP limit is below the minimum viable charge current
 */
void test_ocpp_limit_blocks_when_below_min(void) {
    setup_normal_standalone();
    ctx.OcppMode = true;
    ctx.OcppCurrentLimit = 4.0f;
    ctx.MinCurrent = 6;
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/*
 * @feature Power Availability
 * @req REQ-PWR-015
 * @scenario OCPP limit above MinCurrent allows power availability
 * @given EVSE is standalone with OcppMode enabled and OcppCurrentLimit=10.0A (above MinCurrent=6A)
 * @when evse_is_current_available is called
 * @then Returns 1 (available) because OCPP limit is above the minimum viable charge current
 */
void test_ocpp_limit_allows_when_above_min(void) {
    setup_normal_standalone();
    ctx.OcppMode = true;
    ctx.OcppCurrentLimit = 10.0f;
    ctx.MinCurrent = 6;
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(1, result);
}

/*
 * @feature Power Availability
 * @req REQ-PWR-016
 * @scenario OCPP negative limit (no limit set) allows power availability
 * @given EVSE is standalone with OcppMode enabled and OcppCurrentLimit=-1.0A (no limit)
 * @when evse_is_current_available is called
 * @then Returns 1 (available) because negative OCPP limit means no restriction
 */
void test_ocpp_no_limit_allows(void) {
    setup_normal_standalone();
    ctx.OcppMode = true;
    ctx.OcppCurrentLimit = -1.0f;
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(1, result);
}

/*
 * @feature Power Availability
 * @req REQ-PWR-017
 * @scenario OCPP availability check is skipped for non-standalone configurations
 * @given EVSE is master (LoadBl=1) with OcppMode enabled and OcppCurrentLimit=3.0A (below MinCurrent)
 * @when evse_is_current_available is called
 * @then Returns 1 (available) because OCPP check requires LoadBl=0 (standalone)
 */
void test_ocpp_check_only_for_standalone(void) {
    setup_normal_standalone();
    ctx.OcppMode = true;
    ctx.OcppCurrentLimit = 3.0f;  // Below min
    ctx.MinCurrent = 6;
    ctx.LoadBl = 1;  // Master - OCPP check skipped for LoadBl != 0
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(1, result);
}

// ---- PWM duty cycle conversion ----

/*
 * @feature Power Availability
 * @req REQ-PWR-018
 * @scenario PWM duty cycle conversion for 6A (minimum charge current)
 * @given A charge current of 60 deciamps (6A)
 * @when evse_current_to_duty is called
 * @then Returns 102 as the PWM duty cycle value (60/0.6 * 1024/1000)
 */
void test_current_to_duty_6A(void) {
    // 60 (6A * 10) -> 60/0.6 = 100 -> 100 * 1024/1000 = 102
    uint32_t duty = evse_current_to_duty(60);
    TEST_ASSERT_EQUAL_INT(102, duty);
}

/*
 * @feature Power Availability
 * @req REQ-PWR-019
 * @scenario PWM duty cycle conversion for 16A (common residential limit)
 * @given A charge current of 160 deciamps (16A)
 * @when evse_current_to_duty is called
 * @then Returns a duty cycle value between 100 and 600 (low-range formula)
 */
void test_current_to_duty_16A(void) {
    // 160 -> 160/0.6 = 266.67 -> 266 * 1024/1000 = 272
    uint32_t duty = evse_current_to_duty(160);
    TEST_ASSERT_TRUE(duty > 100 && duty < 600);
}

/*
 * @feature Power Availability
 * @req REQ-PWR-020
 * @scenario PWM duty cycle conversion for 51A (upper boundary of low-range formula)
 * @given A charge current of 510 deciamps (51A)
 * @when evse_current_to_duty is called
 * @then Returns a duty cycle value between 800 and 1000 (near top of low-range)
 */
void test_current_to_duty_51A(void) {
    // 510 -> 510/0.6 = 850 -> 850 * 1024/1000 = 870
    uint32_t duty = evse_current_to_duty(510);
    TEST_ASSERT_TRUE(duty > 800 && duty < 1000);
}

/*
 * @feature Power Availability
 * @req REQ-PWR-021
 * @scenario PWM duty cycle conversion for 60A (high-range formula)
 * @given A charge current of 600 deciamps (60A)
 * @when evse_current_to_duty is called
 * @then Returns a duty cycle value between 850 and 1000 (high-range formula: current/2.5 + 640)
 */
void test_current_to_duty_high_range(void) {
    // 600 -> 600/2.5 + 640 = 880 -> 880 * 1024/1000 = 901
    uint32_t duty = evse_current_to_duty(600);
    TEST_ASSERT_TRUE(duty > 850 && duty < 1000);
}

/*
 * @feature Power Availability
 * @req REQ-PWR-022
 * @scenario PWM duty cycle conversion for 80A (near maximum charge current)
 * @given A charge current of 800 deciamps (80A)
 * @when evse_current_to_duty is called
 * @then Returns a duty cycle value between 950 and 1024 (near the top of the PWM range)
 */
void test_current_to_duty_80A(void) {
    // 800 -> 800/2.5 + 640 = 960 -> 960 * 1024/1000 = 983
    uint32_t duty = evse_current_to_duty(800);
    TEST_ASSERT_TRUE(duty > 950 && duty < 1024);
}

// ---- Main ----
int main(void) {
    TEST_SUITE_BEGIN("Power Availability");

    RUN_TEST(test_normal_mode_always_available);
    RUN_TEST(test_normal_mode_available_with_high_load);
    RUN_TEST(test_smart_maxmains_allows_under_limit);
    RUN_TEST(test_smart_maxmains_blocks_over_limit);
    RUN_TEST(test_smart_maxcircuit_allows_under_limit);
    RUN_TEST(test_smart_maxcircuit_blocks_over_limit);
    RUN_TEST(test_maxsummains_allows_under_limit);
    RUN_TEST(test_maxsummains_blocks_over_limit);
    RUN_TEST(test_maxsummains_zero_disables_check);
    RUN_TEST(test_solar_no_surplus_blocks);
    RUN_TEST(test_solar_surplus_allows);
    RUN_TEST(test_solar_insufficient_surplus_blocks);
    RUN_TEST(test_solar_with_active_evse_checks_fair_share);
    RUN_TEST(test_ocpp_limit_blocks_when_below_min);
    RUN_TEST(test_ocpp_limit_allows_when_above_min);
    RUN_TEST(test_ocpp_no_limit_allows);
    RUN_TEST(test_ocpp_check_only_for_standalone);
    RUN_TEST(test_current_to_duty_6A);
    RUN_TEST(test_current_to_duty_16A);
    RUN_TEST(test_current_to_duty_51A);
    RUN_TEST(test_current_to_duty_high_range);
    RUN_TEST(test_current_to_duty_80A);

    TEST_SUITE_RESULTS();
}
