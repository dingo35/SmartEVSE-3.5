"""Finder 7M.38.8.400.0212 energy meter profile.

EMConfig[] index 11 (EM_FINDER_7M). FLOAT32, FC4.
Uses decimal register addresses.
"""

METER_TYPE = 11
METER_NAME = "Finder 7M.38.8.400.0212"
ENDIANNESS = "HBF_HWF"
FUNCTION_CODE = 4
DATA_TYPE = "FLOAT32"

# From EMConfig[11]: IRegister=2516, IDivisor=0
#                    PRegister=2536, PDivisor=0
#                    ERegister=2638, EDivisor=3, ERegister_Exp=0
REGISTERS = {
    'current_l1':    2516,
    'current_l2':    2518,  # +2
    'current_l3':    2520,  # +4
    'power_total':   2536,
    'energy_import': 2638,
}

I_DIVISOR = 0
P_DIVISOR = 0
E_DIVISOR = 3    # Wh -> divide by 1000
E_DIVISOR_EXP = 0

IS_3PHASE = True

TEST_VECTORS = [
    {
        'name': 'nominal_3phase_16A',
        'description': '3-phase 16A',
        'current_values': [16.0, 16.0, 16.0],
        'power_value': 11040.0,
        'energy_import_value': 1234567.0,  # Wh
        'energy_export_value': None,
        'expected_current_mA': [16000, 16000, 16000],
        'expected_power_W': 11040,
        'expected_energy_import_Wh': 1234,
        'expected_energy_export_Wh': None,
    },
    {
        'name': 'low_current',
        'description': '3-phase 6A',
        'current_values': [6.0, 6.0, 6.0],
        'power_value': 4140.0,
        'energy_import_value': 50000.0,
        'energy_export_value': None,
        'expected_current_mA': [6000, 6000, 6000],
        'expected_power_W': 4140,
        'expected_energy_import_Wh': 50,
        'expected_energy_export_Wh': None,
    },
    {
        'name': 'zero',
        'description': 'No load',
        'current_values': [0.0, 0.0, 0.0],
        'power_value': 0.0,
        'energy_import_value': 0.0,
        'energy_export_value': None,
        'expected_current_mA': [0, 0, 0],
        'expected_power_W': 0,
        'expected_energy_import_Wh': 0,
        'expected_energy_export_Wh': None,
    },
]
