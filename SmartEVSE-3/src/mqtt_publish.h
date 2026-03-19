#ifndef MQTT_PUBLISH_H
#define MQTT_PUBLISH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MQTT_CACHE_MAX_SLOTS 88  /* 81 current topics + headroom */

/* One slot per published MQTT topic, in mqttPublishData() call order */
typedef enum {
    MQTT_SLOT_MAINS_L1 = 0,
    MQTT_SLOT_MAINS_L2,
    MQTT_SLOT_MAINS_L3,
    MQTT_SLOT_MAINS_IMPORT_ENERGY,
    MQTT_SLOT_MAINS_EXPORT_ENERGY,
    MQTT_SLOT_MAINS_POWER_L1,
    MQTT_SLOT_MAINS_POWER_L2,
    MQTT_SLOT_MAINS_POWER_L3,
    MQTT_SLOT_MAINS_ENERGY_L1,
    MQTT_SLOT_MAINS_ENERGY_L2,
    MQTT_SLOT_MAINS_ENERGY_L3,
    MQTT_SLOT_EV_L1,
    MQTT_SLOT_EV_L2,
    MQTT_SLOT_EV_L3,
    MQTT_SLOT_EV_IMPORT_ENERGY,
    MQTT_SLOT_EV_EXPORT_ENERGY,
    MQTT_SLOT_EV_POWER_L1,
    MQTT_SLOT_EV_POWER_L2,
    MQTT_SLOT_EV_POWER_L3,
    MQTT_SLOT_EV_ENERGY_L1,
    MQTT_SLOT_EV_ENERGY_L2,
    MQTT_SLOT_EV_ENERGY_L3,
    MQTT_SLOT_ESP_TEMP,
    MQTT_SLOT_MODE,
    MQTT_SLOT_MAX_CURRENT,
    MQTT_SLOT_CUSTOM_BUTTON,
    MQTT_SLOT_CHARGE_CURRENT,
    MQTT_SLOT_CHARGE_CURRENT_OVERRIDE,
    MQTT_SLOT_NR_OF_PHASES,
    MQTT_SLOT_ACCESS,
    MQTT_SLOT_RFID,
    MQTT_SLOT_ENABLE_C2,
    MQTT_SLOT_RFID_LAST_READ,
    MQTT_SLOT_STATE,
    MQTT_SLOT_ERROR,
    MQTT_SLOT_EV_PLUG_STATE,
    MQTT_SLOT_WIFI_SSID,
    MQTT_SLOT_WIFI_BSSID,
    MQTT_SLOT_CPPWM,
    MQTT_SLOT_CPPWM_OVERRIDE,
    MQTT_SLOT_EV_INITIAL_SOC,
    MQTT_SLOT_EV_FULL_SOC,
    MQTT_SLOT_EV_COMPUTED_SOC,
    MQTT_SLOT_EV_REMAINING_SOC,
    MQTT_SLOT_EV_TIME_UNTIL_FULL,
    MQTT_SLOT_EV_ENERGY_CAPACITY,
    MQTT_SLOT_EV_ENERGY_REQUEST,
    MQTT_SLOT_EVCCID,
    MQTT_SLOT_REQUIRED_EVCCID,
    MQTT_SLOT_EV_CHARGE_POWER,
    MQTT_SLOT_EV_ENERGY_CHARGED,
    MQTT_SLOT_EV_TOTAL_ENERGY_CHARGED,
    MQTT_SLOT_HOME_BATTERY_CURRENT,
    MQTT_SLOT_OCPP,
    MQTT_SLOT_OCPP_CONNECTION,
    MQTT_SLOT_LED_COLOR_OFF,
    MQTT_SLOT_LED_COLOR_NORMAL,
    MQTT_SLOT_LED_COLOR_SMART,
    MQTT_SLOT_LED_COLOR_SOLAR,
    MQTT_SLOT_LED_COLOR_CUSTOM,
    MQTT_SLOT_CABLE_LOCK,
    MQTT_SLOT_ESP_UPTIME,
    MQTT_SLOT_WIFI_RSSI,
    MQTT_SLOT_LOAD_BL,
    MQTT_SLOT_PAIRING_PIN,
    MQTT_SLOT_FIRMWARE_VERSION,
    MQTT_SLOT_SOLAR_STOP_TIMER,
    MQTT_SLOT_CURRENT_MAX_SUM_MAINS,
    MQTT_SLOT_PRIO_STRATEGY,
    MQTT_SLOT_ROTATION_INTERVAL,
    MQTT_SLOT_IDLE_TIMEOUT,
    MQTT_SLOT_ROTATION_TIMER,
    MQTT_SLOT_SCHEDULE_STATE,
    MQTT_SLOT_FREE_HEAP,
    MQTT_SLOT_MQTT_MSG_COUNT,
    MQTT_SLOT_OCPP_TX_ACTIVE,
    MQTT_SLOT_OCPP_CURRENT_LIMIT,
    MQTT_SLOT_OCPP_SMART_CHARGING,
    MQTT_SLOT_METER_TIMEOUT_COUNT,
    MQTT_SLOT_METER_RECOVERY_COUNT,
    MQTT_SLOT_API_STALE_COUNT,
    MQTT_SLOT_COUNT  /* must be <= MQTT_CACHE_MAX_SLOTS */
} mqtt_slot_t;

/* Entry flags */
#define MQTT_ENTRY_EMPTY  0x00
#define MQTT_ENTRY_INT    0x01
#define MQTT_ENTRY_STR    0x02
#define MQTT_ENTRY_STALE  0x80  /* OR'd with type — forces next publish */

typedef struct {
    int32_t  int_val;
    uint16_t str_hash;     /* CRC16 of string payload */
    uint32_t last_pub_s;   /* seconds timestamp of last publish */
    uint8_t  flags;        /* EMPTY/INT/STR | STALE */
} mqtt_cache_entry_t;

typedef struct {
    mqtt_cache_entry_t entries[MQTT_CACHE_MAX_SLOTS];
    uint16_t heartbeat_s;  /* re-publish interval for unchanged values */
} mqtt_cache_t;

/* Initialize cache with given heartbeat interval (seconds). */
void mqtt_cache_init(mqtt_cache_t *cache, uint16_t heartbeat_s);

/* Check whether an integer value should be published. Updates cache if yes. */
bool mqtt_should_publish_int(mqtt_cache_t *cache, mqtt_slot_t slot,
                             int32_t value, uint32_t now_s);

/* Check whether a string value should be published. Updates cache if yes. */
bool mqtt_should_publish_str(mqtt_cache_t *cache, mqtt_slot_t slot,
                             const char *value, uint32_t now_s);

/* Mark all entries as stale (forces full re-publish on next cycle). */
void mqtt_cache_force_all(mqtt_cache_t *cache);

/* CRC16-CCITT (polynomial 0x1021) — no lookup table. */
uint16_t mqtt_crc16(const char *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif
