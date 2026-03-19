/*
 * diag_modbus.h - Modbus frame timing event ring buffer
 *
 * Pure C module — no platform dependencies, testable natively.
 * Captures Modbus request/response/error events with timestamps
 * for diagnosing meter communication issues.
 */

#ifndef DIAG_MODBUS_H
#define DIAG_MODBUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Event types */
#define DIAG_MB_EVENT_SENT      0  /* Request sent to device       */
#define DIAG_MB_EVENT_RECEIVED  1  /* Response received            */
#define DIAG_MB_EVENT_ERROR     2  /* Error/timeout on request     */

/* Modbus event record — 8 bytes per event */
typedef struct __attribute__((packed)) {
    uint32_t timestamp_ms;   /* millis() when event occurred     (4) */
    uint8_t  address;        /* Modbus device address            (1) */
    uint8_t  function;       /* Modbus function code             (1) */
    uint8_t  event_type;     /* DIAG_MB_EVENT_*                  (1) */
    uint8_t  error_code;     /* 0 on success, error code on fail (1) */
} diag_mb_event_t;

/* Ring buffer configuration */
#define DIAG_MB_RING_SIZE  32  /* 32 * 8 = 256 bytes */

/* Ring buffer state */
typedef struct {
    diag_mb_event_t events[DIAG_MB_RING_SIZE];
    uint8_t  head;     /* Next write position       */
    uint8_t  count;    /* Number of valid entries    */
    bool     enabled;  /* Capture enabled            */
} diag_mb_ring_t;

/* Initialize the Modbus event ring buffer. */
void diag_mb_init(diag_mb_ring_t *ring);

/* Record a Modbus event. No-op if ring is NULL or disabled. */
void diag_mb_record(diag_mb_ring_t *ring, uint32_t timestamp_ms,
                    uint8_t address, uint8_t function,
                    uint8_t event_type, uint8_t error_code);

/* Read up to max_count events in chronological order.
 * Returns the number of events actually copied. */
uint8_t diag_mb_read(const diag_mb_ring_t *ring, diag_mb_event_t *out,
                     uint8_t max_count);

/* Reset the ring buffer (clears all events). */
void diag_mb_reset(diag_mb_ring_t *ring);

/* Enable/disable event capture. */
void diag_mb_enable(diag_mb_ring_t *ring, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* DIAG_MODBUS_H */
