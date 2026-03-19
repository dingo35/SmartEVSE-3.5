/*
 * test_ocpp_telemetry.c - OCPP telemetry counter tests
 *
 * Tests the pure C telemetry counter management functions.
 */

#include "test_framework.h"
#include "ocpp_telemetry.h"

static ocpp_telemetry_t t;

static void setup(void) {
    ocpp_telemetry_init(&t);
}

/* ---- Init ---- */

/*
 * @feature OCPP Telemetry
 * @req REQ-OCPP-080
 * @scenario Telemetry init zeros all counters
 * @given A telemetry struct with arbitrary values
 * @when ocpp_telemetry_init is called
 * @then All counters and flags are zero/false
 */
void test_telemetry_init_zeros_all(void) {
    t.ws_connect_count = 42;
    t.tx_active = true;
    t.auth_accept_count = 99;
    ocpp_telemetry_init(&t);
    TEST_ASSERT_EQUAL_INT(0, t.ws_connect_count);
    TEST_ASSERT_EQUAL_INT(0, t.ws_disconnect_count);
    TEST_ASSERT_EQUAL_INT(0, t.tx_start_count);
    TEST_ASSERT_EQUAL_INT(0, t.tx_stop_count);
    TEST_ASSERT_FALSE(t.tx_active);
    TEST_ASSERT_EQUAL_INT(0, t.auth_accept_count);
    TEST_ASSERT_EQUAL_INT(0, t.auth_reject_count);
    TEST_ASSERT_EQUAL_INT(0, t.auth_timeout_count);
    TEST_ASSERT_FALSE(t.lb_conflict);
}

/* ---- Transaction lifecycle ---- */

/*
 * @feature OCPP Telemetry
 * @req REQ-OCPP-081
 * @scenario Transaction start increments counter and sets active flag
 * @given Telemetry is initialized
 * @when ocpp_telemetry_tx_started is called
 * @then tx_start_count is 1 and tx_active is true
 */
void test_telemetry_tx_start(void) {
    setup();
    ocpp_telemetry_tx_started(&t);
    TEST_ASSERT_EQUAL_INT(1, t.tx_start_count);
    TEST_ASSERT_TRUE(t.tx_active);
}

/*
 * @feature OCPP Telemetry
 * @req REQ-OCPP-081
 * @scenario Transaction stop increments counter and clears active flag
 * @given A transaction has been started
 * @when ocpp_telemetry_tx_stopped is called
 * @then tx_stop_count is 1 and tx_active is false
 */
void test_telemetry_tx_stop(void) {
    setup();
    ocpp_telemetry_tx_started(&t);
    ocpp_telemetry_tx_stopped(&t);
    TEST_ASSERT_EQUAL_INT(1, t.tx_stop_count);
    TEST_ASSERT_FALSE(t.tx_active);
}

/*
 * @feature OCPP Telemetry
 * @req REQ-OCPP-081
 * @scenario Multiple transactions accumulate counters
 * @given Telemetry is initialized
 * @when 3 transactions are started and stopped
 * @then tx_start_count and tx_stop_count are both 3
 */
void test_telemetry_multiple_tx(void) {
    setup();
    for (int i = 0; i < 3; i++) {
        ocpp_telemetry_tx_started(&t);
        ocpp_telemetry_tx_stopped(&t);
    }
    TEST_ASSERT_EQUAL_INT(3, t.tx_start_count);
    TEST_ASSERT_EQUAL_INT(3, t.tx_stop_count);
    TEST_ASSERT_FALSE(t.tx_active);
}

/* ---- Authorization tracking ---- */

/*
 * @feature OCPP Telemetry
 * @req REQ-OCPP-082
 * @scenario Auth accept/reject/timeout counters increment independently
 * @given Telemetry is initialized
 * @when 2 accepts, 1 reject, 1 timeout are recorded
 * @then Each counter reflects the correct count
 */
void test_telemetry_auth_counters(void) {
    setup();
    ocpp_telemetry_auth_accepted(&t);
    ocpp_telemetry_auth_accepted(&t);
    ocpp_telemetry_auth_rejected(&t);
    ocpp_telemetry_auth_timeout(&t);
    TEST_ASSERT_EQUAL_INT(2, t.auth_accept_count);
    TEST_ASSERT_EQUAL_INT(1, t.auth_reject_count);
    TEST_ASSERT_EQUAL_INT(1, t.auth_timeout_count);
}

/* ---- WebSocket tracking ---- */

/*
 * @feature OCPP Telemetry
 * @req REQ-OCPP-083
 * @scenario WebSocket connect/disconnect counters track reconnections
 * @given Telemetry is initialized
 * @when 5 connects and 4 disconnects occur (currently connected)
 * @then ws_connect_count=5, ws_disconnect_count=4
 */
void test_telemetry_ws_reconnect_tracking(void) {
    setup();
    for (int i = 0; i < 5; i++) {
        ocpp_telemetry_ws_connected(&t);
        if (i < 4) ocpp_telemetry_ws_disconnected(&t);
    }
    TEST_ASSERT_EQUAL_INT(5, t.ws_connect_count);
    TEST_ASSERT_EQUAL_INT(4, t.ws_disconnect_count);
}

/* ---- NULL safety ---- */

/*
 * @feature OCPP Telemetry
 * @req REQ-OCPP-080
 * @scenario NULL pointer does not crash
 * @given NULL telemetry pointer
 * @when Any telemetry function is called
 * @then No crash occurs
 */
void test_telemetry_null_safety(void) {
    ocpp_telemetry_init(NULL);
    ocpp_telemetry_ws_connected(NULL);
    ocpp_telemetry_ws_disconnected(NULL);
    ocpp_telemetry_tx_started(NULL);
    ocpp_telemetry_tx_stopped(NULL);
    ocpp_telemetry_auth_accepted(NULL);
    ocpp_telemetry_auth_rejected(NULL);
    ocpp_telemetry_auth_timeout(NULL);
    /* If we got here, no crash */
    TEST_ASSERT_TRUE(true);
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("OCPP Telemetry");

    RUN_TEST(test_telemetry_init_zeros_all);
    RUN_TEST(test_telemetry_tx_start);
    RUN_TEST(test_telemetry_tx_stop);
    RUN_TEST(test_telemetry_multiple_tx);
    RUN_TEST(test_telemetry_auth_counters);
    RUN_TEST(test_telemetry_ws_reconnect_tracking);
    RUN_TEST(test_telemetry_null_safety);

    TEST_SUITE_RESULTS();
}
