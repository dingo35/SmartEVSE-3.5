"""Sensorbox v2 meter profile.

The Sensorbox uses CT sensors and reports current only (no power/energy).
EMConfig[] index 1 (EM_SENSORBOX).
Note: Power and energy registers are 0xFFFF (not available).
"""

METER_TYPE = 1
METER_NAME = "Sensorbox v2"
ENDIANNESS = "HBF_HWF"
FUNCTION_CODE = 4
DATA_TYPE = "FLOAT32"

# From EMConfig[1]: IRegister=0x0000, IDivisor=0
#                   PRegister=0xFFFF (N/A), ERegister=0xFFFF (N/A)
REGISTERS = {
    'current_l1': 0x0000,
    'current_l2': 0x0002,  # IRegister + 2
    'current_l3': 0x0004,  # IRegister + 4
}

I_DIVISOR = 0
P_DIVISOR = 0
E_DIVISOR = 0
E_DIVISOR_EXP = 0

IS_3PHASE = True
CURRENT_ONLY = True  # No power/energy registers

TEST_VECTORS = [
    {
        'name': 'nominal_3phase_16A',
        'description': '3-phase 16A CT measurement',
        'current_values': [16.0, 16.0, 16.0],
        'power_value': None,
        'energy_import_value': None,
        'energy_export_value': None,
        'expected_current_mA': [16000, 16000, 16000],
        'expected_power_W': None,
        'expected_energy_import_Wh': None,
        'expected_energy_export_Wh': None,
    },
    {
        'name': 'unbalanced_load',
        'description': 'Unbalanced 3-phase load',
        'current_values': [25.0, 10.0, 5.0],
        'power_value': None,
        'energy_import_value': None,
        'energy_export_value': None,
        'expected_current_mA': [25000, 10000, 5000],
        'expected_power_W': None,
        'expected_energy_import_Wh': None,
        'expected_energy_export_Wh': None,
    },
    {
        'name': 'zero_current',
        'description': 'No load',
        'current_values': [0.0, 0.0, 0.0],
        'power_value': None,
        'energy_import_value': None,
        'energy_export_value': None,
        'expected_current_mA': [0, 0, 0],
        'expected_power_W': None,
        'expected_energy_import_Wh': None,
        'expected_energy_export_Wh': None,
    },
]
