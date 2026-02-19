/*
 * test_ocpp.c - OCPP edge cases for current limiting and mode behavior
 *
 * Tests:
 *   - OcppCurrentLimit at exact boundary values (equal to MinCurrent, MaxCurrent)
 *   - OcppMode with LoadBl != 0 (should be ignored per code)
 *   - OcppCurrentLimit interaction with OverrideCurrent
 *   - CPDutyOverride flag behavior
 *   - OcppCurrentLimit = 0.0 (should zero the current)
 *   - Negative OcppCurrentLimit (should be ignored, treated as no limit)
 *   - OCPP interaction with evse_is_current_available
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

/* Helper: set up a standalone EVSE in STATE_C with OCPP enabled */
static void setup_ocpp_charging(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;          /* Standalone */
    ctx.MaxCurrent = 16;
    ctx.MaxCapacity = 16;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 32;
    ctx.MaxMains = 25;
    ctx.ChargeCurrent = 160;
    ctx.OcppMode = true;
    ctx.State = STATE_C;
    ctx.BalancedState[0] = STATE_C;
    ctx.BalancedMax[0] = 160;
    ctx.Balanced[0] = 160;
    ctx.phasesLastUpdateFlag = true;
}

/* ---- Boundary: OcppCurrentLimit == MinCurrent ---- */

/*
 * @feature OCPP Current Limiting
 * @req REQ-OCPP-001
 * @scenario OCPP current limit exactly at MinCurrent boundary is accepted
 * @given EVSE is standalone in STATE_C with OcppMode enabled and MinCurrent=6A
 * @when OcppCurrentLimit is set to 6.0A (exactly MinCurrent) and balanced current is calculated
 * @then ChargeCurrent is set to 60 deciamps (6A) because the limit equals MinCurrent
 */
void test_ocpp_limit_equal_to_mincurrent(void) {
    setup_ocpp_charging();
    ctx.OcppCurrentLimit = 6.0f;  /* Exactly MinCurrent */
    evse_calc_balanced_current(&ctx, 0);
    /* 6A is valid (>= MinCurrent), so ChargeCurrent should be 60 */
    TEST_ASSERT_EQUAL_INT(60, ctx.ChargeCurrent);
}

/* ---- Boundary: OcppCurrentLimit == MaxCurrent ---- */

/*
 * @feature OCPP Current Limiting
 * @req REQ-OCPP-002
 * @scenario OCPP current limit exactly at MaxCurrent does not reduce current
 * @given EVSE is standalone in STATE_C with OcppMode enabled and MaxCurrent=16A
 * @when OcppCurrentLimit is set to 16.0A (exactly MaxCurrent) and balanced current is calculated
 * @then ChargeCurrent remains at 160 deciamps (16A) because OCPP limit does not cap below MaxCurrent
 */
void test_ocpp_limit_equal_to_maxcurrent(void) {
    setup_ocpp_charging();
    ctx.OcppCurrentLimit = 16.0f;  /* Exactly MaxCurrent */
    evse_calc_balanced_current(&ctx, 0);
    /* 16A == MaxCurrent, so no reduction: ChargeCurrent stays 160 */
    TEST_ASSERT_EQUAL_INT(160, ctx.ChargeCurrent);
}

/* ---- OcppCurrentLimit above MaxCurrent does not increase current ---- */

/*
 * @feature OCPP Current Limiting
 * @req REQ-OCPP-003
 * @scenario OCPP current limit above MaxCurrent does not increase current beyond MaxCurrent
 * @given EVSE is standalone in STATE_C with OcppMode enabled and MaxCurrent=16A
 * @when OcppCurrentLimit is set to 32.0A (well above MaxCurrent) and balanced current is calculated
 * @then ChargeCurrent stays at 160 deciamps (MaxCurrent) because OCPP cannot raise current above hardware limit
 */
void test_ocpp_limit_above_maxcurrent_no_increase(void) {
    setup_ocpp_charging();
    ctx.OcppCurrentLimit = 32.0f;  /* Well above MaxCurrent of 16 */
    evse_calc_balanced_current(&ctx, 0);
    /* OCPP limit is higher than ChargeCurrent, so no capping: stays at MaxCurrent*10 */
    TEST_ASSERT_EQUAL_INT(160, ctx.ChargeCurrent);
}

/* ---- OcppMode with LoadBl != 0 (master or node): OCPP limit ignored ---- */

/*
 * @feature OCPP Current Limiting
 * @req REQ-OCPP-004
 * @scenario OCPP current limit is ignored when LoadBl is set to master
 * @given EVSE is a master (LoadBl=1) in STATE_C with OcppMode enabled
 * @when OcppCurrentLimit is set to 3.0A (below MinCurrent) and balanced current is calculated
 * @then ChargeCurrent remains at 160 deciamps because OCPP limit requires standalone mode (LoadBl=0)
 */
void test_ocpp_ignored_when_loadbl_master(void) {
    setup_ocpp_charging();
    ctx.LoadBl = 1;  /* Master in load-balanced setup */
    ctx.OcppCurrentLimit = 3.0f;  /* Below MinCurrent */
    ctx.MaxCircuit = 32;
    evse_calc_balanced_current(&ctx, 0);
    /* With LoadBl=1, OCPP limit check (!ctx->LoadBl) is false, so limit is ignored.
     * ChargeCurrent should be set from MaxCurrent, not zeroed. */
    TEST_ASSERT_EQUAL_INT(160, ctx.ChargeCurrent);
}

/*
 * @feature OCPP Current Limiting
 * @req REQ-OCPP-005
 * @scenario OCPP current limit is ignored when LoadBl is set to node
 * @given EVSE is a node (LoadBl=2) in STATE_C with OcppMode enabled
 * @when OcppCurrentLimit is set to 3.0A (below MinCurrent) and balanced current is calculated
 * @then ChargeCurrent remains at 160 deciamps because OCPP limit requires standalone mode (LoadBl=0)
 */
void test_ocpp_ignored_when_loadbl_node(void) {
    setup_ocpp_charging();
    ctx.LoadBl = 2;  /* Node */
    ctx.OcppCurrentLimit = 3.0f;  /* Below MinCurrent */
    evse_calc_balanced_current(&ctx, 0);
    /* OCPP check requires !ctx->LoadBl, so with LoadBl=2 the limit is ignored */
    TEST_ASSERT_EQUAL_INT(160, ctx.ChargeCurrent);
}

/* ---- OverrideCurrent takes precedence over OCPP limit ---- */

/*
 * @feature OCPP Current Limiting
 * @req REQ-OCPP-006
 * @scenario OverrideCurrent takes precedence over OCPP limit
 * @given EVSE is standalone in STATE_C with OcppCurrentLimit=10.0A and OverrideCurrent=80 deciamps
 * @when Balanced current is calculated
 * @then ChargeCurrent is set to 80 deciamps because OverrideCurrent is applied after OCPP capping
 */
void test_override_current_overrides_ocpp(void) {
    setup_ocpp_charging();
    ctx.OcppCurrentLimit = 10.0f;  /* Would cap at 100 */
    ctx.OverrideCurrent = 80;      /* Override to 8A */
    evse_calc_balanced_current(&ctx, 0);
    /* OverrideCurrent is applied AFTER OCPP, and unconditionally sets ChargeCurrent */
    TEST_ASSERT_EQUAL_INT(80, ctx.ChargeCurrent);
}

/*
 * @feature OCPP Current Limiting
 * @req REQ-OCPP-007
 * @scenario OverrideCurrent restores charging even when OCPP would zero the current
 * @given EVSE is standalone in STATE_C with OcppCurrentLimit=3.0A (below MinCurrent) and OverrideCurrent=120
 * @when Balanced current is calculated
 * @then ChargeCurrent is set to 120 deciamps because OverrideCurrent overrides the OCPP-zeroed value
 */
void test_override_current_overrides_ocpp_zero(void) {
    setup_ocpp_charging();
    ctx.OcppCurrentLimit = 3.0f;   /* Below MinCurrent, would zero ChargeCurrent */
    ctx.OverrideCurrent = 120;     /* Override to 12A */
    evse_calc_balanced_current(&ctx, 0);
    /* OverrideCurrent is applied after OCPP zeroed ChargeCurrent, overriding it */
    TEST_ASSERT_EQUAL_INT(120, ctx.ChargeCurrent);
}

/* ---- OcppCurrentLimit = 0.0 should zero the current ---- */

/*
 * @feature OCPP Current Limiting
 * @req REQ-OCPP-008
 * @scenario OCPP current limit of 0.0A zeros the charge current
 * @given EVSE is standalone in STATE_C with OcppMode enabled
 * @when OcppCurrentLimit is set to 0.0A and balanced current is calculated
 * @then ChargeCurrent is zeroed because 0.0A is below MinCurrent (6A)
 */
void test_ocpp_limit_zero_zeros_current(void) {
    setup_ocpp_charging();
    ctx.OcppCurrentLimit = 0.0f;
    evse_calc_balanced_current(&ctx, 0);
    /* 0.0 < MinCurrent(6), so ChargeCurrent should be zeroed */
    TEST_ASSERT_EQUAL_INT(0, ctx.ChargeCurrent);
}

/* ---- Negative OcppCurrentLimit: treated as "no limit set" ---- */

/*
 * @feature OCPP Current Limiting
 * @req REQ-OCPP-009
 * @scenario Negative OCPP current limit is treated as no limit set
 * @given EVSE is standalone in STATE_C with OcppMode enabled
 * @when OcppCurrentLimit is set to -1.0A (default init value meaning no limit)
 * @then ChargeCurrent remains at 160 deciamps because the OCPP capping block is skipped
 */
void test_ocpp_negative_limit_no_restriction(void) {
    setup_ocpp_charging();
    ctx.OcppCurrentLimit = -1.0f;  /* Default init value, means no limit */
    evse_calc_balanced_current(&ctx, 0);
    /* Negative limit means OCPP block is skipped (>= 0.0 check fails) */
    TEST_ASSERT_EQUAL_INT(160, ctx.ChargeCurrent);
}

/* ---- OCPP blocks current availability when limit < MinCurrent ---- */

/*
 * @feature OCPP Current Limiting
 * @req REQ-OCPP-010
 * @scenario OCPP limit of 0.0A blocks current availability check
 * @given EVSE is standalone with OcppMode enabled and OcppCurrentLimit=0.0A
 * @when evse_is_current_available is called
 * @then Returns 0 (unavailable) because OCPP limit is below MinCurrent
 */
void test_ocpp_blocks_current_available_at_zero(void) {
    setup_ocpp_charging();
    ctx.OcppCurrentLimit = 0.0f;
    int available = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(0, available);
}

/* ---- OCPP allows current availability when limit >= MinCurrent ---- */

/*
 * @feature OCPP Current Limiting
 * @req REQ-OCPP-011
 * @scenario OCPP limit at MinCurrent allows current availability
 * @given EVSE is standalone with OcppMode enabled and OcppCurrentLimit=6.0A (MinCurrent)
 * @when evse_is_current_available is called
 * @then Returns 1 (available) because 6.0A is not less than MinCurrent
 */
void test_ocpp_allows_current_available_at_mincurrent(void) {
    setup_ocpp_charging();
    ctx.OcppCurrentLimit = 6.0f;  /* Exactly MinCurrent */
    int available = evse_is_current_available(&ctx);
    /* 6.0 is NOT < MinCurrent(6), so the OCPP check passes */
    TEST_ASSERT_EQUAL_INT(1, available);
}

/* ---- OCPP current availability: negative limit is not blocking ---- */

/*
 * @feature OCPP Current Limiting
 * @req REQ-OCPP-012
 * @scenario Negative OCPP limit does not block current availability
 * @given EVSE is standalone with OcppMode enabled and OcppCurrentLimit=-1.0A (no limit)
 * @when evse_is_current_available is called
 * @then Returns 1 (available) because negative limit skips the OCPP availability check
 */
void test_ocpp_negative_limit_allows_current_available(void) {
    setup_ocpp_charging();
    ctx.OcppCurrentLimit = -1.0f;
    int available = evse_is_current_available(&ctx);
    /* Negative: OcppCurrentLimit >= 0.0 is false, so OCPP check is skipped */
    TEST_ASSERT_EQUAL_INT(1, available);
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("OCPP Edge Cases");

    RUN_TEST(test_ocpp_limit_equal_to_mincurrent);
    RUN_TEST(test_ocpp_limit_equal_to_maxcurrent);
    RUN_TEST(test_ocpp_limit_above_maxcurrent_no_increase);
    RUN_TEST(test_ocpp_ignored_when_loadbl_master);
    RUN_TEST(test_ocpp_ignored_when_loadbl_node);
    RUN_TEST(test_override_current_overrides_ocpp);
    RUN_TEST(test_override_current_overrides_ocpp_zero);
    RUN_TEST(test_ocpp_limit_zero_zeros_current);
    RUN_TEST(test_ocpp_negative_limit_no_restriction);
    RUN_TEST(test_ocpp_blocks_current_available_at_zero);
    RUN_TEST(test_ocpp_allows_current_available_at_mincurrent);
    RUN_TEST(test_ocpp_negative_limit_allows_current_available);

    TEST_SUITE_RESULTS();
}
