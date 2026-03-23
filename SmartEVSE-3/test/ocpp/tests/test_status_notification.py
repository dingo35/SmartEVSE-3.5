"""
OCPP 1.6J StatusNotification Tests.

REQ-OCPP-140: StatusNotification sent on state change
REQ-OCPP-141: StatusNotification Available when idle
REQ-OCPP-142: StatusNotification Charging during active session
REQ-OCPP-143: StatusNotification Faulted on error

@feature OCPP Compatibility
"""

import pytest

from message_replay import (
    ChargePointSimulator,
    CALL_RESULT,
    VALID_CP_STATUSES,
    VALID_ERROR_CODES,
)


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_status_notification_valid_status(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-140
    @scenario Connector status changes trigger notification
    @given the simulator is booted and accepted
    @when the connector status changes
    @then a StatusNotification is sent with the new status
    @and the status is a valid OCPP 1.6 ChargePointStatus enum value
    """
    cp, csms = booted_charge_point

    # Send various status transitions
    for status in ["Available", "Preparing", "Charging", "Finishing", "Available"]:
        response = await cp.send_status_notification(status=status)
        assert response.msg_type == CALL_RESULT

    # Verify all statuses were valid
    status_msgs = [d for a, d in csms.received_messages if a == "StatusNotification"]
    for msg in status_msgs:
        assert msg["status"] in VALID_CP_STATUSES, (
            f"Invalid ChargePointStatus: {msg['status']}"
        )
        assert msg["error_code"] in VALID_ERROR_CODES, (
            f"Invalid ChargePointErrorCode: {msg['error_code']}"
        )
        assert msg["connector_id"] >= 0


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_status_notification_available_when_idle(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-141
    @scenario Idle charge point reports Available
    @given no vehicle is connected
    @when the simulator reports status
    @then StatusNotification contains status "Available"
    """
    cp, csms = booted_charge_point

    response = await cp.send_status_notification(
        connector_id=1,
        status="Available",
        error_code="NoError",
    )
    assert response.msg_type == CALL_RESULT

    status_msg = await csms.wait_for_message("StatusNotification", timeout=5)
    assert status_msg["status"] == "Available"
    assert status_msg["error_code"] == "NoError"
    assert status_msg["connector_id"] == 1


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_status_notification_charging(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-142
    @scenario Active charge session reports Charging
    @given a transaction is active and current is flowing
    @when the simulator reports status
    @then StatusNotification contains status "Charging"
    """
    cp, csms = booted_charge_point

    # Start a transaction
    await cp.send_authorize("AB12CD34")
    await cp.send_start_transaction(id_tag="AB12CD34", meter_start=0)

    # Report Charging status
    response = await cp.send_status_notification(status="Charging")
    assert response.msg_type == CALL_RESULT

    # Find the Charging status notification
    status_msgs = [d for a, d in csms.received_messages if a == "StatusNotification"]
    charging_msgs = [m for m in status_msgs if m["status"] == "Charging"]
    assert len(charging_msgs) > 0, "Expected at least one Charging StatusNotification"


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_status_notification_faulted(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-143
    @scenario Error condition triggers Faulted status
    @given the simulator detects an error condition
    @when the simulator reports status
    @then StatusNotification contains status "Faulted"
    @and errorCode is not "NoError"
    """
    cp, csms = booted_charge_point

    response = await cp.send_status_notification(
        status="Faulted",
        error_code="GroundFailure",
    )
    assert response.msg_type == CALL_RESULT

    status_msg = [d for a, d in csms.received_messages if a == "StatusNotification"]
    faulted = [m for m in status_msg if m["status"] == "Faulted"]
    assert len(faulted) > 0
    assert faulted[0]["error_code"] != "NoError"
    assert faulted[0]["error_code"] in VALID_ERROR_CODES


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_status_notification_full_lifecycle(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-140
    @scenario Complete status lifecycle during a charge session
    @given a booted charge point
    @when a full charge session occurs
    @then status transitions follow Available -> Preparing -> Charging -> Finishing -> Available
    """
    cp, csms = booted_charge_point

    expected_statuses = ["Available", "Preparing", "Charging", "Finishing", "Available"]
    for status in expected_statuses:
        await cp.send_status_notification(status=status)

    status_msgs = [d for a, d in csms.received_messages if a == "StatusNotification"]
    actual_statuses = [m["status"] for m in status_msgs]

    assert actual_statuses == expected_statuses, (
        f"Status transitions: expected {expected_statuses}, got {actual_statuses}"
    )
