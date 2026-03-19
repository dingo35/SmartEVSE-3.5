/*
 * diag_storage.h - Diagnostic telemetry LittleFS persistence
 *
 * Dumps ring buffer snapshots to LittleFS files with a 4-file
 * retention policy. Provides auto-dump triggers on error events.
 */

#ifndef DIAG_STORAGE_H
#define DIAG_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "diag_telemetry.h"
#include <stdint.h>
#include <stdbool.h>

/* Maximum number of stored dump files */
#define DIAG_MAX_FILES  4

/* Directory for diagnostic dumps on LittleFS */
#define DIAG_DIR        "/diag"

/* Extended profile data — captured once per dump, not per-snapshot */
typedef struct __attribute__((packed)) {
    uint16_t balanced[8];           /* Current allocation per node     (16) */
    uint16_t balanced_max[8];       /* Max current per node            (16) */
    uint8_t  balanced_state[8];     /* State per node                  (8)  */
    uint16_t balanced_error[8];     /* Error per node                  (16) */
    uint8_t  node_online[8];        /* Online status per node          (8)  */
    uint8_t  node_phases[8];        /* Phases per node                 (8)  */
    uint16_t node_solar_timer[8];   /* SolarTimer per node             (16) */
    uint16_t max_sum_mains;         /* MaxSumMains setting             (2)  */
    uint8_t  active_evse_count;     /* Number of active EVSEs          (1)  */
    uint8_t  _reserved[3];         /* Padding                          (3)  */
} diag_extended_t;                  /* Total: 94 bytes                      */

/* Auto-dump trigger reasons (bitfield) */
#define DIAG_TRIGGER_NONE          0x00
#define DIAG_TRIGGER_ERROR_ONSET   0x01  /* ErrorFlags 0→nonzero          */
#define DIAG_TRIGGER_UNEXPECTED_A  0x02  /* STATE_C→STATE_A               */
#define DIAG_TRIGGER_SOLAR_MAX     0x04  /* SolarStopTimer reached max    */
#define DIAG_TRIGGER_METER_TIMEOUT 0x08  /* MainsMeter.Timeout exceeded   */
#define DIAG_TRIGGER_MANUAL        0x10  /* REST/MQTT explicit dump       */

/* Initialize storage (create /diag directory if needed). */
void diag_storage_init(void);

/* Dump current ring buffer to a new LittleFS file.
 * Enforces 4-file retention (deletes oldest if at limit).
 * trigger_reason: one of DIAG_TRIGGER_* values.
 * Returns true on success. */
bool diag_storage_dump(uint8_t trigger_reason);

/* List stored dump files. Fills filenames[] with up to max_files names.
 * Returns the number of files found. */
uint8_t diag_storage_list(char filenames[][32], uint8_t max_files);

/* Delete a specific dump file. Returns true on success. */
bool diag_storage_delete(const char *filename);

/* Check auto-dump trigger conditions.
 * Call from timer1s after the state machine tick.
 * old_error/old_state/old_solar_timer: values before the tick. */
void diag_storage_check_triggers(uint8_t old_error, uint8_t old_state,
                                  uint16_t old_solar_timer);

/* Enable/disable auto-dump triggers. Enabled by default. */
void diag_storage_set_auto_dump(bool enabled);

/* Get auto-dump enabled status. */
bool diag_storage_get_auto_dump(void);

/* Format file list as JSON into buf. Returns bytes written or -1. */
int diag_storage_list_json(char *buf, size_t bufsz);

#ifdef __cplusplus
}
#endif

#endif /* DIAG_STORAGE_H */
