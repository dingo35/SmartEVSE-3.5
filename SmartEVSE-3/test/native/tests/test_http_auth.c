/*
 * test_http_auth.c — pure C tests for the HTTP auth decision (Plan 16 Phase 1).
 * See src/http_auth.h and docs/security/plan-16-http-auth-layer.md.
 */

#include "test_framework.h"
#include "http_auth.h"

/* ---- AuthMode=OFF — everything passes through (backward compat) ---- */

/*
 * @feature HTTP Auth
 * @req REQ-AUTH-001
 * @scenario AuthMode=OFF allows any request (no PIN, no Origin)
 */
void test_auth_off_allows_unauthenticated(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_AUTH_ALLOW,
        http_auth_decide(AUTH_MODE_OFF,
            /*pw_ok*/ false, /*pw_ts*/ 0, /*now*/ 1000,
            /*origin*/ NULL, /*host*/ NULL));
}

/*
 * @feature HTTP Auth
 * @req REQ-AUTH-001
 * @scenario AuthMode=OFF allows request with foreign Origin (no CSRF check)
 */
void test_auth_off_allows_foreign_origin(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_AUTH_ALLOW,
        http_auth_decide(AUTH_MODE_OFF, false, 0, 1000,
            "http://evil.example", "smartevse.local"));
}

/* ---- AuthMode=REQUIRED — denies unauthenticated ---- */

/*
 * @feature HTTP Auth
 * @req REQ-AUTH-002
 * @scenario AuthMode=REQUIRED denies request without PIN verification
 */
void test_auth_required_denies_unauth(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_AUTH_DENY_UNAUTH,
        http_auth_decide(AUTH_MODE_REQUIRED,
            false, 0, 1000, NULL, "smartevse.local"));
}

/*
 * @feature HTTP Auth
 * @req REQ-AUTH-002
 * @scenario AuthMode=REQUIRED allows PIN-verified request
 */
void test_auth_required_allows_authed(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_AUTH_ALLOW,
        http_auth_decide(AUTH_MODE_REQUIRED,
            /*pw_ok*/ true, /*pw_ts*/ 900, /*now*/ 1000,
            NULL, "smartevse.local"));
}

/* ---- Session timeout ---- */

/*
 * @feature HTTP Auth
 * @req REQ-AUTH-003
 * @scenario Authenticated session expires after HTTP_AUTH_SESSION_TIMEOUT_MS idle
 */
void test_auth_session_expires(void) {
    uint32_t now = 10UL * HTTP_AUTH_SESSION_TIMEOUT_MS;  /* safely past the window */
    uint32_t ts  = now - HTTP_AUTH_SESSION_TIMEOUT_MS - 1;
    TEST_ASSERT_EQUAL_INT(HTTP_AUTH_DENY_SESSION_EXPIRED,
        http_auth_decide(AUTH_MODE_REQUIRED, true, ts, now, NULL, NULL));
}

/*
 * @feature HTTP Auth
 * @req REQ-AUTH-003
 * @scenario Authenticated session still valid just before the timeout boundary
 */
void test_auth_session_just_before_timeout(void) {
    uint32_t now = 10UL * HTTP_AUTH_SESSION_TIMEOUT_MS;
    uint32_t ts  = now - HTTP_AUTH_SESSION_TIMEOUT_MS + 1;
    TEST_ASSERT_EQUAL_INT(HTTP_AUTH_ALLOW,
        http_auth_decide(AUTH_MODE_REQUIRED, true, ts, now, NULL, NULL));
}

/*
 * @feature HTTP Auth
 * @req REQ-AUTH-003
 * @scenario Session with zero timestamp is treated as "never set" (defensive)
 */
void test_auth_session_zero_ts_does_not_expire(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_AUTH_ALLOW,
        http_auth_decide(AUTH_MODE_REQUIRED, true, 0, 99999UL, NULL, NULL));
}

/* ---- CSRF Origin check ---- */

/*
 * @feature HTTP Auth
 * @req REQ-AUTH-004
 * @scenario Missing Origin header allowed (non-browser integration)
 */
void test_auth_no_origin_allowed(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_AUTH_ALLOW,
        http_auth_decide(AUTH_MODE_REQUIRED, true, 900, 1000,
            NULL, "192.168.1.50"));
}

/*
 * @feature HTTP Auth
 * @req REQ-AUTH-004
 * @scenario Matching Origin allowed
 */
void test_auth_matching_origin_allowed(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_AUTH_ALLOW,
        http_auth_decide(AUTH_MODE_REQUIRED, true, 900, 1000,
            "http://192.168.1.50", "192.168.1.50"));
}

/*
 * @feature HTTP Auth
 * @req REQ-AUTH-004
 * @scenario Matching hostname in origin allowed
 */
void test_auth_matching_hostname_origin_allowed(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_AUTH_ALLOW,
        http_auth_decide(AUTH_MODE_REQUIRED, true, 900, 1000,
            "http://smartevse-1234.local", "smartevse-1234.local"));
}

/*
 * @feature HTTP Auth
 * @req REQ-AUTH-004
 * @scenario Foreign Origin blocked as CSRF
 */
void test_auth_foreign_origin_blocked(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_AUTH_DENY_CSRF,
        http_auth_decide(AUTH_MODE_REQUIRED, true, 900, 1000,
            "http://evil.example", "192.168.1.50"));
}

/*
 * @feature HTTP Auth
 * @req REQ-AUTH-004
 * @scenario Origin with unexpected scheme blocked
 */
void test_auth_origin_bad_scheme_blocked(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_AUTH_DENY_CSRF,
        http_auth_decide(AUTH_MODE_REQUIRED, true, 900, 1000,
            "ws://192.168.1.50", "192.168.1.50"));
}

/*
 * @feature HTTP Auth
 * @req REQ-AUTH-004
 * @scenario https:// Origin matching device IP allowed
 */
void test_auth_https_matching_origin_allowed(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_AUTH_ALLOW,
        http_auth_decide(AUTH_MODE_REQUIRED, true, 900, 1000,
            "https://192.168.1.50:8443", "192.168.1.50"));
}

/* ---- Precedence — CSRF check only applied when PIN ok ---- */

/*
 * @feature HTTP Auth
 * @req REQ-AUTH-005
 * @scenario Unauth + foreign Origin reports UNAUTH first (PIN check precedes CSRF)
 */
void test_auth_unauth_precedes_csrf(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_AUTH_DENY_UNAUTH,
        http_auth_decide(AUTH_MODE_REQUIRED, false, 0, 1000,
            "http://evil.example", "192.168.1.50"));
}

int main(void) {
    TEST_SUITE_BEGIN("HTTP Auth");

    RUN_TEST(test_auth_off_allows_unauthenticated);
    RUN_TEST(test_auth_off_allows_foreign_origin);
    RUN_TEST(test_auth_required_denies_unauth);
    RUN_TEST(test_auth_required_allows_authed);
    RUN_TEST(test_auth_session_expires);
    RUN_TEST(test_auth_session_just_before_timeout);
    RUN_TEST(test_auth_session_zero_ts_does_not_expire);
    RUN_TEST(test_auth_no_origin_allowed);
    RUN_TEST(test_auth_matching_origin_allowed);
    RUN_TEST(test_auth_matching_hostname_origin_allowed);
    RUN_TEST(test_auth_foreign_origin_blocked);
    RUN_TEST(test_auth_origin_bad_scheme_blocked);
    RUN_TEST(test_auth_https_matching_origin_allowed);
    RUN_TEST(test_auth_unauth_precedes_csrf);

    TEST_SUITE_RESULTS();
}
