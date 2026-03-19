/*
 * diag_loader.h - Load .diag binary files for test replay
 *
 * Pure C loader for the diag binary format.
 * Used by native tests to replay captured diagnostic data.
 */

#ifndef DIAG_LOADER_H
#define DIAG_LOADER_H

#include "diag_telemetry.h"
#include <stdint.h>
#include <stdbool.h>

/* Maximum snapshots that can be loaded (memory-safe for native tests) */
#define DIAG_LOADER_MAX_SNAPSHOTS  256

/* Loaded diagnostic capture */
typedef struct {
    diag_file_header_t header;
    diag_snapshot_t    snapshots[DIAG_LOADER_MAX_SNAPSHOTS];
    uint16_t           count;       /* Actual number of snapshots loaded */
    bool               crc_valid;   /* True if CRC32 matched */
    bool               loaded;      /* True if a file was successfully loaded */
} diag_capture_t;

/* Load a .diag file from disk.
 * Returns true on success, false on error (file not found, corrupt, etc.).
 * On success, cap->loaded is true and cap->count has the snapshot count. */
bool diag_load_file(const char *filepath, diag_capture_t *cap);

/* Load from an in-memory buffer (e.g., for embedded test fixtures).
 * Returns true on success. */
bool diag_load_buffer(const uint8_t *data, size_t len, diag_capture_t *cap);

/* Create a synthetic .diag capture in memory for testing.
 * Fills cap with count snapshots, timestamps starting at start_time.
 * Uses the provided profile. Returns true on success. */
bool diag_create_synthetic(diag_capture_t *cap, uint16_t count,
                           uint32_t start_time, diag_profile_t profile);

#endif /* DIAG_LOADER_H */
