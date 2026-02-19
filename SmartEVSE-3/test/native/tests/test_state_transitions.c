/*
 * test_state_transitions.c - Core pilot-driven state machine transitions
 *
 * Tests the fundamental IEC 61851-1 state transitions:
 *   STATE_A (disconnected) -> STATE_B (connected) -> STATE_C (charging)
 *   and all intermediate / error paths.
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

// ---- helpers ----
static void setup_idle(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
}

// Setup a context that's ready to transition from A->B
static void setup_ready_to_charge(void) {
    setup_idle();
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;           // Standalone
    ctx.ChargeCurrent = 100;  // 10A
    ctx.ModemStage = 1;       // Skip modem negotiation (already authenticated)
}

// ---- Tests ----

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-001
 * @scenario EVSE initialises to disconnected state
 * @given The EVSE is powered on
 * @when evse_init() is called
 * @then The state machine starts in STATE_A (disconnected)
 */
void test_init_state_is_A(void) {
    evse_init(&ctx, NULL);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-002
 * @scenario Contactors are open after initialisation
 * @given The EVSE is powered on
 * @when evse_init() is called
 * @then Both contactor1 and contactor2 are off (open)
 */
void test_init_contactors_off(void) {
    evse_init(&ctx, NULL);
    TEST_ASSERT_FALSE(ctx.contactor1_state);
    TEST_ASSERT_FALSE(ctx.contactor2_state);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-003
 * @scenario Pilot signal is connected after initialisation
 * @given The EVSE is powered on
 * @when evse_init() is called
 * @then The pilot signal is connected (pilot_connected is true)
 */
void test_init_pilot_connected(void) {
    evse_init(&ctx, NULL);
    TEST_ASSERT_TRUE(ctx.pilot_connected);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-004
 * @scenario No error flags after initialisation
 * @given The EVSE is powered on
 * @when evse_init() is called
 * @then ErrorFlags is NO_ERROR
 */
void test_init_no_errors(void) {
    evse_init(&ctx, NULL);
    TEST_ASSERT_EQUAL_INT(NO_ERROR, ctx.ErrorFlags);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-005
 * @scenario STATE_A remains when no vehicle is connected
 * @given The EVSE is in STATE_A (disconnected) and ready to charge
 * @when A 12V pilot signal is received (no vehicle present)
 * @then The state remains STATE_A
 */
void test_A_stays_A_on_12V(void) {
    setup_ready_to_charge();
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-006
 * @scenario Vehicle connection triggers STATE_A to STATE_B transition
 * @given The EVSE is in STATE_A, authorized, and ready to charge
 * @when A 9V pilot signal is received (vehicle connected)
 * @then The state transitions to STATE_B (connected, not charging)
 */
void test_A_to_B_on_9V_when_ready(void) {
    setup_ready_to_charge();
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-007
 * @scenario Modem negotiation required before STATE_B when ModemStage is 0
 * @given The EVSE is in STATE_A with ModemStage=0 (unauthenticated modem)
 * @when A 9V pilot signal is received (vehicle connected)
 * @then The state transitions to STATE_MODEM_REQUEST for ISO15118 negotiation
 */
void test_A_to_modem_when_modem_stage_0(void) {
    setup_idle();
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;
    ctx.ChargeCurrent = 100;
    ctx.ModemEnabled = true;  // Modem must be enabled for modem flow
    ctx.ModemStage = 0;  // Modem not yet authenticated
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_MODEM_REQUEST, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-007B
 * @scenario A→B goes directly to STATE_B when modem is disabled
 * @given The EVSE is in STATE_A with ModemStage=0 but ModemEnabled=false
 * @when A 9V pilot signal is received (vehicle connected)
 * @then The state transitions directly to STATE_B (modem flow skipped)
 */
void test_A_to_B_skips_modem_when_disabled(void) {
    setup_idle();
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;
    ctx.ChargeCurrent = 100;
    ctx.ModemEnabled = false;  // Modem disabled (default)
    ctx.ModemStage = 0;
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-008
 * @scenario Unauthorized EVSE blocks STATE_A to STATE_B transition
 * @given The EVSE is in STATE_A with AccessStatus OFF (not authorized)
 * @when A 9V pilot signal is received (vehicle connected)
 * @then The state remains STATE_A (transition blocked)
 */
void test_A_stays_A_when_access_off(void) {
    setup_ready_to_charge();
    ctx.AccessStatus = OFF;
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-009
 * @scenario Active errors divert vehicle connection to STATE_B1 (waiting)
 * @given The EVSE is in STATE_A with an active TEMP_HIGH error flag
 * @when A 9V pilot signal is received (vehicle connected)
 * @then The state transitions to STATE_B1 (connected, waiting due to error)
 */
// Original line 3127-3128: with errors, 9V pilot triggers transition to B1
void test_A_to_B1_when_errors(void) {
    setup_ready_to_charge();
    ctx.ErrorFlags = TEMP_HIGH;
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B1, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-010
 * @scenario Active ChargeDelay diverts vehicle connection to STATE_B1
 * @given The EVSE is in STATE_A with ChargeDelay > 0
 * @when A 9V pilot signal is received (vehicle connected)
 * @then The state transitions to STATE_B1 (connected, waiting for delay)
 */
// Original line 3127-3128: with ChargeDelay, 9V pilot triggers transition to B1
void test_A_to_B1_when_charge_delay(void) {
    setup_ready_to_charge();
    ctx.ChargeDelay = 10;
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B1, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-011
 * @scenario Vehicle disconnect in STATE_B returns to STATE_A
 * @given The EVSE is in STATE_B (vehicle connected, not charging)
 * @when A 12V pilot signal is received (vehicle disconnected)
 * @then The state transitions back to STATE_A (disconnected)
 */
void test_B_to_A_on_disconnect(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_B);
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-012
 * @scenario Vehicle charge request after diode check triggers STATE_B to STATE_C
 * @given The EVSE is in STATE_B with DiodeCheck passed and sufficient current
 * @when A 6V pilot signal is sustained for 500ms (vehicle requests charge)
 * @then The state transitions to STATE_C (charging)
 */
void test_B_to_C_on_6V_with_diode_check(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_B);
    ctx.DiodeCheck = 1;
    ctx.ChargeCurrent = ctx.MaxCurrent * 10;

    // Need 500ms of 6V (>50 ticks at 10ms)
    for (int i = 0; i < 55; i++) {
        evse_tick_10ms(&ctx, PILOT_6V);
    }
    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-013
 * @scenario Charge request without diode check does not transition to STATE_C
 * @given The EVSE is in STATE_B with DiodeCheck NOT passed
 * @when A 6V pilot signal is sustained for 500ms
 * @then The state does NOT transition to STATE_C
 */
void test_B_to_C_requires_diode_check(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_B);
    ctx.DiodeCheck = 0;  // No diode check yet
    ctx.ChargeCurrent = ctx.MaxCurrent * 10;

    for (int i = 0; i < 55; i++) {
        evse_tick_10ms(&ctx, PILOT_6V);
    }
    // Should NOT reach STATE_C without diode check
    TEST_ASSERT_NOT_EQUAL(STATE_C, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-014
 * @scenario PILOT_DIODE signal sets DiodeCheck flag
 * @given The EVSE is in STATE_B with DiodeCheck=0
 * @when A PILOT_DIODE signal is received
 * @then DiodeCheck is set to 1
 */
void test_diode_check_sets_on_pilot_diode(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_B);
    ctx.DiodeCheck = 0;
    evse_tick_10ms(&ctx, PILOT_DIODE);
    TEST_ASSERT_EQUAL_INT(1, ctx.DiodeCheck);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-015
 * @scenario Contactor 1 is closed when entering STATE_C
 * @given The EVSE transitions to STATE_C (charging)
 * @when evse_set_state is called with STATE_C
 * @then contactor1_state is true (closed, power flowing)
 */
void test_C_contactor1_on(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_C);
    TEST_ASSERT_TRUE(ctx.contactor1_state);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-016
 * @scenario Vehicle disconnect during charging returns to STATE_A with contactors open
 * @given The EVSE is in STATE_C (charging)
 * @when A 12V pilot signal is received (vehicle disconnected)
 * @then The state transitions to STATE_A and contactor1 is opened
 */
void test_C_to_A_on_disconnect(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_C);
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
    TEST_ASSERT_FALSE(ctx.contactor1_state);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-017
 * @scenario Vehicle stops charging request, transitions from STATE_C to STATE_B
 * @given The EVSE is in STATE_C (charging)
 * @when A 9V pilot signal is received (vehicle stops charge request)
 * @then The state transitions to STATE_B and DiodeCheck is reset to 0
 */
// Original line 3243-3244: STATE_C + 9V → STATE_B (not STATE_C1)
void test_C_to_B_on_9V(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_C);
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_EQUAL_INT(0, ctx.DiodeCheck);  // DiodeCheck reset (line 3245)
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-018
 * @scenario Shorted pilot during charging transitions to STATE_B after debounce
 * @given The EVSE is in STATE_C (charging)
 * @when PILOT_SHORT is sustained for more than 500ms (debounce period)
 * @then The state transitions to STATE_B
 */
// PILOT_SHORT in STATE_C: debounce 500ms then go to STATE_B
void test_C_to_B_on_pilot_short(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_C);
    // Need >50 ticks of PILOT_SHORT (500ms debounce)
    for (int i = 0; i < 55; i++) {
        evse_tick_10ms(&ctx, PILOT_SHORT);
    }
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-019
 * @scenario Vehicle disconnect from STATE_C1 returns to STATE_A
 * @given The EVSE is in STATE_C1 (charging suspended)
 * @when A 12V pilot signal is received (vehicle disconnected)
 * @then The state transitions to STATE_A
 */
void test_C1_to_A_on_disconnect(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_C1);
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-020
 * @scenario STATE_C1 transitions to STATE_B1 on 9V pilot (not STATE_B)
 * @given The EVSE is in STATE_C1 (charging suspended)
 * @when A 9V pilot signal is received
 * @then The state transitions to STATE_B1 (waiting), not STATE_B
 */
// Original line 3212: STATE_C1 + 9V → STATE_B1 (not STATE_B)
void test_C1_to_B1_on_9V(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_C1);
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B1, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-021
 * @scenario C1Timer expiry transitions STATE_C1 to STATE_B1 with contactors open
 * @given The EVSE is in STATE_C1 with C1Timer set to 6 seconds
 * @when The C1Timer counts down to zero via tick_1s
 * @then The state transitions to STATE_B1 and both contactors are opened
 */
// C1Timer countdown is in tick_1s (original lines 1616-1625)
void test_C1_timer_transitions_to_B1(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_C1);
    // C1Timer is set to 6 on entry
    TEST_ASSERT_EQUAL_INT(6, ctx.C1Timer);
    // Tick 1s 7 times to expire timer (6 decrements + 1 to trigger)
    for (int i = 0; i < 7; i++) {
        evse_tick_1s(&ctx);
    }
    TEST_ASSERT_EQUAL_INT(STATE_B1, ctx.State);
    TEST_ASSERT_FALSE(ctx.contactor1_state);
    TEST_ASSERT_FALSE(ctx.contactor2_state);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-022
 * @scenario Vehicle disconnect from STATE_B1 returns to STATE_A
 * @given The EVSE is in STATE_B1 (waiting) with pilot reconnected
 * @when A 12V pilot signal is received (vehicle disconnected)
 * @then The state transitions to STATE_A
 */
void test_B1_to_A_on_disconnect(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_B1);
    // B1 entry sets PilotDisconnected=true when AccessStatus=ON
    // Must wait for PilotDisconnectTime to clear before pilot is read
    ctx.PilotDisconnected = false;
    ctx.PilotDisconnectTime = 0;
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-023
 * @scenario Entering STATE_B1 sets a non-zero ChargeDelay
 * @given The EVSE is ready to charge with ChargeDelay=0
 * @when The state is set to STATE_B1
 * @then ChargeDelay is set to a value greater than 0
 */
void test_set_state_B1_sets_charge_delay(void) {
    setup_ready_to_charge();
    ctx.ChargeDelay = 0;
    evse_set_state(&ctx, STATE_B1);
    TEST_ASSERT_GREATER_THAN(0, ctx.ChargeDelay);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-024
 * @scenario Entering STATE_A clears LESS_6A error and ChargeDelay
 * @given The EVSE has LESS_6A error flag set and ChargeDelay > 0
 * @when The state is set to STATE_A
 * @then LESS_6A is cleared from ErrorFlags and ChargeDelay is set to 0
 */
void test_set_state_A_clears_errors_and_delay(void) {
    setup_ready_to_charge();
    ctx.ErrorFlags = LESS_6A;
    ctx.ChargeDelay = 10;
    evse_set_state(&ctx, STATE_A);
    TEST_ASSERT_EQUAL_INT(0, ctx.ErrorFlags & LESS_6A);
    TEST_ASSERT_EQUAL_INT(0, ctx.ChargeDelay);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-025
 * @scenario State transitions are recorded in the transition log
 * @given The EVSE is ready to charge
 * @when Two state transitions occur (STATE_B then STATE_C)
 * @then transition_count is 2 and the log contains both states in order
 */
void test_transition_log_records_states(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_B);
    evse_set_state(&ctx, STATE_C);
    TEST_ASSERT_EQUAL_INT(2, ctx.transition_count);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.transition_log[0]);
    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.transition_log[1]);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-026
 * @scenario Entering STATE_C1 sets PWM to off (+12V)
 * @given The EVSE is in STATE_C (charging)
 * @when The state is set to STATE_C1 (charging suspended)
 * @then PWM duty is set to 1024 (off / +12V constant)
 */
void test_set_state_C1_sets_pwm_off(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_C);
    evse_set_state(&ctx, STATE_C1);
    TEST_ASSERT_EQUAL_INT(1024, ctx.last_pwm_duty);  // PWM off = +12V
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-027
 * @scenario Full charge cycle: A -> B -> C -> B -> A
 * @given The EVSE is in STATE_A, authorized and ready to charge
 * @when The vehicle connects (9V), requests charge (6V), stops (9V), disconnects (12V)
 * @then The EVSE transitions A->B->C->B->A with correct contactor states
 */
void test_full_charge_cycle(void) {
    setup_ready_to_charge();
    // A: vehicle not connected
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);

    // Vehicle plugs in -> 9V pilot
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_FALSE(ctx.contactor1_state);

    // Vehicle requests charge -> 6V after diode check
    ctx.DiodeCheck = 1;
    ctx.ChargeCurrent = ctx.MaxCurrent * 10;
    for (int i = 0; i < 55; i++) {
        evse_tick_10ms(&ctx, PILOT_6V);
    }
    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.State);
    TEST_ASSERT_TRUE(ctx.contactor1_state);

    // Vehicle stops -> 9V goes back to STATE_B (original line 3244)
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);

    // Vehicle disconnects -> 12V
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
    TEST_ASSERT_FALSE(ctx.contactor1_state);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-028
 * @scenario Vehicle disconnect during ACTSTART is NOT handled in tick_10ms
 * @given The EVSE is in STATE_ACTSTART (activation mode)
 * @when A 12V pilot signal is received (vehicle disconnected)
 * @then The state stays ACTSTART (original behavior: no pilot check in ACTSTART,
 *       timer expires → STATE_B → next tick detects 12V → STATE_A)
 */
void test_actstart_to_A_on_disconnect(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_ACTSTART);
    ctx.ActivationTimer = 3;  // Timer still running
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_ACTSTART, ctx.State);  // Stays in ACTSTART
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-029
 * @scenario ActivationMode=0 triggers STATE_ACTSTART on pilot detection in STATE_B
 * @given The EVSE is in STATE_B with ActivationMode=0
 * @when A 9V pilot signal is received
 * @then The state transitions to STATE_ACTSTART with ActivationTimer set to 3
 */
void test_activation_mode_triggers_actstart(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_B);
    ctx.ActivationMode = 0;
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_ACTSTART, ctx.State);
    TEST_ASSERT_EQUAL_INT(3, ctx.ActivationTimer);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-030
 * @scenario ActivationMode counts down each second
 * @given The EVSE has ActivationMode set to 5
 * @when One second tick occurs
 * @then ActivationMode decrements to 4
 */
// ActivationMode countdown and ActivationTimer in tick_1s
void test_activation_mode_countdown(void) {
    evse_init(&ctx, NULL);
    ctx.ActivationMode = 5;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(4, ctx.ActivationMode);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-031
 * @scenario ActivationMode=255 (always active) does not count down
 * @given The EVSE has ActivationMode set to 255
 * @when One second tick occurs
 * @then ActivationMode remains at 255
 */
void test_activation_mode_255_does_not_countdown(void) {
    evse_init(&ctx, NULL);
    ctx.ActivationMode = 255;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(255, ctx.ActivationMode);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-032
 * @scenario STATE_ACTSTART returns to STATE_B when ActivationTimer expires
 * @given The EVSE is in STATE_ACTSTART with ActivationTimer=0 (expired)
 * @when A non-12V pilot signal is received
 * @then The state transitions to STATE_B and ActivationMode is set to 255
 */
void test_actstart_returns_to_B_when_timer_expires(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_ACTSTART);
    ctx.ActivationTimer = 0;
    evse_tick_10ms(&ctx, PILOT_9V);  // Non-12V pilot, timer expired
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_EQUAL_INT(255, ctx.ActivationMode);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-033
 * @scenario COMM_B_OK state transitions to STATE_B with ActivationMode set
 * @given The EVSE is in STATE_COMM_B_OK (master approved B transition)
 * @when A 9V pilot signal is received
 * @then The state transitions to STATE_B and ActivationMode is set to 30
 */
// COMM_B_OK handler
void test_comm_b_ok_transitions_to_B(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.State = STATE_COMM_B_OK;
    ctx.BalancedState[0] = STATE_COMM_B_OK;
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_EQUAL_INT(30, ctx.ActivationMode);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-034
 * @scenario COMM_C_OK state transitions to STATE_C
 * @given The EVSE is in STATE_COMM_C_OK (master approved C transition)
 * @when A 6V pilot signal is received
 * @then The state transitions to STATE_C (charging)
 */
// COMM_C_OK handler
void test_comm_c_ok_transitions_to_C(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
    ctx.State = STATE_COMM_C_OK;
    ctx.BalancedState[0] = STATE_COMM_C_OK;
    evse_tick_10ms(&ctx, PILOT_6V);
    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-035
 * @scenario Node EVSE (LoadBl >= 2) sends COMM_B instead of transitioning directly to STATE_B
 * @given The EVSE is configured as a node (LoadBl=2)
 * @when A 9V pilot signal is received (vehicle connected)
 * @then The state transitions to STATE_COMM_B (requesting master permission)
 */
// Node sends COMM_B when LoadBl > 1
void test_node_sends_comm_b(void) {
    setup_idle();
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 2;  // Node
    ctx.ModemStage = 1;
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_COMM_B, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-PHASE-001
 * @scenario STATE_B entry from STATE_A sets Nr_Of_Phases directly via CheckSwitchingPhases
 * @given The EVSE is in STATE_A with EnableC2=ALWAYS_OFF and 3 phases configured
 * @when The state is set to STATE_B
 * @then Nr_Of_Phases_Charging is set directly to 1 (single phase)
 */
// CheckSwitchingPhases called on STATE_B entry
// When in STATE_A, phases are set directly; when in STATE_B+, Switching_Phases_C2 is set
void test_state_B_calls_check_switching_phases_from_A(void) {
    evse_init(&ctx, NULL);
    ctx.EnableC2 = ALWAYS_OFF;
    ctx.Nr_Of_Phases_Charging = 3;
    // From STATE_A, CheckSwitchingPhases sets Nr_Of_Phases directly
    evse_set_state(&ctx, STATE_B);
    TEST_ASSERT_EQUAL_INT(1, ctx.Nr_Of_Phases_Charging);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-PHASE-002
 * @scenario STATE_B re-entry sets Switching_Phases_C2 flag instead of direct phase change
 * @given The EVSE is already in STATE_B with EnableC2=ALWAYS_OFF and 3 phases configured
 * @when The state is set to STATE_B again
 * @then Switching_Phases_C2 is set to GOING_TO_SWITCH_1P (deferred switch)
 */
void test_state_B_calls_check_switching_phases_from_B(void) {
    evse_init(&ctx, NULL);
    ctx.EnableC2 = ALWAYS_OFF;
    ctx.Nr_Of_Phases_Charging = 3;
    ctx.State = STATE_B;  // Already past STATE_A
    ctx.BalancedState[0] = STATE_B;
    evse_set_state(&ctx, STATE_B);
    // From STATE_B, CheckSwitchingPhases sets Switching_Phases_C2
    TEST_ASSERT_EQUAL_INT(GOING_TO_SWITCH_1P, ctx.Switching_Phases_C2);
}

// ---- PILOT_CONNECTED guard in STATE_B entry (M3 fidelity fix) ----

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-M3
 * @scenario STATE_B entry does NOT set pilot_connected when modem disabled
 * @given ModemEnabled=false, EVSE transitions A→B
 * @when evse_set_state is called with STATE_B
 * @then pilot_connected is NOT explicitly set by STATE_B entry
 *       (it remains at whatever value it had before)
 */
void test_state_b_no_pilot_reconnect_without_modem(void) {
    setup_idle();
    ctx.Mode = MODE_NORMAL;
    ctx.ModemEnabled = false;
    /* Disconnect pilot before B entry to verify B doesn't reconnect it */
    ctx.pilot_connected = false;
    evse_set_state(&ctx, STATE_B);
    /* Without modem, STATE_B should NOT call record_pilot(true) */
    TEST_ASSERT_FALSE(ctx.pilot_connected);
    /* Verify we are actually in STATE_B */
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
}

/*
 * @feature IEC 61851-1 State Transitions
 * @req REQ-IEC61851-M3B
 * @scenario STATE_B entry DOES set pilot_connected when modem enabled
 * @given ModemEnabled=true, EVSE transitions to STATE_B
 * @when evse_set_state is called with STATE_B
 * @then pilot_connected is set to true
 */
void test_state_b_pilot_reconnect_with_modem(void) {
    setup_idle();
    ctx.Mode = MODE_NORMAL;
    ctx.ModemEnabled = true;
    ctx.pilot_connected = false;
    evse_set_state(&ctx, STATE_B);
    TEST_ASSERT_TRUE(ctx.pilot_connected);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
}

// ---- Main ----
int main(void) {
    TEST_SUITE_BEGIN("State Transitions");

    RUN_TEST(test_init_state_is_A);
    RUN_TEST(test_init_contactors_off);
    RUN_TEST(test_init_pilot_connected);
    RUN_TEST(test_init_no_errors);
    RUN_TEST(test_A_stays_A_on_12V);
    RUN_TEST(test_A_to_B_on_9V_when_ready);
    RUN_TEST(test_A_to_modem_when_modem_stage_0);
    RUN_TEST(test_A_to_B_skips_modem_when_disabled);
    RUN_TEST(test_A_stays_A_when_access_off);
    RUN_TEST(test_A_to_B1_when_errors);
    RUN_TEST(test_A_to_B1_when_charge_delay);
    RUN_TEST(test_B_to_A_on_disconnect);
    RUN_TEST(test_B_to_C_on_6V_with_diode_check);
    RUN_TEST(test_B_to_C_requires_diode_check);
    RUN_TEST(test_diode_check_sets_on_pilot_diode);
    RUN_TEST(test_C_contactor1_on);
    RUN_TEST(test_C_to_A_on_disconnect);
    RUN_TEST(test_C_to_B_on_9V);
    RUN_TEST(test_C_to_B_on_pilot_short);
    RUN_TEST(test_C1_to_A_on_disconnect);
    RUN_TEST(test_C1_to_B1_on_9V);
    RUN_TEST(test_C1_timer_transitions_to_B1);
    RUN_TEST(test_B1_to_A_on_disconnect);
    RUN_TEST(test_set_state_B1_sets_charge_delay);
    RUN_TEST(test_set_state_A_clears_errors_and_delay);
    RUN_TEST(test_transition_log_records_states);
    RUN_TEST(test_set_state_C1_sets_pwm_off);
    RUN_TEST(test_full_charge_cycle);
    RUN_TEST(test_actstart_to_A_on_disconnect);
    RUN_TEST(test_activation_mode_triggers_actstart);
    RUN_TEST(test_activation_mode_countdown);
    RUN_TEST(test_activation_mode_255_does_not_countdown);
    RUN_TEST(test_actstart_returns_to_B_when_timer_expires);
    RUN_TEST(test_comm_b_ok_transitions_to_B);
    RUN_TEST(test_comm_c_ok_transitions_to_C);
    RUN_TEST(test_node_sends_comm_b);
    RUN_TEST(test_state_B_calls_check_switching_phases_from_A);
    RUN_TEST(test_state_B_calls_check_switching_phases_from_B);
    RUN_TEST(test_state_b_no_pilot_reconnect_without_modem);
    RUN_TEST(test_state_b_pilot_reconnect_with_modem);

    TEST_SUITE_RESULTS();
}
