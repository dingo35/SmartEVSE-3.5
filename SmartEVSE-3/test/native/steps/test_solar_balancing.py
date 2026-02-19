"""
Step definitions for solar_balancing.feature

Maps Gherkin scenarios to C test binaries ``test_solar_balancing``
and ``test_phase_switching``. Each scenario verifies solar-specific
behavior including automatic phase switching and startup current
forcing.

C binaries:
  - build/test_solar_balancing
  - build/test_phase_switching
Key test functions mapped:
  - test_solar_3p_timer_triggers_1p_switch  (3P shortage -> 1P)
  - test_solar_1p_timer_triggers_3p_switch  (1P surplus -> 3P)
  - test_solar_insufficient_surplus_resets_timer (timer reset)
  - test_solar_startup_forces_mincurrent    (startup MinCurrent)
"""

import pytest
from pytest_bdd import scenario, given, when, then

from conftest import check_specific_test

# ---------------------------------------------------------------------------
# Scenarios
# ---------------------------------------------------------------------------

@scenario(
    "../features/solar_balancing.feature",
    "Shortage in 3-phase mode triggers switch to 1-phase",
)
def test_3p_to_1p():
    pass


@scenario(
    "../features/solar_balancing.feature",
    "Surplus in 1-phase mode triggers switch to 3-phase",
)
def test_1p_to_3p():
    pass


@scenario(
    "../features/solar_balancing.feature",
    "Insufficient surplus resets the solar stop timer",
)
def test_timer_reset():
    pass


@scenario(
    "../features/solar_balancing.feature",
    "Solar startup forces minimum current",
)
def test_startup_mincurrent():
    pass


# ---------------------------------------------------------------------------
# Given steps
# ---------------------------------------------------------------------------

@given("the charger is in solar mode")
def solar_mode(context):
    context["mode"] = "solar"


@given("the charger is charging")
def charger_charging(context):
    context["state"] = "C"


@given("the charger is in 3-phase mode with AUTO phase switching")
def three_phase_auto(context):
    context["phases"] = 3
    context["phase_switching"] = "AUTO"


@given("there is a current shortage")
def current_shortage(context):
    context["shortage"] = True


@given("the charger is in 1-phase mode at maximum current")
def one_phase_max(context):
    context["phases"] = 1
    context["at_max"] = True


@given("there is sufficient solar surplus")
def solar_surplus(context):
    context["surplus"] = True


@given("the charger is in 1-phase mode below maximum current")
def one_phase_below_max(context):
    context["phases"] = 1
    context["at_max"] = False


@given("the solar stop timer is running")
def solar_timer_running(context):
    context["solar_timer_active"] = True


@given("a new charging session has just started")
def new_session(context):
    context["startup"] = True


# ---------------------------------------------------------------------------
# When steps
# ---------------------------------------------------------------------------

@when("the solar stop timer expires")
def solar_timer_expires(context):
    context["timer_expired"] = True


@when("the surplus is insufficient for 3-phase upgrade")
def surplus_insufficient(context):
    context["surplus_insufficient"] = True


@when("current is calculated during the startup period")
def calc_during_startup(context):
    context["action"] = "calc_startup"


# ---------------------------------------------------------------------------
# Then steps
# ---------------------------------------------------------------------------

@then("the charger switches to 1-phase mode")
def check_switch_to_1p(context):
    assert check_specific_test(
        "test_solar_balancing", "test_solar_3p_timer_triggers_1p_switch"
    )


@then("the charger switches to 3-phase mode")
def check_switch_to_3p(context):
    assert check_specific_test(
        "test_solar_balancing", "test_solar_1p_timer_triggers_3p_switch"
    )


@then("the solar stop timer is reset to zero")
def check_timer_reset(context):
    assert check_specific_test(
        "test_solar_balancing", "test_solar_insufficient_surplus_resets_timer"
    )


@then("the EVSE receives exactly the minimum current")
def check_mincurrent(context):
    assert check_specific_test(
        "test_solar_balancing", "test_solar_startup_forces_mincurrent"
    )
