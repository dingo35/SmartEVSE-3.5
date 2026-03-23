"""Finder 7E.78.8.400.0212 energy meter profile.

EMConfig[] index 3 (EM_FINDER_7E). FLOAT32, FC4.
"""

METER_TYPE = 3
METER_NAME = "Finder 7E.78.8.400.0212"
ENDIANNESS = "HBF_HWF"
FUNCTION_CODE = 4
DATA_TYPE = "FLOAT32"

# From EMConfig[3]: IRegister=0x100E, IDivisor=0
#                   PRegister=0x1026, PDivisor=0
#                   ERegister=0x1106, EDivisor=3, ERegister_Exp=0x110E, EDivisor_Exp=3
REGISTERS = {
    'current_l1':    0x100E,
    'current_l2':    0x1010,  # +2
    'current_l3':    0x1012,  # +4
    'power_total':   0x1026,
    'energy_import': 0x1106,
    'energy_export': 0x110E,
}

I_DIVISOR = 0    # Current in Amps
P_DIVISOR = 0    # Power in Watts
E_DIVISOR = 3    # Energy in Wh (divide by 1000 to get kWh equivalent in firmware)
E_DIVISOR_EXP = 3

IS_3PHASE = True

TEST_VECTORS = [
    {
        'name': 'nominal_3phase_16A',
        'description': '3-phase 16A balanced load',
        'current_values': [16.0, 16.0, 16.0],
        'power_value': 11040.0,
        'energy_import_value': 1234567.0,  # Wh
        'energy_export_value': 0.0,
        'expected_current_mA': [16000, 16000, 16000],
        'expected_power_W': 11040,
        'expected_energy_import_Wh': 1234,  # 1234567/1000 = 1234
        'expected_energy_export_Wh': 0,
    },
    {
        'name': 'minimum_6A',
        'description': '3-phase 6A minimum charge',
        'current_values': [6.0, 6.0, 6.0],
        'power_value': 4140.0,
        'energy_import_value': 5000.0,
        'energy_export_value': 0.0,
        'expected_current_mA': [6000, 6000, 6000],
        'expected_power_W': 4140,
        'expected_energy_import_Wh': 5,
        'expected_energy_export_Wh': 0,
    },
    {
        'name': 'max_current_32A',
        'description': '3-phase 32A',
        'current_values': [32.0, 32.0, 32.0],
        'power_value': 22080.0,
        'energy_import_value': 99999000.0,
        'energy_export_value': 500000.0,
        'expected_current_mA': [32000, 32000, 32000],
        'expected_power_W': 22080,
        'expected_energy_import_Wh': 99999,
        'expected_energy_export_Wh': 500,
    },
]
