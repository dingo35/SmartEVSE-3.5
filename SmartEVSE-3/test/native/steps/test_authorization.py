"""
Step definitions for authorization.feature

Maps Gherkin scenarios to the C test binary ``test_authorization``.
Each scenario verifies access control behavior by checking specific
C test function results.

C binary: build/test_authorization
Key test functions mapped:
  - test_set_access_off_from_C_goes_C1   (revoke during charge -> C1)
  - test_set_access_pause_from_C_goes_C1 (pause during charge -> C1)
  - test_set_access_off_from_B_goes_B1   (revoke while connected -> B1)
  - test_access_timer_expires_turns_off   (RFID lock timer -> revoke)
  - test_access_timer_counts_down_in_state_A (timer countdown)
"""

import pytest
from pytest_bdd import scenario, given, when, then

from conftest import check_specific_test

# ---------------------------------------------------------------------------
# Scenarios
# ---------------------------------------------------------------------------

@scenario(
    "../features/authorization.feature",
    "Revoking access during charging suspends the session",
)
def test_revoke_during_charge():
    pass


@scenario(
    "../features/authorization.feature",
    "Pausing access during charging suspends the session",
)
def test_pause_during_charge():
    pass


@scenario(
    "../features/authorization.feature",
    "Revoking access while connected stops charging preparation",
)
def test_revoke_while_connected():
    pass


@scenario(
    "../features/authorization.feature",
    "RFID lock timer expires and revokes access",
)
def test_rfid_timer_expires():
    pass


# ---------------------------------------------------------------------------
# Given steps
# ---------------------------------------------------------------------------

@given("the charger is initialized")
def charger_initialized(context):
    context["initialized"] = True


@given("the charger is charging in State C")
def charging_state_c(context):
    context["state"] = "C"


@given("the charger is connected in State B")
def connected_state_b(context):
    context["state"] = "B"


@given("the charger is idle with RFID access granted")
def idle_rfid_access(context):
    context["state"] = "A"
    context["rfid_access"] = True


@given("the RFID lock timer is set")
def rfid_timer_set(context):
    context["rfid_timer_active"] = True


# ---------------------------------------------------------------------------
# When steps
# ---------------------------------------------------------------------------

@when("access is set to OFF")
def access_set_off(context):
    context["access_action"] = "OFF"


@when("access is set to PAUSE")
def access_set_pause(context):
    context["access_action"] = "PAUSE"


@when("the lock timer expires")
def lock_timer_expires(context):
    context["timer_expired"] = True


# ---------------------------------------------------------------------------
# Then steps
# ---------------------------------------------------------------------------

@then("the charger transitions to State C1")
def check_transition_to_c1(context):
    if context.get("access_action") == "OFF":
        assert check_specific_test(
            "test_authorization", "test_set_access_off_from_C_goes_C1"
        )
    elif context.get("access_action") == "PAUSE":
        assert check_specific_test(
            "test_authorization", "test_set_access_pause_from_C_goes_C1"
        )


@then("charging is suspended")
def charging_suspended(context):
    # Verified by the C1 transition -- C1 means charging is suspended.
    assert check_specific_test(
        "test_authorization", "test_set_access_off_from_C_goes_C1"
    )


@then("the charger transitions to State B1")
def check_transition_to_b1(context):
    assert check_specific_test(
        "test_authorization", "test_set_access_off_from_B_goes_B1"
    )


@then("access is revoked")
def access_revoked(context):
    assert check_specific_test(
        "test_authorization", "test_access_timer_expires_turns_off"
    )
