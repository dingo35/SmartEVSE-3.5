"""
Error response handling tests: exceptions, CRC errors, truncated frames.

Tests REQ-MTR-160 through REQ-MTR-163.

@feature Modbus Compatibility
"""

import sys
import os
import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from decode_bridge import (
    DecodeBridge, MODBUS_EXCEPTION, MODBUS_INVALID,
    MODBUS_RESPONSE, MODBUS_REQUEST,
)
from frame_builder import build_exception_response, build_read_response


class TestIllegalDataAddress:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-160
    @scenario Meter returns exception for unsupported register
    """

    def test_exception_code_02(self, bridge):
        """@given a Modbus exception response with code 0x02
        @when the frame is processed by modbus_decode()
        @then the exception is detected"""
        frame_bytes = build_exception_response(
            address=1, function_code=0x04, exception_code=0x02
        )
        frame = bridge.decode_frame(frame_bytes)
        assert frame.Type == MODBUS_EXCEPTION
        assert frame.Exception == 0x02
        assert frame.Address == 1
        assert frame.Function == (0x04 | 0x80)


class TestIllegalFunction:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-161
    @scenario Wrong function code rejected by meter
    """

    def test_exception_code_01(self, bridge):
        """@given a Modbus exception response with code 0x01
        @when the frame is processed by modbus_decode()
        @then the exception is detected"""
        frame_bytes = build_exception_response(
            address=1, function_code=0x03, exception_code=0x01
        )
        frame = bridge.decode_frame(frame_bytes)
        assert frame.Type == MODBUS_EXCEPTION
        assert frame.Exception == 0x01


class TestCRCError:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-162
    @scenario Corrupted frame on RS485 bus

    Note: The SmartEVSE firmware strips CRC before calling ModbusDecode(),
    so CRC validation happens at the transport layer. This test verifies
    that invalid frames (wrong byte count) are rejected.
    """

    def test_mismatched_byte_count(self, bridge):
        """@given a response frame with incorrect byte count field
        @when processed by modbus_decode()
        @then the frame is rejected (MODBUS_INVALID)"""
        # Build a response with byte_count=8 but only 4 bytes of data
        bad_frame = bytes([1, 0x04, 8, 0x41, 0x80, 0x00, 0x00])
        frame = bridge.decode_frame(bad_frame)
        assert frame.Type == MODBUS_INVALID


class TestTruncatedFrame:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-163
    @scenario Incomplete frame received (communication interrupted)
    """

    def test_too_short_frame(self, bridge):
        """@given a frame that is only 2 bytes (missing payload)
        @when processed by modbus_decode()
        @then the frame is rejected without crash"""
        short_frame = bytes([1, 0x04])
        frame = bridge.decode_frame(short_frame)
        assert frame.Type == MODBUS_INVALID

    def test_truncated_response(self, bridge):
        """@given a response frame missing the last 2 data bytes
        @when processed by modbus_decode()
        @then the frame is rejected (byte count mismatch)"""
        # Expected: addr + fc + bytecount(4) + 4 data bytes = 7
        # Truncated: addr + fc + bytecount(4) + 2 data bytes = 5
        truncated = bytes([1, 0x04, 4, 0x41, 0x80])
        frame = bridge.decode_frame(truncated)
        assert frame.Type == MODBUS_INVALID

    def test_empty_frame(self, bridge):
        """@given an empty buffer
        @when processed by modbus_decode()
        @then no crash occurs"""
        # modbus_decode checks for NULL but Python ctypes sends a valid pointer
        # with length 0; the function checks len < 5
        empty = bytes([])
        # The function expects at least the buf pointer to be valid
        # With 0 length it should return without processing
        frame = bridge.decode_frame(empty)
        assert frame.Type == MODBUS_INVALID

    def test_single_byte(self, bridge):
        """@given a frame of only 1 byte
        @when processed
        @then rejected without buffer overflow"""
        frame = bridge.decode_frame(bytes([1]))
        assert frame.Type == MODBUS_INVALID
