"""Eastron SDM120 1-phase energy meter profile.

Register map from Eastron SDM120 Modbus protocol document.
EMConfig[] index 10 (EM_EASTRON1P).
"""

METER_TYPE = 10
METER_NAME = "Eastron SDM120 (1-phase)"
ENDIANNESS = "HBF_HWF"
FUNCTION_CODE = 4
DATA_TYPE = "FLOAT32"

# From EMConfig[10]: IRegister=0x0006, IDivisor=0
#                    PRegister=0x000C, PDivisor=0
#                    ERegister=0x0048, EDivisor=0, ERegister_Exp=0x004A
REGISTERS = {
    'current_l1':    0x0006,
    'power_total':   0x000C,
    'energy_import': 0x0048,
    'energy_export': 0x004A,
}

I_DIVISOR = 0
P_DIVISOR = 0
E_DIVISOR = 0
E_DIVISOR_EXP = 0

IS_3PHASE = False

TEST_VECTORS = [
    {
        'name': 'nominal_1phase_16A',
        'description': '1-phase 16A at 230V',
        'current_values': [16.0],
        'power_value': 3680.0,
        'energy_import_value': 567.89,
        'energy_export_value': 0.0,
        'expected_current_mA': [16000, 0, 0],
        'expected_power_W': 3680,
        'expected_energy_import_Wh': 567890,
        'expected_energy_export_Wh': 0,
    },
    {
        'name': 'minimum_6A',
        'description': '1-phase 6A minimum charge',
        'current_values': [6.0],
        'power_value': 1380.0,
        'energy_import_value': 0.001,
        'energy_export_value': 0.0,
        'expected_current_mA': [6000, 0, 0],
        'expected_power_W': 1380,
        'expected_energy_import_Wh': 1,
        'expected_energy_export_Wh': 0,
    },
    {
        'name': 'standby',
        'description': 'Standby 0.05A',
        'current_values': [0.05],
        'power_value': 11.5,
        'energy_import_value': 100.0,
        'energy_export_value': 0.0,
        'expected_current_mA': [50, 0, 0],
        'expected_power_W': 11,
        'expected_energy_import_Wh': 100000,
        'expected_energy_export_Wh': 0,
    },
]
