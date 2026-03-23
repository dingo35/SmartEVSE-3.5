"""Orno OR-WE-517 3-phase energy meter profile.

EMConfig[] index 17 (EM_ORNO3P). FLOAT32, FC4.
"""

METER_TYPE = 17
METER_NAME = "Orno OR-WE-517 (3-phase)"
ENDIANNESS = "HBF_HWF"
FUNCTION_CODE = 4
DATA_TYPE = "FLOAT32"

# From EMConfig[17]: IRegister=0x000C, IDivisor=0
#                    PRegister=0x001C, PDivisor=0
#                    ERegister=0x0100, EDivisor=0, ERegister_Exp=0x0110
REGISTERS = {
    'current_l1':    0x000C,
    'current_l2':    0x000E,  # +2
    'current_l3':    0x0010,  # +4
    'power_total':   0x001C,
    'energy_import': 0x0100,
    'energy_export': 0x0110,
}

I_DIVISOR = 0
P_DIVISOR = 0
E_DIVISOR = 0    # kWh
E_DIVISOR_EXP = 0

IS_3PHASE = True

TEST_VECTORS = [
    {
        'name': 'nominal_3phase_16A',
        'description': '3-phase 16A',
        'current_values': [16.0, 16.0, 16.0],
        'power_value': 11040.0,
        'energy_import_value': 1234.567,
        'energy_export_value': 0.0,
        'expected_current_mA': [16000, 16000, 16000],
        'expected_power_W': 11040,
        'expected_energy_import_Wh': 1234567,
        'expected_energy_export_Wh': 0,
    },
    {
        'name': 'low_current',
        'description': '3-phase 6A',
        'current_values': [6.0, 6.0, 6.0],
        'power_value': 4140.0,
        'energy_import_value': 100.0,
        'energy_export_value': 0.0,
        'expected_current_mA': [6000, 6000, 6000],
        'expected_power_W': 4140,
        'expected_energy_import_Wh': 100000,
        'expected_energy_export_Wh': 0,
    },
    {
        'name': 'zero',
        'description': 'No load',
        'current_values': [0.0, 0.0, 0.0],
        'power_value': 0.0,
        'energy_import_value': 0.0,
        'energy_export_value': 0.0,
        'expected_current_mA': [0, 0, 0],
        'expected_power_W': 0,
        'expected_energy_import_Wh': 0,
        'expected_energy_export_Wh': 0,
    },
]
