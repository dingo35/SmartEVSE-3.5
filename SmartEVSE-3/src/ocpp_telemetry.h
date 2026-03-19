/*
 * ocpp_telemetry.h - OCPP connection and transaction telemetry
 *
 * Pure C module that tracks OCPP health metrics: WebSocket connect/disconnect
 * counts, transaction lifecycle events, authorization results, and message
 * statistics. Designed to be updated from esp32.cpp callbacks and read by
 * the HTTP API and MQTT publisher.
 */

#ifndef OCPP_TELEMETRY_H
#define OCPP_TELEMETRY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* WebSocket connection tracking */
    uint32_t ws_connect_count;
    uint32_t ws_disconnect_count;

    /* Transaction lifecycle */
    uint32_t tx_start_count;
    uint32_t tx_stop_count;
    bool     tx_active;

    /* Authorization results */
    uint32_t auth_accept_count;
    uint32_t auth_reject_count;
    uint32_t auth_timeout_count;

    /* Message statistics */
    uint32_t msg_sent_count;
    uint32_t msg_recv_count;

    /* Load balancing conflict tracking */
    bool     lb_conflict;
} ocpp_telemetry_t;

/* Initialize all counters to zero */
void ocpp_telemetry_init(ocpp_telemetry_t *t);

/* Counter increment functions */
void ocpp_telemetry_ws_connected(ocpp_telemetry_t *t);
void ocpp_telemetry_ws_disconnected(ocpp_telemetry_t *t);
void ocpp_telemetry_tx_started(ocpp_telemetry_t *t);
void ocpp_telemetry_tx_stopped(ocpp_telemetry_t *t);
void ocpp_telemetry_auth_accepted(ocpp_telemetry_t *t);
void ocpp_telemetry_auth_rejected(ocpp_telemetry_t *t);
void ocpp_telemetry_auth_timeout(ocpp_telemetry_t *t);

#ifdef __cplusplus
}
#endif

#endif /* OCPP_TELEMETRY_H */
