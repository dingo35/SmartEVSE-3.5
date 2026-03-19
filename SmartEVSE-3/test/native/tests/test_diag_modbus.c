/*
 * test_diag_modbus.c - Native tests for Modbus event ring buffer
 */

#include "test_framework.h"
#include "diag_modbus.h"
#include <string.h>

static diag_mb_ring_t ring;

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-049
 * @scenario Modbus event ring initializes empty and disabled
 * @given A diag_mb_ring_t
 * @when diag_mb_init is called
 * @then count is 0, head is 0, enabled is false
 */
void test_mb_init(void)
{
    diag_mb_init(&ring);
    TEST_ASSERT_EQUAL(0, ring.count);
    TEST_ASSERT_EQUAL(0, ring.head);
    TEST_ASSERT_FALSE(ring.enabled);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-049
 * @scenario Disabled ring rejects records
 * @given An initialized but disabled Modbus ring
 * @when diag_mb_record is called
 * @then count stays 0
 */
void test_mb_disabled_rejects(void)
{
    diag_mb_init(&ring);
    diag_mb_record(&ring, 1000, 1, 4, DIAG_MB_EVENT_SENT, 0);
    TEST_ASSERT_EQUAL(0, ring.count);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-049
 * @scenario Record and read a single event
 * @given An enabled Modbus ring
 * @when One SENT event is recorded
 * @then diag_mb_read returns 1 event with correct fields
 */
void test_mb_record_and_read(void)
{
    diag_mb_init(&ring);
    diag_mb_enable(&ring, true);
    diag_mb_record(&ring, 5000, 2, 3, DIAG_MB_EVENT_SENT, 0);

    TEST_ASSERT_EQUAL(1, ring.count);

    diag_mb_event_t out[1];
    uint8_t n = diag_mb_read(&ring, out, 1);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL(5000, (int)out[0].timestamp_ms);
    TEST_ASSERT_EQUAL(2, out[0].address);
    TEST_ASSERT_EQUAL(3, out[0].function);
    TEST_ASSERT_EQUAL(DIAG_MB_EVENT_SENT, out[0].event_type);
    TEST_ASSERT_EQUAL(0, out[0].error_code);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-049
 * @scenario Ring wraps around after 32 events
 * @given An enabled Modbus ring
 * @when 35 events are recorded
 * @then count is 32 and oldest 3 events are overwritten
 */
void test_mb_wrap_around(void)
{
    diag_mb_init(&ring);
    diag_mb_enable(&ring, true);

    for (uint32_t i = 1; i <= 35; i++)
        diag_mb_record(&ring, i * 100, 1, 4, DIAG_MB_EVENT_SENT, 0);

    TEST_ASSERT_EQUAL(32, ring.count);

    diag_mb_event_t out[32];
    uint8_t n = diag_mb_read(&ring, out, 32);
    TEST_ASSERT_EQUAL(32, n);
    /* Oldest should be event #4 (timestamps 400) */
    TEST_ASSERT_EQUAL(400, (int)out[0].timestamp_ms);
    /* Newest should be event #35 (timestamp 3500) */
    TEST_ASSERT_EQUAL(3500, (int)out[31].timestamp_ms);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-049
 * @scenario Reset clears all events
 * @given A ring with 5 events
 * @when diag_mb_reset is called
 * @then count is 0 and read returns 0
 */
void test_mb_reset(void)
{
    diag_mb_init(&ring);
    diag_mb_enable(&ring, true);
    for (int i = 0; i < 5; i++)
        diag_mb_record(&ring, i * 10, 1, 4, DIAG_MB_EVENT_SENT, 0);
    TEST_ASSERT_EQUAL(5, ring.count);

    diag_mb_reset(&ring);
    TEST_ASSERT_EQUAL(0, ring.count);

    diag_mb_event_t out[1];
    TEST_ASSERT_EQUAL(0, diag_mb_read(&ring, out, 1));
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-049
 * @scenario Error events record error code
 * @given An enabled Modbus ring
 * @when An ERROR event with error code 0xE2 is recorded
 * @then The event has event_type ERROR and error_code 0xE2
 */
void test_mb_error_event(void)
{
    diag_mb_init(&ring);
    diag_mb_enable(&ring, true);
    diag_mb_record(&ring, 9999, 5, 4, DIAG_MB_EVENT_ERROR, 0xE2);

    diag_mb_event_t out[1];
    uint8_t n = diag_mb_read(&ring, out, 1);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL(DIAG_MB_EVENT_ERROR, out[0].event_type);
    TEST_ASSERT_EQUAL(0xE2, out[0].error_code);
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-049
 * @scenario Event struct is exactly 8 bytes
 * @given The diag_mb_event_t struct
 * @when sizeof is checked
 * @then The size is 8 bytes
 */
void test_mb_event_size(void)
{
    TEST_ASSERT_EQUAL(8, (int)sizeof(diag_mb_event_t));
}

/*
 * @feature Diagnostic Telemetry
 * @req REQ-E2E-049
 * @scenario NULL ring pointer is safe for all operations
 * @given A NULL ring pointer
 * @when init, record, read, reset, enable are called
 * @then No crash occurs
 */
void test_mb_null_safety(void)
{
    diag_mb_init(NULL);
    diag_mb_record(NULL, 0, 0, 0, 0, 0);
    diag_mb_event_t out[1];
    TEST_ASSERT_EQUAL(0, diag_mb_read(NULL, out, 1));
    diag_mb_reset(NULL);
    diag_mb_enable(NULL, true);
    TEST_ASSERT_TRUE(1);
}

int main(void)
{
    TEST_SUITE_BEGIN("Diagnostic Modbus Event Ring");

    RUN_TEST(test_mb_init);
    RUN_TEST(test_mb_disabled_rejects);
    RUN_TEST(test_mb_record_and_read);
    RUN_TEST(test_mb_wrap_around);
    RUN_TEST(test_mb_reset);
    RUN_TEST(test_mb_error_event);
    RUN_TEST(test_mb_event_size);
    RUN_TEST(test_mb_null_safety);

    TEST_SUITE_RESULTS();
}
