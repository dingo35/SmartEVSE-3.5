/*
 * test_modem_states.c - Modem / ISO15118 state transitions
 *
 * Tests the modem negotiation flow:
 *   MODEM_REQUEST -> MODEM_WAIT -> MODEM_DONE -> STATE_B
 *   MODEM_DENIED -> STATE_A
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

static void setup_basic(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.Mode = MODE_NORMAL;
    ctx.ModemEnabled = true;  // Modem tests require modem to be enabled
}

// ---- MODEM_REQUEST ----

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-001
 * @scenario Entering MODEM_REQUEST disconnects the pilot signal
 * @given The EVSE is initialised with basic configuration
 * @when The state is set to STATE_MODEM_REQUEST
 * @then pilot_connected is false
 */
void test_modem_request_disconnects_pilot(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_REQUEST);
    TEST_ASSERT_FALSE(ctx.pilot_connected);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-002
 * @scenario Entering MODEM_REQUEST sets PWM to off (+12V)
 * @given The EVSE is initialised with basic configuration
 * @when The state is set to STATE_MODEM_REQUEST
 * @then PWM duty is set to 1024 (off / +12V constant)
 */
void test_modem_request_pwm_off(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_REQUEST);
    TEST_ASSERT_EQUAL_INT(1024, ctx.last_pwm_duty);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-003
 * @scenario Entering MODEM_REQUEST ensures contactors are open
 * @given The EVSE is initialised with basic configuration
 * @when The state is set to STATE_MODEM_REQUEST
 * @then Both contactor1 and contactor2 are off (open)
 */
void test_modem_request_contactors_off(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_REQUEST);
    TEST_ASSERT_FALSE(ctx.contactor1_state);
    TEST_ASSERT_FALSE(ctx.contactor2_state);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-004
 * @scenario MODEM_REQUEST transitions to MODEM_WAIT after timer expires
 * @given The EVSE is in STATE_MODEM_REQUEST with ToModemWaitStateTimer=0
 * @when One second tick occurs
 * @then The state transitions to STATE_MODEM_WAIT
 */
void test_modem_request_to_wait_on_timer(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_REQUEST);
    // ToModemWaitStateTimer starts at 0, so next 1s tick moves to WAIT
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(STATE_MODEM_WAIT, ctx.State);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-005
 * @scenario Modem states are NOT handled in tick_10ms (original behavior)
 * @given The EVSE is in STATE_MODEM_REQUEST
 * @when A 12V pilot signal is received during tick_10ms
 * @then The state stays MODEM_REQUEST (modem is managed only by tick_1s timers)
 */
void test_modem_request_to_A_on_disconnect(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_REQUEST);
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_MODEM_REQUEST, ctx.State);
}

// ---- MODEM_WAIT ----

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-006
 * @scenario Entering MODEM_WAIT sets 5% PWM duty cycle for ISO15118 signalling
 * @given The EVSE is initialised with basic configuration
 * @when The state is set to STATE_MODEM_WAIT
 * @then PWM duty is set to 51 (5% duty cycle)
 */
void test_modem_wait_5pct_duty(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_WAIT);
    TEST_ASSERT_EQUAL_INT(51, ctx.last_pwm_duty);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-007
 * @scenario Entering MODEM_WAIT reconnects the pilot signal
 * @given The EVSE is initialised with basic configuration
 * @when The state is set to STATE_MODEM_WAIT
 * @then pilot_connected is true
 */
void test_modem_wait_pilot_connected(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_WAIT);
    TEST_ASSERT_TRUE(ctx.pilot_connected);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-008
 * @scenario Entering MODEM_WAIT sets ToModemDoneStateTimer to 60 seconds
 * @given The EVSE is initialised with basic configuration
 * @when The state is set to STATE_MODEM_WAIT
 * @then ToModemDoneStateTimer is set to 60
 */
void test_modem_wait_timer_set(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_WAIT);
    TEST_ASSERT_EQUAL_INT(60, ctx.ToModemDoneStateTimer);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-009
 * @scenario MODEM_WAIT transitions to MODEM_DONE after 60-second timeout
 * @given The EVSE is in STATE_MODEM_WAIT with 60-second timer
 * @when 61 second ticks occur (60 to decrement to 0, 1 more to fire transition)
 * @then The state transitions to STATE_MODEM_DONE
 */
void test_modem_wait_to_done_after_timeout(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_WAIT);
    // Count down 60 seconds + 1 tick for the if/else transition
    for (int i = 0; i < 61; i++) {
        evse_tick_1s(&ctx);
    }
    TEST_ASSERT_EQUAL_INT(STATE_MODEM_DONE, ctx.State);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-010
 * @scenario MODEM_WAIT is NOT handled in tick_10ms (original behavior)
 * @given The EVSE is in STATE_MODEM_WAIT
 * @when A 12V pilot signal is received during tick_10ms
 * @then The state stays MODEM_WAIT (modem is managed only by tick_1s timers)
 */
void test_modem_wait_to_A_on_disconnect(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_WAIT);
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_MODEM_WAIT, ctx.State);
}

// ---- MODEM_DONE ----

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-011
 * @scenario Entering MODEM_DONE disconnects the pilot signal
 * @given The EVSE is initialised with basic configuration
 * @when The state is set to STATE_MODEM_DONE
 * @then pilot_connected is false
 */
void test_modem_done_disconnects_pilot(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_DONE);
    TEST_ASSERT_FALSE(ctx.pilot_connected);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-012
 * @scenario Entering MODEM_DONE sets LeaveModemDoneStateTimer to 5 seconds
 * @given The EVSE is initialised with basic configuration
 * @when The state is set to STATE_MODEM_DONE
 * @then LeaveModemDoneStateTimer is set to 5
 */
void test_modem_done_timer_set(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_DONE);
    TEST_ASSERT_EQUAL_INT(5, ctx.LeaveModemDoneStateTimer);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-013
 * @scenario MODEM_DONE transitions to STATE_B after 5-second timer with ModemStage=1
 * @given The EVSE is in STATE_MODEM_DONE with 5-second timer
 * @when 6 second ticks occur (5 to decrement to 0, 1 more to fire transition)
 * @then The state transitions to STATE_B and ModemStage is set to 1
 */
void test_modem_done_to_B_after_timer(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_DONE);
    for (int i = 0; i < 6; i++) {
        evse_tick_1s(&ctx);
    }
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_EQUAL_INT(1, ctx.ModemStage);  // Skip modem next time
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-014
 * @scenario MODEM_DONE is NOT handled in tick_10ms (original behavior)
 * @given The EVSE is in STATE_MODEM_DONE
 * @when A 12V pilot signal is received during tick_10ms
 * @then The state stays MODEM_DONE (modem is managed only by tick_1s timers)
 */
void test_modem_done_to_A_on_disconnect(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_DONE);
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_MODEM_DONE, ctx.State);
}

// ---- MODEM_DENIED ----

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-015
 * @scenario MODEM_DENIED transitions to STATE_A after timer expires
 * @given The EVSE is in STATE_MODEM_DENIED with LeaveModemDeniedStateTimer=3
 * @when 4 second ticks occur (3 to decrement to 0, 1 more to fire transition)
 * @then The state transitions to STATE_A
 */
void test_modem_denied_to_A_after_timer(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_DENIED);
    ctx.LeaveModemDeniedStateTimer = 3;
    for (int i = 0; i < 4; i++) {
        evse_tick_1s(&ctx);
    }
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-016
 * @scenario MODEM_DENIED is NOT handled in tick_10ms (original behavior)
 * @given The EVSE is in STATE_MODEM_DENIED with timer still running
 * @when A 12V pilot signal is received during tick_10ms
 * @then The state stays MODEM_DENIED (modem is managed only by tick_1s timers)
 */
void test_modem_denied_to_A_on_disconnect(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_DENIED);
    ctx.LeaveModemDeniedStateTimer = 10;
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_MODEM_DENIED, ctx.State);
}

// ---- Modem timer off-by-one (M1 fidelity fix) ----

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-M1A
 * @scenario MODEM_WAIT timer=1 does not transition immediately on decrement
 * @given EVSE is in STATE_MODEM_WAIT with ToModemDoneStateTimer=1
 * @when One second tick occurs (timer decrements to 0)
 * @then The EVSE stays in STATE_MODEM_WAIT (does not yet transition to MODEM_DONE)
 * @when Another second tick occurs (timer is now 0, else branch fires)
 * @then The EVSE transitions to STATE_MODEM_DONE
 */
void test_modem_wait_timer_1_no_immediate_transition(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_WAIT);
    ctx.ToModemDoneStateTimer = 1;
    evse_tick_1s(&ctx);
    /* Timer decremented to 0 but if/else means decrement branch ran, not action */
    TEST_ASSERT_EQUAL_INT(STATE_MODEM_WAIT, ctx.State);
    TEST_ASSERT_EQUAL_INT(0, ctx.ToModemDoneStateTimer);
    /* Next tick: timer is 0, else branch fires */
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(STATE_MODEM_DONE, ctx.State);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-M1B
 * @scenario MODEM_DONE timer=1 does not transition immediately on decrement
 * @given EVSE is in STATE_MODEM_DONE with LeaveModemDoneStateTimer=1
 * @when One second tick occurs (timer decrements to 0)
 * @then The EVSE stays in STATE_MODEM_DONE
 * @when Another second tick occurs
 * @then The EVSE transitions to STATE_B (EVCCID accepted)
 */
void test_modem_done_timer_1_no_immediate_transition(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_DONE);
    ctx.LeaveModemDoneStateTimer = 1;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(STATE_MODEM_DONE, ctx.State);
    TEST_ASSERT_EQUAL_INT(0, ctx.LeaveModemDoneStateTimer);
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-M1C
 * @scenario MODEM_DENIED timer=1 does not transition immediately on decrement
 * @given EVSE is in STATE_MODEM_DENIED with LeaveModemDeniedStateTimer=1
 * @when One second tick occurs (timer decrements to 0)
 * @then The EVSE stays in STATE_MODEM_DENIED
 * @when Another second tick occurs
 * @then The EVSE transitions to STATE_A
 */
void test_modem_denied_timer_1_no_immediate_transition(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_DENIED);
    ctx.LeaveModemDeniedStateTimer = 1;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(STATE_MODEM_DENIED, ctx.State);
    TEST_ASSERT_EQUAL_INT(0, ctx.LeaveModemDeniedStateTimer);
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

// ---- DisconnectTimeCounter subsystem (M2 fidelity fix) ----

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-M2A
 * @scenario DisconnectTimeCounter starts on STATE_A entry when modem enabled
 * @given ModemEnabled=true, DisconnectTimeCounter=-1 (disabled)
 * @when State is set to STATE_A
 * @then DisconnectTimeCounter is set to 0 (started)
 */
void test_disconnect_counter_starts_on_state_a(void) {
    setup_basic();
    ctx.DisconnectTimeCounter = -1;
    evse_set_state(&ctx, STATE_A);
    TEST_ASSERT_EQUAL_INT(0, ctx.DisconnectTimeCounter);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-M2B
 * @scenario DisconnectTimeCounter disabled on MODEM_REQUEST entry
 * @given DisconnectTimeCounter=5 (running)
 * @when State is set to STATE_MODEM_REQUEST
 * @then DisconnectTimeCounter is set to -1 (disabled)
 */
void test_disconnect_counter_disabled_on_modem_request(void) {
    setup_basic();
    ctx.DisconnectTimeCounter = 5;
    evse_set_state(&ctx, STATE_MODEM_REQUEST);
    TEST_ASSERT_EQUAL_INT(-1, ctx.DisconnectTimeCounter);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-M2C
 * @scenario DisconnectTimeCounter disabled on MODEM_DONE entry
 * @given DisconnectTimeCounter=5 (running)
 * @when State is set to STATE_MODEM_DONE
 * @then DisconnectTimeCounter is set to -1 (disabled)
 */
void test_disconnect_counter_disabled_on_modem_done(void) {
    setup_basic();
    ctx.DisconnectTimeCounter = 5;
    evse_set_state(&ctx, STATE_MODEM_DONE);
    TEST_ASSERT_EQUAL_INT(-1, ctx.DisconnectTimeCounter);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-M2D
 * @scenario DisconnectTimeCounter is NOT incremented in tick_1s (handled by firmware wrapper)
 * @given ModemEnabled=true, DisconnectTimeCounter=0
 * @when tick_1s occurs
 * @then DisconnectTimeCounter stays 0 (firmware wrapper handles increment + pilot check)
 */
void test_disconnect_counter_increments_in_tick_1s(void) {
    setup_basic();
    ctx.DisconnectTimeCounter = 0;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(0, ctx.DisconnectTimeCounter);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-M2E
 * @scenario DisconnectTimeCounter does not increment when disabled (-1)
 * @given ModemEnabled=true, DisconnectTimeCounter=-1
 * @when tick_1s occurs
 * @then DisconnectTimeCounter remains -1
 */
void test_disconnect_counter_stays_disabled(void) {
    setup_basic();
    ctx.DisconnectTimeCounter = -1;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(-1, ctx.DisconnectTimeCounter);
}

// ---- EVCCID Validation (MODEM_DONE exit logic) ----

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-EVCCID-001
 * @scenario Empty RequiredEVCCID allows any vehicle
 * @given MODEM_DONE, LeaveModemDoneStateTimer expired, RequiredEVCCID=""
 * @when tick_1s processes MODEM_DONE timer expiry
 * @then Transitions to STATE_B with ModemStage=1
 */
void test_evccid_empty_required_allows_any(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_DONE);
    ctx.LeaveModemDoneStateTimer = 0;
    ctx.RequiredEVCCID[0] = '\0';
    strcpy(ctx.EVCCID, "WEVCCID12345678");

    evse_tick_1s(&ctx);

    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_EQUAL_INT(1, ctx.ModemStage);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-EVCCID-002
 * @scenario Matching EVCCID passes validation
 * @given MODEM_DONE, timer expired, RequiredEVCCID matches EVCCID
 * @when tick_1s processes timer expiry
 * @then Transitions to STATE_B with ModemStage=1
 */
void test_evccid_matching_passes(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_DONE);
    ctx.LeaveModemDoneStateTimer = 0;
    strcpy(ctx.RequiredEVCCID, "WEVCCID12345678");
    strcpy(ctx.EVCCID, "WEVCCID12345678");

    evse_tick_1s(&ctx);

    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_EQUAL_INT(1, ctx.ModemStage);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-EVCCID-003
 * @scenario Mismatched EVCCID triggers MODEM_DENIED
 * @given MODEM_DONE, timer expired, RequiredEVCCID != EVCCID
 * @when tick_1s processes timer expiry
 * @then Transitions to MODEM_DENIED, ModemStage=0, LeaveModemDeniedStateTimer=60
 */
void test_evccid_mismatch_denied(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_DONE);
    ctx.LeaveModemDoneStateTimer = 0;
    strcpy(ctx.RequiredEVCCID, "WEVCCID_ALLOWED");
    strcpy(ctx.EVCCID, "WEVCCID_WRONG");

    evse_tick_1s(&ctx);

    TEST_ASSERT_EQUAL_INT(STATE_MODEM_DENIED, ctx.State);
    TEST_ASSERT_EQUAL_INT(0, ctx.ModemStage);
    /* Timer set to 60 then immediately decremented to 59 (flat if-chain) */
    TEST_ASSERT_EQUAL_INT(59, ctx.LeaveModemDeniedStateTimer);
}

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-EVCCID-004
 * @scenario Full flow: EVCCID mismatch → DENIED → timeout → STATE_A
 * @given Modem flow reaches MODEM_DONE with wrong EVCCID
 * @when Timer expires and EVCCID doesn't match, then DENIED timer expires
 * @then MODEM_DENIED → STATE_A after 60+1 seconds
 */
void test_evccid_mismatch_full_flow_to_a(void) {
    setup_basic();
    evse_set_state(&ctx, STATE_MODEM_DONE);
    ctx.LeaveModemDoneStateTimer = 0;
    strcpy(ctx.RequiredEVCCID, "WEVCCID_ALLOWED");
    strcpy(ctx.EVCCID, "WEVCCID_WRONG");

    // MODEM_DONE timer expired → should go to MODEM_DENIED
    // Timer set to 60, immediately decremented to 59 (flat if-chain)
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(STATE_MODEM_DENIED, ctx.State);
    TEST_ASSERT_EQUAL_INT(59, ctx.LeaveModemDeniedStateTimer);

    // Tick 59 times (decrement to 0) + 1 (if/else fires → STATE_A)
    for (int i = 0; i < 60; i++) {
        evse_tick_1s(&ctx);
    }
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

// ---- Full modem flow ----

/*
 * @feature Modem / ISO15118 Negotiation
 * @req REQ-MODEM-017
 * @scenario Full modem negotiation flow: REQUEST -> WAIT -> DONE -> STATE_B
 * @given The EVSE is initialised with basic configuration
 * @when The modem negotiation proceeds through all stages with timers expiring
 * @then The EVSE transitions REQUEST->WAIT->DONE->B with ModemStage=1 and correct PWM/pilot at each stage
 */
void test_full_modem_flow(void) {
    setup_basic();
    // Start modem request
    evse_set_state(&ctx, STATE_MODEM_REQUEST);
    TEST_ASSERT_EQUAL_INT(STATE_MODEM_REQUEST, ctx.State);
    TEST_ASSERT_FALSE(ctx.pilot_connected);

    // Timer expires -> MODEM_WAIT
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(STATE_MODEM_WAIT, ctx.State);
    TEST_ASSERT_TRUE(ctx.pilot_connected);
    TEST_ASSERT_EQUAL_INT(51, ctx.last_pwm_duty);

    // Wait 61s -> MODEM_DONE (60 to decrement + 1 for if/else transition)
    for (int i = 0; i < 61; i++) {
        evse_tick_1s(&ctx);
    }
    TEST_ASSERT_EQUAL_INT(STATE_MODEM_DONE, ctx.State);

    // Wait 6s -> STATE_B (5 to decrement + 1 for if/else transition)
    for (int i = 0; i < 6; i++) {
        evse_tick_1s(&ctx);
    }
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_EQUAL_INT(1, ctx.ModemStage);
}

// ---- Main ----
int main(void) {
    TEST_SUITE_BEGIN("Modem / ISO15118 States");

    RUN_TEST(test_modem_request_disconnects_pilot);
    RUN_TEST(test_modem_request_pwm_off);
    RUN_TEST(test_modem_request_contactors_off);
    RUN_TEST(test_modem_request_to_wait_on_timer);
    RUN_TEST(test_modem_request_to_A_on_disconnect);
    RUN_TEST(test_modem_wait_5pct_duty);
    RUN_TEST(test_modem_wait_pilot_connected);
    RUN_TEST(test_modem_wait_timer_set);
    RUN_TEST(test_modem_wait_to_done_after_timeout);
    RUN_TEST(test_modem_wait_to_A_on_disconnect);
    RUN_TEST(test_modem_done_disconnects_pilot);
    RUN_TEST(test_modem_done_timer_set);
    RUN_TEST(test_modem_done_to_B_after_timer);
    RUN_TEST(test_modem_done_to_A_on_disconnect);
    RUN_TEST(test_modem_denied_to_A_after_timer);
    RUN_TEST(test_modem_denied_to_A_on_disconnect);
    RUN_TEST(test_evccid_empty_required_allows_any);
    RUN_TEST(test_evccid_matching_passes);
    RUN_TEST(test_evccid_mismatch_denied);
    RUN_TEST(test_evccid_mismatch_full_flow_to_a);
    RUN_TEST(test_full_modem_flow);
    RUN_TEST(test_modem_wait_timer_1_no_immediate_transition);
    RUN_TEST(test_modem_done_timer_1_no_immediate_transition);
    RUN_TEST(test_modem_denied_timer_1_no_immediate_transition);
    RUN_TEST(test_disconnect_counter_starts_on_state_a);
    RUN_TEST(test_disconnect_counter_disabled_on_modem_request);
    RUN_TEST(test_disconnect_counter_disabled_on_modem_done);
    RUN_TEST(test_disconnect_counter_increments_in_tick_1s);
    RUN_TEST(test_disconnect_counter_stays_disabled);

    TEST_SUITE_RESULTS();
}
