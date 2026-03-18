/*
 * test_mqtt_publish.c — Native tests for MQTT change-only publish cache
 *
 * Tests the pure C cache logic in mqtt_publish.c: change detection,
 * heartbeat re-publish, forced publish, CRC16, and slot bounds.
 */

#include "test_framework.h"
#include "mqtt_publish.h"
#include <string.h>

static mqtt_cache_t cache;

/* ---- Integer change detection ---- */

/*
 * @feature MQTT Change-Only Publishing
 * @req REQ-MQTT-020
 * @scenario First integer publish always goes through
 * @given An empty cache with heartbeat 60s
 * @when An integer value is checked for slot MQTT_SLOT_ESP_TEMP
 * @then mqtt_should_publish_int returns true (first time)
 */
void test_int_first_publish(void) {
    mqtt_cache_init(&cache, 60);
    TEST_ASSERT_TRUE(mqtt_should_publish_int(&cache, MQTT_SLOT_ESP_TEMP, 42, 100));
}

/*
 * @feature MQTT Change-Only Publishing
 * @req REQ-MQTT-020
 * @scenario Unchanged integer is suppressed
 * @given A cache with MQTT_SLOT_ESP_TEMP previously published as 42
 * @when The same value 42 is checked at now_s=110 (before heartbeat)
 * @then mqtt_should_publish_int returns false
 */
void test_int_unchanged_suppressed(void) {
    mqtt_cache_init(&cache, 60);
    mqtt_should_publish_int(&cache, MQTT_SLOT_ESP_TEMP, 42, 100);
    TEST_ASSERT_FALSE(mqtt_should_publish_int(&cache, MQTT_SLOT_ESP_TEMP, 42, 110));
}

/*
 * @feature MQTT Change-Only Publishing
 * @req REQ-MQTT-020
 * @scenario Changed integer value triggers publish
 * @given A cache with MQTT_SLOT_ESP_TEMP previously published as 42
 * @when A different value 43 is checked
 * @then mqtt_should_publish_int returns true
 */
void test_int_changed_publishes(void) {
    mqtt_cache_init(&cache, 60);
    mqtt_should_publish_int(&cache, MQTT_SLOT_ESP_TEMP, 42, 100);
    TEST_ASSERT_TRUE(mqtt_should_publish_int(&cache, MQTT_SLOT_ESP_TEMP, 43, 110));
}

/*
 * @feature MQTT Change-Only Publishing
 * @req REQ-MQTT-021
 * @scenario Heartbeat forces re-publish of unchanged integer
 * @given A cache with MQTT_SLOT_ESP_TEMP published at t=100 with heartbeat 60s
 * @when The same value is checked at t=160 (heartbeat elapsed)
 * @then mqtt_should_publish_int returns true
 */
void test_int_heartbeat_republish(void) {
    mqtt_cache_init(&cache, 60);
    mqtt_should_publish_int(&cache, MQTT_SLOT_ESP_TEMP, 42, 100);
    TEST_ASSERT_FALSE(mqtt_should_publish_int(&cache, MQTT_SLOT_ESP_TEMP, 42, 159));
    TEST_ASSERT_TRUE(mqtt_should_publish_int(&cache, MQTT_SLOT_ESP_TEMP, 42, 160));
}

/* ---- String change detection ---- */

/*
 * @feature MQTT Change-Only Publishing
 * @req REQ-MQTT-020
 * @scenario First string publish always goes through
 * @given An empty cache with heartbeat 60s
 * @when A string value "Normal" is checked for slot MQTT_SLOT_MODE
 * @then mqtt_should_publish_str returns true
 */
void test_str_first_publish(void) {
    mqtt_cache_init(&cache, 60);
    TEST_ASSERT_TRUE(mqtt_should_publish_str(&cache, MQTT_SLOT_MODE, "Normal", 100));
}

/*
 * @feature MQTT Change-Only Publishing
 * @req REQ-MQTT-020
 * @scenario Changed string value triggers publish
 * @given A cache with MQTT_SLOT_MODE previously published as "Normal"
 * @when A different string "Solar" is checked
 * @then mqtt_should_publish_str returns true
 */
void test_str_changed_publishes(void) {
    mqtt_cache_init(&cache, 60);
    mqtt_should_publish_str(&cache, MQTT_SLOT_MODE, "Normal", 100);
    TEST_ASSERT_FALSE(mqtt_should_publish_str(&cache, MQTT_SLOT_MODE, "Normal", 110));
    TEST_ASSERT_TRUE(mqtt_should_publish_str(&cache, MQTT_SLOT_MODE, "Solar", 110));
}

/* ---- Forced publish ---- */

/*
 * @feature MQTT Change-Only Publishing
 * @req REQ-MQTT-022
 * @scenario mqtt_cache_force_all marks all entries stale
 * @given A cache with MQTT_SLOT_ESP_TEMP published (unchanged value)
 * @when mqtt_cache_force_all is called then the same value is checked
 * @then mqtt_should_publish_int returns true (forced)
 */
void test_force_all_triggers_publish(void) {
    mqtt_cache_init(&cache, 60);
    mqtt_should_publish_int(&cache, MQTT_SLOT_ESP_TEMP, 42, 100);
    TEST_ASSERT_FALSE(mqtt_should_publish_int(&cache, MQTT_SLOT_ESP_TEMP, 42, 105));
    mqtt_cache_force_all(&cache);
    TEST_ASSERT_TRUE(mqtt_should_publish_int(&cache, MQTT_SLOT_ESP_TEMP, 42, 105));
}

/* ---- CRC16 ---- */

/*
 * @feature MQTT Change-Only Publishing
 * @req REQ-MQTT-020
 * @scenario CRC16 produces consistent non-zero hashes
 * @given Known string inputs
 * @when mqtt_crc16 is called
 * @then Different strings produce different hashes and same strings produce same hash
 */
void test_crc16_consistency(void) {
    uint16_t h1 = mqtt_crc16("Normal", 6);
    uint16_t h2 = mqtt_crc16("Normal", 6);
    uint16_t h3 = mqtt_crc16("Solar", 5);
    TEST_ASSERT_EQUAL_INT((int)h1, (int)h2);
    TEST_ASSERT_NOT_EQUAL((int)h1, (int)h3);
    /* Empty string should also produce a valid hash */
    uint16_t h4 = mqtt_crc16("", 0);
    (void)h4; /* just ensure no crash */
}

/* ---- Slot bounds ---- */

/*
 * @feature MQTT Change-Only Publishing
 * @req REQ-MQTT-020
 * @scenario Out-of-range slot is rejected
 * @given A valid cache
 * @when mqtt_should_publish_int is called with slot >= MQTT_CACHE_MAX_SLOTS
 * @then Returns false (no crash, no publish)
 */
void test_invalid_slot_rejected(void) {
    mqtt_cache_init(&cache, 60);
    TEST_ASSERT_FALSE(mqtt_should_publish_int(&cache, (mqtt_slot_t)MQTT_CACHE_MAX_SLOTS, 1, 100));
    TEST_ASSERT_FALSE(mqtt_should_publish_str(&cache, (mqtt_slot_t)99, "x", 100));
}

/* ---- Slot count within bounds ---- */

/*
 * @feature MQTT Change-Only Publishing
 * @req REQ-MQTT-020
 * @scenario MQTT_SLOT_COUNT fits within MQTT_CACHE_MAX_SLOTS
 * @given The enum definition
 * @when MQTT_SLOT_COUNT is compared to MQTT_CACHE_MAX_SLOTS
 * @then MQTT_SLOT_COUNT is less than or equal to MQTT_CACHE_MAX_SLOTS
 */
void test_slot_count_within_bounds(void) {
    TEST_ASSERT_TRUE(MQTT_SLOT_COUNT <= MQTT_CACHE_MAX_SLOTS);
}

int main(void) {
    TEST_SUITE_BEGIN("MQTT Publish Cache");

    RUN_TEST(test_int_first_publish);
    RUN_TEST(test_int_unchanged_suppressed);
    RUN_TEST(test_int_changed_publishes);
    RUN_TEST(test_int_heartbeat_republish);
    RUN_TEST(test_str_first_publish);
    RUN_TEST(test_str_changed_publishes);
    RUN_TEST(test_force_all_triggers_publish);
    RUN_TEST(test_crc16_consistency);
    RUN_TEST(test_invalid_slot_rejected);
    RUN_TEST(test_slot_count_within_bounds);

    TEST_SUITE_RESULTS();
}
