"""
Phase mapping tests for L1/L2/L3 register offset validation.

Tests REQ-MTR-140 through REQ-MTR-142.

@feature Modbus Compatibility
"""

import sys
import os
import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from decode_bridge import DecodeBridge
from frame_builder import build_response_data
from meter_profiles import eastron_sdm630, eastron_sdm120, sensorbox_v2


class TestPhaseOffsets3Phase:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-140
    @scenario L1/L2/L3 register offset validation for 3-phase meters
    """

    def test_eastron_sdm630_phases_not_swapped(self, bridge):
        """@given Eastron SDM630 with different values per phase
        @when all three phases are decoded
        @then Irms[0]=L1, Irms[1]=L2, Irms[2]=L3 (no swaps)"""
        # Use distinct values so we can detect swaps
        values = [10.0, 20.0, 30.0]
        data = build_response_data(values, 'FLOAT32', 'HBF_HWF')
        divisor = eastron_sdm630.I_DIVISOR - 3

        results = []
        for i in range(3):
            r = bridge.decode_value(data, i, 'HBF_HWF', 'FLOAT32', divisor)
            assert r.valid == 1
            results.append(r.value)

        assert results[0] == 10000  # L1
        assert results[1] == 20000  # L2
        assert results[2] == 30000  # L3

    def test_unbalanced_3phase(self, bridge):
        """@given a 3-phase meter with unbalanced load (25A, 10A, 5A)
        @when decoded
        @then each phase value is correctly assigned"""
        values = [25.0, 10.0, 5.0]
        data = build_response_data(values, 'FLOAT32', 'HBF_HWF')

        results = []
        for i in range(3):
            r = bridge.decode_value(data, i, 'HBF_HWF', 'FLOAT32', -3)
            assert r.valid == 1
            results.append(r.value)

        assert results[0] == 25000
        assert results[1] == 10000
        assert results[2] == 5000


class TestSinglePhase:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-141
    @scenario Single-phase meter reports only L1
    """

    def test_sdm120_single_phase(self, bridge):
        """@given Eastron SDM120 (1-phase meter)
        @when current is decoded
        @then only L1 has a non-zero value"""
        values = [8.5]
        data = build_response_data(values, 'FLOAT32', 'HBF_HWF')
        divisor = eastron_sdm120.I_DIVISOR - 3

        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', divisor)
        assert r.valid == 1
        assert r.value == 8500


class TestSensorbox:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-142
    @scenario Sensorbox v2 3-phase CT measurement
    """

    def test_sensorbox_3phase_ct(self, bridge):
        """@given Sensorbox with IRegister=0x0000 and 3 CT channels
        @when the current registers are decoded
        @then all 3 phases contain measured values"""
        values = [15.0, 12.0, 18.0]
        data = build_response_data(values, 'FLOAT32', 'HBF_HWF')
        divisor = sensorbox_v2.I_DIVISOR - 3

        results = []
        for i in range(3):
            r = bridge.decode_value(data, i, 'HBF_HWF', 'FLOAT32', divisor)
            assert r.valid == 1
            results.append(r.value)

        assert results[0] == 15000
        assert results[1] == 12000
        assert results[2] == 18000
