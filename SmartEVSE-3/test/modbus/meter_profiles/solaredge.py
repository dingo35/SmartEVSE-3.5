"""SolarEdge SunSpec energy meter profile.

EMConfig[] index 7 (EM_SOLAREDGE). INT16, FC3.
Uses decimal register addresses (SunSpec convention).
"""

METER_TYPE = 7
METER_NAME = "SolarEdge SunSpec"
ENDIANNESS = "HBF_HWF"
FUNCTION_CODE = 3
DATA_TYPE = "INT16"

# From EMConfig[7]: IRegister=40191, IDivisor=0
#                   PRegister=40206, PDivisor=0
#                   ERegister=40234, EDivisor=3, ERegister_Exp=40226, EDivisor_Exp=3
REGISTERS = {
    'current_l1':    40191,
    'current_l2':    40192,  # +1 for INT16
    'current_l3':    40193,  # +2
    'power_total':   40206,
    'energy_import': 40234,
    'energy_export': 40226,
}

I_DIVISOR = 0    # 0.1A by scale factor (handled externally)
P_DIVISOR = 0    # 1W
E_DIVISOR = 3    # Wh -> divide by 1000
E_DIVISOR_EXP = 3

IS_3PHASE = True

TEST_VECTORS = [
    {
        'name': 'nominal_3phase_16A',
        'description': '3-phase 16A',
        'current_values': [16, 16, 16],
        'power_value': 11040,
        'energy_import_value': None,
        'energy_export_value': None,
        'expected_current_mA': [16000, 16000, 16000],
        'expected_power_W': 11040,
        'expected_energy_import_Wh': None,
        'expected_energy_export_Wh': None,
    },
    {
        'name': 'low_current',
        'description': '3-phase 6A',
        'current_values': [6, 6, 6],
        'power_value': 4140,
        'energy_import_value': None,
        'energy_export_value': None,
        'expected_current_mA': [6000, 6000, 6000],
        'expected_power_W': 4140,
        'expected_energy_import_Wh': None,
        'expected_energy_export_Wh': None,
    },
    {
        'name': 'zero',
        'description': 'No load',
        'current_values': [0, 0, 0],
        'power_value': 0,
        'energy_import_value': None,
        'energy_export_value': None,
        'expected_current_mA': [0, 0, 0],
        'expected_power_W': 0,
        'expected_energy_import_Wh': None,
        'expected_energy_export_Wh': None,
    },
]
