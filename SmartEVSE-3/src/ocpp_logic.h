/*
 * ocpp_logic.h - Pure C OCPP decision logic
 *
 * Extracted from esp32.cpp OCPP lambdas and glue code so that authorization,
 * connector state, RFID formatting, settings validation, and load-balancing
 * exclusivity logic can be tested natively without MicroOcpp or Arduino.
 *
 * All functions operate on plain C types — no MicroOcpp, Arduino, or ESP-IDF
 * dependencies allowed in this header.
 */

#ifndef OCPP_LOGIC_H
#define OCPP_LOGIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Auth path selection ---- */

typedef enum {
    OCPP_AUTH_PATH_OCPP_CONTROLLED = 0,  /* RFIDReader=0 or 6: OCPP controls Access_bit */
    OCPP_AUTH_PATH_BUILTIN_RFID    = 1   /* RFIDReader=1..5: built-in RFID store controls, OCPP just reports */
} ocpp_auth_path_t;

ocpp_auth_path_t ocpp_select_auth_path(uint8_t rfid_reader);

/* ---- Connector state ---- */

bool ocpp_is_connector_plugged(uint8_t cp_voltage);
bool ocpp_is_ev_ready(uint8_t cp_voltage);

/* ---- Access decision (OCPP-controlled auth path) ---- */

/*
 * Returns true if OCPP should set Access_bit ON.
 * Caller provides:
 *   permits_charge       — current ocppPermitsCharge() result
 *   prev_permits_charge  — tracked value from previous loop iteration
 */
bool ocpp_should_set_access(bool permits_charge, bool prev_permits_charge);

/*
 * Returns true if OCPP should clear Access_bit to OFF.
 * Caller provides:
 *   permits_charge  — current ocppPermitsCharge() result
 *   access_status   — current AccessStatus (0=OFF, 1=ON, 2=PAUSE)
 */
bool ocpp_should_clear_access(bool permits_charge, uint8_t access_status);

/*
 * Returns true if setting Access_bit should be deferred despite OCPP
 * permitting charge. This guards against FreeVend/auto-auth bypassing
 * Solar mode surplus checks or ChargeDelay.
 *
 *   mode          — current Mode (MODE_NORMAL=0, MODE_SMART=1, MODE_SOLAR=2)
 *   charge_delay  — current ChargeDelay (>0 means delay active)
 *   error_flags   — current ErrorFlags (NO_SUN bit means no solar surplus)
 */
bool ocpp_should_defer_access(uint8_t mode, uint8_t charge_delay,
                              uint16_t error_flags);

/* ---- RFID hex formatting ---- */

#define OCPP_RFID_HEX_MAX  15  /* 7 bytes * 2 hex chars + NUL */

/*
 * Format RFID bytes as uppercase hex string.
 *   rfid       — raw RFID bytes (7 bytes expected)
 *   rfid_len   — number of bytes in rfid array
 *   out        — output buffer (must be >= OCPP_RFID_HEX_MAX)
 *   out_size   — size of output buffer
 *
 * Old reader format: rfid[0]==0x01 means 6-byte UID starts at rfid[1].
 * New reader format: rfid[0]!=0x01 means 7-byte UID starts at rfid[0].
 */
void ocpp_format_rfid_hex(const uint8_t *rfid, size_t rfid_len,
                          char *out, size_t out_size);

/* ---- Load balancing exclusivity ---- */

typedef enum {
    OCPP_LB_OK           = 0,  /* No conflict */
    OCPP_LB_CONFLICT     = 1,  /* LoadBl != 0 while OCPP active — Smart Charging ineffective */
    OCPP_LB_NEEDS_REINIT = 2   /* LoadBl changed from non-zero to 0 — OCPP needs disable/enable */
} ocpp_lb_status_t;

/*
 * Check whether OCPP Smart Charging and internal load balancing conflict.
 *   load_bl         — current LoadBl value (0=standalone)
 *   ocpp_mode       — true if OCPP is enabled
 *   was_standalone   — true if LoadBl was 0 when OCPP was last initialized
 */
ocpp_lb_status_t ocpp_check_lb_exclusivity(uint8_t load_bl, bool ocpp_mode,
                                           bool was_standalone);

/* ---- Settings validation ---- */

typedef enum {
    OCPP_VALIDATE_OK          = 0,
    OCPP_VALIDATE_EMPTY       = 1,
    OCPP_VALIDATE_BAD_SCHEME  = 2,
    OCPP_VALIDATE_TOO_LONG    = 3,
    OCPP_VALIDATE_BAD_CHARS   = 4
} ocpp_validate_result_t;

/*
 * Validate OCPP backend URL.
 * Must start with "ws://" or "wss://", be non-empty, and have content after scheme.
 */
ocpp_validate_result_t ocpp_validate_backend_url(const char *url);

/*
 * Validate OCPP ChargeBoxId.
 * OCPP 1.6 CiString20: max 20 chars, printable ASCII, no special chars.
 */
ocpp_validate_result_t ocpp_validate_chargebox_id(const char *cb_id);

/*
 * Validate OCPP auth key.
 * OCPP 1.6: max 40 chars.
 */
ocpp_validate_result_t ocpp_validate_auth_key(const char *auth_key);

/* ---- IEC 61851 → OCPP StatusNotification mapping ---- */

/*
 * OCPP 1.6 ChargePointStatus values as string constants.
 * These match the StatusNotification.req status field.
 */
#define OCPP_STATUS_AVAILABLE      "Available"
#define OCPP_STATUS_PREPARING      "Preparing"
#define OCPP_STATUS_CHARGING       "Charging"
#define OCPP_STATUS_SUSPENDED_EVSE "SuspendedEVSE"
#define OCPP_STATUS_SUSPENDED_EV   "SuspendedEV"
#define OCPP_STATUS_FINISHING      "Finishing"
#define OCPP_STATUS_FAULTED        "Faulted"

/*
 * Map IEC 61851 state letter to OCPP 1.6 ChargePointStatus string.
 *   iec_state         — IEC 61851 state ('A'-'F' from evse_state_to_iec61851())
 *   evse_ready        — true if EVSE is offering current (PWM > 0)
 *   tx_active         — true if an OCPP transaction is running
 *
 * Returns a pointer to a static string constant (never NULL).
 *
 * Mapping:
 *   A (no vehicle)           → Available (or Finishing if tx just ended)
 *   B (vehicle connected)    → Preparing (or SuspendedEV if tx active but EV not drawing)
 *   C (charging)             → Charging (or SuspendedEVSE if EVSE not offering current)
 *   D (with ventilation)     → Charging
 *   E (error)                → Faulted
 *   F (not available)        → Faulted
 */
const char *ocpp_iec61851_to_status(char iec_state, bool evse_ready,
                                    bool tx_active);

/* ---- Silent OCPP session loss detection (upstream commit ecd088b) ---- */

/*
 * The MicroOcpp WebSocket layer keeps the transport alive with ping/pong frames,
 * but those don't prove the OCPP backend is still processing application messages.
 * If the backend goes silent at the OCPP layer (e.g. CSMS app crash, broken proxy),
 * pings keep flowing and the charger thinks it's connected while transactions are
 * silently dropped. The fix: send periodic Heartbeat probes and track responses,
 * forcing a WebSocket reconnect if the backend stays silent for too long.
 */

#define OCPP_PROBE_INTERVAL_MS    90000UL  /* Send Heartbeat probe every 90 seconds  */
#define OCPP_SILENCE_TIMEOUT_MS  300000UL  /* Force reconnect after 5 minutes silent */

typedef enum {
    OCPP_SILENCE_NO_ACTION       = 0,
    OCPP_SILENCE_SEND_PROBE      = 1,
    OCPP_SILENCE_FORCE_RECONNECT = 2
} ocpp_silence_action_t;

/*
 * Decide whether to send a heartbeat probe or force a WebSocket reconnect.
 *
 *   ws_connected      — true if the underlying WebSocket reports connected
 *   now_ms            — current monotonic millisecond timestamp (e.g. millis())
 *   last_response_ms  — timestamp of the last received OCPP-level response;
 *                       0 means "not yet initialized" — function returns
 *                       NO_ACTION rather than triggering a reconnect on cold
 *                       boot before the first probe is sent
 *   last_probe_ms     — timestamp of the last probe we sent
 *
 * Force-reconnect takes priority over probe so we don't spam probes when the
 * backend is already declared dead.
 *
 * The function is pure: callers update last_probe_ms / last_response_ms based
 * on the returned action.
 */
ocpp_silence_action_t ocpp_silence_decide(bool ws_connected,
                                          unsigned long now_ms,
                                          unsigned long last_response_ms,
                                          unsigned long last_probe_ms);

/* ---- Connector lock decision (upstream commit 05c7fc2) ---- */

/*
 * Decide whether the connector cable lock actuator should be engaged.
 *
 * Upstream bug: the previous implementation reset OcppForcesLock to false
 * unconditionally and then conditionally set it to true within the same loop
 * iteration, causing brief false→true flips that the lock dispatcher could
 * sample mid-flip and translate into rapid actuator unlock/relock cycles.
 *
 * Fix: compute the lock decision into a local first, assign once at the end.
 * Extracted to pure C so the (input → boolean) decision can be tested
 * exhaustively.
 *
 *   tx_present                  — getTransaction() returned non-null
 *   tx_authorized               — transaction->isAuthorized()
 *   tx_active_or_running        — transaction->isActive() || ->isRunning()
 *   cp_voltage                  — OcppTrackCPvoltage (PILOT_xV constant)
 *   locking_tx_present          — OcppLockingTx is non-null
 *   locking_tx_start_requested  — OcppLockingTx->getStartSync().isRequested()
 *
 * Lock conditions (any one is sufficient):
 *   1. Active authorized transaction with the connector plugged
 *      (cp_voltage in [PILOT_3V .. PILOT_9V])
 *   2. A LockingTx exists and its StartTransaction has been requested
 *      (LockingTx persists past tx completion to keep the connector locked
 *      until the same RFID card is presented again)
 */
bool ocpp_should_force_lock(bool tx_present,
                            bool tx_authorized,
                            bool tx_active_or_running,
                            uint8_t cp_voltage,
                            bool locking_tx_present,
                            bool locking_tx_start_requested);

#ifdef __cplusplus
}
#endif

#endif /* OCPP_LOGIC_H */
