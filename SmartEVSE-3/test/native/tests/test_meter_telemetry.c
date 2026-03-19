/*
 * test_meter_telemetry.c - Meter telemetry counter tests
 *
 * Tests:
 *   - Initialization zeros all counters
 *   - Configure sets meter type and address
 *   - Increment each counter type independently
 *   - Counters saturate at UINT32_MAX
 *   - Reset slot preserves type/address
 *   - Reset all preserves type/address for every slot
 *   - Out-of-range slot is handled safely
 *   - Error rate calculation
 *   - NULL pointer safety
 */

#include "test_framework.h"
#include "meter_telemetry.h"

static meter_telemetry_t tel;

/* ---- Initialization ---- */

/*
 * @feature Meter Telemetry
 * @req REQ-MTR-001
 * @scenario Initialization zeros all counters and metadata
 * @given An uninitialized meter_telemetry_t struct
 * @when meter_telemetry_init is called
 * @then All counters, meter types, and addresses are zero
 */
void test_init_zeros_all(void) {
    /* Fill with garbage first */
    memset(&tel, 0xFF, sizeof(tel));
    meter_telemetry_init(&tel);

    for (int i = 0; i < METER_TELEMETRY_MAX_METERS; i++) {
        const meter_counters_t *m = meter_telemetry_get(&tel, (uint8_t)i);
        TEST_ASSERT_TRUE(m != NULL);
        TEST_ASSERT_EQUAL_INT(0, m->request_count);
        TEST_ASSERT_EQUAL_INT(0, m->response_count);
        TEST_ASSERT_EQUAL_INT(0, m->crc_error_count);
        TEST_ASSERT_EQUAL_INT(0, m->timeout_count);
        TEST_ASSERT_EQUAL_INT(0, m->meter_type);
        TEST_ASSERT_EQUAL_INT(0, m->meter_address);
    }
}

/* ---- Configure ---- */

/*
 * @feature Meter Telemetry
 * @req REQ-MTR-002
 * @scenario Configure sets meter type and address for a slot
 * @given An initialized telemetry struct
 * @when meter_telemetry_configure is called with type=4 (Eastron3P) and address=10
 * @then The slot reflects the configured type and address
 */
void test_configure_sets_type_and_address(void) {
    meter_telemetry_init(&tel);
    meter_telemetry_configure(&tel, METER_SLOT_MAINS, 4, 10);

    const meter_counters_t *m = meter_telemetry_get(&tel, METER_SLOT_MAINS);
    TEST_ASSERT_EQUAL_INT(4, m->meter_type);
    TEST_ASSERT_EQUAL_INT(10, m->meter_address);
    /* Counters should still be zero */
    TEST_ASSERT_EQUAL_INT(0, m->request_count);
}

/* ---- Increment counters ---- */

/*
 * @feature Meter Telemetry
 * @req REQ-MTR-003
 * @scenario Each counter increments independently
 * @given An initialized telemetry struct with mains slot configured
 * @when request, response, crc_error, and timeout are each incremented different numbers of times
 * @then Each counter reflects only its own increments
 */
void test_increment_counters_independently(void) {
    meter_telemetry_init(&tel);
    meter_telemetry_configure(&tel, METER_SLOT_MAINS, 4, 10);

    meter_telemetry_inc_request(&tel, METER_SLOT_MAINS);
    meter_telemetry_inc_request(&tel, METER_SLOT_MAINS);
    meter_telemetry_inc_request(&tel, METER_SLOT_MAINS);
    meter_telemetry_inc_response(&tel, METER_SLOT_MAINS);
    meter_telemetry_inc_response(&tel, METER_SLOT_MAINS);
    meter_telemetry_inc_crc_error(&tel, METER_SLOT_MAINS);
    /* No timeout increments */

    const meter_counters_t *m = meter_telemetry_get(&tel, METER_SLOT_MAINS);
    TEST_ASSERT_EQUAL_INT(3, m->request_count);
    TEST_ASSERT_EQUAL_INT(2, m->response_count);
    TEST_ASSERT_EQUAL_INT(1, m->crc_error_count);
    TEST_ASSERT_EQUAL_INT(0, m->timeout_count);
}

/*
 * @feature Meter Telemetry
 * @req REQ-MTR-004
 * @scenario Counters on different slots are independent
 * @given An initialized telemetry struct with mains and EV slots configured
 * @when Mains slot gets 5 requests and EV slot gets 2 requests
 * @then Each slot reflects only its own request count
 */
void test_slots_are_independent(void) {
    meter_telemetry_init(&tel);
    meter_telemetry_configure(&tel, METER_SLOT_MAINS, 4, 10);
    meter_telemetry_configure(&tel, METER_SLOT_EV, 10, 12);

    for (int i = 0; i < 5; i++)
        meter_telemetry_inc_request(&tel, METER_SLOT_MAINS);
    for (int i = 0; i < 2; i++)
        meter_telemetry_inc_request(&tel, METER_SLOT_EV);

    const meter_counters_t *mains = meter_telemetry_get(&tel, METER_SLOT_MAINS);
    const meter_counters_t *ev = meter_telemetry_get(&tel, METER_SLOT_EV);
    TEST_ASSERT_EQUAL_INT(5, mains->request_count);
    TEST_ASSERT_EQUAL_INT(2, ev->request_count);
}

/* ---- Saturation ---- */

/*
 * @feature Meter Telemetry
 * @req REQ-MTR-005
 * @scenario Counter saturates at UINT32_MAX instead of wrapping
 * @given A telemetry struct with request_count set to UINT32_MAX - 1
 * @when request is incremented twice
 * @then Counter reaches UINT32_MAX and stays there
 */
void test_counter_saturates_at_max(void) {
    meter_telemetry_init(&tel);
    /* Directly set to near-max to avoid billions of increments */
    tel.meters[METER_SLOT_MAINS].request_count = UINT32_MAX - 1;

    meter_telemetry_inc_request(&tel, METER_SLOT_MAINS);
    TEST_ASSERT_TRUE(tel.meters[METER_SLOT_MAINS].request_count == UINT32_MAX);

    /* One more should not wrap */
    meter_telemetry_inc_request(&tel, METER_SLOT_MAINS);
    TEST_ASSERT_TRUE(tel.meters[METER_SLOT_MAINS].request_count == UINT32_MAX);
}

/* ---- Reset slot ---- */

/*
 * @feature Meter Telemetry
 * @req REQ-MTR-006
 * @scenario Reset slot zeros counters but preserves type and address
 * @given A mains slot with type=4, address=10, and non-zero counters
 * @when meter_telemetry_reset_slot is called
 * @then All counters are zero but meter_type=4 and meter_address=10 are preserved
 */
void test_reset_slot_preserves_config(void) {
    meter_telemetry_init(&tel);
    meter_telemetry_configure(&tel, METER_SLOT_MAINS, 4, 10);
    meter_telemetry_inc_request(&tel, METER_SLOT_MAINS);
    meter_telemetry_inc_crc_error(&tel, METER_SLOT_MAINS);

    meter_telemetry_reset_slot(&tel, METER_SLOT_MAINS);

    const meter_counters_t *m = meter_telemetry_get(&tel, METER_SLOT_MAINS);
    TEST_ASSERT_EQUAL_INT(0, m->request_count);
    TEST_ASSERT_EQUAL_INT(0, m->crc_error_count);
    TEST_ASSERT_EQUAL_INT(4, m->meter_type);
    TEST_ASSERT_EQUAL_INT(10, m->meter_address);
}

/* ---- Reset all ---- */

/*
 * @feature Meter Telemetry
 * @req REQ-MTR-007
 * @scenario Reset all zeros counters on every slot but preserves each slot's config
 * @given Mains and EV slots are configured with non-zero counters
 * @when meter_telemetry_reset_all is called
 * @then All counters are zero and both slots retain their type and address
 */
void test_reset_all_preserves_config(void) {
    meter_telemetry_init(&tel);
    meter_telemetry_configure(&tel, METER_SLOT_MAINS, 4, 10);
    meter_telemetry_configure(&tel, METER_SLOT_EV, 10, 12);
    meter_telemetry_inc_request(&tel, METER_SLOT_MAINS);
    meter_telemetry_inc_timeout(&tel, METER_SLOT_EV);

    meter_telemetry_reset_all(&tel);

    const meter_counters_t *mains = meter_telemetry_get(&tel, METER_SLOT_MAINS);
    const meter_counters_t *ev = meter_telemetry_get(&tel, METER_SLOT_EV);
    TEST_ASSERT_EQUAL_INT(0, mains->request_count);
    TEST_ASSERT_EQUAL_INT(4, mains->meter_type);
    TEST_ASSERT_EQUAL_INT(0, ev->timeout_count);
    TEST_ASSERT_EQUAL_INT(10, ev->meter_type);
    TEST_ASSERT_EQUAL_INT(12, ev->meter_address);
}

/* ---- Out-of-range slot ---- */

/*
 * @feature Meter Telemetry
 * @req REQ-MTR-008
 * @scenario Out-of-range slot returns NULL and does not crash
 * @given An initialized telemetry struct
 * @when get, increment, configure, and reset are called with slot=METER_TELEMETRY_MAX_METERS
 * @then get returns NULL and no crash occurs
 */
void test_out_of_range_slot_safe(void) {
    meter_telemetry_init(&tel);

    const meter_counters_t *m = meter_telemetry_get(&tel, METER_TELEMETRY_MAX_METERS);
    TEST_ASSERT_TRUE(m == NULL);

    /* These should not crash */
    meter_telemetry_inc_request(&tel, METER_TELEMETRY_MAX_METERS);
    meter_telemetry_inc_response(&tel, 255);
    meter_telemetry_configure(&tel, METER_TELEMETRY_MAX_METERS, 4, 10);
    meter_telemetry_reset_slot(&tel, METER_TELEMETRY_MAX_METERS);

    /* Verify no corruption of valid slots */
    const meter_counters_t *valid = meter_telemetry_get(&tel, METER_SLOT_MAINS);
    TEST_ASSERT_EQUAL_INT(0, valid->request_count);
}

/* ---- Error rate ---- */

/*
 * @feature Meter Telemetry
 * @req REQ-MTR-009
 * @scenario Error rate is zero when no requests have been sent
 * @given An initialized telemetry struct with zero request count
 * @when meter_telemetry_error_rate is called
 * @then Returns 0
 */
void test_error_rate_zero_requests(void) {
    meter_telemetry_init(&tel);
    TEST_ASSERT_EQUAL_INT(0, meter_telemetry_error_rate(&tel, METER_SLOT_MAINS));
}

/*
 * @feature Meter Telemetry
 * @req REQ-MTR-010
 * @scenario Error rate calculated correctly from CRC errors and timeouts
 * @given 100 requests with 3 CRC errors and 7 timeouts
 * @when meter_telemetry_error_rate is called
 * @then Returns 10 (10%)
 */
void test_error_rate_mixed_errors(void) {
    meter_telemetry_init(&tel);
    tel.meters[METER_SLOT_MAINS].request_count = 100;
    tel.meters[METER_SLOT_MAINS].crc_error_count = 3;
    tel.meters[METER_SLOT_MAINS].timeout_count = 7;

    TEST_ASSERT_EQUAL_INT(10, meter_telemetry_error_rate(&tel, METER_SLOT_MAINS));
}

/*
 * @feature Meter Telemetry
 * @req REQ-MTR-011
 * @scenario Error rate caps at 100% when errors exceed requests
 * @given 10 requests with 15 timeout errors (more errors than requests due to counter manipulation)
 * @when meter_telemetry_error_rate is called
 * @then Returns 100 (capped)
 */
void test_error_rate_caps_at_100(void) {
    meter_telemetry_init(&tel);
    tel.meters[METER_SLOT_MAINS].request_count = 10;
    tel.meters[METER_SLOT_MAINS].timeout_count = 15;

    TEST_ASSERT_EQUAL_INT(100, meter_telemetry_error_rate(&tel, METER_SLOT_MAINS));
}

/*
 * @feature Meter Telemetry
 * @req REQ-MTR-012
 * @scenario Error rate for out-of-range slot returns 0
 * @given An initialized telemetry struct
 * @when meter_telemetry_error_rate is called with an invalid slot
 * @then Returns 0
 */
void test_error_rate_invalid_slot(void) {
    meter_telemetry_init(&tel);
    TEST_ASSERT_EQUAL_INT(0, meter_telemetry_error_rate(&tel, METER_TELEMETRY_MAX_METERS));
}

/* ---- NULL pointer safety ---- */

/*
 * @feature Meter Telemetry
 * @req REQ-MTR-013
 * @scenario All functions handle NULL pointer without crashing
 * @given A NULL meter_telemetry_t pointer
 * @when All API functions are called with NULL
 * @then No crash occurs, get returns NULL, error_rate returns 0
 */
void test_null_pointer_safety(void) {
    meter_telemetry_init(NULL);
    meter_telemetry_configure(NULL, 0, 4, 10);
    meter_telemetry_inc_request(NULL, 0);
    meter_telemetry_inc_response(NULL, 0);
    meter_telemetry_inc_crc_error(NULL, 0);
    meter_telemetry_inc_timeout(NULL, 0);
    meter_telemetry_reset_slot(NULL, 0);
    meter_telemetry_reset_all(NULL);

    const meter_counters_t *m = meter_telemetry_get(NULL, 0);
    TEST_ASSERT_TRUE(m == NULL);
    TEST_ASSERT_EQUAL_INT(0, meter_telemetry_error_rate(NULL, 0));
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("Meter Telemetry Counters");

    RUN_TEST(test_init_zeros_all);
    RUN_TEST(test_configure_sets_type_and_address);
    RUN_TEST(test_increment_counters_independently);
    RUN_TEST(test_slots_are_independent);
    RUN_TEST(test_counter_saturates_at_max);
    RUN_TEST(test_reset_slot_preserves_config);
    RUN_TEST(test_reset_all_preserves_config);
    RUN_TEST(test_out_of_range_slot_safe);
    RUN_TEST(test_error_rate_zero_requests);
    RUN_TEST(test_error_rate_mixed_errors);
    RUN_TEST(test_error_rate_caps_at_100);
    RUN_TEST(test_error_rate_invalid_slot);
    RUN_TEST(test_null_pointer_safety);

    TEST_SUITE_RESULTS();
}
