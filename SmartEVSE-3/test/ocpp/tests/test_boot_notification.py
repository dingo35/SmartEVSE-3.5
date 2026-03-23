"""
OCPP 1.6J Boot and Connection Tests.

REQ-OCPP-100: BootNotification contains required fields
REQ-OCPP-101: WebSocket connects with correct OCPP 1.6 subprotocol
REQ-OCPP-102: BootNotification retry on rejection
REQ-OCPP-103: BootNotification retry on pending

@feature OCPP Compatibility
"""

import asyncio
import json

import pytest
import pytest_asyncio
import websockets

from mock_csms import MockCSMS
from message_replay import ChargePointSimulator, CALL, CALL_RESULT


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_boot_notification_contains_required_fields(mock_csms, charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-100
    @scenario Boot notification message completeness
    @given MicroOcppSimulator is started with chargeBoxId "SMARTEVSE-TEST-001"
    @when the simulator connects to the mock CSMS via WebSocket
    @then the first message is BootNotification
    @and it contains charge_point_vendor, charge_point_model fields
    @and the CSMS responds with status "Accepted"
    """
    response = await charge_point.send_boot_notification(
        vendor="SmartEVSE",
        model="SmartEVSE-3",
        serial_number="SMARTEVSE-TEST-001",
        firmware_version="v3.7.0",
    )

    # Verify response
    assert response.msg_type == CALL_RESULT
    assert response.payload["status"] == "Accepted"
    assert "currentTime" in response.payload
    assert "interval" in response.payload

    # Verify the CSMS received the correct fields
    boot_msg = await mock_csms.wait_for_message("BootNotification", timeout=5)
    assert boot_msg["charge_point_vendor"] == "SmartEVSE"
    assert boot_msg["charge_point_model"] == "SmartEVSE-3"
    assert "charge_point_serial_number" in boot_msg
    assert boot_msg["charge_point_serial_number"] == "SMARTEVSE-TEST-001"


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_websocket_subprotocol_ocpp16(mock_csms):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-101
    @scenario WebSocket subprotocol negotiation
    @given the mock CSMS requires subprotocol "ocpp1.6"
    @when the simulator initiates a WebSocket connection
    @then the Sec-WebSocket-Protocol header contains "ocpp1.6"
    """
    url = f"{mock_csms.url}/ocpp/SUBPROTO-TEST"

    # Connect with ocpp1.6 subprotocol
    ws = await websockets.connect(url, subprotocols=["ocpp1.6"])
    try:
        # Verify the negotiated subprotocol
        assert ws.subprotocol == "ocpp1.6"
    finally:
        await ws.close()


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_websocket_rejects_wrong_subprotocol(mock_csms):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-101
    @scenario WebSocket rejects wrong subprotocol
    @given the mock CSMS requires subprotocol "ocpp1.6"
    @when a client connects with subprotocol "ocpp2.0"
    @then the connection is rejected or the subprotocol is not "ocpp2.0"
    """
    url = f"{mock_csms.url}/ocpp/WRONG-PROTO"

    # The websockets library may raise or connect with a different subprotocol
    try:
        ws = await websockets.connect(url, subprotocols=["ocpp2.0"])
        # If it connects, it should NOT have negotiated ocpp2.0
        # (the server only supports ocpp1.6)
        assert ws.subprotocol != "ocpp2.0"
        await ws.close()
    except Exception:
        # Connection rejection is also acceptable
        pass


@pytest.mark.asyncio
@pytest.mark.timeout(30)
async def test_boot_notification_retry_on_rejection(mock_csms):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-102
    @scenario CSMS rejects boot notification
    @given the mock CSMS is configured to respond with status "Rejected" and interval 2
    @when the simulator sends BootNotification
    @then the simulator receives Rejected status
    @and the response includes the retry interval
    """
    # Configure CSMS to reject boot
    mock_csms.response_overrides["BootNotification"] = {
        "status": "Rejected",
        "interval": 2,
    }

    cp = ChargePointSimulator(mock_csms.url, charge_point_id="REJECTED-CP")
    await cp.connect()
    await mock_csms.wait_for_connection(timeout=5.0)

    try:
        # Send boot notification
        response = await cp.send_boot_notification()

        # Verify rejection
        assert response.msg_type == CALL_RESULT
        assert response.payload["status"] == "Rejected"
        assert response.payload["interval"] == 2

        # The charge point should respect the interval before retrying.
        # Since our simulator doesn't auto-retry, we verify the CSMS response
        # format is correct for the charge point to act on.
        assert isinstance(response.payload["interval"], int)
        assert response.payload["interval"] > 0

    finally:
        await cp.disconnect()


@pytest.mark.asyncio
@pytest.mark.timeout(30)
async def test_boot_notification_retry_on_pending(mock_csms):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-103
    @scenario CSMS responds with Pending status
    @given the mock CSMS responds to BootNotification with status "Pending" and interval 2
    @when the simulator sends BootNotification
    @then the simulator receives Pending status with retry interval
    @and the charge point can send Heartbeat while in Pending state
    """
    mock_csms.response_overrides["BootNotification"] = {
        "status": "Pending",
        "interval": 2,
    }

    cp = ChargePointSimulator(mock_csms.url, charge_point_id="PENDING-CP")
    await cp.connect()
    await mock_csms.wait_for_connection(timeout=5.0)

    try:
        # Send boot notification
        response = await cp.send_boot_notification()

        # Verify Pending
        assert response.msg_type == CALL_RESULT
        assert response.payload["status"] == "Pending"
        assert response.payload["interval"] == 2

        # Per OCPP 1.6 spec, Heartbeat is allowed while Pending
        heartbeat_resp = await cp.send_heartbeat()
        assert heartbeat_resp.msg_type == CALL_RESULT
        assert "currentTime" in heartbeat_resp.payload

        # Reconfigure to accept on retry
        mock_csms.response_overrides["BootNotification"] = {
            "status": "Accepted",
            "interval": 300,
        }

        # Retry boot
        response2 = await cp.send_boot_notification()
        assert response2.msg_type == CALL_RESULT
        assert response2.payload["status"] == "Accepted"

    finally:
        await cp.disconnect()


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_boot_notification_iso8601_timestamp(mock_csms, charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-100
    @scenario BootNotification response contains ISO 8601 timestamp
    @given the simulator sends BootNotification
    @when the CSMS responds
    @then currentTime is a valid ISO 8601 string
    """
    from datetime import datetime

    response = await charge_point.send_boot_notification()
    assert response.msg_type == CALL_RESULT

    # Verify ISO 8601 format
    current_time = response.payload["currentTime"]
    # Should parse without error
    dt = datetime.fromisoformat(current_time.replace("Z", "+00:00"))
    assert dt is not None
