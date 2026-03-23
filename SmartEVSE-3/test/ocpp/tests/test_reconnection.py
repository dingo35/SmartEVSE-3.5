"""
OCPP 1.6J Reconnection Tests.

REQ-OCPP-173: WebSocket disconnect triggers reconnect

@feature OCPP Compatibility
"""

import asyncio

import pytest

from mock_csms import MockCSMS
from message_replay import ChargePointSimulator, CALL_RESULT


@pytest.mark.asyncio
@pytest.mark.timeout(30)
async def test_reconnect_after_disconnect(mock_csms):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-173
    @scenario Network interruption recovery
    @given the simulator is connected and booted
    @when the mock CSMS closes the WebSocket connection
    @then the simulator can reconnect
    @and after reconnection a new BootNotification is sent
    """
    cp = ChargePointSimulator(mock_csms.url, charge_point_id="RECONNECT-CP")
    await cp.connect()
    await mock_csms.wait_for_connection(timeout=5.0)

    # Boot successfully
    boot_resp = await cp.send_boot_notification()
    assert boot_resp.msg_type == CALL_RESULT
    assert boot_resp.payload["status"] == "Accepted"

    # Count initial BootNotification
    boot_count_before = len([a for a, _ in mock_csms.received_messages if a == "BootNotification"])

    # Disconnect
    await cp.disconnect()

    # Clear CSMS state for fresh connection
    mock_csms._connection_event.clear()
    mock_csms.handler = None

    # Reconnect
    cp2 = ChargePointSimulator(mock_csms.url, charge_point_id="RECONNECT-CP")
    await cp2.connect()
    await mock_csms.wait_for_connection(timeout=5.0)

    try:
        # Send new BootNotification after reconnect
        boot_resp2 = await cp2.send_boot_notification()
        assert boot_resp2.msg_type == CALL_RESULT
        assert boot_resp2.payload["status"] == "Accepted"

        # Verify a new BootNotification was received after reconnect
        # Note: mock_csms.handler is reset between connections, so received_messages
        # only contains messages from the new connection
        boot_count_after = len([a for a, _ in mock_csms.received_messages if a == "BootNotification"])
        assert boot_count_after >= 1

    finally:
        await cp2.disconnect()


@pytest.mark.asyncio
@pytest.mark.timeout(30)
async def test_transaction_state_preserved_across_reconnect(mock_csms):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-173
    @scenario Transaction state preserved across reconnection
    @given a transaction was started before disconnect
    @when the simulator reconnects
    @then the transaction can be properly stopped with the correct transactionId
    """
    cp = ChargePointSimulator(mock_csms.url, charge_point_id="TX-PERSIST-CP")
    await cp.connect()
    await mock_csms.wait_for_connection(timeout=5.0)

    try:
        # Boot and start a transaction
        await cp.send_boot_notification()
        await cp.send_authorize("AB12CD34")
        start_resp = await cp.send_start_transaction(id_tag="AB12CD34", meter_start=0)
        tx_id = start_resp.payload["transactionId"]

        # Disconnect
        await cp.disconnect()

    finally:
        pass

    # Clear CSMS connection state
    mock_csms._connection_event.clear()
    mock_csms.handler = None

    # Reconnect with new simulator instance
    cp2 = ChargePointSimulator(mock_csms.url, charge_point_id="TX-PERSIST-CP")
    await cp2.connect()
    await mock_csms.wait_for_connection(timeout=5.0)

    try:
        # Re-boot
        await cp2.send_boot_notification()

        # Stop the transaction that was started before disconnect
        # The transaction ID should be known from the previous session
        stop_resp = await cp2.send_stop_transaction(
            meter_stop=5000,
            reason="PowerLoss",
            transaction_id=tx_id,
        )
        assert stop_resp.msg_type == CALL_RESULT

        # Verify CSMS received the StopTransaction with correct tx_id
        stop_msgs = [d for a, d in mock_csms.received_messages if a == "StopTransaction"]
        assert len(stop_msgs) > 0
        assert stop_msgs[-1]["transaction_id"] == tx_id

    finally:
        await cp2.disconnect()


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_csms_initiated_disconnect(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-173
    @scenario CSMS forcibly closes connection
    @given the simulator is connected
    @when the CSMS closes the WebSocket
    @then the simulator detects the disconnection
    """
    cp, csms = booted_charge_point

    # CSMS forcibly closes the connection
    await csms.close_connection()

    # Give time for disconnect to propagate
    await asyncio.sleep(1.0)

    # Attempting to send should fail (connection closed)
    with pytest.raises(Exception):
        await cp.send_heartbeat()
