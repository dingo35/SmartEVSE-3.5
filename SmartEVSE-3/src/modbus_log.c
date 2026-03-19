/*
 * modbus_log.c - Modbus frame event ring buffer
 *
 * Pure C implementation. No platform dependencies.
 */

#include "modbus_log.h"
#include <string.h>

void modbus_log_init(modbus_log_t *log)
{
    if (!log) return;
    memset(log, 0, sizeof(*log));
}

void modbus_log_append(modbus_log_t *log, uint32_t timestamp_ms,
                       uint8_t direction, uint8_t address, uint8_t function,
                       uint16_t reg, uint16_t reg_count, uint8_t result)
{
    if (!log) return;

    modbus_log_entry_t *e = &log->entries[log->head];
    e->timestamp_ms = timestamp_ms;
    e->direction = direction;
    e->address = address;
    e->function = function;
    e->reg = reg;
    e->reg_count = reg_count;
    e->result = result;

    log->head = (log->head + 1) % MODBUS_LOG_SIZE;
    if (log->count < MODBUS_LOG_SIZE) {
        log->count++;
    }
    if (log->total_logged < UINT32_MAX) {
        log->total_logged++;
    }
}

uint16_t modbus_log_count(const modbus_log_t *log)
{
    if (!log) return 0;
    return log->count;
}

const modbus_log_entry_t *modbus_log_get(const modbus_log_t *log, uint16_t index)
{
    if (!log || index >= log->count) return NULL;

    /* oldest entry is at (head - count) mod SIZE */
    uint16_t start;
    if (log->count < MODBUS_LOG_SIZE) {
        start = 0;
    } else {
        start = log->head; /* head points to the oldest when full */
    }
    uint16_t actual = (start + index) % MODBUS_LOG_SIZE;
    return &log->entries[actual];
}

void modbus_log_clear(modbus_log_t *log)
{
    if (!log) return;
    log->head = 0;
    log->count = 0;
    /* Preserve total_logged across clears */
}

uint32_t modbus_log_total(const modbus_log_t *log)
{
    if (!log) return 0;
    return log->total_logged;
}
