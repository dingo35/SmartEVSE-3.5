// Hardware dependencies used by meter.cpp, replaced for the host regression test.
#define __EVSE_MAIN
#define __EVSE_ESP32
#include <cstdint>
#include <cstdlib>
#include <ctime>

#define _LOG_A(...) ((void)0)
#define _LOG_D(...) ((void)0)
#define _LOG_I(...) ((void)0)
#define _LOG_V(...) ((void)0)
#define MODBUS_RESPONSE 3
#define COMM_TIMEOUT 11
#define COMM_CIRCTIMEOUT 20
#define COMM_EVTIMEOUT 64
#define MENU_MAINS 17

struct Sensorbox {
    uint8_t SoftwareVer, WiFiConnected, WiFiAPSTA, WIFImode, WIFImodeSynced;
    uint8_t IP[4], APpassword[9];
};
enum CapacityMode_t { CAP_DISABLED, FIXED, INTERVAL, FLANDERS };
struct CapacityNode {
    uint16_t start_minutes;
    int32_t max_power_watts;
    CapacityNode *next;
};
struct ShadowPrefs {
    void markUChar(const char *, uint8_t *) {}
} shadowPrefs;

uint8_t SB2_WIFImode, SubMenu, LCDNav, Grid, LoadBl;
bool GridActive, LocalTimeSet;
CapacityMode_t CapacityMode = CAP_DISABLED;
uint16_t MaxSumMains;
unsigned long pow_10[10] = {1, 10, 100, 1000, 10000, 100000,
                          1000000, 10000000, 100000000, 1000000000};
uint16_t getItemValue(uint8_t) { return 25; }
void CalcIsum() {}
void ModbusWriteSingleRequest(uint8_t, uint16_t, uint16_t) {}
