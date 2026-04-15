/*
 * http_auth.c — Pure C HTTP auth decision. See http_auth.h.
 */

#include "http_auth.h"
#include <string.h>

/* Return true if `origin` is plausibly this device's own origin.
 *
 * `origin` looks like "http://smartevse-1234.local" or "http://192.168.1.50:80"
 * or "https://192.168.1.50". We don't do full URL parsing; we just require that
 * `host` (hostname / IP, no scheme, no port, no trailing slash) appears as a
 * substring of `origin` AND the origin starts with "http://" or "https://".
 *
 * That is deliberately loose — the purpose of this check is to block
 * cross-origin requests from random websites, not to act as a strong
 * same-origin policy. A user on their own LAN typing the device's IP into
 * a browser tab should always match. */
static bool http_auth_origin_matches(const char *origin, const char *host) {
    if (!origin || !host) return false;
    if (origin[0] == '\0' || host[0] == '\0') return false;

    /* Must start with http:// or https:// */
    if (strncmp(origin, "http://", 7) != 0 &&
        strncmp(origin, "https://", 8) != 0) {
        return false;
    }

    /* host must appear somewhere in origin (anywhere past the scheme). */
    return strstr(origin, host) != NULL;
}

http_auth_result_t http_auth_decide(uint8_t        auth_mode,
                                    bool           lcd_password_ok,
                                    uint32_t       password_ok_ts_ms,
                                    uint32_t       now_ms,
                                    const char    *origin_header,
                                    const char    *allowed_origin_host) {
    /* AuthMode=0: legacy — let everything through. Preserves backward
     * compatibility on upgrade for every existing installation. */
    if (auth_mode == AUTH_MODE_OFF) {
        return HTTP_AUTH_ALLOW;
    }

    /* AuthMode>=1: the PIN must have been verified recently. */
    if (!lcd_password_ok) {
        return HTTP_AUTH_DENY_UNAUTH;
    }

    /* Session-timeout: once idle past HTTP_AUTH_SESSION_TIMEOUT_MS, revoke.
     * Caller is responsible for resetting LCDPasswordOK based on this result.
     * Guard against a zero timestamp meaning "never set" to avoid spurious
     * expiration on cold boot with the flag somehow already true. */
    if (password_ok_ts_ms != 0 &&
        (now_ms - password_ok_ts_ms) >= HTTP_AUTH_SESSION_TIMEOUT_MS) {
        return HTTP_AUTH_DENY_SESSION_EXPIRED;
    }

    /* CSRF Origin check: only when the request carries an Origin header. A
     * missing Origin is normal for non-browser clients (curl, Home Assistant,
     * custom scripts) that deliberately integrate with the REST API. Block
     * only when Origin is PRESENT and does NOT match the device's own host. */
    if (origin_header != NULL && origin_header[0] != '\0') {
        if (!http_auth_origin_matches(origin_header, allowed_origin_host)) {
            return HTTP_AUTH_DENY_CSRF;
        }
    }

    return HTTP_AUTH_ALLOW;
}
