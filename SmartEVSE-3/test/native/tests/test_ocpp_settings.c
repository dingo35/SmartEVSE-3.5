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
    RUN_TEST(test_cbid_valid);
    RUN_TEST(test_cbid_special_chars_rejected);
    RUN_TEST(test_cbid_too_long_rejected);
    RUN_TEST(test_cbid_exactly_20_accepted);
    RUN_TEST(test_cbid_empty_rejected);
    RUN_TEST(test_cbid_ampersand_rejected);
    RUN_TEST(test_auth_key_too_long);
    RUN_TEST(test_auth_key_exactly_40_accepted);
    RUN_TEST(test_auth_key_empty_accepted);

    TEST_SUITE_RESULTS();
}
