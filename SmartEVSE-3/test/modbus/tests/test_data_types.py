"""
Data type decode tests for FLOAT32, INT32, and INT16.

Tests REQ-MTR-130 through REQ-MTR-134.

@feature Modbus Compatibility
"""

import sys
import os
import struct
import math
import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from decode_bridge import DecodeBridge
from frame_builder import build_response_data


class TestFLOAT32Positive:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-130
    @scenario IEEE 754 single-precision float decode
    """

    @pytest.mark.parametrize("value,divisor,expected", [
        (0.001, 0, 0),       # Very small, truncates to 0
        (1.0, 0, 1),
        (16.0, 0, 16),
        (32.0, 0, 32),
        (999.99, 0, 999),    # Truncated (not rounded)
        (16.0, -3, 16000),   # Multiply by 1000 (A -> mA)
        (1234.567, -3, 1234567),  # kWh -> Wh
    ])
    def test_float32_positive(self, bridge, value, divisor, expected):
        """@given registers containing FLOAT32 positive values
        @when decoded as FLOAT32
        @then each value matches within tolerance"""
        data = build_response_data([value], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', divisor)
        assert r.valid == 1
        # Allow small tolerance for float precision
        assert abs(r.value - expected) <= 1, \
            f"value={value}, divisor={divisor}: expected {expected}, got {r.value}"


class TestFLOAT32Negative:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-131
    @scenario Negative float for power export / reverse current
    """

    def test_negative_power(self, bridge):
        """@given FLOAT32 value -5000.0 (export power)
        @when decoded
        @then the result is -5000"""
        data = build_response_data([-5000.0], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', 0)
        assert r.valid == 1
        assert r.value == -5000

    def test_negative_current(self, bridge):
        """@given FLOAT32 value -16.0 (reversed CT)
        @when decoded with -3 divisor
        @then the result is -16000"""
        data = build_response_data([-16.0], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', -3)
        assert r.valid == 1
        assert r.value == -16000


class TestINT32:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-132
    @scenario 32-bit integer for high energy readings
    """

    def test_max_signed(self, bridge):
        """@given INT32 value 2147483647 (max signed)
        @when decoded
        @then the result matches without overflow"""
        data = build_response_data([2147483647], 'INT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'INT32', 0)
        assert r.valid == 1
        assert r.value == 2147483647

    def test_min_signed(self, bridge):
        """@given INT32 value -2147483648 (min signed)
        @when decoded
        @then the result matches"""
        data = build_response_data([-2147483648], 'INT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'INT32', 0)
        assert r.valid == 1
        assert r.value == -2147483648

    def test_zero(self, bridge):
        """@given INT32 value 0
        @when decoded
        @then the result is 0"""
        data = build_response_data([0], 'INT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'INT32', 0)
        assert r.valid == 1
        assert r.value == 0

    def test_with_divisor(self, bridge):
        """@given INT32 value 1600 with divisor 2
        @when decoded
        @then result is 16 (1600 / 100)"""
        data = build_response_data([1600], 'INT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'INT32', 2)
        assert r.valid == 1
        assert r.value == 16

    def test_with_negative_divisor(self, bridge):
        """@given INT32 value 16000 with divisor -1
        @when decoded
        @then result is 160000 (16000 * 10)"""
        data = build_response_data([16000], 'INT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'INT32', -1)
        assert r.valid == 1
        assert r.value == 160000


class TestINT16:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-133
    @scenario 16-bit integer boundaries
    """

    @pytest.mark.parametrize("value", [0, 1, 32767, -1, -32768])
    def test_int16_range(self, bridge, value):
        """@given INT16 values at boundaries
        @when decoded
        @then each value is correctly interpreted (signed)"""
        data = build_response_data([value], 'INT16', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'INT16', 0)
        assert r.valid == 1
        assert r.value == value, f"expected {value}, got {r.value}"

    def test_int16_with_divisor(self, bridge):
        """@given INT16 value 1600 with divisor 2
        @when decoded
        @then result is 16"""
        data = build_response_data([1600], 'INT16', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'INT16', 2)
        assert r.valid == 1
        assert r.value == 16


class TestFLOAT32Special:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-134
    @scenario Meter returns NaN or Infinity (sensor fault)
    """

    def test_nan_returns_zero(self, bridge):
        """@given registers containing IEEE 754 NaN
        @when decoded as FLOAT32
        @then the decode returns zero with valid=0"""
        # NaN: 0x7FC00000
        nan_bytes = struct.pack('>f', float('nan'))
        r = bridge.decode_value(nan_bytes, 0, 'HBF_HWF', 'FLOAT32', 0)
        assert r.valid == 0
        assert r.value == 0

    def test_infinity_returns_zero(self, bridge):
        """@given registers containing IEEE 754 +Infinity
        @when decoded as FLOAT32
        @then the decode returns zero with valid=0"""
        inf_bytes = struct.pack('>f', float('inf'))
        r = bridge.decode_value(inf_bytes, 0, 'HBF_HWF', 'FLOAT32', 0)
        assert r.valid == 0
        assert r.value == 0

    def test_negative_infinity_returns_zero(self, bridge):
        """@given registers containing IEEE 754 -Infinity
        @when decoded as FLOAT32
        @then the decode returns zero with valid=0"""
        inf_bytes = struct.pack('>f', float('-inf'))
        r = bridge.decode_value(inf_bytes, 0, 'HBF_HWF', 'FLOAT32', 0)
        assert r.valid == 0
        assert r.value == 0
