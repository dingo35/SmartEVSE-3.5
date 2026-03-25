/*
 * test_session_log.c — Native tests for charge session logging
 *
 * Tests the pure session logging logic:
 *   - Session start/end lifecycle
 *   - Energy calculation (end - start)
 *   - JSON output with ERE-required fields
 *   - OCPP transaction ID alignment
 *   - Edge cases (no active session, buffer overflow, etc.)
 */

#include "test_framework.h"
#include "session_log.h"
#include <string.h>
#include <stdlib.h>

static char json_buf[512];

/* ---- Session lifecycle tests ---- */

/*
 * @feature Charge Session Logging
 * @req REQ-ERE-001
 * @scenario Start and end a normal charge session
 * @given The session logger is initialized
 * @when A session is started then ended with energy readings
 * @then energy_charged_wh equals end_energy - start_energy
 */
void test_session_basic_lifecycle(void) {
    session_init();
    session_start(1710850200, 142300, 0); /* MODE_NORMAL */
    TEST_ASSERT_TRUE(session_is_active());

    session_end(1710865500, 154645, 160, 3);
    TEST_ASSERT_FALSE(session_is_active());

    const session_record_t *last = session_get_last();
    TEST_ASSERT_TRUE(last != NULL);
    TEST_ASSERT_EQUAL_INT(1, last->session_id);
    TEST_ASSERT_EQUAL_INT(1710850200, (int)last->start_time);
    TEST_ASSERT_EQUAL_INT(1710865500, (int)last->end_time);
    TEST_ASSERT_EQUAL_INT(142300, last->start_energy_wh);
    TEST_ASSERT_EQUAL_INT(154645, last->end_energy_wh);
    TEST_ASSERT_EQUAL_INT(12345, last->energy_charged_wh);
    TEST_ASSERT_EQUAL_INT(160, last->max_current_da);
    TEST_ASSERT_EQUAL_INT(3, last->phases);
    TEST_ASSERT_EQUAL_INT(0, last->mode);
    TEST_ASSERT_EQUAL_INT(0, last->ocpp_active);
}

/*
 * @feature Charge Session Logging
 * @req REQ-ERE-002
 * @scenario Session IDs increment across sessions
 * @given The session logger is initialized
 * @when Two sessions are started and ended
 * @then The second session has a higher session_id
 */
void test_session_id_increments(void) {
    session_init();
    session_start(1710000000, 0, 0);
    session_end(1710001000, 1000, 100, 1);
    const session_record_t *first = session_get_last();
    TEST_ASSERT_TRUE(first != NULL);
    uint32_t first_id = first->session_id;

    session_start(1710002000, 1000, 0);
    session_end(1710003000, 2000, 100, 1);
    const session_record_t *second = session_get_last();
    TEST_ASSERT_TRUE(second != NULL);
    TEST_ASSERT_GREATER_THAN((int)first_id, (int)second->session_id);
}

/*
 * @feature Charge Session Logging
 * @req REQ-ERE-003
 * @scenario OCPP transaction ID replaces session ID
 * @given An active charge session
 * @when session_set_ocpp_id is called with a transaction ID
 * @then The session record has ocpp_active=1 and the OCPP transaction ID
 */
void test_session_ocpp_id(void) {
    session_init();
    session_start(1710000000, 50000, 2); /* MODE_SOLAR */
    session_set_ocpp_id(42);
    session_end(1710001000, 55000, 160, 3);

    const session_record_t *last = session_get_last();
    TEST_ASSERT_TRUE(last != NULL);
    TEST_ASSERT_EQUAL_INT(42, (int)last->session_id);
    TEST_ASSERT_EQUAL_INT(1, last->ocpp_active);
    TEST_ASSERT_EQUAL_INT(5000, last->energy_charged_wh);
}

/*
 * @feature Charge Session Logging
 * @req REQ-ERE-004
 * @scenario session_end without prior session_start is ignored
 * @given The session logger is initialized with no active session
 * @when session_end is called
 * @then No crash occurs and session_get_last returns NULL
 */
void test_session_end_without_start(void) {
    session_init();
    TEST_ASSERT_FALSE(session_is_active());
    session_end(1710001000, 55000, 160, 3);
    TEST_ASSERT_FALSE(session_is_active());
    TEST_ASSERT_TRUE(session_get_last() == NULL);
}

/*
 * @feature Charge Session Logging
 * @req REQ-ERE-005
 * @scenario session_start while session active discards previous
 * @given An active charge session
 * @when session_start is called again
 * @then The previous session is discarded and a new one begins
 */
void test_session_start_while_active(void) {
    session_init();
    session_start(1710000000, 50000, 0);
    TEST_ASSERT_TRUE(session_is_active());

    /* Start a new session without ending the first */
    session_start(1710001000, 60000, 1);
    TEST_ASSERT_TRUE(session_is_active());

    session_end(1710002000, 65000, 100, 1);
    const session_record_t *last = session_get_last();
    TEST_ASSERT_TRUE(last != NULL);
    TEST_ASSERT_EQUAL_INT(60000, last->start_energy_wh);
    TEST_ASSERT_EQUAL_INT(5000, last->energy_charged_wh);
    TEST_ASSERT_EQUAL_INT(1, last->mode); /* MODE_SMART */
}

/*
 * @feature Charge Session Logging
 * @req REQ-ERE-006
 * @scenario session_get_last before any session returns NULL
 * @given The session logger is freshly initialized
 * @when session_get_last is called
 * @then NULL is returned
 */
void test_session_get_last_before_any(void) {
    session_init();
    TEST_ASSERT_TRUE(session_get_last() == NULL);
}

/*
 * @feature Charge Session Logging
 * @req REQ-ERE-007
 * @scenario session_set_ocpp_id with no active session is ignored
 * @given The session logger is initialized with no active session
 * @when session_set_ocpp_id is called
 * @then No crash occurs and no state changes
 */
void test_session_set_ocpp_no_active(void) {
    session_init();
    session_set_ocpp_id(99);
    TEST_ASSERT_FALSE(session_is_active());
    TEST_ASSERT_TRUE(session_get_last() == NULL);
}

/*
 * @feature Charge Session Logging
 * @req REQ-ERE-008
 * @scenario Solar mode session records mode correctly
 * @given The session logger is initialized
 * @when A session is started with MODE_SOLAR
 * @then The completed record has mode=2
 */
void test_session_solar_mode(void) {
    session_init();
    session_start(1710000000, 10000, 2); /* MODE_SOLAR */
    session_end(1710001000, 15000, 80, 1);

    const session_record_t *last = session_get_last();
    TEST_ASSERT_TRUE(last != NULL);
    TEST_ASSERT_EQUAL_INT(2, last->mode);
    TEST_ASSERT_EQUAL_INT(1, last->phases);
    TEST_ASSERT_EQUAL_INT(80, last->max_current_da);
}

/* ---- JSON output tests ---- */

/*
 * @feature Charge Session JSON Export
 * @req REQ-ERE-010
 * @scenario JSON output contains all ERE-required fields
 * @given A completed charge session
 * @when session_to_json is called
 * @then The JSON contains session_id, start, end, kwh, and energy fields
 */
void test_session_json_basic(void) {
    session_init();
    /* 2026-03-19T14:30:00Z = epoch 1773930600 */
    session_start(1773930600, 142300, 2);
    session_end(1773945900, 154645, 160, 3);

    const session_record_t *last = session_get_last();
    TEST_ASSERT_TRUE(last != NULL);

    int n = session_to_json(last, json_buf, sizeof(json_buf));
    TEST_ASSERT_GREATER_THAN(0, n);

    /* Verify key fields present */
    TEST_ASSERT_TRUE(strstr(json_buf, "\"session_id\":1") != NULL);
    TEST_ASSERT_TRUE(strstr(json_buf, "\"start\":\"2026-03-19T14:30:00Z\"") != NULL);
    TEST_ASSERT_TRUE(strstr(json_buf, "\"end\":\"2026-03-19T18:45:00Z\"") != NULL);
    TEST_ASSERT_TRUE(strstr(json_buf, "\"kwh\":12.345") != NULL);
    TEST_ASSERT_TRUE(strstr(json_buf, "\"start_energy_wh\":142300") != NULL);
    TEST_ASSERT_TRUE(strstr(json_buf, "\"end_energy_wh\":154645") != NULL);
    TEST_ASSERT_TRUE(strstr(json_buf, "\"max_current_a\":16.0") != NULL);
    TEST_ASSERT_TRUE(strstr(json_buf, "\"phases\":3") != NULL);
    TEST_ASSERT_TRUE(strstr(json_buf, "\"mode\":\"solar\"") != NULL);
    TEST_ASSERT_TRUE(strstr(json_buf, "\"ocpp_tx_id\":null") != NULL);
}

/*
 * @feature Charge Session JSON Export
 * @req REQ-ERE-011
 * @scenario JSON with OCPP active includes transaction ID
 * @given A completed session with OCPP transaction ID set
 * @when session_to_json is called
 * @then ocpp_tx_id contains the numeric transaction ID
 */
void test_session_json_ocpp(void) {
    session_init();
    session_start(1773930600, 50000, 0);
    session_set_ocpp_id(42);
    session_end(1773945900, 55000, 320, 3);

    const session_record_t *last = session_get_last();
    int n = session_to_json(last, json_buf, sizeof(json_buf));
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_TRUE(strstr(json_buf, "\"ocpp_tx_id\":42") != NULL);
    TEST_ASSERT_TRUE(strstr(json_buf, "\"mode\":\"normal\"") != NULL);
}

/*
 * @feature Charge Session JSON Export
 * @req REQ-ERE-012
 * @scenario Null record returns -1
 * @given A NULL session record pointer
 * @when session_to_json is called
 * @then It returns -1
 */
void test_session_json_null_record(void) {
    int n = session_to_json(NULL, json_buf, sizeof(json_buf));
    TEST_ASSERT_EQUAL_INT(-1, n);
}

/*
 * @feature Charge Session JSON Export
 * @req REQ-ERE-012
 * @scenario Null buffer returns -1
 * @given A valid session record
 * @when session_to_json is called with NULL buffer
 * @then It returns -1
 */
void test_session_json_null_buffer(void) {
    session_init();
    session_start(1710000000, 0, 0);
    session_end(1710001000, 1000, 100, 1);
    const session_record_t *last = session_get_last();
    int n = session_to_json(last, NULL, 512);
    TEST_ASSERT_EQUAL_INT(-1, n);
}

/*
 * @feature Charge Session JSON Export
 * @req REQ-ERE-012
 * @scenario Zero-length buffer returns -1
 * @given A valid session record
 * @when session_to_json is called with bufsz=0
 * @then It returns -1
 */
void test_session_json_zero_buffer(void) {
    session_init();
    session_start(1710000000, 0, 0);
    session_end(1710001000, 1000, 100, 1);
    const session_record_t *last = session_get_last();
    int n = session_to_json(last, json_buf, 0);
    TEST_ASSERT_EQUAL_INT(-1, n);
}

/*
 * @feature Charge Session JSON Export
 * @req REQ-ERE-013
 * @scenario Too-small buffer returns -1
 * @given A valid session record
 * @when session_to_json is called with a very small buffer
 * @then It returns -1 (truncation detected)
 */
void test_session_json_small_buffer(void) {
    session_init();
    session_start(1710000000, 0, 0);
    session_end(1710001000, 1000, 100, 1);
    const session_record_t *last = session_get_last();
    int n = session_to_json(last, json_buf, 10);
    TEST_ASSERT_EQUAL_INT(-1, n);
}

/*
 * @feature Charge Session JSON Export
 * @req REQ-ERE-014
 * @scenario Smart mode string in JSON
 * @given A completed session in MODE_SMART
 * @when session_to_json is called
 * @then mode field is "smart"
 */
void test_session_json_smart_mode(void) {
    session_init();
    session_start(1710000000, 0, 1); /* MODE_SMART */
    session_end(1710001000, 500, 60, 1);

    const session_record_t *last = session_get_last();
    int n = session_to_json(last, json_buf, sizeof(json_buf));
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_TRUE(strstr(json_buf, "\"mode\":\"smart\"") != NULL);
}

/*
 * @feature Charge Session Logging
 * @req REQ-ERE-015
 * @scenario Zero energy session is recorded correctly
 * @given A session where start and end energy are the same
 * @when The session ends
 * @then energy_charged_wh is 0
 */
void test_session_zero_energy(void) {
    session_init();
    session_start(1710000000, 50000, 0);
    session_end(1710000061, 50000, 0, 1);

    const session_record_t *last = session_get_last();
    TEST_ASSERT_TRUE(last != NULL);
    TEST_ASSERT_EQUAL_INT(0, last->energy_charged_wh);
}

/* ---- Circuit energy tests ---- */

/*
 * @feature Charge Session Logging
 * @req REQ-CIR-020
 * @scenario Session with circuit energy includes circuit_kwh in JSON
 * @given A completed session with CircuitMeter energy set
 * @when session_to_json is called
 * @then JSON includes circuit_kwh field with correct value
 */
void test_session_circuit_energy_json(void) {
    session_init();
    session_start(1773930600, 142300, 0);
    session_set_circuit_energy(200000, 0); /* start energy */
    session_set_circuit_energy(0, 212345); /* end energy */
    session_end(1773945900, 154645, 160, 3);

    const session_record_t *last = session_get_last();
    TEST_ASSERT_TRUE(last != NULL);
    TEST_ASSERT_EQUAL_INT(200000, last->circuit_start_energy_wh);
    TEST_ASSERT_EQUAL_INT(212345, last->circuit_end_energy_wh);
    TEST_ASSERT_EQUAL_INT(12345, last->circuit_energy_wh);

    int n = session_to_json(last, json_buf, sizeof(json_buf));
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_TRUE(strstr(json_buf, "\"circuit_kwh\":12.345") != NULL);
}

/*
 * @feature Charge Session Logging
 * @req REQ-CIR-021
 * @scenario Session without circuit energy omits circuit_kwh from JSON
 * @given A completed session without CircuitMeter energy
 * @when session_to_json is called
 * @then JSON does not include circuit_kwh field
 */
void test_session_no_circuit_energy_json(void) {
    session_init();
    session_start(1773930600, 142300, 0);
    session_end(1773945900, 154645, 160, 3);

    const session_record_t *last = session_get_last();
    TEST_ASSERT_TRUE(last != NULL);
    TEST_ASSERT_EQUAL_INT(0, last->circuit_energy_wh);

    int n = session_to_json(last, json_buf, sizeof(json_buf));
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_TRUE(strstr(json_buf, "circuit_kwh") == NULL);
}

/*
 * @feature Charge Session Logging
 * @req REQ-CIR-022
 * @scenario Circuit energy calculation: end minus start
 * @given A session with circuit start=100000 and circuit end=107500
 * @when The session ends
 * @then circuit_energy_wh equals 7500
 */
void test_session_circuit_energy_calculation(void) {
    session_init();
    session_start(1710000000, 50000, 2);
    session_set_circuit_energy(100000, 0);
    session_set_circuit_energy(0, 107500);
    session_end(1710001000, 55000, 80, 1);

    const session_record_t *last = session_get_last();
    TEST_ASSERT_TRUE(last != NULL);
    TEST_ASSERT_EQUAL_INT(100000, last->circuit_start_energy_wh);
    TEST_ASSERT_EQUAL_INT(107500, last->circuit_end_energy_wh);
    TEST_ASSERT_EQUAL_INT(7500, last->circuit_energy_wh);
}

/*
 * @feature Charge Session Logging
 * @req REQ-CIR-023
 * @scenario session_set_circuit_energy with no active session is ignored
 * @given No active session
 * @when session_set_circuit_energy is called
 * @then No crash and no state change
 */
void test_session_set_circuit_energy_no_active(void) {
    session_init();
    session_set_circuit_energy(100000, 0);
    TEST_ASSERT_FALSE(session_is_active());
    TEST_ASSERT_TRUE(session_get_last() == NULL);
}

/* ---- NTP time validity guard tests ---- */

/*
 * @feature Charge Session Logging
 * @req REQ-ERE-020
 * @scenario session_start with timestamp 0 is rejected
 * @given The session logger is initialized
 * @when session_start is called with timestamp 0 (NTP not synced)
 * @then No session is started and session_is_active returns 0
 */
void test_session_start_rejects_zero_timestamp(void) {
    session_init();
    session_start(0, 50000, 0);
    TEST_ASSERT_FALSE(session_is_active());
}

/*
 * @feature Charge Session Logging
 * @req REQ-ERE-021
 * @scenario session_start with pre-2024 timestamp is rejected
 * @given The session logger is initialized
 * @when session_start is called with timestamp 1000 (pre-2024)
 * @then No session is started and session_is_active returns 0
 */
void test_session_start_rejects_pre2024_timestamp(void) {
    session_init();
    session_start(1000, 50000, 0);
    TEST_ASSERT_FALSE(session_is_active());
}

/*
 * @feature Charge Session Logging
 * @req REQ-ERE-022
 * @scenario session_start with valid 2024+ timestamp succeeds
 * @given The session logger is initialized
 * @when session_start is called with timestamp 1710000000 (March 2024)
 * @then A session is started and session_is_active returns 1
 */
void test_session_start_accepts_valid_timestamp(void) {
    session_init();
    session_start(1710000000, 50000, 0);
    TEST_ASSERT_TRUE(session_is_active());
}

/* ---- Minimum duration filter tests ---- */

/*
 * @feature Charge Session Logging
 * @req REQ-ERE-025
 * @scenario Session shorter than 60 seconds is discarded
 * @given The session logger is initialized
 * @when A session is started and ended after only 30 seconds
 * @then The session is discarded, session_is_active returns 0, and session_get_last returns NULL
 */
void test_session_short_duration_discarded(void) {
    session_init();
    session_start(1710000000, 50000, 0);
    TEST_ASSERT_TRUE(session_is_active());

    session_end(1710000030, 51000, 100, 1); /* 30 seconds — below threshold */
    TEST_ASSERT_FALSE(session_is_active());
    TEST_ASSERT_TRUE(session_get_last() == NULL);
}

/*
 * @feature Charge Session Logging
 * @req REQ-ERE-026
 * @scenario Session with duration exactly 60 seconds is kept
 * @given The session logger is initialized
 * @when A session is started and ended after exactly 60 seconds
 * @then The session is stored and session_get_last returns a valid record with correct energy
 */
void test_session_exact_min_duration_kept(void) {
    session_init();
    session_start(1710000000, 50000, 0);
    session_end(1710000060, 51000, 100, 1); /* Exactly 60 seconds — at threshold */

    const session_record_t *last = session_get_last();
    TEST_ASSERT_TRUE(last != NULL);
    TEST_ASSERT_EQUAL_INT(1000, last->energy_charged_wh);
}

int main(void) {
    TEST_SUITE_BEGIN("Charge Session Logging");

    /* Lifecycle tests */
    RUN_TEST(test_session_basic_lifecycle);
    RUN_TEST(test_session_id_increments);
    RUN_TEST(test_session_ocpp_id);
    RUN_TEST(test_session_end_without_start);
    RUN_TEST(test_session_start_while_active);
    RUN_TEST(test_session_get_last_before_any);
    RUN_TEST(test_session_set_ocpp_no_active);
    RUN_TEST(test_session_solar_mode);

    /* JSON tests */
    RUN_TEST(test_session_json_basic);
    RUN_TEST(test_session_json_ocpp);
    RUN_TEST(test_session_json_null_record);
    RUN_TEST(test_session_json_null_buffer);
    RUN_TEST(test_session_json_zero_buffer);
    RUN_TEST(test_session_json_small_buffer);
    RUN_TEST(test_session_json_smart_mode);
    RUN_TEST(test_session_zero_energy);

    /* Circuit energy tests */
    RUN_TEST(test_session_circuit_energy_json);
    RUN_TEST(test_session_no_circuit_energy_json);
    RUN_TEST(test_session_circuit_energy_calculation);
    RUN_TEST(test_session_set_circuit_energy_no_active);

    /* Minimum duration filter tests */
    RUN_TEST(test_session_short_duration_discarded);
    RUN_TEST(test_session_exact_min_duration_kept);

    /* NTP time validity guard tests */
    RUN_TEST(test_session_start_rejects_zero_timestamp);
    RUN_TEST(test_session_start_rejects_pre2024_timestamp);
    RUN_TEST(test_session_start_accepts_valid_timestamp);

    TEST_SUITE_RESULTS();
}
