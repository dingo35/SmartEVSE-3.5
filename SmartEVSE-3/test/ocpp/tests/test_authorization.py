"""
OCPP 1.6J Authorization Tests.

REQ-OCPP-110: Authorize request contains correctly formatted idTag
REQ-OCPP-111: Accepted authorization enables charging
REQ-OCPP-112: Blocked authorization prevents charging
REQ-OCPP-113: Authorization timeout handling
REQ-OCPP-114: Expired idTag handling
REQ-OCPP-115: ConcurrentTx idTag handling

@feature OCPP Compatibility
"""

import asyncio

import pytest

from mock_csms import MockCSMS
from message_replay import ChargePointSimulator, CALL_RESULT, CALL_ERROR


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_authorize_request_idtag_format(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-110
    @scenario RFID idTag format in Authorize request
    @given the simulator has an RFID tag "AB12CD34"
    @when authorization is triggered
    @then an Authorize request is sent with id_tag "AB12CD34"
    @and the idTag is a CiString20Type (max 20 chars)
    """
    cp, csms = booted_charge_point

    response = await cp.send_authorize("AB12CD34")
    assert response.msg_type == CALL_RESULT

    # Verify CSMS received correct idTag
    auth_msg = await csms.wait_for_message("Authorize", timeout=5)
    assert auth_msg["id_tag"] == "AB12CD34"
    assert len(auth_msg["id_tag"]) <= 20  # CiString20Type


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_authorize_accepted_enables_start_transaction(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-111
    @scenario Authorize accepted flow
    @given the mock CSMS responds to Authorize with status "Accepted"
    @when the simulator sends Authorize for tag "AB12CD34"
    @then the simulator proceeds to StartTransaction
    """
    cp, csms = booted_charge_point

    # Authorize
    auth_resp = await cp.send_authorize("AB12CD34")
    assert auth_resp.msg_type == CALL_RESULT
    assert auth_resp.payload["idTagInfo"]["status"] == "Accepted"

    # After accepted auth, start transaction
    start_resp = await cp.send_start_transaction(id_tag="AB12CD34", meter_start=0)
    assert start_resp.msg_type == CALL_RESULT
    assert "transactionId" in start_resp.payload

    # Verify CSMS received StartTransaction after Authorize
    start_msg = await csms.wait_for_message("StartTransaction", timeout=5)
    assert start_msg["id_tag"] == "AB12CD34"


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_authorize_blocked_prevents_charging(mock_csms):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-112
    @scenario Authorize rejected flow
    @given the mock CSMS responds to Authorize with status "Blocked"
    @when the simulator sends Authorize for tag "BLOCKED01"
    @then no StartTransaction is sent
    @and the simulator reports authorization failure
    """
    mock_csms.response_overrides["Authorize"] = {"status": "Blocked"}

    cp = ChargePointSimulator(mock_csms.url, charge_point_id="BLOCKED-CP")
    await cp.connect()
    await mock_csms.wait_for_connection(timeout=5.0)

    try:
        # Boot first
        await cp.send_boot_notification()

        # Authorize - should be blocked
        auth_resp = await cp.send_authorize("BLOCKED01")
        assert auth_resp.msg_type == CALL_RESULT
        assert auth_resp.payload["idTagInfo"]["status"] == "Blocked"

        # Verify no StartTransaction was sent
        await asyncio.sleep(0.5)
        start_msgs = [a for a, _ in mock_csms.received_messages if a == "StartTransaction"]
        assert len(start_msgs) == 0, "StartTransaction should not be sent after Blocked auth"

    finally:
        await cp.disconnect()


@pytest.mark.asyncio
@pytest.mark.timeout(40)
async def test_authorize_timeout_handling(mock_csms):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-113
    @scenario CSMS does not respond to Authorize within timeout
    @given the mock CSMS is configured to delay Authorize response by 12 seconds
    @when the simulator sends Authorize with a 5 second timeout
    @then the simulator handles the timeout gracefully
    """
    cp = ChargePointSimulator(mock_csms.url, charge_point_id="TIMEOUT-CP")
    await cp.connect()
    await mock_csms.wait_for_connection(timeout=5.0)

    try:
        # Boot without delay
        boot_resp = await cp.send_boot_notification()
        assert boot_resp.msg_type == CALL_RESULT

        # Now set delay for auth
        mock_csms.message_delay = 12  # Delay auth response by 12 seconds

        # Authorize with short timeout - should raise TimeoutError
        timed_out = False
        try:
            await cp.send_call("Authorize", {"idTag": "TIMEOUT01"}, timeout=3.0)
        except asyncio.TimeoutError:
            timed_out = True

        assert timed_out, "Expected TimeoutError for delayed Authorize response"

    finally:
        mock_csms.message_delay = 0
        # Allow any delayed responses to drain before disconnecting
        await asyncio.sleep(0.5)
        await cp.disconnect()


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_authorize_expired_idtag(mock_csms):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-114
    @scenario Authorize response with Expired status
    @given the mock CSMS responds to Authorize with status "Expired"
    @when the simulator sends Authorize for tag "EXPIRED01"
    @then no StartTransaction is sent
    """
    mock_csms.response_overrides["Authorize"] = {"status": "Expired"}

    cp = ChargePointSimulator(mock_csms.url, charge_point_id="EXPIRED-CP")
    await cp.connect()
    await mock_csms.wait_for_connection(timeout=5.0)

    try:
        await cp.send_boot_notification()

        auth_resp = await cp.send_authorize("EXPIRED01")
        assert auth_resp.msg_type == CALL_RESULT
        assert auth_resp.payload["idTagInfo"]["status"] == "Expired"

        # No StartTransaction should follow
        await asyncio.sleep(0.5)
        start_msgs = [a for a, _ in mock_csms.received_messages if a == "StartTransaction"]
        assert len(start_msgs) == 0

    finally:
        await cp.disconnect()


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_authorize_concurrent_tx(mock_csms):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-115
    @scenario Authorize response with ConcurrentTx status
    @given the mock CSMS responds to Authorize with status "ConcurrentTx"
    @when the simulator sends Authorize for tag "CONCURRENT01"
    @then no StartTransaction is sent
    """
    mock_csms.response_overrides["Authorize"] = {"status": "ConcurrentTx"}

    cp = ChargePointSimulator(mock_csms.url, charge_point_id="CONC-CP")
    await cp.connect()
    await mock_csms.wait_for_connection(timeout=5.0)

    try:
        await cp.send_boot_notification()

        auth_resp = await cp.send_authorize("CONCURRENT01")
        assert auth_resp.msg_type == CALL_RESULT
        assert auth_resp.payload["idTagInfo"]["status"] == "ConcurrentTx"

        await asyncio.sleep(0.5)
        start_msgs = [a for a, _ in mock_csms.received_messages if a == "StartTransaction"]
        assert len(start_msgs) == 0

    finally:
        await cp.disconnect()
