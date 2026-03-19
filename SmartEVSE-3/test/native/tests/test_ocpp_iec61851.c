/*
 * test_ocpp_iec61851.c - IEC 61851 to OCPP StatusNotification mapping tests
 *
 * Tests the pure C mapping from IEC 61851 state letters (A-F) to OCPP 1.6
 * ChargePointStatus values. This mapping is used by EVCC and other external
 * controllers that need accurate CP state reporting via OCPP.
 */

#include "test_framework.h"
#include "ocpp_logic.h"
#include <string.h>

/* ---- State A: No vehicle connected ---- */

/*
 * @feature OCPP IEC 61851 Status Mapping
 * @req REQ-OCPP-090
 * @scenario State A without active transaction maps to Available
 * @given IEC 61851 state is A (no vehicle), no transaction active
 * @when ocpp_iec61851_to_status is called
 * @then Returns "Available"
 */
void test_iec_a_no_tx_available(void) {
    TEST_ASSERT_EQUAL_STRING(OCPP_STATUS_AVAILABLE,
        ocpp_iec61851_to_status('A', false, false));
}

/*
 * @feature OCPP IEC 61851 Status Mapping
 * @req REQ-OCPP-090
 * @scenario State A with active transaction maps to Finishing
 * @given IEC 61851 state is A (no vehicle), transaction still active (just unplugged)
 * @when ocpp_iec61851_to_status is called
 * @then Returns "Finishing" because the transaction is ending
 */
void test_iec_a_tx_active_finishing(void) {
    TEST_ASSERT_EQUAL_STRING(OCPP_STATUS_FINISHING,
        ocpp_iec61851_to_status('A', false, true));
}

/* ---- State B: Vehicle connected, not charging ---- */

/*
 * @feature OCPP IEC 61851 Status Mapping
 * @req REQ-OCPP-091
 * @scenario State B without transaction maps to Preparing
 * @given IEC 61851 state is B (vehicle connected), no transaction
 * @when ocpp_iec61851_to_status is called
 * @then Returns "Preparing" because the vehicle is waiting for authorization
 */
void test_iec_b_no_tx_preparing(void) {
    TEST_ASSERT_EQUAL_STRING(OCPP_STATUS_PREPARING,
        ocpp_iec61851_to_status('B', true, false));
}

/*
 * @feature OCPP IEC 61851 Status Mapping
 * @req REQ-OCPP-091
 * @scenario State B with active transaction maps to SuspendedEV
 * @given IEC 61851 state is B (connected but not drawing), transaction active
 * @when ocpp_iec61851_to_status is called
 * @then Returns "SuspendedEV" because the EV has paused charging
 */
void test_iec_b_tx_active_suspended_ev(void) {
    TEST_ASSERT_EQUAL_STRING(OCPP_STATUS_SUSPENDED_EV,
        ocpp_iec61851_to_status('B', true, true));
}

/* ---- State C: Charging ---- */

/*
 * @feature OCPP IEC 61851 Status Mapping
 * @req REQ-OCPP-092
 * @scenario State C with EVSE offering current maps to Charging
 * @given IEC 61851 state is C (charging), EVSE ready (PWM > 0)
 * @when ocpp_iec61851_to_status is called
 * @then Returns "Charging"
 */
void test_iec_c_evse_ready_charging(void) {
    TEST_ASSERT_EQUAL_STRING(OCPP_STATUS_CHARGING,
        ocpp_iec61851_to_status('C', true, true));
}

/*
 * @feature OCPP IEC 61851 Status Mapping
 * @req REQ-OCPP-092
 * @scenario State C with EVSE not offering current maps to SuspendedEVSE
 * @given IEC 61851 state is C, EVSE not ready (current = 0, e.g. OCPP limit)
 * @when ocpp_iec61851_to_status is called
 * @then Returns "SuspendedEVSE" because the EVSE has paused charging
 */
void test_iec_c_evse_not_ready_suspended_evse(void) {
    TEST_ASSERT_EQUAL_STRING(OCPP_STATUS_SUSPENDED_EVSE,
        ocpp_iec61851_to_status('C', false, true));
}

/* ---- State D: Charging with ventilation ---- */

/*
 * @feature OCPP IEC 61851 Status Mapping
 * @req REQ-OCPP-093
 * @scenario State D with EVSE ready maps to Charging
 * @given IEC 61851 state is D (charging with ventilation), EVSE ready
 * @when ocpp_iec61851_to_status is called
 * @then Returns "Charging" (same as State C for OCPP)
 */
void test_iec_d_evse_ready_charging(void) {
    TEST_ASSERT_EQUAL_STRING(OCPP_STATUS_CHARGING,
        ocpp_iec61851_to_status('D', true, true));
}

/*
 * @feature OCPP IEC 61851 Status Mapping
 * @req REQ-OCPP-093
 * @scenario State D with EVSE not ready maps to SuspendedEVSE
 * @given IEC 61851 state is D, EVSE not ready
 * @when ocpp_iec61851_to_status is called
 * @then Returns "SuspendedEVSE"
 */
void test_iec_d_evse_not_ready_suspended_evse(void) {
    TEST_ASSERT_EQUAL_STRING(OCPP_STATUS_SUSPENDED_EVSE,
        ocpp_iec61851_to_status('D', false, true));
}

/* ---- State E/F: Error / Not available ---- */

/*
 * @feature OCPP IEC 61851 Status Mapping
 * @req REQ-OCPP-094
 * @scenario State E maps to Faulted
 * @given IEC 61851 state is E (error)
 * @when ocpp_iec61851_to_status is called
 * @then Returns "Faulted"
 */
void test_iec_e_faulted(void) {
    TEST_ASSERT_EQUAL_STRING(OCPP_STATUS_FAULTED,
        ocpp_iec61851_to_status('E', false, false));
}

/*
 * @feature OCPP IEC 61851 Status Mapping
 * @req REQ-OCPP-094
 * @scenario State F maps to Faulted
 * @given IEC 61851 state is F (not available)
 * @when ocpp_iec61851_to_status is called
 * @then Returns "Faulted"
 */
void test_iec_f_faulted(void) {
    TEST_ASSERT_EQUAL_STRING(OCPP_STATUS_FAULTED,
        ocpp_iec61851_to_status('F', false, false));
}

/*
 * @feature OCPP IEC 61851 Status Mapping
 * @req REQ-OCPP-094
 * @scenario Unknown state maps to Faulted
 * @given IEC 61851 state is an invalid character
 * @when ocpp_iec61851_to_status is called
 * @then Returns "Faulted" as a safe default
 */
void test_iec_unknown_faulted(void) {
    TEST_ASSERT_EQUAL_STRING(OCPP_STATUS_FAULTED,
        ocpp_iec61851_to_status('X', false, false));
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("OCPP IEC 61851 Status Mapping");

    RUN_TEST(test_iec_a_no_tx_available);
    RUN_TEST(test_iec_a_tx_active_finishing);
    RUN_TEST(test_iec_b_no_tx_preparing);
    RUN_TEST(test_iec_b_tx_active_suspended_ev);
    RUN_TEST(test_iec_c_evse_ready_charging);
    RUN_TEST(test_iec_c_evse_not_ready_suspended_evse);
    RUN_TEST(test_iec_d_evse_ready_charging);
    RUN_TEST(test_iec_d_evse_not_ready_suspended_evse);
    RUN_TEST(test_iec_e_faulted);
    RUN_TEST(test_iec_f_faulted);
    RUN_TEST(test_iec_unknown_faulted);

    TEST_SUITE_RESULTS();
}
