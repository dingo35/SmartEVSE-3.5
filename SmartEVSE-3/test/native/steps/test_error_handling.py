"""
Step definitions for error_handling.feature

Maps Gherkin scenarios to the C test binary ``test_error_handling``.
Each scenario verifies safety mechanisms including temperature
protection with hysteresis and meter communication timeouts.

C binary: build/test_error_handling
Key test functions mapped:
  - test_temp_high_triggers_error        (temperature exceeds max)
  - test_temp_high_shuts_down_charging   (overtemp suspends charge)
  - test_temp_recovery_boundary          (hysteresis at maxTemp - 10)
  - test_mains_meter_timeout_sets_ct_nocomm  (meter timeout error)
  - test_ct_nocomm_recovers_on_communication (meter recovery)
"""

import pytest
from pytest_bdd import scenario, given, when, then

from conftest import check_specific_test

# ---------------------------------------------------------------------------
# Scenarios
# ---------------------------------------------------------------------------

@scenario(
    "../features/error_handling.feature",
    "Temperature exceeding maximum triggers shutdown",
)
def test_temp_shutdown():
    pass


@scenario(
    "../features/error_handling.feature",
    "Temperature recovery with hysteresis",
)
def test_temp_hysteresis():
    pass


@scenario(
    "../features/error_handling.feature",
    "Mains meter communication timeout",
)
def test_meter_timeout():
    pass


# ---------------------------------------------------------------------------
# Given steps
# ---------------------------------------------------------------------------

@given("the charger is initialized")
def charger_initialized(context):
    context["initialized"] = True


@given("the charger is charging")
def charger_charging(context):
    context["state"] = "C"


@given("the EVSE temperature exceeds the maximum")
def temp_exceeds_max(context):
    context["temp_exceeded"] = True


@given("a temperature error is active")
def temp_error_active(context):
    context["temp_error"] = True


@given("the maximum temperature is 65 degrees")
def max_temp_65(context):
    context["max_temp"] = 65


@given("the charger is in Smart mode with a mains meter")
def smart_mode_mains_meter(context):
    context["mode"] = "smart"
    context["mains_meter"] = True


# ---------------------------------------------------------------------------
# When steps
# ---------------------------------------------------------------------------

@when("the 1-second tick fires")
def tick_1s_fires(context):
    context["tick"] = "1s"


@when("the temperature drops to 55 degrees")
def temp_drops_55(context):
    context["temp"] = 55


@when("the temperature drops to 54 degrees")
def temp_drops_54(context):
    context["temp"] = 54


@when("the mains meter stops communicating")
def meter_stops(context):
    context["meter_comm"] = "lost"


@when("communication is restored")
def meter_restored(context):
    context["meter_comm"] = "restored"


# ---------------------------------------------------------------------------
# Then steps
# ---------------------------------------------------------------------------

@then("a temperature error flag is set")
def check_temp_error_set(context):
    assert check_specific_test(
        "test_error_handling", "test_temp_high_triggers_error"
    )


@then("charging is suspended")
def charging_suspended(context):
    assert check_specific_test(
        "test_error_handling", "test_temp_high_shuts_down_charging"
    )


@then("the temperature error remains active")
def temp_error_remains(context):
    # At 55 degrees (maxTemp=65, threshold=55), error should persist.
    assert check_specific_test(
        "test_error_handling", "test_temp_recovery_boundary"
    )


@then("the temperature error is cleared")
def temp_error_cleared(context):
    # At 54 degrees (below maxTemp - 10 = 55), error should clear.
    assert check_specific_test(
        "test_error_handling", "test_temp_recovery_boundary"
    )


@then("a CT_NOCOMM error is raised")
def ct_nocomm_raised(context):
    assert check_specific_test(
        "test_error_handling", "test_mains_meter_timeout_sets_ct_nocomm"
    )


@then("the CT_NOCOMM error is cleared")
def ct_nocomm_cleared(context):
    assert check_specific_test(
        "test_error_handling", "test_ct_nocomm_recovers_on_communication"
    )
