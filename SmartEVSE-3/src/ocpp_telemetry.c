/*
 * ocpp_telemetry.c - OCPP connection and transaction telemetry
 *
 * Pure C counter management. All functions are simple and safe to call
 * from any context. The struct is ~40 bytes — well within RAM budget.
 */

#include "ocpp_telemetry.h"
#include <string.h>

void ocpp_telemetry_init(ocpp_telemetry_t *t) {
    if (!t) return;
    memset(t, 0, sizeof(*t));
}

void ocpp_telemetry_ws_connected(ocpp_telemetry_t *t) {
    if (!t) return;
    t->ws_connect_count++;
}

void ocpp_telemetry_ws_disconnected(ocpp_telemetry_t *t) {
    if (!t) return;
    t->ws_disconnect_count++;
}

void ocpp_telemetry_tx_started(ocpp_telemetry_t *t) {
    if (!t) return;
    t->tx_start_count++;
    t->tx_active = true;
}

void ocpp_telemetry_tx_stopped(ocpp_telemetry_t *t) {
    if (!t) return;
    t->tx_stop_count++;
    t->tx_active = false;
}

void ocpp_telemetry_auth_accepted(ocpp_telemetry_t *t) {
    if (!t) return;
    t->auth_accept_count++;
}

void ocpp_telemetry_auth_rejected(ocpp_telemetry_t *t) {
    if (!t) return;
    t->auth_reject_count++;
}

void ocpp_telemetry_auth_timeout(ocpp_telemetry_t *t) {
    if (!t) return;
    t->auth_timeout_count++;
}
