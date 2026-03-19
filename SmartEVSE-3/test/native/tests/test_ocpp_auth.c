/*
 * test_ocpp_auth.c - OCPP authorization logic tests
 *
 * Tests the pure C authorization decision functions extracted from esp32.cpp.
 * Covers auth path selection, Access_bit set/clear decisions, and edge cases.
 */

#include "test_framework.h"
#include "ocpp_logic.h"

/* ---- Auth path selection ---- */

/*
 * @feature OCPP Authorization
 * @req REQ-OCPP-025
 * @scenario Auth path selection returns OCPP-controlled for RFIDReader=6
 * @given RFIDReader is set to 6 (Rmt/OCPP mode)
 * @when ocpp_select_auth_path is called
 * @then Returns OCPP_AUTH_PATH_OCPP_CONTROLLED because OCPP controls Access_bit
 */
void test_auth_path_rfid_reader_6(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_AUTH_PATH_OCPP_CONTROLLED, ocpp_select_auth_path(6));
}

/*
 * @feature OCPP Authorization
 * @req REQ-OCPP-026
 * @scenario Auth path selection returns OCPP-controlled for RFIDReader=0
 * @given RFIDReader is set to 0 (Disabled)
 * @when ocpp_select_auth_path is called
 * @then Returns OCPP_AUTH_PATH_OCPP_CONTROLLED because OCPP controls Access_bit when RFID is disabled
 */
void test_auth_path_rfid_reader_0(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_AUTH_PATH_OCPP_CONTROLLED, ocpp_select_auth_path(0));
}

/*
 * @feature OCPP Authorization
 * @req REQ-OCPP-027
 * @scenario Auth path selection returns builtin-RFID for RFIDReader=1..5
 * @given RFIDReader is set to 1 (built-in RFID store)
 * @when ocpp_select_auth_path is called
 * @then Returns OCPP_AUTH_PATH_BUILTIN_RFID because built-in RFID controls charging
 */
void test_auth_path_rfid_reader_1(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_AUTH_PATH_BUILTIN_RFID, ocpp_select_auth_path(1));
}

/*
 * @feature OCPP Authorization
 * @req REQ-OCPP-027
 * @scenario Auth path selection returns builtin-RFID for RFIDReader=5
 * @given RFIDReader is set to 5
 * @when ocpp_select_auth_path is called
 * @then Returns OCPP_AUTH_PATH_BUILTIN_RFID
 */
void test_auth_path_rfid_reader_5(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_AUTH_PATH_BUILTIN_RFID, ocpp_select_auth_path(5));
}

/*
 * @feature OCPP Authorization
 * @req REQ-OCPP-027
 * @scenario Auth path selection returns builtin-RFID for RFIDReader=3
 * @given RFIDReader is set to 3
 * @when ocpp_select_auth_path is called
 * @then Returns OCPP_AUTH_PATH_BUILTIN_RFID
 */
void test_auth_path_rfid_reader_3(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_AUTH_PATH_BUILTIN_RFID, ocpp_select_auth_path(3));
}

/* ---- Access_bit set decision ---- */

/*
 * @feature OCPP Authorization
 * @req REQ-OCPP-020
 * @scenario OCPP sets Access_bit on rising edge of permits_charge
 * @given Previous permits_charge was false
 * @when Current permits_charge transitions to true
 * @then ocpp_should_set_access returns true (set Access_bit ON)
 */
void test_should_set_access_on_rising_edge(void) {
    TEST_ASSERT_TRUE(ocpp_should_set_access(true, false));
}

/*
 * @feature OCPP Authorization
 * @req REQ-OCPP-022
 * @scenario OCPP does not re-set Access_bit if already permitted
 * @given Previous permits_charge was already true
 * @when Current permits_charge is still true
 * @then ocpp_should_set_access returns false (Access_bit already set once)
 */
void test_should_not_set_access_when_already_permitted(void) {
    TEST_ASSERT_FALSE(ocpp_should_set_access(true, true));
}

/*
 * @feature OCPP Authorization
 * @req REQ-OCPP-020
 * @scenario OCPP does not set Access_bit when charge not permitted
 * @given Previous permits_charge was false
 * @when Current permits_charge is still false
 * @then ocpp_should_set_access returns false
 */
void test_should_not_set_access_when_not_permitted(void) {
    TEST_ASSERT_FALSE(ocpp_should_set_access(false, false));
}

/*
 * @feature OCPP Authorization
 * @req REQ-OCPP-020
 * @scenario OCPP does not set Access_bit on falling edge
 * @given Previous permits_charge was true
 * @when Current permits_charge transitions to false
 * @then ocpp_should_set_access returns false
 */
void test_should_not_set_access_on_falling_edge(void) {
    TEST_ASSERT_FALSE(ocpp_should_set_access(false, true));
}

/* ---- Access_bit clear decision ---- */

/*
 * @feature OCPP Authorization
 * @req REQ-OCPP-021
 * @scenario OCPP clears Access_bit when permission revoked and access is ON
 * @given AccessStatus is ON (1) and permits_charge is false
 * @when ocpp_should_clear_access is called
 * @then Returns true (clear Access_bit)
 */
void test_should_clear_access_when_revoked_and_on(void) {
    TEST_ASSERT_TRUE(ocpp_should_clear_access(false, 1));  /* 1 = ON */
}

/*
 * @feature OCPP Authorization
 * @req REQ-OCPP-021
 * @scenario OCPP does not clear Access_bit when permission is still granted
 * @given AccessStatus is ON (1) and permits_charge is true
 * @when ocpp_should_clear_access is called
 * @then Returns false (Access_bit should stay)
 */
void test_should_not_clear_access_when_still_permitted(void) {
    TEST_ASSERT_FALSE(ocpp_should_clear_access(true, 1));
}

/*
 * @feature OCPP Authorization
 * @req REQ-OCPP-021
 * @scenario OCPP does not clear Access_bit when access is already OFF
 * @given AccessStatus is OFF (0) and permits_charge is false
 * @when ocpp_should_clear_access is called
 * @then Returns false (Access_bit already cleared)
 */
void test_should_not_clear_access_when_already_off(void) {
    TEST_ASSERT_FALSE(ocpp_should_clear_access(false, 0));  /* 0 = OFF */
}

/*
 * @feature OCPP Authorization
 * @req REQ-OCPP-021
 * @scenario OCPP does not clear Access_bit when access is PAUSE
 * @given AccessStatus is PAUSE (2) and permits_charge is false
 * @when ocpp_should_clear_access is called
 * @then Returns false because the clear logic only triggers on ON, not PAUSE
 */
void test_should_not_clear_access_when_paused(void) {
    TEST_ASSERT_FALSE(ocpp_should_clear_access(false, 2));  /* 2 = PAUSE */
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("OCPP Authorization Logic");

    RUN_TEST(test_auth_path_rfid_reader_6);
    RUN_TEST(test_auth_path_rfid_reader_0);
    RUN_TEST(test_auth_path_rfid_reader_1);
    RUN_TEST(test_auth_path_rfid_reader_5);
    RUN_TEST(test_auth_path_rfid_reader_3);
    RUN_TEST(test_should_set_access_on_rising_edge);
    RUN_TEST(test_should_not_set_access_when_already_permitted);
    RUN_TEST(test_should_not_set_access_when_not_permitted);
    RUN_TEST(test_should_not_set_access_on_falling_edge);
    RUN_TEST(test_should_clear_access_when_revoked_and_on);
    RUN_TEST(test_should_not_clear_access_when_still_permitted);
    RUN_TEST(test_should_not_clear_access_when_already_off);
    RUN_TEST(test_should_not_clear_access_when_paused);

    TEST_SUITE_RESULTS();
}
