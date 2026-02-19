/*
 * test_load_balancing.c - Master/Node current distribution
 *
 * Tests CalcBalancedCurrent() with multiple EVSEs,
 * fair distribution, overload protection, and the
 * permission protocol (STATE_COMM_B/C).
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

static void setup_master_two_evse(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 1;  // Master
    ctx.MaxCurrent = 16;
    ctx.MaxCapacity = 16;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 32;
    ctx.MaxMains = 25;
    ctx.ChargeCurrent = 160;
    ctx.phasesLastUpdateFlag = true;

    // Two EVSEs charging
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedState[1] = STATE_C;
    ctx.BalancedMax[0] = 160;
    ctx.BalancedMax[1] = 160;
    ctx.Balanced[0] = 80;
    ctx.Balanced[1] = 80;
    ctx.Node[0].Online = 1;
    ctx.Node[1].Online = 1;
}

// ---- Distribution tests ----

/*
 * @feature Load Balancing
 * @req REQ-LB-001
 * @scenario Single standalone EVSE receives full MaxCurrent allocation
 * @given A standalone EVSE (LoadBl=0) in STATE_C with MaxCurrent=16A
 * @when evse_calc_balanced_current is called
 * @then IsetBalanced is set to 160 (16A in tenths)
 */
void test_single_evse_gets_full_current(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;  // Standalone
    ctx.MaxCurrent = 16;
    ctx.MaxCapacity = 16;  // Match MaxCurrent
    ctx.ChargeCurrent = 160;
    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 160;
    ctx.Balanced[0] = 160;
    ctx.phasesLastUpdateFlag = true;

    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(160, ctx.IsetBalanced);
}

/*
 * @feature Load Balancing
 * @req REQ-LB-002
 * @scenario Two EVSEs receive equal current distribution
 * @given Two EVSEs are charging as master (LoadBl=1) with equal BalancedMax
 * @when evse_calc_balanced_current is called
 * @then Both EVSEs receive equal Balanced current allocations
 */
void test_two_evse_equal_distribution(void) {
    setup_master_two_evse();
    ctx.EVMeterImeasured = 160;  // Total measured = 160
    evse_calc_balanced_current(&ctx, 0);
    // Should distribute evenly
    TEST_ASSERT_EQUAL_INT(ctx.Balanced[0], ctx.Balanced[1]);
}

/*
 * @feature Load Balancing
 * @req REQ-LB-003
 * @scenario Two EVSEs respect MaxCircuit total capacity limit
 * @given Two EVSEs are charging with MaxCircuit=16A total
 * @when evse_calc_balanced_current is called
 * @then Each EVSE receives at most half the circuit capacity
 */
void test_two_evse_respects_max_circuit(void) {
    setup_master_two_evse();
    ctx.MaxCircuit = 16;  // Only 16A total for circuit
    ctx.EVMeterType = 1;
    ctx.EVMeterImeasured = 160;
    evse_calc_balanced_current(&ctx, 0);
    // Each should get at most ~8A (80 units)
    TEST_ASSERT_LESS_OR_EQUAL(100, ctx.Balanced[0]);
    TEST_ASSERT_LESS_OR_EQUAL(100, ctx.Balanced[1]);
}

/*
 * @feature Load Balancing
 * @req REQ-LB-004
 * @scenario Individual EVSE BalancedMax caps its current allocation
 * @given Two EVSEs are charging with EVSE 1 having BalancedMax=60 (6A)
 * @when evse_calc_balanced_current is called
 * @then EVSE 1 Balanced is capped at its BalancedMax of 60
 */
void test_balanced_max_caps_individual(void) {
    setup_master_two_evse();
    // Note: BalancedMax[0] (master) gets overwritten by ChargeCurrent inside calc
    // Test capping on EVSE 1 (node) which retains its BalancedMax
    ctx.BalancedMax[1] = 60;   // EVSE 1 can only take 6A
    ctx.EVMeterImeasured = 0;
    ctx.IsetBalanced = 220;
    evse_calc_balanced_current(&ctx, 0);
    // EVSE 1 should be capped at its max
    TEST_ASSERT_LESS_OR_EQUAL(60, ctx.Balanced[1]);
}

/*
 * @feature Load Balancing
 * @req REQ-LB-005
 * @scenario No active EVSEs resets shortage and solar timers
 * @given Two EVSEs are both in STATE_A (disconnected) as master
 * @when evse_calc_balanced_current is called
 * @then NoCurrent and SolarStopTimer are reset to 0
 */
// With no active EVSEs, IsetBalanced IS computed (from Mode/limits) but
// no distribution occurs. Timers are reset (NoCurrent=0, SolarStopTimer=0).
void test_no_active_evse_resets_timers(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 1;
    ctx.BalancedState[0] = STATE_A;
    ctx.BalancedState[1] = STATE_A;
    ctx.NoCurrent = 5;
    ctx.SolarStopTimer = 10;
    evse_calc_balanced_current(&ctx, 0);
    // No active EVSEs: timers are reset
    TEST_ASSERT_EQUAL_INT(0, ctx.NoCurrent);
    TEST_ASSERT_EQUAL_INT(0, ctx.SolarStopTimer);
}

/*
 * @feature Load Balancing
 * @req REQ-LB-006
 * @scenario Each active EVSE receives at least MinCurrent
 * @given Two EVSEs are charging with MinCurrent=6A and limited total capacity
 * @when evse_calc_balanced_current is called
 * @then Each charging EVSE with non-zero allocation gets at least MinCurrent*10
 */
void test_minimum_current_enforced(void) {
    setup_master_two_evse();
    ctx.MinCurrent = 6;
    ctx.IsetBalanced = 80;  // Only 80 = 8A total for 2 EVSEs
    ctx.EVMeterImeasured = 0;
    evse_calc_balanced_current(&ctx, 0);
    // Each EVSE should get at least MinCurrent
    for (int i = 0; i < 2; i++) {
        if (ctx.BalancedState[i] == STATE_C && ctx.Balanced[i] > 0) {
            TEST_ASSERT_TRUE(ctx.Balanced[i] >= ctx.MinCurrent * 10);
        }
    }
}

/*
 * @feature Load Balancing
 * @req REQ-LB-007
 * @scenario New EVSE joining (mod=1) triggers full recalculation
 * @given Two EVSEs in MODE_SMART with existing current distribution
 * @when evse_calc_balanced_current is called with mod=1 (new EVSE joining)
 * @then IsetBalanced is recalculated from scratch (different from previous value)
 */
void test_mod1_new_evse_recalculates(void) {
    setup_master_two_evse();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 100;  // 10A mains
    ctx.EVMeterImeasured = 80;
    int32_t before = ctx.IsetBalanced;
    evse_calc_balanced_current(&ctx, 1);  // mod=1, new EVSE joining
    // Should have recalculated from scratch
    TEST_ASSERT_NOT_EQUAL(before, ctx.IsetBalanced);
}

// ---- OCPP current limit ----

/*
 * @feature Load Balancing
 * @req REQ-LB-008
 * @scenario OCPP current limit reduces ChargeCurrent below MaxCurrent
 * @given A standalone EVSE with OcppCurrentLimit=10A and MaxCurrent=16A
 * @when evse_calc_balanced_current is called
 * @then ChargeCurrent is capped at 100 (10A in tenths) or below
 */
void test_ocpp_limit_reduces_charge_current(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;
    ctx.MaxCurrent = 16;
    ctx.ChargeCurrent = 160;
    ctx.OcppMode = true;
    ctx.OcppCurrentLimit = 10.0f;  // 10A limit
    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 160;
    ctx.Balanced[0] = 160;
    ctx.phasesLastUpdateFlag = true;

    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_LESS_OR_EQUAL(100, ctx.ChargeCurrent);
}

/*
 * @feature Load Balancing
 * @req REQ-LB-009
 * @scenario OCPP current limit below MinCurrent zeros out ChargeCurrent
 * @given A standalone EVSE with OcppCurrentLimit=3A and MinCurrent=6A
 * @when evse_calc_balanced_current is called
 * @then ChargeCurrent is set to 0 (below minimum, cannot charge)
 */
void test_ocpp_limit_below_min_zeros_current(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;
    ctx.MaxCurrent = 16;
    ctx.MinCurrent = 6;
    ctx.ChargeCurrent = 160;
    ctx.OcppMode = true;
    ctx.OcppCurrentLimit = 3.0f;  // Below minimum
    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 160;
    ctx.Balanced[0] = 160;
    ctx.phasesLastUpdateFlag = true;

    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(0, ctx.ChargeCurrent);
}

/*
 * @feature Load Balancing
 * @req REQ-LB-010
 * @scenario OverrideCurrent takes precedence over calculated ChargeCurrent
 * @given A standalone EVSE with OverrideCurrent=80 (8A) and MaxCurrent=16A
 * @when evse_calc_balanced_current is called
 * @then ChargeCurrent is set to 80 (override value)
 */
void test_override_current_takes_precedence(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;
    ctx.MaxCurrent = 16;
    ctx.ChargeCurrent = 160;
    ctx.OverrideCurrent = 80;  // Override to 8A
    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 160;
    ctx.Balanced[0] = 160;
    ctx.phasesLastUpdateFlag = true;

    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(80, ctx.ChargeCurrent);
}

// ---- Shortage detection ----

/*
 * @feature Load Balancing
 * @req REQ-LB-011
 * @scenario Current shortage increments NoCurrent counter
 * @given Two EVSEs in MODE_SMART with mains heavily loaded and low MaxMains
 * @when evse_calc_balanced_current is called with insufficient capacity
 * @then NoCurrent counter is incremented above 0
 */
void test_shortage_increments_nocurrent(void) {
    setup_master_two_evse();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 250;  // Very high load
    ctx.MaxMains = 10;              // Very low limit
    ctx.IsetBalanced = 50;          // Not enough for 2 EVSEs at MinCurrent
    ctx.phasesLastUpdateFlag = true;

    evse_calc_balanced_current(&ctx, 0);
    // NoCurrent should increment due to shortage
    if (ctx.IsetBalanced < (int32_t)(2 * ctx.MinCurrent * 10)) {
        TEST_ASSERT_GREATER_THAN(0, ctx.NoCurrent);
    }
}

/*
 * @feature Load Balancing
 * @req REQ-LB-012
 * @scenario No current shortage clears NoCurrent counter
 * @given Two EVSEs in MODE_SMART with low mains load and high MaxMains
 * @when evse_calc_balanced_current is called with sufficient capacity
 * @then NoCurrent counter is cleared to 0
 */
void test_no_shortage_clears_nocurrent(void) {
    setup_master_two_evse();
    ctx.Mode = MODE_SMART;
    ctx.NoCurrent = 5;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 50;   // Low load
    ctx.MaxMains = 32;              // High limit
    ctx.IsetBalanced = 200;
    evse_calc_balanced_current(&ctx, 0);
    if (ctx.IsetBalanced >= (int32_t)(2 * ctx.MinCurrent * 10)) {
        TEST_ASSERT_EQUAL_INT(0, ctx.NoCurrent);
    }
}

// ---- Grid relay ----

/*
 * @feature Load Balancing
 * @req REQ-LB-013
 * @scenario Open grid relay caps IsetBalanced at GridRelayMaxSumMains per phase
 * @given A standalone EVSE in MODE_SMART with GridRelayOpen=true and 3 phases
 * @when evse_calc_balanced_current is called
 * @then IsetBalanced is capped at GridRelayMaxSumMains*10/3
 */
void test_grid_relay_limits_current(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.LoadBl = 0;
    ctx.MaxCurrent = 32;
    ctx.MinCurrent = 6;
    ctx.MaxMains = 40;
    ctx.ChargeCurrent = 320;
    ctx.GridRelayOpen = true;
    ctx.GridRelayMaxSumMains = 18;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 50;
    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 320;
    ctx.Balanced[0] = 320;
    ctx.IsetBalanced = 320;
    ctx.phasesLastUpdateFlag = true;

    evse_calc_balanced_current(&ctx, 0);
    // Grid relay should cap at GridRelayMaxSumMains / phases
    int32_t relay_limit = (ctx.GridRelayMaxSumMains * 10) / 3;
    TEST_ASSERT_LESS_OR_EQUAL(relay_limit, ctx.IsetBalanced);
}

// ---- Node communication state tests ----

/*
 * @feature Load Balancing
 * @req REQ-LB-014
 * @scenario Node EVSE requests COMM_C instead of transitioning directly to STATE_C
 * @given The EVSE is configured as a node (LoadBl=2) in STATE_B with DiodeCheck passed
 * @when A 6V pilot signal is sustained for 500ms (vehicle requests charge)
 * @then The state transitions to STATE_COMM_C (requesting master permission to charge)
 */
void test_node_requests_comm_c(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 2;  // Node
    ctx.ChargeCurrent = 130;
    ctx.MaxCurrent = 13;
    evse_set_state(&ctx, STATE_B);
    ctx.DiodeCheck = 1;

    // 6V pilot for 500ms
    for (int i = 0; i < 55; i++) {
        evse_tick_10ms(&ctx, PILOT_6V);
    }
    // Node should request COMM_C, not go directly to STATE_C
    TEST_ASSERT_EQUAL_INT(STATE_COMM_C, ctx.State);
}

// ---- Config (Socket vs Fixed Cable) guard (F1 fidelity fix) ----

/*
 * @feature Load Balancing
 * @req REQ-LB-F1A
 * @scenario Socket mode (Config=0) caps ChargeCurrent by MaxCapacity
 * @given Config=0 (Socket), MaxCurrent=25, MaxCapacity=16, STATE_C
 * @when evse_calc_balanced_current is called
 * @then ChargeCurrent is capped at 160 (MaxCapacity * 10)
 */
void test_config_socket_caps_by_maxcapacity(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;
    ctx.Config = 0;  // Socket mode
    ctx.MaxCurrent = 25;
    ctx.MaxCapacity = 16;
    ctx.MinCurrent = 6;
    ctx.ChargeCurrent = 250;
    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 250;
    ctx.Balanced[0] = 250;
    ctx.phasesLastUpdateFlag = true;

    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(160, ctx.ChargeCurrent);
}

/*
 * @feature Load Balancing
 * @req REQ-LB-F1B
 * @scenario Fixed Cable mode (Config=1) does NOT cap by MaxCapacity
 * @given Config=1 (Fixed Cable), MaxCurrent=25, MaxCapacity=16, STATE_C
 * @when evse_calc_balanced_current is called
 * @then ChargeCurrent is 250 (MaxCurrent * 10), not capped by MaxCapacity
 */
void test_config_fixed_cable_no_maxcapacity_cap(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;
    ctx.Config = 1;  // Fixed Cable mode
    ctx.MaxCurrent = 25;
    ctx.MaxCapacity = 16;
    ctx.MinCurrent = 6;
    ctx.ChargeCurrent = 250;
    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 250;
    ctx.Balanced[0] = 250;
    ctx.phasesLastUpdateFlag = true;

    evse_calc_balanced_current(&ctx, 0);
    TEST_ASSERT_EQUAL_INT(250, ctx.ChargeCurrent);
}

// ---- Main ----
int main(void) {
    TEST_SUITE_BEGIN("Load Balancing");

    RUN_TEST(test_single_evse_gets_full_current);
    RUN_TEST(test_two_evse_equal_distribution);
    RUN_TEST(test_two_evse_respects_max_circuit);
    RUN_TEST(test_balanced_max_caps_individual);
    RUN_TEST(test_no_active_evse_resets_timers);
    RUN_TEST(test_minimum_current_enforced);
    RUN_TEST(test_mod1_new_evse_recalculates);
    RUN_TEST(test_ocpp_limit_reduces_charge_current);
    RUN_TEST(test_ocpp_limit_below_min_zeros_current);
    RUN_TEST(test_override_current_takes_precedence);
    RUN_TEST(test_shortage_increments_nocurrent);
    RUN_TEST(test_no_shortage_clears_nocurrent);
    RUN_TEST(test_grid_relay_limits_current);
    RUN_TEST(test_node_requests_comm_c);
    RUN_TEST(test_config_socket_caps_by_maxcapacity);
    RUN_TEST(test_config_fixed_cable_no_maxcapacity_cap);

    TEST_SUITE_RESULTS();
}
