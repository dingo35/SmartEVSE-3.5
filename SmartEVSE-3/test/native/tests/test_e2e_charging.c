/*
 * test_e2e_charging.c - End-to-end charging flow tests
 *
 * These tests verify complete charging sequences from connect to disconnect,
 * including DiodeCheck acquisition, slave COMM handshake, OCPP authorization,
 * error scenarios, and reconnect-after-disconnect state cleanliness.
 */

#include "test_framework.h"
#include "evse_state_machine.h"

static evse_ctx_t ctx;

static void setup_standalone(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;
    ctx.AccessStatus = ON;
    ctx.ModemStage = 1;
    ctx.MaxCurrent = 13;
    ctx.MaxCapacity = 13;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 32;
    ctx.MaxMains = 25;
}

static void setup_slave(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 2;
    ctx.AccessStatus = ON;
    ctx.ModemStage = 1;
    ctx.MaxCurrent = 13;
    ctx.MaxCapacity = 13;
    ctx.MinCurrent = 6;
    ctx.MaxCircuit = 32;
    ctx.MaxMains = 25;
}

// ===================================================================
// Scenario A: Standalone happy path with DiodeCheck acquisition
// ===================================================================

/*
 * @feature End-to-End Charging
 * @req REQ-E2E-001
 * @scenario Complete standalone charge cycle with DiodeCheck
 * @given Standalone EVSE, authorized, Normal mode
 * @when Car connects (9V) → DiodeCheck (DIODE) → requests charge (6V) → stops (9V) → disconnects (12V)
 * @then Full cycle: A → B → (DiodeCheck) → C → B → A with correct contactor states
 */
void test_e2e_standalone_happy_path(void) {
    setup_standalone();

    // Step 1: Initial state
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
    TEST_ASSERT_FALSE(ctx.contactor1_state);
    TEST_ASSERT_EQUAL_INT(NO_ERROR, ctx.ErrorFlags);

    // Step 2: Car connects — 9V pilot
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_FALSE(ctx.contactor1_state);
    TEST_ASSERT_EQUAL_INT(0, ctx.DiodeCheck);
    TEST_ASSERT_EQUAL_INT(30, ctx.ActivationMode);

    // Step 3: DiodeCheck — ADC reads LOW phase of CP waveform
    evse_tick_10ms(&ctx, PILOT_DIODE);
    TEST_ASSERT_EQUAL_INT(1, ctx.DiodeCheck);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);

    // Step 4: Car pulls to 6V — debounce 500ms (51 ticks)
    for (int i = 0; i < 51; i++) {
        evse_tick_10ms(&ctx, PILOT_6V);
    }
    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.State);
    TEST_ASSERT_TRUE(ctx.contactor1_state);

    // Step 5: Charging in progress — 6V maintained
    evse_tick_10ms(&ctx, PILOT_6V);
    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.State);

    // Step 6: Car finishes — pulls to 9V
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_EQUAL_INT(0, ctx.DiodeCheck);
    TEST_ASSERT_FALSE(ctx.contactor1_state);

    // Step 7: Disconnect
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
    TEST_ASSERT_FALSE(ctx.contactor1_state);
}

// ===================================================================
// Scenario B: Slave with COMM_B/COMM_C handshake
// ===================================================================

/*
 * @feature End-to-End Charging
 * @req REQ-E2E-002
 * @scenario Slave EVSE full charge cycle with master handshake
 * @given Slave EVSE (LoadBl=2), authorized
 * @when Car connects → COMM_B → master approves → B → DiodeCheck → 6V → COMM_C → master approves → C → disconnect
 * @then Full slave cycle: A → COMM_B → COMM_B_OK → B → COMM_C → COMM_C_OK → C → B → A
 */
void test_e2e_slave_happy_path(void) {
    setup_slave();

    // Step 1: Car connects — 9V → COMM_B (slave requests permission)
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_COMM_B, ctx.State);

    // Step 2: Master approves (external sets COMM_B_OK)
    ctx.State = STATE_COMM_B_OK;
    ctx.BalancedState[0] = STATE_COMM_B_OK;
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_EQUAL_INT(30, ctx.ActivationMode);

    // Step 3: DiodeCheck
    evse_tick_10ms(&ctx, PILOT_DIODE);
    TEST_ASSERT_EQUAL_INT(1, ctx.DiodeCheck);

    // Step 4: Car requests charge → COMM_C (slave requests charge permission)
    for (int i = 0; i < 55; i++) {
        evse_tick_10ms(&ctx, PILOT_6V);
    }
    TEST_ASSERT_EQUAL_INT(STATE_COMM_C, ctx.State);

    // Step 5: Master approves charge
    ctx.State = STATE_COMM_C_OK;
    ctx.BalancedState[0] = STATE_COMM_C_OK;
    evse_tick_10ms(&ctx, PILOT_6V);
    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.State);
    TEST_ASSERT_TRUE(ctx.contactor1_state);

    // Step 6: Car stops charging
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);

    // Step 7: Car disconnects
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
    TEST_ASSERT_FALSE(ctx.contactor1_state);
}

// ===================================================================
// Scenario C: OCPP authorization flow
// ===================================================================

/*
 * @feature End-to-End Charging
 * @req REQ-E2E-003
 * @scenario OCPP grants access mid-session, car starts charging
 * @given Standalone EVSE with OcppMode=true, AccessStatus=OFF
 * @when Car connects, OCPP grants access, car charges, OCPP revokes
 * @then A (blocked) → A (OCPP grants) → B → C → C1 (revoked) → B1
 */
void test_e2e_ocpp_authorization_flow(void) {
    setup_standalone();
    ctx.OcppMode = true;
    ctx.OcppCurrentLimit = 16.0f;
    ctx.AccessStatus = OFF;

    // Step 1: Car connects but no OCPP authorization
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);

    // Step 2: OCPP grants access
    evse_set_access(&ctx, ON);
    TEST_ASSERT_EQUAL_INT(ON, ctx.AccessStatus);

    // Step 3: Next tick with 9V → B
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);

    // Step 4: DiodeCheck + 6V → C
    ctx.DiodeCheck = 1;
    ctx.ChargeCurrent = ctx.MaxCurrent * 10;
    for (int i = 0; i < 55; i++) {
        evse_tick_10ms(&ctx, PILOT_6V);
    }
    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.State);
    TEST_ASSERT_TRUE(ctx.contactor1_state);

    // Step 5: OCPP revokes access mid-charge
    evse_set_access(&ctx, OFF);
    TEST_ASSERT_EQUAL_INT(STATE_C1, ctx.State);
    TEST_ASSERT_EQUAL_INT(1024, ctx.last_pwm_duty);

    // Step 6: C1 timer expires → B1
    for (int i = 0; i < 7; i++) {
        evse_tick_1s(&ctx);
    }
    TEST_ASSERT_EQUAL_INT(STATE_B1, ctx.State);
    TEST_ASSERT_FALSE(ctx.contactor1_state);
}

/*
 * @feature End-to-End Charging
 * @req REQ-E2E-004
 * @scenario OCPP denied — car connects but stays in STATE_A
 * @given OCPP mode with AccessStatus=OFF
 * @when Car repeatedly presents 9V pilot
 * @then State stays A for 100+ ticks
 */
void test_e2e_ocpp_denied_stays_in_a(void) {
    setup_standalone();
    ctx.OcppMode = true;
    ctx.OcppCurrentLimit = 16.0f;
    ctx.AccessStatus = OFF;

    for (int i = 0; i < 100; i++) {
        evse_tick_10ms(&ctx, PILOT_9V);
    }
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

// ===================================================================
// Scenario D: Reconnect after disconnect (state cleanliness)
// ===================================================================

/*
 * @feature End-to-End Charging
 * @req REQ-E2E-005
 * @scenario Two full charge cycles — verify no stale state leaks
 * @given Standalone EVSE
 * @when First cycle: connect → charge → disconnect. Second cycle: connect → charge → disconnect.
 * @then Both cycles complete successfully, all state is clean between them
 */
void test_e2e_reconnect_after_disconnect(void) {
    setup_standalone();

    // === FIRST SESSION ===
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);

    evse_tick_10ms(&ctx, PILOT_DIODE);
    TEST_ASSERT_EQUAL_INT(1, ctx.DiodeCheck);

    ctx.ChargeCurrent = ctx.MaxCurrent * 10;
    for (int i = 0; i < 55; i++) {
        evse_tick_10ms(&ctx, PILOT_6V);
    }
    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.State);

    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
    TEST_ASSERT_FALSE(ctx.contactor1_state);

    // Verify state is clean
    TEST_ASSERT_EQUAL_INT(0, ctx.ChargeDelay);
    TEST_ASSERT_EQUAL_INT(0, ctx.ErrorFlags & LESS_6A);

    // === SECOND SESSION ===
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_EQUAL_INT(0, ctx.DiodeCheck);  // Must be reset

    evse_tick_10ms(&ctx, PILOT_DIODE);
    TEST_ASSERT_EQUAL_INT(1, ctx.DiodeCheck);

    ctx.ChargeCurrent = ctx.MaxCurrent * 10;
    for (int i = 0; i < 55; i++) {
        evse_tick_10ms(&ctx, PILOT_6V);
    }
    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.State);
    TEST_ASSERT_TRUE(ctx.contactor1_state);

    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
}

// ===================================================================
// Scenario E: Temperature error during charge + recovery
// ===================================================================

/*
 * @feature End-to-End Charging
 * @req REQ-E2E-006
 * @scenario Temperature spike during STATE_C, recovery after cooldown
 * @given EVSE charging in STATE_C
 * @when Temperature exceeds maxTemp, then cools below hysteresis
 * @then C → C1 → B1 (TEMP_HIGH), cooldown clears error
 */
void test_e2e_temp_error_during_charge(void) {
    setup_standalone();
    ctx.maxTemp = 65;
    ctx.TempEVSE = 25;

    evse_set_state(&ctx, STATE_C);
    TEST_ASSERT_TRUE(ctx.contactor1_state);

    // Temperature spikes
    ctx.TempEVSE = 70;
    evse_tick_1s(&ctx);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & TEMP_HIGH) != 0);
    TEST_ASSERT_EQUAL_INT(STATE_C1, ctx.State);

    // C1 timer → B1
    for (int i = 0; i < 7; i++) {
        evse_tick_1s(&ctx);
    }
    TEST_ASSERT_EQUAL_INT(STATE_B1, ctx.State);
    TEST_ASSERT_FALSE(ctx.contactor1_state);

    // Temperature cools to recovery threshold (maxTemp - 10)
    ctx.TempEVSE = 54;
    evse_tick_1s(&ctx);
    TEST_ASSERT_FALSE(ctx.ErrorFlags & TEMP_HIGH);
}

// ===================================================================
// Scenario F: CT_NOCOMM during charge + recovery
// ===================================================================

/*
 * @feature End-to-End Charging
 * @req REQ-E2E-007
 * @scenario Meter communication lost during STATE_C
 * @given EVSE charging in Smart mode with MainsMeter
 * @when Meter timeout reaches 0
 * @then CT_NOCOMM set, power unavailable, C → C1
 */
void test_e2e_ct_nocomm_during_charge(void) {
    setup_standalone();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterTimeout = 2;

    evse_set_state(&ctx, STATE_C);

    // Countdown
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(1, ctx.MainsMeterTimeout);
    TEST_ASSERT_FALSE(ctx.ErrorFlags & CT_NOCOMM);

    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(0, ctx.MainsMeterTimeout);
    // Next tick: timeout=0, error set
    evse_tick_1s(&ctx);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & CT_NOCOMM) != 0);

    // Recovery: meter comes back
    ctx.MainsMeterTimeout = 10;
    evse_tick_1s(&ctx);
    TEST_ASSERT_FALSE(ctx.ErrorFlags & CT_NOCOMM);
}

// ===================================================================
// Scenario G: Multiple simultaneous errors
// ===================================================================

/*
 * @feature End-to-End Charging
 * @req REQ-E2E-008
 * @scenario TEMP_HIGH + CT_NOCOMM simultaneously during charge
 * @given EVSE charging in Smart mode
 * @when Temperature spikes AND meter fails at same time
 * @then Both errors set, both must clear for full recovery
 */
void test_e2e_multiple_errors_during_charge(void) {
    setup_standalone();
    ctx.Mode = MODE_SMART;
    ctx.MainsMeterType = 1;
    ctx.MainsMeterTimeout = 0;
    ctx.maxTemp = 65;
    ctx.TempEVSE = 70;

    evse_set_state(&ctx, STATE_C);

    evse_tick_1s(&ctx);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & TEMP_HIGH) != 0);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & CT_NOCOMM) != 0);

    // Recover meter first
    ctx.MainsMeterTimeout = 10;
    evse_tick_1s(&ctx);
    TEST_ASSERT_FALSE(ctx.ErrorFlags & CT_NOCOMM);
    TEST_ASSERT_TRUE((ctx.ErrorFlags & TEMP_HIGH) != 0);

    // Cool down
    ctx.TempEVSE = 50;
    evse_tick_1s(&ctx);
    TEST_ASSERT_FALSE(ctx.ErrorFlags & TEMP_HIGH);
}

// ===================================================================
// Scenario H: No B→C without DiodeCheck
// ===================================================================

/*
 * @feature End-to-End Charging
 * @req REQ-E2E-009
 * @scenario 6V pilot without DiodeCheck does not transition to STATE_C
 * @given EVSE in STATE_B, DiodeCheck=0
 * @when 55 ticks of 6V pilot
 * @then State stays STATE_B (DiodeCheck blocks B→C)
 */
void test_e2e_no_charge_without_diode(void) {
    setup_standalone();

    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_EQUAL_INT(0, ctx.DiodeCheck);

    // Do NOT set DiodeCheck — skip the PILOT_DIODE step
    ctx.ChargeCurrent = ctx.MaxCurrent * 10;
    for (int i = 0; i < 100; i++) {
        evse_tick_10ms(&ctx, PILOT_6V);
    }
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);  // Blocked by DiodeCheck
}

// ===================================================================
// Scenario I: Charge delay blocks charging
// ===================================================================

/*
 * @feature End-to-End Charging
 * @req REQ-E2E-010
 * @scenario ChargeDelay > 0 blocks A→B transition, sends to B1
 * @given Standalone EVSE with ChargeDelay=10
 * @when Car connects (9V)
 * @then Goes to B1 (not B), must wait for delay to expire
 */
void test_e2e_charge_delay_blocks_charging(void) {
    setup_standalone();
    ctx.ChargeDelay = 10;

    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B1, ctx.State);  // Not STATE_B

    // Count down delay
    for (int i = 0; i < 10; i++) {
        evse_tick_1s(&ctx);
    }
    TEST_ASSERT_EQUAL_INT(0, ctx.ChargeDelay);
}

// ===================================================================
// Scenario J: StateTimer reset on C→B transition
// ===================================================================

/*
 * @feature End-to-End Charging
 * @req REQ-E2E-011
 * @scenario StateTimer is properly reset between charge sessions
 * @given EVSE in STATE_C with accumulated StateTimer
 * @when Car stops (9V → B), then requests charge again (6V)
 * @then Debounce starts from 0, requiring full 500ms
 */
void test_e2e_state_timer_reset_on_c_to_b(void) {
    setup_standalone();
    evse_set_state(&ctx, STATE_C);

    // Accumulate StateTimer via PILOT_SHORT
    for (int i = 0; i < 30; i++) {
        evse_tick_10ms(&ctx, PILOT_SHORT);
    }
    TEST_ASSERT_EQUAL_INT(30, ctx.StateTimer);

    // Car stops → 9V → B
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);

    // 9V in STATE_B resets StateTimer
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(0, ctx.StateTimer);

    // Verify debounce starts fresh — 50 ticks not enough
    ctx.DiodeCheck = 1;
    ctx.ChargeCurrent = ctx.MaxCurrent * 10;
    for (int i = 0; i < 50; i++) {
        evse_tick_10ms(&ctx, PILOT_6V);
    }
    TEST_ASSERT_NOT_EQUAL(STATE_C, ctx.State);

    evse_tick_10ms(&ctx, PILOT_6V);  // 51st tick
    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.State);
}

// ===================================================================
// Scenario K: Power reduction C → C1 → B1
// ===================================================================

/*
 * @feature End-to-End Charging
 * @req REQ-E2E-012
 * @scenario Power unavailable during charge suspends charging
 * @given EVSE charging in STATE_C
 * @when evse_set_power_unavailable is called
 * @then C → C1 (PWM off) → B1 (contactors open after C1Timer)
 */
void test_e2e_power_unavailable_c_to_c1_to_b1(void) {
    setup_standalone();
    evse_set_state(&ctx, STATE_C);
    TEST_ASSERT_TRUE(ctx.contactor1_state);

    evse_set_power_unavailable(&ctx);
    TEST_ASSERT_EQUAL_INT(STATE_C1, ctx.State);
    TEST_ASSERT_EQUAL_INT(1024, ctx.last_pwm_duty);
    TEST_ASSERT_EQUAL_INT(6, ctx.C1Timer);

    // C1Timer counts down
    for (int i = 0; i < 7; i++) {
        evse_tick_1s(&ctx);
    }
    TEST_ASSERT_EQUAL_INT(STATE_B1, ctx.State);
    TEST_ASSERT_FALSE(ctx.contactor1_state);
}

int main(void) {
    TEST_SUITE_BEGIN("End-to-End Charging Flows");

    /* Happy paths */
    RUN_TEST(test_e2e_standalone_happy_path);
    RUN_TEST(test_e2e_slave_happy_path);

    /* OCPP */
    RUN_TEST(test_e2e_ocpp_authorization_flow);
    RUN_TEST(test_e2e_ocpp_denied_stays_in_a);

    /* Reconnect */
    RUN_TEST(test_e2e_reconnect_after_disconnect);

    /* Error scenarios */
    RUN_TEST(test_e2e_temp_error_during_charge);
    RUN_TEST(test_e2e_ct_nocomm_during_charge);
    RUN_TEST(test_e2e_multiple_errors_during_charge);

    /* Safety guards */
    RUN_TEST(test_e2e_no_charge_without_diode);
    RUN_TEST(test_e2e_charge_delay_blocks_charging);
    RUN_TEST(test_e2e_state_timer_reset_on_c_to_b);
    RUN_TEST(test_e2e_power_unavailable_c_to_c1_to_b1);

    TEST_SUITE_RESULTS();
}
