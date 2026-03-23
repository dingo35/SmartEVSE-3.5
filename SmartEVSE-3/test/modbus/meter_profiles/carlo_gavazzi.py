"""Carlo Gavazzi EM340 energy meter profile.

EMConfig[] index 16 (EM_CARLO_CAVAZZI). Uses HBF_LWF endianness and INT32.
Note: The firmware spells this "CAVAZZI" but the manufacturer is "Gavazzi".
"""

METER_TYPE = 16
METER_NAME = "Carlo Gavazzi EM340"
ENDIANNESS = "HBF_LWF"
FUNCTION_CODE = 4
DATA_TYPE = "INT32"

# From EMConfig[16]: IRegister=0x000C, IDivisor=3 (mA)
#                    PRegister=0x0028, PDivisor=1 (0.1W)
#                    ERegister=0x0034, EDivisor=1, ERegister_Exp=0x004E, EDivisor_Exp=1
REGISTERS = {
    'current_l1':    0x000C,
    'current_l2':    0x000E,  # +2
    'current_l3':    0x0010,  # +4
    'power_total':   0x0028,
    'energy_import': 0x0034,
    'energy_export': 0x004E,
}

I_DIVISOR = 3    # mA -> divide by 1000
P_DIVISOR = 1    # 0.1W -> divide by 10
E_DIVISOR = 1    # 0.1kWh -> divide by 10
E_DIVISOR_EXP = 1

IS_3PHASE = True

TEST_VECTORS = [
    {
        'name': 'nominal_3phase_16A',
        'description': '3-phase 16A, current in mA, HBF_LWF endianness',
        'current_values': [16000, 16000, 16000],  # mA
        'power_value': 110400,  # 0.1W
        'energy_import_value': 12345,  # 0.1kWh
        'energy_export_value': 0,
        'expected_current_mA': [16000, 16000, 16000],
        'expected_power_W': 11040,
        'expected_energy_import_Wh': 1234500,
        'expected_energy_export_Wh': 0,
    },
    {
        'name': 'low_current',
        'description': '3-phase 6A',
        'current_values': [6000, 6000, 6000],
        'power_value': 41400,
        'energy_import_value': 100,
        'energy_export_value': 0,
        'expected_current_mA': [6000, 6000, 6000],
        'expected_power_W': 4140,
        'expected_energy_import_Wh': 10000,
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
]
