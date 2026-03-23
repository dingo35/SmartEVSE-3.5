"""WAGO 879-30x0 energy meter profile.

EMConfig[] index 8 (EM_WAGO). FLOAT32, FC3.
"""

METER_TYPE = 8
METER_NAME = "WAGO 879-30x0"
ENDIANNESS = "HBF_HWF"
FUNCTION_CODE = 3
DATA_TYPE = "FLOAT32"

# From EMConfig[8]: IRegister=0x500C, IDivisor=0
#                   PRegister=0x5012, PDivisor=-3  (kW, multiply by 1000)
#                   ERegister=0x600C, EDivisor=0, ERegister_Exp=0x6018
REGISTERS = {
    'current_l1':    0x500C,
    'current_l2':    0x500E,  # +2
    'current_l3':    0x5010,  # +4
    'power_total':   0x5012,
    'energy_import': 0x600C,
    'energy_export': 0x6018,
}

I_DIVISOR = 0     # Amps
P_DIVISOR = -3    # kW -> multiply by 1000 for W
E_DIVISOR = 0     # kWh
E_DIVISOR_EXP = 0

IS_3PHASE = True

TEST_VECTORS = [
    {
        'name': 'nominal_3phase_16A',
        'description': '3-phase 16A, power in kW',
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
        'description': '3-phase 6A, power in kW',
        'current_values': [6.0, 6.0, 6.0],
        'power_value': 4.14,
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
