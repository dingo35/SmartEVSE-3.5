/*
 * test_ocpp_settings.c - OCPP settings validation tests
 *
 * Tests the pure C validation functions for OCPP backend URL, ChargeBoxId,
 * and auth key. Currently esp32.cpp passes these values to MicroOcpp without
 * validation — these functions add input checking.
 */

#include "test_framework.h"
#include "ocpp_logic.h"

/* ---- Backend URL validation ---- */

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-060
 * @scenario Valid wss:// URL accepted
 * @given URL is "wss://ocpp.example.com/smartevse"
 * @when ocpp_validate_backend_url is called
 * @then Returns OCPP_VALIDATE_OK
 */
void test_url_valid_wss(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_OK,
        ocpp_validate_backend_url("wss://ocpp.example.com/smartevse"));
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-061
 * @scenario Valid ws:// URL accepted
 * @given URL is "ws://192.168.1.100:8180/steve/websocket/CentralSystemService"
 * @when ocpp_validate_backend_url is called
 * @then Returns OCPP_VALIDATE_OK
 */
void test_url_valid_ws(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_OK,
        ocpp_validate_backend_url("ws://192.168.1.100:8180/steve/websocket/CentralSystemService"));
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-062
 * @scenario URL without ws/wss scheme rejected
 * @given URL is "http://ocpp.example.com"
 * @when ocpp_validate_backend_url is called
 * @then Returns OCPP_VALIDATE_BAD_SCHEME
 */
void test_url_http_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_BAD_SCHEME,
        ocpp_validate_backend_url("http://ocpp.example.com"));
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-062
 * @scenario URL with https scheme rejected
 * @given URL is "https://ocpp.example.com"
 * @when ocpp_validate_backend_url is called
 * @then Returns OCPP_VALIDATE_BAD_SCHEME
 */
void test_url_https_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_BAD_SCHEME,
        ocpp_validate_backend_url("https://ocpp.example.com"));
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-063
 * @scenario Empty URL rejected
 * @given URL is empty string
 * @when ocpp_validate_backend_url is called
 * @then Returns OCPP_VALIDATE_EMPTY
 */
void test_url_empty_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_EMPTY,
        ocpp_validate_backend_url(""));
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-063
 * @scenario NULL URL rejected
 * @given URL pointer is NULL
 * @when ocpp_validate_backend_url is called
 * @then Returns OCPP_VALIDATE_EMPTY
 */
void test_url_null_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_EMPTY,
        ocpp_validate_backend_url(NULL));
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-062
 * @scenario Bare scheme without host rejected
 * @given URL is "ws://" with no host
 * @when ocpp_validate_backend_url is called
 * @then Returns OCPP_VALIDATE_BAD_SCHEME because there is no content after scheme
 */
void test_url_bare_scheme_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_BAD_SCHEME,
        ocpp_validate_backend_url("ws://"));
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-062
 * @scenario Bare wss scheme without host rejected
 * @given URL is "wss://" with no host
 * @when ocpp_validate_backend_url is called
 * @then Returns OCPP_VALIDATE_BAD_SCHEME
 */
void test_url_bare_wss_scheme_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_BAD_SCHEME,
        ocpp_validate_backend_url("wss://"));
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-062
 * @scenario Plain text without scheme rejected
 * @given URL is "ocpp.example.com" (no ws:// prefix)
 * @when ocpp_validate_backend_url is called
 * @then Returns OCPP_VALIDATE_BAD_SCHEME
 */
void test_url_no_scheme_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_BAD_SCHEME,
        ocpp_validate_backend_url("ocpp.example.com"));
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-097
 * @scenario URL with CRLF injection rejected
 * @given URL is "ws://example.com\r\nHost: evil"
 * @when ocpp_validate_backend_url is called
 * @then Returns OCPP_VALIDATE_BAD_CHARS because CRLF is not allowed
 */
void test_url_crlf_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_BAD_CHARS,
        ocpp_validate_backend_url("ws://example.com\r\nHost: evil"));
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-097
 * @scenario URL with space rejected
 * @given URL is "ws://example.com/path with space"
 * @when ocpp_validate_backend_url is called
 * @then Returns OCPP_VALIDATE_BAD_CHARS because spaces are not allowed
 */
void test_url_space_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_BAD_CHARS,
        ocpp_validate_backend_url("ws://example.com/path with space"));
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-097
 * @scenario URL with valid special characters accepted
 * @given URL contains all allowed special chars (path/query/fragment): . : / - _ ? = & @ % + #
 * @when ocpp_validate_backend_url is called
 * @then Returns OCPP_VALIDATE_OK
 * @note `@` moved from the authority to the path segment because the H-4
 *       SSRF hardening now rejects embedded `user@host` userinfo in the
 *       authority. `@` is still permitted elsewhere in the URL.
 */
void test_url_valid_special_chars_accepted(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_OK,
        ocpp_validate_backend_url("ws://host.com:8080/path-name_ok?a=1&b=2#frag+%20@tag"));
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-097
 * @scenario URL with backslash rejected
 * @given URL contains a backslash character
 * @when ocpp_validate_backend_url is called
 * @then Returns OCPP_VALIDATE_BAD_CHARS
 */
void test_url_backslash_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_BAD_CHARS,
        ocpp_validate_backend_url("ws://example.com\\path"));
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-097
 * @scenario URL with curly braces rejected
 * @given URL contains curly brace characters
 * @when ocpp_validate_backend_url is called
 * @then Returns OCPP_VALIDATE_BAD_CHARS
 */
void test_url_braces_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_BAD_CHARS,
        ocpp_validate_backend_url("ws://example.com/{path}"));
}

/* ---- ChargeBoxId validation ---- */

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-064
 * @scenario Valid ChargeBoxId accepted
 * @given ChargeBoxId is "SmartEVSE-12345"
 * @when ocpp_validate_chargebox_id is called
 * @then Returns OCPP_VALIDATE_OK
 */
void test_cbid_valid(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_OK,
        ocpp_validate_chargebox_id("SmartEVSE-12345"));
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-064
 * @scenario ChargeBoxId with special characters rejected
 * @given ChargeBoxId contains '<' character
 * @when ocpp_validate_chargebox_id is called
 * @then Returns OCPP_VALIDATE_BAD_CHARS
 */
void test_cbid_special_chars_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_BAD_CHARS,
        ocpp_validate_chargebox_id("Smart<EVSE>"));
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-065
 * @scenario ChargeBoxId length > 20 rejected (OCPP 1.6 CiString20)
 * @given ChargeBoxId is 21 characters long
 * @when ocpp_validate_chargebox_id is called
 * @then Returns OCPP_VALIDATE_TOO_LONG
 */
void test_cbid_too_long_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_TOO_LONG,
        ocpp_validate_chargebox_id("123456789012345678901"));  /* 21 chars */
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-065
 * @scenario ChargeBoxId exactly 20 characters is accepted
 * @given ChargeBoxId is exactly 20 characters long
 * @when ocpp_validate_chargebox_id is called
 * @then Returns OCPP_VALIDATE_OK
 */
void test_cbid_exactly_20_accepted(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_OK,
        ocpp_validate_chargebox_id("12345678901234567890"));  /* 20 chars */
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-064
 * @scenario Empty ChargeBoxId rejected
 * @given ChargeBoxId is empty string
 * @when ocpp_validate_chargebox_id is called
 * @then Returns OCPP_VALIDATE_EMPTY
 */
void test_cbid_empty_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_EMPTY,
        ocpp_validate_chargebox_id(""));
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-064
 * @scenario ChargeBoxId with ampersand rejected
 * @given ChargeBoxId contains '&' character
 * @when ocpp_validate_chargebox_id is called
 * @then Returns OCPP_VALIDATE_BAD_CHARS
 */
void test_cbid_ampersand_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_BAD_CHARS,
        ocpp_validate_chargebox_id("Smart&EVSE"));
}

/* ---- Auth key validation ---- */

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-066
 * @scenario Auth key length > 40 rejected (OCPP 1.6 limit)
 * @given Auth key is 41 characters long
 * @when ocpp_validate_auth_key is called
 * @then Returns OCPP_VALIDATE_TOO_LONG
 */
void test_auth_key_too_long(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_TOO_LONG,
        ocpp_validate_auth_key("12345678901234567890123456789012345678901"));  /* 41 chars */
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-066
 * @scenario Auth key exactly 40 characters is accepted
 * @given Auth key is exactly 40 characters long
 * @when ocpp_validate_auth_key is called
 * @then Returns OCPP_VALIDATE_OK
 */
void test_auth_key_exactly_40_accepted(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_OK,
        ocpp_validate_auth_key("1234567890123456789012345678901234567890"));  /* 40 chars */
}

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-066
 * @scenario Empty auth key is valid (no auth configured)
 * @given Auth key is empty string
 * @when ocpp_validate_auth_key is called
 * @then Returns OCPP_VALIDATE_OK because empty means no auth
 */
void test_auth_key_empty_accepted(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_OK,
        ocpp_validate_auth_key(""));
}

/* ---- Security H-4: SSRF hardening of backend URL validator ---- */

/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-H4-001
 * @scenario Loopback IPv4 127.0.0.1 rejected
 */
void test_url_loopback_127_0_0_1_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_SSRF_LOOPBACK,
        ocpp_validate_backend_url("wss://127.0.0.1/ocpp"));
}
/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-H4-001
 * @scenario Any 127.x loopback rejected (covers 127.42.0.1 etc.)
 */
void test_url_loopback_127_any_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_SSRF_LOOPBACK,
        ocpp_validate_backend_url("ws://127.42.9.7:8080/"));
}
/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-H4-001
 * @scenario localhost hostname rejected
 */
void test_url_loopback_localhost_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_SSRF_LOOPBACK,
        ocpp_validate_backend_url("wss://localhost/ocpp"));
}
/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-H4-001
 * @scenario LOCALHOST uppercase rejected (case-insensitive host check)
 */
void test_url_loopback_localhost_uppercase_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_SSRF_LOOPBACK,
        ocpp_validate_backend_url("wss://LOCALHOST:8443/"));
}
/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-H4-001
 * @scenario 0.0.0.0 (bind-any / loopback alias) rejected
 */
void test_url_loopback_0_0_0_0_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_SSRF_LOOPBACK,
        ocpp_validate_backend_url("ws://0.0.0.0/ocpp"));
}
/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-H4-001
 * @scenario IPv6 loopback [::1] rejected
 */
void test_url_loopback_ipv6_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_SSRF_LOOPBACK,
        ocpp_validate_backend_url("wss://[::1]/ocpp"));
}
/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-H4-002
 * @scenario IPv4 link-local 169.254.x rejected (AutoIP / APIPA)
 */
void test_url_linklocal_ipv4_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_SSRF_LINK_LOCAL,
        ocpp_validate_backend_url("wss://169.254.10.20:8443/"));
}
/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-H4-002
 * @scenario IPv6 link-local fe80:: rejected
 */
void test_url_linklocal_ipv6_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_SSRF_LINK_LOCAL,
        ocpp_validate_backend_url("wss://[fe80::1]/ocpp"));
}
/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-H4-003
 * @scenario Embedded user:pass@host rejected
 */
void test_url_embedded_creds_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_EMBEDDED_CREDS,
        ocpp_validate_backend_url("wss://user:pass@evil.example/ocpp"));
}
/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-H4-003
 * @scenario Embedded @ in authority (even without colon) rejected
 */
void test_url_embedded_at_rejected(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_EMBEDDED_CREDS,
        ocpp_validate_backend_url("ws://user@evil.example/ocpp"));
}
/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-H4-004
 * @scenario @ inside path component is allowed (authority is clean)
 */
void test_url_at_in_path_allowed(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_OK,
        ocpp_validate_backend_url("wss://ocpp.tapelectric.app/path/with@sign"));
}
/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-H4-005
 * @scenario RFC1918 private ranges still allowed — many users self-host CSMS on LAN
 */
void test_url_rfc1918_private_still_allowed(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_OK,
        ocpp_validate_backend_url("ws://192.168.1.10:8080/steve/"));
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_OK,
        ocpp_validate_backend_url("ws://10.0.0.5/ocpp"));
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_OK,
        ocpp_validate_backend_url("ws://172.20.0.1/"));
}
/*
 * @feature OCPP Settings Validation
 * @req REQ-OCPP-H4-005
 * @scenario Normal public hostnames still allowed (regression-proof)
 */
void test_url_public_hostname_still_allowed(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_OK,
        ocpp_validate_backend_url("wss://ocpp.tapelectric.app/CB123"));
    TEST_ASSERT_EQUAL_INT(OCPP_VALIDATE_OK,
        ocpp_validate_backend_url("wss://csms.example.com:8443/ocpp?id=42"));
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("OCPP Settings Validation");

    RUN_TEST(test_url_valid_wss);
    RUN_TEST(test_url_valid_ws);
    RUN_TEST(test_url_http_rejected);
    RUN_TEST(test_url_https_rejected);
    RUN_TEST(test_url_empty_rejected);
    RUN_TEST(test_url_null_rejected);
    RUN_TEST(test_url_bare_scheme_rejected);
    RUN_TEST(test_url_bare_wss_scheme_rejected);
    RUN_TEST(test_url_no_scheme_rejected);
    RUN_TEST(test_url_crlf_rejected);
    RUN_TEST(test_url_space_rejected);
    RUN_TEST(test_url_valid_special_chars_accepted);
    RUN_TEST(test_url_backslash_rejected);
    RUN_TEST(test_url_braces_rejected);
    RUN_TEST(test_cbid_valid);
    RUN_TEST(test_cbid_special_chars_rejected);
    RUN_TEST(test_cbid_too_long_rejected);
    RUN_TEST(test_cbid_exactly_20_accepted);
    RUN_TEST(test_cbid_empty_rejected);
    RUN_TEST(test_cbid_ampersand_rejected);
    RUN_TEST(test_auth_key_too_long);
    RUN_TEST(test_auth_key_exactly_40_accepted);
    RUN_TEST(test_auth_key_empty_accepted);

    /* Security H-4 — SSRF hardening */
    RUN_TEST(test_url_loopback_127_0_0_1_rejected);
    RUN_TEST(test_url_loopback_127_any_rejected);
    RUN_TEST(test_url_loopback_localhost_rejected);
    RUN_TEST(test_url_loopback_localhost_uppercase_rejected);
    RUN_TEST(test_url_loopback_0_0_0_0_rejected);
    RUN_TEST(test_url_loopback_ipv6_rejected);
    RUN_TEST(test_url_linklocal_ipv4_rejected);
    RUN_TEST(test_url_linklocal_ipv6_rejected);
    RUN_TEST(test_url_embedded_creds_rejected);
    RUN_TEST(test_url_embedded_at_rejected);
    RUN_TEST(test_url_at_in_path_allowed);
    RUN_TEST(test_url_rfc1918_private_still_allowed);
    RUN_TEST(test_url_public_hostname_still_allowed);

    TEST_SUITE_RESULTS();
}
