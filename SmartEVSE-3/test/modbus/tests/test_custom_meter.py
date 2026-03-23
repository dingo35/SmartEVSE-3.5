"""
Custom meter tests: user-defined register configuration.

Tests REQ-MTR-103.

@feature Modbus Compatibility
"""

import sys
import os
import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from decode_bridge import DecodeBridge
from frame_builder import build_response_data
from meter_profiles import custom


class TestCustomMeterRegisters:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-103
    @scenario Custom meter type allows arbitrary register configuration
    """

    def test_custom_current_decode(self, bridge):
        """@given custom meter (LBF_LWF, INT32) with known current values
        @when decoded through the pipeline
        @then correct mA values are extracted"""
        tv = custom.TEST_VECTORS[0]
        data = build_response_data(tv['current_values'], 'INT32', 'LBF_LWF')
        divisor = custom.I_DIVISOR - 3  # = -3, multiply by 1000

        results = []
        for i in range(3):
            r = bridge.decode_value(data, i, 'LBF_LWF', 'INT32', divisor)
            assert r.valid == 1
            results.append(r.value)

        for i in range(3):
            assert results[i] == tv['expected_current_mA'][i]

    def test_custom_power_decode(self, bridge):
        """@given custom meter with power value
        @when decoded
        @then correct W value is extracted"""
        tv = custom.TEST_VECTORS[0]
        data = build_response_data([tv['power_value']], 'INT32', 'LBF_LWF')
        r = bridge.decode_value(data, 0, 'LBF_LWF', 'INT32', custom.P_DIVISOR)
        assert r.valid == 1
        assert r.value == tv['expected_power_W']

    def test_custom_negative_power(self, bridge):
        """@given custom meter with negative power (export)
        @when decoded
        @then negative value is preserved"""
        tv = custom.TEST_VECTORS[2]
        data = build_response_data([tv['power_value']], 'INT32', 'LBF_LWF')
        r = bridge.decode_value(data, 0, 'LBF_LWF', 'INT32', custom.P_DIVISOR)
        assert r.valid == 1
        assert r.value == tv['expected_power_W']

    def test_custom_zero_current(self, bridge):
        """@given custom meter with zero current on all phases
        @when decoded
        @then all values are 0"""
        tv = custom.TEST_VECTORS[1]
        data = build_response_data(tv['current_values'], 'INT32', 'LBF_LWF')
        divisor = custom.I_DIVISOR - 3

        for i in range(3):
            r = bridge.decode_value(data, i, 'LBF_LWF', 'INT32', divisor)
            assert r.valid == 1
            assert r.value == 0
