#ifndef MQTT_PARSER_H
#define MQTT_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Parsed MQTT command — the result of parsing a topic+payload
typedef enum {
    MQTT_CMD_NONE = 0,
    MQTT_CMD_MODE,
    MQTT_CMD_CUSTOM_BUTTON,
    MQTT_CMD_CURRENT_OVERRIDE,
    MQTT_CMD_MAX_SUM_MAINS,
    MQTT_CMD_CP_PWM_OVERRIDE,
    MQTT_CMD_MAINS_METER,
    MQTT_CMD_EV_METER,
    MQTT_CMD_HOME_BATTERY_CURRENT,
    MQTT_CMD_REQUIRED_EVCCID,
    MQTT_CMD_COLOR,
    MQTT_CMD_CABLE_LOCK,
    MQTT_CMD_ENABLE_C2,
    MQTT_CMD_PRIO_STRATEGY,
    MQTT_CMD_ROTATION_INTERVAL,
    MQTT_CMD_IDLE_TIMEOUT,
} mqtt_cmd_type_t;

// Mode values matching firmware MODE_NORMAL/MODE_SOLAR/MODE_SMART
#define MQTT_MODE_OFF     0xFF
#define MQTT_MODE_PAUSE   0xFE
#define MQTT_MODE_NORMAL  0
#define MQTT_MODE_SMART   1
#define MQTT_MODE_SOLAR   2

// Color indices
#define MQTT_COLOR_OFF     0
#define MQTT_COLOR_NORMAL  1
#define MQTT_COLOR_SMART   2
#define MQTT_COLOR_SOLAR   3
#define MQTT_COLOR_CUSTOM  4

typedef struct {
    mqtt_cmd_type_t cmd;
    union {
        uint8_t mode;                           // MQTT_CMD_MODE (MQTT_MODE_*)
        bool custom_button;                     // MQTT_CMD_CUSTOM_BUTTON
        uint16_t current_override;              // MQTT_CMD_CURRENT_OVERRIDE
        uint16_t max_sum_mains;                 // MQTT_CMD_MAX_SUM_MAINS
        int16_t cp_pwm;                         // MQTT_CMD_CP_PWM_OVERRIDE
        struct { int32_t L1, L2, L3; } mains_meter;
        struct { int32_t L1, L2, L3; int32_t W; int32_t Wh; } ev_meter;
        int16_t home_battery_current;           // MQTT_CMD_HOME_BATTERY_CURRENT
        char evccid[32];                        // MQTT_CMD_REQUIRED_EVCCID
        struct { uint8_t index; uint8_t r, g, b; } color; // MQTT_CMD_COLOR
        uint8_t cable_lock;                     // MQTT_CMD_CABLE_LOCK
        uint8_t enable_c2;                      // MQTT_CMD_ENABLE_C2
        uint8_t prio_strategy;                  // MQTT_CMD_PRIO_STRATEGY (0-2)
        uint16_t rotation_interval;             // MQTT_CMD_ROTATION_INTERVAL (0, 30-1440)
        uint16_t idle_timeout;                  // MQTT_CMD_IDLE_TIMEOUT (30-300)
    };
} mqtt_command_t;

// EnableC2 string values for backwards-compatible parsing
#define MQTT_ENABLE_C2_COUNT 5
extern const char *mqtt_enable_c2_strings[MQTT_ENABLE_C2_COUNT];

// Parse a topic+payload into a structured command.
// prefix: the MQTT prefix (e.g. "SmartEVSE/123456")
// Returns true if a valid command was parsed, false if unrecognized/invalid.
bool mqtt_parse_command(const char *prefix, const char *topic,
                        const char *payload, mqtt_command_t *out);

// Parse "L1:L2:L3" mains meter format. Returns true on success.
bool mqtt_parse_mains_meter(const char *payload, int32_t *L1, int32_t *L2, int32_t *L3);

// Parse "L1:L2:L3:W:WH" EV meter format. Returns true on success.
bool mqtt_parse_ev_meter(const char *payload, int32_t *L1, int32_t *L2, int32_t *L3,
                         int32_t *W, int32_t *Wh);

// Parse "R,G,B" color format. Returns true on success.
bool mqtt_parse_rgb(const char *payload, uint8_t *r, uint8_t *g, uint8_t *b);

#ifdef __cplusplus
}
#endif

#endif
