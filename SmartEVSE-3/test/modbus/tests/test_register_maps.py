"""
Register map validation tests: verify EMConfig[] entries match meter datasheets.

Tests REQ-MTR-100 through REQ-MTR-102.

@feature Modbus Compatibility
"""

import sys
import os
import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from meter_profiles import (
    eastron_sdm630, eastron_sdm120, sensorbox_v2,
    abb_b23, finder_7e, phoenix_contact, sinotimer,
    solaredge, wago, schneider, finder_7m,
    chint, carlo_gavazzi, orno_3p, orno_1p, custom,
)


class TestEastronSDM630RegisterMap:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-100
    @scenario Eastron SDM630 register addresses match datasheet
    """

    def test_current_register(self):
        """@given EMConfig[] entry for meter type 4 (Eastron3P)
        @when the register address is checked
        @then IRegister 0x0006 matches 'Phase 1 current' (Input Register 30007)"""
        assert eastron_sdm630.REGISTERS['current_l1'] == 0x0006
        assert eastron_sdm630.FUNCTION_CODE == 4  # Input registers

    def test_power_register(self):
        """@then PRegister 0x0034 matches 'Total system power' (Input Register 30053)"""
        assert eastron_sdm630.REGISTERS['power_total'] == 0x0034

    def test_energy_register(self):
        """@then ERegister 0x0048 matches 'Total import kWh' (Input Register 30073)"""
        assert eastron_sdm630.REGISTERS['energy_import'] == 0x0048

    def test_data_type(self):
        """@then data type is FLOAT32 (IEEE 754)"""
        assert eastron_sdm630.DATA_TYPE == 'FLOAT32'

    def test_endianness(self):
        """@then endianness is HBF_HWF (big-endian)"""
        assert eastron_sdm630.ENDIANNESS == 'HBF_HWF'


class TestABBB23RegisterMap:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-101
    @scenario ABB B23 register addresses match datasheet
    """

    def test_current_register(self):
        """@given EMConfig[] entry for meter type 6 (ABB)
        @then IRegister 0x5B0C matches phase currents in holding registers"""
        assert abb_b23.REGISTERS['current_l1'] == 0x5B0C

    def test_function_code(self):
        """@then Function code is 3 (holding registers, not input registers)"""
        assert abb_b23.FUNCTION_CODE == 3

    def test_data_type(self):
        """@then data type is INT32"""
        assert abb_b23.DATA_TYPE == 'INT32'


# All 3-phase meter profiles to test phase offset consistency
THREE_PHASE_PROFILES = [
    eastron_sdm630, abb_b23, finder_7e, phoenix_contact,
    wago, schneider, finder_7m, chint, carlo_gavazzi, orno_3p,
]

PROFILE_IDS_3P = [p.METER_NAME for p in THREE_PHASE_PROFILES]


class TestPhaseOffsetConsistency:
    """
    @feature Modbus Compatibility
    @req REQ-MTR-102
    @scenario Phase register offsets are consistent
    """

    @pytest.mark.parametrize("profile", THREE_PHASE_PROFILES, ids=PROFILE_IDS_3P)
    def test_phase_offset_plus_2(self, profile):
        """@given all 3-phase meter types in EMConfig[]
        @when current register addresses for L1, L2, L3 are examined
        @then L2 = IRegister + 2 and L3 = IRegister + 4"""
        l1 = profile.REGISTERS['current_l1']
        l2 = profile.REGISTERS['current_l2']
        l3 = profile.REGISTERS['current_l3']

        if profile.DATA_TYPE == 'INT16':
            # INT16: single register per phase, offset by 1
            assert l2 == l1 + 1, \
                f"{profile.METER_NAME}: L2={l2}, expected {l1 + 1}"
            assert l3 == l1 + 2, \
                f"{profile.METER_NAME}: L3={l3}, expected {l1 + 2}"
        else:
            # FLOAT32/INT32: two registers per phase, offset by 2
            assert l2 == l1 + 2, \
                f"{profile.METER_NAME}: L2={l2}, expected {l1 + 2}"
            assert l3 == l1 + 4, \
                f"{profile.METER_NAME}: L3={l3}, expected {l1 + 4}"


class TestAllProfilesHaveTestVectors:
    """Verify every meter profile has at least 2 test vectors."""

    ALL_PROFILES = [
        eastron_sdm630, eastron_sdm120, sensorbox_v2,
        abb_b23, finder_7e, phoenix_contact, sinotimer,
        solaredge, wago, schneider, finder_7m,
        chint, carlo_gavazzi, orno_3p, orno_1p, custom,
    ]

    @pytest.mark.parametrize("profile",
                             ALL_PROFILES,
                             ids=[p.METER_NAME for p in ALL_PROFILES])
    def test_has_test_vectors(self, profile):
        """Each meter profile must have at least 2 test vectors."""
        assert len(profile.TEST_VECTORS) >= 2, \
            f"{profile.METER_NAME} has only {len(profile.TEST_VECTORS)} test vectors"
