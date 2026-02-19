/*
 * test_tick_10ms.c - Tests for the 10ms timer tick (evse_tick_10ms)
 *
 * Covers pilot disconnect guards, RFID reader, capacity limits,
 * state B debounce, state C transitions, COMM states, activation,
 * and modem disconnect handling.
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

static void setup_idle(void) {
    evse_init(&ctx, NULL);
    ctx.AccessStatus = ON;
}

static void setup_ready_to_charge(void) {
    setup_idle();
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;
    ctx.ChargeCurrent = 100;
    ctx.ModemStage = 1;
}

/* ---- Pilot disconnect guards ---- */

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-001
 * @scenario Pilot disconnect guard ignores 9V signal while active
 * @given EVSE is ready to charge with PilotDisconnected=true and PilotDisconnectTime > 0
 * @when A 9V pilot signal (vehicle connected) is received during a 10ms tick
 * @then The EVSE remains in STATE_A because the disconnect guard suppresses the pilot reading
 */
void test_pilot_disconnect_guards_reading(void) {
    setup_ready_to_charge();
    ctx.PilotDisconnected = true;
    ctx.PilotDisconnectTime = 3;
    /* With PilotDisconnected=true the 9V pilot should be ignored */
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-002
 * @scenario Pilot disconnect flag clears when timer reaches zero
 * @given EVSE is ready to charge with PilotDisconnected=true and PilotDisconnectTime=0
 * @when A 9V pilot signal is received during a 10ms tick
 * @then PilotDisconnected is cleared to false and pilot_connected is set to true
 */
void test_pilot_disconnect_clears_on_timer_zero(void) {
    setup_ready_to_charge();
    ctx.PilotDisconnected = true;
    ctx.PilotDisconnectTime = 0;
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_FALSE(ctx.PilotDisconnected);
    TEST_ASSERT_TRUE(ctx.pilot_connected);
}

/* ---- RFID reader ---- */

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-003
 * @scenario RFID reader type 1 starts access lock timer
 * @given EVSE is ready to charge with RFIDReader=1, AccessTimer=0, and AccessStatus=ON
 * @when A 12V pilot signal (no vehicle) is received during a 10ms tick
 * @then AccessTimer is set to RFIDLOCKTIME to begin the RFID lock countdown
 */
void test_rfid_reader_1_starts_access_timer(void) {
    setup_ready_to_charge();
    ctx.RFIDReader = 1;
    ctx.AccessTimer = 0;
    ctx.AccessStatus = ON;
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(RFIDLOCKTIME, ctx.AccessTimer);
}

/* ---- MaxCapacity limits ---- */

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-004
 * @scenario MaxCapacity limits charge current when below MaxCurrent
 * @given EVSE is ready to charge with MaxCapacity=8A which is less than MaxCurrent=13A
 * @when A 9V pilot signal triggers the A-to-B transition during a 10ms tick
 * @then ChargeCurrent is set to 80 deciamps (MaxCapacity * 10)
 */
void test_maxcapacity_limits_charge_current(void) {
    setup_ready_to_charge();
    ctx.MaxCapacity = 8;   /* Less than MaxCurrent (13) */
    ctx.MaxCurrent = 13;
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(80, ctx.ChargeCurrent);
}

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-005
 * @scenario MaxCapacity at or above MaxCurrent falls back to MinCurrent
 * @given EVSE is ready to charge with MaxCapacity=16A >= MaxCurrent=13A and MinCurrent=6A
 * @when A 9V pilot signal triggers the A-to-B transition during a 10ms tick
 * @then ChargeCurrent is set to 60 deciamps (MinCurrent * 10) as the default starting point
 */
void test_maxcapacity_default_uses_mincurrent(void) {
    setup_ready_to_charge();
    ctx.MaxCapacity = 16;  /* >= MaxCurrent (13) */
    ctx.MaxCurrent = 13;
    ctx.MinCurrent = 6;
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(60, ctx.ChargeCurrent);
}

/* ---- A->B with no current ---- */

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-006
 * @scenario LESS_6A error flag set when insufficient current available in Smart mode
 * @given EVSE is in Smart mode standalone with very low MaxMains=2A and MainsMeterImeasured=200 (overloaded)
 * @when A 9V pilot signal (vehicle connected) is received during a 10ms tick
 * @then The LESS_6A error flag is set because available current is below MinCurrent
 */
void test_less_6a_when_no_current_available(void) {
    setup_idle();
    ctx.Mode = MODE_SMART;
    ctx.LoadBl = 0;
    ctx.ModemStage = 1;
    ctx.MaxMains = 2;                /* Very low mains limit */
    ctx.MainsMeterType = 1;
    ctx.MainsMeterImeasured = 200;   /* Already overloaded */
    ctx.MinCurrent = 6;
    ctx.ChargeCurrent = 60;
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_TRUE(ctx.ErrorFlags & LESS_6A);
}

/* ---- STATE_B + 6V debounce ---- */

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-007
 * @scenario STATE_B with 6V pilot increments state timer for debounce
 * @given EVSE is in STATE_B with DiodeCheck=1 and StateTimer=0
 * @when A 6V pilot signal (vehicle requesting charge) is received during a 10ms tick
 * @then StateTimer increments to 1, counting toward the B-to-C debounce threshold
 */
void test_b_6v_increments_state_timer(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_B);
    ctx.DiodeCheck = 1;
    ctx.StateTimer = 0;
    evse_tick_10ms(&ctx, PILOT_6V);
    TEST_ASSERT_EQUAL_INT(1, ctx.StateTimer);
}

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-008
 * @scenario STATE_B with 9V pilot resets the state timer
 * @given EVSE is in STATE_B with StateTimer=30 (partially debounced)
 * @when A 9V pilot signal (vehicle connected but not requesting charge) is received
 * @then StateTimer is reset to 0, canceling the B-to-C debounce countdown
 */
void test_b_9v_resets_state_timer(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_B);
    ctx.StateTimer = 30;
    ctx.ActivationMode = 255; /* prevent ACTSTART */
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(0, ctx.StateTimer);
}

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-009
 * @scenario STATE_B to STATE_C transition requires 55 consecutive 6V ticks
 * @given EVSE is in STATE_B with DiodeCheck=1 and ChargeCurrent at MaxCurrent
 * @when 50 consecutive 6V pilot ticks are received (below threshold) then 5 more (reaching 55)
 * @then The EVSE does not transition at 50 ticks but transitions to STATE_C at 55 ticks
 */
void test_b_to_c_debounce_threshold(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_B);
    ctx.DiodeCheck = 1;
    ctx.ChargeCurrent = ctx.MaxCurrent * 10;
    /* 50 ticks: should NOT yet transition */
    for (int i = 0; i < 50; i++)
        evse_tick_10ms(&ctx, PILOT_6V);
    TEST_ASSERT_NOT_EQUAL(STATE_C, ctx.State);
    /* 5 more ticks: now it should */
    for (int i = 0; i < 5; i++)
        evse_tick_10ms(&ctx, PILOT_6V);
    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.State);
}

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-010
 * @scenario STATE_B to STATE_C transition is blocked when errors are present
 * @given EVSE is in STATE_B with DiodeCheck=1 but ErrorFlags contains TEMP_HIGH
 * @when 55 consecutive 6V pilot ticks are received (past debounce threshold)
 * @then The EVSE does not transition to STATE_C because an error condition is active
 */
void test_b_to_c_requires_diode_and_no_errors(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_B);
    ctx.DiodeCheck = 1;
    ctx.ErrorFlags = TEMP_HIGH; /* Error present */
    ctx.ChargeCurrent = ctx.MaxCurrent * 10;
    for (int i = 0; i < 55; i++)
        evse_tick_10ms(&ctx, PILOT_6V);
    TEST_ASSERT_NOT_EQUAL(STATE_C, ctx.State);
}

/* ---- STATE_C transitions ---- */

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-011
 * @scenario STATE_C transitions to STATE_B after sustained pilot short debounce
 * @given EVSE is in STATE_C (actively charging)
 * @when Fewer than 50 PILOT_SHORT ticks are received followed by enough to exceed 50 total
 * @then The EVSE stays in STATE_C below 50 ticks but transitions to STATE_B after 50 consecutive short ticks
 */
void test_c_short_debounce(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_C);
    /* Less than 50 ticks of PILOT_SHORT: should stay in C */
    for (int i = 0; i < 45; i++)
        evse_tick_10ms(&ctx, PILOT_SHORT);
    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.State);
    /* Push past 50: should transition to B */
    for (int i = 0; i < 10; i++)
        evse_tick_10ms(&ctx, PILOT_SHORT);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
}

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-012
 * @scenario STATE_C stays in STATE_C on 6V pilot and resets state timer
 * @given EVSE is in STATE_C with StateTimer=20
 * @when A 6V pilot signal is received during a 10ms tick
 * @then The EVSE remains in STATE_C and StateTimer is reset to 0
 */
void test_c_6v_no_transition(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_C);
    ctx.StateTimer = 20;
    evse_tick_10ms(&ctx, PILOT_6V);
    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.State);
    TEST_ASSERT_EQUAL_INT(0, ctx.StateTimer);
}

/* ---- COMM_B stays on 9V ---- */

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-013
 * @scenario COMM_B state does not trigger A-to-B transition logic on 9V pilot
 * @given EVSE is a node (LoadBl=2) in STATE_COMM_B waiting for master confirmation
 * @when A 9V pilot signal is received during a 10ms tick
 * @then The EVSE does not transition to STATE_B because COMM_B bypasses the A-to-B path
 */
void test_comm_b_stays_on_9v(void) {
    setup_idle();
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 2;
    ctx.ModemStage = 1;
    evse_set_state(&ctx, STATE_COMM_B);
    ctx.BalancedState[0] = STATE_COMM_B;
    /* 9V should NOT trigger A->B logic because State is COMM_B */
    evse_tick_10ms(&ctx, PILOT_9V);
    /* COMM_B with 9V, no error, no chargeDelay, but it's STATE_COMM_B so the
     * condition ctx.State != STATE_COMM_B prevents A->B. Should stay COMM_B or go B1. */
    TEST_ASSERT_NOT_EQUAL(STATE_B, ctx.State);
}

/* ---- Node B->COMM_C ---- */

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-014
 * @scenario Node transitions from STATE_B to STATE_COMM_C instead of STATE_C
 * @given EVSE is a node (LoadBl=2) in STATE_B with DiodeCheck=1 and sufficient charge current
 * @when 55 consecutive 6V pilot ticks are received (past debounce threshold)
 * @then The EVSE transitions to STATE_COMM_C (waiting for master to confirm charge start)
 */
void test_node_b_to_comm_c(void) {
    setup_idle();
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 2;
    ctx.ModemStage = 1;
    evse_set_state(&ctx, STATE_B);
    ctx.DiodeCheck = 1;
    ctx.ChargeCurrent = ctx.MaxCurrent * 10;
    for (int i = 0; i < 55; i++)
        evse_tick_10ms(&ctx, PILOT_6V);
    TEST_ASSERT_EQUAL_INT(STATE_COMM_C, ctx.State);
}

/* ---- A->B side effects ---- */

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-015
 * @scenario A-to-B transition sets BalancedMax from MaxCapacity
 * @given EVSE is ready to charge in standalone mode with MaxCapacity=10A
 * @when A 9V pilot signal triggers the A-to-B transition
 * @then BalancedMax[0] is set to 100 deciamps (MaxCapacity * 10)
 */
void test_a_to_b_sets_balanced_max(void) {
    setup_ready_to_charge();
    ctx.MaxCapacity = 10;
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(100, ctx.BalancedMax[0]);
}

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-016
 * @scenario A-to-B transition does NOT set extra PWM duty (F3 fidelity fix)
 * @given EVSE is ready to charge in standalone mode
 * @when A 9V pilot signal triggers the A-to-B transition
 * @then PWM duty remains 1024 (from STATE_A entry); module does not set PWM on A→B,
 *       the platform callback (on_state_change) handles timer setup
 */
void test_a_to_b_no_extra_pwm(void) {
    setup_ready_to_charge();
    /* After init, last_pwm_duty is 0 (memset). Remember it. */
    uint32_t before = ctx.last_pwm_duty;
    evse_tick_10ms(&ctx, PILOT_9V);
    /* STATE_B entry does not call record_cp_duty. The platform callback
     * (on_state_change) handles timer setup, not the module. */
    TEST_ASSERT_EQUAL_INT(before, ctx.last_pwm_duty);
}

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-017
 * @scenario A-to-B transition initializes ActivationMode to 30 and clears AccessTimer
 * @given EVSE is ready to charge in standalone mode
 * @when A 9V pilot signal triggers the A-to-B transition
 * @then ActivationMode is set to 30 (countdown for activation) and AccessTimer is cleared to 0
 */
void test_a_to_b_sets_activation_mode_30(void) {
    setup_ready_to_charge();
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(30, ctx.ActivationMode);
    TEST_ASSERT_EQUAL_INT(0, ctx.AccessTimer);
}

/* ---- B1 with errors stays B1 on 9V ---- */

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-018
 * @scenario STATE_B1 remains in B1 when errors are present on 9V pilot
 * @given EVSE is in STATE_B1 (connected but waiting) with TEMP_HIGH error flag set
 * @when A 9V pilot signal is received during a 10ms tick
 * @then The EVSE stays in STATE_B1 because errors prevent transition to charging states
 */
void test_b1_with_errors_stays_b1_on_9v(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_B1);
    ctx.PilotDisconnected = false;
    ctx.PilotDisconnectTime = 0;
    ctx.ErrorFlags = TEMP_HIGH;
    evse_tick_10ms(&ctx, PILOT_9V);
    /* Should go to B1 (already there) since errors present */
    TEST_ASSERT_EQUAL_INT(STATE_B1, ctx.State);
}

/* ---- Modem states to A on 12V ---- */

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-019
 * @scenario Modem states are NOT handled in tick_10ms (matches original Timer10ms)
 * @given EVSE is in one of the modem states (REQUEST, WAIT, DONE, or DENIED)
 * @when A 12V pilot signal (no vehicle) is received during a 10ms tick
 * @then The EVSE stays in its modem state (modem is managed solely by tick_1s)
 */
void test_modem_states_to_a_on_12v(void) {
    uint8_t modem_states[] = {STATE_MODEM_REQUEST, STATE_MODEM_WAIT,
                              STATE_MODEM_DONE, STATE_MODEM_DENIED};
    for (int i = 0; i < 4; i++) {
        setup_ready_to_charge();
        ctx.State = modem_states[i];
        ctx.BalancedState[0] = modem_states[i];
        evse_tick_10ms(&ctx, PILOT_12V);
        TEST_ASSERT_EQUAL_INT(modem_states[i], ctx.State);  // Stays in modem state
    }
}

/* ---- ACTSTART to B when timer zero ---- */

/*
 * @feature 10ms Tick Processing
 * @req REQ-TICK10-020
 * @scenario ACTSTART transitions to STATE_B when activation timer expires
 * @given EVSE is in STATE_ACTSTART with ActivationTimer=0 (timer expired)
 * @when A 9V pilot signal is received during a 10ms tick
 * @then The EVSE transitions to STATE_B and ActivationMode is set to 255 (disabled)
 */
void test_actstart_to_b_when_timer_zero(void) {
    setup_ready_to_charge();
    evse_set_state(&ctx, STATE_ACTSTART);
    ctx.ActivationTimer = 0;
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_EQUAL_INT(255, ctx.ActivationMode);
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("Tick 10ms");

    RUN_TEST(test_pilot_disconnect_guards_reading);
    RUN_TEST(test_pilot_disconnect_clears_on_timer_zero);
    RUN_TEST(test_rfid_reader_1_starts_access_timer);
    RUN_TEST(test_maxcapacity_limits_charge_current);
    RUN_TEST(test_maxcapacity_default_uses_mincurrent);
    RUN_TEST(test_less_6a_when_no_current_available);
    RUN_TEST(test_b_6v_increments_state_timer);
    RUN_TEST(test_b_9v_resets_state_timer);
    RUN_TEST(test_b_to_c_debounce_threshold);
    RUN_TEST(test_b_to_c_requires_diode_and_no_errors);
    RUN_TEST(test_c_short_debounce);
    RUN_TEST(test_c_6v_no_transition);
    RUN_TEST(test_comm_b_stays_on_9v);
    RUN_TEST(test_node_b_to_comm_c);
    RUN_TEST(test_a_to_b_sets_balanced_max);
    RUN_TEST(test_a_to_b_no_extra_pwm);
    RUN_TEST(test_a_to_b_sets_activation_mode_30);
    RUN_TEST(test_b1_with_errors_stays_b1_on_9v);
    RUN_TEST(test_modem_states_to_a_on_12v);
    RUN_TEST(test_actstart_to_b_when_timer_zero);

    TEST_SUITE_RESULTS();
}
