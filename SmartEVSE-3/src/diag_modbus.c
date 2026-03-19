/*
 * diag_modbus.c - Modbus frame timing event ring buffer
 *
 * Pure C module — no platform dependencies, testable natively.
 */

#include "diag_modbus.h"
#include <string.h>

void diag_mb_init(diag_mb_ring_t *ring)
{
    if (!ring)
        return;
    memset(ring, 0, sizeof(*ring));
    ring->enabled = false;
}

void diag_mb_record(diag_mb_ring_t *ring, uint32_t timestamp_ms,
                    uint8_t address, uint8_t function,
                    uint8_t event_type, uint8_t error_code)
{
    if (!ring || !ring->enabled)
        return;

    diag_mb_event_t *ev = &ring->events[ring->head];
    ev->timestamp_ms = timestamp_ms;
    ev->address      = address;
    ev->function     = function;
    ev->event_type   = event_type;
    ev->error_code   = error_code;

    ring->head = (ring->head + 1) % DIAG_MB_RING_SIZE;
    if (ring->count < DIAG_MB_RING_SIZE)
        ring->count++;
}

uint8_t diag_mb_read(const diag_mb_ring_t *ring, diag_mb_event_t *out,
                     uint8_t max_count)
{
    if (!ring || !out || ring->count == 0)
        return 0;

    uint8_t to_read = ring->count;
    if (to_read > max_count)
        to_read = max_count;

    uint8_t start;
    if (ring->count < DIAG_MB_RING_SIZE)
        start = 0;
    else
        start = ring->head;

    for (uint8_t i = 0; i < to_read; i++) {
        uint8_t idx = (start + i) % DIAG_MB_RING_SIZE;
        out[i] = ring->events[idx];
    }

    return to_read;
}

void diag_mb_reset(diag_mb_ring_t *ring)
{
    if (!ring)
        return;
    ring->head  = 0;
    ring->count = 0;
}

void diag_mb_enable(diag_mb_ring_t *ring, bool enable)
{
    if (!ring)
        return;
    ring->enabled = enable;
}
