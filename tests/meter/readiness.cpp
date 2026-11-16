#include "stubs.h"
#include "../../SmartEVSE-3/src/meter.cpp"
#include <cassert>
#include <cstdio>
#include <cstring>

Meter MainsMeter(EM_EASTRON3P, 1, COMM_TIMEOUT);
Meter CircuitMeter(EM_EASTRON3P, 2, COMM_CIRCTIMEOUT);
Meter EVMeter(EM_EASTRON3P, 3, COMM_EVTIMEOUT);

static void respond(uint16_t reg, float value, uint8_t type = MODBUS_RESPONSE) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    uint8_t data[24] = {uint8_t(bits >> 24), uint8_t(bits >> 16),
                        uint8_t(bits >> 8), uint8_t(bits)};
    ModBus response = {};
    response.Address = EVMeter.Address;
    response.Register = reg;
    response.Type = type;
    response.Data = data;
    response.DataLength = reg == EMConfig[EVMeter.Type].IRegister ? 24 : 4;
    EVMeter.ResponseToMeasurement(response);
}

int main() {
    assert(!EVMeter.Import_energy_read);
    // A running current feed must not hide a missing energy-register response.
    respond(EMConfig[EVMeter.Type].IRegister, 6.0f);
    assert(EVMeter.Timeout == COMM_EVTIMEOUT);
    assert(!EVMeter.Import_energy_read);
    respond(EMConfig[EVMeter.Type].PRegister, 1380.0f);
    assert(!EVMeter.Import_energy_read);
    respond(EMConfig[EVMeter.Type].ERegister_Exp, 10.0f);
    assert(!EVMeter.Import_energy_read);
    respond(EMConfig[EVMeter.Type].ERegister, 1000.0f, 0);
    assert(!EVMeter.Import_energy_read);
    respond(EMConfig[EVMeter.Type].ERegister, 1000.0f);
    assert(EVMeter.Import_energy_read);
    assert(EVMeter.Import_active_energy == 1000000);

    EVMeter = Meter(EM_EASTRON3P, 3, COMM_EVTIMEOUT);
    assert(!EVMeter.Import_energy_read);
    respond(EMConfig[EVMeter.Type].ERegister, 0.0f);
    assert(EVMeter.Import_energy_read);
    assert(EVMeter.Import_active_energy == 0);

    // Reversed Eastron wiring swaps the import and export registers.
    EVMeter = Meter(EM_EASTRON3P_INV, 3, COMM_EVTIMEOUT);
    respond(EMConfig[EVMeter.Type].ERegister, 10.0f);
    assert(!EVMeter.Import_energy_read);
    respond(EMConfig[EVMeter.Type].ERegister_Exp, 1000.0f);
    assert(EVMeter.Import_energy_read);
    assert(EVMeter.Import_active_energy == 1000000);
    puts("Meter readiness regression tests passed");
}
