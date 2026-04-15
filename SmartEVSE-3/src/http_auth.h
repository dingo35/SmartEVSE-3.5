/*
 * http_auth.h — Pure C HTTP auth decision for the SmartEVSE Web UI / REST API.
 *
 * Extracted from the HTTP handlers so the auth decision is unit-testable
 * without Arduino / Mongoose. See docs/security/plan-16-http-auth-layer.md
 * for the design, threat model, and backward-compatibility strategy.
 */

#ifndef HTTP_AUTH_H
#define HTTP_AUTH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AuthMode settings — persisted in NVS alongside LCDPin.
 * Default is OFF so existing installations are not bricked on upgrade.
 */
#define AUTH_MODE_OFF       0  /* Legacy — all endpoints unauthenticated */
#define AUTH_MODE_REQUIRED  1  /* Mutating + sensitive-data endpoints require PIN */

/*
 * Default idle timeout for the per-server authenticated flag. After this many
 * milliseconds without an authenticated request, LCDPasswordOK is cleared and
 * the next mutating request requires a fresh PIN verification.
 */
#define HTTP_AUTH_SESSION_TIMEOUT_MS  (30UL * 60UL * 1000UL)   /* 30 minutes */

/*
 * Decide whether an incoming HTTP request should be allowed through the auth
 * gate. Pure — caller extracts the needed bits from Mongoose / millis() state.
 *
 *   auth_mode              — current AuthMode setting (AUTH_MODE_OFF / REQUIRED)
 *   lcd_password_ok        — current LCDPasswordOK flag
 *   password_ok_ts_ms      — millis() at which LCDPasswordOK was set (0 if never)
 *   now_ms                 — current millis()
 *   origin_header          — request `Origin:` value; NULL if absent
 *   allowed_origin_host    — the device's own hostname/IP string, or NULL if not known
 *
 * Return values:
 *   HTTP_AUTH_ALLOW            — let the request through
 *   HTTP_AUTH_DENY_UNAUTH      — 401, needs PIN verification
 *   HTTP_AUTH_DENY_CSRF        — 403, Origin header does not match
 *   HTTP_AUTH_DENY_SESSION_EXPIRED — 401, session timed out (same 401 as unauth
 *                                   to the client but reported separately so the
 *                                   server can reset LCDPasswordOK)
 */
typedef enum {
    HTTP_AUTH_ALLOW                   = 0,
    HTTP_AUTH_DENY_UNAUTH             = 1,
    HTTP_AUTH_DENY_CSRF               = 2,
    HTTP_AUTH_DENY_SESSION_EXPIRED    = 3
} http_auth_result_t;

http_auth_result_t http_auth_decide(uint8_t        auth_mode,
                                    bool           lcd_password_ok,
                                    uint32_t       password_ok_ts_ms,
                                    uint32_t       now_ms,
                                    const char    *origin_header,
                                    const char    *allowed_origin_host);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_AUTH_H */
