"""
pytest configuration and shared fixtures for Modbus compatibility tests.
"""

import pytest
from decode_bridge import DecodeBridge


@pytest.fixture(scope='session')
def bridge():
    """Shared DecodeBridge instance (loads C shared library once)."""
    return DecodeBridge()


@pytest.fixture(scope='session')
def all_meter_profiles():
    """Load all meter profile modules and return as a dict keyed by meter type index."""
    from meter_profiles import eastron_sdm630, eastron_sdm120, sensorbox_v2
    from meter_profiles import abb_b23, finder_7e, phoenix_contact, sinotimer
    from meter_profiles import solaredge, wago, schneider, finder_7m
    from meter_profiles import chint, carlo_gavazzi, custom, orno_3p, orno_1p

    profiles = {}
    for mod in [eastron_sdm630, eastron_sdm120, sensorbox_v2,
                abb_b23, finder_7e, phoenix_contact, sinotimer,
                solaredge, wago, schneider, finder_7m,
                chint, carlo_gavazzi, custom, orno_3p, orno_1p]:
        profiles[mod.METER_TYPE] = mod
    return profiles
