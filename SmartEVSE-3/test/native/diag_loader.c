/*
 * diag_loader.c - Load .diag binary files for test replay
 *
 * Pure C implementation. Compiles natively with gcc.
 */

#include "diag_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool diag_load_buffer(const uint8_t *data, size_t len, diag_capture_t *cap)
{
    if (!data || !cap || len < sizeof(diag_file_header_t))
        return false;

    memset(cap, 0, sizeof(*cap));

    /* Parse header */
    memcpy(&cap->header, data, sizeof(diag_file_header_t));

    if (memcmp(cap->header.magic, DIAG_FILE_MAGIC, DIAG_FILE_MAGIC_LEN) != 0)
        return false;

    if (cap->header.version != DIAG_FILE_VERSION)
        return false;

    if (cap->header.snapshot_size != sizeof(diag_snapshot_t))
        return false;

    uint16_t count = cap->header.count;
    if (count > DIAG_LOADER_MAX_SNAPSHOTS)
        count = DIAG_LOADER_MAX_SNAPSHOTS;

    size_t snap_offset = sizeof(diag_file_header_t);
    size_t snap_bytes  = (size_t)count * sizeof(diag_snapshot_t);
    size_t needed      = snap_offset + snap_bytes + sizeof(uint32_t);

    if (len < needed) {
        /* Not enough data, load what we can */
        size_t avail = len - snap_offset;
        count = (uint16_t)(avail / sizeof(diag_snapshot_t));
        snap_bytes = (size_t)count * sizeof(diag_snapshot_t);
    }

    if (count > 0)
        memcpy(cap->snapshots, data + snap_offset, snap_bytes);

    cap->count = count;

    /* Verify CRC if present */
    size_t payload_len = snap_offset + snap_bytes;
    if (len >= payload_len + sizeof(uint32_t)) {
        uint32_t stored_crc;
        memcpy(&stored_crc, data + payload_len, sizeof(uint32_t));
        uint32_t computed_crc = diag_crc32(data, payload_len);
        cap->crc_valid = (stored_crc == computed_crc);
    } else {
        cap->crc_valid = false;
    }

    cap->loaded = true;
    return true;
}

bool diag_load_file(const char *filepath, diag_capture_t *cap)
{
    if (!filepath || !cap)
        return false;

    memset(cap, 0, sizeof(*cap));

    FILE *f = fopen(filepath, "rb");
    if (!f)
        return false;

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || (size_t)fsize > 1024 * 1024) {
        fclose(f);
        return false;  /* Sanity limit: 1 MB */
    }

    uint8_t *buf = (uint8_t *)malloc((size_t)fsize);
    if (!buf) {
        fclose(f);
        return false;
    }

    size_t nread = fread(buf, 1, (size_t)fsize, f);
    fclose(f);

    bool ok = diag_load_buffer(buf, nread, cap);
    free(buf);
    return ok;
}

bool diag_create_synthetic(diag_capture_t *cap, uint16_t count,
                           uint32_t start_time, diag_profile_t profile)
{
    if (!cap || count == 0 || count > DIAG_LOADER_MAX_SNAPSHOTS)
        return false;

    memset(cap, 0, sizeof(*cap));

    /* Fill header */
    memcpy(cap->header.magic, DIAG_FILE_MAGIC, DIAG_FILE_MAGIC_LEN);
    cap->header.version      = DIAG_FILE_VERSION;
    cap->header.profile      = (uint8_t)profile;
    cap->header.snapshot_size = sizeof(diag_snapshot_t);
    cap->header.count         = count;
    cap->header.start_uptime  = start_time;
    snprintf(cap->header.firmware_version, sizeof(cap->header.firmware_version),
             "test_synth");

    /* Generate snapshots with incrementing timestamps */
    for (uint16_t i = 0; i < count; i++) {
        diag_snapshot_t *s = &cap->snapshots[i];
        memset(s, 0, sizeof(*s));
        s->timestamp = start_time + i;
        s->state     = 0;  /* STATE_A */
        s->mode      = (uint8_t)profile;
    }

    cap->count     = count;
    cap->crc_valid = true;
    cap->loaded    = true;
    return true;
}
