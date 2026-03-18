/*
 * test_lb_convergence.c - Multi-cycle load balancing convergence tests
 *
 * Issue #21: [Plan-02] Increment 1: Convergence Test Suite
 *
 * Tests multi-cycle convergence behavior of evse_calc_balanced_current().
 * Uses simulate_n_cycles() to run the regulation loop multiple times,
 * feeding back meter measurements that reflect the previous cycle's
 * Balanced[] allocations. This documents the current algorithm's
 * convergence characteristics — some tests may document known-bad behavior.
 *
 * Zero production code changes. Test-only.
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

/* ---- Simulation helper ----
 *
 * Runs evse_calc_balanced_current() for `cycles` iterations.
 * Between cycles, simulates meter feedback:
 *   - MainsMeterImeasured = baseload + sum(Balanced[active])
 *   - EVMeterImeasured = sum(Balanced[active])
 *   - Isum = MainsMeterImeasured (assumes single-phase for simplicity)
 *   - phasesLastUpdateFlag = true (new measurement each cycle)
 *
 * The baseload parameter represents non-EVSE consumption on the mains.
 */
static void simulate_n_cycles(evse_ctx_t *c, int cycles, int32_t baseload) {
    for (int i = 0; i < cycles; i++) {
        /* Feed back meter readings based on previous Balanced[] */
        int32_t total_ev = 0;
        for (int n = 0; n < NR_EVSES; n++) {
            if (c->BalancedState[n] == STATE_C)
                total_ev += c->Balanced[n];
        }
        c->EVMeterImeasured = (int16_t)total_ev;
        c->MainsMeterImeasured = (int16_t)(baseload + total_ev);
        c->Isum = c->MainsMeterImeasured;
        c->phasesLastUpdateFlag = true;

        evse_calc_balanced_current(c, 0);
    }
}

/* Helper: set up master with N EVSEs in Smart mode */
static void setup_smart_master_n(int n, uint16_t max_mains, int32_t baseload) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_SMART;
    ctx.LoadBl = 1;
    ctx.MaxCurrent = 32;
    ctx.MaxCapacity = 32;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 64;
    ctx.MaxMains = max_mains;
    ctx.ChargeCurrent = 320;
    ctx.MainsMeterType = 1;
    ctx.EVMeterType = 1;
    ctx.phasesLastUpdateFlag = true;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.EmaAlpha = 100;        /* No EMA smoothing for convergence tests */
    ctx.SmartDeadBand = 0;     /* No dead band */
    ctx.RampRateDivisor = 1;   /* No ramp rate limiting in regulation */
    ctx.MaxRampRate = 0;       /* No ramp rate limiter (master, not standalone) */
    ctx.SettlingWindow = 0;    /* No settling suppression */
    ctx.NoCurrentThreshold = NOCURRENT_THRESHOLD_DEFAULT;
    ctx.SolarMinRunTime = 0;

    for (int i = 0; i < n && i < NR_EVSES; i++) {
        ctx.BalancedState[i] = STATE_C;
        ctx.BalancedMax[i] = 320;
        ctx.Balanced[i] = (uint16_t)(ctx.MinCurrent * 10); /* Start at min */
        ctx.Node[i].Online = 1;
        ctx.Node[i].IntTimer = 100;
    }

    /* Set initial meter readings based on starting Balanced values */
    int32_t total_ev = 0;
    for (int i = 0; i < n && i < NR_EVSES; i++)
        total_ev += ctx.Balanced[i];
    ctx.EVMeterImeasured = (int16_t)total_ev;
    ctx.MainsMeterImeasured = (int16_t)(baseload + total_ev);
    ctx.Isum = ctx.MainsMeterImeasured;
    ctx.IsetBalanced = (int32_t)total_ev;
}

/* Helper: set up standalone EVSE in Smart mode */
static void setup_smart_standalone(uint16_t max_mains, int32_t baseload) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_SMART;
    ctx.LoadBl = 0;
    ctx.MaxCurrent = 32;
    ctx.MaxCapacity = 32;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 32;
    ctx.MaxMains = max_mains;
    ctx.ChargeCurrent = 320;
    ctx.MainsMeterType = 1;
    ctx.EVMeterType = 1;
    ctx.phasesLastUpdateFlag = true;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.EmaAlpha = 100;
    ctx.SmartDeadBand = 0;
    ctx.RampRateDivisor = 1;
    ctx.MaxRampRate = 0;
    ctx.SettlingWindow = 0;
    ctx.NoCurrentThreshold = NOCURRENT_THRESHOLD_DEFAULT;
    ctx.SolarMinRunTime = 0;

    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 320;
    ctx.Balanced[0] = (uint16_t)(ctx.MinCurrent * 10);
    ctx.Node[0].Online = 1;
    ctx.Node[0].IntTimer = 100;

    int32_t total_ev = ctx.Balanced[0];
    ctx.EVMeterImeasured = (int16_t)total_ev;
    ctx.MainsMeterImeasured = (int16_t)(baseload + total_ev);
    ctx.Isum = ctx.MainsMeterImeasured;
    ctx.IsetBalanced = (int32_t)total_ev;
}

/* Helper: set up standalone EVSE in Solar mode */
static void setup_solar_standalone(uint16_t max_mains, int32_t baseload,
                                   uint16_t import_current) {
    setup_smart_standalone(max_mains, baseload);
    ctx.Mode = MODE_SOLAR;
    ctx.ImportCurrent = import_current;
    ctx.StartCurrent = 4;
    ctx.StopTime = 10;
    ctx.SolarFineDeadBand = SOLAR_FINE_DEADBAND_DEFAULT;
}

/* ========================================================================
 * GROUP A: Multi-cycle convergence (single EVSE, Smart mode)
 * ======================================================================== */

/*
 * @feature LB Convergence
 * @req REQ-LB-020
 * @scenario Standalone Smart mode converges to target within 20 cycles
 * @given A standalone EVSE in Smart mode with 25A mains limit and 50dA baseload
 * @when 20 regulation cycles are simulated with meter feedback
 * @then IsetBalanced stabilizes within 5 deciamps of the expected target (200dA)
 */
void test_smart_standalone_converges_to_target(void) {
    setup_smart_standalone(25, 50);  /* 25A mains, 5A baseload */

    simulate_n_cycles(&ctx, 20, 50);

    /* Target: (MaxMains*10) - baseload = 250 - 50 = 200 dA
     * With EVMeter: min(MaxMains*10 - Mains, MaxCircuit*10 - EV) applies.
     * Check convergence within 5 dA of target. */
    int32_t target = 200;  /* (25*10) - 50 = 200 */
    int32_t diff = ctx.IsetBalanced - target;
    if (diff < 0) diff = -diff;
    TEST_ASSERT_LESS_OR_EQUAL(5, diff);
}

/*
 * @feature LB Convergence
 * @req REQ-LB-021
 * @scenario Smart mode converges monotonically when starting below target
 * @given A standalone EVSE in Smart mode starting at MinCurrent (60dA) with headroom
 * @when 10 regulation cycles are simulated
 * @then IsetBalanced increases each cycle (monotonic convergence upward)
 */
void test_smart_standalone_monotonic_increase(void) {
    setup_smart_standalone(25, 50);
    ctx.Balanced[0] = 60;
    ctx.IsetBalanced = 60;

    int32_t prev = ctx.IsetBalanced;
    bool monotonic = true;
    for (int i = 0; i < 10; i++) {
        int32_t total_ev = ctx.Balanced[0];
        ctx.EVMeterImeasured = (int16_t)total_ev;
        ctx.MainsMeterImeasured = (int16_t)(50 + total_ev);
        ctx.Isum = ctx.MainsMeterImeasured;
        ctx.phasesLastUpdateFlag = true;

        evse_calc_balanced_current(&ctx, 0);

        if (ctx.IsetBalanced < prev && (prev - ctx.IsetBalanced) > 1) {
            monotonic = false;
            break;
        }
        prev = ctx.IsetBalanced;
    }
    TEST_ASSERT_TRUE(monotonic);
}

/*
 * @feature LB Convergence
 * @req REQ-LB-022
 * @scenario Smart mode recovers when mains load increases mid-session
 * @given A standalone EVSE converged to 200dA with 5A baseload
 * @when Baseload suddenly increases by 100dA (10A) reducing available capacity
 * @then After 20 more cycles, IsetBalanced settles near the new target (100dA)
 */
void test_smart_standalone_recovers_from_load_increase(void) {
    setup_smart_standalone(25, 50);
    simulate_n_cycles(&ctx, 20, 50);

    /* Sudden baseload increase: 50 -> 150 dA */
    simulate_n_cycles(&ctx, 20, 150);

    /* New target: (250 - 150) = 100 dA */
    int32_t target = 100;
    int32_t diff = ctx.IsetBalanced - target;
    if (diff < 0) diff = -diff;
    TEST_ASSERT_LESS_OR_EQUAL(10, diff);
}

/*
 * @feature LB Convergence
 * @req REQ-LB-023
 * @scenario Smart mode recovers when mains load decreases mid-session
 * @given A standalone EVSE converged with 15A baseload
 * @when Baseload drops by 100dA (10A) increasing available capacity
 * @then After 20 more cycles, IsetBalanced settles near the new higher target
 */
void test_smart_standalone_recovers_from_load_decrease(void) {
    setup_smart_standalone(25, 150);
    simulate_n_cycles(&ctx, 20, 150);

    /* Baseload drops: 150 -> 50 dA */
    simulate_n_cycles(&ctx, 20, 50);

    int32_t target = 200;  /* (250 - 50) = 200 */
    int32_t diff = ctx.IsetBalanced - target;
    if (diff < 0) diff = -diff;
    TEST_ASSERT_LESS_OR_EQUAL(10, diff);
}

/* ========================================================================
 * GROUP E: Multi-node convergence
 * ======================================================================== */

/*
 * @feature LB Convergence
 * @req REQ-LB-024
 * @scenario Two EVSEs in Normal mode converge to equal distribution
 * @given Master with 2 EVSEs in Normal mode, MaxCircuit=32A, no baseload
 * @when 5 regulation cycles are simulated
 * @then Both EVSEs receive equal current within 1 deciamp
 */
void test_two_evse_normal_converges_equal(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 1;
    ctx.MaxCurrent = 32;
    ctx.MaxCapacity = 32;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 32;
    ctx.MaxMains = 50;
    ctx.ChargeCurrent = 320;
    ctx.phasesLastUpdateFlag = true;
    ctx.NoCurrentThreshold = NOCURRENT_THRESHOLD_DEFAULT;

    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedState[1] = STATE_C;
    ctx.BalancedMax[0] = 320;
    ctx.BalancedMax[1] = 320;
    ctx.Balanced[0] = 60;
    ctx.Balanced[1] = 60;
    ctx.Node[0].Online = 1;
    ctx.Node[1].Online = 1;
    ctx.Node[0].IntTimer = 100;
    ctx.Node[1].IntTimer = 100;

    /* Normal mode doesn't use meter feedback for IsetBalanced.
     * IsetBalanced = (MaxCircuit*10) - Baseload_EV.
     * Just run a few cycles. */
    for (int i = 0; i < 5; i++) {
        ctx.phasesLastUpdateFlag = true;
        evse_calc_balanced_current(&ctx, 0);
    }

    /* Each should get 320/2 = 160 */
    int diff = (int)ctx.Balanced[0] - (int)ctx.Balanced[1];
    if (diff < 0) diff = -diff;
    TEST_ASSERT_LESS_OR_EQUAL(1, diff);
    TEST_ASSERT_EQUAL_INT(160, ctx.Balanced[0]);
}

/*
 * @feature LB Convergence
 * @req REQ-LB-025
 * @scenario Two EVSEs in Smart mode converge to fair sharing
 * @given Master with 2 EVSEs in Smart mode, 25A mains limit, 5A baseload
 * @when 20 regulation cycles are simulated with meter feedback
 * @then Both EVSEs receive current within 5dA of each other
 */
void test_two_evse_smart_converges_fair(void) {
    setup_smart_master_n(2, 25, 50);  /* 25A mains, 5A baseload */

    simulate_n_cycles(&ctx, 20, 50);

    int diff = (int)ctx.Balanced[0] - (int)ctx.Balanced[1];
    if (diff < 0) diff = -diff;
    TEST_ASSERT_LESS_OR_EQUAL(5, diff);

    /* Both should be > MinCurrent */
    TEST_ASSERT_GREATER_THAN(60, ctx.Balanced[0]);
    TEST_ASSERT_GREATER_THAN(60, ctx.Balanced[1]);
}

/*
 * @feature LB Convergence
 * @req REQ-LB-026
 * @scenario Four EVSEs in Smart mode converge with sufficient headroom
 * @given Master with 4 EVSEs in Smart mode, 50A mains limit, 5A baseload
 * @when 30 regulation cycles are simulated with meter feedback
 * @then All 4 EVSEs receive current within 10dA of each other
 */
void test_four_evse_smart_converges(void) {
    setup_smart_master_n(4, 50, 50);

    simulate_n_cycles(&ctx, 30, 50);

    /* Check all 4 are reasonably balanced */
    uint16_t min_bal = ctx.Balanced[0], max_bal = ctx.Balanced[0];
    for (int i = 1; i < 4; i++) {
        if (ctx.Balanced[i] < min_bal) min_bal = ctx.Balanced[i];
        if (ctx.Balanced[i] > max_bal) max_bal = ctx.Balanced[i];
    }
    TEST_ASSERT_LESS_OR_EQUAL(10, max_bal - min_bal);

    /* Each should be above MinCurrent */
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[i]);
    }
}

/*
 * @feature LB Convergence
 * @req REQ-LB-027
 * @scenario Third EVSE joining mid-session causes redistribution
 * @given Master with 2 EVSEs converged in Smart mode
 * @when A third EVSE starts charging (mod=1) and 20 more cycles run
 * @then All 3 EVSEs converge to fair sharing within 10dA
 */
void test_third_evse_joining_reconverges(void) {
    setup_smart_master_n(2, 40, 50);
    simulate_n_cycles(&ctx, 20, 50);

    /* Third EVSE joins */
    ctx.BalancedState[2] = STATE_C;
    ctx.BalancedMax[2] = 320;
    ctx.Balanced[2] = 60;
    ctx.Node[2].Online = 1;
    ctx.Node[2].IntTimer = 100;

    /* First call with mod=1 for new EVSE joining */
    ctx.phasesLastUpdateFlag = true;
    int32_t total_ev = ctx.Balanced[0] + ctx.Balanced[1] + ctx.Balanced[2];
    ctx.EVMeterImeasured = (int16_t)total_ev;
    ctx.MainsMeterImeasured = (int16_t)(50 + total_ev);
    ctx.Isum = ctx.MainsMeterImeasured;
    evse_calc_balanced_current(&ctx, 1);

    /* Then run normal cycles */
    simulate_n_cycles(&ctx, 20, 50);

    /* All 3 should be within 10 dA of each other */
    uint16_t min_bal = ctx.Balanced[0], max_bal = ctx.Balanced[0];
    for (int i = 1; i < 3; i++) {
        if (ctx.Balanced[i] < min_bal) min_bal = ctx.Balanced[i];
        if (ctx.Balanced[i] > max_bal) max_bal = ctx.Balanced[i];
    }
    TEST_ASSERT_LESS_OR_EQUAL(10, max_bal - min_bal);
}

/*
 * @feature LB Convergence
 * @req REQ-LB-028
 * @scenario EVSE disconnecting causes fair redistribution to remaining
 * @given Master with 3 EVSEs converged in Smart mode
 * @when EVSE 2 disconnects and 20 more cycles run
 * @then Remaining 2 EVSEs converge to fair sharing, each getting more than before
 */
void test_evse_disconnect_reconverges(void) {
    setup_smart_master_n(3, 40, 50);
    simulate_n_cycles(&ctx, 20, 50);

    uint16_t before_0 = ctx.Balanced[0];

    /* EVSE 2 disconnects */
    ctx.BalancedState[2] = STATE_A;
    ctx.Balanced[2] = 0;

    simulate_n_cycles(&ctx, 20, 50);

    /* Remaining 2 should each get more than before */
    TEST_ASSERT_GREATER_THAN(before_0, ctx.Balanced[0]);
    /* And be fair to each other */
    int diff = (int)ctx.Balanced[0] - (int)ctx.Balanced[1];
    if (diff < 0) diff = -diff;
    TEST_ASSERT_LESS_OR_EQUAL(5, diff);
}

/* ========================================================================
 * GROUP F: Capacity-limited scenarios
 * ======================================================================== */

/*
 * @feature LB Convergence
 * @req REQ-LB-029
 * @scenario MaxMains limits total EVSE allocation in Smart mode
 * @given Standalone EVSE in Smart mode, MaxMains=10A, 5A baseload
 * @when 20 regulation cycles are simulated
 * @then IsetBalanced converges to MinCurrent (60dA) since available (50dA) < MinCurrent
 *       The algorithm inflates IsetBalanced to ActiveEVSE*MinCurrent*10 during shortage.
 */
void test_maxmains_caps_convergence(void) {
    setup_smart_standalone(10, 50);  /* 10A mains, 5A baseload */

    simulate_n_cycles(&ctx, 20, 50);

    /* Available headroom: (100 - 50) = 50 dA, but MinCurrent = 60 dA.
     * The algorithm sets IsetBalanced = ActiveEVSE * MinCurrent * 10 = 60
     * when in shortage, then NoCurrent increments. */
    TEST_ASSERT_EQUAL_INT(60, ctx.IsetBalanced);
    /* NoCurrent should be incrementing due to hard shortage */
    TEST_ASSERT_GREATER_THAN(0, ctx.NoCurrent);
}

/*
 * @feature LB Convergence
 * @req REQ-LB-030
 * @scenario Tight capacity with 4 EVSEs triggers priority scheduling
 * @given Master with 4 EVSEs, only enough power for 2 at MinCurrent
 * @when 10 regulation cycles are simulated
 * @then NoCurrent stays at 0 (priority scheduling handles shortage gracefully)
 */
void test_tight_capacity_four_evse_priority(void) {
    setup_smart_master_n(4, 15, 50);
    /* Available: (150 - 50) = 100 dA. Need: 4*60 = 240 dA. Hard shortage. */
    /* Priority scheduling should pause lower-priority EVSEs. */

    simulate_n_cycles(&ctx, 10, 50);

    /* Priority scheduling: at least one EVSE should be active */
    bool any_active = false;
    for (int i = 0; i < 4; i++) {
        if (ctx.Balanced[i] >= 60)
            any_active = true;
    }
    TEST_ASSERT_TRUE(any_active);

    /* NoCurrent should stay 0 (deliberate pause, not hard shortage) */
    TEST_ASSERT_EQUAL_INT(0, ctx.NoCurrent);
}

/*
 * @feature LB Convergence
 * @req REQ-LB-031
 * @scenario Hard shortage with single EVSE triggers NoCurrent
 * @given Standalone EVSE in Smart mode, mains heavily overloaded
 * @when Multiple regulation cycles are simulated with baseload exceeding MaxMains
 * @then NoCurrent counter increments indicating sustained shortage
 */
void test_hard_shortage_standalone_triggers_nocurrent(void) {
    setup_smart_standalone(10, 120);
    /* Baseload 120 > MaxMains*10 = 100. Impossible to serve EVSE. */

    simulate_n_cycles(&ctx, 15, 120);

    /* Should have triggered NoCurrent */
    TEST_ASSERT_GREATER_THAN(0, ctx.NoCurrent);
}

/*
 * @feature LB Convergence
 * @req REQ-LB-032
 * @scenario MaxCircuit limits per-EVSE allocation independently of MaxMains
 * @given Standalone EVSE in Smart mode, MaxCircuit=10A, MaxMains=50A, 3A baseload
 * @when 20 regulation cycles are simulated
 * @then Balanced[0] does not exceed MaxCircuit*10 (100dA)
 */
void test_maxcircuit_limits_convergence(void) {
    setup_smart_standalone(50, 30);
    ctx.MaxCircuit = 10;

    simulate_n_cycles(&ctx, 20, 30);

    /* MaxCircuit guard: Balanced[0] should not exceed 100 */
    TEST_ASSERT_LESS_OR_EQUAL(100, ctx.Balanced[0]);
}

/*
 * @feature LB Convergence
 * @req REQ-LB-033
 * @scenario MaxSumMains limit overrides MaxMains when configured
 * @given Standalone EVSE in Smart mode with MaxSumMains=15A
 * @when 20 regulation cycles are simulated with Isum near the limit
 * @then IsetBalanced is constrained by MaxSumMains, not just MaxMains
 */
void test_maxsummains_constrains_convergence(void) {
    setup_smart_standalone(50, 50);
    ctx.MaxSumMains = 15;  /* Sum limit: 150 dA */

    simulate_n_cycles(&ctx, 20, 50);

    /* With MaxSumMains=15: Idifference = (150 - Isum).
     * Isum = 50 + Balanced[0]. Target converges to Balanced[0] = 100. */
    TEST_ASSERT_LESS_OR_EQUAL(110, ctx.Balanced[0]);
}

/* ========================================================================
 * GROUP: Solar mode convergence
 * ======================================================================== */

/*
 * @feature LB Convergence
 * @req REQ-LB-034
 * @scenario Solar mode converges to export surplus
 * @given Standalone EVSE in Solar mode, large export (Isum negative)
 * @when 30 regulation cycles are simulated with meter feedback
 * @then IsetBalanced increases to absorb available solar surplus
 */
void test_solar_standalone_converges_to_surplus(void) {
    /* Simulate solar export: baseload = -200 dA (20A export) */
    setup_solar_standalone(25, -200, 0);

    simulate_n_cycles(&ctx, 30, -200);

    /* With 20A export, EVSE should be charging.
     * Exact value depends on fine regulation. Check it's above MinCurrent. */
    TEST_ASSERT_GREATER_THAN(60, ctx.Balanced[0]);
}

/*
 * @feature LB Convergence
 * @req REQ-LB-035
 * @scenario Solar mode with ImportCurrent allows partial grid import
 * @given Standalone EVSE in Solar mode with ImportCurrent=6A
 * @when 30 regulation cycles run with modest solar export
 * @then EVSE charges above pure-solar level due to allowed import
 */
void test_solar_import_current_allows_grid_use(void) {
    /* Baseload = -80 dA (8A export). ImportCurrent = 6A allows 6A import. */
    setup_solar_standalone(25, -80, 6);

    simulate_n_cycles(&ctx, 30, -80);

    /* With 8A export + 6A allowed import, up to 14A available.
     * EVSE should be charging above MinCurrent. */
    TEST_ASSERT_GREATER_OR_EQUAL(60, ctx.Balanced[0]);
}

/* ========================================================================
 * GROUP: Stability tests (oscillation detection)
 * ======================================================================== */

/*
 * @feature LB Convergence
 * @req REQ-LB-036
 * @scenario Converged Smart mode EVSE remains stable (no oscillation)
 * @given A standalone EVSE converged in Smart mode
 * @when 20 additional regulation cycles run with constant conditions
 * @then IsetBalanced varies by no more than 5dA across all cycles
 */
void test_smart_stability_no_oscillation(void) {
    setup_smart_standalone(25, 50);
    simulate_n_cycles(&ctx, 30, 50);  /* Converge */

    /* Now measure stability over 20 more cycles */
    int32_t min_iset = ctx.IsetBalanced, max_iset = ctx.IsetBalanced;
    for (int i = 0; i < 20; i++) {
        int32_t total_ev = ctx.Balanced[0];
        ctx.EVMeterImeasured = (int16_t)total_ev;
        ctx.MainsMeterImeasured = (int16_t)(50 + total_ev);
        ctx.Isum = ctx.MainsMeterImeasured;
        ctx.phasesLastUpdateFlag = true;

        evse_calc_balanced_current(&ctx, 0);

        if (ctx.IsetBalanced < min_iset) min_iset = ctx.IsetBalanced;
        if (ctx.IsetBalanced > max_iset) max_iset = ctx.IsetBalanced;
    }

    /* Oscillation check: max swing should be small */
    TEST_ASSERT_LESS_OR_EQUAL(5, max_iset - min_iset);
}

/*
 * @feature LB Convergence
 * @req REQ-LB-037
 * @scenario Two-EVSE Smart mode remains stable after convergence
 * @given Master with 2 EVSEs converged in Smart mode
 * @when 20 additional regulation cycles run with constant conditions
 * @then Balanced[0] and Balanced[1] each vary by no more than 5dA
 */
void test_two_evse_stability_no_oscillation(void) {
    setup_smart_master_n(2, 25, 50);
    simulate_n_cycles(&ctx, 30, 50);  /* Converge */

    uint16_t min0 = ctx.Balanced[0], max0 = ctx.Balanced[0];
    uint16_t min1 = ctx.Balanced[1], max1 = ctx.Balanced[1];
    for (int i = 0; i < 20; i++) {
        int32_t total_ev = ctx.Balanced[0] + ctx.Balanced[1];
        ctx.EVMeterImeasured = (int16_t)total_ev;
        ctx.MainsMeterImeasured = (int16_t)(50 + total_ev);
        ctx.Isum = ctx.MainsMeterImeasured;
        ctx.phasesLastUpdateFlag = true;

        evse_calc_balanced_current(&ctx, 0);

        if (ctx.Balanced[0] < min0) min0 = ctx.Balanced[0];
        if (ctx.Balanced[0] > max0) max0 = ctx.Balanced[0];
        if (ctx.Balanced[1] < min1) min1 = ctx.Balanced[1];
        if (ctx.Balanced[1] > max1) max1 = ctx.Balanced[1];
    }

    TEST_ASSERT_LESS_OR_EQUAL(5, max0 - min0);
    TEST_ASSERT_LESS_OR_EQUAL(5, max1 - min1);
}

/* ========================================================================
 * GROUP: Adaptive gain / oscillation dampening (Issue #22)
 * ======================================================================== */

/*
 * @feature LB Convergence
 * @req REQ-LB-038
 * @scenario Oscillation detection increments OscillationCount on sign flip
 * @given Standalone EVSE in Smart mode with alternating positive/negative Idifference
 * @when Regulation cycles produce sign flips in Idifference
 * @then OscillationCount increments, indicating detected oscillation
 */
void test_oscillation_detected_on_sign_flip(void) {
    setup_smart_standalone(25, 50);
    ctx.RampRateDivisor = 1;

    /* Cycle 1: large headroom (Idifference positive) */
    ctx.MainsMeterImeasured = 100;  /* Mains low -> headroom */
    ctx.EVMeterImeasured = 60;
    ctx.Isum = 100;
    ctx.phasesLastUpdateFlag = true;
    evse_calc_balanced_current(&ctx, 0);

    /* Cycle 2: sudden overload (Idifference negative) */
    ctx.MainsMeterImeasured = 280;  /* Mains high -> shortage */
    ctx.EVMeterImeasured = 200;
    ctx.Isum = 280;
    ctx.phasesLastUpdateFlag = true;
    evse_calc_balanced_current(&ctx, 0);

    /* Cycle 3: headroom again (sign flip back) */
    ctx.MainsMeterImeasured = 100;
    ctx.EVMeterImeasured = 60;
    ctx.Isum = 100;
    ctx.phasesLastUpdateFlag = true;
    evse_calc_balanced_current(&ctx, 0);

    /* After sign flips, OscillationCount should have incremented */
    TEST_ASSERT_GREATER_THAN(0, ctx.OscillationCount);
}

/*
 * @feature LB Convergence
 * @req REQ-LB-039
 * @scenario Adaptive gain increases effective divisor during oscillation
 * @given Standalone EVSE in Smart mode with OscillationCount > 0
 * @when Regulation cycle runs with positive Idifference
 * @then IsetBalanced increases by less than Idifference/RampRateDivisor
 *       (adaptive gain reduces the step size)
 */
void test_adaptive_gain_reduces_step_during_oscillation(void) {
    setup_smart_standalone(25, 50);
    ctx.RampRateDivisor = 2;
    ctx.IsetBalanced = 100;
    ctx.IsetBalancedPrev = 100;

    /* Manually set oscillation state */
    ctx.OscillationCount = 3;

    /* Apply a regulation cycle with headroom */
    ctx.MainsMeterImeasured = 100;  /* headroom = 250 - 100 = 150 */
    ctx.EVMeterImeasured = 60;
    ctx.Isum = 100;
    ctx.phasesLastUpdateFlag = true;

    int32_t before = ctx.IsetBalanced;
    evse_calc_balanced_current(&ctx, 0);

    /* With Idifference ~150 and base divisor=2, non-adaptive step = 75.
     * Adaptive gain should use higher effective divisor, so step < 75. */
    int32_t step = ctx.IsetBalanced - before;
    TEST_ASSERT_TRUE(step > 0);       /* Still increases */
    TEST_ASSERT_TRUE(step < 75);      /* But less than non-adaptive */
}

/*
 * @feature LB Convergence
 * @req REQ-LB-040
 * @scenario OscillationCount decays when no sign flip occurs
 * @given Standalone EVSE in Smart mode with OscillationCount = 5
 * @when Multiple consecutive regulation cycles have same-sign Idifference
 * @then OscillationCount decays back toward 0
 */
void test_oscillation_count_decays_when_stable(void) {
    setup_smart_standalone(25, 50);

    /* Start with elevated oscillation count */
    ctx.OscillationCount = 5;
    ctx.IdiffPrev = 50;  /* Previous Idifference was positive */

    /* Run several stable cycles with consistent positive headroom */
    for (int i = 0; i < 10; i++) {
        int32_t total_ev = ctx.Balanced[0];
        ctx.EVMeterImeasured = (int16_t)total_ev;
        ctx.MainsMeterImeasured = (int16_t)(50 + total_ev);
        ctx.Isum = ctx.MainsMeterImeasured;
        ctx.phasesLastUpdateFlag = true;
        evse_calc_balanced_current(&ctx, 0);
    }

    /* After 10 stable cycles, OscillationCount should have decayed */
    TEST_ASSERT_TRUE(ctx.OscillationCount < 5);
}

/*
 * @feature LB Convergence
 * @req REQ-LB-041
 * @scenario Adaptive gain improves convergence under alternating load
 * @given Standalone EVSE in Smart mode with alternating baseload (simulating noisy grid)
 * @when 40 regulation cycles are simulated with alternating +-20dA baseload noise
 * @then IsetBalanced peak-to-peak oscillation is less than 30dA (dampened)
 */
void test_adaptive_gain_dampens_noisy_load(void) {
    setup_smart_standalone(25, 50);

    /* Converge first with stable load */
    simulate_n_cycles(&ctx, 20, 50);

    /* Now introduce alternating load noise */
    int32_t min_iset = ctx.IsetBalanced, max_iset = ctx.IsetBalanced;
    for (int i = 0; i < 40; i++) {
        int32_t noise = (i % 2 == 0) ? 20 : -20;
        int32_t total_ev = ctx.Balanced[0];
        ctx.EVMeterImeasured = (int16_t)total_ev;
        ctx.MainsMeterImeasured = (int16_t)(50 + noise + total_ev);
        ctx.Isum = ctx.MainsMeterImeasured;
        ctx.phasesLastUpdateFlag = true;
        evse_calc_balanced_current(&ctx, 0);

        if (ctx.IsetBalanced < min_iset) min_iset = ctx.IsetBalanced;
        if (ctx.IsetBalanced > max_iset) max_iset = ctx.IsetBalanced;
    }

    /* Adaptive gain should dampen the oscillation.
     * Without it, +-20dA noise / divisor=1 would cause ~40dA swing. */
    TEST_ASSERT_LESS_OR_EQUAL(30, max_iset - min_iset);
}

/*
 * @feature LB Convergence
 * @req REQ-LB-042
 * @scenario Normal mode is unaffected by adaptive gain
 * @given Standalone EVSE in Normal mode
 * @when Regulation cycles run
 * @then OscillationCount remains 0 (adaptive gain only applies to Smart/Solar)
 */
void test_normal_mode_no_adaptive_gain(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;
    ctx.MaxCurrent = 16;
    ctx.MaxCapacity = 16;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 16;
    ctx.ChargeCurrent = 160;
    ctx.phasesLastUpdateFlag = true;
    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 160;
    ctx.Balanced[0] = 160;
    ctx.NoCurrentThreshold = NOCURRENT_THRESHOLD_DEFAULT;

    for (int i = 0; i < 10; i++) {
        ctx.phasesLastUpdateFlag = true;
        evse_calc_balanced_current(&ctx, 0);
    }

    TEST_ASSERT_EQUAL_INT(0, ctx.OscillationCount);
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("LB Convergence (Plan-02, Issues #21-#22)");

    /* Group A: Single EVSE multi-cycle convergence */
    RUN_TEST(test_smart_standalone_converges_to_target);
    RUN_TEST(test_smart_standalone_monotonic_increase);
    RUN_TEST(test_smart_standalone_recovers_from_load_increase);
    RUN_TEST(test_smart_standalone_recovers_from_load_decrease);

    /* Group E: Multi-node convergence */
    RUN_TEST(test_two_evse_normal_converges_equal);
    RUN_TEST(test_two_evse_smart_converges_fair);
    RUN_TEST(test_four_evse_smart_converges);
    RUN_TEST(test_third_evse_joining_reconverges);
    RUN_TEST(test_evse_disconnect_reconverges);

    /* Group F: Capacity-limited scenarios */
    RUN_TEST(test_maxmains_caps_convergence);
    RUN_TEST(test_tight_capacity_four_evse_priority);
    RUN_TEST(test_hard_shortage_standalone_triggers_nocurrent);
    RUN_TEST(test_maxcircuit_limits_convergence);
    RUN_TEST(test_maxsummains_constrains_convergence);

    /* Solar mode convergence */
    RUN_TEST(test_solar_standalone_converges_to_surplus);
    RUN_TEST(test_solar_import_current_allows_grid_use);

    /* Stability / oscillation detection */
    RUN_TEST(test_smart_stability_no_oscillation);
    RUN_TEST(test_two_evse_stability_no_oscillation);

    /* Adaptive gain / oscillation dampening (Issue #22) */
    RUN_TEST(test_oscillation_detected_on_sign_flip);
    RUN_TEST(test_adaptive_gain_reduces_step_during_oscillation);
    RUN_TEST(test_oscillation_count_decays_when_stable);
    RUN_TEST(test_adaptive_gain_dampens_noisy_load);
    RUN_TEST(test_normal_mode_no_adaptive_gain);

    TEST_SUITE_RESULTS();
}
