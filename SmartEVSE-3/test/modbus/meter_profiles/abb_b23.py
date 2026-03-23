"""ABB B23 212-100 energy meter profile.

EMConfig[] index 6 (EM_ABB). Uses function code 3 (holding registers)
and INT32 data type.
"""

METER_TYPE = 6
METER_NAME = "ABB B23 212-100"
ENDIANNESS = "HBF_HWF"
FUNCTION_CODE = 3  # Holding registers (not input registers)
DATA_TYPE = "INT32"

# From EMConfig[6]: IRegister=0x5B0C, IDivisor=2
#                   PRegister=0x5B14, PDivisor=2
#                   ERegister=0x5000, EDivisor=2, ERegister_Exp=0x5004, EDivisor_Exp=2
REGISTERS = {
    'current_l1':    0x5B0C,
    'current_l2':    0x5B0E,  # +2
    'current_l3':    0x5B10,  # +4
    'power_total':   0x5B14,
    'energy_import': 0x5000,
    'energy_export': 0x5004,
}

I_DIVISOR = 2    # 0.01A resolution -> divide by 100
P_DIVISOR = 2    # 0.01W resolution -> divide by 100
E_DIVISOR = 2    # 0.01kWh resolution -> divide by 100
E_DIVISOR_EXP = 2

IS_3PHASE = True

TEST_VECTORS = [
    {
        'name': 'nominal_3phase_16A',
        'description': '3-phase 16A, values in 0.01A units',
        # ABB stores as 0.01A, so 16A = 1600 in register
        'current_values': [1600, 1600, 1600],
        'power_value': 1104000,  # 11040W in 0.01W units
        'energy_import_value': 12345678,  # 123456.78 kWh in 0.01kWh
        'energy_export_value': 0,
        'expected_current_mA': [16000, 16000, 16000],
        'expected_power_W': 11040,
        'expected_energy_import_Wh': 123456780,
        'expected_energy_export_Wh': 0,
    },
    {
        'name': 'low_current',
        'description': '3-phase 6A minimum',
        'current_values': [600, 600, 600],
        'power_value': 414000,
        'energy_import_value': 100,  # 1.00 kWh
        'energy_export_value': 0,
        'expected_current_mA': [6000, 6000, 6000],
        'expected_power_W': 4140,
        'expected_energy_import_Wh': 1000,
        'expected_energy_export_Wh': 0,
    },
    {
        'name': 'max_current_32A',
        'description': '3-phase 32A',
        'current_values': [3200, 3200, 3200],
        'power_value': 2208000,
        'energy_import_value': 9999999,
        'energy_export_value': 50012,
        'expected_current_mA': [32000, 32000, 32000],
        'expected_power_W': 22080,
        'expected_energy_import_Wh': 99999990,
        'expected_energy_export_Wh': 500120,
    },
]
