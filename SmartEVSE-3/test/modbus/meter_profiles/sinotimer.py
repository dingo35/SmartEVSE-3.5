"""Sinotimer DTS6619 energy meter profile.

EMConfig[] index 12 (EM_SINOTIMER). Uses INT16 data type.
"""

METER_TYPE = 12
METER_NAME = "Sinotimer DTS6619"
ENDIANNESS = "HBF_HWF"
FUNCTION_CODE = 4
DATA_TYPE = "INT16"

# From EMConfig[12]: IRegister=0x0003, IDivisor=2
#                    PRegister=0x0008, PDivisor=0
#                    ERegister=0x0027, EDivisor=2, ERegister_Exp=0x0031, EDivisor_Exp=2
# Note: Energy registers use INT32 (2 x INT16), but current/power are INT16
REGISTERS = {
    'current_l1':    0x0003,
    'current_l2':    0x0004,  # +1 for INT16 (single register per phase)
    'current_l3':    0x0005,  # +2
    'power_total':   0x0008,
    'energy_import': 0x0027,
    'energy_export': 0x0031,
}

I_DIVISOR = 2    # 0.01A resolution
P_DIVISOR = 0    # 1W resolution
E_DIVISOR = 2    # 0.01kWh -> but note energy uses 32-bit (2 registers)
E_DIVISOR_EXP = 2

IS_3PHASE = True

TEST_VECTORS = [
    {
        'name': 'nominal_3phase_16A',
        'description': '3-phase 16A, values in 0.01A units',
        'current_values': [1600, 1600, 1600],
        'power_value': 11040,
        'energy_import_value': None,  # Energy is 32-bit, skip for INT16 test
        'energy_export_value': None,
        'expected_current_mA': [16000, 16000, 16000],
        'expected_power_W': 11040,
        'expected_energy_import_Wh': None,
        'expected_energy_export_Wh': None,
    },
    {
        'name': 'low_current_6A',
        'description': '3-phase 6A minimum',
        'current_values': [600, 600, 600],
        'power_value': 4140,
        'energy_import_value': None,
        'energy_export_value': None,
        'expected_current_mA': [6000, 6000, 6000],
        'expected_power_W': 4140,
        'expected_energy_import_Wh': None,
        'expected_energy_export_Wh': None,
    },
    {
        'name': 'zero_current',
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
