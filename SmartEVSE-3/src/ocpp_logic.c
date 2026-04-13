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
     *
     * Invalid mode values (> MODE_SOLAR) are treated as "don't defer" — safe default.
     */
    if (mode > MODE_SOLAR) {
        return false;
    }

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
        if (pos + 3 > out_size) break;
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

/* Case-insensitive prefix compare, byte-level — avoids depending on strncasecmp
 * availability in the pure-C build. */
static bool ocpp_ci_has_prefix(const char *s, size_t s_len, const char *prefix) {
    size_t p_len = strlen(prefix);
    if (s_len < p_len) return false;
    for (size_t i = 0; i < p_len; i++) {
        char a = s[i], b = prefix[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = (char)(b + ('a' - 'A'));
        if (a != b) return false;
    }
    return true;
}

/* SECURITY H-4: detect an IP literal or hostname that points to the charger
 * itself or somewhere on the same node (SSRF). Also catches 0.0.0.0 which some
 * stacks alias to loopback. The check is prefix-based on the raw authority,
 * matching common forms written into an OCPP URL. */
static bool ocpp_authority_is_loopback(const char *host, size_t host_len) {
    if (ocpp_ci_has_prefix(host, host_len, "localhost")) {
        /* Accept "localhost", "localhost:port", "localhost/path" — reject all */
        if (host_len == 9) return true;
        char c = host[9];
        if (c == ':' || c == '/' || c == '\0') return true;
    }
    /* 127.0.0.0/8 — any address starting with 127. is loopback */
    if (host_len >= 4 && host[0] == '1' && host[1] == '2' && host[2] == '7' && host[3] == '.') {
        return true;
    }
    /* 0.0.0.0 — some stacks bind-any, resolves to loopback on connect */
    if (host_len >= 7 && strncmp(host, "0.0.0.0", 7) == 0) {
        char c = (host_len > 7) ? host[7] : '\0';
        if (c == '\0' || c == ':' || c == '/') return true;
    }
    /* IPv6 loopback — raw "::1" or bracketed "[::1]" */
    if (host_len >= 3 && strncmp(host, "::1", 3) == 0) {
        char c = (host_len > 3) ? host[3] : '\0';
        if (c == '\0' || c == ':' || c == '/') return true;
    }
    if (host_len >= 5 && strncmp(host, "[::1]", 5) == 0) return true;
    return false;
}

/* SECURITY H-4: link-local addresses — the charger must never speak OCPP into
 * the 169.254.0.0/16 AutoIP range or the IPv6 fe80::/10 link-local range. */
static bool ocpp_authority_is_link_local(const char *host, size_t host_len) {
    if (host_len >= 8 && strncmp(host, "169.254.", 8) == 0) return true;
    if (ocpp_ci_has_prefix(host, host_len, "fe80:"))  return true;
    if (ocpp_ci_has_prefix(host, host_len, "[fe80:")) return true;
    return false;
}

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

    /* SECURITY H-4: parse the authority (between scheme and first /?# or end)
     * and run the anti-SSRF + anti-userinfo checks on just that portion.
     * Characters permitted elsewhere in the URL (path/query) are not affected. */
    const char *auth_start = url + scheme_len;
    size_t auth_len = 0;
    bool has_userinfo = false;
    while (auth_start[auth_len] != '\0' &&
           auth_start[auth_len] != '/'  &&
           auth_start[auth_len] != '?'  &&
           auth_start[auth_len] != '#') {
        if (auth_start[auth_len] == '@') has_userinfo = true;
        auth_len++;
    }
    if (has_userinfo) {
        return OCPP_VALIDATE_EMBEDDED_CREDS;
    }
    if (auth_len == 0) {
        return OCPP_VALIDATE_BAD_SCHEME;  /* scheme with no host */
    }
    if (ocpp_authority_is_loopback(auth_start, auth_len)) {
        return OCPP_VALIDATE_SSRF_LOOPBACK;
    }
    if (ocpp_authority_is_link_local(auth_start, auth_len)) {
        return OCPP_VALIDATE_SSRF_LINK_LOCAL;
    }

    /* Validate characters after scheme: allow only URL-safe characters.
     * Reject control chars, spaces, CRLF injection, and other problematic chars. */
    for (size_t i = scheme_len; url[i] != '\0'; i++) {
        char c = url[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9')) {
            continue;
        }
        /* Allowed URL special characters */
        if (c == '.' || c == ':' || c == '/' || c == '-' || c == '_' ||
            c == '?' || c == '=' || c == '&' || c == '@' || c == '%' ||
            c == '+' || c == '#' || c == '[' || c == ']') {
            continue;
        }
        return OCPP_VALIDATE_BAD_CHARS;
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

/* ---- Silent OCPP session loss detection (upstream commit ecd088b) ---- */

ocpp_silence_action_t ocpp_silence_decide(bool ws_connected,
                                          unsigned long now_ms,
                                          unsigned long last_response_ms,
                                          unsigned long last_probe_ms) {
    /* No decision while transport is down — the WS layer will reconnect on
     * its own and ocppInit() reseeds last_response_ms. */
    if (!ws_connected) {
        return OCPP_SILENCE_NO_ACTION;
    }

    /* Force-reconnect takes priority. The 0-guard prevents a stale or
     * uninitialized last_response_ms from triggering a reconnect on cold
     * boot before any response has ever been observed. */
    if (last_response_ms != 0 &&
        (now_ms - last_response_ms) >= OCPP_SILENCE_TIMEOUT_MS) {
        return OCPP_SILENCE_FORCE_RECONNECT;
    }

    if ((now_ms - last_probe_ms) >= OCPP_PROBE_INTERVAL_MS) {
        return OCPP_SILENCE_SEND_PROBE;
    }

    return OCPP_SILENCE_NO_ACTION;
}

/* ---- Connector lock decision (upstream commit 05c7fc2) ---- */

bool ocpp_should_force_lock(bool tx_present,
                            bool tx_authorized,
                            bool tx_active_or_running,
                            uint8_t cp_voltage,
                            bool locking_tx_present,
                            bool locking_tx_start_requested) {
    /* Active authorized transaction with the connector plugged. */
    if (tx_present && tx_authorized && tx_active_or_running &&
        cp_voltage >= PILOT_3V && cp_voltage <= PILOT_9V) {
        return true;
    }

    /* LockingTx waiting for matching RFID swipe to release. */
    if (locking_tx_present && locking_tx_start_requested) {
        return true;
    }

    return false;
}

/* ---- Occupied-input decision (upstream commit afd72a8) ---- */

bool ocpp_should_report_occupied(bool locking_tx_present,
                                 bool tx_notif_defined,
                                 bool tx_notif_is_stoptx,
                                 unsigned long now_ms,
                                 unsigned long last_tx_notif_ms) {
    /* A LockingTx (RFID-bound transaction) keeps the connector occupied. */
    if (locking_tx_present) {
        return true;
    }

    /* Brief grace window after StopTx so the CSMS sees "Finishing" before
     * "Available" (OCPP 1.6 state-sequence requirement). */
    if (tx_notif_defined && tx_notif_is_stoptx &&
        (now_ms - last_tx_notif_ms) < OCPP_FINISHING_GRACE_MS) {
        return true;
    }

    return false;
}
