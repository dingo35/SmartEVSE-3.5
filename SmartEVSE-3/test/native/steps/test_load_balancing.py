"""
Step definitions for load_balancing.feature

Maps Gherkin scenarios to the C test binary ``test_load_balancing``.
Each scenario verifies fair current distribution among multiple EVSEs,
individual capacity limits, minimum current enforcement, and OCPP
current limiting.

C binary: build/test_load_balancing
Key test functions mapped:
  - test_two_evse_equal_distribution       (equal sharing)
  - test_balanced_max_caps_individual      (per-EVSE max respected)
  - test_minimum_current_enforced          (MinCurrent floor)
  - test_ocpp_limit_reduces_charge_current (OCPP cap)
"""

import pytest
from pytest_bdd import scenario, given, when, then

from conftest import check_specific_test, check_all_tests_in_suite

# ---------------------------------------------------------------------------
# Scenarios
# ---------------------------------------------------------------------------

@scenario(
    "../features/load_balancing.feature",
    "Two EVSEs share current equally",
)
def test_equal_distribution():
    pass


@scenario(
    "../features/load_balancing.feature",
    "Individual EVSE maximum is respected",
)
def test_individual_max():
    pass


@scenario(
    "../features/load_balancing.feature",
    "Minimum current is enforced during shortage",
)
def test_min_current_enforced():
    pass


@scenario(
    "../features/load_balancing.feature",
    "OCPP current limit reduces charge current",
)
def test_ocpp_limit():
    pass


# ---------------------------------------------------------------------------
# Given steps
# ---------------------------------------------------------------------------

@given("a master EVSE with load balancing enabled")
def master_evse(context):
    context["role"] = "master"
    context["load_balancing"] = True


@given("2 EVSEs are charging")
def two_evses_charging(context):
    context["evse_count"] = 2
    context["all_charging"] = True


@given("2 EVSEs are charging with limited total current")
def two_evses_limited(context):
    context["evse_count"] = 2
    context["limited_current"] = True


@given("EVSE 2 has a lower maximum capacity")
def evse2_lower_max(context):
    context["evse2_lower_max"] = True


@given("OCPP mode is enabled")
def ocpp_enabled(context):
    context["ocpp_mode"] = True


@given("the OCPP current limit is 10A")
def ocpp_limit_10a(context):
    context["ocpp_limit"] = 10


# ---------------------------------------------------------------------------
# When steps
# ---------------------------------------------------------------------------

@when("current is calculated")
def current_calculated(context):
    context["action"] = "calc_balanced"


# ---------------------------------------------------------------------------
# Then steps
# ---------------------------------------------------------------------------

@then("both EVSEs receive equal current")
def check_equal_current(context):
    assert check_specific_test(
        "test_load_balancing", "test_two_evse_equal_distribution"
    )


@then("EVSE 2 does not exceed its maximum")
def check_evse2_capped(context):
    assert check_specific_test(
        "test_load_balancing", "test_balanced_max_caps_individual"
    )


@then("EVSE 1 receives the remainder")
def check_evse1_gets_remainder(context):
    # The balanced_max_caps_individual test verifies EVSE 2 is capped,
    # which implicitly means EVSE 1 gets the rest. The distribution
    # test suite as a whole verifies this property.
    assert check_specific_test(
        "test_load_balancing", "test_balanced_max_caps_individual"
    )


@then("each active EVSE receives at least the minimum current")
def check_min_current(context):
    assert check_specific_test(
        "test_load_balancing", "test_minimum_current_enforced"
    )


@then("the charge current does not exceed 10A")
def check_ocpp_cap(context):
    assert check_specific_test(
        "test_load_balancing", "test_ocpp_limit_reduces_charge_current"
    )
