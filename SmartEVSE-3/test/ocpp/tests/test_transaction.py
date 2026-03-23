"""
OCPP 1.6J Transaction Lifecycle Tests.

REQ-OCPP-120: StartTransaction contains required fields
REQ-OCPP-121: StopTransaction contains required fields
REQ-OCPP-122: Transaction energy is monotonically increasing
REQ-OCPP-123: StopTransaction on PowerLoss reason
REQ-OCPP-124: StartTransaction with rejected transactionId

@feature OCPP Compatibility
"""

import asyncio
from datetime import datetime

import pytest

from mock_csms import MockCSMS
from message_replay import (
    ChargePointSimulator,
    CALL_RESULT,
    VALID_STOP_REASONS,
)


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_start_transaction_required_fields(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-120
    @scenario StartTransaction message completeness
    @given authorization is accepted for tag "AB12CD34"
    @when the simulator sends StartTransaction
    @then the message contains connectorId, idTag, meterStart, and timestamp
    @and connectorId is 1 (single connector)
    @and meterStart is a non-negative integer (Wh)
    @and timestamp is ISO 8601 format
    """
    cp, csms = booted_charge_point

    # Authorize first
    await cp.send_authorize("AB12CD34")

    # Start transaction
    response = await cp.send_start_transaction(
        connector_id=1,
        id_tag="AB12CD34",
        meter_start=0,
    )
    assert response.msg_type == CALL_RESULT
    assert "transactionId" in response.payload

    # Verify CSMS received correct fields
    start_msg = await csms.wait_for_message("StartTransaction", timeout=5)
    assert start_msg["connector_id"] == 1
    assert start_msg["id_tag"] == "AB12CD34"
    assert isinstance(start_msg["meter_start"], int)
    assert start_msg["meter_start"] >= 0

    # Verify timestamp is ISO 8601
    ts = start_msg["timestamp"]
    dt = datetime.fromisoformat(ts.replace("Z", "+00:00"))
    assert dt is not None


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_stop_transaction_required_fields(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-121
    @scenario StopTransaction message completeness
    @given a transaction is active with a transactionId
    @when the simulator sends StopTransaction
    @then the message contains transactionId, meterStop, and timestamp
    @and meterStop >= meterStart from the StartTransaction
    @and reason is one of the OCPP 1.6 defined reasons
    """
    cp, csms = booted_charge_point

    await cp.send_authorize("AB12CD34")
    start_resp = await cp.send_start_transaction(id_tag="AB12CD34", meter_start=100)
    tx_id = start_resp.payload["transactionId"]

    # Stop transaction
    stop_resp = await cp.send_stop_transaction(
        meter_stop=5100,
        reason="Local",
        transaction_id=tx_id,
    )
    assert stop_resp.msg_type == CALL_RESULT

    # Verify CSMS received correct fields
    stop_msg = await csms.wait_for_message("StopTransaction", timeout=5)
    assert stop_msg["transaction_id"] == tx_id
    assert isinstance(stop_msg["meter_stop"], int)
    assert stop_msg["meter_stop"] >= 100  # >= meterStart

    # Verify timestamp
    ts = stop_msg["timestamp"]
    dt = datetime.fromisoformat(ts.replace("Z", "+00:00"))
    assert dt is not None

    # Verify reason is valid
    assert stop_msg.get("reason") in VALID_STOP_REASONS


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_transaction_energy_monotonically_increasing(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-122
    @scenario Meter values during transaction show non-decreasing energy
    @given a transaction is active
    @when multiple MeterValues are sent during the transaction
    @then each Energy.Active.Import.Register value >= previous value
    """
    cp, csms = booted_charge_point

    await cp.send_authorize("AB12CD34")
    start_resp = await cp.send_start_transaction(id_tag="AB12CD34", meter_start=0)
    tx_id = start_resp.payload["transactionId"]

    # Send multiple meter values with increasing energy
    energy_values = [1000, 2500, 5000, 7500, 10000]
    for energy in energy_values:
        await cp.send_meter_values(energy_wh=energy, transaction_id=tx_id)

    # Verify monotonically increasing
    meter_msgs = await csms.wait_for_message_count("MeterValues", len(energy_values), timeout=5)

    prev_energy = -1
    for msg in meter_msgs:
        for mv in msg["meter_value"]:
            for sv in mv["sampled_value"]:
                if sv.get("measurand") == "Energy.Active.Import.Register":
                    energy = int(sv["value"])
                    assert energy >= prev_energy, (
                        f"Energy decreased: {energy} < {prev_energy}"
                    )
                    prev_energy = energy


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_stop_transaction_powerloss_reason(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-123
    @scenario Clean shutdown stops active transaction
    @given a transaction is active
    @when the simulator stops the transaction due to power loss
    @then StopTransaction is sent with reason "PowerLoss" or "Other"
    """
    cp, csms = booted_charge_point

    await cp.send_authorize("AB12CD34")
    start_resp = await cp.send_start_transaction(id_tag="AB12CD34", meter_start=0)
    tx_id = start_resp.payload["transactionId"]

    # Stop with PowerLoss reason
    stop_resp = await cp.send_stop_transaction(
        meter_stop=1000,
        reason="PowerLoss",
        transaction_id=tx_id,
    )
    assert stop_resp.msg_type == CALL_RESULT

    stop_msg = await csms.wait_for_message("StopTransaction", timeout=5)
    assert stop_msg["reason"] in ("PowerLoss", "Other")


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_start_transaction_rejected(mock_csms):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-124
    @scenario CSMS rejects StartTransaction
    @given the mock CSMS responds to StartTransaction with idTagInfo status "Invalid"
    @when the simulator sends StartTransaction
    @then the simulator does not proceed with charging
    @and no MeterValues are sent for this transaction
    """
    mock_csms.response_overrides["StartTransaction"] = {
        "transaction_id": -1,
        "status": "Invalid",
    }

    cp = ChargePointSimulator(mock_csms.url, charge_point_id="REJECTED-TX-CP")
    await cp.connect()
    await mock_csms.wait_for_connection(timeout=5.0)

    try:
        await cp.send_boot_notification()
        await cp.send_authorize("AB12CD34")

        start_resp = await cp.send_start_transaction(id_tag="AB12CD34", meter_start=0)
        assert start_resp.msg_type == CALL_RESULT
        assert start_resp.payload["idTagInfo"]["status"] == "Invalid"

        # No MeterValues should be sent for rejected transaction
        await asyncio.sleep(0.5)
        meter_msgs = [a for a, _ in mock_csms.received_messages if a == "MeterValues"]
        assert len(meter_msgs) == 0, "MeterValues should not be sent for rejected transaction"

    finally:
        await cp.disconnect()


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_full_transaction_lifecycle(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-120
    @scenario Complete transaction lifecycle
    @given a booted charge point
    @when a full session is executed: auth -> start -> meter -> stop
    @then all messages complete successfully
    @and meterStop >= meterStart
    """
    cp, csms = booted_charge_point

    results = await cp.run_full_session(
        id_tag="LIFECYCLE01",
        energy_wh=5000,
        meter_intervals=3,
        phases=3,
    )

    # Verify all stages completed
    assert results["authorize"].msg_type == CALL_RESULT
    assert results["authorize"].payload["idTagInfo"]["status"] == "Accepted"

    assert results["start_transaction"].msg_type == CALL_RESULT
    assert "transactionId" in results["start_transaction"].payload

    assert len(results["meter_values"]) == 3

    assert results["stop_transaction"].msg_type == CALL_RESULT

    # Verify the CSMS received all expected messages
    msg_types = [a for a, _ in csms.received_messages]
    assert "BootNotification" in msg_types
    assert "Authorize" in msg_types
    assert "StartTransaction" in msg_types
    assert "MeterValues" in msg_types
    assert "StopTransaction" in msg_types
    assert "StatusNotification" in msg_types
