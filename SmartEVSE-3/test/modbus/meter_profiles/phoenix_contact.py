"""Phoenix Contact EEM-350-D-MCB energy meter profile.

EMConfig[] index 2 (EM_PHOENIX_CONTACT).
Uses HBF_LWF endianness (word-swapped) and INT32.
"""

METER_TYPE = 2
METER_NAME = "Phoenix Contact EEM-350-D-MCB"
ENDIANNESS = "HBF_LWF"
FUNCTION_CODE = 4
DATA_TYPE = "INT32"

# From EMConfig[2]: IRegister=0x000C, IDivisor=3
#                   PRegister=0x0028, PDivisor=1
#                   ERegister=0x003E, EDivisor=1
REGISTERS = {
    'current_l1':    0x000C,
    'current_l2':    0x000E,  # +2
    'current_l3':    0x0010,  # +4
    'power_total':   0x0028,
    'energy_import': 0x003E,
    'energy_export': 0x0000,  # Not available (0)
}

I_DIVISOR = 3    # mA resolution -> divide by 1000
P_DIVISOR = 1    # 0.1W resolution -> divide by 10
E_DIVISOR = 1    # 0.1kWh resolution -> divide by 10
E_DIVISOR_EXP = 0

IS_3PHASE = True

TEST_VECTORS = [
    {
        'name': 'nominal_3phase_16A',
        'description': '3-phase 16A, current in mA',
        # Phoenix stores current in mA, so 16A = 16000 in register
        'current_values': [16000, 16000, 16000],
        'power_value': 110400,  # 11040W in 0.1W units
        'energy_import_value': 12345,  # 1234.5 kWh in 0.1kWh
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
        'energy_import_value': 10,  # 1.0 kWh
        'energy_export_value': 0,
        'expected_current_mA': [6000, 6000, 6000],
        'expected_power_W': 4140,
        'expected_energy_import_Wh': 1000,
        'expected_energy_export_Wh': 0,
    },
    {
        'name': 'max_current_32A',
        'description': '3-phase 32A',
        'current_values': [32000, 32000, 32000],
        'power_value': 220800,
        'energy_import_value': 999999,
        'energy_export_value': 0,
        'expected_current_mA': [32000, 32000, 32000],
        'expected_power_W': 22080,
        'expected_energy_import_Wh': 99999900,
        'expected_energy_export_Wh': 0,
    },
]
