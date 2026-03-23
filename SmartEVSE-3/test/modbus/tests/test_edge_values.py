"""
Edge value tests: max current, very small current, large energy, InvEastron.

Tests REQ-MTR-170 through REQ-MTR-173.

@feature Modbus Compatibility
"""

import sys
import os
import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from decode_bridge import DecodeBridge
from frame_builder import build_response_data


class TestMaxCurrent:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-170
    @scenario High current reading near CT maximum
    """

    def test_100A_ct_max(self, bridge):
        """@given a FLOAT32 register containing 100.0 Amps (max CT rating)
        @when decoded with divisor -3
        @then Irms = 100000 mA without overflow"""
        data = build_response_data([100.0], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', -3)
        assert r.valid == 1
        assert r.value == 100000


class TestSmallCurrent:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-171
    @scenario Sub-amp standby current
    """

    def test_standby_50mA(self, bridge):
        """@given a FLOAT32 register containing 0.05 Amps
        @when decoded with divisor -3
        @then Irms = 50 mA (not rounded to zero)"""
        data = build_response_data([0.05], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', -3)
        assert r.valid == 1
        assert r.value == 50

    def test_very_small_1mA(self, bridge):
        """@given a FLOAT32 register containing 0.001 Amps
        @when decoded with divisor -3
        @then Irms = 1 mA"""
        data = build_response_data([0.001], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', -3)
        assert r.valid == 1
        assert r.value == 1


class TestLargeEnergy:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-172
    @scenario Energy counter after years of operation
    """

    def test_large_energy_float(self, bridge):
        """@given a FLOAT32 register containing 99999.0 kWh
        @when decoded with divisor -3 (kWh -> Wh)
        @then the value is 99999000 Wh without overflow"""
        data = build_response_data([99999.0], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', -3)
        assert r.valid == 1
        assert r.value == 99999000

    def test_large_energy_int32(self, bridge):
        """@given an INT32 register containing 999999 (0.01kWh units)
        @when decoded with divisor -1 (multiply by 10 for Wh)
        @then the value does not overflow"""
        data = build_response_data([999999], 'INT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'INT32', -1)
        assert r.valid == 1
        assert r.value == 9999990


class TestInvEastron:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-173
    @scenario Inverted Eastron CT polarity handling
    """

    def test_inv_eastron_negative_current(self, bridge):
        """@given meter type 5 (InvEastron) with current -16.0A
        @when decoded (before firmware sign inversion)
        @then raw decoded value is -16000 mA
        @and firmware inverts sign to get +16000 mA"""
        # InvEastron has same decode as Eastron, but firmware negates power
        # and uses power sign to flip current sign
        data = build_response_data([-16.0], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', -3)
        assert r.valid == 1
        assert r.value == -16000
        # Firmware would negate: abs(-16000) = 16000

    def test_inv_eastron_positive_current(self, bridge):
        """@given InvEastron with current 16.0A (normal direction)
        @when decoded
        @then raw decoded value is 16000 mA"""
        data = build_response_data([16.0], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', -3)
        assert r.valid == 1
        assert r.value == 16000
