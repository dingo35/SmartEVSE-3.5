/*
 * session_log.h — Charge session tracking for ERE certificate reporting
 *
 * Pure C module — no platform dependencies. Tracks charge sessions with
 * start/end timestamps and energy readings for Dutch ERE (Emissie Reductie
 * Eenheden) certificate reporting.
 *
 * Architecture: MQTT-only persistence. A single "last session" record is
 * kept in RAM (32 bytes). On session end, the firmware publishes the record
 * via MQTT (retained) for Home Assistant or other backends to persist.
 */

#ifndef SESSION_LOG_H
#define SESSION_LOG_H

#include <stdint.h>
#include <stddef.h>

#define SESSION_MIN_VALID_TIME 1704067200  /* 2024-01-01T00:00:00Z */
#define SESSION_MIN_DURATION_S 60  /* Ignore sessions shorter than 60 seconds */

#ifdef __cplusplus
extern "C" {
#endif

/* Session record — 32 bytes, matches ERE-required fields */
typedef struct {
    uint32_t session_id;           /* Incrementing ID (or OCPP transaction ID) */
    uint32_t start_time;           /* UTC epoch seconds */
    uint32_t end_time;             /* UTC epoch seconds */
    int32_t  start_energy_wh;      /* EVMeter.Import_active_energy at start */
    int32_t  end_energy_wh;        /* EVMeter.Import_active_energy at end */
    int32_t  energy_charged_wh;    /* end - start */
    uint16_t max_current_da;       /* Peak current during session (deciamps) */
    uint8_t  phases;               /* Nr_Of_Phases_Charging at session end */
    uint8_t  mode;                 /* MODE_NORMAL / MODE_SMART / MODE_SOLAR */
    uint8_t  ocpp_active;          /* Was OCPP controlling this session? */
    uint8_t  _reserved[3];         /* Alignment padding */
} session_record_t;

/* Initialize session logger state. Call once at startup. */
void session_init(void);

/* Start a new charge session. If a session is already active, it is discarded. */
void session_start(uint32_t timestamp, int32_t start_energy_wh, uint8_t mode);

/* End the current charge session. No-op if no session is active. */
void session_end(uint32_t timestamp, int32_t end_energy_wh,
                 uint16_t max_current_da, uint8_t phases);

/* Set OCPP transaction ID on the active session. No-op if no session active. */
void session_set_ocpp_id(uint32_t ocpp_transaction_id);

/* Returns 1 if a session is currently active, 0 otherwise. */
uint8_t session_is_active(void);

/* Get the last completed session record. Returns NULL if no session completed yet. */
const session_record_t *session_get_last(void);

/*
 * Format a session record as JSON into buf.
 * Returns the number of bytes written (excluding NUL), or -1 on error
 * (NULL record, NULL buf, or bufsz == 0).
 *
 * Output includes ISO 8601 timestamps and kWh value for ERE compatibility.
 */
int session_to_json(const session_record_t *rec, char *buf, size_t bufsz);

#ifdef __cplusplus
}
#endif

#endif /* SESSION_LOG_H */
