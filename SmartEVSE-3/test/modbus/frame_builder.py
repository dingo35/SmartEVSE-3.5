"""
Modbus RTU frame builder for compatibility testing.

Generates Modbus RTU response frames from register values,
matching what real energy meters would send on the RS485 bus.
The SmartEVSE firmware strips CRC before calling ModbusDecode(),
so frames here are built WITHOUT CRC (matching the decode input).
"""

import struct
from typing import List, Union


def float32_to_bytes(value: float) -> bytes:
    """Encode a float as IEEE 754 big-endian (network byte order)."""
    return struct.pack('>f', value)


def int32_to_bytes(value: int) -> bytes:
    """Encode a signed 32-bit integer as big-endian."""
    return struct.pack('>i', value)


def int16_to_bytes(value: int) -> bytes:
    """Encode a signed 16-bit integer as big-endian."""
    return struct.pack('>h', value)


def uint16_to_bytes(value: int) -> bytes:
    """Encode an unsigned 16-bit integer as big-endian."""
    return struct.pack('>H', value)


def encode_register_value(value: Union[float, int], datatype: str) -> bytes:
    """Encode a value as Modbus register bytes (big-endian wire format).

    Args:
        value: The physical value to encode.
        datatype: One of 'FLOAT32', 'INT32', 'INT16'.

    Returns:
        Bytes in big-endian (high byte first, high word first) order.
        For FLOAT32/INT32: 4 bytes (2 registers).
        For INT16: 2 bytes (1 register).
    """
    if datatype == 'FLOAT32':
        return float32_to_bytes(float(value))
    elif datatype == 'INT32':
        return int32_to_bytes(int(value))
    elif datatype == 'INT16':
        return int16_to_bytes(int(value))
    else:
        raise ValueError(f"Unknown datatype: {datatype}")


def reorder_bytes(data: bytes, endianness: str, datatype: str) -> bytes:
    """Reorder bytes from canonical big-endian to the meter's wire format.

    The canonical form is HBF_HWF (big-endian). This function converts to
    the target endianness as the meter would send it on the wire.

    Args:
        data: Bytes in HBF_HWF (big-endian) order.
        endianness: Target endianness ('HBF_HWF', 'HBF_LWF', 'LBF_LWF', 'LBF_HWF').
        datatype: 'FLOAT32', 'INT32', or 'INT16'.

    Returns:
        Bytes in the meter's wire format.
    """
    if datatype == 'INT16' or len(data) == 2:
        # For 16-bit values, only byte order within the word matters
        if endianness in ('LBF_LWF', 'LBF_HWF'):
            return bytes([data[1], data[0]])
        else:
            return data

    # 32-bit values: data is [HB_HW, LB_HW, HB_LW, LB_LW] in big-endian
    b0, b1, b2, b3 = data[0], data[1], data[2], data[3]

    if endianness == 'HBF_HWF':
        # Big endian: no change
        return bytes([b0, b1, b2, b3])
    elif endianness == 'HBF_LWF':
        # High byte first, low word first: swap words
        return bytes([b2, b3, b0, b1])
    elif endianness == 'LBF_LWF':
        # Little endian: reverse all
        return bytes([b3, b2, b1, b0])
    elif endianness == 'LBF_HWF':
        # Low byte first, high word first: swap bytes in each word
        return bytes([b1, b0, b3, b2])
    else:
        raise ValueError(f"Unknown endianness: {endianness}")


def build_response_data(values: List[Union[float, int]], datatype: str,
                        endianness: str) -> bytes:
    """Build the data portion of a Modbus response from a list of values.

    Args:
        values: List of register values in order.
        datatype: 'FLOAT32', 'INT32', or 'INT16'.
        endianness: Meter endianness.

    Returns:
        Concatenated register data bytes in the meter's wire format.
    """
    result = b''
    for v in values:
        raw = encode_register_value(v, datatype)
        reordered = reorder_bytes(raw, endianness, datatype)
        result += reordered
    return result


def build_read_response(address: int, function_code: int,
                        data: bytes) -> bytes:
    """Build a complete Modbus read response frame (no CRC).

    Format: [address] [function] [byte_count] [data...]

    Args:
        address: Slave address (1-247).
        function_code: 3 (holding) or 4 (input).
        data: Register data bytes.

    Returns:
        Complete response frame without CRC.
    """
    byte_count = len(data)
    return bytes([address, function_code, byte_count]) + data


def build_read_request(address: int, function_code: int,
                       start_register: int, count: int) -> bytes:
    """Build a Modbus read request frame (no CRC).

    Format: [address] [function] [reg_hi] [reg_lo] [count_hi] [count_lo]

    Args:
        address: Slave address.
        function_code: 3 or 4.
        start_register: Starting register address.
        count: Number of registers to read.

    Returns:
        Complete request frame without CRC.
    """
    return bytes([
        address,
        function_code,
        (start_register >> 8) & 0xFF,
        start_register & 0xFF,
        (count >> 8) & 0xFF,
        count & 0xFF,
    ])


def build_exception_response(address: int, function_code: int,
                              exception_code: int) -> bytes:
    """Build a Modbus exception response frame (no CRC).

    Format: [address] [function | 0x80] [exception_code]

    Args:
        address: Slave address.
        function_code: Original function code.
        exception_code: Exception code (1=illegal function, 2=illegal address).

    Returns:
        3-byte exception response without CRC.
    """
    return bytes([address, function_code | 0x80, exception_code])
