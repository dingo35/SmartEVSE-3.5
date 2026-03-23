"""Chint DTSU666 energy meter profile.

EMConfig[] index 15 (EM_CHINT). FLOAT32, FC3.
"""

METER_TYPE = 15
METER_NAME = "Chint DTSU666"
ENDIANNESS = "HBF_HWF"
FUNCTION_CODE = 3
DATA_TYPE = "FLOAT32"

# From EMConfig[15]: IRegister=0x200C, IDivisor=3 (mA)
#                    PRegister=0x2012, PDivisor=1 (0.1W)
#                    ERegister=0x101E, EDivisor=0, ERegister_Exp=0x1028
REGISTERS = {
    'current_l1':    0x200C,
    'current_l2':    0x200E,  # +2
    'current_l3':    0x2010,  # +4
    'power_total':   0x2012,
    'energy_import': 0x101E,
    'energy_export': 0x1028,
}

I_DIVISOR = 3    # mA -> divide by 1000
P_DIVISOR = 1    # 0.1W -> divide by 10
E_DIVISOR = 0    # kWh
E_DIVISOR_EXP = 0

IS_3PHASE = True

TEST_VECTORS = [
    {
        'name': 'nominal_3phase_16A',
        'description': '3-phase 16A, current in mA',
        'current_values': [16000.0, 16000.0, 16000.0],  # mA
        'power_value': 110400.0,  # 0.1W units
        'energy_import_value': 1234.567,  # kWh
        'energy_export_value': 0.0,
        'expected_current_mA': [16000, 16000, 16000],
        'expected_power_W': 11040,
        'expected_energy_import_Wh': 1234567,
        'expected_energy_export_Wh': 0,
    },
    {
        'name': 'low_current',
        'description': '3-phase 6A',
        'current_values': [6000.0, 6000.0, 6000.0],
        'power_value': 41400.0,
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
