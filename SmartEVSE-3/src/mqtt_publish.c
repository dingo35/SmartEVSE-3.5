/*
 * mqtt_publish.c — Change-only MQTT publish cache
 *
 * Pure C module that tracks previously published values per MQTT topic slot.
 * Before publishing, callers check mqtt_should_publish_int/str to determine
 * if the value changed, the heartbeat elapsed, or a forced re-publish is needed.
 *
 * No heap allocation — all state lives in the caller-provided mqtt_cache_t.
 */

#include "mqtt_publish.h"
#include <string.h>

void mqtt_cache_init(mqtt_cache_t *cache, uint16_t heartbeat_s) {
    memset(cache, 0, sizeof(*cache));
    cache->heartbeat_s = heartbeat_s;
}

uint16_t mqtt_crc16(const char *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint8_t)data[i]) << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

bool mqtt_should_publish_int(mqtt_cache_t *cache, mqtt_slot_t slot,
                             int32_t value, uint32_t now_s) {
    if (slot < 0 || slot >= MQTT_CACHE_MAX_SLOTS)
        return false;

    mqtt_cache_entry_t *e = &cache->entries[slot];

    /* Stale (forced) — always publish */
    if (e->flags & MQTT_ENTRY_STALE) {
        e->int_val = value;
        e->flags = MQTT_ENTRY_INT;
        e->last_pub_s = now_s;
        return true;
    }

    /* Empty (first time) — always publish */
    if ((e->flags & 0x0F) == MQTT_ENTRY_EMPTY) {
        e->int_val = value;
        e->flags = MQTT_ENTRY_INT;
        e->last_pub_s = now_s;
        return true;
    }

    /* Value changed */
    if (e->int_val != value) {
        e->int_val = value;
        e->flags = MQTT_ENTRY_INT;
        e->last_pub_s = now_s;
        return true;
    }

    /* Heartbeat elapsed */
    if (cache->heartbeat_s > 0 && (now_s - e->last_pub_s) >= cache->heartbeat_s) {
        e->last_pub_s = now_s;
        return true;
    }

    return false;
}

bool mqtt_should_publish_str(mqtt_cache_t *cache, mqtt_slot_t slot,
                             const char *value, uint32_t now_s) {
    if (slot < 0 || slot >= MQTT_CACHE_MAX_SLOTS)
        return false;

    mqtt_cache_entry_t *e = &cache->entries[slot];
    uint16_t hash = mqtt_crc16(value, strlen(value));

    /* Stale (forced) — always publish */
    if (e->flags & MQTT_ENTRY_STALE) {
        e->str_hash = hash;
        e->flags = MQTT_ENTRY_STR;
        e->last_pub_s = now_s;
        return true;
    }

    /* Empty (first time) — always publish */
    if ((e->flags & 0x0F) == MQTT_ENTRY_EMPTY) {
        e->str_hash = hash;
        e->flags = MQTT_ENTRY_STR;
        e->last_pub_s = now_s;
        return true;
    }

    /* Value changed (CRC16 mismatch) */
    if (e->str_hash != hash) {
        e->str_hash = hash;
        e->flags = MQTT_ENTRY_STR;
        e->last_pub_s = now_s;
        return true;
    }

    /* Heartbeat elapsed */
    if (cache->heartbeat_s > 0 && (now_s - e->last_pub_s) >= cache->heartbeat_s) {
        e->last_pub_s = now_s;
        return true;
    }

    return false;
}

void mqtt_cache_force_all(mqtt_cache_t *cache) {
    for (int i = 0; i < MQTT_CACHE_MAX_SLOTS; i++) {
        cache->entries[i].flags |= MQTT_ENTRY_STALE;
    }
}
