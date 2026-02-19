/*
 * test_authorization.c - RFID / OCPP / Switch access control
 *
 * Tests the authorization gate:
 *   - AccessStatus (ON/OFF/PAUSE) gating transitions
 *   - setAccess() side effects (stop charging when access removed)
 *   - OCPP current limit blocking
 *   - RFID lock timer
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

static void setup_basic(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;
    ctx.ChargeCurrent = 130;
}

// ---- setAccess tests ----

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-001
 * @scenario Setting access to ON stores the authorization status
 * @given The EVSE is initialised in basic configuration
 * @when evse_set_access is called with ON
 * @then AccessStatus is set to ON
 */
void test_set_access_on(void) {
    setup_basic();
    evse_set_access(&ctx, ON);
    TEST_ASSERT_EQUAL_INT(ON, ctx.AccessStatus);
}

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-002
 * @scenario Setting access to OFF stores the authorization status
 * @given The EVSE is initialised in basic configuration
 * @when evse_set_access is called with OFF
 * @then AccessStatus is set to OFF
 */
void test_set_access_off(void) {
    setup_basic();
    evse_set_access(&ctx, OFF);
    TEST_ASSERT_EQUAL_INT(OFF, ctx.AccessStatus);
}

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-003
 * @scenario Revoking access during charging suspends the session (C -> C1)
 * @given The EVSE is in STATE_C (charging) with AccessStatus ON
 * @when evse_set_access is called with OFF
 * @then The state transitions to STATE_C1 and AccessStatus is OFF
 */
void test_set_access_off_from_C_goes_C1(void) {
    setup_basic();
    ctx.AccessStatus = ON;
    evse_set_state(&ctx, STATE_C);
    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.State);

    evse_set_access(&ctx, OFF);
    TEST_ASSERT_EQUAL_INT(STATE_C1, ctx.State);
    TEST_ASSERT_EQUAL_INT(OFF, ctx.AccessStatus);
}

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-004
 * @scenario Pausing access during charging suspends the session (C -> C1)
 * @given The EVSE is in STATE_C (charging) with AccessStatus ON
 * @when evse_set_access is called with PAUSE
 * @then The state transitions to STATE_C1 and AccessStatus is PAUSE
 */
void test_set_access_pause_from_C_goes_C1(void) {
    setup_basic();
    ctx.AccessStatus = ON;
    evse_set_state(&ctx, STATE_C);

    evse_set_access(&ctx, PAUSE);
    TEST_ASSERT_EQUAL_INT(STATE_C1, ctx.State);
    TEST_ASSERT_EQUAL_INT(PAUSE, ctx.AccessStatus);
}

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-005
 * @scenario Revoking access in STATE_B moves to waiting state (B -> B1)
 * @given The EVSE is in STATE_B (connected) with AccessStatus ON
 * @when evse_set_access is called with OFF
 * @then The state transitions to STATE_B1 (waiting)
 */
void test_set_access_off_from_B_goes_B1(void) {
    setup_basic();
    ctx.AccessStatus = ON;
    evse_set_state(&ctx, STATE_B);

    evse_set_access(&ctx, OFF);
    TEST_ASSERT_EQUAL_INT(STATE_B1, ctx.State);
}

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-006
 * @scenario Revoking access during modem request aborts to B1
 * @given The EVSE is in STATE_MODEM_REQUEST with AccessStatus ON
 * @when evse_set_access is called with OFF
 * @then The state transitions to STATE_B1 (waiting)
 */
void test_set_access_off_from_modem_request_goes_B1(void) {
    setup_basic();
    ctx.AccessStatus = ON;
    evse_set_state(&ctx, STATE_MODEM_REQUEST);

    evse_set_access(&ctx, OFF);
    TEST_ASSERT_EQUAL_INT(STATE_B1, ctx.State);
}

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-007
 * @scenario Revoking access during modem wait aborts to B1
 * @given The EVSE is in STATE_MODEM_WAIT with AccessStatus ON
 * @when evse_set_access is called with OFF
 * @then The state transitions to STATE_B1 (waiting)
 */
void test_set_access_off_from_modem_wait_goes_B1(void) {
    setup_basic();
    ctx.AccessStatus = ON;
    evse_set_state(&ctx, STATE_MODEM_WAIT);

    evse_set_access(&ctx, OFF);
    TEST_ASSERT_EQUAL_INT(STATE_B1, ctx.State);
}

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-008
 * @scenario Revoking access in STATE_A has no state transition side effect
 * @given The EVSE is in STATE_A (disconnected)
 * @when evse_set_access is called with OFF
 * @then The state remains STATE_A
 */
void test_set_access_off_from_A_stays_A(void) {
    setup_basic();
    ctx.State = STATE_A;
    evse_set_access(&ctx, OFF);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-009
 * @scenario Granting access in STATE_B1 does not auto-recover to STATE_B
 * @given The EVSE is in STATE_B1 (waiting)
 * @when evse_set_access is called with ON
 * @then The state remains STATE_B1 (no automatic recovery)
 */
void test_set_access_on_from_B1_does_not_auto_recover(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_B1);
    evse_set_access(&ctx, ON);
    // setAccess(ON) doesn't auto-recover state
    TEST_ASSERT_EQUAL_INT(STATE_B1, ctx.State);
}

// ---- OCPP authorization gating ----

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-010
 * @scenario OCPP current limit below MinCurrent blocks current availability
 * @given OCPP mode is enabled with OcppCurrentLimit=3A and MinCurrent=6A
 * @when evse_is_current_available is called
 * @then The function returns 0 (current not available)
 */
void test_ocpp_blocks_current_availability(void) {
    setup_basic();
    ctx.OcppMode = true;
    ctx.OcppCurrentLimit = 3.0f;  // Below MinCurrent (6A)
    ctx.MinCurrent = 6;
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-011
 * @scenario OCPP current limit above MinCurrent allows current availability
 * @given OCPP mode is enabled with OcppCurrentLimit=10A and MinCurrent=6A
 * @when evse_is_current_available is called
 * @then The function returns 1 (current available)
 */
void test_ocpp_allows_when_limit_sufficient(void) {
    setup_basic();
    ctx.OcppMode = true;
    ctx.OcppCurrentLimit = 10.0f;  // Above MinCurrent
    ctx.MinCurrent = 6;
    ctx.AccessStatus = ON;
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(1, result);
}

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-012
 * @scenario Negative OCPP current limit is ignored (not set)
 * @given OCPP mode is enabled with OcppCurrentLimit=-1 (unset)
 * @when evse_is_current_available is called
 * @then The function returns 1 (limit not applied)
 */
void test_ocpp_negative_limit_ignored(void) {
    setup_basic();
    ctx.OcppMode = true;
    ctx.OcppCurrentLimit = -1.0f;  // Not set
    ctx.MinCurrent = 6;
    int result = evse_is_current_available(&ctx);
    TEST_ASSERT_EQUAL_INT(1, result);
}

// ---- RFID lock timer ----

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-013
 * @scenario RFID lock timer starts when EV disconnects in STATE_A
 * @given The EVSE has AccessStatus ON, RFIDReader=2, and AccessTimer=0
 * @when A 12V pilot signal is received (EV disconnects)
 * @then AccessTimer is set to RFIDLOCKTIME
 */
// Original line 3090: AccessTimer starts on PILOT_12V (disconnect), not on connect
void test_access_timer_starts_on_rfid(void) {
    setup_basic();
    ctx.AccessStatus = ON;
    ctx.RFIDReader = 2;
    ctx.AccessTimer = 0;

    // AccessTimer starts when EV disconnects (12V in STATE_A)
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(RFIDLOCKTIME, ctx.AccessTimer);
}

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-014
 * @scenario Access timer counts down each second while in STATE_A
 * @given The EVSE is in STATE_A with AccessTimer=5
 * @when One second tick occurs
 * @then AccessTimer decrements to 4
 */
void test_access_timer_counts_down_in_state_A(void) {
    setup_basic();
    ctx.State = STATE_A;
    ctx.AccessStatus = ON;
    ctx.AccessTimer = 5;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(4, ctx.AccessTimer);
}

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-015
 * @scenario Access timer expiry revokes authorization
 * @given The EVSE is in STATE_A with AccessTimer=1 and AccessStatus ON
 * @when One second tick occurs (timer reaches 0)
 * @then AccessStatus is set to OFF (authorization revoked)
 */
void test_access_timer_expires_turns_off(void) {
    setup_basic();
    ctx.State = STATE_A;
    ctx.AccessStatus = ON;
    ctx.AccessTimer = 1;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(0, ctx.AccessTimer);
    TEST_ASSERT_EQUAL_INT(OFF, ctx.AccessStatus);
}

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-016
 * @scenario Access timer is cleared when EVSE is not in STATE_A
 * @given The EVSE is in STATE_B with AccessTimer=30
 * @when One second tick occurs
 * @then AccessTimer is reset to 0
 */
void test_access_timer_cleared_when_not_in_A(void) {
    setup_basic();
    ctx.State = STATE_B;
    ctx.BalancedState[0] = STATE_B;
    ctx.AccessTimer = 30;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(0, ctx.AccessTimer);
}

// ---- Access blocks state transition ----

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-017
 * @scenario No STATE_A to STATE_B transition without authorization
 * @given The EVSE is in STATE_A with AccessStatus OFF
 * @when A 9V pilot signal is received (vehicle connected)
 * @then The state remains STATE_A (transition blocked)
 */
void test_no_A_to_B_without_access(void) {
    setup_basic();
    ctx.AccessStatus = OFF;
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

/*
 * @feature Authorization & Access Control
 * @req REQ-AUTH-018
 * @scenario No STATE_B to STATE_C transition after access revoked mid-session
 * @given The EVSE is in STATE_B with DiodeCheck passed but AccessStatus revoked to OFF
 * @when A 6V pilot signal is sustained for 500ms
 * @then The state does NOT transition to STATE_C
 */
void test_no_B_to_C_without_access(void) {
    setup_basic();
    ctx.AccessStatus = ON;
    evse_set_state(&ctx, STATE_B);
    ctx.DiodeCheck = 1;
    ctx.ChargeCurrent = ctx.MaxCurrent * 10;
    ctx.AccessStatus = OFF;  // Revoke access

    for (int i = 0; i < 55; i++) {
        evse_tick_10ms(&ctx, PILOT_6V);
    }
    TEST_ASSERT_NOT_EQUAL(STATE_C, ctx.State);
}

// ---- Main ----
int main(void) {
    TEST_SUITE_BEGIN("Authorization");

    RUN_TEST(test_set_access_on);
    RUN_TEST(test_set_access_off);
    RUN_TEST(test_set_access_off_from_C_goes_C1);
    RUN_TEST(test_set_access_pause_from_C_goes_C1);
    RUN_TEST(test_set_access_off_from_B_goes_B1);
    RUN_TEST(test_set_access_off_from_modem_request_goes_B1);
    RUN_TEST(test_set_access_off_from_modem_wait_goes_B1);
    RUN_TEST(test_set_access_off_from_A_stays_A);
    RUN_TEST(test_set_access_on_from_B1_does_not_auto_recover);
    RUN_TEST(test_ocpp_blocks_current_availability);
    RUN_TEST(test_ocpp_allows_when_limit_sufficient);
    RUN_TEST(test_ocpp_negative_limit_ignored);
    RUN_TEST(test_access_timer_starts_on_rfid);
    RUN_TEST(test_access_timer_counts_down_in_state_A);
    RUN_TEST(test_access_timer_expires_turns_off);
    RUN_TEST(test_access_timer_cleared_when_not_in_A);
    RUN_TEST(test_no_A_to_B_without_access);
    RUN_TEST(test_no_B_to_C_without_access);

    TEST_SUITE_RESULTS();
}
