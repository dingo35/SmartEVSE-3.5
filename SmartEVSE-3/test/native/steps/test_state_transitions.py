"""
Step definitions for state_transitions.feature

Maps Gherkin scenarios to the C test binary ``test_state_transitions``.
Each scenario verifies one or more C-level test functions by running
the binary and inspecting per-test [OK]/[FAIL] markers in the output.

C binary: build/test_state_transitions
Key test functions mapped:
  - test_A_to_B_on_9V_when_ready        (A -> B on vehicle plug-in)
  - test_full_charge_cycle               (A -> B -> C -> B -> A)
  - test_A_to_B1_when_errors             (error blocks normal transition)
  - test_A_to_B1_when_charge_delay       (delay blocks normal transition)
  - test_A_stays_A_when_access_off       (no access blocks transition)
  - test_init_contactors_off             (contactors off at init)
  - test_C_contactor1_on                 (contactor 1 on in State C)
  - test_C_to_B_on_9V                    (vehicle stops charging)
  - test_C_to_A_on_disconnect            (vehicle disconnects during charge)
"""

import pytest
from pytest_bdd import scenario, given, when, then

from conftest import check_specific_test

# ---------------------------------------------------------------------------
# Scenarios
# ---------------------------------------------------------------------------

@scenario(
    "../features/state_transitions.feature",
    "Vehicle connection triggers State A to B transition",
)
def test_a_to_b():
    pass


@scenario(
    "../features/state_transitions.feature",
    "Full charge cycle from connection to disconnection",
)
def test_full_cycle():
    pass


@scenario(
    "../features/state_transitions.feature",
    "Errors prevent charging and route to waiting state",
)
def test_errors_to_b1():
    pass


@scenario(
    "../features/state_transitions.feature",
    "Charge delay routes to waiting state",
)
def test_delay_to_b1():
    pass


@scenario(
    "../features/state_transitions.feature",
    "Unauthorized charger ignores vehicle connection",
)
def test_unauthorized():
    pass


# ---------------------------------------------------------------------------
# Given steps
# ---------------------------------------------------------------------------

@given("the charger is initialized")
def charger_initialized(context):
    context["initialized"] = True


@given("authorization is granted")
def auth_granted(context):
    context["authorized"] = True


@given("the charger is idle in State A")
def idle_state_a(context):
    context["state"] = "A"


@given("the modem is authenticated")
def modem_authenticated(context):
    context["modem_authenticated"] = True


@given("the charger is in standalone normal mode")
def standalone_normal(context):
    context["mode"] = "standalone_normal"


@given("the charger has a temperature error")
def charger_temp_error(context):
    context["error"] = "TEMP_HIGH"


@given("a charge delay is active")
def charge_delay_active(context):
    context["charge_delay"] = True


@given("authorization is revoked")
def auth_revoked(context):
    context["authorized"] = False


# ---------------------------------------------------------------------------
# When steps
# ---------------------------------------------------------------------------

@when("a vehicle plugs in")
def vehicle_plugs_in(context):
    context["action"] = "plug_in"


@when("the vehicle requests charging after diode check")
def vehicle_requests_charge(context):
    context["action"] = "request_charge_with_diode"


@when("the vehicle stops requesting charge")
def vehicle_stops_charge(context):
    context["action"] = "stop_charge"


@when("the vehicle disconnects")
def vehicle_disconnects(context):
    context["action"] = "disconnect"


# ---------------------------------------------------------------------------
# Then steps
# ---------------------------------------------------------------------------

@then("the charger transitions to State B")
def check_transition_to_b(context):
    if context.get("error") == "TEMP_HIGH" or context.get("charge_delay"):
        pytest.skip("Error/delay scenario uses different transition")
    if context.get("mode") == "standalone_normal":
        # Part of the full cycle test
        assert check_specific_test(
            "test_state_transitions", "test_full_charge_cycle"
        )
    else:
        assert check_specific_test(
            "test_state_transitions", "test_A_to_B_on_9V_when_ready"
        )


@then("the contactors remain off")
def contactors_off(context):
    assert check_specific_test(
        "test_state_transitions", "test_init_contactors_off"
    )


@then("the charger transitions to State C")
def check_transition_to_c(context):
    assert check_specific_test(
        "test_state_transitions", "test_B_to_C_on_6V_with_diode_check"
    )


@then("contactor 1 is energized")
def contactor1_on(context):
    assert check_specific_test(
        "test_state_transitions", "test_C_contactor1_on"
    )


@then("the charger returns to State B")
def check_return_to_b(context):
    assert check_specific_test(
        "test_state_transitions", "test_C_to_B_on_9V"
    )


@then("the charger returns to State A")
def check_return_to_a(context):
    if context.get("mode") == "standalone_normal":
        assert check_specific_test(
            "test_state_transitions", "test_full_charge_cycle"
        )
    else:
        assert check_specific_test(
            "test_state_transitions", "test_B_to_A_on_disconnect"
        )


@then("all contactors are off")
def all_contactors_off(context):
    assert check_specific_test(
        "test_state_transitions", "test_C_to_A_on_disconnect"
    )


@then("the charger transitions to State B1")
def check_transition_to_b1(context):
    if context.get("error") == "TEMP_HIGH":
        assert check_specific_test(
            "test_state_transitions", "test_A_to_B1_when_errors"
        )
    elif context.get("charge_delay"):
        assert check_specific_test(
            "test_state_transitions", "test_A_to_B1_when_charge_delay"
        )
    else:
        pytest.fail("Unexpected context for B1 transition")


@then("charging does not start")
def charging_does_not_start(context):
    # Verified by the B1 transition -- contactors never close in B1.
    assert check_specific_test(
        "test_state_transitions", "test_A_to_B1_when_errors"
    )


@then("the charger remains in State A")
def charger_stays_a(context):
    assert check_specific_test(
        "test_state_transitions", "test_A_stays_A_when_access_off"
    )
