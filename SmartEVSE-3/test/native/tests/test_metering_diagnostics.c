/*
 * test_metering_diagnostics.c - Metering diagnostic counter tests
 *
 * Tests:
 *   - meter_timeout_count increments on CT_NOCOMM
 *   - meter_recovery_count increments on CT_NOCOMM recovery
 *   - api_stale_count increments on API staleness detection
 *   - Counters are cumulative (multiple events add up)
 *   - Counters are zero after init
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

/* ---- Counters zero after init ---- */

/*
 * @feature Metering Diagnostics
 * @req REQ-MTR-030
 * @scenario All diagnostic counters are zero after initialization
 * @given A freshly initialized EVSE context
 * @when evse_init completes
 * @then meter_timeout_count, meter_recovery_count, and api_stale_count are all 0
 */
void test_counters_zero_after_init(void) {
    evse_init(&ctx, NULL);
    TEST_ASSERT_EQUAL_INT(0, ctx.meter_timeout_count);
    TEST_ASSERT_EQUAL_INT(0, ctx.meter_recovery_count);
    TEST_ASSERT_EQUAL_INT(0, ctx.api_stale_count);
}

/* ---- meter_timeout_count increments on CT_NOCOMM ---- */

/*
 * @feature Metering Diagnostics
 * @req REQ-MTR-031
 * @scenario meter_timeout_count increments when CT_NOCOMM is set
 * @given EVSE in Smart mode with MainsMeterType=1 and MainsMeterTimeout=0
 * @when A 1-second tick triggers CT_NOCOMM
 * @then meter_timeout_count increments by 1
 */
void test_timeout_count_increments_on_ct_nocomm(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.LoadBl = 0;
    ctx.MainsMeterTimeout = 0;

    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(1, ctx.meter_timeout_count);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & CT_NOCOMM) != 0);
}

/* ---- meter_timeout_count increments on node CT_NOCOMM ---- */

/*
 * @feature Metering Diagnostics
 * @req REQ-MTR-032
 * @scenario meter_timeout_count increments for node (LoadBl > 1)
 * @given EVSE as node with LoadBl=2 and MainsMeterTimeout=0
 * @when A 1-second tick triggers CT_NOCOMM
 * @then meter_timeout_count increments by 1
 */
void test_timeout_count_increments_on_node_ct_nocomm(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.LoadBl = 2;
    ctx.MainsMeterTimeout = 0;

    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(1, ctx.meter_timeout_count);
}

/* ---- meter_recovery_count increments on CT_NOCOMM recovery ---- */

/*
 * @feature Metering Diagnostics
 * @req REQ-MTR-033
 * @scenario meter_recovery_count increments when CT_NOCOMM clears
 * @given EVSE with CT_NOCOMM set and MainsMeterTimeout restored to >0
 * @when A 1-second tick clears CT_NOCOMM
 * @then meter_recovery_count increments by 1
 */
void test_recovery_count_increments(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.LoadBl = 0;

    /* Trigger CT_NOCOMM */
    ctx.MainsMeterTimeout = 0;
    evse_tick_1s(&ctx);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & CT_NOCOMM) != 0);
    TEST_ASSERT_EQUAL_INT(0, ctx.meter_recovery_count);

    /* Simulate data restored */
    ctx.MainsMeterTimeout = 5;
    evse_tick_1s(&ctx);
    TEST_ASSERT_FALSE(ctx.ErrorFlags & CT_NOCOMM);
    TEST_ASSERT_EQUAL_INT(1, ctx.meter_recovery_count);
}

/* ---- api_stale_count increments on staleness ---- */

/*
 * @feature Metering Diagnostics
 * @req REQ-MTR-034
 * @scenario api_stale_count increments when API data goes stale
 * @given EVSE in API mode with staleness timer about to expire
 * @when Timer reaches 0
 * @then api_stale_count increments by 1
 */
void test_api_stale_count_increments(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = EM_API_METER;
    ctx.LoadBl = 0;
    ctx.MaxMains = 25;
    ctx.api_mains_timeout = 1;
    ctx.api_mains_staleness_timer = 1;
    ctx.api_mains_stale = false;
    ctx.MainsMeterTimeout = COMM_TIMEOUT;

    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(1, ctx.api_stale_count);
    TEST_ASSERT_TRUE(ctx.api_mains_stale);
}

/* ---- Counters are cumulative ---- */

/*
 * @feature Metering Diagnostics
 * @req REQ-MTR-035
 * @scenario Counters accumulate across multiple events
 * @given EVSE that has already had one timeout and recovery
 * @when Another timeout and recovery cycle occurs
 * @then Counters show 2 timeouts and 2 recoveries
 */
void test_counters_are_cumulative(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.LoadBl = 0;

    /* First timeout */
    ctx.MainsMeterTimeout = 0;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(1, ctx.meter_timeout_count);

    /* First recovery */
    ctx.MainsMeterTimeout = 5;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(1, ctx.meter_recovery_count);

    /* Second timeout */
    ctx.MainsMeterTimeout = 0;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(2, ctx.meter_timeout_count);

    /* Second recovery */
    ctx.MainsMeterTimeout = 5;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(2, ctx.meter_recovery_count);
}

/* ---- CT_NOCOMM suppressed for API mode does not increment timeout counter ---- */

/*
 * @feature Metering Diagnostics
 * @req REQ-MTR-036
 * @scenario meter_timeout_count does NOT increment when CT_NOCOMM is suppressed for API mode
 * @given EVSE in API mode with staleness enabled and MainsMeterTimeout=0
 * @when A 1-second tick occurs (CT_NOCOMM suppressed)
 * @then meter_timeout_count remains 0
 */
void test_timeout_count_not_incremented_when_suppressed(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = EM_API_METER;
    ctx.LoadBl = 0;
    ctx.api_mains_timeout = 120;
    ctx.api_mains_staleness_timer = 60;
    ctx.api_mains_stale = false;
    ctx.MainsMeterTimeout = 0;

    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(0, ctx.meter_timeout_count);
    TEST_ASSERT_FALSE((ctx.ErrorFlags & CT_NOCOMM) != 0);
}

/* ---- Test runner ---- */

int main(void) {
    TEST_SUITE_BEGIN("Metering Diagnostics");

    RUN_TEST(test_counters_zero_after_init);
    RUN_TEST(test_timeout_count_increments_on_ct_nocomm);
    RUN_TEST(test_timeout_count_increments_on_node_ct_nocomm);
    RUN_TEST(test_recovery_count_increments);
    RUN_TEST(test_api_stale_count_increments);
    RUN_TEST(test_counters_are_cumulative);
    RUN_TEST(test_timeout_count_not_incremented_when_suppressed);

    TEST_SUITE_RESULTS();
}
