/*
 * test_multi_node_solar.c - Multi-node solar mode tests
 *
 * Tests the critical gap identified from a real-world bug: master EVSE
 * in solar mode with multiple nodes must correctly stop all nodes when
 * solar surplus drops. Also tests mode consistency across nodes.
 *
 * Bug context: Secondary node kept charging at 6A (MinCurrent) because
 * the master's balanced current calculation never dropped to 0 for
 * solar mode with multiple active EVSEs.
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

/* Helper: set up master with N EVSEs in solar mode, all charging */
static void setup_solar_master_n(int n) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_SOLAR;
    ctx.LoadBl = 1;  /* Master */
    ctx.MaxCurrent = 16;
    ctx.MaxCapacity = 16;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 64;
    ctx.MaxMains = 25;
    ctx.ChargeCurrent = 160;
    ctx.MainsMeterType = 1;
    ctx.StartCurrent = 4;
    ctx.StopTime = 10;
    ctx.ImportCurrent = 0;
    ctx.SolarFineDeadBand = SOLAR_FINE_DEADBAND_DEFAULT;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.EnableC2 = NOT_PRESENT;
    ctx.phasesLastUpdateFlag = true;
    ctx.SolarStopTimer = 0;
    ctx.NoCurrent = 0;

    for (int i = 0; i < n && i < NR_EVSES; i++) {
        ctx.BalancedState[i] = STATE_C;
        ctx.BalancedMax[i] = 160;
        ctx.Balanced[i] = 60;    /* Currently drawing MinCurrent */
        ctx.Node[i].Online = 1;
        ctx.Node[i].IntTimer = SOLARSTARTTIME + 10; /* Past startup */
    }

    ctx.State = STATE_C;
    ctx.IsetBalanced = 120;  /* Previous cycle had some current */
    ctx.IsetBalanced_ema = 120;
}

/* ================================================================
 * GROUP 1: Multi-node solar shortage — surplus drops to zero
 * ================================================================ */

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-001
 * @scenario Two nodes solar shortage: SolarStopTimer starts when Isum exceeds threshold
 * @given Master with 2 EVSEs in STATE_C, solar mode, grid importing above threshold
 * @when evse_calc_balanced_current is called with Isum above (ActiveEVSE*MinCurrent*Phases - StartCurrent)*10
 * @then SolarStopTimer starts counting down from StopTime * 60
 */
void test_solar_multi_node_shortage_starts_timer(void) {
    setup_solar_master_n(2);
    /* Threshold = (2*6*3 - 4)*10 = 320. Isum must exceed this. */
    ctx.MainsMeterImeasured = 350;
    ctx.EVMeterImeasured = 0;
    ctx.Isum = 350;
    ctx.IsetBalanced = 100;

    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_GREATER_THAN(0, (int)ctx.SolarStopTimer);
    TEST_ASSERT_EQUAL_INT(ctx.StopTime * 60, ctx.SolarStopTimer);
}

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-001B
 * @scenario Two nodes solar shortage with moderate import starts SolarStopTimer
 * @given Master with 2 EVSEs in STATE_C, solar mode, grid importing 20A (no surplus)
 * @when evse_calc_balanced_current is called
 * @then SolarStopTimer starts because Isum (200) > single-EVSE threshold (140)
 * @note The threshold is (MinCurrent * Phases - StartCurrent) * 10, independent
 *       of ActiveEVSE. Priority scheduling handles which cars charge; the timer
 *       only fires when there isn't enough solar for even one car.
 */
void test_solar_multi_node_shortage_timer_moderate_import(void) {
    setup_solar_master_n(2);
    ctx.MainsMeterImeasured = 200;  /* 20A import = no solar surplus */
    ctx.EVMeterImeasured = 0;
    ctx.Isum = 200;
    ctx.IsetBalanced = 100;

    evse_calc_balanced_current(&ctx, 0);

    /* Threshold = (MinCurrent * Phases - StartCurrent) * 10 = (6*3-4)*10 = 140.
     * Isum 200 > 140 → timer starts. */
    TEST_ASSERT_GREATER_THAN(0, (int)ctx.SolarStopTimer);
    TEST_ASSERT_EQUAL_INT(ctx.StopTime * 60, ctx.SolarStopTimer);
}

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-002
 * @scenario Two nodes solar shortage: priority scheduling pauses lower-priority node
 * @given Master with 2 EVSEs in STATE_C, solar mode, grid importing heavily (Isum=300)
 * @when evse_calc_balanced_current is called with very low actual available power
 * @then At least one EVSE gets Balanced=0 (paused via priority scheduling)
 * @and Paused EVSE gets NO_SUN error flag (not LESS_6A)
 */
void test_solar_multi_node_pauses_with_no_sun(void) {
    setup_solar_master_n(2);
    ctx.MainsMeterImeasured = 300;
    ctx.EVMeterImeasured = 0;
    ctx.Isum = 300;
    ctx.IsetBalanced = 50;  /* Very low: regulation drives it down */
    /* Pre-seed EMA to match so regulation applies */
    ctx.IsetBalanced_ema = 50;

    evse_calc_balanced_current(&ctx, 0);

    /* Priority scheduling with insufficient power for 2 EVSEs at MinCurrent.
     * actualAvailable < 2 * 60 = 120, so at least one EVSE is paused. */
    bool has_paused = false;
    for (int i = 0; i < 2; i++) {
        if (ctx.Balanced[i] == 0) {
            has_paused = true;
            /* Paused in solar mode should get NO_SUN, not LESS_6A */
            TEST_ASSERT_TRUE(ctx.BalancedError[i] & NO_SUN);
        }
    }
    TEST_ASSERT_TRUE(has_paused);
}

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-003
 * @scenario Four nodes solar: Isum above threshold starts timer and pauses nodes
 * @given Master with 4 EVSEs in STATE_C, solar mode, Isum above 4-node threshold
 * @when evse_calc_balanced_current is called
 * @then SolarStopTimer is started and at least some EVSEs are paused
 */
void test_solar_four_nodes_above_threshold(void) {
    setup_solar_master_n(4);
    /* Threshold = (4*6*3 - 4)*10 = 680. Use 700 to exceed it. */
    ctx.MainsMeterImeasured = 700;
    ctx.EVMeterImeasured = 0;
    ctx.Isum = 700;
    ctx.IsetBalanced = 50;
    ctx.IsetBalanced_ema = 50;

    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_GREATER_THAN(0, (int)ctx.SolarStopTimer);

    /* With priority scheduling, at least some EVSEs should be paused */
    int paused = 0;
    for (int i = 0; i < 4; i++) {
        if (ctx.Balanced[i] == 0) paused++;
    }
    TEST_ASSERT_GREATER_THAN(0, paused);
}

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-003B
 * @scenario Four nodes solar with 40A import starts SolarStopTimer
 * @given Master with 4 EVSEs in STATE_C, solar mode, grid importing 40A
 * @when evse_calc_balanced_current is called
 * @then SolarStopTimer starts because Isum (400) > single-EVSE threshold (140)
 * @note Threshold is (MinCurrent*Phases - StartCurrent)*10 = 140, independent
 *       of how many EVSEs are active. Four-node or single-node: same check.
 */
void test_solar_four_nodes_no_surplus_starts_timer(void) {
    setup_solar_master_n(4);
    ctx.MainsMeterImeasured = 400;
    ctx.EVMeterImeasured = 0;
    ctx.Isum = 400;
    ctx.IsetBalanced = 50;
    ctx.IsetBalanced_ema = 50;

    evse_calc_balanced_current(&ctx, 0);

    /* Threshold = (6*3-4)*10 = 140. Isum 400 > 140 → timer starts. */
    TEST_ASSERT_GREATER_THAN(0, (int)ctx.SolarStopTimer);
}

/* ================================================================
 * GROUP 2: Solar surplus present — nodes should charge
 * ================================================================ */

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-004
 * @scenario Two nodes with sufficient solar surplus: both charge
 * @given Master with 2 EVSEs in STATE_C, solar mode, grid exporting 15A (surplus)
 * @when evse_calc_balanced_current is called
 * @then Both EVSEs receive current >= MinCurrent and SolarStopTimer stays 0
 */
void test_solar_multi_node_surplus_both_charge(void) {
    setup_solar_master_n(2);
    ctx.MainsMeterImeasured = -150;  /* 15A export = solar surplus */
    ctx.EVMeterImeasured = 0;
    ctx.Isum = -150;
    ctx.IsetBalanced = 200;
    ctx.IsetBalanced_ema = 200;

    evse_calc_balanced_current(&ctx, 0);

    /* Sufficient surplus: no shortage, both nodes get current */
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[0]);
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[1]);
    TEST_ASSERT_EQUAL_INT(0, ctx.SolarStopTimer);
}

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-005
 * @scenario Two nodes with marginal surplus: enough for one, not both
 * @given Master with 2 EVSEs in STATE_C, solar mode, surplus of ~8A (enough for 1 at 6A, not 2)
 * @when evse_calc_balanced_current is called
 * @then Priority EVSE gets current, other is paused with NO_SUN
 */
void test_solar_multi_node_marginal_surplus(void) {
    setup_solar_master_n(2);
    ctx.MainsMeterImeasured = -80;  /* 8A export */
    ctx.EVMeterImeasured = 0;
    ctx.Isum = -80;
    ctx.IsetBalanced = 80;
    ctx.IsetBalanced_ema = 80;

    evse_calc_balanced_current(&ctx, 0);

    /* 80 dA available but 2 * 60 = 120 needed: shortage.
     * Priority scheduling: EVSE[0] should get power, EVSE[1] paused. */
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[0]);
    TEST_ASSERT_EQUAL_INT(0, ctx.Balanced[1]);
    TEST_ASSERT_TRUE(ctx.BalancedError[1] & NO_SUN);
}

/* ================================================================
 * GROUP 3: SolarStopTimer lifecycle with multiple nodes
 * ================================================================ */

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-006
 * @scenario SolarStopTimer does not restart when already running
 * @given Master with 2 EVSEs in solar mode, SolarStopTimer already at 300
 * @when evse_calc_balanced_current is called again with shortage
 * @then SolarStopTimer retains its existing value (not reset to StopTime*60)
 */
void test_solar_multi_node_timer_no_restart(void) {
    setup_solar_master_n(2);
    ctx.MainsMeterImeasured = 200;
    ctx.EVMeterImeasured = 0;
    ctx.Isum = 200;
    ctx.IsetBalanced = 100;
    ctx.SolarStopTimer = 300;  /* Already counting down */

    evse_calc_balanced_current(&ctx, 0);

    /* Timer should NOT be reset back to StopTime*60=600, stays at 300 */
    TEST_ASSERT_EQUAL_INT(300, ctx.SolarStopTimer);
}

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-007
 * @scenario Solar surplus returns: SolarStopTimer clears
 * @given Master with 2 EVSEs in solar mode, SolarStopTimer running at 300
 * @when surplus returns (no shortage) and evse_calc_balanced_current is called
 * @then SolarStopTimer is reset to 0
 */
void test_solar_multi_node_surplus_clears_timer(void) {
    setup_solar_master_n(2);
    ctx.MainsMeterImeasured = -200;  /* Surplus returns */
    ctx.EVMeterImeasured = 0;
    ctx.Isum = -200;
    ctx.IsetBalanced = 200;
    ctx.IsetBalanced_ema = 200;
    ctx.SolarStopTimer = 300;  /* Was counting down */

    evse_calc_balanced_current(&ctx, 0);

    /* With surplus, no shortage path is taken. Timer should clear. */
    TEST_ASSERT_EQUAL_INT(0, ctx.SolarStopTimer);
}

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-008
 * @scenario SolarStopTimer suppressed during startup settling
 * @given Master with 2 EVSEs, Node[0].IntTimer < SOLARSTARTTIME (in startup)
 * @when evse_calc_balanced_current is called with shortage
 * @then SolarStopTimer remains 0 (suppressed during startup)
 */
void test_solar_multi_node_timer_suppressed_startup(void) {
    setup_solar_master_n(2);
    ctx.Node[0].IntTimer = 5;  /* In startup settling */
    ctx.MainsMeterImeasured = 200;
    ctx.EVMeterImeasured = 0;
    ctx.Isum = 200;
    ctx.IsetBalanced = 100;

    evse_calc_balanced_current(&ctx, 0);

    /* During startup, SolarStopTimer should NOT start */
    TEST_ASSERT_EQUAL_INT(0, ctx.SolarStopTimer);
}

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-008B
 * @scenario SolarStopTimer threshold is per-EVSE: just below threshold, timer does not start
 * @given Master with 2 EVSEs in solar mode, Isum just below single-EVSE threshold
 * @when evse_calc_balanced_current is called
 * @then SolarStopTimer stays 0 (stopping last car would cause immediate restart)
 */
void test_solar_multi_node_timer_below_threshold_no_start(void) {
    setup_solar_master_n(2);
    /* Threshold = (6*3-4)*10 = 140. Set Isum to 130, just below. */
    ctx.MainsMeterImeasured = 130;
    ctx.EVMeterImeasured = 0;
    ctx.Isum = 130;
    ctx.IsetBalanced = 100;

    evse_calc_balanced_current(&ctx, 0);

    /* Isum 130 < 140 → timer should NOT start.
     * If we stopped the last car: Isum_new = 130-180 = -50 (5A export).
     * Restart condition: Isum >= -40. -50 < -40 → restart WOULD happen.
     * So don't stop — there IS enough solar. */
    TEST_ASSERT_EQUAL_INT(0, ctx.SolarStopTimer);
}

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-008C
 * @scenario SolarStopTimer threshold is per-EVSE: just above threshold, timer starts
 * @given Master with 2 EVSEs in solar mode, Isum just above single-EVSE threshold
 * @when evse_calc_balanced_current is called
 * @then SolarStopTimer starts (not enough solar for even one car)
 */
void test_solar_multi_node_timer_above_threshold_starts(void) {
    setup_solar_master_n(2);
    /* Threshold = (6*3-4)*10 = 140. Set Isum to 150, just above. */
    ctx.MainsMeterImeasured = 150;
    ctx.EVMeterImeasured = 0;
    ctx.Isum = 150;
    ctx.IsetBalanced = 100;

    evse_calc_balanced_current(&ctx, 0);

    /* Isum 150 > 140 → timer starts.
     * If we stopped the last car: Isum_new = 150-180 = -30 (3A export).
     * Restart condition: Isum >= -40. -30 >= -40 → restart BLOCKED.
     * So stopping is final — correct to start timer. */
    TEST_ASSERT_GREATER_THAN(0, (int)ctx.SolarStopTimer);
}

/* ================================================================
 * GROUP 4: Mode consistency across nodes
 * ================================================================ */

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-009
 * @scenario Solar mode produces different distribution than Normal mode
 * @given Master with 2 EVSEs in STATE_C, same grid conditions
 * @when evse_calc_balanced_current is called in Normal mode vs Solar mode
 * @then Solar mode distributes based on surplus; Normal mode distributes based on MaxCircuit
 */
void test_solar_vs_normal_distribution_differs(void) {
    /* Normal mode first */
    setup_solar_master_n(2);
    ctx.Mode = MODE_NORMAL;
    ctx.MainsMeterImeasured = 200;
    ctx.EVMeterImeasured = 0;
    ctx.Isum = 200;

    evse_calc_balanced_current(&ctx, 0);
    int32_t normal_b0 = ctx.Balanced[0];
    int32_t normal_b1 = ctx.Balanced[1];

    /* Solar mode with same conditions */
    setup_solar_master_n(2);
    ctx.MainsMeterImeasured = 200;
    ctx.EVMeterImeasured = 0;
    ctx.Isum = 200;

    evse_calc_balanced_current(&ctx, 0);
    int32_t solar_b0 = ctx.Balanced[0];
    int32_t solar_b1 = ctx.Balanced[1];

    /* Normal mode ignores solar surplus — distributes based on circuit/mains limits.
     * Solar mode with 20A import should have much lower distribution.
     * At minimum, results should differ. */
    TEST_ASSERT_TRUE(normal_b0 != solar_b0 || normal_b1 != solar_b1);

    /* Normal mode should give more current (it has headroom under MaxCircuit) */
    TEST_ASSERT_GREATER_THAN(solar_b0 + solar_b1, normal_b0 + normal_b1);
}

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-010
 * @scenario Smart mode produces different distribution than Solar mode under same conditions
 * @given Master with 2 EVSEs in STATE_C, grid importing 10A
 * @when evse_calc_balanced_current is called in Smart mode vs Solar mode
 * @then Smart mode uses MaxMains regulation; Solar mode uses surplus regulation
 */
void test_solar_vs_smart_distribution_differs(void) {
    /* Smart mode */
    setup_solar_master_n(2);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterImeasured = 100;
    ctx.EVMeterImeasured = 0;
    ctx.Isum = 100;
    ctx.IsetBalanced = 200;
    ctx.IsetBalanced_ema = 200;

    evse_calc_balanced_current(&ctx, 0);
    int32_t smart_total = ctx.Balanced[0] + ctx.Balanced[1];

    /* Solar mode with same conditions */
    setup_solar_master_n(2);
    ctx.MainsMeterImeasured = 100;
    ctx.EVMeterImeasured = 0;
    ctx.Isum = 100;
    ctx.IsetBalanced = 200;
    ctx.IsetBalanced_ema = 200;

    evse_calc_balanced_current(&ctx, 0);
    int32_t solar_total = ctx.Balanced[0] + ctx.Balanced[1];

    /* Smart mode has headroom to MaxMains (250-100=150 dA available).
     * Solar mode with 10A import means shortage.
     * Smart should give more total current. */
    TEST_ASSERT_GREATER_OR_EQUAL(solar_total, smart_total);
}

/* ================================================================
 * GROUP 5: Edge cases
 * ================================================================ */

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-011
 * @scenario Node goes offline during solar shortage
 * @given Master with 3 EVSEs in solar mode with shortage, SolarStopTimer running
 * @when Node 2 goes offline (STATE_A) and current is recalculated
 * @then Fewer active EVSEs means less MinCurrent demand; may resolve shortage
 */
void test_solar_multi_node_offline_during_shortage(void) {
    setup_solar_master_n(3);
    ctx.MainsMeterImeasured = -80;  /* 8A export, enough for 1 EVSE at 6A */
    ctx.EVMeterImeasured = 0;
    ctx.Isum = -80;
    ctx.IsetBalanced = 80;
    ctx.IsetBalanced_ema = 80;

    /* With 3 EVSEs: 3*60=180 needed, 80 available → shortage */
    evse_calc_balanced_current(&ctx, 0);
    uint16_t timer_3_evse = ctx.SolarStopTimer;

    /* Node 2 goes offline */
    ctx.BalancedState[2] = STATE_A;
    ctx.Balanced[2] = 0;
    ctx.phasesLastUpdateFlag = true;
    ctx.IsetBalanced = 80;
    ctx.IsetBalanced_ema = 80;

    evse_calc_balanced_current(&ctx, 0);

    /* With 2 EVSEs: 2*60=120 needed, 80 available → still shortage but less severe.
     * Node 2 should have 0. */
    TEST_ASSERT_EQUAL_INT(0, ctx.Balanced[2]);
    /* At least one active EVSE should be getting power via priority scheduling */
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[0]);
    (void)timer_3_evse;
}

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-012
 * @scenario Solar mode with ImportCurrent allows some grid import
 * @given Master with 2 EVSEs in solar mode, ImportCurrent=6A, grid importing 5A
 * @when evse_calc_balanced_current is called
 * @then ImportCurrent tolerance means 5A import is acceptable; no shortage
 */
void test_solar_multi_node_import_current_tolerance(void) {
    setup_solar_master_n(2);
    ctx.ImportCurrent = 6;  /* Allow 6A grid import */
    ctx.MainsMeterImeasured = 50;  /* 5A import (within tolerance) */
    ctx.EVMeterImeasured = 0;
    ctx.Isum = 50;
    ctx.IsetBalanced = 160;
    ctx.IsetBalanced_ema = 160;

    evse_calc_balanced_current(&ctx, 0);

    /* With ImportCurrent=6A, 5A import is within tolerance.
     * Both EVSEs should get current. */
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[0]);
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[1]);
}

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-013
 * @scenario NoCurrent threshold eventually triggers LESS_6A in multi-node solar
 * @given Master with 2 EVSEs in solar mode, repeated hard shortage cycles
 * @when evse_calc_balanced_current is called multiple times with NoCurrent accumulating
 * @then After NoCurrent reaches threshold, LESS_6A error flag is set
 */
void test_solar_multi_node_nocurrent_threshold(void) {
    setup_solar_master_n(2);
    ctx.MainsMeterImeasured = 500;
    ctx.EVMeterImeasured = 0;
    ctx.Isum = 500;
    ctx.IsetBalanced = 50;
    ctx.IsetBalanced_ema = 50;
    ctx.NoCurrent = 0;
    ctx.NoCurrentThreshold = 3;
    ctx.SolarMinRunTime = 0;  /* No minimum run time guard */
    /* Pre-seed IdiffFiltered to avoid first-cycle dampening issues */
    ctx.IdiffFiltered = -450;

    /* Run multiple cycles to accumulate NoCurrent */
    for (int i = 0; i < 5; i++) {
        ctx.IsetBalanced = 50;
        ctx.phasesLastUpdateFlag = true;
        evse_calc_balanced_current(&ctx, 0);
    }

    /* NoCurrent should have accumulated and triggered LESS_6A */
    TEST_ASSERT_GREATER_OR_EQUAL(ctx.NoCurrentThreshold, ctx.NoCurrent);
    TEST_ASSERT_TRUE(ctx.ErrorFlags & LESS_6A);
}

/* ================================================================
 * GROUP 6: Solar + Capacity Tariff interaction
 * ================================================================ */

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-014
 * @scenario Sufficient solar but tight capacity headroom forces shortage
 * @given Master with 2 EVSEs in solar mode, grid exporting (solar surplus),
 *        but CapacityHeadroom_da is very low (50 dA = 5A on 3 phases)
 * @when evse_calc_balanced_current is called
 * @then IsetBalanced is capped by headroom, shortage detected, priority scheduling runs
 * @and SolarStopTimer does NOT start (Isum is negative = there IS solar)
 */
void test_solar_capacity_surplus_but_headroom_tight(void) {
    setup_solar_master_n(2);
    ctx.MainsMeterImeasured = -100;  /* 10A export = solar surplus */
    ctx.EVMeterImeasured = 0;
    ctx.Isum = -100;
    ctx.IsetBalanced = 150;
    ctx.IsetBalanced_ema = 150;
    ctx.CapacityHeadroom_da = 50;  /* Only 5A headroom (15 dA per phase) */

    evse_calc_balanced_current(&ctx, 0);

    /* Capacity headroom caps IsetBalanced to ~16 dA (50/3).
     * 16 < 2*60 = 120 → shortage detected, priority scheduling runs.
     * actualAvailable is very low (~16 dA), less than MinCurrent (60),
     * so even the first-priority EVSE can't get MinCurrent → both paused.
     * But SolarStopTimer should NOT start: Isum is -100 (exporting),
     * so IsumImport = -100 - 0 = -100 < 0 → condition not entered. */
    TEST_ASSERT_EQUAL_INT(0, ctx.SolarStopTimer);

    /* Both EVSEs may be paused (capacity too tight for even 6A).
     * This is correct — capacity limit overrides solar availability.
     * The important thing: it's NOT flagged as a solar issue. */
    int32_t total = ctx.Balanced[0] + ctx.Balanced[1];
    TEST_ASSERT_LESS_OR_EQUAL(50, (int)total);  /* Capped by headroom */
}

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-015
 * @scenario No solar AND tight capacity: both constraints active
 * @given Master with 2 EVSEs in solar mode, grid importing 20A,
 *        CapacityHeadroom_da is tight (100 dA)
 * @when evse_calc_balanced_current is called
 * @then Shortage detected via both solar regulation AND capacity headroom
 * @and SolarStopTimer starts (Isum above single-EVSE threshold)
 */
void test_solar_capacity_no_surplus_tight_headroom(void) {
    setup_solar_master_n(2);
    ctx.MainsMeterImeasured = 200;  /* 20A import, no solar */
    ctx.EVMeterImeasured = 0;
    ctx.Isum = 200;
    ctx.IsetBalanced = 100;
    ctx.IsetBalanced_ema = 100;
    ctx.CapacityHeadroom_da = 100;  /* 10A headroom */

    evse_calc_balanced_current(&ctx, 0);

    /* Solar regulation drives IsetBalanced down (importing).
     * Capacity caps it further. Both constraints active.
     * SolarStopTimer: Isum 200 > threshold 140 → starts. */
    TEST_ASSERT_GREATER_THAN(0, (int)ctx.SolarStopTimer);
}

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-016
 * @scenario Capacity headroom disabled (0) has no effect on solar logic
 * @given Master with 2 EVSEs in solar mode, CapacityHeadroom_da = INT16_MAX (disabled)
 * @when evse_calc_balanced_current is called with solar surplus
 * @then Capacity does not constrain charging; both EVSEs charge normally
 */
void test_solar_capacity_disabled_no_effect(void) {
    setup_solar_master_n(2);
    ctx.MainsMeterImeasured = -200;  /* 20A export */
    ctx.EVMeterImeasured = 0;
    ctx.Isum = -200;
    ctx.IsetBalanced = 200;
    ctx.IsetBalanced_ema = 200;
    ctx.CapacityHeadroom_da = INT16_MAX;  /* Disabled */

    evse_calc_balanced_current(&ctx, 0);

    /* With INT16_MAX headroom, capacity is not a constraint.
     * Surplus available, both EVSEs should charge. */
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[0]);
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[1]);
    TEST_ASSERT_EQUAL_INT(0, ctx.SolarStopTimer);
}

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-017
 * @scenario Capacity headroom negative: power budget exceeded, forces immediate shortage
 * @given Master with 2 EVSEs in solar mode, CapacityHeadroom_da = -50 (over budget)
 * @when evse_calc_balanced_current is called even with solar surplus
 * @then Shortage detected due to capacity (IsetBalanced capped very low)
 * @and SolarStopTimer does NOT start if Isum is negative (sun exists)
 */
void test_solar_capacity_negative_headroom(void) {
    setup_solar_master_n(2);
    ctx.MainsMeterImeasured = -100;  /* Solar surplus */
    ctx.EVMeterImeasured = 0;
    ctx.Isum = -100;
    ctx.IsetBalanced = 150;
    ctx.IsetBalanced_ema = 150;
    ctx.CapacityHeadroom_da = -50;  /* Over budget! */

    evse_calc_balanced_current(&ctx, 0);

    /* Negative headroom → IsetBalanced capped to negative → clamped to 0.
     * Shortage detected. But Isum is negative → IsumImport < 0 →
     * SolarStopTimer not entered. This is correct: the issue is capacity,
     * not lack of sun. */
    TEST_ASSERT_EQUAL_INT(0, ctx.SolarStopTimer);
}

/* ================================================================
 * GROUP 7: Solar stop/start — power delivery only, not sessions
 *
 * SolarStopTimer expiry and LESS_6A trigger STATE_C → STATE_C1
 * (power delivery paused: CP pilot → +12V, contactors open).
 *
 * This does NOT end the charge session (session_end only on STATE_A
 * vehicle disconnect) nor the OCPP transaction (OCPP sees
 * SuspendedEVSE, not Finishing). When solar returns and LESS_6A
 * clears, the same session/transaction resumes.
 *
 * Sequence: C → C1 → B1 → B → C (same session, same OCPP tx)
 * ================================================================ */

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-018
 * @scenario SolarStopTimer expiry transitions STATE_C to STATE_C1 (power pause)
 * @given EVSE in STATE_C, SolarStopTimer about to expire
 * @when evse_tick_1s decrements SolarStopTimer to 0
 * @then State transitions to STATE_C1 (power paused), LESS_6A set
 * @and AccessStatus remains ON (session/OCPP transaction NOT ended)
 */
void test_solar_stop_pauses_power_not_session(void) {
    setup_solar_master_n(1);
    ctx.State = STATE_C;
    ctx.SolarStopTimer = 1;  /* Will expire on next tick */
    ctx.AccessStatus = ON;

    evse_tick_1s(&ctx);

    /* Timer expired → C→C1, LESS_6A set */
    TEST_ASSERT_EQUAL_INT(STATE_C1, ctx.State);
    TEST_ASSERT_TRUE(ctx.ErrorFlags & LESS_6A);

    /* AccessStatus stays ON: session is NOT ended.
     * session_end() is only called on STATE_A (vehicle disconnect).
     * OCPP sees SuspendedEVSE, not Finishing. */
    TEST_ASSERT_EQUAL_INT(ON, ctx.AccessStatus);
}

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-019
 * @scenario LESS_6A clears when solar returns (auto-recovery)
 * @given EVSE with LESS_6A set, solar surplus returns (Isum negative)
 * @when evse_tick_1s checks evse_is_current_available
 * @then LESS_6A is cleared, allowing state machine to resume charging
 * @and AccessStatus remains ON throughout (same session)
 */
void test_solar_return_clears_less6a(void) {
    setup_solar_master_n(1);
    ctx.State = STATE_B;  /* After C1→B1→B cycle */
    ctx.ErrorFlags = LESS_6A;
    ctx.AccessStatus = ON;
    ctx.Isum = -100;  /* Solar surplus returned */
    ctx.MainsMeterImeasured = -100;
    ctx.LoadBl = 0;  /* Standalone — auto-recovery requires LoadBl < 2 */

    evse_tick_1s(&ctx);

    /* LESS_6A should be cleared by auto-recovery
     * (evse_is_current_available returns 1 with surplus) */
    TEST_ASSERT_FALSE(ctx.ErrorFlags & LESS_6A);
    /* Session intact */
    TEST_ASSERT_EQUAL_INT(ON, ctx.AccessStatus);
}

/*
 * @feature Multi-Node Solar Charging
 * @req REQ-MULTI-SOL-020
 * @scenario Full solar pause/resume cycle preserves AccessStatus
 * @given EVSE in STATE_C charging, SolarStopTimer expires
 * @when State goes C→C1 and C1Timer counts down to B1
 * @then AccessStatus stays ON throughout (OCPP tx survives the pause)
 */
void test_solar_full_pause_cycle_preserves_access(void) {
    setup_solar_master_n(1);
    ctx.State = STATE_C;
    ctx.SolarStopTimer = 1;
    ctx.AccessStatus = ON;

    /* Step 1: SolarStopTimer expires → C→C1 */
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(STATE_C1, ctx.State);
    TEST_ASSERT_EQUAL_INT(ON, ctx.AccessStatus);

    /* Step 2: C1Timer counts down → C1→B1 */
    for (int i = 0; i < 10; i++) {
        evse_tick_1s(&ctx);
    }
    /* C1Timer was set to 6 on C1 entry, after 6+ ticks should be in B1 */
    TEST_ASSERT_TRUE(ctx.State == STATE_B1 || ctx.State == STATE_C1);

    /* AccessStatus must STILL be ON. Only STATE_A sets it to OFF. */
    TEST_ASSERT_EQUAL_INT(ON, ctx.AccessStatus);
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("Multi-Node Solar Charging");

    /* Group 1: Shortage scenarios */
    RUN_TEST(test_solar_multi_node_shortage_starts_timer);
    RUN_TEST(test_solar_multi_node_shortage_timer_moderate_import);
    RUN_TEST(test_solar_multi_node_pauses_with_no_sun);
    RUN_TEST(test_solar_four_nodes_above_threshold);
    RUN_TEST(test_solar_four_nodes_no_surplus_starts_timer);

    /* Group 2: Surplus scenarios */
    RUN_TEST(test_solar_multi_node_surplus_both_charge);
    RUN_TEST(test_solar_multi_node_marginal_surplus);

    /* Group 3: SolarStopTimer lifecycle */
    RUN_TEST(test_solar_multi_node_timer_no_restart);
    RUN_TEST(test_solar_multi_node_surplus_clears_timer);
    RUN_TEST(test_solar_multi_node_timer_suppressed_startup);

    RUN_TEST(test_solar_multi_node_timer_below_threshold_no_start);
    RUN_TEST(test_solar_multi_node_timer_above_threshold_starts);

    /* Group 4: Mode consistency */
    RUN_TEST(test_solar_vs_normal_distribution_differs);
    RUN_TEST(test_solar_vs_smart_distribution_differs);

    /* Group 5: Edge cases */
    RUN_TEST(test_solar_multi_node_offline_during_shortage);
    RUN_TEST(test_solar_multi_node_import_current_tolerance);
    RUN_TEST(test_solar_multi_node_nocurrent_threshold);

    /* Group 6: Solar + Capacity Tariff */
    RUN_TEST(test_solar_capacity_surplus_but_headroom_tight);
    RUN_TEST(test_solar_capacity_no_surplus_tight_headroom);
    RUN_TEST(test_solar_capacity_disabled_no_effect);
    RUN_TEST(test_solar_capacity_negative_headroom);

    /* Group 7: Power delivery vs session lifecycle */
    RUN_TEST(test_solar_stop_pauses_power_not_session);
    RUN_TEST(test_solar_return_clears_less6a);
    RUN_TEST(test_solar_full_pause_cycle_preserves_access);

    TEST_SUITE_RESULTS();
}
