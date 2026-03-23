"""
Full decode pipeline tests: frame -> ModbusDecode -> meter_decode_value.

Tests REQ-MTR-110 through REQ-MTR-115.

@feature Modbus Compatibility
"""

import sys
import os
import struct
import pytest

# Add parent directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from decode_bridge import DecodeBridge, MODBUS_RESPONSE, MODBUS_REQUEST
from frame_builder import (
    build_response_data, build_read_response, build_read_request,
    encode_register_value, reorder_bytes,
)
from meter_profiles import (
    eastron_sdm630, eastron_sdm120, sensorbox_v2,
    abb_b23, finder_7e, phoenix_contact, sinotimer,
    solaredge, wago, schneider, finder_7m,
    chint, carlo_gavazzi, orno_3p, orno_1p, custom,
)


def decode_current_pipeline(bridge, profile, current_values):
    """Run the full current decode pipeline for a meter profile.

    Simulates what the firmware does:
    1. Encode current values in meter's wire format
    2. Call meter_decode_value with IDivisor - 3 (firmware convention)

    Returns list of decoded current values in mA.
    """
    data = build_response_data(current_values, profile.DATA_TYPE, profile.ENDIANNESS)
    i_divisor = profile.I_DIVISOR - 3  # Firmware applies -3 to convert A -> mA

    results = []
    for i in range(len(current_values)):
        r = bridge.decode_value(data, i, profile.ENDIANNESS, profile.DATA_TYPE, i_divisor)
        results.append(r)
    return results


def decode_power_pipeline(bridge, profile, power_value):
    """Decode power using the firmware's convention (PDivisor directly)."""
    data = build_response_data([power_value], profile.DATA_TYPE, profile.ENDIANNESS)
    return bridge.decode_value(data, 0, profile.ENDIANNESS, profile.DATA_TYPE, profile.P_DIVISOR)


def decode_energy_pipeline(bridge, profile, energy_value, datatype_override=None):
    """Decode energy using firmware's convention (EDivisor - 3 for kWh->Wh)."""
    dt = datatype_override or profile.DATA_TYPE
    data = build_response_data([energy_value], dt, profile.ENDIANNESS)
    e_divisor = profile.E_DIVISOR - 3
    return bridge.decode_value(data, 0, profile.ENDIANNESS, dt, e_divisor)


class TestEastronSDM630:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-110
    @scenario Complete Eastron SDM630 decode pipeline for balanced load
    """

    def test_nominal_3phase_16A(self, bridge):
        """@given Eastron SDM630 with 16A on all phases
        @when decoded through the full pipeline
        @then Irms = 16000 mA per phase"""
        tv = eastron_sdm630.TEST_VECTORS[0]
        results = decode_current_pipeline(bridge, eastron_sdm630, tv['current_values'])
        for i, r in enumerate(results):
            assert r.valid == 1
            assert r.value == tv['expected_current_mA'][i], \
                f"Phase {i}: expected {tv['expected_current_mA'][i]}, got {r.value}"

    def test_nominal_power(self, bridge):
        """@given Eastron SDM630 with 11040W total power
        @when decoded
        @then power = 11040 W"""
        tv = eastron_sdm630.TEST_VECTORS[0]
        r = decode_power_pipeline(bridge, eastron_sdm630, tv['power_value'])
        assert r.valid == 1
        assert r.value == tv['expected_power_W']

    def test_nominal_energy(self, bridge):
        """@given Eastron SDM630 with 1234.567 kWh import energy
        @when decoded
        @then energy = 1234567 Wh"""
        tv = eastron_sdm630.TEST_VECTORS[0]
        r = decode_energy_pipeline(bridge, eastron_sdm630, tv['energy_import_value'])
        assert r.valid == 1
        assert r.value == tv['expected_energy_import_Wh']

    def test_single_phase_load(self, bridge):
        """@given Eastron SDM630 with 6A on L1 only
        @when decoded
        @then L1=6000mA, L2=0, L3=0"""
        tv = eastron_sdm630.TEST_VECTORS[1]
        results = decode_current_pipeline(bridge, eastron_sdm630, tv['current_values'])
        for i, r in enumerate(results):
            assert r.valid == 1
            assert r.value == tv['expected_current_mA'][i]

    def test_max_current_32A(self, bridge):
        """@given Eastron SDM630 with 32A on all phases
        @when decoded
        @then Irms = 32000 mA per phase"""
        tv = eastron_sdm630.TEST_VECTORS[2]
        results = decode_current_pipeline(bridge, eastron_sdm630, tv['current_values'])
        for i, r in enumerate(results):
            assert r.valid == 1
            assert r.value == tv['expected_current_mA'][i]


class TestEastronSDM120:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-111
    @scenario Eastron SDM120 single-phase current decode
    """

    def test_nominal_1phase_16A(self, bridge):
        """@given Eastron SDM120 with 16A
        @when decoded
        @then L1=16000mA"""
        tv = eastron_sdm120.TEST_VECTORS[0]
        results = decode_current_pipeline(bridge, eastron_sdm120, tv['current_values'])
        assert results[0].valid == 1
        assert results[0].value == tv['expected_current_mA'][0]

    def test_standby_current(self, bridge):
        """@given Eastron SDM120 with 0.05A standby
        @when decoded
        @then L1=50mA (not rounded to zero)"""
        tv = eastron_sdm120.TEST_VECTORS[2]
        results = decode_current_pipeline(bridge, eastron_sdm120, tv['current_values'])
        assert results[0].valid == 1
        assert results[0].value == tv['expected_current_mA'][0]


class TestABBB23:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-112
    @scenario ABB B23 holding register read with INT32 values
    """

    def test_nominal_3phase_16A(self, bridge):
        """@given ABB B23 with INT32 values for 16A (0.01A units)
        @when decoded with FC=3 pipeline
        @then Irms = 16000 mA per phase"""
        tv = abb_b23.TEST_VECTORS[0]
        results = decode_current_pipeline(bridge, abb_b23, tv['current_values'])
        for i, r in enumerate(results):
            assert r.valid == 1
            assert r.value == tv['expected_current_mA'][i], \
                f"Phase {i}: expected {tv['expected_current_mA'][i]}, got {r.value}"

    def test_power(self, bridge):
        """@given ABB B23 with 11040W power (0.01W units)
        @when decoded
        @then power = 11040 W"""
        tv = abb_b23.TEST_VECTORS[0]
        r = decode_power_pipeline(bridge, abb_b23, tv['power_value'])
        assert r.valid == 1
        assert r.value == tv['expected_power_W']


class TestPhoenixContact:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-113
    @scenario Phoenix Contact INT32 decode with HBF_LWF endianness
    """

    def test_nominal_3phase_16A(self, bridge):
        """@given Phoenix Contact with current in mA, HBF_LWF endianness
        @when decoded
        @then Irms = 16000 mA per phase"""
        tv = phoenix_contact.TEST_VECTORS[0]
        results = decode_current_pipeline(bridge, phoenix_contact, tv['current_values'])
        for i, r in enumerate(results):
            assert r.valid == 1
            assert r.value == tv['expected_current_mA'][i]

    def test_power(self, bridge):
        """@given Phoenix Contact with 0.1W power units
        @when decoded
        @then power = 11040 W"""
        tv = phoenix_contact.TEST_VECTORS[0]
        r = decode_power_pipeline(bridge, phoenix_contact, tv['power_value'])
        assert r.valid == 1
        assert r.value == tv['expected_power_W']


class TestSinotimer:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-114
    @scenario Sinotimer 16-bit register decode
    """

    def test_nominal_3phase_16A(self, bridge):
        """@given Sinotimer with INT16 values for 16A (0.01A units)
        @when decoded
        @then Irms = 16000 mA per phase"""
        tv = sinotimer.TEST_VECTORS[0]
        results = decode_current_pipeline(bridge, sinotimer, tv['current_values'])
        for i, r in enumerate(results):
            assert r.valid == 1
            assert r.value == tv['expected_current_mA'][i]

    def test_power(self, bridge):
        """@given Sinotimer with 1W resolution power
        @when decoded
        @then power = 11040 W"""
        tv = sinotimer.TEST_VECTORS[0]
        r = decode_power_pipeline(bridge, sinotimer, tv['power_value'])
        assert r.valid == 1
        assert r.value == tv['expected_power_W']


# Parameterized test across all meter types that support current
ALL_CURRENT_PROFILES = [
    eastron_sdm630, eastron_sdm120, abb_b23, finder_7e,
    phoenix_contact, sinotimer, wago, schneider, finder_7m,
    chint, carlo_gavazzi, orno_3p, orno_1p, custom,
]

PROFILE_IDS = [p.METER_NAME for p in ALL_CURRENT_PROFILES]


class TestParameterizedDecode:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-115
    @scenario Parameterized test across all active meter types
    """

    @pytest.mark.parametrize("profile", ALL_CURRENT_PROFILES, ids=PROFILE_IDS)
    def test_first_vector_current(self, bridge, profile):
        """@given test vectors for each meter type
        @when the decode pipeline runs
        @then decoded current matches expected values"""
        tv = profile.TEST_VECTORS[0]
        results = decode_current_pipeline(bridge, profile, tv['current_values'])
        for i, r in enumerate(results):
            assert r.valid == 1, f"{profile.METER_NAME} phase {i}: decode failed"
            assert r.value == tv['expected_current_mA'][i], \
                f"{profile.METER_NAME} phase {i}: " \
                f"expected {tv['expected_current_mA'][i]}, got {r.value}"

    @pytest.mark.parametrize("profile", ALL_CURRENT_PROFILES, ids=PROFILE_IDS)
    def test_first_vector_power(self, bridge, profile):
        """@given test vectors for each meter type
        @when the decode pipeline runs
        @then decoded power matches expected value"""
        tv = profile.TEST_VECTORS[0]
        if tv.get('power_value') is None:
            pytest.skip("No power test vector")
        r = decode_power_pipeline(bridge, profile, tv['power_value'])
        assert r.valid == 1, f"{profile.METER_NAME}: power decode failed"
        assert r.value == tv['expected_power_W'], \
            f"{profile.METER_NAME}: expected {tv['expected_power_W']}, got {r.value}"
