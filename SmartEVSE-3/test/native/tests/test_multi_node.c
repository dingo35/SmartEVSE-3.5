/*
 * test_multi_node.c - Multi-node load balancing edge cases
 *
 * Tests:
 *   - 4+ EVSEs charging simultaneously with fair distribution
 *   - One EVSE at max capacity while others share remainder
 *   - Node going offline (BalancedState changes from C to A)
 *   - All nodes at MinCurrent during shortage
 *   - MaxCircuit limiting with multiple nodes
 *   - BalancedError interactions
 *   - NoCurrent threshold behavior at high values
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

/* Helper: set up master with N EVSEs all charging */
static void setup_master_n_evse(int n) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 1;  /* Master */
    ctx.MaxCurrent = 32;
    ctx.MaxCapacity = 32;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 64;
    ctx.MaxMains = 50;
    ctx.ChargeCurrent = 320;
    ctx.phasesLastUpdateFlag = true;

    for (int i = 0; i < n && i < NR_EVSES; i++) {
        ctx.BalancedState[i] = STATE_C;
        ctx.BalancedMax[i] = 320;    /* Each can take up to 32A */
        ctx.Balanced[i] = 100;       /* Currently drawing 10A each */
        ctx.Node[i].Online = 1;
        ctx.Node[i].IntTimer = 100;  /* Past solar startup */
    }
}

/* ---- 4 EVSEs fair distribution ---- */

/*
 * @feature Multi-Node Load Balancing
 * @req REQ-MULTI-001
 * @scenario Four EVSEs receive equal current distribution
 * @given Master with 4 EVSEs all in STATE_C, MaxCircuit=64A, no EV meter baseload
 * @when Balanced current is calculated
 * @then Each EVSE receives 160 deciamps (640 / 4 = 160) with all values equal
 */
void test_four_evse_fair_distribution(void) {
    setup_master_n_evse(4);
    ctx.MaxCircuit = 64;    /* 64A total for circuit */
    ctx.EVMeterType = 0;    /* No EV meter, so MaxCircuit check only applies for LoadBl==1 with EVMeter */
    ctx.EVMeterImeasured = 0;

    evse_calc_balanced_current(&ctx, 0);

    /* In Normal mode with LoadBl=1:
     * IsetBalanced = (MaxCircuit * 10) - Baseload_EV
     * Baseload_EV = EVMeterImeasured(0) - TotalCurrent(400) = -400, clamped to 0
     * IsetBalanced = 640 - 0 = 640
     * Each of 4 EVSEs should get 640/4 = 160 */
    TEST_ASSERT_EQUAL_INT(ctx.Balanced[0], ctx.Balanced[1]);
    TEST_ASSERT_EQUAL_INT(ctx.Balanced[1], ctx.Balanced[2]);
    TEST_ASSERT_EQUAL_INT(ctx.Balanced[2], ctx.Balanced[3]);
    TEST_ASSERT_EQUAL_INT(160, ctx.Balanced[0]);
}

/* ---- 4 EVSEs: BalancedMax[0] gets overwritten by ChargeCurrent ---- */

/*
 * @feature Multi-Node Load Balancing
 * @req REQ-MULTI-002
 * @scenario Master EVSE BalancedMax is derived from ChargeCurrent
 * @given Master with 4 EVSEs all in STATE_C, master ChargeCurrent limited to 200 deciamps (20A)
 * @when Balanced current is calculated
 * @then Master EVSE (Balanced[0]) does not exceed its ChargeCurrent limit of 200 deciamps
 */
void test_four_evse_master_max_from_chargecurrent(void) {
    setup_master_n_evse(4);
    ctx.ChargeCurrent = 200;  /* Master limited to 20A */
    ctx.BalancedMax[0] = 200;
    ctx.EVMeterImeasured = 0;

    evse_calc_balanced_current(&ctx, 0);

    /* BalancedMax[0] is set to ChargeCurrent (200) in calc.
     * IsetBalanced = 640. Distribution capped at individual max.
     * EVSE[0] gets min(average, 200). Others get up to 320 each.
     * With 640 total and EVSE[0] capped at 200:
     * First pass: avg=160, 160 < 200, so no cap in first pass.
     * Actually: avg = 640/4 = 160 for each. BalancedMax[0]=200, 160 < 200.
     * BalancedMax[1..3]=320, 160 < 320. All get 160. */
    TEST_ASSERT_LESS_OR_EQUAL(200, ctx.Balanced[0]);
}

/* ---- One EVSE at low max, others share remainder ---- */

/*
 * @feature Multi-Node Load Balancing
 * @req REQ-MULTI-003
 * @scenario One EVSE with low max capacity, remainder shared by others
 * @given Master with 3 EVSEs in STATE_C, node 1 limited to BalancedMax=60 deciamps (6A)
 * @when Balanced current is calculated
 * @then Node 1 receives at most 60 deciamps while nodes 0 and 2 share the remainder equally
 */
void test_one_evse_low_max_others_share(void) {
    setup_master_n_evse(3);
    ctx.BalancedMax[1] = 60;    /* Node 1 can only take 6A */
    ctx.EVMeterImeasured = 0;

    evse_calc_balanced_current(&ctx, 0);

    /* IsetBalanced = 640 (MaxCircuit=64, Baseload_EV=0)
     * EVSE[1] capped at 60 (its max).
     * Remaining: 640-60=580 for 2 EVSEs -> 290 each.
     * But BalancedMax[0] = ChargeCurrent = 320, so 290 < 320. OK.
     * BalancedMax[2] = 320, 290 < 320. OK. */
    TEST_ASSERT_LESS_OR_EQUAL(60, ctx.Balanced[1]);
    /* Others should be roughly equal and larger */
    TEST_ASSERT_EQUAL_INT(ctx.Balanced[0], ctx.Balanced[2]);
    TEST_ASSERT_GREATER_THAN(60, ctx.Balanced[0]);
}

/* ---- Node goes offline: BalancedState changes from C to A ---- */

/*
 * @feature Multi-Node Load Balancing
 * @req REQ-MULTI-004
 * @scenario Node going offline causes redistribution to remaining active nodes
 * @given Master with 3 EVSEs in STATE_C receiving equal distribution
 * @when Node 2 goes offline (BalancedState changes to STATE_A) and current is recalculated
 * @then Remaining active nodes each receive more current than before and offline node gets 0
 */
void test_node_goes_offline_redistributes(void) {
    setup_master_n_evse(3);
    ctx.EVMeterImeasured = 0;
    /* First calculate with 3 active */
    evse_calc_balanced_current(&ctx, 0);
    int32_t balanced_with_3 = ctx.Balanced[0];

    /* Node 2 goes offline */
    ctx.BalancedState[2] = STATE_A;
    ctx.Balanced[2] = 0;
    ctx.phasesLastUpdateFlag = true;
    evse_calc_balanced_current(&ctx, 0);

    /* With only 2 active, each should get more than before */
    TEST_ASSERT_GREATER_THAN(balanced_with_3, ctx.Balanced[0]);
    /* Node 2 should remain at 0 */
    TEST_ASSERT_EQUAL_INT(0, ctx.Balanced[2]);
}

/* ---- All nodes at MinCurrent during shortage ---- */

/*
 * @feature Multi-Node Load Balancing
 * @req REQ-MULTI-005
 * @scenario Priority scheduling allocates available power to highest-priority EVSE during shortage
 * @given Master with 4 EVSEs in STATE_C in Smart mode, available power ~105 dA (enough for 1 EVSE, not 4)
 * @when Balanced current is calculated with insufficient power for all nodes
 * @then Highest-priority EVSE gets all available power, others are paused
 */
void test_all_nodes_mincurrent_during_shortage(void) {
    setup_master_n_evse(4);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 480;  /* Very high load */
    ctx.MaxMains = 50;
    ctx.IsetBalanced = 100;  /* Too little for 4 EVSEs at 6A each (240 needed) */

    evse_calc_balanced_current(&ctx, 0);

    /* Priority scheduling: available ~105 dA (100 + Idifference/4).
     * EVSE[0] gets MinCurrent(60) + surplus(45) = 105.
     * EVSEs [1]-[3] are paused (0). NoCurrent stays 0 (deliberate pause). */
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[0]);
    for (int i = 1; i < 4; i++) {
        TEST_ASSERT_EQUAL_INT(0, ctx.Balanced[i]);
    }
    TEST_ASSERT_EQUAL_INT(0, ctx.NoCurrent);
}

/* ---- MaxCircuit limiting with multiple nodes ---- */

/*
 * @feature Multi-Node Load Balancing
 * @req REQ-MULTI-006
 * @scenario MaxCircuit limits total current distribution across all nodes
 * @given Master with 4 EVSEs in STATE_C in Normal mode with MaxCircuit=24A
 * @when Balanced current is calculated
 * @then Total distributed current across all nodes does not exceed 240 deciamps (MaxCircuit * 10)
 */
void test_maxcircuit_limits_total_distribution(void) {
    setup_master_n_evse(4);
    ctx.MaxCircuit = 24;  /* Only 24A for entire circuit */
    ctx.EVMeterType = 0;
    ctx.EVMeterImeasured = 0;

    evse_calc_balanced_current(&ctx, 0);

    /* In Normal mode with LoadBl=1:
     * IsetBalanced = (MaxCircuit * 10) - Baseload_EV = 240 - 0 = 240
     * With 4 EVSEs: 240/4 = 60 each = exactly MinCurrent */
    int32_t total = 0;
    for (int i = 0; i < 4; i++) {
        total += ctx.Balanced[i];
    }
    TEST_ASSERT_LESS_OR_EQUAL(240, total);
}

/* ---- MaxCircuit limiting with EV meter ---- */

/*
 * @feature Multi-Node Load Balancing
 * @req REQ-MULTI-007
 * @scenario MaxCircuit accounts for EV meter baseload in distribution
 * @given Master with 2 EVSEs in STATE_C, MaxCircuit=20A, EV meter measuring 250 deciamps total
 * @when Balanced current is calculated with EV meter baseload subtracted
 * @then Total distributed current does not exceed (MaxCircuit * 10) minus baseload
 */
void test_maxcircuit_with_ev_meter_baseload(void) {
    setup_master_n_evse(2);
    ctx.MaxCircuit = 20;
    ctx.EVMeterType = 1;
    ctx.EVMeterImeasured = 250;  /* 25A total measured on EV meter */
    /* TotalCurrent = 100+100 = 200 */
    /* Baseload_EV = 250 - 200 = 50 */
    /* IsetBalanced = min(ChargeCurrent, (MaxCircuit*10)-Baseload_EV) = min(320, 200-50) = 150 */

    evse_calc_balanced_current(&ctx, 0);

    /* Guard rail: IsetBalanced capped to (MaxCircuit*10) - Baseload_EV = 200 - 50 = 150 */
    int32_t total = ctx.Balanced[0] + ctx.Balanced[1];
    TEST_ASSERT_LESS_OR_EQUAL(150, total);
}

/* ---- 6 EVSEs: large cluster fair distribution ---- */

/*
 * @feature Multi-Node Load Balancing
 * @req REQ-MULTI-008
 * @scenario Six EVSEs in large cluster receive fair distribution
 * @given Master with 6 EVSEs all in STATE_C, MaxCircuit=64A
 * @when Balanced current is calculated
 * @then All 6 EVSEs receive equal current within 1 deciamp tolerance (integer division rounding)
 */
void test_six_evse_fair_distribution(void) {
    setup_master_n_evse(6);
    ctx.MaxCircuit = 64;
    ctx.EVMeterImeasured = 0;

    evse_calc_balanced_current(&ctx, 0);

    /* IsetBalanced = 640. 6 EVSEs, each gets 640/6 = 106.
     * Check all are equal (or within 1 due to integer division). */
    for (int i = 1; i < 6; i++) {
        int diff = (int)ctx.Balanced[0] - (int)ctx.Balanced[i];
        if (diff < 0) diff = -diff;
        TEST_ASSERT_LESS_OR_EQUAL(1, diff);
    }
}

/* ---- NoCurrent increments on hard shortage ---- */

/*
 * @feature Multi-Node Load Balancing
 * @req REQ-MULTI-009
 * @scenario NoCurrent counter increments during hard power shortage
 * @given Master with 4 EVSEs in Smart mode, 50A mains measured against 25A limit, IsetBalanced too low
 * @when Balanced current is calculated and total MinCurrent demand exceeds available power
 * @then NoCurrent counter increments above 0 indicating sustained shortage
 */
void test_nocurrent_increments_on_hard_shortage(void) {
    setup_master_n_evse(4);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 500;  /* 50A measured */
    ctx.MaxMains = 25;              /* Only 25A limit */
    /* Baseload = 500 - 400 = 100. IsetBalanced upper = MaxMains*10 - Baseload = 250 - 100 = 150 */
    /* But 4 EVSEs * 60 = 240 needed. 240 > 150 => hard shortage */
    ctx.IsetBalanced = 100;

    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_GREATER_THAN(0, ctx.NoCurrent);
}

/* ---- NoCurrent stays 0 when there is enough power ---- */

/*
 * @feature Multi-Node Load Balancing
 * @req REQ-MULTI-010
 * @scenario NoCurrent counter clears when sufficient power is available
 * @given Master with 2 EVSEs in Smart mode, low mains load, IsetBalanced=400, NoCurrent previously at 5
 * @when Balanced current is calculated with plenty of available power
 * @then NoCurrent counter is cleared to 0
 */
void test_nocurrent_zero_when_sufficient(void) {
    setup_master_n_evse(2);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 50;   /* Low mains load */
    ctx.MaxMains = 50;
    ctx.IsetBalanced = 400;
    ctx.NoCurrent = 5;  /* Previously had shortage */

    evse_calc_balanced_current(&ctx, 0);

    /* Plenty of current: NoCurrent should be cleared */
    TEST_ASSERT_EQUAL_INT(0, ctx.NoCurrent);
}

/* ---- Node at STATE_B does not receive current ---- */

/*
 * @feature Multi-Node Load Balancing
 * @req REQ-MULTI-011
 * @scenario Node in STATE_B does not participate in current distribution
 * @given Master with 3 EVSEs, nodes 0 and 2 in STATE_C, node 1 in STATE_B (waiting)
 * @when Balanced current is calculated
 * @then Only active STATE_C nodes (0 and 2) receive distributed current, each getting 320 deciamps
 */
void test_state_b_node_gets_no_current(void) {
    setup_master_n_evse(3);
    ctx.BalancedState[1] = STATE_B;  /* Node 1 waiting, not charging */
    ctx.EVMeterImeasured = 0;

    evse_calc_balanced_current(&ctx, 0);

    /* STATE_B nodes don't participate in distribution.
     * Their Balanced value is NOT set by calc, so it stays at whatever it was.
     * Only STATE_C nodes get distributed. */
    /* Active EVSEs: 0 and 2.  IsetBalanced = 640. Each gets 320. */
    TEST_ASSERT_EQUAL_INT(320, ctx.Balanced[0]);
    TEST_ASSERT_EQUAL_INT(320, ctx.Balanced[2]);
}

/* ---- IsetBalanced capped at ActiveMax ---- */

/*
 * @feature Multi-Node Load Balancing
 * @req REQ-MULTI-012
 * @scenario IsetBalanced is capped at the sum of all active node maximums
 * @given Master with 2 EVSEs in STATE_C, node 1 limited to BalancedMax=80 deciamps (8A)
 * @when Balanced current is calculated with IsetBalanced exceeding ActiveMax (320+80=400)
 * @then IsetBalanced is capped to 400, node 0 gets 320 and node 1 gets 80
 */
void test_isetbalanced_capped_at_active_max(void) {
    setup_master_n_evse(2);
    ctx.BalancedMax[1] = 80;  /* Node 1 max 8A */
    ctx.EVMeterImeasured = 0;
    /* IsetBalanced = 640, but ActiveMax = 320 + 80 = 400 */

    evse_calc_balanced_current(&ctx, 0);

    /* IsetBalanced is capped to ActiveMax(400). Then distribution:
     * EVSE[1] gets 80 (capped at its max).
     * EVSE[0] gets 400-80 = 320. */
    TEST_ASSERT_EQUAL_INT(320, ctx.Balanced[0]);
    TEST_ASSERT_EQUAL_INT(80, ctx.Balanced[1]);
}

/* ---- Unequal BalancedMax distribution ---- */

/*
 * @feature Multi-Node Load Balancing
 * @req REQ-MULTI-013
 * @scenario Three EVSEs with all different BalancedMax values
 * @given Master with 3 EVSEs: max 320, 160, 80 deciamps, MaxCircuit=64A
 * @when Balanced current is calculated
 * @then Each EVSE capped at its max, remainder redistributed to uncapped EVSEs
 */
void test_three_evse_all_different_max(void) {
    setup_master_n_evse(3);
    ctx.BalancedMax[0] = 320;  /* 32A */
    ctx.BalancedMax[1] = 160;  /* 16A */
    ctx.BalancedMax[2] = 80;   /* 8A */
    ctx.EVMeterImeasured = 0;

    evse_calc_balanced_current(&ctx, 0);

    /* IsetBalanced = 640. ActiveMax = 320+160+80 = 560.
     * IsetBalanced capped at 560. Distribution:
     * First pass: EVSE[2] capped at 80 (80 < 560/3=186).
     * Remaining: 560-80 = 480 for 2 EVSEs -> 240 each.
     * EVSE[1] capped at 160 (160 < 240).
     * Remaining for EVSE[0]: 560-80-160 = 320. Capped at 320. */
    TEST_ASSERT_LESS_OR_EQUAL(320, (int)ctx.Balanced[0]);
    TEST_ASSERT_LESS_OR_EQUAL(160, (int)ctx.Balanced[1]);
    TEST_ASSERT_LESS_OR_EQUAL(80, (int)ctx.Balanced[2]);
    /* Total should equal IsetBalanced (capped at ActiveMax=560) */
    int total = ctx.Balanced[0] + ctx.Balanced[1] + ctx.Balanced[2];
    TEST_ASSERT_EQUAL_INT(560, total);
}

/*
 * @feature Multi-Node Load Balancing
 * @req REQ-MULTI-014
 * @scenario Tight circuit with unequal max: surplus from small EVSE redistributed
 * @given 2 EVSEs: EVSE[0] max 320, EVSE[1] max 60 (MinCurrent), MaxCircuit=25A
 * @when Balanced current is calculated
 * @then EVSE[1] gets 60, EVSE[0] gets remainder (250-60=190)
 */
void test_unequal_max_tight_circuit(void) {
    setup_master_n_evse(2);
    ctx.MaxCircuit = 25;
    ctx.BalancedMax[0] = 320;
    ctx.BalancedMax[1] = 60;   /* Can only take MinCurrent */
    ctx.EVMeterImeasured = 0;

    evse_calc_balanced_current(&ctx, 0);

    /* IsetBalanced = 250. ActiveMax = 320+60 = 380.
     * IsetBalanced = min(250, 380) = 250.
     * EVSE[1] capped at 60. Remaining: 250-60 = 190 for EVSE[0]. */
    TEST_ASSERT_EQUAL_INT(60, ctx.Balanced[1]);
    TEST_ASSERT_EQUAL_INT(190, ctx.Balanced[0]);
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("Multi-Node Load Balancing Edge Cases");

    RUN_TEST(test_four_evse_fair_distribution);
    RUN_TEST(test_four_evse_master_max_from_chargecurrent);
    RUN_TEST(test_one_evse_low_max_others_share);
    RUN_TEST(test_node_goes_offline_redistributes);
    RUN_TEST(test_all_nodes_mincurrent_during_shortage);
    RUN_TEST(test_maxcircuit_limits_total_distribution);
    RUN_TEST(test_maxcircuit_with_ev_meter_baseload);
    RUN_TEST(test_six_evse_fair_distribution);
    RUN_TEST(test_nocurrent_increments_on_hard_shortage);
    RUN_TEST(test_nocurrent_zero_when_sufficient);
    RUN_TEST(test_state_b_node_gets_no_current);
    RUN_TEST(test_isetbalanced_capped_at_active_max);
    RUN_TEST(test_three_evse_all_different_max);
    RUN_TEST(test_unequal_max_tight_circuit);

    TEST_SUITE_RESULTS();
}
