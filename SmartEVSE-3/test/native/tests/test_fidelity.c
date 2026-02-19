/*
 * test_fidelity.c - Regression tests for behavioral differences found by
 * systematic comparison of original Timer10ms/Timer1S vs extracted state machine.
 *
 * These tests verify that the extracted module matches the original Dingo35
 * firmware behavior. Each test references the specific discrepancy ID from
 * the fidelity analysis (D1-D6).
 */

#include "test_framework.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

static void setup_standalone(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;  // Standalone
    ctx.AccessStatus = ON;
    ctx.ModemStage = 1;  // Skip modem
    ctx.MaxCurrent = 13;
    ctx.MaxCapacity = 13;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 32;
    ctx.MaxMains = 25;
}

static void setup_slave(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 2;  // Slave
    ctx.AccessStatus = ON;
    ctx.ModemStage = 1;
    ctx.MaxCurrent = 13;
    ctx.MaxCapacity = 13;
    ctx.MinCurrent = 6;
}

// ===================================================================
// D1: DisconnectTimeCounter NOT double-incremented
// ===================================================================

/*
 * @feature Fidelity: DisconnectTimeCounter
 * @req REQ-FID-D1A
 * @scenario Module tick_1s does not increment DisconnectTimeCounter
 * @given ModemEnabled=true, DisconnectTimeCounter=0
 * @when tick_1s is called
 * @then Counter stays 0 (firmware wrapper handles increment + pilot check)
 */
void test_fid_disconnect_counter_not_in_module(void) {
    evse_init(&ctx, NULL);
    ctx.ModemEnabled = true;
    ctx.DisconnectTimeCounter = 0;
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(0, ctx.DisconnectTimeCounter);
}

/*
 * @feature Fidelity: DisconnectTimeCounter
 * @req REQ-FID-D1B
 * @scenario Module correctly sets counter via set_state for STATE_A
 * @given ModemEnabled=true, DisconnectTimeCounter=-1
 * @when evse_set_state to STATE_A
 * @then Counter is set to 0 (start counting)
 */
void test_fid_disconnect_counter_starts_on_state_a(void) {
    evse_init(&ctx, NULL);
    ctx.ModemEnabled = true;
    ctx.DisconnectTimeCounter = -1;
    evse_set_state(&ctx, STATE_A);
    TEST_ASSERT_EQUAL_INT(0, ctx.DisconnectTimeCounter);
}

/*
 * @feature Fidelity: DisconnectTimeCounter
 * @req REQ-FID-D1C
 * @scenario Module disables counter on MODEM_REQUEST entry
 * @given DisconnectTimeCounter=5
 * @when evse_set_state to STATE_MODEM_REQUEST
 * @then Counter is set to -1 (disabled)
 */
void test_fid_disconnect_counter_disabled_on_modem_request(void) {
    evse_init(&ctx, NULL);
    ctx.ModemEnabled = true;
    ctx.DisconnectTimeCounter = 5;
    evse_set_state(&ctx, STATE_MODEM_REQUEST);
    TEST_ASSERT_EQUAL_INT(-1, ctx.DisconnectTimeCounter);
}

// ===================================================================
// D2: PilotDisconnectTime reconnect only in tick_10ms
// ===================================================================

/*
 * @feature Fidelity: PilotDisconnectTime
 * @req REQ-FID-D2A
 * @scenario tick_1s only decrements timer, does not reconnect
 * @given PilotDisconnectTime=1, PilotDisconnected=true
 * @when tick_1s is called (timer reaches 0)
 * @then PilotDisconnected is still true (reconnect happens in tick_10ms)
 */
void test_fid_pilot_disconnect_no_reconnect_in_tick_1s(void) {
    evse_init(&ctx, NULL);
    ctx.PilotDisconnectTime = 1;
    ctx.PilotDisconnected = true;
    ctx.pilot_connected = false;

    evse_tick_1s(&ctx);

    TEST_ASSERT_EQUAL_INT(0, ctx.PilotDisconnectTime);
    TEST_ASSERT_TRUE(ctx.PilotDisconnected);  // NOT reconnected
    TEST_ASSERT_FALSE(ctx.pilot_connected);   // NOT reconnected
}

/*
 * @feature Fidelity: PilotDisconnectTime
 * @req REQ-FID-D2B
 * @scenario tick_10ms reconnects when PilotDisconnectTime reaches 0
 * @given PilotDisconnected=true, PilotDisconnectTime=0, State=B1
 * @when tick_10ms is called with PILOT_9V
 * @then PilotDisconnected=false, pilot_connected=true
 */
void test_fid_pilot_disconnect_reconnects_in_tick_10ms(void) {
    setup_standalone();
    ctx.State = STATE_B1;
    ctx.PilotDisconnected = true;
    ctx.PilotDisconnectTime = 0;
    ctx.pilot_connected = false;

    evse_tick_10ms(&ctx, PILOT_9V);

    TEST_ASSERT_FALSE(ctx.PilotDisconnected);
    TEST_ASSERT_TRUE(ctx.pilot_connected);
}

// ===================================================================
// D3: Fall-through from COMM_B_OK → STATE_B handler in same tick
// ===================================================================

/*
 * @feature Fidelity: Fall-through behavior
 * @req REQ-FID-D3A
 * @scenario COMM_B_OK transitions to STATE_B and B handler runs same tick
 * @given State=COMM_B_OK, DiodeCheck=0
 * @when tick_10ms with PILOT_DIODE
 * @then State=STATE_B, and DiodeCheck=1 (B handler fired in same tick)
 */
void test_fid_comm_b_ok_falls_through_to_b_handler(void) {
    setup_standalone();
    ctx.State = STATE_COMM_B_OK;
    ctx.BalancedState[0] = STATE_COMM_B_OK;
    ctx.DiodeCheck = 0;

    evse_tick_10ms(&ctx, PILOT_DIODE);

    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_EQUAL_INT(1, ctx.DiodeCheck);  // B handler set this in same tick
}

/*
 * @feature Fidelity: Fall-through behavior
 * @req REQ-FID-D3B
 * @scenario COMM_B_OK → B → 12V disconnect in same tick
 * @given State=COMM_B_OK
 * @when tick_10ms with PILOT_12V
 * @then State=STATE_A (B handler detects disconnect in same tick)
 */
void test_fid_comm_b_ok_to_b_then_disconnect(void) {
    setup_standalone();
    ctx.State = STATE_COMM_B_OK;
    ctx.BalancedState[0] = STATE_COMM_B_OK;

    evse_tick_10ms(&ctx, PILOT_12V);

    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

// ===================================================================
// D4: Fall-through from A→B transition to STATE_B handler
// ===================================================================

/*
 * @feature Fidelity: Fall-through behavior
 * @req REQ-FID-D4A
 * @scenario A→B transition allows B handler to run and detect PILOT_DIODE
 * @given State=STATE_A, PILOT_DIODE would trigger A→B on 9V but we pass 9V
 *        followed by PILOT_DIODE on same tick is not possible, so we test
 *        that A→B sets ActivationMode=30 then the B handler resets StateTimer
 * @when tick_10ms with PILOT_9V in STATE_A with access
 * @then State=STATE_B, ActivationMode=30, and StateTimer reset to 0
 */
void test_fid_a_to_b_falls_through_to_b_handler(void) {
    setup_standalone();

    evse_tick_10ms(&ctx, PILOT_9V);

    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_EQUAL_INT(30, ctx.ActivationMode);
    // B handler fires: pilot=9V, StateTimer reset to 0
    TEST_ASSERT_EQUAL_INT(0, ctx.StateTimer);
}

// ===================================================================
// D5: Fall-through from COMM_C_OK → STATE_C handler
// ===================================================================

/*
 * @feature Fidelity: Fall-through behavior
 * @req REQ-FID-D5A
 * @scenario COMM_C_OK transitions to STATE_C and C handler runs same tick
 * @given State=COMM_C_OK
 * @when tick_10ms with PILOT_6V
 * @then State=STATE_C, StateTimer=0 (C handler's else resets StateTimer)
 */
void test_fid_comm_c_ok_falls_through_to_c_handler(void) {
    setup_slave();
    ctx.State = STATE_COMM_C_OK;
    ctx.BalancedState[0] = STATE_COMM_C_OK;
    ctx.StateTimer = 42;  // Non-zero to verify it gets reset

    evse_tick_10ms(&ctx, PILOT_6V);

    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.State);
    // C handler fires with PILOT_6V — this is the normal pilot voltage for
    // charging, so the else branch sets StateTimer=0
    TEST_ASSERT_EQUAL_INT(0, ctx.StateTimer);
}

/*
 * @feature Fidelity: Fall-through behavior
 * @req REQ-FID-D5B
 * @scenario COMM_C_OK → C → immediate 12V disconnect in same tick
 * @given State=COMM_C_OK
 * @when tick_10ms with PILOT_12V
 * @then State=STATE_A (C handler detects disconnect immediately)
 */
void test_fid_comm_c_ok_to_c_then_disconnect(void) {
    setup_slave();
    ctx.State = STATE_COMM_C_OK;
    ctx.BalancedState[0] = STATE_COMM_C_OK;

    evse_tick_10ms(&ctx, PILOT_12V);

    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

// ===================================================================
// D6: ACTSTART does NOT check PILOT_12V (matches original)
// ===================================================================

/*
 * @feature Fidelity: ACTSTART no pilot check
 * @req REQ-FID-D6A
 * @scenario ACTSTART ignores PILOT_12V (original has no pilot check here)
 * @given State=ACTSTART with timer running (3 seconds)
 * @when tick_10ms with PILOT_12V
 * @then State stays ACTSTART (timer must expire → B → detects 12V → A)
 */
void test_fid_actstart_no_pilot_12v_check(void) {
    setup_standalone();
    evse_set_state(&ctx, STATE_ACTSTART);
    ctx.ActivationTimer = 3;

    evse_tick_10ms(&ctx, PILOT_12V);

    TEST_ASSERT_EQUAL_INT(STATE_ACTSTART, ctx.State);
}

/*
 * @feature Fidelity: ACTSTART no pilot check
 * @req REQ-FID-D6B
 * @scenario ACTSTART timer expiry leads to STATE_B (B handler is before ACTSTART
 *           in the chain, so B handler does NOT fire this tick — runs next tick)
 * @given State=ACTSTART, ActivationTimer=0
 * @when tick_10ms with PILOT_12V, then another tick_10ms with PILOT_12V
 * @then First tick: ACTSTART → B. Second tick: B → A (12V detected)
 */
void test_fid_actstart_timer_then_disconnect(void) {
    setup_standalone();
    evse_set_state(&ctx, STATE_ACTSTART);
    ctx.ActivationTimer = 0;

    // First tick: ACTSTART handler fires, timer=0 → STATE_B
    // B handler already passed (before ACTSTART) so doesn't fire
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_EQUAL_INT(255, ctx.ActivationMode);

    // Second tick: B handler detects PILOT_12V → STATE_A
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

// ===================================================================
// D7: Modem states NOT handled in tick_10ms (matches original)
// ===================================================================

/*
 * @feature Fidelity: Modem states not in tick_10ms
 * @req REQ-FID-D7A
 * @scenario Modem states are invisible to tick_10ms
 * @given EVSE in each modem state
 * @when tick_10ms with any pilot value
 * @then State does not change (modem managed entirely by tick_1s)
 */
void test_fid_modem_states_invisible_to_tick_10ms(void) {
    uint8_t states[] = {STATE_MODEM_REQUEST, STATE_MODEM_WAIT,
                        STATE_MODEM_DONE, STATE_MODEM_DENIED};
    uint8_t pilots[] = {PILOT_12V, PILOT_9V, PILOT_6V, PILOT_DIODE};

    for (int s = 0; s < 4; s++) {
        for (int p = 0; p < 4; p++) {
            evse_init(&ctx, NULL);
            ctx.ModemEnabled = true;
            ctx.State = states[s];
            evse_tick_10ms(&ctx, pilots[p]);
            TEST_ASSERT_EQUAL_INT(states[s], ctx.State);
        }
    }
}

// ===================================================================
// D8: Handler order matches original (ACTSTART before COMM_C_OK before C)
// ===================================================================

/*
 * @feature Fidelity: Handler ordering
 * @req REQ-FID-D8A
 * @scenario B→ACTSTART falls through to ACTSTART handler in same tick
 * @given State=STATE_B, ActivationMode=0 (expired)
 * @when tick_10ms with PILOT_9V
 * @then State=STATE_ACTSTART, ActivationTimer=3
 *       (ACTSTART handler runs but timer is 3, so no action)
 */
void test_fid_b_to_actstart_falls_through(void) {
    setup_standalone();
    evse_set_state(&ctx, STATE_B);
    ctx.ActivationMode = 0;

    evse_tick_10ms(&ctx, PILOT_9V);

    TEST_ASSERT_EQUAL_INT(STATE_ACTSTART, ctx.State);
    TEST_ASSERT_EQUAL_INT(3, ctx.ActivationTimer);
}

// ===================================================================
// Config field used correctly in calc_balanced_current
// ===================================================================

/*
 * @feature Fidelity: Config field
 * @req REQ-FID-CFG-A
 * @scenario Socket mode (Config=0) caps ChargeCurrent by MaxCapacity
 * @given Config=0, MaxCurrent=25, MaxCapacity=16, STATE_C
 * @when calc_balanced_current is called
 * @then ChargeCurrent=160 (capped by MaxCapacity)
 */
void test_fid_config_socket_caps_by_maxcapacity(void) {
    setup_standalone();
    ctx.Config = 0;
    ctx.MaxCurrent = 25;
    ctx.MaxCapacity = 16;
    ctx.BalancedState[0] = STATE_C;
    ctx.Balanced[0] = 100;

    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_EQUAL_INT(160, ctx.ChargeCurrent);
}

/*
 * @feature Fidelity: Config field
 * @req REQ-FID-CFG-B
 * @scenario Fixed cable mode (Config=1) does NOT cap by MaxCapacity
 * @given Config=1, MaxCurrent=25, MaxCapacity=16, STATE_C
 * @when calc_balanced_current is called
 * @then ChargeCurrent=250 (MaxCurrent * 10, not capped)
 */
void test_fid_config_fixed_cable_no_maxcapacity_cap(void) {
    setup_standalone();
    ctx.Config = 1;
    ctx.MaxCurrent = 25;
    ctx.MaxCapacity = 16;
    ctx.BalancedState[0] = STATE_C;
    ctx.Balanced[0] = 100;

    evse_calc_balanced_current(&ctx, 0);

    TEST_ASSERT_EQUAL_INT(250, ctx.ChargeCurrent);
}

int main(void) {
    TEST_SUITE_BEGIN("Fidelity Regression Tests");

    /* D1: DisconnectTimeCounter */
    RUN_TEST(test_fid_disconnect_counter_not_in_module);
    RUN_TEST(test_fid_disconnect_counter_starts_on_state_a);
    RUN_TEST(test_fid_disconnect_counter_disabled_on_modem_request);

    /* D2: PilotDisconnectTime */
    RUN_TEST(test_fid_pilot_disconnect_no_reconnect_in_tick_1s);
    RUN_TEST(test_fid_pilot_disconnect_reconnects_in_tick_10ms);

    /* D3: COMM_B_OK fall-through */
    RUN_TEST(test_fid_comm_b_ok_falls_through_to_b_handler);
    RUN_TEST(test_fid_comm_b_ok_to_b_then_disconnect);

    /* D4: A→B fall-through */
    RUN_TEST(test_fid_a_to_b_falls_through_to_b_handler);

    /* D5: COMM_C_OK fall-through */
    RUN_TEST(test_fid_comm_c_ok_falls_through_to_c_handler);
    RUN_TEST(test_fid_comm_c_ok_to_c_then_disconnect);

    /* D6: ACTSTART no pilot check */
    RUN_TEST(test_fid_actstart_no_pilot_12v_check);
    RUN_TEST(test_fid_actstart_timer_then_disconnect);

    /* D7: Modem not in tick_10ms */
    RUN_TEST(test_fid_modem_states_invisible_to_tick_10ms);

    /* D8: Handler ordering */
    RUN_TEST(test_fid_b_to_actstart_falls_through);

    /* Config field */
    RUN_TEST(test_fid_config_socket_caps_by_maxcapacity);
    RUN_TEST(test_fid_config_fixed_cable_no_maxcapacity_cap);

    TEST_SUITE_RESULTS();
}
