/*
 * test_ocpp_lb.c - OCPP + Load Balancing exclusivity tests
 *
 * Tests the pure C function that detects conflicts between OCPP Smart Charging
 * and internal load balancing. The OCPP Smart Charging callback is only registered
 * at ocppInit() time when LoadBl=0, but LoadBl can change at runtime.
 */

#include "test_framework.h"
#include "ocpp_logic.h"

/* ---- No conflict: standalone mode ---- */

/*
 * @feature OCPP Load Balancing Exclusivity
 * @req REQ-OCPP-030
 * @scenario OCPP+LoadBl=0 has no conflict, Smart Charging active
 * @given OCPP is enabled, LoadBl=0 (standalone), and OCPP was initialized in standalone mode
 * @when ocpp_check_lb_exclusivity is called
 * @then Returns OCPP_LB_OK because Smart Charging and LoadBl are compatible
 */
void test_lb_standalone_no_conflict(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_LB_OK,
        ocpp_check_lb_exclusivity(0, true, true));
}

/*
 * @feature OCPP Load Balancing Exclusivity
 * @req REQ-OCPP-030
 * @scenario OCPP disabled has no conflict regardless of LoadBl
 * @given OCPP is disabled, LoadBl=1
 * @when ocpp_check_lb_exclusivity is called
 * @then Returns OCPP_LB_OK because OCPP is not active
 */
void test_lb_ocpp_disabled_no_conflict(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_LB_OK,
        ocpp_check_lb_exclusivity(1, false, false));
}

/* ---- Conflict: LoadBl != 0 while OCPP active ---- */

/*
 * @feature OCPP Load Balancing Exclusivity
 * @req REQ-OCPP-031
 * @scenario OCPP+LoadBl=1 is a conflict, Smart Charging ineffective
 * @given OCPP is enabled, LoadBl=1 (master), OCPP was initialized standalone
 * @when ocpp_check_lb_exclusivity is called
 * @then Returns OCPP_LB_CONFLICT because the state machine ignores OCPP limits when LoadBl!=0
 */
void test_lb_master_conflict(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_LB_CONFLICT,
        ocpp_check_lb_exclusivity(1, true, true));
}

/*
 * @feature OCPP Load Balancing Exclusivity
 * @req REQ-OCPP-031
 * @scenario OCPP+LoadBl=2 (node) is a conflict
 * @given OCPP is enabled, LoadBl=2 (node), OCPP was initialized standalone
 * @when ocpp_check_lb_exclusivity is called
 * @then Returns OCPP_LB_CONFLICT
 */
void test_lb_node_conflict(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_LB_CONFLICT,
        ocpp_check_lb_exclusivity(2, true, true));
}

/*
 * @feature OCPP Load Balancing Exclusivity
 * @req REQ-OCPP-032
 * @scenario LoadBl changes from 0 to 1 while OCPP active is a conflict
 * @given OCPP is enabled, LoadBl changed to 1 at runtime, was_standalone=true
 * @when ocpp_check_lb_exclusivity is called
 * @then Returns OCPP_LB_CONFLICT because Smart Charging callback is still registered but limits are ignored
 */
void test_lb_changed_0_to_1_conflict(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_LB_CONFLICT,
        ocpp_check_lb_exclusivity(1, true, true));
}

/* ---- Needs reinit: LoadBl was non-zero at init, now zero ---- */

/*
 * @feature OCPP Load Balancing Exclusivity
 * @req REQ-OCPP-033
 * @scenario LoadBl changes from 1 to 0 while OCPP active needs reinit
 * @given OCPP is enabled, LoadBl=0 now, but was_standalone=false (was non-zero at init)
 * @when ocpp_check_lb_exclusivity is called
 * @then Returns OCPP_LB_NEEDS_REINIT because Smart Charging callback was never registered
 */
void test_lb_changed_1_to_0_needs_reinit(void) {
    TEST_ASSERT_EQUAL_INT(OCPP_LB_NEEDS_REINIT,
        ocpp_check_lb_exclusivity(0, true, false));
}

/* ---- Main ---- */
int main(void) {
    TEST_SUITE_BEGIN("OCPP Load Balancing Exclusivity");

    RUN_TEST(test_lb_standalone_no_conflict);
    RUN_TEST(test_lb_ocpp_disabled_no_conflict);
    RUN_TEST(test_lb_master_conflict);
    RUN_TEST(test_lb_node_conflict);
    RUN_TEST(test_lb_changed_0_to_1_conflict);
    RUN_TEST(test_lb_changed_1_to_0_needs_reinit);

    TEST_SUITE_RESULTS();
}
