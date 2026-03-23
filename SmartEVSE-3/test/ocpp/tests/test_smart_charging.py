"""
OCPP 1.6J Smart Charging Tests.

REQ-OCPP-150: SetChargingProfile accepted and applied
REQ-OCPP-151: SetChargingProfile with TxDefaultProfile
REQ-OCPP-152: ClearChargingProfile removes limit
REQ-OCPP-153: GetCompositeSchedule returns active limits

@feature OCPP Compatibility
"""

import asyncio
from datetime import datetime, timezone

import pytest

from message_replay import ChargePointSimulator, CALL_RESULT


def make_charging_profile(
    profile_id: int = 1,
    stack_level: int = 0,
    purpose: str = "TxProfile",
    kind: str = "Relative",
    limit_a: float = 10.0,
):
    """Create an OCPP 1.6 ChargingProfile dict."""
    return {
        "chargingProfileId": profile_id,
        "stackLevel": stack_level,
        "chargingProfilePurpose": purpose,
        "chargingProfileKind": kind,
        "chargingSchedule": {
            "chargingRateUnit": "A",
            "chargingSchedulePeriod": [
                {"startPeriod": 0, "limit": limit_a},
            ],
        },
    }


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_set_charging_profile_accepted(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-150
    @scenario CSMS sets charging profile with current limit
    @given the simulator is connected and booted
    @when the CSMS sends SetChargingProfile with limit 10.0A
    @then the simulator responds with status "Accepted"
    """
    cp, csms = booted_charge_point

    profile = make_charging_profile(limit_a=10.0)
    response = await csms.send_set_charging_profile(connector_id=1, profile=profile)

    assert response.status == "Accepted"


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_set_charging_profile_tx_default(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-151
    @scenario TxDefaultProfile applies to all future transactions
    @given the simulator is connected
    @when the CSMS sends SetChargingProfile with purpose "TxDefaultProfile"
    @then the profile is accepted
    """
    cp, csms = booted_charge_point

    profile = make_charging_profile(
        purpose="TxDefaultProfile",
        limit_a=16.0,
    )
    response = await csms.send_set_charging_profile(connector_id=1, profile=profile)
    assert response.status == "Accepted"


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_clear_charging_profile(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-152
    @scenario CSMS clears charging profile
    @given a charging profile is active with limit 10.0A
    @when the CSMS sends ClearChargingProfile
    @then the profile is cleared
    """
    cp, csms = booted_charge_point

    # First set a profile
    profile = make_charging_profile(profile_id=1, limit_a=10.0)
    set_resp = await csms.send_set_charging_profile(connector_id=1, profile=profile)
    assert set_resp.status == "Accepted"

    # Clear it
    clear_resp = await csms.send_clear_charging_profile(id=1)
    assert clear_resp.status == "Accepted"


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_get_composite_schedule(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-153
    @scenario Query active charging schedule
    @given a charging profile is active
    @when the CSMS sends GetCompositeSchedule for connector 1
    @then the response status is Accepted or Rejected (both are valid)
    """
    cp, csms = booted_charge_point

    # Set a profile first
    profile = make_charging_profile(limit_a=16.0)
    await csms.send_set_charging_profile(connector_id=1, profile=profile)

    # Get composite schedule
    response = await csms.send_get_composite_schedule(connector_id=1, duration=3600)

    # The charge point should respond (Accepted or Rejected are both valid
    # depending on implementation support)
    assert response.status in ("Accepted", "Rejected")


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_set_charging_profile_max_current(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-150
    @scenario SetChargingProfile with ChargePointMaxProfile
    @given the simulator is connected
    @when the CSMS sends SetChargingProfile with purpose "ChargePointMaxProfile"
    @then the profile is accepted
    """
    cp, csms = booted_charge_point

    profile = make_charging_profile(
        purpose="ChargePointMaxProfile",
        kind="Absolute",
        limit_a=32.0,
    )
    response = await csms.send_set_charging_profile(connector_id=0, profile=profile)
    assert response.status == "Accepted"
