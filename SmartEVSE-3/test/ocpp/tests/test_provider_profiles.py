"""
OCPP 1.6J Provider-Specific Profile Tests.

REQ-OCPP-180: Tap Electric full session flow
REQ-OCPP-181: Tibber FreeVend auto-start flow
REQ-OCPP-182: SteVe local auth flow
REQ-OCPP-183: CSMS with mandatory Heartbeat enforcement

@feature OCPP Compatibility
"""

import asyncio
import json
import os

import pytest

from mock_csms import MockCSMS
from message_replay import ChargePointSimulator, CALL_RESULT

# Path to provider profile JSON files
PROFILE_DIR = os.path.join(os.path.dirname(os.path.dirname(__file__)), "provider_profiles")


def load_profile(name: str) -> dict:
    """Load a provider profile configuration."""
    path = os.path.join(PROFILE_DIR, f"{name}.json")
    with open(path) as f:
        return json.load(f)


@pytest.mark.asyncio
@pytest.mark.timeout(30)
async def test_tap_electric_full_session(mock_csms):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-180
    @scenario Complete charge session with Tap Electric-like CSMS
    @given the mock CSMS behaves like Tap Electric (requires RFID auth, sends SetChargingProfile)
    @when a full session is executed: boot -> auth -> start -> meter -> stop
    @then all messages are accepted by the CSMS
    @and the session completes without errors
    """
    profile = load_profile("tap_electric")

    # Configure CSMS to behave like Tap Electric
    mock_csms.response_overrides["BootNotification"] = profile["boot_response"]
    mock_csms.response_overrides["Authorize"] = profile["authorize_response"]

    cp = ChargePointSimulator(mock_csms.url, charge_point_id="TAP-ELEC-001")
    await cp.connect()
    await mock_csms.wait_for_connection(timeout=5.0)

    try:
        results = await cp.run_full_session(
            id_tag="TAPCARD01",
            energy_wh=8000,
            meter_intervals=3,
            phases=3,
        )

        # Verify full session completed
        assert results["boot"].msg_type == CALL_RESULT
        assert results["boot"].payload["status"] == "Accepted"
        assert results["authorize"].msg_type == CALL_RESULT
        assert results["authorize"].payload["idTagInfo"]["status"] == "Accepted"
        assert results["start_transaction"].msg_type == CALL_RESULT
        assert results["stop_transaction"].msg_type == CALL_RESULT

        # Verify CSMS received all expected messages
        msg_types = [a for a, _ in mock_csms.received_messages]
        assert "BootNotification" in msg_types
        assert "Authorize" in msg_types
        assert "StartTransaction" in msg_types
        assert "MeterValues" in msg_types
        assert "StopTransaction" in msg_types

        # If Tap Electric sends a charging profile after StartTransaction,
        # verify the simulator can receive it
        if profile["features"]["sends_charging_profile"]:
            charging_profile = profile.get("charging_profile")
            if charging_profile:
                resp = await mock_csms.send_set_charging_profile(
                    connector_id=1,
                    profile=charging_profile,
                )
                assert resp.status == "Accepted"

    finally:
        await cp.disconnect()


@pytest.mark.asyncio
@pytest.mark.timeout(30)
async def test_tibber_freevend_autostart(mock_csms):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-181
    @scenario Auto-start session with Tibber-like CSMS
    @given the mock CSMS behaves like Tibber (FreeVend enabled, no RFID required)
    @when a vehicle is connected
    @then a transaction starts (auto-authorized)
    @and MeterValues are sent at the configured interval
    """
    profile = load_profile("tibber")

    mock_csms.response_overrides["BootNotification"] = profile["boot_response"]
    mock_csms.response_overrides["Authorize"] = profile["authorize_response"]

    cp = ChargePointSimulator(mock_csms.url, charge_point_id="TIBBER-001")
    await cp.connect()
    await mock_csms.wait_for_connection(timeout=5.0)

    try:
        # Boot
        boot_resp = await cp.send_boot_notification()
        assert boot_resp.msg_type == CALL_RESULT
        assert boot_resp.payload["status"] == "Accepted"

        # FreeVend flow: no explicit RFID scan needed
        # The charge point auto-starts when vehicle connects
        await cp.send_status_notification(status="Available")
        await cp.send_status_notification(status="Preparing")

        # Auto-authorize with empty/default tag (FreeVend)
        auth_resp = await cp.send_authorize("FREEVEND")
        assert auth_resp.payload["idTagInfo"]["status"] == "Accepted"

        # Start transaction
        start_resp = await cp.send_start_transaction(
            id_tag="FREEVEND",
            meter_start=0,
        )
        assert start_resp.msg_type == CALL_RESULT

        await cp.send_status_notification(status="Charging")

        # Send meter values (simulating configured interval)
        for i in range(3):
            await cp.send_meter_values(energy_wh=(i + 1) * 2000, phases=1)

        # Stop
        await cp.send_status_notification(status="Finishing")
        tx_id = start_resp.payload["transactionId"]
        stop_resp = await cp.send_stop_transaction(
            meter_stop=6000,
            reason="EVDisconnected",
            transaction_id=tx_id,
        )
        assert stop_resp.msg_type == CALL_RESULT

        await cp.send_status_notification(status="Available")

        # Verify MeterValues were sent
        meter_msgs = [a for a, _ in mock_csms.received_messages if a == "MeterValues"]
        assert len(meter_msgs) >= 3

    finally:
        await cp.disconnect()


@pytest.mark.asyncio
@pytest.mark.timeout(30)
async def test_steve_local_auth(mock_csms):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-182
    @scenario Session with SteVe-like local CSMS
    @given the mock CSMS behaves like SteVe (accepts any idTag, no Smart Charging)
    @when a session is started with a dummy idTag
    @then the transaction lifecycle completes normally
    """
    profile = load_profile("steve")

    mock_csms.response_overrides["BootNotification"] = profile["boot_response"]
    mock_csms.response_overrides["Authorize"] = profile["authorize_response"]

    cp = ChargePointSimulator(mock_csms.url, charge_point_id="STEVE-001")
    await cp.connect()
    await mock_csms.wait_for_connection(timeout=5.0)

    try:
        results = await cp.run_full_session(
            id_tag="DUMMYTAG01",
            energy_wh=3000,
            meter_intervals=2,
            phases=1,
        )

        assert results["boot"].payload["status"] == "Accepted"
        assert results["authorize"].payload["idTagInfo"]["status"] == "Accepted"
        assert results["start_transaction"].msg_type == CALL_RESULT
        assert results["stop_transaction"].msg_type == CALL_RESULT

        # SteVe doesn't send Smart Charging profiles - verify no
        # SetChargingProfile was received by the charge point
        profile_calls = [a for a, _ in cp.received_calls if a == "SetChargingProfile"]
        assert len(profile_calls) == 0

    finally:
        await cp.disconnect()


@pytest.mark.asyncio
@pytest.mark.timeout(30)
async def test_heartbeat_enforcement(mock_csms):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-183
    @scenario CSMS expects regular heartbeats
    @given the mock CSMS expects Heartbeat every 300 seconds
    @when the charge point sends Heartbeat
    @then the CSMS accepts the Heartbeat with a valid timestamp
    """
    mock_csms.response_overrides["BootNotification"] = {
        "status": "Accepted",
        "interval": 300,
    }

    cp = ChargePointSimulator(mock_csms.url, charge_point_id="HEARTBEAT-CP")
    await cp.connect()
    await mock_csms.wait_for_connection(timeout=5.0)

    try:
        # Boot with heartbeat interval
        boot_resp = await cp.send_boot_notification()
        assert boot_resp.payload["interval"] == 300

        # Send heartbeats
        from datetime import datetime

        for _ in range(3):
            hb_resp = await cp.send_heartbeat()
            assert hb_resp.msg_type == CALL_RESULT
            assert "currentTime" in hb_resp.payload
            # Verify timestamp is valid
            dt = datetime.fromisoformat(hb_resp.payload["currentTime"].replace("Z", "+00:00"))
            assert dt is not None

        # Verify CSMS received all heartbeats
        hb_msgs = [a for a, _ in mock_csms.received_messages if a == "Heartbeat"]
        assert len(hb_msgs) >= 3

    finally:
        await cp.disconnect()
