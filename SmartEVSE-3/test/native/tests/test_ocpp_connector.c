/*
 * test_ocpp_connector.c - OCPP connector state mapping tests
 *
 * Tests the pure C connector state functions extracted from esp32.cpp.
 * Maps CP pilot voltage levels to OCPP connector plugged/EV-ready states.
 */

#include "test_framework.h"
#include "ocpp_logic.h"
#include "evse_ctx.h"

/* ---- Connector plugged ---- */

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-040
 * @scenario CP voltage PILOT_3V indicates connector plugged
 * @given CP voltage is PILOT_3V (3V, State C/D)
 * @when ocpp_is_connector_plugged is called
 * @then Returns true because PILOT_3V is within plugged range
 */
void test_connector_plugged_at_3v(void) {
    TEST_ASSERT_TRUE(ocpp_is_connector_plugged(PILOT_3V));
}

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-040
 * @scenario CP voltage PILOT_6V indicates connector plugged
 * @given CP voltage is PILOT_6V (6V, State C)
 * @when ocpp_is_connector_plugged is called
 * @then Returns true because PILOT_6V is within plugged range
 */
void test_connector_plugged_at_6v(void) {
    TEST_ASSERT_TRUE(ocpp_is_connector_plugged(PILOT_6V));
}

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-040
 * @scenario CP voltage PILOT_9V indicates connector plugged
 * @given CP voltage is PILOT_9V (9V, State B)
 * @when ocpp_is_connector_plugged is called
 * @then Returns true because PILOT_9V is within plugged range
 */
void test_connector_plugged_at_9v(void) {
    TEST_ASSERT_TRUE(ocpp_is_connector_plugged(PILOT_9V));
}

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-041
 * @scenario CP voltage PILOT_12V indicates connector unplugged
 * @given CP voltage is PILOT_12V (12V, State A)
 * @when ocpp_is_connector_plugged is called
 * @then Returns false because PILOT_12V means no vehicle connected
 */
void test_connector_unplugged_at_12v(void) {
    TEST_ASSERT_FALSE(ocpp_is_connector_plugged(PILOT_12V));
}

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-042
 * @scenario CP voltage PILOT_NOK indicates connector unplugged
 * @given CP voltage is PILOT_NOK (0, fault condition)
 * @when ocpp_is_connector_plugged is called
 * @then Returns false because PILOT_NOK is outside plugged range
 */
void test_connector_unplugged_at_nok(void) {
    TEST_ASSERT_FALSE(ocpp_is_connector_plugged(PILOT_NOK));
}

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-042
 * @scenario CP voltage PILOT_DIODE indicates connector unplugged
 * @given CP voltage is PILOT_DIODE (1, diode check)
 * @when ocpp_is_connector_plugged is called
 * @then Returns false because PILOT_DIODE is below PILOT_3V
 */
void test_connector_unplugged_at_diode(void) {
    TEST_ASSERT_FALSE(ocpp_is_connector_plugged(PILOT_DIODE));
}

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-042
 * @scenario CP voltage PILOT_SHORT indicates connector unplugged
 * @given CP voltage is PILOT_SHORT (255, short circuit)
 * @when ocpp_is_connector_plugged is called
 * @then Returns false because PILOT_SHORT is above PILOT_9V
 */
void test_connector_unplugged_at_short(void) {
    TEST_ASSERT_FALSE(ocpp_is_connector_plugged(PILOT_SHORT));
}

/* ---- EV ready ---- */

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-043
 * @scenario CP voltage PILOT_3V indicates EV ready (State C/D)
 * @given CP voltage is PILOT_3V
 * @when ocpp_is_ev_ready is called
 * @then Returns true because PILOT_3V is within EV-ready range
 */
void test_ev_ready_at_3v(void) {
    TEST_ASSERT_TRUE(ocpp_is_ev_ready(PILOT_3V));
}

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-043
 * @scenario CP voltage PILOT_6V indicates EV ready (State C)
 * @given CP voltage is PILOT_6V
 * @when ocpp_is_ev_ready is called
 * @then Returns true because PILOT_6V is within EV-ready range
 */
void test_ev_ready_at_6v(void) {
    TEST_ASSERT_TRUE(ocpp_is_ev_ready(PILOT_6V));
}

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-044
 * @scenario CP voltage PILOT_9V indicates EV connected but not ready (State B)
 * @given CP voltage is PILOT_9V
 * @when ocpp_is_ev_ready is called
 * @then Returns false because State B means connected but not requesting charge
 */
void test_ev_not_ready_at_9v(void) {
    TEST_ASSERT_FALSE(ocpp_is_ev_ready(PILOT_9V));
}

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-044
 * @scenario CP voltage PILOT_12V indicates no EV (State A)
 * @given CP voltage is PILOT_12V
 * @when ocpp_is_ev_ready is called
 * @then Returns false because no vehicle is connected
 */
void test_ev_not_ready_at_12v(void) {
    TEST_ASSERT_FALSE(ocpp_is_ev_ready(PILOT_12V));
}

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-044
 * @scenario CP voltage PILOT_NOK indicates EV not ready
 * @given CP voltage is PILOT_NOK (fault)
 * @when ocpp_is_ev_ready is called
 * @then Returns false
 */
void test_ev_not_ready_at_nok(void) {
    TEST_ASSERT_FALSE(ocpp_is_ev_ready(PILOT_NOK));
}

/* ---- Connector lock decision (upstream commit 05c7fc2) ---- */

/*
 * @feature OCPP Connector Lock
 * @req REQ-OCPP-110
 * @scenario Active authorized transaction with car plugged → lock
 * @given tx is present, authorized, active; CP voltage is PILOT_6V (plugged)
 * @when ocpp_should_force_lock is called
 * @then Returns true — connector must be locked during charging
 */
void test_lock_active_tx_plugged(void) {
    bool locked = ocpp_should_force_lock(
        /*tx_present*/ true, /*tx_authorized*/ true,
        /*tx_active_or_running*/ true,
        /*cp_voltage*/ PILOT_6V,
        /*locking_tx_present*/ false,
        /*locking_tx_start_requested*/ false);
    TEST_ASSERT_TRUE(locked);
}

/*
 * @feature OCPP Connector Lock
 * @req REQ-OCPP-110
 * @scenario Lock condition holds at PILOT_3V boundary (charging)
 * @given Active authorized tx, CP voltage is PILOT_3V (lower bound)
 * @when ocpp_should_force_lock is called
 * @then Returns true
 */
void test_lock_active_tx_at_3v_boundary(void) {
    TEST_ASSERT_TRUE(ocpp_should_force_lock(
        true, true, true, PILOT_3V, false, false));
}

/*
 * @feature OCPP Connector Lock
 * @req REQ-OCPP-110
 * @scenario Lock condition holds at PILOT_9V boundary (plugged, not charging yet)
 * @given Active authorized tx, CP voltage is PILOT_9V (upper bound)
 * @when ocpp_should_force_lock is called
 * @then Returns true
 */
void test_lock_active_tx_at_9v_boundary(void) {
    TEST_ASSERT_TRUE(ocpp_should_force_lock(
        true, true, true, PILOT_9V, false, false));
}

/*
 * @feature OCPP Connector Lock
 * @req REQ-OCPP-111
 * @scenario No lock when no transaction present
 * @given tx_present false, but CP voltage and other inputs say "active"
 * @when ocpp_should_force_lock is called
 * @then Returns false — no transaction means no lock
 */
void test_lock_no_tx_no_lock(void) {
    TEST_ASSERT_FALSE(ocpp_should_force_lock(
        false, true, true, PILOT_6V, false, false));
}

/*
 * @feature OCPP Connector Lock
 * @req REQ-OCPP-111
 * @scenario No lock when transaction is not authorized
 * @given tx present but unauthorized, plugged
 * @when ocpp_should_force_lock is called
 * @then Returns false
 */
void test_lock_unauthorized_tx_no_lock(void) {
    TEST_ASSERT_FALSE(ocpp_should_force_lock(
        true, false, true, PILOT_6V, false, false));
}

/*
 * @feature OCPP Connector Lock
 * @req REQ-OCPP-111
 * @scenario No lock when transaction neither active nor running
 * @given tx authorized but isActive==false && isRunning==false
 * @when ocpp_should_force_lock is called
 * @then Returns false
 */
void test_lock_inactive_tx_no_lock(void) {
    TEST_ASSERT_FALSE(ocpp_should_force_lock(
        true, true, false, PILOT_6V, false, false));
}

/*
 * @feature OCPP Connector Lock
 * @req REQ-OCPP-111
 * @scenario No lock when connector unplugged (PILOT_12V)
 * @given Active authorized tx but CP says no vehicle
 * @when ocpp_should_force_lock is called
 * @then Returns false
 */
void test_lock_active_tx_unplugged_no_lock(void) {
    TEST_ASSERT_FALSE(ocpp_should_force_lock(
        true, true, true, PILOT_12V, false, false));
}

/*
 * @feature OCPP Connector Lock
 * @req REQ-OCPP-111
 * @scenario No lock when connector reads PILOT_NOK (fault) and no LockingTx
 * @given Authorized active tx, CP voltage PILOT_NOK
 * @when ocpp_should_force_lock is called
 * @then Returns false — pilot fault means cable state is unknown,
 *       fall back to unlocked unless a LockingTx demands otherwise
 */
void test_lock_pilot_nok_no_lock(void) {
    TEST_ASSERT_FALSE(ocpp_should_force_lock(
        true, true, true, PILOT_NOK, false, false));
}

/*
 * @feature OCPP Connector Lock
 * @req REQ-OCPP-112
 * @scenario LockingTx with start requested keeps connector locked
 * @given No regular tx active, LockingTx present and StartSync requested
 * @when ocpp_should_force_lock is called
 * @then Returns true — RFID-locked connector waits for matching swipe
 */
void test_lock_locking_tx_start_requested(void) {
    TEST_ASSERT_TRUE(ocpp_should_force_lock(
        false, false, false, PILOT_12V, true, true));
}

/*
 * @feature OCPP Connector Lock
 * @req REQ-OCPP-112
 * @scenario LockingTx without start request does not force lock
 * @given LockingTx present but its StartSync has not been requested
 * @when ocpp_should_force_lock is called
 * @then Returns false
 */
void test_lock_locking_tx_no_start_request(void) {
    TEST_ASSERT_FALSE(ocpp_should_force_lock(
        false, false, false, PILOT_12V, true, false));
}

/*
 * @feature OCPP Connector Lock
 * @req REQ-OCPP-113
 * @scenario All-false baseline returns false
 * @given Every input false / PILOT_NOK
 * @when ocpp_should_force_lock is called
 * @then Returns false — no condition triggers
 */
void test_lock_all_false_baseline(void) {
    TEST_ASSERT_FALSE(ocpp_should_force_lock(
        false, false, false, PILOT_NOK, false, false));
}

/* ---- Occupied-input decision (upstream afd72a8, fixes #348) ---- */

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-120
 * @scenario LockingTx present → occupied (pre-existing condition)
 * @given locking_tx_present=true, no recent StopTx
 * @when ocpp_should_report_occupied is called
 * @then Returns true
 */
void test_occupied_locking_tx_present(void) {
    TEST_ASSERT_TRUE(ocpp_should_report_occupied(
        /*locking_tx_present*/ true,
        /*tx_notif_defined*/   false,
        /*tx_notif_is_stoptx*/ false,
        /*now_ms*/             1000000UL,
        /*last_tx_notif_ms*/   0UL));
}

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-121
 * @scenario StopTx within grace window → occupied (Finishing)
 * @given No locking tx, StopTx fired 500 ms ago (< 2000 ms grace)
 * @when ocpp_should_report_occupied is called
 * @then Returns true so CSMS sees Finishing before Available
 */
void test_occupied_stoptx_inside_grace_window(void) {
    unsigned long now = 1000000UL;
    TEST_ASSERT_TRUE(ocpp_should_report_occupied(
        /*locking_tx_present*/ false,
        /*tx_notif_defined*/   true,
        /*tx_notif_is_stoptx*/ true,
        /*now_ms*/             now,
        /*last_tx_notif_ms*/   now - 500UL));
}

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-121
 * @scenario StopTx exactly at grace boundary → NOT occupied (< is strict)
 * @given No locking tx, StopTx fired exactly OCPP_FINISHING_GRACE_MS ago
 * @when ocpp_should_report_occupied is called
 * @then Returns false — grace window has elapsed
 */
void test_occupied_stoptx_at_grace_boundary_exclusive(void) {
    unsigned long now = 1000000UL;
    TEST_ASSERT_FALSE(ocpp_should_report_occupied(
        /*locking_tx_present*/ false,
        /*tx_notif_defined*/   true,
        /*tx_notif_is_stoptx*/ true,
        /*now_ms*/             now,
        /*last_tx_notif_ms*/   now - OCPP_FINISHING_GRACE_MS));
}

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-122
 * @scenario StopTx past grace window → NOT occupied (Available)
 * @given No locking tx, StopTx fired 3 seconds ago
 * @when ocpp_should_report_occupied is called
 * @then Returns false — grace expired, transition to Available
 */
void test_occupied_stoptx_past_grace_window(void) {
    unsigned long now = 1000000UL;
    TEST_ASSERT_FALSE(ocpp_should_report_occupied(
        /*locking_tx_present*/ false,
        /*tx_notif_defined*/   true,
        /*tx_notif_is_stoptx*/ true,
        /*now_ms*/             now,
        /*last_tx_notif_ms*/   now - 3000UL));
}

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-122
 * @scenario Non-StopTx notification within grace → NOT occupied
 * @given tx_notif_defined=true but tx_notif_is_stoptx=false (e.g. StartTx)
 * @when ocpp_should_report_occupied is called
 * @then Returns false — only StopTx triggers Finishing
 */
void test_occupied_non_stoptx_notification_ignored(void) {
    unsigned long now = 1000000UL;
    TEST_ASSERT_FALSE(ocpp_should_report_occupied(
        /*locking_tx_present*/ false,
        /*tx_notif_defined*/   true,
        /*tx_notif_is_stoptx*/ false,
        /*now_ms*/             now,
        /*last_tx_notif_ms*/   now - 500UL));
}

/*
 * @feature OCPP Connector State
 * @req REQ-OCPP-122
 * @scenario Uninitialized notification state → NOT occupied
 * @given tx_notif_defined=false (no notification has ever fired)
 * @when ocpp_should_report_occupied is called
 * @then Returns false — no spurious Finishing on fresh boot
 */
void test_occupied_notification_undefined(void) {
    TEST_ASSERT_FALSE(ocpp_should_report_occupied(
        /*locking_tx_present*/ false,
        /*tx_notif_defined*/   false,
        /*tx_notif_is_stoptx*/ true,    /* would match, but defined=false */
        /*now_ms*/             1000000UL,
        /*last_tx_notif_ms*/   999500UL));
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("OCPP Connector State");

    RUN_TEST(test_connector_plugged_at_3v);
    RUN_TEST(test_connector_plugged_at_6v);
    RUN_TEST(test_connector_plugged_at_9v);
    RUN_TEST(test_connector_unplugged_at_12v);
    RUN_TEST(test_connector_unplugged_at_nok);
    RUN_TEST(test_connector_unplugged_at_diode);
    RUN_TEST(test_connector_unplugged_at_short);
    RUN_TEST(test_ev_ready_at_3v);
    RUN_TEST(test_ev_ready_at_6v);
    RUN_TEST(test_ev_not_ready_at_9v);
    RUN_TEST(test_ev_not_ready_at_12v);
    RUN_TEST(test_lock_active_tx_plugged);
    RUN_TEST(test_lock_active_tx_at_3v_boundary);
    RUN_TEST(test_lock_active_tx_at_9v_boundary);
    RUN_TEST(test_lock_no_tx_no_lock);
    RUN_TEST(test_lock_unauthorized_tx_no_lock);
    RUN_TEST(test_lock_inactive_tx_no_lock);
    RUN_TEST(test_lock_active_tx_unplugged_no_lock);
    RUN_TEST(test_lock_pilot_nok_no_lock);
    RUN_TEST(test_lock_locking_tx_start_requested);
    RUN_TEST(test_lock_locking_tx_no_start_request);
    RUN_TEST(test_lock_all_false_baseline);
    RUN_TEST(test_ev_not_ready_at_nok);
    RUN_TEST(test_occupied_locking_tx_present);
    RUN_TEST(test_occupied_stoptx_inside_grace_window);
    RUN_TEST(test_occupied_stoptx_at_grace_boundary_exclusive);
    RUN_TEST(test_occupied_stoptx_past_grace_window);
    RUN_TEST(test_occupied_non_stoptx_notification_ignored);
    RUN_TEST(test_occupied_notification_undefined);

    TEST_SUITE_RESULTS();
}
