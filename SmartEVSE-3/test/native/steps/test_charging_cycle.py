"""
Step definitions for charging_cycle.feature

End-to-end scenarios that exercise the full charging workflow by
combining tests from multiple C test binaries: state_transitions,
modem_states, and tick_10ms.

C binaries used:
  - build/test_state_transitions  (full charge cycle, contactor control)
  - build/test_modem_states       (modem negotiation flow)
  - build/test_tick_10ms          (debounce, side-effects of A->B)
"""

import pytest
from pytest_bdd import scenario, given, when, then

from conftest import check_specific_test

# ---------------------------------------------------------------------------
# Scenarios
# ---------------------------------------------------------------------------

@scenario(
    "../features/charging_cycle.feature",
    "Normal standalone charge session",
)
def test_normal_standalone():
    pass


@scenario(
    "../features/charging_cycle.feature",
    "Modem negotiation before charging",
)
def test_modem_negotiation():
    pass


# ---------------------------------------------------------------------------
# Given steps
# ---------------------------------------------------------------------------

@given("a standalone charger in normal mode")
def standalone_normal(context):
    context["mode"] = "standalone_normal"


@given("authorization is granted")
def auth_granted(context):
    context["authorized"] = True


@given("a charger requiring modem authentication")
def modem_required(context):
    context["modem_required"] = True
    context["modem_stage"] = 0


# ---------------------------------------------------------------------------
# When steps
# ---------------------------------------------------------------------------

@when("a vehicle plugs in")
def vehicle_plugs_in(context):
    context["action"] = "plug_in"


@when("the vehicle requests charging")
def vehicle_requests_charge(context):
    context["action"] = "request_charge"


@when("the diode check passes")
def diode_check_passes(context):
    context["diode_check"] = True


@when("the vehicle finishes charging")
def vehicle_finishes(context):
    context["action"] = "stop_charge"


@when("the vehicle disconnects")
def vehicle_disconnects(context):
    context["action"] = "disconnect"


@when("the modem authentication completes")
def modem_auth_completes(context):
    context["modem_authenticated"] = True


# ---------------------------------------------------------------------------
# Then steps
# ---------------------------------------------------------------------------

@then("the charger enters State B")
def check_enters_b(context):
    if context.get("modem_required"):
        # Modem flow completes to State B
        assert check_specific_test(
            "test_modem_states", "test_modem_done_to_B_after_timer"
        )
    else:
        assert check_specific_test(
            "test_state_transitions", "test_A_to_B_on_9V_when_ready"
        )


@then("the charger enters State C after debounce")
def check_enters_c_debounce(context):
    assert check_specific_test(
        "test_tick_10ms", "test_b_to_c_debounce_threshold"
    )


@then("contactor 1 is on")
def contactor1_on(context):
    assert check_specific_test(
        "test_state_transitions", "test_C_contactor1_on"
    )


@then("the charger returns to State B")
def check_returns_b(context):
    assert check_specific_test(
        "test_state_transitions", "test_C_to_B_on_9V"
    )


@then("the charger returns to State A")
def check_returns_a(context):
    assert check_specific_test(
        "test_state_transitions", "test_B_to_A_on_disconnect"
    )


@then("all contactors are off")
def all_contactors_off(context):
    assert check_specific_test(
        "test_state_transitions", "test_C_to_A_on_disconnect"
    )


@then("modem negotiation begins")
def modem_negotiation_begins(context):
    assert check_specific_test(
        "test_modem_states", "test_modem_request_disconnects_pilot"
    )


@then("the modem stage is set to authenticated")
def modem_stage_authenticated(context):
    # The full modem flow test verifies ModemStage=1 at the end.
    assert check_specific_test(
        "test_modem_states", "test_full_modem_flow"
    )
