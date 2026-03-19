/*
 * diag_telemetry.c - Diagnostic telemetry ring buffer implementation
 *
 * Pure C module — no platform dependencies, testable natively.
 */

#include "diag_telemetry.h"
#include <string.h>

/* ---- Ring buffer API ---- */

void diag_ring_init(diag_ring_t *ring, diag_snapshot_t *buf, uint16_t capacity)
{
    if (!ring)
        return;
    memset(ring, 0, sizeof(*ring));
    ring->buffer   = buf;
    ring->capacity = capacity;
    ring->profile  = DIAG_PROFILE_OFF;
    ring->sample_divider = 1;
}

void diag_ring_reset(diag_ring_t *ring)
{
    if (!ring)
        return;
    ring->head         = 0;
    ring->count        = 0;
    ring->profile      = DIAG_PROFILE_OFF;
    ring->sample_divider = 1;
    ring->tick_counter = 0;
    ring->start_time   = 0;
    ring->frozen       = false;
}

void diag_ring_push(diag_ring_t *ring, const diag_snapshot_t *snap)
{
    if (!ring || !snap || !ring->buffer)
        return;
    if (ring->frozen || ring->profile == DIAG_PROFILE_OFF)
        return;

    ring->buffer[ring->head] = *snap;
    ring->head = (ring->head + 1) % ring->capacity;
    if (ring->count < ring->capacity)
        ring->count++;
}

uint16_t diag_ring_read(const diag_ring_t *ring, diag_snapshot_t *out,
                        uint16_t max_count)
{
    if (!ring || !out || !ring->buffer || ring->count == 0)
        return 0;

    uint16_t to_read = ring->count;
    if (to_read > max_count)
        to_read = max_count;

    /* Calculate the start index (oldest entry) */
    uint16_t start;
    if (ring->count < ring->capacity)
        start = 0;
    else
        start = ring->head;  /* head points to the oldest after wrap */

    for (uint16_t i = 0; i < to_read; i++)
    {
        uint16_t idx = (start + i) % ring->capacity;
        out[i] = ring->buffer[idx];
    }

    return to_read;
}

void diag_set_profile(diag_ring_t *ring, diag_profile_t profile)
{
    if (!ring)
        return;

    ring->profile = profile;
    ring->tick_counter = 0;

    switch (profile)
    {
    case DIAG_PROFILE_MODBUS:
    case DIAG_PROFILE_FAST:
        ring->sample_divider = 1;  /* Every tick (called from 100ms context) */
        break;
    case DIAG_PROFILE_GENERAL:
    case DIAG_PROFILE_SOLAR:
    case DIAG_PROFILE_LOADBAL:
        ring->sample_divider = 1;  /* Every tick (called from 1s context) */
        break;
    case DIAG_PROFILE_OFF:
    default:
        ring->sample_divider = 1;
        break;
    }
}

void diag_ring_freeze(diag_ring_t *ring, bool freeze)
{
    if (!ring)
        return;
    ring->frozen = freeze;
}

bool diag_ring_tick(diag_ring_t *ring)
{
    if (!ring || ring->profile == DIAG_PROFILE_OFF)
        return false;

    ring->tick_counter++;
    if (ring->tick_counter >= ring->sample_divider)
    {
        ring->tick_counter = 0;
        return true;
    }
    return false;
}

/* ---- CRC32 (ISO 3309 / zlib polynomial) ---- */

uint32_t diag_crc32(const uint8_t *data, size_t len)
{
    if (!data || len == 0)
        return 0;

    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

/* ---- Binary serialization ---- */

size_t diag_ring_serialize(const diag_ring_t *ring, uint8_t *buf, size_t bufsz,
                           const char *fw_version, uint32_t serial_nr)
{
    if (!ring || !buf || !ring->buffer)
        return 0;

    size_t header_sz = sizeof(diag_file_header_t);
    size_t snap_sz   = (size_t)ring->count * sizeof(diag_snapshot_t);
    size_t crc_sz    = sizeof(uint32_t);
    size_t total     = header_sz + snap_sz + crc_sz;

    if (bufsz < total)
        return 0;

    /* Build header */
    diag_file_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, DIAG_FILE_MAGIC, DIAG_FILE_MAGIC_LEN);
    hdr.version       = DIAG_FILE_VERSION;
    hdr.profile       = (uint8_t)ring->profile;
    hdr.snapshot_size  = (uint16_t)sizeof(diag_snapshot_t);
    hdr.count          = ring->count;
    hdr.serial_nr      = serial_nr;
    hdr.start_uptime   = ring->start_time;

    if (fw_version)
    {
        size_t fwlen = strlen(fw_version);
        if (fwlen > sizeof(hdr.firmware_version) - 1)
            fwlen = sizeof(hdr.firmware_version) - 1;
        memcpy(hdr.firmware_version, fw_version, fwlen);
    }

    /* Write header */
    memcpy(buf, &hdr, header_sz);

    /* Write snapshots in chronological order */
    if (ring->count > 0)
    {
        diag_snapshot_t *snap_out = (diag_snapshot_t *)(buf + header_sz);
        uint16_t n = diag_ring_read(ring, snap_out, ring->count);
        (void)n;  /* read count already verified via ring->count */
    }

    /* Write CRC32 over header + snapshots */
    size_t payload_len = header_sz + snap_sz;
    uint32_t crc = diag_crc32(buf, payload_len);
    memcpy(buf + payload_len, &crc, sizeof(crc));

    return total;
}
