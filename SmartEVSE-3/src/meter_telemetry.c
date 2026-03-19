/*
 * meter_telemetry.c - Per-meter communication error counters
 *
 * Pure C implementation. No platform dependencies.
 */

#include "meter_telemetry.h"
#include <string.h>

void meter_telemetry_init(meter_telemetry_t *tel)
{
    if (!tel) return;
    memset(tel, 0, sizeof(*tel));
}

void meter_telemetry_configure(meter_telemetry_t *tel, uint8_t slot,
                               uint8_t meter_type, uint8_t meter_address)
{
    if (!tel || slot >= METER_TELEMETRY_MAX_METERS) return;
    tel->meters[slot].meter_type = meter_type;
    tel->meters[slot].meter_address = meter_address;
}

/* Saturating increment helper */
static void sat_inc(uint32_t *counter)
{
    if (*counter < UINT32_MAX) {
        (*counter)++;
    }
}

void meter_telemetry_inc_request(meter_telemetry_t *tel, uint8_t slot)
{
    if (!tel || slot >= METER_TELEMETRY_MAX_METERS) return;
    sat_inc(&tel->meters[slot].request_count);
}

void meter_telemetry_inc_response(meter_telemetry_t *tel, uint8_t slot)
{
    if (!tel || slot >= METER_TELEMETRY_MAX_METERS) return;
    sat_inc(&tel->meters[slot].response_count);
}

void meter_telemetry_inc_crc_error(meter_telemetry_t *tel, uint8_t slot)
{
    if (!tel || slot >= METER_TELEMETRY_MAX_METERS) return;
    sat_inc(&tel->meters[slot].crc_error_count);
}

void meter_telemetry_inc_timeout(meter_telemetry_t *tel, uint8_t slot)
{
    if (!tel || slot >= METER_TELEMETRY_MAX_METERS) return;
    sat_inc(&tel->meters[slot].timeout_count);
}

void meter_telemetry_reset_slot(meter_telemetry_t *tel, uint8_t slot)
{
    if (!tel || slot >= METER_TELEMETRY_MAX_METERS) return;
    uint8_t saved_type = tel->meters[slot].meter_type;
    uint8_t saved_addr = tel->meters[slot].meter_address;
    memset(&tel->meters[slot], 0, sizeof(meter_counters_t));
    tel->meters[slot].meter_type = saved_type;
    tel->meters[slot].meter_address = saved_addr;
}

void meter_telemetry_reset_all(meter_telemetry_t *tel)
{
    if (!tel) return;
    for (uint8_t i = 0; i < METER_TELEMETRY_MAX_METERS; i++) {
        meter_telemetry_reset_slot(tel, i);
    }
}

const meter_counters_t *meter_telemetry_get(const meter_telemetry_t *tel,
                                            uint8_t slot)
{
    if (!tel || slot >= METER_TELEMETRY_MAX_METERS) return NULL;
    return &tel->meters[slot];
}

uint8_t meter_telemetry_error_rate(const meter_telemetry_t *tel, uint8_t slot)
{
    if (!tel || slot >= METER_TELEMETRY_MAX_METERS) return 0;
    const meter_counters_t *m = &tel->meters[slot];
    if (m->request_count == 0) return 0;
    uint32_t errors = m->crc_error_count + m->timeout_count;
    /* Cap at 100% */
    if (errors >= m->request_count) return 100;
    return (uint8_t)((errors * 100) / m->request_count);
}
