/*
 * diag_sampler.h - Diagnostic telemetry firmware integration
 *
 * Provides the sampling bridge between firmware globals and the
 * pure C ring buffer (diag_telemetry.h).
 *
 * Call diag_sampler_init() once at startup.
 * Call diag_sample() from Timer1S (1 Hz) or Timer10ms (10 Hz for FAST/MODBUS).
 */

#ifndef DIAG_SAMPLER_H
#define DIAG_SAMPLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "diag_telemetry.h"

/* Initialize the diagnostic sampler (allocates static ring buffer). */
void diag_sampler_init(void);

/* Take a sample if the active profile says it's time.
 * Call from timer1s (1 Hz context) for GENERAL/SOLAR/LOADBAL profiles. */
void diag_sample(void);

/* Take a fast sample (10 Hz context) for MODBUS/FAST profiles.
 * Call from Timer100ms.  No-op if profile is not MODBUS/FAST.
 * The uptime counter is NOT incremented here — only diag_sample() does that. */
void diag_sample_fast(void);

/* Get a pointer to the global ring buffer (for REST endpoints). */
diag_ring_t *diag_get_ring(void);

/* Start capture with a given profile.  Resets the ring buffer. */
void diag_start(diag_profile_t profile);

/* Stop capture (freezes the ring buffer for download). */
void diag_stop(void);

/* Format a JSON status response into buf.  Returns bytes written or -1. */
int diag_status_json(char *buf, size_t bufsz);

/* Push latest snapshot to connected WebSocket clients.
 * Implemented in network_common.cpp. No-op if no clients connected. */
void diag_ws_push_snapshot(const diag_snapshot_t *snap);

#ifdef __cplusplus
}
#endif

#endif /* DIAG_SAMPLER_H */
