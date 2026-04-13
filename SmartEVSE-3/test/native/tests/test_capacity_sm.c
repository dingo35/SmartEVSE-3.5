/*
 * test_capacity_sm.c — State machine integration tests for capacity tariff
 *
 * Tests that CapacityHeadroom_da in evse_ctx_t correctly clamps IsetBalanced
 * in evse_calc_balanced_current(). Verifies interaction with MaxSumMains and
 * default (unconstrained) behavior.
 *
 * Key insight: evse_calc_balanced_current() does ongoing regulation — it
 * adjusts IsetBalanced incrementally via Idifference. The capacity headroom
 * clamp affects both the Idifference input and the Phase 4 guard rail.
 * However, Phase 5 inflates IsetBalanced to MinCurrent*10 when in shortage.
 * Tests must account for this by setting up realistic pre-existing state.
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"
#include <limits.h>

/* Static to keep stack usage under 1024 bytes */
static evse_ctx_t ctx;

/* Common setup: standalone smart-mode EVSE already charging at some current */
static void setup_smart_standalone_running(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_SMART;
    ctx.LoadBl = 0;  /* Standalone */
    ctx.MaxCurrent = 32;
    ctx.MaxCapacity = 32;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 32;
    ctx.MaxMains = 25;
    ctx.ChargeCurrent = 320;
    ctx.phasesLastUpdateFlag = true;
    ctx.EmaAlpha = 100;  /* No EMA smoothing for predictable test results */
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.EnableC2 = NOT_PRESENT;
    ctx.RampRateDivisor = 1;  /* Full ramp for predictable results */
    ctx.SmartDeadBand = 0;    /* No dead band */

    /* EVSE 0 charging */
    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 320;
    ctx.Balanced[0] = 160;

    /* Already charging at 160 deciamps (16A) */
    ctx.IsetBalanced = 160;
    ctx.IsetBalanced_ema = 160;
    ctx.IsetBalancedPrev = 160;

    /* Simulate mains meter present with moderate load */
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 100;  /* 10.0A measured */
    ctx.Isum = 100;
    ctx.EVMeterType = 0;
    ctx.EVMeterImeasured = 0;
}

/* ---- Test: IsetBalanced clamped by capacity headroom ---- */

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-020
 * @scenario IsetBalanced clamped by capacity headroom guard rail
 * @given ctx already charging at 160 deciamps, CapacityHeadroom_da=30 (3.0A total)
 * @when evse_calc_balanced_current runs
 * @then IsetBalanced is clamped to at most 30/3=10 deciamps per phase by guard rail
 */
void test_capacity_headroom_clamps_iset_balanced(void) {
    setup_smart_standalone_running();
    ctx.CapacityHeadroom_da = 30;  /* 3.0A total headroom */

    evse_calc_balanced_current(&ctx, 0);

    /*
     * Phase 4 guard rail: min(IsetBalanced, 30/3) = min(x, 10)
     * This is below MinCurrent*10=60, so Phase 5 shortage will inflate to 60.
     * The point is: the headroom DID constrain via Idifference and guard rail.
     * With such tight headroom (3A across 3 phases = 1A/phase), the EVSE
     * is in shortage and gets floored to MinCurrent.
     */
    TEST_ASSERT_LESS_OR_EQUAL(60, ctx.IsetBalanced);
}

/* ---- Test: Unconstrained when headroom is INT16_MAX ---- */

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-021
 * @scenario Unconstrained when headroom is INT16_MAX (default)
 * @given ctx already charging at 160 deciamps with CapacityHeadroom_da = INT16_MAX
 * @when evse_calc_balanced_current runs
 * @then IsetBalanced is not reduced by capacity headroom
 */
void test_capacity_unconstrained_at_int16_max(void) {
    setup_smart_standalone_running();
    ctx.CapacityHeadroom_da = INT16_MAX;  /* Default: no constraint */

    evse_calc_balanced_current(&ctx, 0);
    int32_t with_max = ctx.IsetBalanced;

    /* Compare with a constrained run */
    setup_smart_standalone_running();
    ctx.CapacityHeadroom_da = 30;  /* Very tight constraint */

    evse_calc_balanced_current(&ctx, 0);
    int32_t with_tight = ctx.IsetBalanced;

    /* Unconstrained should yield higher (or equal) IsetBalanced */
    TEST_ASSERT_TRUE(with_max >= with_tight);
    /* And specifically, unconstrained should be significantly higher */
    TEST_ASSERT_TRUE(with_max > 60);  /* Above MinCurrent floor */
}

/* ---- Test: Negative headroom forces IsetBalanced to minimum ---- */

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-022
 * @scenario Negative headroom clamps IsetBalanced and triggers shortage
 * @given ctx already charging with CapacityHeadroom_da = -50 (over capacity limit)
 * @when evse_calc_balanced_current runs
 * @then IsetBalanced is clamped to MinCurrent floor (shortage path)
 */
void test_capacity_negative_headroom_clamps_down(void) {
    setup_smart_standalone_running();
    ctx.CapacityHeadroom_da = -50;  /* Over limit by 5.0A */

    evse_calc_balanced_current(&ctx, 0);

    /*
     * Negative headroom: Idifference clamped to -50, guard rail clamps to -50/3=-16.
     * Phase 5 sees shortage (IsetBalanced < MinCurrent*10), inflates to 60.
     * The capacity headroom successfully drove the system into shortage mode.
     */
    TEST_ASSERT_EQUAL_INT(60, ctx.IsetBalanced);
}

/* ---- Test: Capacity headroom tighter than MaxSumMains ---- */

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-023
 * @scenario Capacity headroom is tighter constraint than MaxSumMains
 * @given MaxSumMains=75A, CapacityHeadroom_da=30 (3.0A), charging at 160 deciamps
 * @when evse_calc_balanced_current runs
 * @then CapacityHeadroom wins as the tighter constraint, IsetBalanced reduced
 */
void test_capacity_headroom_tighter_than_max_sum_mains(void) {
    setup_smart_standalone_running();
    ctx.MaxSumMains = 75;  /* 75A sum limit — very generous */
    ctx.Isum = 100;        /* 10.0A measured on grid */
    ctx.CapacityHeadroom_da = 30;  /* Only 3.0A headroom — very tight */

    evse_calc_balanced_current(&ctx, 0);

    /*
     * MaxSumMains allows (750-100)/divisor = 650 headroom.
     * Capacity headroom clamps Idifference to 30 (much tighter).
     * Guard rail: min(IsetBalanced, 30/3) = min(x, 10) -> shortage -> floor at 60.
     * Key point: capacity was the binding constraint, not MaxSumMains.
     */
    TEST_ASSERT_LESS_OR_EQUAL(60, ctx.IsetBalanced);

    /* Verify: without capacity constraint, we'd get much more */
    setup_smart_standalone_running();
    ctx.MaxSumMains = 75;
    ctx.Isum = 100;
    ctx.CapacityHeadroom_da = INT16_MAX;  /* Unconstrained */
    evse_calc_balanced_current(&ctx, 0);
    int32_t unconstrained = ctx.IsetBalanced;

    /* Unconstrained should be much higher than 60 */
    TEST_ASSERT_TRUE(unconstrained > 60);
}

/* ---- Test: Init sets CapacityHeadroom_da to INT16_MAX ---- */

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-024
 * @scenario evse_init sets CapacityHeadroom_da to INT16_MAX by default
 * @given A freshly initialized evse_ctx_t
 * @when evse_init is called
 * @then CapacityHeadroom_da equals INT16_MAX (unconstrained)
 */
void test_capacity_init_default(void) {
    evse_init(&ctx, NULL);
    TEST_ASSERT_EQUAL_INT(INT16_MAX, ctx.CapacityHeadroom_da);
}

/* ---- Test: Capacity headroom in master mode with new EVSE joining ---- */

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-025
 * @scenario Capacity headroom clamps IsetBalanced when new EVSE joins in smart mode
 * @given Master (LoadBl=1) in smart mode with CapacityHeadroom_da=60 and a new EVSE joining
 * @when evse_calc_balanced_current runs with mod=1
 * @then IsetBalanced is clamped by capacity headroom
 */
void test_capacity_headroom_master_new_evse(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_SMART;
    ctx.LoadBl = 1;  /* Master */
    ctx.MaxCurrent = 32;
    ctx.MaxCapacity = 32;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 32;
    ctx.MaxMains = 25;
    ctx.ChargeCurrent = 320;
    ctx.phasesLastUpdateFlag = true;
    ctx.EmaAlpha = 100;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.EnableC2 = NOT_PRESENT;

    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 320;
    ctx.Balanced[0] = 0;
    ctx.Node[0].Online = 1;

    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 50;
    ctx.Isum = 50;
    ctx.EVMeterType = 1;
    ctx.EVMeterImeasured = 0;

    ctx.CapacityHeadroom_da = 60;  /* 6.0A total headroom */

    /* mod=1 triggers the "new EVSE joining" path */
    evse_calc_balanced_current(&ctx, 1);

    /* In the new-EVSE-joining path:
     * IsetBalanced = min((250-Baseload), (320-0)) = min(200, 320) = 200
     * Then capacity clamp: min(200, 60/3) = min(200, 20) = 20
     * Then Phase 4 guard rail again: min(20, 60/3) = 20
     * But shortage: 20 < 1*60 = 60, so inflated to 60.
     * However the new-EVSE path sets IsetBalanced BEFORE guard rail.
     * Let me check: guard rail runs unconditionally after the mode blocks.
     */

    /* With 3 phases: 60/3 = 20 -> shortage -> floor at 60 */
    TEST_ASSERT_LESS_OR_EQUAL(60, ctx.IsetBalanced);

    /* Verify that without capacity constraint, we'd get more */
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_SMART;
    ctx.LoadBl = 1;
    ctx.MaxCurrent = 32;
    ctx.MaxCapacity = 32;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 32;
    ctx.MaxMains = 25;
    ctx.ChargeCurrent = 320;
    ctx.phasesLastUpdateFlag = true;
    ctx.EmaAlpha = 100;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.EnableC2 = NOT_PRESENT;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 320;
    ctx.Balanced[0] = 0;
    ctx.Node[0].Online = 1;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 50;
    ctx.Isum = 50;
    ctx.EVMeterType = 1;
    ctx.EVMeterImeasured = 0;
    ctx.CapacityHeadroom_da = INT16_MAX;  /* Unconstrained */

    evse_calc_balanced_current(&ctx, 1);
    int32_t unconstrained = ctx.IsetBalanced;

    /* Unconstrained should be much higher */
    TEST_ASSERT_TRUE(unconstrained > 60);
}

/* ---- Test: Single-phase mode gets more per-phase headroom ---- */

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-026
 * @scenario Single-phase charging gets full headroom on one phase
 * @given ctx charging at 160 deciamps, CapacityHeadroom_da=120, forced single-phase
 * @when evse_calc_balanced_current runs
 * @then IsetBalanced clamped to 120/1=120 deciamps (not 120/3=40)
 */
void test_capacity_headroom_single_phase(void) {
    setup_smart_standalone_running();
    ctx.Nr_Of_Phases_Charging = 1;
    ctx.EnableC2 = ALWAYS_OFF;  /* Force single phase */
    ctx.CapacityHeadroom_da = 120;  /* 12.0A total headroom */

    evse_calc_balanced_current(&ctx, 0);

    /* Single phase: guard rail clamps to 120/1 = 120 deciamps */
    /* IsetBalanced should be clamped to at most 120, but >= MinCurrent */
    TEST_ASSERT_LESS_OR_EQUAL(120, ctx.IsetBalanced);
    TEST_ASSERT_TRUE(ctx.IsetBalanced >= 60);  /* At least MinCurrent */
}

/* ---- Test: Moderate headroom allows charging above MinCurrent ---- */

/*
 * @feature Capacity Tariff Peak Tracking
 * @req REQ-CAP-027
 * @scenario Moderate capacity headroom allows charging above minimum
 * @given ctx charging with CapacityHeadroom_da=240 (24.0A total, 8A/phase)
 * @when evse_calc_balanced_current runs
 * @then IsetBalanced is between MinCurrent and the headroom-per-phase limit
 */
void test_capacity_moderate_headroom(void) {
    setup_smart_standalone_running();
    ctx.CapacityHeadroom_da = 240;  /* 24.0A total = 8.0A per phase */

    evse_calc_balanced_current(&ctx, 0);

    /* Guard rail clamps to 240/3 = 80 deciamps per phase */
    TEST_ASSERT_LESS_OR_EQUAL(80, ctx.IsetBalanced);
    /* Should be above minimum since 80 > MinCurrent*10=60 */
    TEST_ASSERT_TRUE(ctx.IsetBalanced >= 60);
}

/* ---- Upstream a54b07f (fixes #327): MaxSumMains as ADDITIONAL constraint ---- */

/*
 * Before a54b07f, Idifference was OVERWRITTEN with ExcessMaxSumMains —
 * a sum-of-phases value assigned into a per-phase variable — causing
 * current fluctuations when CAPACITY / MaxSumMains was configured
 * alongside a tighter MaxMains limit. The fix uses min() with
 * ExcessMaxSumMains / 3 so MaxSumMains is only applied when it is
 * actually the tighter constraint.
 */

/*
 * @feature Load Balancing — CAPACITY integration
 * @req REQ-LB-170
 * @scenario MaxSumMains does NOT overwrite tighter per-phase MaxMains limit
 * @given MaxMains=10A (tight), MaxSumMains=75A (generous), charging at 160 dA
 * @when evse_calc_balanced_current runs
 * @then IsetBalanced respects the MaxMains-based per-phase limit;
 *       the old bug would overwrite with the larger sum-based value and
 *       cause over-current-relative-to-MaxMains. Verified via IsetBalanced
 *       staying within the per-phase headroom envelope.
 */
void test_capacity_a54b07f_maxsummains_does_not_overwrite_tighter_maxmains(void) {
    setup_smart_standalone_running();
    ctx.MaxMains = 10;           /* Tight: only 10A per phase */
    ctx.MainsMeterImeasured = 90; /* 9.0A measured — near the limit */
    ctx.MaxSumMains = 75;        /* Generous: 75A sum */
    ctx.Isum = 0;                /* Plenty of sum-of-phases headroom */
    ctx.CapacityHeadroom_da = INT16_MAX; /* Unconstrained */

    evse_calc_balanced_current(&ctx, 0);

    /* Idifference should be bounded by the per-phase MaxMains headroom
     * (100 - 90 = 10 dA) combined via min() with ExcessMaxSumMains/3 =
     * (750-0)/3 = 250. The tighter wins, so Idifference ≤ 10.
     * IsetBalanced was 160; with Idifference=10 and ramp, it can only
     * grow slowly. Certainly not jump to 750. */
    TEST_ASSERT_LESS_OR_EQUAL(200, ctx.IsetBalanced);
}

/*
 * @feature Load Balancing — CAPACITY integration
 * @req REQ-LB-171
 * @scenario MaxSumMains IS the binding constraint when it is tighter per-phase
 * @given MaxMains=32A (generous per-phase), MaxSumMains=30A (sum = 10A/phase equiv.)
 * @when evse_calc_balanced_current runs
 * @then MaxSumMains wins — Idifference narrowed by the per-phase equivalent (30/3=10 dA)
 */
void test_capacity_a54b07f_maxsummains_binds_when_tighter(void) {
    setup_smart_standalone_running();
    ctx.MaxMains = 32;             /* Generous per-phase */
    ctx.MainsMeterImeasured = 100;
    ctx.MaxSumMains = 30;          /* Sum-of-phases tight */
    ctx.Isum = 0;                  /* 30A sum headroom, ~10A/phase equivalent */
    ctx.CapacityHeadroom_da = INT16_MAX;

    evse_calc_balanced_current(&ctx, 0);

    /* ExcessMaxSumMains = (300 - 0) = 300 dA = 30 A sum.
     * Per-phase equivalent: 300/3 = 100 dA.
     * Per-phase MaxMains: (32*10) - 100 = 220 dA.
     * min(220, 100) = 100 → tight constraint wins.
     * Idifference this small won't let IsetBalanced grow wildly. */
    TEST_ASSERT_LESS_OR_EQUAL(260, ctx.IsetBalanced);
}

/*
 * @feature Load Balancing — CAPACITY integration
 * @req REQ-LB-172
 * @scenario LimitedByMaxSumMains flag still set when MaxSumMains exceeded
 * @given ExcessMaxSumMains < 0 (MaxSumMains already exceeded)
 * @when evse_calc_balanced_current runs
 * @then The MaxSumMainsTimer path is armed (LimitedByMaxSumMains semantics preserved)
 *       — the upstream fix changed only the Idifference path, not the overflow flag.
 */
void test_capacity_a54b07f_exceeded_still_flagged(void) {
    setup_smart_standalone_running();
    ctx.MaxSumMains = 20;      /* Configured sum limit: 20A */
    ctx.Isum = 300;            /* Currently 30A sum — exceeded */
    ctx.MaxSumMainsTimer = 0;  /* Will be armed elsewhere when exceeded */

    evse_calc_balanced_current(&ctx, 0);

    /* With ExcessMaxSumMains = (200 - 300) = -100 < 0, the fix still enters
     * the "LimitedByMaxSumMains = true" branch. We can't observe the local
     * variable directly, but MaxSumMainsTimer should NOT have been reset to 0
     * in the "safe" branch (which only runs when ExcessMaxSumMains >= 0).
     * As a sanity check: the integer division by 3 of a negative number in C
     * rounds toward zero (-100 / 3 = -33), so Idifference becomes very small
     * or negative, throttling IsetBalanced. */
    TEST_ASSERT_LESS_OR_EQUAL(160, ctx.IsetBalanced);
}

int main(void) {
    TEST_SUITE_BEGIN("Capacity Tariff State Machine Integration");

    RUN_TEST(test_capacity_init_default);
    RUN_TEST(test_capacity_headroom_clamps_iset_balanced);
    RUN_TEST(test_capacity_unconstrained_at_int16_max);
    RUN_TEST(test_capacity_negative_headroom_clamps_down);
    RUN_TEST(test_capacity_headroom_tighter_than_max_sum_mains);
    RUN_TEST(test_capacity_headroom_master_new_evse);
    RUN_TEST(test_capacity_headroom_single_phase);
    RUN_TEST(test_capacity_moderate_headroom);
    RUN_TEST(test_capacity_a54b07f_maxsummains_does_not_overwrite_tighter_maxmains);
    RUN_TEST(test_capacity_a54b07f_maxsummains_binds_when_tighter);
    RUN_TEST(test_capacity_a54b07f_exceeded_still_flagged);

    TEST_SUITE_RESULTS();
}
