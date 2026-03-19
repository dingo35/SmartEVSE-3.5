/*
 * ocpp_logic.c - Pure C OCPP decision logic
 *
 * Extracted from esp32.cpp OCPP lambdas. All functions are pure: they take
 * inputs and return decisions without side effects, making them testable
 * natively with gcc on any host.
 */

#include "ocpp_logic.h"
#include "evse_ctx.h"
#include <string.h>
#include <stdio.h>

/* ---- Auth path selection ---- */

ocpp_auth_path_t ocpp_select_auth_path(uint8_t rfid_reader) {
    /* RFIDReader=0 (disabled) or RFIDReader=6 (Rmt/OCPP): OCPP controls Access_bit */
    if (rfid_reader == 0 || rfid_reader == 6) {
        return OCPP_AUTH_PATH_OCPP_CONTROLLED;
    }
    /* RFIDReader=1..5: built-in RFID store controls charging, OCPP just reports */
    return OCPP_AUTH_PATH_BUILTIN_RFID;
}

/* ---- Connector state ---- */

bool ocpp_is_connector_plugged(uint8_t cp_voltage) {
    /* Matches esp32.cpp: PILOT_3V..PILOT_9V means connector is plugged */
    return cp_voltage >= PILOT_3V && cp_voltage <= PILOT_9V;
}

bool ocpp_is_ev_ready(uint8_t cp_voltage) {
    /* PILOT_3V..PILOT_6V = State C (EV requesting charge) */
    return cp_voltage >= PILOT_3V && cp_voltage <= PILOT_6V;
}

/* ---- Access decision ---- */

bool ocpp_should_set_access(bool permits_charge, bool prev_permits_charge) {
    /*
     * Set Access_bit only on rising edge: OCPP just started permitting charge.
     * From esp32.cpp: if (!OcppTrackPermitsCharge && ocppPermitsCharge())
     * This ensures we set Access_bit only once per OCPP transaction, so other
     * modules can override it without OCPP re-setting it.
     */
    return !prev_permits_charge && permits_charge;
}

bool ocpp_should_clear_access(bool permits_charge, uint8_t access_status) {
    /*
     * Clear Access_bit when OCPP revokes permission and Access is currently ON.
     * From esp32.cpp: if (AccessStatus == ON && !ocppPermitsCharge())
     */
    return access_status == 1 && !permits_charge;  /* 1 = ON */
}

bool ocpp_should_defer_access(uint8_t mode, uint8_t charge_delay,
                              uint16_t error_flags) {
    /*
     * When FreeVend/auto-auth is active, OCPP grants ocppPermitsCharge()
     * unconditionally. But in Solar mode, we should defer Access_bit until
     * the state machine's solar logic confirms surplus is available.
     *
     * Deferral conditions:
     * 1. Solar mode + NO_SUN error → no solar surplus available
     * 2. Any mode + ChargeDelay active → still in delay period
     */
    if (mode == MODE_SOLAR && (error_flags & NO_SUN)) {
        return true;
    }

    if (charge_delay > 0) {
        return true;
    }

    return false;
}

/* ---- RFID hex formatting ---- */

void ocpp_format_rfid_hex(const uint8_t *rfid, size_t rfid_len,
                          char *out, size_t out_size) {
    if (!rfid || !out || out_size == 0 || rfid_len == 0) {
        if (out && out_size > 0) out[0] = '\0';
        return;
    }

    size_t start = 0;
    size_t count = rfid_len;

    /*
     * Old reader format: rfid[0]==0x01 means the 6-byte UID starts at rfid[1].
     * New reader format: rfid[0]!=0x01 means the 7-byte UID starts at rfid[0].
     * From esp32.cpp ocppLoop():
     *   if (RFID[0] == 0x01) snprintf(buf, ..., RFID[1]..RFID[6])
     *   else                 snprintf(buf, ..., RFID[0]..RFID[6])
     */
    if (rfid_len >= 7 && rfid[0] == 0x01) {
        start = 1;
        count = 6;
    }

    out[0] = '\0';
    for (size_t i = start; i < start + count && i < rfid_len; i++) {
        size_t pos = (i - start) * 2;
        if (pos + 2 >= out_size) break;
        snprintf(out + pos, 3, "%02X", rfid[i]);
    }
}

/* ---- Load balancing exclusivity ---- */

ocpp_lb_status_t ocpp_check_lb_exclusivity(uint8_t load_bl, bool ocpp_mode,
                                           bool was_standalone) {
    if (!ocpp_mode) {
        return OCPP_LB_OK;
    }

    if (load_bl != 0) {
        /* LoadBl is non-zero while OCPP is active — Smart Charging is ineffective.
         * The state machine guards with !ctx->LoadBl, so OCPP limits are silently
         * ignored even though the backend thinks they're enforced. */
        return OCPP_LB_CONFLICT;
    }

    if (!was_standalone) {
        /* LoadBl was non-zero when OCPP was initialized, now it's 0.
         * The Smart Charging callback was never registered — need OCPP reinit. */
        return OCPP_LB_NEEDS_REINIT;
    }

    return OCPP_LB_OK;
}

/* ---- Settings validation ---- */

ocpp_validate_result_t ocpp_validate_backend_url(const char *url) {
    if (!url || url[0] == '\0') {
        return OCPP_VALIDATE_EMPTY;
    }

    /* Must start with ws:// or wss:// */
    bool has_ws  = (strncmp(url, "ws://", 5) == 0);
    bool has_wss = (strncmp(url, "wss://", 6) == 0);

    if (!has_ws && !has_wss) {
        return OCPP_VALIDATE_BAD_SCHEME;
    }

    /* Must have content after the scheme */
    size_t scheme_len = has_wss ? 6 : 5;
    if (url[scheme_len] == '\0') {
        return OCPP_VALIDATE_BAD_SCHEME;
    }

    return OCPP_VALIDATE_OK;
}

ocpp_validate_result_t ocpp_validate_chargebox_id(const char *cb_id) {
    if (!cb_id || cb_id[0] == '\0') {
        return OCPP_VALIDATE_EMPTY;
    }

    size_t len = strlen(cb_id);

    /* OCPP 1.6 CiString20: max 20 characters */
    if (len > 20) {
        return OCPP_VALIDATE_TOO_LONG;
    }

    /* Only printable ASCII allowed, no special/control chars */
    for (size_t i = 0; i < len; i++) {
        char c = cb_id[i];
        if (c < 0x20 || c > 0x7E) {
            return OCPP_VALIDATE_BAD_CHARS;
        }
        /* Reject chars that could cause issues in OCPP identifiers */
        if (c == '<' || c == '>' || c == '&' || c == '"' || c == '\'') {
            return OCPP_VALIDATE_BAD_CHARS;
        }
    }

    return OCPP_VALIDATE_OK;
}

ocpp_validate_result_t ocpp_validate_auth_key(const char *auth_key) {
    if (!auth_key || auth_key[0] == '\0') {
        /* Empty auth key is acceptable (no auth) */
        return OCPP_VALIDATE_OK;
    }

    size_t len = strlen(auth_key);

    /* OCPP 1.6: max 40 characters for AuthorizationKey */
    if (len > 40) {
        return OCPP_VALIDATE_TOO_LONG;
    }

    return OCPP_VALIDATE_OK;
}

/* ---- IEC 61851 → OCPP StatusNotification mapping ---- */

const char *ocpp_iec61851_to_status(char iec_state, bool evse_ready,
                                    bool tx_active) {
    switch (iec_state) {
    case 'A':
        /* No vehicle connected. If a transaction just ended, MicroOcpp may
         * report Finishing briefly — but from an IEC 61851 perspective, this
         * is Available. */
        return tx_active ? OCPP_STATUS_FINISHING : OCPP_STATUS_AVAILABLE;

    case 'B':
        /* Vehicle connected but not charging. During a transaction, the EV
         * has paused charging (SuspendedEV). Otherwise, it's Preparing. */
        return tx_active ? OCPP_STATUS_SUSPENDED_EV : OCPP_STATUS_PREPARING;

    case 'C':
        /* Vehicle charging. If EVSE is not offering current (e.g., OCPP
         * limit set to 0 or load balancer paused), it's SuspendedEVSE. */
        if (!evse_ready) {
            return OCPP_STATUS_SUSPENDED_EVSE;
        }
        return OCPP_STATUS_CHARGING;

    case 'D':
        /* Charging with ventilation — same as C for OCPP purposes. */
        if (!evse_ready) {
            return OCPP_STATUS_SUSPENDED_EVSE;
        }
        return OCPP_STATUS_CHARGING;

    case 'E':
    case 'F':
        return OCPP_STATUS_FAULTED;

    default:
        return OCPP_STATUS_FAULTED;
    }
}
