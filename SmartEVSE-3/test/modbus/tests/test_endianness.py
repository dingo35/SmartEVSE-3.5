"""
Endianness handling tests for all byte/word order combinations.

Tests REQ-MTR-120 through REQ-MTR-123.

@feature Modbus Compatibility
"""

import sys
import os
import struct
import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from decode_bridge import DecodeBridge
from frame_builder import build_response_data, encode_register_value, reorder_bytes


class TestHBF_HWF:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-120
    @scenario Standard big-endian float decode (Eastron, ABB, Finder, etc.)
    """

    def test_float32_16(self, bridge):
        """@given FLOAT32 value 16.0 encoded as HBF_HWF (0x41800000)
        @when decoded with HBF_HWF endianness
        @then the result is 16000 (with divisor -3 for mA)"""
        data = build_response_data([16.0], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', -3)
        assert r.valid == 1
        assert r.value == 16000

    def test_float32_zero(self, bridge):
        """@given FLOAT32 value 0.0 encoded as HBF_HWF
        @when decoded
        @then the result is 0"""
        data = build_response_data([0.0], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', 0)
        assert r.valid == 1
        assert r.value == 0

    def test_float32_negative(self, bridge):
        """@given FLOAT32 value -5000.0 encoded as HBF_HWF
        @when decoded
        @then the result is -5000"""
        data = build_response_data([-5000.0], 'FLOAT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'FLOAT32', 0)
        assert r.valid == 1
        assert r.value == -5000

    def test_int32_value(self, bridge):
        """@given INT32 value 100000 encoded as HBF_HWF
        @when decoded
        @then the result is 100000"""
        data = build_response_data([100000], 'INT32', 'HBF_HWF')
        r = bridge.decode_value(data, 0, 'HBF_HWF', 'INT32', 0)
        assert r.valid == 1
        assert r.value == 100000


class TestHBF_LWF:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-121
    @scenario Word-swapped big-endian decode (Phoenix Contact, Carlo Gavazzi)
    """

    def test_int32_100000(self, bridge):
        """@given INT32 value 100000 encoded as HBF_LWF
        @when decoded with HBF_LWF endianness
        @then the result is 100000"""
        data = build_response_data([100000], 'INT32', 'HBF_LWF')
        r = bridge.decode_value(data, 0, 'HBF_LWF', 'INT32', 0)
        assert r.valid == 1
        assert r.value == 100000

    def test_int32_negative(self, bridge):
        """@given INT32 value -16000 encoded as HBF_LWF
        @when decoded
        @then the result is -16000"""
        data = build_response_data([-16000], 'INT32', 'HBF_LWF')
        r = bridge.decode_value(data, 0, 'HBF_LWF', 'INT32', 0)
        assert r.valid == 1
        assert r.value == -16000

    def test_int32_zero(self, bridge):
        """@given INT32 value 0 encoded as HBF_LWF
        @when decoded
        @then the result is 0"""
        data = build_response_data([0], 'INT32', 'HBF_LWF')
        r = bridge.decode_value(data, 0, 'HBF_LWF', 'INT32', 0)
        assert r.valid == 1
        assert r.value == 0

    def test_float32_value(self, bridge):
        """@given FLOAT32 value 12.3 encoded as HBF_LWF
        @when decoded
        @then the result is approximately 12300 (with divisor -3)"""
        data = build_response_data([12.3], 'FLOAT32', 'HBF_LWF')
        r = bridge.decode_value(data, 0, 'HBF_LWF', 'FLOAT32', -3)
        assert r.valid == 1
        assert r.value == 12300


class TestLBF_LWF:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-122
    @scenario Little-endian decode (Custom meter default)
    """

    def test_int32_value(self, bridge):
        """@given INT32 value 123456 encoded as LBF_LWF
        @when decoded with LBF_LWF endianness
        @then the result is 123456"""
        data = build_response_data([123456], 'INT32', 'LBF_LWF')
        r = bridge.decode_value(data, 0, 'LBF_LWF', 'INT32', 0)
        assert r.valid == 1
        assert r.value == 123456

    def test_int32_negative(self, bridge):
        """@given INT32 value -50000 encoded as LBF_LWF
        @when decoded
        @then the result is -50000"""
        data = build_response_data([-50000], 'INT32', 'LBF_LWF')
        r = bridge.decode_value(data, 0, 'LBF_LWF', 'INT32', 0)
        assert r.valid == 1
        assert r.value == -50000

    def test_float32_value(self, bridge):
        """@given FLOAT32 value 32.0 encoded as LBF_LWF
        @when decoded
        @then the result is 32"""
        data = build_response_data([32.0], 'FLOAT32', 'LBF_LWF')
        r = bridge.decode_value(data, 0, 'LBF_LWF', 'FLOAT32', 0)
        assert r.valid == 1
        assert r.value == 32


class TestINT16Endianness:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-123
    @scenario INT16 decode is endianness-independent for single register
    """

    @pytest.mark.parametrize("endianness", ['HBF_HWF', 'HBF_LWF', 'LBF_LWF'])
    def test_int16_same_across_endianness(self, bridge, endianness):
        """@given INT16 value 1600 in a single register
        @when decoded with any endianness setting
        @then the result is the same"""
        data = build_response_data([1600], 'INT16', endianness)
        r = bridge.decode_value(data, 0, endianness, 'INT16', 0)
        assert r.valid == 1
        assert r.value == 1600

    @pytest.mark.parametrize("endianness", ['HBF_HWF', 'HBF_LWF', 'LBF_LWF'])
    def test_int16_negative(self, bridge, endianness):
        """@given INT16 value -100 in a single register
        @when decoded with any endianness
        @then the result is -100"""
        data = build_response_data([-100], 'INT16', endianness)
        r = bridge.decode_value(data, 0, endianness, 'INT16', 0)
        assert r.valid == 1
        assert r.value == -100
