"""Custom meter profile (user-configurable registers).

EMConfig[] index 19 (EM_CUSTOM). LBF_LWF (little endian), INT32.
Default registers are all 0; user configures via settings.
"""

METER_TYPE = 19
METER_NAME = "Custom"
ENDIANNESS = "LBF_LWF"
FUNCTION_CODE = 4
DATA_TYPE = "INT32"

# From EMConfig[19]: all registers default to 0
# Users configure via the web interface/API
REGISTERS = {
    'current_l1':    0x0000,
    'current_l2':    0x0002,
    'current_l3':    0x0004,
    'power_total':   0x0000,
    'energy_import': 0x0000,
    'energy_export': 0x0000,
}

I_DIVISOR = 0
P_DIVISOR = 0
E_DIVISOR = 0
E_DIVISOR_EXP = 0

IS_3PHASE = True

# Custom meter test vectors use user-defined register addresses
TEST_VECTORS = [
    {
        'name': 'custom_registers',
        'description': 'Custom meter with user-defined register offsets',
        'current_values': [16, 16, 16],  # Amps (IDivisor=0, firmware multiplies by 1000)
        'power_value': 11040,
        'energy_import_value': 1234,  # kWh (EDivisor=0, firmware multiplies by 1000)
        'energy_export_value': 0,
        'expected_current_mA': [16000, 16000, 16000],
        'expected_power_W': 11040,
        'expected_energy_import_Wh': 1234000,
        'expected_energy_export_Wh': 0,
    },
    {
        'name': 'zero',
        'description': 'No load',
        'current_values': [0, 0, 0],
        'power_value': 0,
        'energy_import_value': 0,
        'energy_export_value': 0,
        'expected_current_mA': [0, 0, 0],
        'expected_power_W': 0,
        'expected_energy_import_Wh': 0,
        'expected_energy_export_Wh': 0,
    },
    {
        'name': 'negative_values',
        'description': 'Custom meter with negative power (export)',
        'current_values': [10, 10, 10],  # Amps
        'power_value': -5000,
        'energy_import_value': 100,  # kWh
        'energy_export_value': 50,   # kWh
        'expected_current_mA': [10000, 10000, 10000],
        'expected_power_W': -5000,
        'expected_energy_import_Wh': 100000,
        'expected_energy_export_Wh': 50000,
    },
]
