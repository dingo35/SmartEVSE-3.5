/*
 * serial_parser.h - Pure C serial message parsing for CH32<->ESP32 communication
 *
 * Parses structured serial messages (Irms, PowerMeasured) and modbus node
 * status buffers into typed structs. No platform dependencies.
 */

#ifndef SERIAL_PARSER_H
#define SERIAL_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Parsed Irms message result */
typedef struct {
    uint16_t address;       /* Meter address (e.g. 011) */
    int16_t irms[3];        /* Current per phase in 0.1A (L1, L2, L3) */
} serial_irms_t;

/* Parsed PowerMeasured message result */
typedef struct {
    uint16_t address;       /* Meter address */
    int16_t power;          /* Power in watts */
} serial_power_t;

/* Parsed node status message (from receiveNodeStatus buf[]) */
typedef struct {
    uint8_t state;          /* buf[1] - Node State */
    uint8_t error;          /* buf[3] - Error status */
    uint8_t mode;           /* buf[7] - Node Mode */
    uint16_t solar_timer;   /* (buf[8]<<8) | buf[9] */
    uint8_t config_changed; /* buf[13] */
    uint16_t max_current;   /* buf[15] * 10 (in 0.1A) */
} serial_node_status_t;

/* Input for Isum calculation */
typedef struct {
    int16_t mains_irms[3];      /* Raw mains current per phase in 0.1A */
    int16_t battery_current;    /* From getBatteryCurrent() in 0.1A */
    uint8_t enable_c2;          /* EnableC2 setting */
} calc_isum_input_t;

/* Output from Isum calculation */
typedef struct {
    int16_t adjusted_irms[3];   /* Battery-adjusted mains current */
    int32_t isum;               /* Sum of all phases */
} calc_isum_result_t;

/*
 * Parse "Irms:XXX,YYY,ZZZ,WWW" from a serial buffer.
 * Returns true if found and parsed successfully.
 */
bool serial_parse_irms(const char *buf, serial_irms_t *out);

/*
 * Parse "PowerMeasured:XXX,YYY" from a serial buffer.
 * Returns true if found and parsed successfully.
 */
bool serial_parse_power(const char *buf, serial_power_t *out);

/*
 * Parse a modbus node status response buffer into structured data.
 * buf must be at least 16 bytes. Returns true on success.
 */
bool serial_parse_node_status(const uint8_t *buf, uint8_t buf_len,
                               serial_node_status_t *out);

/*
 * Pure Isum calculation: adjusts mains currents for battery and sums phases.
 * enable_c2 values: 0=NOT_PRESENT, 1=ALWAYS_OFF, 2=SOLAR_OFF, 3=ALWAYS_ON, 4=AUTO
 * When enable_c2 == ALWAYS_OFF (1), full battery current applied to L1 only.
 * Otherwise battery current is distributed equally across all 3 phases.
 */
calc_isum_result_t calc_isum(const calc_isum_input_t *input);

/*
 * Pure battery current logic: returns battery current if conditions met.
 * Returns 0 if data is stale (>60s), mode is not solar, or meter is not API.
 *
 * time_since_update: seconds since last homeBatteryLastUpdate (0 = never updated)
 * mode: current operating mode (MODE_SOLAR = 2)
 * mains_meter_type: MainsMeter.Type value (EM_API = 9)
 * battery_current: raw homeBatteryCurrent value in 0.1A
 */
int16_t calc_battery_current(uint32_t time_since_update, uint8_t mode,
                              uint8_t mains_meter_type, int16_t battery_current);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_PARSER_H */
