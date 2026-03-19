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
    RUN_TEST(test_ev_not_ready_at_nok);

    TEST_SUITE_RESULTS();
}
