"""Eastron SDM630 3-phase energy meter profile.

Register map from Eastron SDM630 Modbus protocol document v2.
EMConfig[] index 4 (EM_EASTRON3P).
"""

METER_TYPE = 4
METER_NAME = "Eastron SDM630 (3-phase)"
ENDIANNESS = "HBF_HWF"
FUNCTION_CODE = 4
DATA_TYPE = "FLOAT32"

# From EMConfig[4]: IRegister=0x0006, IDivisor=0
#                   PRegister=0x0034, PDivisor=0
#                   ERegister=0x0048, EDivisor=0, ERegister_Exp=0x004A, EDivisor_Exp=0
REGISTERS = {
    'current_l1':    0x0006,
    'current_l2':    0x0008,  # IRegister + 2
    'current_l3':    0x000A,  # IRegister + 4
    'power_total':   0x0034,
    'energy_import': 0x0048,
    'energy_export': 0x004A,
}

I_DIVISOR = 0    # Current in Amps (multiply by 10^0 = 1) -> result in 0.1A (firmware convention)
P_DIVISOR = 0    # Power in Watts
E_DIVISOR = 0    # Energy in kWh
E_DIVISOR_EXP = 0

IS_3PHASE = True

TEST_VECTORS = [
    {
        'name': 'nominal_3phase_16A',
        'description': '3-phase 16A balanced load at 230V',
        'current_values': [16.0, 16.0, 16.0],
        'power_value': 11040.0,
        'energy_import_value': 1234.567,
        'energy_export_value': 0.0,
        'expected_current_mA': [16000, 16000, 16000],
        'expected_power_W': 11040,
        'expected_energy_import_Wh': 1234567,
        'expected_energy_export_Wh': 0,
    },
    {
        'name': 'single_phase_6A',
        'description': 'Single phase 6A minimum charge current',
        'current_values': [6.0, 0.0, 0.0],
        'power_value': 1380.0,
        'energy_import_value': 0.001,
        'energy_export_value': 0.0,
        'expected_current_mA': [6000, 0, 0],
        'expected_power_W': 1380,
        'expected_energy_import_Wh': 1,
        'expected_energy_export_Wh': 0,
    },
    {
        'name': 'max_current_32A',
        'description': '3-phase 32A maximum charge current',
        'current_values': [32.0, 32.0, 32.0],
        'power_value': 22080.0,
        'energy_import_value': 99999.0,
        'energy_export_value': 500.0,
        'expected_current_mA': [32000, 32000, 32000],
        'expected_power_W': 22080,
        'expected_energy_import_Wh': 99999000,
        'expected_energy_export_Wh': 500000,
    },
]
