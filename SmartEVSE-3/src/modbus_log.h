/*
 * modbus_log.h - Modbus frame event ring buffer
 *
 * Pure C ring buffer for logging Modbus frame events (request/response/error).
 * Fixed-size, overwrites oldest entry when full.
 * No platform dependencies — testable natively with gcc.
 */

#ifndef MODBUS_LOG_H
#define MODBUS_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Maximum number of log entries in the ring buffer */
#define MODBUS_LOG_SIZE 32

/* Frame direction */
#define MODBUS_DIR_TX  0   /* Request sent */
#define MODBUS_DIR_RX  1   /* Response received */
#define MODBUS_DIR_ERR 2   /* Error/timeout */

/* Single log entry */
typedef struct {
    uint32_t timestamp_ms;  /* Milliseconds since boot */
    uint8_t  direction;     /* MODBUS_DIR_TX / RX / ERR */
    uint8_t  address;       /* Modbus device address */
    uint8_t  function;      /* Function code */
    uint8_t  result;        /* MODBUS_RESPONSE/REQUEST/EXCEPTION or error code */
    uint16_t reg;           /* Register address */
    uint16_t reg_count;     /* Number of registers */
} modbus_log_entry_t;

/* Ring buffer */
typedef struct {
    modbus_log_entry_t entries[MODBUS_LOG_SIZE];
    uint16_t head;          /* Next write position */
    uint16_t count;         /* Number of valid entries (max MODBUS_LOG_SIZE) */
    uint32_t total_logged;  /* Total entries ever logged (wraps at UINT32_MAX) */
} modbus_log_t;

/* Initialize the ring buffer */
void modbus_log_init(modbus_log_t *log);

/* Append an entry to the ring buffer (overwrites oldest when full) */
void modbus_log_append(modbus_log_t *log, uint32_t timestamp_ms,
                       uint8_t direction, uint8_t address, uint8_t function,
                       uint16_t reg, uint16_t reg_count, uint8_t result);

/* Get number of valid entries */
uint16_t modbus_log_count(const modbus_log_t *log);

/*
 * Read entry by index (0 = oldest valid entry).
 * Returns NULL if index >= count or log is NULL.
 */
const modbus_log_entry_t *modbus_log_get(const modbus_log_t *log, uint16_t index);

/* Clear all entries */
void modbus_log_clear(modbus_log_t *log);

/* Get total number of entries ever logged */
uint32_t modbus_log_total(const modbus_log_t *log);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_LOG_H */
