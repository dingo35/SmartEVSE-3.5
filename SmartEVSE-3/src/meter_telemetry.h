/*
 * meter_telemetry.h - Per-meter communication error counters
 *
 * Pure C module for tracking Modbus communication health per meter.
 * Counters track requests, responses, CRC errors, and timeouts.
 * No platform dependencies — testable natively with gcc.
 */

#ifndef METER_TELEMETRY_H
#define METER_TELEMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Maximum number of meters that can be tracked simultaneously */
#define METER_TELEMETRY_MAX_METERS 4

/* Meter slot indices (logical assignment, not Modbus addresses) */
#define METER_SLOT_MAINS  0
#define METER_SLOT_EV     1
#define METER_SLOT_PV     2
#define METER_SLOT_SPARE  3

/*
 * Per-meter communication counters.
 * All counters are uint32_t to avoid overflow in long-running systems.
 * Counters saturate at UINT32_MAX rather than wrapping.
 */
typedef struct {
    uint32_t request_count;     /* Modbus requests sent to this meter */
    uint32_t response_count;    /* Valid responses received */
    uint32_t crc_error_count;   /* Responses with CRC mismatch */
    uint32_t timeout_count;     /* Requests that timed out (no response) */
    uint8_t  meter_type;        /* EM_* type for identification (0 = unused) */
    uint8_t  meter_address;     /* Modbus address */
} meter_counters_t;

/*
 * Aggregate telemetry for all meters.
 */
typedef struct {
    meter_counters_t meters[METER_TELEMETRY_MAX_METERS];
} meter_telemetry_t;

/* Initialize all counters to zero */
void meter_telemetry_init(meter_telemetry_t *tel);

/* Configure a meter slot with type and address */
void meter_telemetry_configure(meter_telemetry_t *tel, uint8_t slot,
                               uint8_t meter_type, uint8_t meter_address);

/* Increment individual counters (saturating at UINT32_MAX) */
void meter_telemetry_inc_request(meter_telemetry_t *tel, uint8_t slot);
void meter_telemetry_inc_response(meter_telemetry_t *tel, uint8_t slot);
void meter_telemetry_inc_crc_error(meter_telemetry_t *tel, uint8_t slot);
void meter_telemetry_inc_timeout(meter_telemetry_t *tel, uint8_t slot);

/* Reset counters for a single meter slot (preserves type/address) */
void meter_telemetry_reset_slot(meter_telemetry_t *tel, uint8_t slot);

/* Reset all counters for all slots (preserves type/address) */
void meter_telemetry_reset_all(meter_telemetry_t *tel);

/* Read counters for a slot (returns NULL if slot out of range) */
const meter_counters_t *meter_telemetry_get(const meter_telemetry_t *tel,
                                            uint8_t slot);

/* Calculate error rate as percentage (0-100), returns 0 if no requests */
uint8_t meter_telemetry_error_rate(const meter_telemetry_t *tel, uint8_t slot);

#ifdef __cplusplus
}
#endif

#endif /* METER_TELEMETRY_H */
