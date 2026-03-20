/*
 * diag_storage.cpp - Diagnostic telemetry LittleFS persistence
 *
 * Dumps ring buffer contents to LittleFS with 4-file retention.
 * Provides auto-dump on error transitions.
 */

#if defined(SMARTEVSE_VERSION)  /* ESP32 firmware only */

#include "diag_storage.h"
#include "diag_sampler.h"
#include "main.h"
#include "esp32.h"
#include "evse_bridge.h"
#include "meter.h"
#include "debug.h"

#include <LittleFS.h>
#include <string.h>
#include <stdio.h>

/* Extern globals for extended data */
extern uint16_t Balanced[];
extern uint16_t BalancedMax[];
extern uint8_t  BalancedState[];
extern uint16_t BalancedError[];
extern uint16_t MaxSumMains;
extern uint8_t  State;
extern uint8_t  ErrorFlags;
extern uint16_t SolarStopTimer;

/* Node struct from main.h */
extern struct Node_t Node[];

extern uint32_t serialnr;

static bool auto_dump_enabled = true;
static uint8_t prev_state_for_trigger = 0;

/* ---- Helpers ---- */

/* Count files in /diag directory */
static uint8_t count_diag_files(void)
{
    File root = LittleFS.open(DIAG_DIR);
    if (!root || !root.isDirectory())
        return 0;

    uint8_t count = 0;
    File f = root.openNextFile();
    while (f) {
        if (!f.isDirectory())
            count++;
        f = root.openNextFile();
    }
    return count;
}

/* Find and delete the oldest file in /diag (by name, which encodes uptime) */
static void delete_oldest_diag_file(void)
{
    File root = LittleFS.open(DIAG_DIR);
    if (!root || !root.isDirectory())
        return;

    char oldest_name[48] = {0};
    uint32_t oldest_uptime = UINT32_MAX;

    File f = root.openNextFile();
    while (f) {
        if (!f.isDirectory()) {
            const char *name = f.name();
            /* Parse uptime from filename: capture_NNNNNNNN.diag */
            uint32_t uptime = 0;
            if (sscanf(name, "capture_%u", &uptime) == 1) {
                if (uptime < oldest_uptime) {
                    oldest_uptime = uptime;
                    snprintf(oldest_name, sizeof(oldest_name),
                             "%s/%s", DIAG_DIR, name);
                }
            }
        }
        f = root.openNextFile();
    }

    if (oldest_name[0]) {
        LittleFS.remove(oldest_name);
        _LOG_D("Diag: deleted oldest dump %s\n", oldest_name);
    }
}

/* Capture extended data from globals */
static void capture_extended(diag_extended_t *ext)
{
    memset(ext, 0, sizeof(*ext));

    for (int i = 0; i < 8; i++) {
        ext->balanced[i]       = Balanced[i];
        ext->balanced_max[i]   = BalancedMax[i];
        ext->balanced_state[i] = BalancedState[i];
        ext->balanced_error[i] = BalancedError[i];
        ext->node_online[i]    = Node[i].Online;
        ext->node_phases[i]    = Node[i].Phases;
        ext->node_solar_timer[i] = Node[i].SolarTimer;
    }
    ext->max_sum_mains = MaxSumMains;

    /* Count active EVSEs */
    uint8_t active = 0;
    for (int i = 0; i < 8; i++) {
        if (BalancedState[i] == 2 || BalancedState[i] == 6 ||
            BalancedState[i] == 7)  /* STATE_C, COMM_C, COMM_C_OK */
            active++;
    }
    ext->active_evse_count = active;
}

/* ---- Public API ---- */

void diag_storage_init(void)
{
    if (!LittleFS.exists(DIAG_DIR)) {
        LittleFS.mkdir(DIAG_DIR);
        _LOG_D("Diag: created %s directory\n", DIAG_DIR);
    }
    prev_state_for_trigger = State;
}

bool diag_storage_dump(uint8_t trigger_reason)
{
    diag_ring_t *ring = diag_get_ring();
    if (!ring || ring->count == 0) {
        _LOG_D("Diag: nothing to dump (empty ring)\n");
        return false;
    }

    /* Enforce retention limit */
    while (count_diag_files() >= DIAG_MAX_FILES) {
        delete_oldest_diag_file();
    }

    /* Freeze ring for consistent read */
    bool was_frozen = ring->frozen;
    diag_ring_freeze(ring, true);

    /* Build filename from uptime */
    char filepath[48];
    snprintf(filepath, sizeof(filepath), "%s/capture_%08u.diag",
             DIAG_DIR, (unsigned)ring->start_time);

    /* Serialize ring buffer to binary */
    size_t max_sz = sizeof(diag_file_header_t)
                  + (size_t)ring->count * sizeof(diag_snapshot_t)
                  + sizeof(diag_extended_t) + 2  /* extended_size field */
                  + sizeof(uint32_t);            /* CRC */
    uint8_t *buf = (uint8_t *)malloc(max_sz);
    if (!buf) {
        if (!was_frozen) diag_ring_freeze(ring, false);
        _LOG_A("Diag: dump malloc failed (%u bytes)\n", (unsigned)max_sz);
        return false;
    }

    size_t n = diag_ring_serialize(ring, buf, max_sz, VERSION, serialnr);
    if (!was_frozen) diag_ring_freeze(ring, false);

    if (n == 0) {
        free(buf);
        _LOG_A("Diag: serialize failed\n");
        return false;
    }

    /* Write to LittleFS */
    File file = LittleFS.open(filepath, "w");
    if (!file) {
        free(buf);
        _LOG_A("Diag: failed to open %s for writing\n", filepath);
        return false;
    }

    size_t written = file.write(buf, n);
    file.close();
    free(buf);

    if (written != n) {
        _LOG_A("Diag: write incomplete (%u/%u)\n", (unsigned)written, (unsigned)n);
        LittleFS.remove(filepath);
        return false;
    }

    _LOG_I("Diag: dumped %u snapshots to %s (trigger=0x%02X, %u bytes)\n",
           (unsigned)ring->count, filepath, trigger_reason, (unsigned)n);
    return true;
}

uint8_t diag_storage_list(char filenames[][32], uint8_t max_files)
{
    File root = LittleFS.open(DIAG_DIR);
    if (!root || !root.isDirectory())
        return 0;

    uint8_t count = 0;
    File f = root.openNextFile();
    while (f && count < max_files) {
        if (!f.isDirectory()) {
            snprintf(filenames[count], 32, "%s", f.name());
            count++;
        }
        f = root.openNextFile();
    }
    return count;
}

bool diag_storage_delete(const char *filename)
{
    if (!filename)
        return false;

    char filepath[48];
    /* If filename already has path prefix, use as-is; otherwise prepend dir */
    if (filename[0] == '/')
        snprintf(filepath, sizeof(filepath), "%s", filename);
    else
        snprintf(filepath, sizeof(filepath), "%s/%s", DIAG_DIR, filename);

    if (!LittleFS.exists(filepath))
        return false;

    bool ok = LittleFS.remove(filepath);
    if (ok)
        _LOG_D("Diag: deleted %s\n", filepath);
    return ok;
}

void diag_storage_check_triggers(uint8_t old_error, uint8_t old_state,
                                  uint16_t old_solar_timer)
{
    if (!auto_dump_enabled)
        return;

    diag_ring_t *ring = diag_get_ring();
    if (!ring || ring->profile == DIAG_PROFILE_OFF || ring->count == 0)
        return;

    uint8_t trigger = DIAG_TRIGGER_NONE;

    /* ErrorFlags transitions from NO_ERROR to any error */
    if (old_error == 0 && ErrorFlags != 0)
        trigger |= DIAG_TRIGGER_ERROR_ONSET;

    /* Unexpected STATE_C → STATE_A (vehicle disconnect during charging) */
    if (old_state == 2 && State == 0)  /* STATE_C=2, STATE_A=0 */
        trigger |= DIAG_TRIGGER_UNEXPECTED_A;

    /* SolarStopTimer reached maximum (solar oscillation detected)
     * Timer counts up from 0; if it jumped significantly, trigger */
    if (SolarStopTimer > 0 && old_solar_timer == 0 && SolarStopTimer >= 60)
        trigger |= DIAG_TRIGGER_SOLAR_MAX;

    /* MainsMeter.Timeout exceeds threshold */
    if (MainsMeter.Timeout >= 10 && old_error == 0)
        trigger |= DIAG_TRIGGER_METER_TIMEOUT;

    if (trigger != DIAG_TRIGGER_NONE) {
        _LOG_I("Diag: auto-dump triggered (reason=0x%02X)\n", trigger);
        diag_storage_dump(trigger);
    }

    prev_state_for_trigger = State;
}

void diag_storage_set_auto_dump(bool enabled)
{
    auto_dump_enabled = enabled;
}

bool diag_storage_get_auto_dump(void)
{
    return auto_dump_enabled;
}

int diag_storage_list_json(char *buf, size_t bufsz)
{
    if (!buf || bufsz == 0)
        return -1;

    char files[DIAG_MAX_FILES][32];
    uint8_t nfiles = diag_storage_list(files, DIAG_MAX_FILES);

    /* Build JSON array of filenames */
    int pos = 0;
    int n = snprintf(buf + pos, bufsz - pos, "{\"files\":[");
    if (n < 0 || (size_t)n >= bufsz - pos) return -1;
    pos += n;

    for (uint8_t i = 0; i < nfiles; i++) {
        /* Add comma separator between entries */
        if (i > 0) {
            if ((size_t)pos + 1 >= bufsz) return -1;
            buf[pos++] = ',';
        }
        /* Open quote */
        if ((size_t)pos + 1 >= bufsz) return -1;
        buf[pos++] = '"';
        /* Escape filename characters for safe JSON */
        for (const char *p = files[i]; *p != '\0'; p++) {
            if (*p == '"' || *p == '\\') {
                if ((size_t)pos + 2 >= bufsz) return -1;
                buf[pos++] = '\\';
                buf[pos++] = *p;
            } else if ((unsigned char)*p < 0x20) {
                /* Skip control characters */
                continue;
            } else {
                if ((size_t)pos + 1 >= bufsz) return -1;
                buf[pos++] = *p;
            }
        }
        /* Close quote */
        if ((size_t)pos + 1 >= bufsz) return -1;
        buf[pos++] = '"';
    }

    n = snprintf(buf + pos, bufsz - pos, "],\"count\":%u,\"auto_dump\":%s}",
                 (unsigned)nfiles, auto_dump_enabled ? "true" : "false");
    if (n < 0 || (size_t)n >= bufsz - pos) return -1;
    pos += n;

    return pos;
}

#endif /* SMARTEVSE_VERSION */
