/*
 * test_bridge_transaction.c - Bridge transaction integrity tests
 *
 * Verifies that bridge transactions (sync_to → operate → sync_from) produce
 * correct results when composed sequentially. On ESP32, these transactions
 * are protected by a FreeRTOS mutex; in native tests the lock/unlock are
 * no-ops but the functional semantics are verified.
 *
 * These tests cover the scenarios that the bridge mutex protects against:
 * - AccessTimer countdown reaching zero and clearing AccessStatus
 * - setAccess(ON) surviving subsequent tick_10ms cycles
 * - Interleaved tick_10ms and tick_1s producing consistent state
 */

#include "test_framework.h"
#include "evse_ctx.h"
#include "evse_state_machine.h"

// Stubs for bridge lock/unlock — no-ops in native test builds (no FreeRTOS).
// On ESP32 these are implemented in evse_bridge.cpp with a real mutex.
void evse_bridge_lock(void) {}
void evse_bridge_unlock(void) {}

static evse_ctx_t ctx;

static void setup_basic(void) {
    evse_init(&ctx, NULL);
    ctx.Mode = MODE_NORMAL;
    ctx.LoadBl = 0;
    ctx.ChargeCurrent = 130;
    ctx.MinCurrent = 6;
    ctx.MaxCurrent = 32;
    ctx.MaxCapacity = 32;
}

// ---- Bridge lock/unlock API contract ----

/*
 * @feature Bridge Transaction Integrity
 * @req REQ-SM-100
 * @scenario Bridge lock/unlock API is callable
 * @given The test environment (native build, no FreeRTOS)
 * @when evse_bridge_lock and evse_bridge_unlock are called
 * @then They complete without error (no-ops in native builds)
 */
void test_bridge_lock_unlock_callable(void) {
    evse_bridge_lock();
    evse_bridge_unlock();
    TEST_ASSERT_EQUAL_INT(1, 1);  // Reached without crash
}

// ---- AccessTimer countdown integrity ----

/*
 * @feature Bridge Transaction Integrity
 * @req REQ-AUTH-024
 * @scenario AccessTimer counts down to zero over 60 sequential tick_1s calls
 * @given EVSE in STATE_A with AccessStatus ON and AccessTimer armed to 60
 * @when tick_1s is called 60 times (simulating 60 seconds with no concurrent interference)
 * @then AccessTimer reaches 0 and AccessStatus is set to OFF
 */
void test_access_timer_full_countdown(void) {
    setup_basic();
    ctx.State = STATE_A;
    ctx.AccessStatus = ON;
    ctx.RFIDReader = 2;
    ctx.AccessTimer = 60;  // RFIDLOCKTIME

    // Simulate 60 seconds of tick_1s calls
    for (int i = 0; i < 59; i++) {
        evse_tick_1s(&ctx);
        TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
        TEST_ASSERT_EQUAL_INT(60 - 1 - i, ctx.AccessTimer);
        TEST_ASSERT_EQUAL_INT(ON, ctx.AccessStatus);
    }

    // 60th tick: timer reaches 0, AccessStatus cleared
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(0, ctx.AccessTimer);
    TEST_ASSERT_EQUAL_INT(OFF, ctx.AccessStatus);
}

/*
 * @feature Bridge Transaction Integrity
 * @req REQ-AUTH-025
 * @scenario AccessTimer countdown survives interleaved tick_10ms calls
 * @given EVSE in STATE_A with AccessTimer=2, pilot at 12V
 * @when tick_10ms and tick_1s are called in alternating sequence
 * @then AccessTimer still reaches 0 and AccessStatus is cleared
 */
void test_access_timer_survives_interleaved_ticks(void) {
    setup_basic();
    ctx.State = STATE_A;
    ctx.AccessStatus = ON;
    ctx.RFIDReader = 2;
    ctx.AccessTimer = 2;

    // Simulate interleaved execution: 10ms tick, then 1s tick, repeat
    // tick_10ms with PILOT_12V in STATE_A should NOT re-arm the timer
    // (because AccessTimer != 0)
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(2, ctx.AccessTimer);  // Not re-armed, not decremented
    TEST_ASSERT_EQUAL_INT(ON, ctx.AccessStatus);

    // First 1s tick: 2 → 1
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(1, ctx.AccessTimer);
    TEST_ASSERT_EQUAL_INT(ON, ctx.AccessStatus);

    // More 10ms ticks between 1s ticks (normal operation: ~100 per second)
    for (int i = 0; i < 10; i++) {
        evse_tick_10ms(&ctx, PILOT_12V);
    }
    TEST_ASSERT_EQUAL_INT(1, ctx.AccessTimer);  // Not modified by tick_10ms
    TEST_ASSERT_EQUAL_INT(ON, ctx.AccessStatus);

    // Second 1s tick: 1 → 0, AccessStatus → OFF
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(0, ctx.AccessTimer);
    TEST_ASSERT_EQUAL_INT(OFF, ctx.AccessStatus);
}

/*
 * @feature Bridge Transaction Integrity
 * @req REQ-AUTH-026
 * @scenario AccessTimer is not re-armed after expiry
 * @given EVSE in STATE_A after AccessTimer just expired (AccessStatus=OFF, AccessTimer=0)
 * @when tick_10ms runs with PILOT_12V
 * @then AccessTimer stays 0 (re-arm requires AccessStatus==ON)
 */
void test_access_timer_not_rearmed_after_expiry(void) {
    setup_basic();
    ctx.State = STATE_A;
    ctx.AccessStatus = OFF;
    ctx.RFIDReader = 2;
    ctx.AccessTimer = 0;

    // tick_10ms should NOT re-arm because AccessStatus is OFF
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(0, ctx.AccessTimer);
    TEST_ASSERT_EQUAL_INT(OFF, ctx.AccessStatus);

    // Multiple ticks: still no re-arm
    for (int i = 0; i < 100; i++) {
        evse_tick_10ms(&ctx, PILOT_12V);
    }
    TEST_ASSERT_EQUAL_INT(0, ctx.AccessTimer);
    TEST_ASSERT_EQUAL_INT(OFF, ctx.AccessStatus);
}

// ---- setAccess(ON) persistence through ticks ----

/*
 * @feature Bridge Transaction Integrity
 * @req REQ-AUTH-027
 * @scenario OCPP setAccess(ON) triggers A→B transition on next tick_10ms
 * @given EVSE in STATE_A with AccessStatus=OFF, car plugged in (pilot 9V)
 * @when setAccess(ON) is called, then tick_10ms runs
 * @then State transitions from A to B (AccessStatus=ON enables the transition)
 */
void test_set_access_on_enables_a_to_b(void) {
    setup_basic();
    ctx.State = STATE_A;
    ctx.AccessStatus = OFF;
    ctx.ErrorFlags = NO_ERROR;
    ctx.ChargeDelay = 0;

    // Car is plugged in but waiting for RFID/OCPP authorization
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);  // Blocked: no access

    // OCPP authorizes
    evse_set_access(&ctx, ON);
    TEST_ASSERT_EQUAL_INT(ON, ctx.AccessStatus);

    // Next tick_10ms should trigger A→B
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
}

/*
 * @feature Bridge Transaction Integrity
 * @req REQ-AUTH-028
 * @scenario AccessStatus ON persists through multiple tick_10ms cycles
 * @given EVSE in STATE_B with AccessStatus=ON (car connected, authorized)
 * @when tick_10ms is called repeatedly with PILOT_9V
 * @then AccessStatus remains ON (not corrupted by tick processing)
 */
void test_access_status_persists_through_ticks(void) {
    setup_basic();
    ctx.State = STATE_B;
    ctx.AccessStatus = ON;
    ctx.ErrorFlags = NO_ERROR;
    ctx.ChargeDelay = 0;
    ctx.DiodeCheck = 0;
    ctx.ActivationMode = 30;

    // Simulate 50 tick_10ms cycles in STATE_B with PILOT_9V
    for (int i = 0; i < 50; i++) {
        evse_tick_10ms(&ctx, PILOT_9V);
        TEST_ASSERT_EQUAL_INT(ON, ctx.AccessStatus);
    }
}

/*
 * @feature Bridge Transaction Integrity
 * @req REQ-AUTH-029
 * @scenario Full OCPP charge cycle: authorize → charge → disconnect → new authorize
 * @given EVSE idle in STATE_A, OCPP mode, car not connected
 * @when First user charges and disconnects, then second user plugs in and authorizes
 * @then Second user's authorization succeeds and A→B transition occurs
 */
void test_ocpp_full_cycle_two_users(void) {
    setup_basic();
    ctx.State = STATE_A;
    ctx.AccessStatus = OFF;
    ctx.RFIDReader = 2;  // EnableOne (simulates OCPP-like behavior)
    ctx.ErrorFlags = NO_ERROR;
    ctx.ChargeDelay = 0;

    // === First user ===
    // OCPP authorizes first user
    evse_set_access(&ctx, ON);

    // Car plugs in → A→B
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);

    // Car requests charge → B→C (after diode check)
    ctx.DiodeCheck = 1;
    ctx.StateTimer = 51;  // Past 500ms debounce
    evse_tick_10ms(&ctx, PILOT_6V);
    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.State);

    // Charging...
    for (int i = 0; i < 10; i++) {
        evse_tick_10ms(&ctx, PILOT_6V);
    }
    TEST_ASSERT_EQUAL_INT(STATE_C, ctx.State);
    TEST_ASSERT_EQUAL_INT(ON, ctx.AccessStatus);

    // Tesla disconnect: C→B (pilot 9V)
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);

    // Cable pulled: B→A (pilot 12V) — AccessStatus cleared by fix
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
    TEST_ASSERT_EQUAL_INT(OFF, ctx.AccessStatus);
    TEST_ASSERT_EQUAL_INT(0, ctx.AccessTimer);

    // === Second user ===
    // Time passes (simulate 10 seconds of idle)
    for (int i = 0; i < 10; i++) {
        evse_tick_1s(&ctx);
        for (int j = 0; j < 100; j++) {
            evse_tick_10ms(&ctx, PILOT_12V);
        }
    }
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);
    TEST_ASSERT_EQUAL_INT(OFF, ctx.AccessStatus);

    // Second car plugs in — should NOT auto-start
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_A, ctx.State);

    // OCPP authorizes second user
    evse_set_access(&ctx, ON);
    TEST_ASSERT_EQUAL_INT(ON, ctx.AccessStatus);

    // Next tick: A→B transition
    evse_tick_10ms(&ctx, PILOT_9V);
    TEST_ASSERT_EQUAL_INT(STATE_B, ctx.State);
    TEST_ASSERT_EQUAL_INT(ON, ctx.AccessStatus);
}

/*
 * @feature Bridge Transaction Integrity
 * @req REQ-AUTH-030
 * @scenario AccessTimer full countdown with interleaved 10ms ticks (real timing)
 * @given EVSE in STATE_A, AccessStatus=ON, AccessTimer=60, pilot 12V
 * @when 60 rounds of (100x tick_10ms + 1x tick_1s) simulate real-world timing
 * @then AccessTimer reaches 0 and AccessStatus transitions to OFF
 */
void test_access_timer_real_world_timing(void) {
    setup_basic();
    ctx.State = STATE_A;
    ctx.AccessStatus = ON;
    ctx.RFIDReader = 2;
    ctx.AccessTimer = 60;

    // Simulate 60 seconds of real operation: 100 tick_10ms per tick_1s
    for (int sec = 0; sec < 59; sec++) {
        for (int ms = 0; ms < 100; ms++) {
            evse_tick_10ms(&ctx, PILOT_12V);
        }
        evse_tick_1s(&ctx);
        TEST_ASSERT_EQUAL_INT(59 - sec, ctx.AccessTimer);
        TEST_ASSERT_EQUAL_INT(ON, ctx.AccessStatus);
    }

    // Final second
    for (int ms = 0; ms < 100; ms++) {
        evse_tick_10ms(&ctx, PILOT_12V);
    }
    evse_tick_1s(&ctx);
    TEST_ASSERT_EQUAL_INT(0, ctx.AccessTimer);
    TEST_ASSERT_EQUAL_INT(OFF, ctx.AccessStatus);

    // After expiry: tick_10ms should NOT re-arm
    evse_tick_10ms(&ctx, PILOT_12V);
    TEST_ASSERT_EQUAL_INT(0, ctx.AccessTimer);
    TEST_ASSERT_EQUAL_INT(OFF, ctx.AccessStatus);
}

// ---- Main ----
int main(void) {
    TEST_SUITE_BEGIN("Bridge Transaction Integrity");

    RUN_TEST(test_bridge_lock_unlock_callable);
    RUN_TEST(test_access_timer_full_countdown);
    RUN_TEST(test_access_timer_survives_interleaved_ticks);
    RUN_TEST(test_access_timer_not_rearmed_after_expiry);
    RUN_TEST(test_set_access_on_enables_a_to_b);
    RUN_TEST(test_access_status_persists_through_ticks);
    RUN_TEST(test_ocpp_full_cycle_two_users);
    RUN_TEST(test_access_timer_real_world_timing);

    TEST_SUITE_RESULTS();
}
