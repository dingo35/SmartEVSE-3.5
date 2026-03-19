/*
 * test_modbus_log.c - Modbus frame event ring buffer tests
 *
 * Tests:
 *   - Init zeros all fields
 *   - Append and retrieve single entry
 *   - Multiple entries maintain order
 *   - Wraparound overwrites oldest
 *   - Get with oldest-first indexing after wraparound
 *   - Clear resets count but preserves total
 *   - Out-of-range index returns NULL
 *   - NULL pointer safety
 *   - Total counter increments and survives clear
 *   - Full buffer then clear then refill
 */

#include "test_framework.h"
#include "modbus_log.h"

static modbus_log_t log_buf;

/* ---- Init ---- */

/*
 * @feature Modbus Frame Logging
 * @req REQ-MTR-090
 * @scenario Init zeros all ring buffer fields
 * @given An uninitialized modbus_log_t
 * @when modbus_log_init is called
 * @then count=0, head=0, total_logged=0
 */
void test_log_init(void) {
    memset(&log_buf, 0xFF, sizeof(log_buf));
    modbus_log_init(&log_buf);

    TEST_ASSERT_EQUAL_INT(0, modbus_log_count(&log_buf));
    TEST_ASSERT_EQUAL_INT(0, modbus_log_total(&log_buf));
}

/* ---- Single entry ---- */

/*
 * @feature Modbus Frame Logging
 * @req REQ-MTR-091
 * @scenario Append and retrieve a single entry
 * @given An initialized ring buffer
 * @when One entry is appended with timestamp=1000, addr=0x0A, func=0x04, reg=0x0006
 * @then count=1, get(0) returns the entry with correct fields
 */
void test_log_single_entry(void) {
    modbus_log_init(&log_buf);
    modbus_log_append(&log_buf, 1000, MODBUS_DIR_TX, 0x0A, 0x04, 0x0006, 12, 0);

    TEST_ASSERT_EQUAL_INT(1, modbus_log_count(&log_buf));
    const modbus_log_entry_t *e = modbus_log_get(&log_buf, 0);
    TEST_ASSERT_TRUE(e != NULL);
    TEST_ASSERT_EQUAL_INT(1000, (int)e->timestamp_ms);
    TEST_ASSERT_EQUAL_INT(MODBUS_DIR_TX, e->direction);
    TEST_ASSERT_EQUAL_INT(0x0A, e->address);
    TEST_ASSERT_EQUAL_INT(0x04, e->function);
    TEST_ASSERT_EQUAL_INT(0x0006, e->reg);
    TEST_ASSERT_EQUAL_INT(12, e->reg_count);
    TEST_ASSERT_EQUAL_INT(0, e->result);
}

/* ---- Multiple entries maintain order ---- */

/*
 * @feature Modbus Frame Logging
 * @req REQ-MTR-092
 * @scenario Multiple entries maintain chronological order
 * @given An initialized ring buffer
 * @when 3 entries are appended with timestamps 100, 200, 300
 * @then get(0) returns t=100, get(1) returns t=200, get(2) returns t=300
 */
void test_log_order(void) {
    modbus_log_init(&log_buf);
    modbus_log_append(&log_buf, 100, MODBUS_DIR_TX, 1, 0x04, 0, 1, 0);
    modbus_log_append(&log_buf, 200, MODBUS_DIR_RX, 1, 0x04, 0, 1, 3);
    modbus_log_append(&log_buf, 300, MODBUS_DIR_TX, 2, 0x03, 0x100, 2, 0);

    TEST_ASSERT_EQUAL_INT(3, modbus_log_count(&log_buf));
    TEST_ASSERT_EQUAL_INT(100, (int)modbus_log_get(&log_buf, 0)->timestamp_ms);
    TEST_ASSERT_EQUAL_INT(200, (int)modbus_log_get(&log_buf, 1)->timestamp_ms);
    TEST_ASSERT_EQUAL_INT(300, (int)modbus_log_get(&log_buf, 2)->timestamp_ms);
}

/* ---- Wraparound ---- */

/*
 * @feature Modbus Frame Logging
 * @req REQ-MTR-093
 * @scenario Ring buffer wraps around and overwrites oldest entries
 * @given An initialized ring buffer
 * @when MODBUS_LOG_SIZE + 5 entries are appended
 * @then count equals MODBUS_LOG_SIZE, oldest entries are overwritten
 */
void test_log_wraparound(void) {
    modbus_log_init(&log_buf);

    /* Fill buffer + 5 extra */
    for (int i = 0; i < MODBUS_LOG_SIZE + 5; i++) {
        modbus_log_append(&log_buf, (uint32_t)(i * 100), MODBUS_DIR_TX,
                          (uint8_t)(i & 0xFF), 0x04, (uint16_t)i, 1, 0);
    }

    TEST_ASSERT_EQUAL_INT(MODBUS_LOG_SIZE, modbus_log_count(&log_buf));
    TEST_ASSERT_EQUAL_INT(MODBUS_LOG_SIZE + 5, (int)modbus_log_total(&log_buf));

    /* Oldest entry should be index 5 (first 5 were overwritten) */
    const modbus_log_entry_t *oldest = modbus_log_get(&log_buf, 0);
    TEST_ASSERT_TRUE(oldest != NULL);
    TEST_ASSERT_EQUAL_INT(500, (int)oldest->timestamp_ms);

    /* Newest entry should be at index count-1 */
    const modbus_log_entry_t *newest = modbus_log_get(&log_buf, MODBUS_LOG_SIZE - 1);
    TEST_ASSERT_TRUE(newest != NULL);
    TEST_ASSERT_EQUAL_INT((MODBUS_LOG_SIZE + 4) * 100, (int)newest->timestamp_ms);
}

/* ---- Exact fill ---- */

/*
 * @feature Modbus Frame Logging
 * @req REQ-MTR-094
 * @scenario Ring buffer at exactly MODBUS_LOG_SIZE entries
 * @given An initialized ring buffer
 * @when Exactly MODBUS_LOG_SIZE entries are appended
 * @then count equals MODBUS_LOG_SIZE, all entries accessible in order
 */
void test_log_exact_fill(void) {
    modbus_log_init(&log_buf);

    for (int i = 0; i < MODBUS_LOG_SIZE; i++) {
        modbus_log_append(&log_buf, (uint32_t)(i * 10), MODBUS_DIR_TX,
                          (uint8_t)i, 0x04, 0, 1, 0);
    }

    TEST_ASSERT_EQUAL_INT(MODBUS_LOG_SIZE, modbus_log_count(&log_buf));

    /* First entry */
    TEST_ASSERT_EQUAL_INT(0, (int)modbus_log_get(&log_buf, 0)->timestamp_ms);
    /* Last entry */
    TEST_ASSERT_EQUAL_INT((MODBUS_LOG_SIZE - 1) * 10,
                          (int)modbus_log_get(&log_buf, MODBUS_LOG_SIZE - 1)->timestamp_ms);
}

/* ---- Clear ---- */

/*
 * @feature Modbus Frame Logging
 * @req REQ-MTR-095
 * @scenario Clear resets count but preserves total_logged
 * @given A ring buffer with 10 entries
 * @when modbus_log_clear is called
 * @then count=0, total_logged=10, get(0) returns NULL
 */
void test_log_clear(void) {
    modbus_log_init(&log_buf);
    for (int i = 0; i < 10; i++) {
        modbus_log_append(&log_buf, (uint32_t)i, MODBUS_DIR_TX, 1, 0x04, 0, 1, 0);
    }

    modbus_log_clear(&log_buf);

    TEST_ASSERT_EQUAL_INT(0, modbus_log_count(&log_buf));
    TEST_ASSERT_EQUAL_INT(10, (int)modbus_log_total(&log_buf));
    TEST_ASSERT_TRUE(modbus_log_get(&log_buf, 0) == NULL);
}

/* ---- Clear then refill ---- */

/*
 * @feature Modbus Frame Logging
 * @req REQ-MTR-096
 * @scenario Clear then refill works correctly
 * @given A ring buffer that was filled, cleared, then has 3 new entries
 * @when Entries are read
 * @then count=3, entries reflect only the new data, total includes both batches
 */
void test_log_clear_then_refill(void) {
    modbus_log_init(&log_buf);
    for (int i = 0; i < 5; i++) {
        modbus_log_append(&log_buf, (uint32_t)(i * 100), MODBUS_DIR_TX, 1, 0x04, 0, 1, 0);
    }
    modbus_log_clear(&log_buf);

    /* Refill with 3 entries */
    modbus_log_append(&log_buf, 9000, MODBUS_DIR_TX, 0x0A, 0x04, 0x06, 6, 0);
    modbus_log_append(&log_buf, 9100, MODBUS_DIR_RX, 0x0A, 0x04, 0x06, 6, 3);
    modbus_log_append(&log_buf, 9200, MODBUS_DIR_ERR, 0x0A, 0x04, 0x06, 6, 0xE4);

    TEST_ASSERT_EQUAL_INT(3, modbus_log_count(&log_buf));
    TEST_ASSERT_EQUAL_INT(8, (int)modbus_log_total(&log_buf)); /* 5 + 3 */
    TEST_ASSERT_EQUAL_INT(9000, (int)modbus_log_get(&log_buf, 0)->timestamp_ms);
    TEST_ASSERT_EQUAL_INT(MODBUS_DIR_ERR, modbus_log_get(&log_buf, 2)->direction);
}

/* ---- Out-of-range index ---- */

/*
 * @feature Modbus Frame Logging
 * @req REQ-MTR-097
 * @scenario Out-of-range index returns NULL
 * @given A ring buffer with 3 entries
 * @when get is called with index=3 and index=MODBUS_LOG_SIZE
 * @then Both return NULL
 */
void test_log_out_of_range(void) {
    modbus_log_init(&log_buf);
    for (int i = 0; i < 3; i++) {
        modbus_log_append(&log_buf, (uint32_t)i, MODBUS_DIR_TX, 1, 0x04, 0, 1, 0);
    }

    TEST_ASSERT_TRUE(modbus_log_get(&log_buf, 3) == NULL);
    TEST_ASSERT_TRUE(modbus_log_get(&log_buf, MODBUS_LOG_SIZE) == NULL);
}

/* ---- NULL safety ---- */

/*
 * @feature Modbus Frame Logging
 * @req REQ-MTR-098
 * @scenario All functions handle NULL pointer without crashing
 * @given NULL modbus_log_t pointer
 * @when All API functions are called with NULL
 * @then No crash, count returns 0, get returns NULL, total returns 0
 */
void test_log_null_safety(void) {
    modbus_log_init(NULL);
    modbus_log_append(NULL, 0, 0, 0, 0, 0, 0, 0);
    modbus_log_clear(NULL);

    TEST_ASSERT_EQUAL_INT(0, modbus_log_count(NULL));
    TEST_ASSERT_EQUAL_INT(0, (int)modbus_log_total(NULL));
    TEST_ASSERT_TRUE(modbus_log_get(NULL, 0) == NULL);
}

/* ---- Direction field values ---- */

/*
 * @feature Modbus Frame Logging
 * @req REQ-MTR-099
 * @scenario TX, RX, and ERR direction values are stored correctly
 * @given An initialized ring buffer
 * @when Three entries with different directions are appended
 * @then Each entry reflects its correct direction
 */
void test_log_directions(void) {
    modbus_log_init(&log_buf);
    modbus_log_append(&log_buf, 100, MODBUS_DIR_TX,  0x0A, 0x04, 0x06, 6, 0);
    modbus_log_append(&log_buf, 200, MODBUS_DIR_RX,  0x0A, 0x04, 0x06, 6, 3);
    modbus_log_append(&log_buf, 300, MODBUS_DIR_ERR, 0x0A, 0x04, 0x06, 6, 0xE4);

    TEST_ASSERT_EQUAL_INT(MODBUS_DIR_TX,  modbus_log_get(&log_buf, 0)->direction);
    TEST_ASSERT_EQUAL_INT(MODBUS_DIR_RX,  modbus_log_get(&log_buf, 1)->direction);
    TEST_ASSERT_EQUAL_INT(MODBUS_DIR_ERR, modbus_log_get(&log_buf, 2)->direction);
    TEST_ASSERT_EQUAL_INT(0xE4, modbus_log_get(&log_buf, 2)->result);
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("Modbus Frame Logging");

    RUN_TEST(test_log_init);
    RUN_TEST(test_log_single_entry);
    RUN_TEST(test_log_order);
    RUN_TEST(test_log_wraparound);
    RUN_TEST(test_log_exact_fill);
    RUN_TEST(test_log_clear);
    RUN_TEST(test_log_clear_then_refill);
    RUN_TEST(test_log_out_of_range);
    RUN_TEST(test_log_null_safety);
    RUN_TEST(test_log_directions);

    TEST_SUITE_RESULTS();
}
