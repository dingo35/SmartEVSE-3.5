"""
OCPP 1.6J MeterValues Tests.

REQ-OCPP-130: MeterValues contains valid measurands
REQ-OCPP-131: MeterValues includes energy reading
REQ-OCPP-132: MeterValues includes current per phase
REQ-OCPP-133: MeterValues clock-aligned interval

@feature OCPP Compatibility
"""

import asyncio

import pytest

from message_replay import (
    ChargePointSimulator,
    CALL_RESULT,
    VALID_MEASURANDS,
    VALID_UNITS,
    VALID_PHASES,
    MEASURAND_UNITS,
)


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_meter_values_valid_measurands(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-130
    @scenario MeterValues payload structure
    @given a transaction is active
    @when the simulator sends MeterValues
    @then each sampledValue contains measurand, value, unit fields
    @and measurand is one of the OCPP 1.6 defined values
    @and unit matches the measurand
    """
    cp, csms = booted_charge_point

    await cp.send_authorize("AB12CD34")
    start_resp = await cp.send_start_transaction(id_tag="AB12CD34", meter_start=0)
    tx_id = start_resp.payload["transactionId"]

    response = await cp.send_meter_values(
        energy_wh=1000,
        current_a=16.0,
        voltage_v=230.0,
        power_w=3680.0,
        phases=3,
        transaction_id=tx_id,
    )
    assert response.msg_type == CALL_RESULT

    # Verify CSMS received meter values
    meter_msg = await csms.wait_for_message("MeterValues", timeout=5)
    assert "meter_value" in meter_msg

    for mv in meter_msg["meter_value"]:
        assert "timestamp" in mv
        assert "sampled_value" in mv

        for sv in mv["sampled_value"]:
            # Every sampled value must have measurand, value, unit
            assert "measurand" in sv, f"Missing measurand in sampledValue: {sv}"
            assert "value" in sv, f"Missing value in sampledValue: {sv}"
            assert "unit" in sv, f"Missing unit in sampledValue: {sv}"

            # Measurand must be valid OCPP 1.6 value
            assert sv["measurand"] in VALID_MEASURANDS, (
                f"Invalid measurand: {sv['measurand']}"
            )

            # Unit must be valid
            assert sv["unit"] in VALID_UNITS, (
                f"Invalid unit: {sv['unit']}"
            )

            # Unit must match measurand
            measurand = sv["measurand"]
            if measurand in MEASURAND_UNITS and MEASURAND_UNITS[measurand]:
                assert sv["unit"] in MEASURAND_UNITS[measurand], (
                    f"Unit {sv['unit']} does not match measurand {measurand}, "
                    f"expected one of {MEASURAND_UNITS[measurand]}"
                )

            # Phase must be valid if present
            if "phase" in sv:
                assert sv["phase"] in VALID_PHASES, (
                    f"Invalid phase: {sv['phase']}"
                )


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_meter_values_includes_energy(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-131
    @scenario Energy active import in MeterValues
    @given a transaction is active
    @when the simulator sends MeterValues
    @then at least one sampledValue has measurand "Energy.Active.Import.Register"
    @and the value is a numeric string
    """
    cp, csms = booted_charge_point

    await cp.send_authorize("AB12CD34")
    start_resp = await cp.send_start_transaction(id_tag="AB12CD34", meter_start=0)
    tx_id = start_resp.payload["transactionId"]

    await cp.send_meter_values(energy_wh=2500, transaction_id=tx_id)

    meter_msg = await csms.wait_for_message("MeterValues", timeout=5)

    # Find energy measurand
    found_energy = False
    for mv in meter_msg["meter_value"]:
        for sv in mv["sampled_value"]:
            if sv.get("measurand") == "Energy.Active.Import.Register":
                found_energy = True
                # Value must be numeric
                value = float(sv["value"])
                assert value >= 0, f"Energy value must be non-negative: {value}"
                assert sv["unit"] in ("Wh", "kWh")

    assert found_energy, "MeterValues must include Energy.Active.Import.Register"


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_meter_values_per_phase_current(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-132
    @scenario Per-phase current reporting
    @given a transaction is active on a 3-phase connection
    @when the simulator sends MeterValues
    @then sampledValues include Current.Import for L1, L2, L3
    @and phase is correctly set to "L1", "L2", "L3"
    """
    cp, csms = booted_charge_point

    await cp.send_authorize("AB12CD34")
    start_resp = await cp.send_start_transaction(id_tag="AB12CD34", meter_start=0)
    tx_id = start_resp.payload["transactionId"]

    await cp.send_meter_values(
        energy_wh=1000,
        current_a=16.0,
        phases=3,
        transaction_id=tx_id,
    )

    meter_msg = await csms.wait_for_message("MeterValues", timeout=5)

    # Collect phases that have Current.Import
    current_phases = set()
    for mv in meter_msg["meter_value"]:
        for sv in mv["sampled_value"]:
            if sv.get("measurand") == "Current.Import" and "phase" in sv:
                current_phases.add(sv["phase"])
                # Value must be numeric and non-negative
                value = float(sv["value"])
                assert value >= 0
                assert sv["unit"] == "A"

    # All three phases must be present
    assert "L1" in current_phases, "Missing Current.Import for L1"
    assert "L2" in current_phases, "Missing Current.Import for L2"
    assert "L3" in current_phases, "Missing Current.Import for L3"


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_meter_values_single_phase(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-132
    @scenario Single-phase current reporting
    @given a transaction is active on a 1-phase connection
    @when the simulator sends MeterValues
    @then sampledValues include Current.Import for L1 only
    """
    cp, csms = booted_charge_point

    await cp.send_authorize("AB12CD34")
    start_resp = await cp.send_start_transaction(id_tag="AB12CD34", meter_start=0)
    tx_id = start_resp.payload["transactionId"]

    await cp.send_meter_values(
        energy_wh=500,
        current_a=32.0,
        phases=1,
        transaction_id=tx_id,
    )

    meter_msg = await csms.wait_for_message("MeterValues", timeout=5)

    current_phases = set()
    for mv in meter_msg["meter_value"]:
        for sv in mv["sampled_value"]:
            if sv.get("measurand") == "Current.Import" and "phase" in sv:
                current_phases.add(sv["phase"])

    assert "L1" in current_phases, "Must include L1 current"
    assert "L2" not in current_phases, "Should not include L2 for single phase"
    assert "L3" not in current_phases, "Should not include L3 for single phase"


@pytest.mark.asyncio
@pytest.mark.timeout(15)
async def test_meter_values_clock_aligned_interval(booted_charge_point):
    """
    @feature OCPP Compatibility
    @req REQ-OCPP-133
    @scenario Clock-aligned meter values at configured interval
    @given the CSMS has configured MeterValueSampleInterval
    @when multiple meter values are sent during a transaction
    @then the timestamps are present and valid ISO 8601
    """
    cp, csms = booted_charge_point

    await cp.send_authorize("AB12CD34")
    start_resp = await cp.send_start_transaction(id_tag="AB12CD34", meter_start=0)
    tx_id = start_resp.payload["transactionId"]

    # Send multiple meter values
    from datetime import datetime

    for i in range(3):
        await cp.send_meter_values(energy_wh=(i + 1) * 1000, transaction_id=tx_id)

    meter_msgs = await csms.wait_for_message_count("MeterValues", 3, timeout=5)

    # All meter values must have valid timestamps
    for msg in meter_msgs:
        for mv in msg["meter_value"]:
            ts = mv["timestamp"]
            dt = datetime.fromisoformat(ts.replace("Z", "+00:00"))
            assert dt is not None, f"Invalid timestamp: {ts}"
