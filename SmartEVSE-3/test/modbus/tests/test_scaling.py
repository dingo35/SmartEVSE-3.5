"""
Value scaling tests: current (A->mA), energy (kWh->Wh), power sign.

Tests REQ-MTR-150 through REQ-MTR-153.

@feature Modbus Compatibility
"""

import sys
import os
import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from decode_bridge import DecodeBridge
from frame_builder import build_response_data


class TestCurrentScaling:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-150
    @scenario Meter reports current in Amps, SmartEVSE stores in mA
    """

    def test_amps_to_milliamps(self, bridge):
        """@given a FLOAT32 register containing 16.543 Amps
        @when decoded with divisor -3
        @then the stored value is 16543 mA (truncated, not rounded)"""
        data = build_response_data([16.543], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', -3)
        assert r.valid == 1
        # C truncates float->int (does not round)
        assert r.value == 16543

    def test_small_current_not_lost(self, bridge):
        """@given a FLOAT32 register containing 0.05 Amps
        @when decoded with divisor -3
        @then the stored value is 50 mA"""
        data = build_response_data([0.05], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', -3)
        assert r.valid == 1
        assert r.value == 50


class TestEnergyScaling:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-151
    @scenario Meter reports energy in kWh, SmartEVSE stores in Wh
    """

    def test_kwh_to_wh(self, bridge):
        """@given a FLOAT32 register containing 1234.567 kWh
        @when decoded with divisor -3 (firmware convention for energy)
        @then the stored value is 1234567 Wh"""
        data = build_response_data([1234.567], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', -3)
        assert r.valid == 1
        assert r.value == 1234567

    def test_very_small_energy(self, bridge):
        """@given a FLOAT32 register containing 0.001 kWh
        @when decoded with divisor -3
        @then the stored value is 1 Wh"""
        data = build_response_data([0.001], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', -3)
        assert r.valid == 1
        assert r.value == 1


class TestPowerSign:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-152
    @scenario Power direction convention
    """

    def test_export_power_negative(self, bridge):
        """@given a FLOAT32 register containing -3000.0 W (export)
        @when decoded
        @then the power value is -3000 W"""
        data = build_response_data([-3000.0], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', 0)
        assert r.valid == 1
        assert r.value == -3000

    def test_import_power_positive(self, bridge):
        """@given a FLOAT32 register containing 5000.0 W (import)
        @when decoded
        @then the power value is 5000 W"""
        data = build_response_data([5000.0], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', 0)
        assert r.valid == 1
        assert r.value == 5000


class TestZeroCurrent:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-153
    @scenario No load condition
    """

    def test_float32_zero_current(self, bridge):
        """@given FLOAT32 registers containing 0.0 for all phases
        @when decoded
        @then all phase values are 0 mA"""
        data = build_response_data([0.0, 0.0, 0.0], 'FLOAT32', 'HBF_HWF')
        for i in range(3):
            r = bridge.decode_value(data, i, 'HBF_HWF', 'FLOAT32', -3)
            assert r.valid == 1
            assert r.value == 0

    def test_int32_zero_current(self, bridge):
        """@given INT32 registers containing 0 for all phases
        @when decoded
        @then all phase values are 0"""
        data = build_response_data([0, 0, 0], 'INT32', 'HBF_HWF')
        for i in range(3):
            r = bridge.decode_value(data, i, 'HBF_HWF', 'INT32', 0)
            assert r.valid == 1
            assert r.value == 0

    def test_int16_zero_current(self, bridge):
        """@given INT16 registers containing 0 for all phases
        @when decoded
        @then all phase values are 0"""
        data = build_response_data([0, 0, 0], 'INT16', 'HBF_HWF')
        for i in range(3):
            r = bridge.decode_value(data, i, 'HBF_HWF', 'INT16', 0)
            assert r.valid == 1
            assert r.value == 0
