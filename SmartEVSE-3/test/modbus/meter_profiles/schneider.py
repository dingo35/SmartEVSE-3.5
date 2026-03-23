"""Schneider iEM3x5x series energy meter profile.

EMConfig[] index 14 (EM_SCHNEIDER). FLOAT32, FC3.
"""

METER_TYPE = 14
METER_NAME = "Schneider iEM3x5x"
ENDIANNESS = "HBF_HWF"
FUNCTION_CODE = 3
DATA_TYPE = "FLOAT32"

# From EMConfig[14]: IRegister=0x0BB7, IDivisor=0
#                    PRegister=0x0BF3, PDivisor=-3  (kW -> multiply by 1000)
#                    ERegister=0xB02B, EDivisor=0, ERegister_Exp=0xB02D
REGISTERS = {
    'current_l1':    0x0BB7,
    'current_l2':    0x0BB9,  # +2
    'current_l3':    0x0BBB,  # +4
    'power_total':   0x0BF3,
    'energy_import': 0xB02B,
    'energy_export': 0xB02D,
}

I_DIVISOR = 0
P_DIVISOR = -3   # kW -> multiply by 1000
E_DIVISOR = 0    # kWh
E_DIVISOR_EXP = 0

IS_3PHASE = True

TEST_VECTORS = [
    {
        'name': 'nominal_3phase_16A',
        'description': '3-phase 16A',
        'current_values': [16.0, 16.0, 16.0],
        'power_value': 11.04,  # kW
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
        'power_value': 4.14,
        'energy_import_value': 50.0,
        'energy_export_value': 0.0,
        'expected_current_mA': [6000, 6000, 6000],
        'expected_power_W': 4140,
        'expected_energy_import_Wh': 50000,
        'expected_energy_export_Wh': 0,
    },
    {
        'name': 'export_power',
        'description': 'Negative power (export)',
        'current_values': [10.0, 10.0, 10.0],
        'power_value': -5.0,  # kW export
        'energy_import_value': 1000.0,
        'energy_export_value': 200.0,
        'expected_current_mA': [10000, 10000, 10000],
        'expected_power_W': -5000,
        'expected_energy_import_Wh': 1000000,
        'expected_energy_export_Wh': 200000,
    },
]
