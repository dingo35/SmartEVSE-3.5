"""
C-to-Python bridge for SmartEVSE Modbus decode functions.

Loads the compiled shared library (libmodbus_decode.so/.dylib) via ctypes
and provides Python-friendly wrappers for modbus_decode() and
meter_decode_value().
"""

import ctypes
import os
import sys
from pathlib import Path
from typing import Optional


# Match C enum/defines from meter_decode.h
ENDIANNESS_LBF_LWF = 0
ENDIANNESS_LBF_HWF = 1
ENDIANNESS_HBF_LWF = 2
ENDIANNESS_HBF_HWF = 3

METER_DATATYPE_INT32 = 0
METER_DATATYPE_FLOAT32 = 1
METER_DATATYPE_INT16 = 2

# Match C defines from modbus_decode.h
MODBUS_INVALID = 0
MODBUS_OK = 1
MODBUS_REQUEST = 2
MODBUS_RESPONSE = 3
MODBUS_EXCEPTION = 4

# Endianness name to constant mapping
ENDIANNESS_MAP = {
    'LBF_LWF': ENDIANNESS_LBF_LWF,
    'LBF_HWF': ENDIANNESS_LBF_HWF,
    'HBF_LWF': ENDIANNESS_HBF_LWF,
    'HBF_HWF': ENDIANNESS_HBF_HWF,
}

DATATYPE_MAP = {
    'INT32': METER_DATATYPE_INT32,
    'FLOAT32': METER_DATATYPE_FLOAT32,
    'INT16': METER_DATATYPE_INT16,
}


class ModbusFrame(ctypes.Structure):
    """Mirrors modbus_frame_t from modbus_decode.h."""
    _fields_ = [
        ('Address', ctypes.c_uint8),
        ('Function', ctypes.c_uint8),
        ('Register', ctypes.c_uint16),
        ('RegisterCount', ctypes.c_uint16),
        ('Value', ctypes.c_uint16),
        ('Data', ctypes.POINTER(ctypes.c_uint8)),
        ('DataLength', ctypes.c_uint8),
        ('Type', ctypes.c_uint8),
        ('RequestAddress', ctypes.c_uint8),
        ('RequestFunction', ctypes.c_uint8),
        ('RequestRegister', ctypes.c_uint16),
        ('Exception', ctypes.c_uint8),
    ]


class MeterReading(ctypes.Structure):
    """Mirrors meter_reading_t from meter_decode.h."""
    _fields_ = [
        ('value', ctypes.c_int32),
        ('valid', ctypes.c_uint8),
    ]


def _find_library() -> str:
    """Find the shared library, checking common locations."""
    base = Path(__file__).parent
    candidates = [
        base / 'libmodbus_decode.so',
        base / 'libmodbus_decode.dylib',
    ]
    for p in candidates:
        if p.exists():
            return str(p)
    raise FileNotFoundError(
        f"Shared library not found. Run 'make -f Makefile.native' in {base}. "
        f"Checked: {[str(c) for c in candidates]}"
    )


class DecodeBridge:
    """Wraps SmartEVSE C decode functions for Python test access."""

    def __init__(self, lib_path: Optional[str] = None):
        if lib_path is None:
            lib_path = _find_library()
        self.lib = ctypes.CDLL(lib_path)
        self._bind_functions()

    def _bind_functions(self):
        # void modbus_frame_init(modbus_frame_t *frame)
        self.lib.modbus_frame_init.argtypes = [ctypes.POINTER(ModbusFrame)]
        self.lib.modbus_frame_init.restype = None

        # void modbus_decode(modbus_frame_t *frame, const uint8_t *buf, uint8_t len)
        self.lib.modbus_decode.argtypes = [
            ctypes.POINTER(ModbusFrame),
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_uint8,
        ]
        self.lib.modbus_decode.restype = None

        # meter_reading_t meter_decode_value(const uint8_t *buf, uint8_t index,
        #     uint8_t endianness, meter_datatype_t datatype, int8_t divisor)
        self.lib.meter_decode_value.argtypes = [
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_uint8,
            ctypes.c_uint8,
            ctypes.c_int,  # enum
            ctypes.c_int8,
        ]
        self.lib.meter_decode_value.restype = MeterReading

        # uint8_t meter_register_size(meter_datatype_t datatype)
        self.lib.meter_register_size.argtypes = [ctypes.c_int]
        self.lib.meter_register_size.restype = ctypes.c_uint8

        # void meter_combine_bytes(void *out, const uint8_t *buf, uint8_t pos,
        #     uint8_t endianness, meter_datatype_t datatype)
        self.lib.meter_combine_bytes.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_uint8,
            ctypes.c_uint8,
            ctypes.c_int,
        ]
        self.lib.meter_combine_bytes.restype = None

    def new_frame(self) -> ModbusFrame:
        """Create and initialize a new ModbusFrame."""
        frame = ModbusFrame()
        self.lib.modbus_frame_init(ctypes.byref(frame))
        return frame

    def decode_frame(self, raw_bytes: bytes,
                     frame: Optional[ModbusFrame] = None) -> ModbusFrame:
        """Parse a raw Modbus frame (no CRC).

        Args:
            raw_bytes: The frame bytes (address + function + payload).
            frame: Optional existing frame for request/response matching.
                   If None, a fresh frame is created.

        Returns:
            Decoded ModbusFrame with Type, Address, Function, etc.
        """
        if frame is None:
            frame = self.new_frame()
        buf = (ctypes.c_uint8 * len(raw_bytes))(*raw_bytes)
        self.lib.modbus_decode(ctypes.byref(frame), buf, len(raw_bytes))
        return frame

    def decode_value(self, data: bytes, index: int,
                     endianness: str, datatype: str,
                     divisor: int) -> MeterReading:
        """Decode a single measurement value from response data.

        Args:
            data: Raw data bytes from the Modbus response.
            index: Register index (0-based).
            endianness: String name ('HBF_HWF', etc.).
            datatype: String name ('FLOAT32', 'INT32', 'INT16').
            divisor: Power-of-10 divisor (positive=divide, negative=multiply).

        Returns:
            MeterReading with .value and .valid fields.
        """
        buf = (ctypes.c_uint8 * len(data))(*data)
        end_val = ENDIANNESS_MAP[endianness]
        dt_val = DATATYPE_MAP[datatype]
        return self.lib.meter_decode_value(buf, index, end_val, dt_val, divisor)

    def decode_current_3phase(self, data: bytes, endianness: str,
                              datatype: str, divisor: int) -> list:
        """Decode 3-phase current from consecutive registers.

        Assumes L1 at index 0, L2 at index 1, L3 at index 2.

        Returns:
            List of 3 MeterReading results [L1, L2, L3].
        """
        results = []
        for i in range(3):
            r = self.decode_value(data, i, endianness, datatype, divisor)
            results.append(r)
        return results

    def register_size(self, datatype: str) -> int:
        """Get the byte size of a register for the given data type."""
        dt_val = DATATYPE_MAP[datatype]
        return self.lib.meter_register_size(dt_val)
