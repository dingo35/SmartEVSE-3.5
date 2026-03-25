/*
 * test_circuit_meter.c - CircuitMeter subpanel protection tests
 *
 * Tests circuit current limiting via MaxCircuitMains and
 * CircuitMeterImeasured in evse_calc_balanced_current().
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

/* Helper: set up a smart-mode master with circuit meter enabled.
 * Uses mod=1 (new EVSE joining) to test the initial calculation path
 * where IsetBalanced is set directly from headroom values. */
static void setup_smart_master_circuit(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_SMART;
    ctx.LoadBl = 1;  /* Master */
    ctx.MaxCurrent = 16;
    ctx.MaxCapacity = 16;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 32;
    ctx.MaxMains = 25;
    ctx.ChargeCurrent = 160;
    ctx.MainsMeterType = 1;
    ctx.EVMeterType = 1;
    ctx.EmaAlpha = 100;  /* No EMA smoothing */
    ctx.phasesLastUpdateFlag = true;

    /* Single EVSE charging */
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 160;
    ctx.Balanced[0] = 100;
    ctx.Node[0].Online = 1;

    /* Mains/EV readings with plenty of headroom */
    ctx.MainsMeterImeasured = 100;  /* 10A */
    ctx.EVMeterImeasured = 100;     /* 10A */
    ctx.Isum = 100;
}

/*
 * @feature CircuitMeter Subpanel Protection
 * @req REQ-CIR-001
 * @scenario Circuit current limiting clamps IsetBalanced on new EVSE join
 * @given MaxCircuitMains=25A, CircuitMeterImeasured=200dA (20A), new EVSE joining (mod=1)
 * @when evse_calc_balanced_current() runs with mod=1 in smart mode
 * @then IsetBalanced is clamped by circuit headroom below the unconstrained value
 */
void test_circuit_limiting_active(void) {
    setup_smart_master_circuit();
    ctx.MaxMains = 40;                 /* 40A — lots of mains headroom */
    ctx.MaxCircuit = 40;
    ctx.MainsMeterImeasured = 50;      /* 5A on mains — plenty of headroom */
    ctx.EVMeterImeasured = 50;
    ctx.Isum = 50;
    ctx.MaxCircuitMains = 25;          /* 25A circuit breaker */
    ctx.CircuitMeterImeasured = 200;   /* 20A measured on circuit */

    /* Run without circuit meter to get baseline (mod=1 for new join) */
    static evse_ctx_t ctx_baseline;
    memcpy(&ctx_baseline, &ctx, sizeof(evse_ctx_t));
    ctx_baseline.MaxCircuitMains = 0;
    evse_calc_balanced_current(&ctx_baseline, 1);
    int32_t baseline = ctx_baseline.IsetBalanced;

    /* Run with circuit meter */
    evse_calc_balanced_current(&ctx, 1);

    /* Circuit headroom = 250 - 200 = 50dA, /3 phases = 16dA.
     * This should clamp IsetBalanced well below the unconstrained baseline.
     * Baseline: min(400-50, 400-50) = 350dA.
     * With circuit: min(350, 50/3=16) = 16dA, which is below MinCurrent,
     * so shortage inflates to 60dA. */
    TEST_ASSERT_TRUE(ctx.IsetBalanced < baseline);
    TEST_ASSERT_EQUAL_INT(60, ctx.IsetBalanced);
}

/*
 * @feature CircuitMeter Subpanel Protection
 * @req REQ-CIR-002
 * @scenario Circuit meter disabled has no effect on IsetBalanced
 * @given MaxCircuitMains=0 (disabled), new EVSE joining (mod=1)
 * @when evse_calc_balanced_current() runs in smart mode
 * @then IsetBalanced is determined only by MaxMains and MaxCircuit limits
 */
void test_circuit_meter_disabled(void) {
    setup_smart_master_circuit();
    ctx.MaxCircuitMains = 0;           /* Disabled */
    ctx.CircuitMeterImeasured = 200;   /* Should be ignored */

    /* Use mod=1 so IsetBalanced is set directly from headroom */
    evse_calc_balanced_current(&ctx, 1);

    /* Without circuit meter, MaxMains headroom = 250 - 100 = 150dA
     * MaxCircuit headroom = 320 - 100 = 220dA
     * min(150, 220) = 150dA, well above MinCurrent */
    TEST_ASSERT_GREATER_THAN(60, ctx.IsetBalanced);
}

/*
 * @feature CircuitMeter Subpanel Protection
 * @req REQ-CIR-003
 * @scenario Circuit overload triggers hard shortage and increments NoCurrent
 * @given MaxCircuitMains=25A and CircuitMeterImeasured=260dA (26A, over limit)
 * @when evse_calc_balanced_current() runs with mod=1
 * @then NoCurrent is incremented due to hard shortage from circuit overload
 */
void test_circuit_overload(void) {
    setup_smart_master_circuit();
    ctx.MaxCircuitMains = 25;          /* 25A circuit breaker */
    ctx.CircuitMeterImeasured = 260;   /* 26A — over limit */
    ctx.NoCurrent = 0;

    evse_calc_balanced_current(&ctx, 1);

    /* Circuit headroom = 250 - 260 = -10dA (overload).
     * Guard rail clamps IsetBalanced, shortage inflates to MinCurrent,
     * hard shortage detection sees circuit overload and increments NoCurrent. */
    TEST_ASSERT_GREATER_THAN(0, ctx.NoCurrent);
}

/*
 * @feature CircuitMeter Subpanel Protection
 * @req REQ-CIR-004
 * @scenario Circuit headroom tighter than MaxMains — circuit wins
 * @given MaxMains=40A (plenty of headroom) but MaxCircuitMains=12A with 60dA measured
 * @when evse_calc_balanced_current() runs with mod=1
 * @then IsetBalanced is limited by circuit headroom, not MaxMains
 */
void test_circuit_tighter_than_maxmains(void) {
    setup_smart_master_circuit();
    ctx.MaxMains = 40;                 /* 40A — lots of mains headroom */
    ctx.MaxCircuit = 40;
    ctx.MainsMeterImeasured = 50;      /* 5A on mains */
    ctx.EVMeterImeasured = 50;
    ctx.Isum = 50;
    ctx.MaxCircuitMains = 12;          /* 12A circuit breaker */
    ctx.CircuitMeterImeasured = 60;    /* 6A on circuit */

    /* Run without circuit meter to get baseline */
    static evse_ctx_t ctx_no_circuit;
    memcpy(&ctx_no_circuit, &ctx, sizeof(evse_ctx_t));
    ctx_no_circuit.MaxCircuitMains = 0;
    evse_calc_balanced_current(&ctx_no_circuit, 1);
    int32_t baseline = ctx_no_circuit.IsetBalanced;

    /* Now run with circuit meter */
    evse_calc_balanced_current(&ctx, 1);

    /* Circuit headroom = 120 - 60 = 60dA, /3 phases = 20dA.
     * Baseline (MaxMains): min(400-50, 400-50) = 350dA.
     * Circuit-limited should be well below. */
    TEST_ASSERT_TRUE(ctx.IsetBalanced < baseline);
    /* Circuit limit forces IsetBalanced down; 60/3 = 20dA < MinCurrent,
     * so shortage inflates to 60. Still much less than baseline. */
    TEST_ASSERT_LESS_OR_EQUAL(60, ctx.IsetBalanced);
}

/*
 * @feature CircuitMeter Subpanel Protection
 * @req REQ-CIR-005
 * @scenario Default initialization sets circuit meter fields to zero
 * @given A freshly initialized evse_ctx_t
 * @when evse_init() is called
 * @then MaxCircuitMains and CircuitMeterImeasured are both 0
 */
void test_circuit_meter_init_defaults(void) {
    evse_init(&ctx, NULL);

    TEST_ASSERT_EQUAL_INT(0, ctx.MaxCircuitMains);
    TEST_ASSERT_EQUAL_INT(0, ctx.CircuitMeterImeasured);
}

int main(void) {
    TEST_SUITE_BEGIN("CircuitMeter Subpanel Protection");

    RUN_TEST(test_circuit_limiting_active);
    RUN_TEST(test_circuit_meter_disabled);
    RUN_TEST(test_circuit_overload);
    RUN_TEST(test_circuit_tighter_than_maxmains);
    RUN_TEST(test_circuit_meter_init_defaults);

    TEST_SUITE_RESULTS();
}
