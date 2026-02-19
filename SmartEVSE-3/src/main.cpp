/*
 * This file has shared code between SmartEVSE-3, SmartEVSE-4 and SmartEVSE-4_CH32
 * #if SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40  //SmartEVSEv3 code
 * #if SMARTEVSE_VERSION >= 40  //SmartEVSEv4 code
 * #ifndef SMARTEVSE_VERSION   //CH32 code
 */

//prevent MQTT compiling on CH32
#if defined(MQTT) && !defined(ESP32)
#error "MQTT requires ESP32 to be defined!"
#endif

#include "main.h"
#include "stdio.h"
#include "stdlib.h"
#include "meter.h"
#include "modbus.h"
#include "memory.h"  //for memcpy
#include <time.h>
#include "evse_bridge.h"
#include "serial_parser.h"
#include "led_color.h"

#ifdef SMARTEVSE_VERSION //ESP32
#define EXT extern
#define _GLCD GLCD()
#include "esp32.h"
#include <ArduinoJson.h>
#include <SPI.h>
#include <Preferences.h>

#include <FS.h>

#include <WiFi.h>
#include "network_common.h"
#include "esp_ota_ops.h"
#include "mbedtls/md_internal.h"

#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <Update.h>

#include <Logging.h>
#include <ModbusServerRTU.h>        // Slave/node
#include <ModbusClientRTU.h>        // Master

#include <soc/sens_reg.h>
#include <soc/sens_struct.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>

//OCPP includes
#if ENABLE_OCPP && defined(SMARTEVSE_VERSION) //run OCPP only on ESP32
#include <MicroOcpp.h>
#include <MicroOcppMongooseClient.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/Context.h>
#endif //ENABLE_OCPP

extern Preferences preferences;
struct DelayedTimeStruct DelayedStartTime;
struct DelayedTimeStruct DelayedStopTime;
extern unsigned char RFID[8];
extern uint16_t LCDPin;
extern uint8_t PIN_SW_IN, PIN_ACTA, PIN_ACTB, PIN_RCM_FAULT; //these pins have to be assigned dynamically because of hw version v3.1
#else //CH32
#define EXT extern "C"
#define _GLCD                                                                   // the GLCD doesnt have to be updated on the CH32
#include "ch32.h"
#include "utils.h"
extern "C" {
    #include "ch32v003fun.h"
    void RCmonCtrl(uint8_t enable);
    void delay(uint32_t ms);
    void testRCMON(void);
}
extern void CheckRS485Comm(void);
#endif


// Global data

#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=40   //CH32 and v4 ESP32
#if SMARTEVSE_VERSION >= 40 //v4 ESP32
#define RETURN return;
#define CHIP "ESP32"
extern void RecomputeSoC(void);
extern uint8_t modem_state;
#include <qca.h>
#else
#define RETURN
#define CHIP "CH32"
#endif

//CALL_ON_RECEIVE(setStatePowerUnavailable) setStatePowerUnavailable() when setStatePowerUnavailable is received
#define CALL_ON_RECEIVE(X) \
    ret = strstr(SerialBuf, #X);\
    if (ret) {\
/*        printf("@MSG: %s DEBUG CALL_ON_RECEIVE: calling %s().\n", CHIP, #X); */ \
        X();\
        RETURN \
    }

//CALL_ON_RECEIVE_PARAM(State:, setState) calls setState(param) when State:param is received
#define CALL_ON_RECEIVE_PARAM(X,Y) \
    ret = strstr(SerialBuf, #X);\
    if (ret) {\
/*        printf("@MSG: %s DEBUG CALL_ON_RECEIVE_PARAM: calling %s(%u).\n", CHIP, #X, atoi(ret+strlen(#X))); */ \
        Y(atoi(ret+strlen(#X)));\
        RETURN \
    }
//SET_ON_RECEIVE(Pilot:, pilot) sets pilot=parm when Pilot:param is received
#define SET_ON_RECEIVE(X,Y) \
    ret = strstr(SerialBuf, #X);\
    if (ret) {\
/*        printf("@MSG: %s DEBUG SET_ON_RECEIVE: setting %s to %u.\n", CHIP, #Y, atoi(ret+strlen(#X))); */ \
        Y = atoi(ret+strlen(#X));\
        RETURN \
    }

uint8_t RCMTestCounter = 0;                                                     // nr of seconds the RCM test is allowed to take
Charging_Protocol_t Charging_Protocol = IEC; // IEC 61851-1 (low-level signaling through PWM), the others are high-level signalling via the modem
#endif

// The following data will be updated by eeprom/storage data at powerup:
uint16_t MaxMains = MAX_MAINS;                                              // Max Mains Amps (hard limit, limited by the MAINS connection) (A)
uint16_t MaxSumMains = MAX_SUMMAINS;                                        // Max Mains Amps summed over all 3 phases, limit used by EU capacity rate
                                                                            // see https://github.com/serkri/SmartEVSE-3/issues/215
                                                                            // 0 means disabled, allowed value 10 - 600 A
uint8_t MaxSumMainsTime = MAX_SUMMAINSTIME;                                 // Number of Minutes we wait when MaxSumMains is exceeded, before we stop charging
uint16_t MaxSumMainsTimer = 0;
uint16_t GridRelayMaxSumMains = GRID_RELAY_MAX_SUMMAINS;                    // Max Mains Amps summed over all 3 phases, switched by relay provided by energy provider
                                                                            // Meant to obey par 14a of Energy Industry Act, where the provider can switch a device
                                                                            // down to 4.2kW by a relay connected to the "switch" connectors.
                                                                            // you will have to set the "Switch" setting to "GridRelay",
                                                                            // and connect the relay to the switch terminals
                                                                            // When the relay opens its contacts, power will be reduced to 4.2kW
                                                                            // The relay is only allowed on the Master
bool GridRelayOpen = false;                                                 // The read status of the relay
bool CustomButton = false;                                                  // The status of the custom button
bool MqttButtonState = false;                                               // The status of the button send via MQTT
uint16_t MaxCurrent = MAX_CURRENT;                                          // Max Charge current (A)
uint16_t MinCurrent = MIN_CURRENT;                                          // Minimal current the EV is happy with (A)
uint8_t Mode = MODE;                                                        // EVSE mode (0:Normal / 1:Smart / 2:Solar)
uint32_t CurrentPWM = 0;                                                    // Current PWM duty cycle value (0 - 1024)
bool CPDutyOverride = false;
uint8_t Lock = LOCK;                                                        // Cable lock device (0:Disable / 1:Solenoid / 2:Motor)
uint8_t CableLock = CABLE_LOCK;                                             // 0 = Disabled (default), 1 = Enabled; when enabled the cable is locked at all times, when disabled only when STATE != A
uint16_t MaxCircuit = MAX_CIRCUIT;                                          // Max current of the EVSE circuit (A)
uint8_t Config = CONFIG;                                                    // Configuration (0:Socket / 1:Fixed Cable)
uint8_t LoadBl = LOADBL;                                                    // Load Balance Setting (0:Disable / 1:Master / 2-8:Node)
uint8_t Switch = SWITCH;                                                    // External Switch (0:Disable / 1:Access B / 2:Access S / 
                                                                            // 3:Smart-Solar B / 4:Smart-Solar S / 5: Grid Relay
                                                                            // 6:Custom B / 7:Custom S)
                                                                            // B=momentary push <B>utton, S=toggle <S>witch
uint8_t AutoUpdate = AUTOUPDATE;                                            // Automatic Firmware Update (0:Disable / 1:Enable)
uint16_t StartCurrent = START_CURRENT;
uint16_t StopTime = STOP_TIME;
uint16_t ImportCurrent = IMPORT_CURRENT;
uint8_t Grid = GRID;                                                        // Type of Grid connected to Sensorbox (0:4Wire / 1:3Wire )
uint8_t SB2_WIFImode = SB2_WIFI_MODE;                                       // Sensorbox-2 WiFi Mode (0:Disabled / 1:Enabled / 2:Start Portal)
uint8_t RFIDReader = RFID_READER;                                           // RFID Reader (0:Disabled / 1:Enabled / 2:Enable One / 3:Learn / 4:Delete / 5:Delete All / 6: Remote via OCPP)
#if FAKE_RFID
uint8_t Show_RFID = 0;
#endif

EnableC2_t EnableC2 = ENABLE_C2;                                            // CONTACT 2 menu setting, can be set to: NOT_PRESENT, ALWAYS_OFF, SOLAR_OFF, ALWAYS_ON, AUTO
uint16_t maxTemp = MAX_TEMPERATURE;

// Priority scheduling settings (Master only, when LoadBl=1)
uint8_t PrioStrategy = PRIO_MODBUS_ADDR;                                    // Priority strategy (0:Modbus Address / 1:First Connected / 2:Last Connected)
uint16_t RotationInterval = 0;                                              // Rotation interval in minutes (0=disabled, 30-1440)
uint16_t IdleTimeout = 60;                                                  // Idle timeout in seconds (30-300)
uint32_t ConnectedTime[NR_EVSES] = {0, 0, 0, 0, 0, 0, 0, 0};              // Uptime when each EVSE entered STATE_C
uint8_t ScheduleState[NR_EVSES] = {0, 0, 0, 0, 0, 0, 0, 0};               // Scheduling state per EVSE (0:Inactive / 1:Active / 2:Paused)
uint16_t RotationTimer = 0;                                                 // Countdown timer for rotation (seconds)

Meter MainsMeter(MAINS_METER, MAINS_METER_ADDRESS, COMM_TIMEOUT);
Meter EVMeter(EV_METER, EV_METER_ADDRESS, COMM_EVTIMEOUT);
uint8_t Nr_Of_Phases_Charging = 3;                                          // Nr of phases we are charging with. Set to 1 or 3, depending on the CONTACT 2 setting, and the MODE we are in.
Switch_Phase_t Switching_Phases_C2 = NO_SWITCH;                             // Switching between 1P and 3P with the second contactor output, depends on the CONTACT 2 setting, and the MODE.

uint8_t State = STATE_A;
uint8_t ErrorFlags;
uint8_t pilot;

uint16_t MaxCapacity;                                                       // Cable limit (A) (limited by the wire in the charge cable, set automatically, or manually if Config=Fixed Cable)
uint16_t ChargeCurrent;                                                     // Calculated Charge Current (Amps *10)
uint16_t OverrideCurrent = 0;                                               // Temporary assigned current (Amps *10) (modbus)
int16_t Isum = 0;                                                           // Sum of all measured Phases (Amps *10) (can be negative)

// Load Balance variables
int16_t IsetBalanced = 0;                                                   // Max calculated current (Amps *10) available for all EVSE's
uint16_t Balanced[NR_EVSES] = {0, 0, 0, 0, 0, 0, 0, 0};                     // Amps value per EVSE
#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40   //CH32 and v3 ESP32
uint16_t BalancedMax[NR_EVSES] = {0, 0, 0, 0, 0, 0, 0, 0};                  // Max Amps value per EVSE
uint8_t BalancedState[NR_EVSES] = {0, 0, 0, 0, 0, 0, 0, 0};                 // State of all EVSE's 0=not active (state A), 1=charge request (State B), 2= Charging (State C)
uint16_t BalancedError[NR_EVSES] = {0, 0, 0, 0, 0, 0, 0, 0};                // Error state of EVSE

Node_t Node[NR_EVSES] = {                                                        // 0: Master / 1: Node 1 ...
   /*         Config   EV     EV       Min      Used    Charge Interval Solar *          // Interval Time   : last Charge time, reset when not charging
    * Online, Changed, Meter, Address, Current, Phases,  Timer,  Timer, Timer, Mode */   // Min Current     : minimal measured current per phase the EV consumes when starting to charge @ 6A (can be lower then 6A)
    {      1,       0,     0,       0,       0,      0,      0,      0,     0,    0 },   // Used Phases     : detected nr of phases when starting to charge (works with configured EVmeter meter, and might work with sensorbox)
    {      0,       1,     0,       0,       0,      0,      0,      0,     0,    0 },
    {      0,       1,     0,       0,       0,      0,      0,      0,     0,    0 },
    {      0,       1,     0,       0,       0,      0,      0,      0,     0,    0 },    
    {      0,       1,     0,       0,       0,      0,      0,      0,     0,    0 },
    {      0,       1,     0,       0,       0,      0,      0,      0,     0,    0 },
    {      0,       1,     0,       0,       0,      0,      0,      0,     0,    0 },
    {      0,       1,     0,       0,       0,      0,      0,      0,     0,    0 }            
};
void ModbusRequestLoop(void);
uint8_t Force_Single_Phase_Charging(void);
uint8_t C1Timer = 0;
uint8_t ModemStage = 0;                                                     // 0: Modem states will be executed when Modem is enabled 1: Modem stages will be skipped, as SoC is already extracted
int8_t DisconnectTimeCounter = -1;                                          // Count for how long we're disconnected, so we can more reliably throw disconnect event. -1 means counter is disabled
uint8_t ToModemWaitStateTimer = 0;                                          // Timer used from STATE_MODEM_REQUEST to STATE_MODEM_WAIT
uint8_t ToModemDoneStateTimer = 0;                                          // Timer used from STATE_MODEM_WAIT to STATE_MODEM_DONE
uint8_t LeaveModemDoneStateTimer = 0;                                       // Timer used from STATE_MODEM_DONE to other, usually STATE_B
uint8_t LeaveModemDeniedStateTimer = 0;                                     // Timer used from STATE_MODEM_DENIED to STATE_B to re-try authentication
uint8_t ModbusRequest = 0;                                                  // Flag to request Modbus information
bool PilotDisconnected = false;
uint8_t PilotDisconnectTime = 0;                                            // Time the Control Pilot line should be disconnected (Sec)
#endif
uint8_t AccessTimer = 0; //FIXME ESP32 vs CH32
int8_t TempEVSE = 0;                                                        // Temperature EVSE in deg C (-50 to +125)
uint8_t ButtonState = 0x07;                                                 // Holds latest push Buttons state (LSB 2:0)
uint8_t OldButtonState = 0x07;                                              // Holds previous push Buttons state (LSB 2:0)
uint8_t LCDNav = 0;
uint8_t SubMenu = 0;
uint8_t ChargeDelay = 0;                                                    // Delays charging at least 60 seconds in case of not enough current available.
uint8_t NoCurrent = 0;                                                      // counts overcurrent situations.
uint8_t TestState = 0;
uint8_t NodeNewMode = 0;
AccessStatus_t AccessStatus = OFF;                                          // 0: OFF, 1: ON, 2: PAUSE
uint8_t ConfigChanged = 0;

uint16_t SolarStopTimer = 0;
#ifdef SMARTEVSE_VERSION //ESP32 v3 and v4
uint8_t RCmon = RC_MON;                                                     // Residual Current Monitor (0:Disable / 1:Enable)
uint8_t DelayedRepeat;                                                      // 0 = no repeat, 1 = daily repeat
uint8_t LCDlock = LCD_LOCK;                                                 // 0 = LCD buttons operational, 1 = LCD buttons disabled
uint16_t BacklightTimer = 0;                                                // Backlight timer (sec)
uint8_t BacklightSet = 0;
uint8_t LCDTimer = 0;
uint16_t CardOffset = CARD_OFFSET;                                          // RFID card used in Enable One mode
uint8_t RFIDstatus = 0;
EXT hw_timer_t * timerA;
esp_adc_cal_characteristics_t * adc_chars_CP;
#endif

uint8_t ActivationMode = 0, ActivationTimer = 0;
volatile uint16_t adcsample = 0;
volatile uint16_t ADCsamples[25];                                           // declared volatile, as they are used in a ISR
volatile uint8_t sampleidx = 0;
char str[20];
extern volatile uint16_t ADC_CP[NUM_ADC_SAMPLES];

int phasesLastUpdate = 0;
bool phasesLastUpdateFlag = false;
int16_t IrmsOriginal[3]={0, 0, 0};
int16_t homeBatteryCurrent = 0;
time_t homeBatteryLastUpdate = 0; // Time in seconds since epoch
// set by EXTERNAL logic through MQTT/REST to indicate cheap tariffs ahead until unix time indicated
uint8_t ColorOff[3] = {0, 0, 0};          // off
uint8_t ColorNormal[3] = {0, 255, 0};   // Green
uint8_t ColorSmart[3] = {0, 255, 0};    // Green
uint8_t ColorSolar[3] = {255, 170, 0};    // Orange
uint8_t ColorCustom[3] = {0, 0, 255};    // Blue

//#define FW_UPDATE_DELAY 30        //DINGO TODO                                            // time between detection of new version and actual update in seconds
#define FW_UPDATE_DELAY 3600                                                    // time between detection of new version and actual update in seconds
uint16_t firmwareUpdateTimer = 0;                                               // timer for firmware updates in seconds, max 0xffff = approx 18 hours
                                                                                // 0 means timer inactive
                                                                                // 0 < timer < FW_UPDATE_DELAY means we are in countdown for an actual update
                                                                                // FW_UPDATE_DELAY <= timer <= 0xffff means we are in countdown for checking
                                                                                //                                              whether an update is necessary
#if ENABLE_OCPP && defined(SMARTEVSE_VERSION) //run OCPP only on ESP32
uint8_t OcppMode = OCPP_MODE; //OCPP Client mode. 0:Disable / 1:Enable

unsigned char OcppRfidUuid [7];
size_t OcppRfidUuidLen;
unsigned long OcppLastRfidUpdate;
unsigned long OcppTrackLastRfidUpdate;

bool OcppForcesLock = false;
std::shared_ptr<MicroOcpp::Configuration> OcppUnlockConnectorOnEVSideDisconnect; // OCPP Config for RFID-based transactions: if false, demand same RFID card again to unlock connector
std::shared_ptr<MicroOcpp::Transaction> OcppLockingTx; // Transaction which locks connector until same RFID card is presented again

bool OcppTrackPermitsCharge = false;
bool OcppTrackAccessBit = false;
uint8_t OcppTrackCPvoltage = PILOT_NOK; //track positive part of CP signal for OCPP transaction logic
MicroOcpp::MOcppMongooseClient *OcppWsClient;

float OcppCurrentLimit = -1.f; // Negative value: no OCPP limit defined

unsigned long OcppStopReadingSyncTime; // Stop value synchronization: delay StopTransaction by a few seconds so it reports an accurate energy reading

bool OcppDefinedTxNotification;
MicroOcpp::TxNotification OcppTrackTxNotification;
unsigned long OcppLastTxNotification;
#endif //ENABLE_OCPP

EXT uint32_t elapsedmax, elapsedtime;

//functions
EXT void setup();
EXT void setState(uint8_t NewState);
EXT void setErrorFlags(uint8_t flags);
EXT int8_t TemperatureSensor();
uint8_t OneWireReadCardId();
EXT uint8_t ProximityPin();
EXT void PowerPanicCtrl(uint8_t enable);
EXT uint8_t ReadESPdata(char *buf);

extern void requestEnergyMeasurement(uint8_t Meter, uint8_t Address, bool Export);
extern void requestNodeConfig(uint8_t NodeNr);
extern void requestPowerMeasurement(uint8_t Meter, uint8_t Address, uint16_t PRegister);
extern void requestNodeStatus(uint8_t NodeNr);
extern uint8_t processAllNodeStates(uint8_t NodeNr);
extern void BroadcastCurrent(void);
extern void CheckRFID(void);
extern void mqttPublishData();
extern void mqttSmartEVSEPublishData();
extern bool MQTTclientSmartEVSE_AppConnected;
extern void DisconnectEvent(void);
extern char EVCCID[32];
extern char RequiredEVCCID[32];
extern bool CPDutyOverride;
extern uint8_t ModbusRequest;
extern unsigned char ease8InOutQuad(unsigned char i);
extern unsigned char triwave8(unsigned char in);

extern const char StrStateName[15][13] = {"A", "B", "C", "D", "COMM_B", "COMM_B_OK", "COMM_C", "COMM_C_OK", "Activate", "B1", "C1", "MODEM_REQ", "MODEM_WAIT", "MODEM_DONE", "MODEM_DENIED"}; //note that the extern is necessary here because the const will point the compiler to internal linkage; https://cplusplus.com/forum/general/81640/
extern const char StrEnableC2[5][12] = { "Not present", "Always Off", "Solar Off", "Always On", "Auto" };

//TODO perhaps move those routines from modbus to main?
extern void ReadItemValueResponse(void);
extern void WriteItemValueResponse(void);
extern void WriteMultipleItemValueResponse(void);
uint8_t ModbusRx[256];                          // Modbus Receive buffer



//constructor
Button::Button(void) {
    // in case of a press button, we do nothing
    // in case of a toggle switch, we have to check the switch position since it might have been changed
    // since last powerup
    //     0            1          2           3           4            5              6          7
    // "Disabled", "Access B", "Access S", "Sma-Sol B", "Sma-Sol S", "Grid Relay", "Custom B", "Custom S"
    CheckSwitch(true);
}


//since in v4 ESP32 only a copy of ErrorFlags is available, we need to have functions so v4 ESP32 can set CH32 ErrorFlags
void setErrorFlags(uint8_t flags) {
#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40
    evse_sync_globals_to_ctx();
    evse_set_error_flags(&g_evse_ctx, flags);
    evse_sync_ctx_to_globals();
#else
    ErrorFlags |= flags;
#endif
#if SMARTEVSE_VERSION >= 40 //v4 ESP32
    Serial1.printf("@setErrorFlags:%u\n", flags);
#endif
}

void clearErrorFlags(uint8_t flags) {
#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40
    evse_sync_globals_to_ctx();
    evse_clear_error_flags(&g_evse_ctx, flags);
    evse_sync_ctx_to_globals();
#else
    ErrorFlags &= ~flags;
#endif
#if SMARTEVSE_VERSION >= 40 //v4 ESP32
    Serial1.printf("@clearErrorFlags:%u\n", flags);
#endif
}

// ChargeDelay owned by CH32 so ESP32 gets a copy
void setChargeDelay(uint8_t delay) {
#if SMARTEVSE_VERSION >= 40 //v4 ESP32
    Serial1.printf("@ChargeDelay:%u\n", delay);
#else
    ChargeDelay = delay;
#endif
}


#ifndef SMARTEVSE_VERSION //CH32 version
void Button::HandleSwitch(void) {
    printf("@ExtSwitch:%u.\n", Pressed);
}
#else //v3 and v4
void Button::HandleSwitch(void) 
{
    if (Pressed) {
        // Switch input pulled low
        switch (Switch) {
            case 1: // Access Button
                setAccess(AccessStatus == ON ? OFF : ON);           // Toggle AccessStatus OFF->ON->OFF (old behaviour) or PAUSE->ON
                _LOG_I("Access: %d\n", AccessStatus);
                MqttButtonState = !MqttButtonState;
                break;
            case 2: // Access Switch
                setAccess(ON);
                MqttButtonState = true;
                break;
            case 3: // Smart-Solar Button
                MqttButtonState = true;
                break;
            case 4: // Smart-Solar Switch
                if (Mode == MODE_SOLAR && AccessStatus == ON) {
                    setMode(MODE_SMART);
                }
                MqttButtonState = true;
                break;
            case 5: // Grid relay
                GridRelayOpen = false;
                MqttButtonState = true;
                break;
            case 6: // Custom button B
                CustomButton = !CustomButton;
                MqttButtonState = CustomButton;
                break;
            case 7: // Custom button S
                CustomButton = true;
                MqttButtonState = CustomButton;
                break;
            default:
                if (State == STATE_C) {                             // Menu option Access is set to Disabled
                    setState(STATE_C1);
                    if (!TestState) setChargeDelay(15);             // Keep in State B for 15 seconds, so the Charge cable can be removed.

                }
                break;
        }
        #if MQTT
                MQTTclient.publish(MQTTprefix + "/CustomButton", MqttButtonState ? "On" : "Off", false, 0);
        #endif  

        // Reset RCM error when switch is pressed/toggled
        // RCM was tripped, but RCM level is back to normal
        if ((ErrorFlags & RCM_TRIPPED) && (digitalRead(PIN_RCM_FAULT) == LOW || RCmon == 0)) {
            clearErrorFlags(RCM_TRIPPED);
        }
        // Also light up the LCD backlight
        BacklightTimer = BACKLIGHT;                                 // Backlight ON

    } else {
        // Switch input released
        uint32_t tmpMillis = millis();

        switch (Switch) {
            case 2: // Access Switch
                setAccess(OFF);
                MqttButtonState = false;
                break;
            case 3: // Smart-Solar Button
                if ((tmpMillis < TimeOfPress + 1500) && AccessStatus == ON) {                            // short press
                    if (Mode == MODE_SMART) {
                        setMode(MODE_SOLAR);
                    } else if (Mode == MODE_SOLAR) {
                        setMode(MODE_SMART);
                    }
                    ErrorFlags &= ~(LESS_6A);                       // Clear All errors
                    ChargeDelay = 0;                                // Clear any Chargedelay
                    setSolarStopTimer(0);                           // Also make sure the SolarTimer is disabled.
                    MaxSumMainsTimer = 0;
                    LCDTimer = 0;
                }
                MqttButtonState = false;
                break;
            case 4: // Smart-Solar Switch
                if (Mode == MODE_SMART && AccessStatus == ON) setMode(MODE_SOLAR);
                MqttButtonState = false;
                break;
            case 5: // Grid relay
                GridRelayOpen = true;
                MqttButtonState = false;
                break;
            case 6: // Custom button B
                break;
            case 7: // Custom button S
                CustomButton = false;
                MqttButtonState = CustomButton;
                break;
            default:
                break;
        }
        #if MQTT
                MQTTclient.publish(MQTTprefix + "/CustomButton", MqttButtonState ? "On" : "Off", false, 0);
                MQTTclient.publish(MQTTprefix + "/CustomButtonPressTime", (tmpMillis - TimeOfPress), false, 0);
        #endif
    }
}
#endif

void Button::CheckSwitch(bool force) {
#if SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40
    uint8_t Read = digitalRead(PIN_SW_IN);
#endif
#ifndef SMARTEVSE_VERSION //CH32
    uint8_t Read = funDigitalRead(SW_IN) && funDigitalRead(BUT_SW_IN);          // BUT_SW_IN = LED pushbutton, SW_IN = 12pin plug at bottom
#endif

#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40   //CH32 and v3 ESP32
    static uint8_t RB2count = 0, RB2last = 2;

    if (force)                                                                  // force to read switch position
        RB2last = 2;

    if ((RB2last == 2) && (Switch == 1 || Switch == 3 || Switch == 6))          // upon initialization we want the toggle switch to be read
        RB2last = 1;                                                            // but not the push buttons, because this would toggle the state
                                                                                // upon reboot

    // External switch changed state?
    if (Read != RB2last) {
        // make sure that noise on the input does not switch
        if (RB2count++ > 10) {
            RB2last = Read;
            Pressed = !RB2last;
            if (Pressed)
                TimeOfPress = millis();
            HandleSwitch();
            RB2count = 0;
        }
    } else { // no change in key....
        RB2count = 0;
        if (Pressed && Switch == 3 && millis() > TimeOfPress + 1500) {
            if (State == STATE_C) {
                setState(STATE_C1);
                if (!TestState) setChargeDelay(15);                             // Keep in State B for 15 seconds, so the Charge cable can be removed.
            }
        }
    }
#endif
}

Button ExtSwitch;

//similar to setAccess; OverrideCurrent owned by ESP32
void setOverrideCurrent(uint16_t Current) { //c
#ifdef SMARTEVSE_VERSION //v3 and v4
    OverrideCurrent = Current;
    SEND_TO_CH32(OverrideCurrent)

    //write_settings TODO doesnt include OverrideCurrent
#if MQTT
    // Update MQTT faster
    lastMqttUpdate = 10;
#endif //MQTT
#else //CH32
    SEND_TO_ESP32(OverrideCurrent)
#endif //SMARTEVSE_VERSION
}


/**
 *  Check if we can switch to 1 or 3 phase charging, depending on the Enable C2 setting 
 */
void CheckSwitchingPhases(void) {
    evse_sync_globals_to_ctx();
    evse_check_switching_phases(&g_evse_ctx);
    evse_sync_ctx_to_globals();
    _LOG_D("NrPhasesCharging:%u\n",Nr_Of_Phases_Charging);
}

/**
 * Set EVSE mode
 * 
 * @param uint8_t Mode
 */
void setMode(uint8_t NewMode) {
#ifdef SMARTEVSE_VERSION //v3 and v4
    if (NewMode > MODE_SOLAR) { //this should never happen
        _LOG_A("ERROR: setMode tries to set Mode to %u.\n", NewMode);
        return;
    }

    // If mainsmeter disabled we can only run in Normal Mode, unless we are a Node
    if (LoadBl <2 && !MainsMeter.Type && NewMode != MODE_NORMAL)
        return;

    // Take care of extra conditionals/checks for custom features
    setAccess(DelayedStartTime.epoch2 ? OFF : ON); //if DelayedStartTime not zero then we are Delayed Charging
    if (NewMode == MODE_SOLAR) {
        // Reset OverrideCurrent if mode is SOLAR
        setOverrideCurrent(0);
    }

    // when switching modes, we just keep charging at the phases we were charging at;
    // it's only the regulation algorithm that is changing...
    // EXCEPT when EnableC2 == Solar Off, because we would expect C2 to be off when in Solar Mode and EnableC2 == Solar Off
    // and also the other way around, multiple phases might be wanted when changing from Solar to Normal or Smart
    if (EnableC2 == SOLAR_OFF) {
        if ((Mode != MODE_SOLAR && NewMode == MODE_SOLAR) || (Mode == MODE_SOLAR && NewMode != MODE_SOLAR)) {

            // Set State to C1 or B1 to make sure CP is disconnected for 5 seconds, before switching contactors on/off
            if (State == STATE_C) setState(STATE_C1); 
            else if (State != STATE_C1 && State == STATE_B) setState(STATE_B1);
            
            _LOG_A("Disconnect CP when switching C2\n");
        }
    }

    // similar to the above, when switching between solar charging at 1P and mode change, we need to switch back to 3P
    // TODO make sure that Smart 3P -> Solar 1P also disconnects
    if ((EnableC2 == AUTO) && (Mode != NewMode) && (Mode == MODE_SOLAR) && (Nr_Of_Phases_Charging == 1) ) {
    
        // Set State to C1 or B1 to make sure CP is disconnected for 5 seconds, before switching contactors on/off
        if (State == STATE_C) setState(STATE_C1);
        else if (State != STATE_C1 && State == STATE_B) setState(STATE_B1);

        _LOG_A("AUTO Solar->Smart/Normal charging 1p->3p\n");
    }

    // Also check all other switching options
    CheckSwitchingPhases();

#if MQTT
    // Update MQTT faster
    lastMqttUpdate = 10;
#endif

    if (NewMode == MODE_SMART) {                                                // the smart-solar button used to clear all those flags toggling between those modes
        clearErrorFlags(LESS_6A);                                               // Clear All errors
        setSolarStopTimer(0);                                                   // Also make sure the SolarTimer is disabled.
        MaxSumMainsTimer = 0;
    }
    setChargeDelay(0);                                                          // Clear any Chargedelay
    BacklightTimer = BACKLIGHT;                                                 // Backlight ON
    if (Mode != NewMode) NodeNewMode = NewMode + 1;
    Mode = NewMode;    
    SEND_TO_CH32(Mode); //d


    //make mode and start/stoptimes persistent on reboot
    request_write_settings();
#else //CH32
    printf("@Mode:%u.\n", NewMode); //a
    _LOG_V("[<-] Mode:%u\n", NewMode);
#endif //SMARTEVSE_VERSION
}


/**
 * Set the solar stop timer
 *
 * @param unsigned int Timer (seconds)
 */
void setSolarStopTimer(uint16_t Timer) {
    if (SolarStopTimer == Timer)
        return;                                                             // prevent unnecessary publishing of SolarStopTimer
    SolarStopTimer = Timer;
    SEND_TO_ESP32(SolarStopTimer);
    SEND_TO_CH32(SolarStopTimer);
#if MQTT
    MQTTclient.publish(MQTTprefix + "/SolarStopTimer", SolarStopTimer, false, 0);
#endif
}

#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40   //CH32 and v3 ESP32
/**
 * Checks all parameters to determine whether
 * we are going to force single phase charging
 * Returns true if we are going to do single phase charging
 * Returns false if we are going to do (traditional) 3 phase charging
 * This is only relevant on a 3P mains and 3P car installation!
 * 1P car will always charge 1P undetermined by CONTACTOR2
 */
uint8_t Force_Single_Phase_Charging() {
    evse_sync_globals_to_ctx();
    return evse_force_single_phase(&g_evse_ctx);
}
#endif

// Write duty cycle to pin
// Value in range 0 (0% duty) to 1024 (100% duty) for ESP32, 1000 (100% duty) for CH32
void SetCPDuty(uint32_t DutyCycle){
#if SMARTEVSE_VERSION >= 40 //ESP32
    Serial1.printf("@SetCPDuty:%u\n", DutyCycle);
#else //CH32 and v3 ESP32
#if SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40 //v3 ESP32
    ledcWrite(CP_CHANNEL, DutyCycle);                                       // update PWM signal
#if DIAG_LOG
    uint32_t readback = ledcRead(CP_CHANNEL);
    if (readback != DutyCycle) {
        _LOG_A("SetCPDuty MISMATCH: wrote %u, read %u\n", DutyCycle, readback);
    }
#endif
#endif
#ifndef SMARTEVSE_VERSION  //CH32
    // update PWM signal
    TIM1->CH1CVR = DutyCycle;
#endif
#endif //v4
    CurrentPWM = DutyCycle;
}

// Set Charge Current 
// Current in Amps * 10 (160 = 16A)
void SetCurrent(uint16_t current) {
#if SMARTEVSE_VERSION >= 40 //ESP32
    Serial1.printf("@SetCurrent:%u\n", current);
#else
    uint32_t DutyCycle;

    if ((current >= (MIN_CURRENT * 10)) && (current <= 510)) DutyCycle = current / 0.6;
                                                                            // calculate DutyCycle from current
    else if ((current > 510) && (current <= 800)) DutyCycle = (current / 2.5) + 640;
    else DutyCycle = 100;                                                   // invalid, use 6A
#if SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40 //v3 ESP32
    DutyCycle = DutyCycle * 1024 / 1000;                                    // conversion to 1024 = 100%
#endif
#if DIAG_LOG
    _LOG_A("SetCurrent(%u) duty=%u\n", current, DutyCycle);
#endif
    SetCPDuty(DutyCycle);
#endif
}


void setStatePowerUnavailable(void) {
#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40   //CH32 and v3 ESP32
    evse_sync_globals_to_ctx();
    evse_set_power_unavailable(&g_evse_ctx);
    evse_sync_ctx_to_globals();
#else //v4 ESP32
    printf("@setStatePowerUnavailable\n");
#endif
}


//this replaces old CP_OFF and CP_ON and PILOT_CONNECTED and PILOT_DISCONNECTED macros
//setPilot(true) switches the PILOT ON (CONNECT), setPilot(false) switches it OFF
void setPilot(bool On) {
    if (On) {
#if SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40   //ESP32 v3
        digitalWrite(PIN_CPOFF, LOW);
    } else
        digitalWrite(PIN_CPOFF, HIGH);
#endif
#ifndef SMARTEVSE_VERSION //CH32
        funDigitalWrite(CPOFF, FUN_LOW);
    } else
        funDigitalWrite(CPOFF, FUN_HIGH);
#endif
#if SMARTEVSE_VERSION >=40 //ESP32 v4
        Serial1.printf("@setPilot:%u\n", On);
    }
#endif
}

// State is owned by the CH32
// because it is highly subject to machine interaction
// and also charging is supposed to function if ESP32 is hung/rebooted
// If the CH32 wants to change that variable, it calls setState
// which sends a message to the ESP32. No other function may change State!
// If the ESP32 wants to change the State it sends a message to CH32
// and if the change is honored, the CH32 sends an update
// to the CH32 through the setState routine
// So the setState code of the CH32 is the only routine that
// is allowed to change the value of State on CH32
// All other code has to use setState
// so for v4 we need:
// a. ESP32 setState sends message to CH32              in ESP32 src/main.cpp (this file)
// b. CH32 receiver that calls local setState           in CH32 src/evse.c
// c. CH32 setState full functionality                  in ESP32 src/main.cpp (this file) to be copied to CH32
// d. CH32 sends message to ESP32                       in ESP32 src/main.cpp (this file) to be copied to CH32
// e. ESP32 receiver that sets local variable           in ESP32 src/main.cpp


void setState(uint8_t NewState) { //c
#if SMARTEVSE_VERSION >= 40
    if (State != NewState) {
        char Str[50];
        snprintf(Str, sizeof(Str), "%02d:%02d:%02d STATE %s -> %s\n",timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, StrStateName[State], StrStateName[NewState] );
        _LOG_A("%s",Str);
        Serial1.printf("@State:%u\n", NewState); //a
    }
#endif
#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40   //CH32 and v3 ESP32
    // Core state machine logic via module; callback handles all post-actions
    evse_sync_globals_to_ctx();
    evse_set_state(&g_evse_ctx, NewState);
    evse_sync_ctx_to_globals();
#endif
}

// make it possible to call setAccess with an int parameter
void setAccess(uint8_t Access) { //c
    AccessStatus_t typed = (AccessStatus_t) Access;
    setAccess(typed);
}

// the Access_bit is owned by the ESP32
// because it is highly subject to human interaction
// and also its status is supposed to get saved in NVS
// so if the CH32 wants to change that variable,
// it sends a message to the ESP32
// and if the change is honored, the ESP32 sends an update
// to the CH32 through the ConfigItem routine
// So the receiving code of the CH32 is the only routine that
// is allowed to change the value of Acces_bit on CH32
// All other code has to use setAccess
// so for v4 we need:
// a. CH32 setAccess sends message to ESP32           in CH32 src/evse.c and/or in src/main.cpp (this file)
// b. ESP32 receiver that calls local setAccess       in ESP32 src/main.cpp
// c. ESP32 setAccess full functionality              in ESP32 src/main.cpp (this file)
// d. ESP32 sends message to CH32                     in ESP32 src/main.cpp (this file)
// e. CH32 receiver that sets local variable          in CH32 src/evse.c

// same for Mode/setMode

void setAccess(AccessStatus_t Access) { //c
#ifdef SMARTEVSE_VERSION //v3 and v4
#if SMARTEVSE_VERSION >= 40
    Serial1.printf("@Access:%u\n", (uint8_t)Access); //d
#endif

#if SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40
    // Bridge to module; state transitions trigger callback automatically
    evse_sync_globals_to_ctx();
    evse_set_access(&g_evse_ctx, Access);
    evse_sync_ctx_to_globals();
#else
    AccessStatus = Access;
    if (Access == OFF || Access == PAUSE) {
        if (State == STATE_C) setState(STATE_C1);
        else if (State != STATE_C1 && (State == STATE_B || State == STATE_MODEM_REQUEST || State == STATE_MODEM_WAIT || State == STATE_MODEM_DONE || State == STATE_MODEM_DENIED)) setState(STATE_B1);
    }
#endif

    //make AccessStatus and CardOffset persistent on reboot
    request_write_settings();

#if MQTT
    // Update MQTT faster
    lastMqttUpdate = 10;
#endif //MQTT
#else //CH32
    SEND_TO_ESP32(Access) //a
#endif //SMARTEVSE_VERSION
}


#ifndef SMARTEVSE_VERSION //CH32
// Determine the state of the Pilot signal
//
uint8_t Pilot() {

    uint16_t sample, Min = 4095, Max = 0;
    uint8_t n, ret;
    static uint8_t old_pilot = 255;

    // calculate Min/Max of last 32 CP measurements (32 ms)
    for (n=0 ; n<NUM_ADC_SAMPLES ;n++) {

        sample = ADC_CP[n];
        if (sample < Min) Min = sample;                                   // store lowest value
        if (sample > Max) Max = sample;                                   // store highest value
    }

    //printf("@MSG: min:%u max:%u\n",Min ,Max);

    // test Min/Max against fixed levels    (needs testing)
    ret = PILOT_NOK;                                                        // Pilot NOT ok
    if (Min >= 4000 ) ret = PILOT_12V;                                      // Pilot at 12V
    if ((Min >= 3300) && (Max < 4000)) ret = PILOT_9V;                      // Pilot at 9V
    if ((Min >= 2400) && (Max < 3300)) ret = PILOT_6V;                      // Pilot at 6V
    if ((Min >= 2000) && (Max < 2400)) ret = PILOT_3V;                      // Pilot at 3V
    if ((Min > 100) && (Max < 350)) ret = PILOT_DIODE;                      // Diode Check OK
    if (ret != old_pilot) {
        printf("@Pilot:%u\n", ret); //d
        old_pilot = ret;
    }
    return ret;
}
#endif
#if defined(SMARTEVSE_VERSION) && SMARTEVSE_VERSION < 40 //ESP32 v4
// Determine the state of the Pilot signal
//
uint8_t Pilot() {

    uint32_t sample, Min = 3300, Max = 0;
    uint32_t voltage;
    uint8_t n;

    // calculate Min/Max of last 25 CP measurements
    for (n=0 ; n<25 ;n++) {
        sample = ADCsamples[n];
        voltage = esp_adc_cal_raw_to_voltage( sample, adc_chars_CP);        // convert adc reading to voltage
        if (voltage < Min) Min = voltage;                                   // store lowest value
        if (voltage > Max) Max = voltage;                                   // store highest value
    }
    // Diagnostic: log ADC Min/Max once per second (every 100 calls at 10ms)
#if DIAG_LOG
    static uint8_t pilotLogCnt = 0;
    if (++pilotLogCnt >= 100) {
        pilotLogCnt = 0;
        _LOG_A("CP: min=%u max=%u\n", Min, Max);
    }
#endif

    // test Min/Max against fixed levels
    if (Min >= 3055 ) return PILOT_12V;                                     // Pilot at 12V (min 11.0V)
    if ((Min >= 2735) && (Max < 3055)) return PILOT_9V;                     // Pilot at 9V
    if ((Min >= 2400) && (Max < 2735)) return PILOT_6V;                     // Pilot at 6V
    if ((Min >= 2000) && (Max < 2400)) return PILOT_3V;                     // Pilot at 3V
    if ((Min >= 1600) && (Max < 2000)) return PILOT_SHORT;                  // Pilot short or open
    if ((Min > 100) && (Max < 300)) return PILOT_DIODE;                     // Diode Check OK
    return PILOT_NOK;                                                       // Pilot NOT ok
}
#endif

#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40   //CH32 and v3 ESP32
// Is there at least 6A(configurable MinCurrent) available for a new EVSE?
// returns 1 if there is 6A available
// returns 0 if there is no current available
// only runs on the Master or when loadbalancing Disabled
// only runs on CH32 for SmartEVSEv4
char IsCurrentAvailable(void) {
    evse_sync_globals_to_ctx();
    int result = evse_is_current_available(&g_evse_ctx);
    evse_sync_ctx_to_globals();
    return (char)result;
}
#else //v4 ESP32
bool Shadow_IsCurrentAvailable; // this is a global variable that will be kept uptodate by Timer1S on CH32
char IsCurrentAvailable(void) {
    //TODO debug:
    _LOG_A("Shadow_IsCurrentAvailable=%d.\n", Shadow_IsCurrentAvailable);
    return Shadow_IsCurrentAvailable;
}
#endif


// Calculates Balanced PWM current for each EVSE
// mod =0 normal
// mod =1 we have a new EVSE requesting to start charging.
// only runs on the Master or when loadbalancing Disabled
void CalcBalancedCurrent(char mod) {
#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40   //CH32 and v3 ESP32
    uint16_t oldSolarStopTimer = SolarStopTimer;

    // Core logic via module; state transitions trigger callback automatically
    evse_sync_globals_to_ctx();
    evse_calc_balanced_current(&g_evse_ctx, (int)mod);
    evse_sync_ctx_to_globals();

    // SolarStopTimer side effects (SEND_TO_ESP32, SEND_TO_CH32, MQTT)
    if (SolarStopTimer != oldSolarStopTimer) {
        uint16_t newVal = SolarStopTimer;
        SolarStopTimer = oldSolarStopTimer;
        setSolarStopTimer(newVal);
    }

    // Logging
    _LOG_V("Checkpoint 5 Isetbalanced=%d.%d A.\n", IsetBalanced/10, abs(IsetBalanced%10));
    if (LoadBl == 1) {
        _LOG_D("Balance: ");
        for (uint8_t n = 0; n < NR_EVSES; n++) {
            _LOG_D_NO_FUNC("EVSE%u:%s(%u.%uA) ", n, StrStateName[BalancedState[n]], Balanced[n]/10, Balanced[n]%10);
        }
        _LOG_D_NO_FUNC("\n");
    }

    // Platform communication
    SEND_TO_ESP32(ChargeCurrent)
#ifndef SMARTEVSE_VERSION //CH32
    uint16_t Balanced0 = Balanced[0];
#endif
    SEND_TO_ESP32(Balanced0)
    SEND_TO_ESP32(IsetBalanced)
#else //ESP32v4
    printf("@CalcBalancedCurrent:%i\n", mod);
#endif
} //CalcBalancedCurrent


// --- Timer1S helpers ---

#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40   //CH32 and v3 ESP32
// Log error flag transitions after the 1S state machine tick
static void timer1s_check_error_transitions(uint8_t oldErrorFlags, uint16_t oldSolarStopTimer) {
    // SolarStopTimer notification (SEND_TO_ESP32 + MQTT on each change)
    if (SolarStopTimer != oldSolarStopTimer) {
        SEND_TO_ESP32(SolarStopTimer)
#if MQTT
        MQTTclient.publish(MQTTprefix + "/SolarStopTimer", SolarStopTimer, false, 0);
#endif
    }

    // Communication error platform extras (SB2 reset, logging)
    if ((ErrorFlags & CT_NOCOMM) && !(oldErrorFlags & CT_NOCOMM)) {
        SB2.SoftwareVer = 0;
        SB2.WIFImodeSynced = 0;
        _LOG_W("Communication error!\n");
    }

    // Error logging
    if ((ErrorFlags & TEMP_HIGH) && !(oldErrorFlags & TEMP_HIGH)) {
        _LOG_W("Error, temperature %u C !\n", TempEVSE);
    }
    if ((ErrorFlags & LESS_6A) && !(oldErrorFlags & LESS_6A)) {
        if (Mode == MODE_SOLAR) { _LOG_I("Waiting for Solar power...\n"); }
        else { _LOG_I("Not enough current available!\n"); }
    }
    if (!(ErrorFlags & LESS_6A) && (oldErrorFlags & LESS_6A)) {
        _LOG_I("No power/current Errors Cleared.\n");
    }
}

#if MODEM
static void timer1s_modem_disconnect(void) {
    if (DisconnectTimeCounter >= 0) {
        DisconnectTimeCounter++;
    }
    if (DisconnectTimeCounter > 3) {
        if (pilot == PILOT_12V) {
            DisconnectTimeCounter = -1;
            printf("@DisconnectEvent\n");
        } else {
            DisconnectTimeCounter = 0;
        }
    }
}
#endif

static void timer1s_modbus_broadcast(void) {
    static uint8_t Broadcast = 1;
    // Every two seconds request measurement data from sensorbox/kwh meters.
    if (LoadBl < 2 && !Broadcast--) {
        ModbusRequest = 1;
        ModbusRequestLoop();
        Broadcast = 1;
    }
}
#endif // CH32 and v3 ESP32

#if SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40 //ESP32 v3
static void timer1s_rfid_poll(void) {
    if (RFIDReader) {
        if (OneWireReadCardId()) {
            CheckRFID();
        } else {
            RFIDstatus = 0;
        }
    }
}
#endif

#if SMARTEVSE_VERSION >=40
static void timer1s_v4_timers(void) {
    if (RFIDReader) Serial1.printf("@OneWireReadCardId\n");
    if (State == STATE_A && modem_state > MODEM_CONFIGURED && modem_state < MODEM_PRESET_NMK)
        modem_state = MODEM_PRESET_NMK;

    if (MaxSumMainsTimer) {
        MaxSumMainsTimer--;
        if (MaxSumMainsTimer == 0) {
            if (State == STATE_C) setState(STATE_C1);
            setErrorFlags(LESS_6A);
        }
    }

    if (ChargeDelay) setChargeDelay(ChargeDelay-1);

    if (AccessTimer && State == STATE_A) {
        if (--AccessTimer == 0) {
            setAccess(OFF);
        }
    } else AccessTimer = 0;
}
#endif

#if MQTT
static void timer1s_mqtt_publish(void) {
    if (lastMqttUpdate++ >= 10) {
        mqttPublishData();
    }
    static uint8_t lastSmartEVSEUpdate = 0;
    if ((MQTTclientSmartEVSE_AppConnected || PairingPin.length()) && ++lastSmartEVSEUpdate >= 5) {
        lastSmartEVSEUpdate = 0;
        mqttSmartEVSEPublishData();
    }
}
#endif

#ifndef SMARTEVSE_VERSION //CH32
static void timer1s_rcm_test(void) {
    if (ErrorFlags & RCM_TEST) {
        if (RCMTestCounter) RCMTestCounter--;
        SEND_TO_ESP32(RCMTestCounter);
        if (ErrorFlags & RCM_TRIPPED) {
            RCMTestCounter = 0;
            SEND_TO_ESP32(RCMTestCounter);
            clearErrorFlags(RCM_TEST | RCM_TRIPPED);
        } else {
            if (RCMTestCounter == 1) {
                if (State) setState(STATE_B1);
                printf("@LCDTimer:0\n");
            }
        }
    }

    printf("@IsCurrentAvailable:%u\n", IsCurrentAvailable());
    SEND_TO_ESP32(ErrorFlags)
    elapsedmax = 0;
}
#endif

// --- Timer1S dispatcher ---

void Timer1S_singlerun(void) {
#ifdef SMARTEVSE_VERSION //ESP32
    if (BacklightTimer) BacklightTimer--;                               // Decrease backlight counter every second.
#endif

#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40   //CH32 and v3 ESP32
    TempEVSE = TemperatureSensor();
    uint16_t oldSolarStopTimer = SolarStopTimer;
    uint8_t  oldErrorFlags = ErrorFlags;

    evse_sync_globals_to_ctx();
    evse_tick_1s(&g_evse_ctx);
    evse_sync_ctx_to_globals();

    timer1s_check_error_transitions(oldErrorFlags, oldSolarStopTimer);
#if MODEM
    timer1s_modem_disconnect();
#endif
    timer1s_modbus_broadcast();
#endif // CH32 and v3 ESP32

#if SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40 //ESP32 v3
    timer1s_rfid_poll();
#endif

#if SMARTEVSE_VERSION >=40
    timer1s_v4_timers();
#endif

#if MQTT
    timer1s_mqtt_publish();
#endif

#ifndef SMARTEVSE_VERSION //CH32
    timer1s_rcm_test();
#endif
} //Timer1S_singlerun




#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40   //CH32 and v3 ESP32
/**
 * Load Balancing 	Modbus Address  LoadBl
    Disabled     	0x01            0x00
    Master       	0x01            0x01
    Node 1 	        0x02            0x02
    Node 2 	        0x03            0x03
    Node 3 	        0x04            0x04
    Node 4 	        0x05            0x05
    Node 5 	        0x06            0x06
    Node 6 	        0x07            0x07
    Node 7 	        0x08            0x08
    Broadcast to all SmartEVSE with address 0x09.
**/

/**
 * In order to keep each node happy, and not timeout with a comm-error you will have to send the chargecurrent for each node in a broadcast message to all nodes
 * (address 09):

    09 10 00 20 00 08 10 00 A0 00 00 00 3C 00 00 00 00 00 00 00 00 00 00 99 24
    Node 0 00 A0 = 160 = 16.0A
    Node 1 00 00 = 0 = 0.0A
    Node 2 00 3C = 60 = 6.0A
    etc.

 *  Each time this message is received on each node, the timeout timer is reset to 10 seconds.
 *  The master will usually send this message every two seconds.
**/

/**
 * Broadcast momentary currents to all Node EVSE's
 */
void BroadcastCurrent(void) {
    //prepare registers 0x0020 thru 0x002A (including) to be sent
    uint8_t buf[sizeof(Balanced)+ 6], i;
    uint8_t *p=buf;
    memcpy(p, Balanced, sizeof(Balanced));
    p = p + sizeof(Balanced);
    // Irms values, we only send the 16 least significant bits (range -327.6A to +327.6A) per phase
    for ( i=0; i<3; i++) {
        p[i * 2] = MainsMeter.Irms[i] & 0xff;
        p[(i * 2) + 1] = MainsMeter.Irms[i] >> 8;
    }
    ModbusWriteMultipleRequest(BROADCAST_ADR, 0x0020, (uint16_t *) buf, 8 + 3);
}

/**
 * EVSE Register 0x02*: System configuration (same on all SmartEVSE in a LoadBalancing setup)
Regis 	Access 	Description 	                                        Unit 	Values
0x0200 	R/W 	EVSE mode 		                                        0:Normal / 1:Smart / 2:Solar
0x0201 	R/W 	EVSE Circuit max Current 	                        A 	10 - 160
0x0202 	R/W 	Grid type to which the Sensorbox is connected 		        0:4Wire / 1:3Wire
0x0203 	R/W 	Sensorbox 2 WiFi Mode                                   0:Disabled / 1:Enabled / 2:Portal
0x0204 	R/W 	Max Mains Current 	                                A 	10 - 200
0x0205 	R/W 	Surplus energy start Current 	                        A 	1 - 16
0x0206 	R/W 	Stop solar charging at 6A after this time 	        min 	0:Disable / 1 - 60
0x0207 	R/W 	Allow grid power when solar charging 	                A 	0 - 6
0x0208 	R/W 	Type of Mains electric meter 		                *
0x0209 	R/W 	Address of Mains electric meter 		                10 - 247
//0x020A 	R/W 	What does Mains electric meter measure 		                0:Mains (Home+EVSE+PV) / 1:Home+EVSE
0x020B 	R/W 	Type of PV electric meter 		                *
0x020C 	R/W 	Address of PV electric meter 		                        10 - 247
0x020D 	R/W 	Byte order of custom electric meter 		                0:LBF & LWF / 1:LBF & HWF / 2:HBF & LWF / 3:HBF & HWF
0x020E 	R/W 	Data type of custom electric meter 		                0:Integer / 1:Double
0x020F 	R/W 	Modbus Function (3/4) of custom electric meter
0x0210 	R/W 	Register for Voltage (V) of custom electric meter 		0 - 65530
0x0211 	R/W 	Divisor for Voltage (V) of custom electric meter 	10x 	0 - 7
0x0212 	R/W 	Register for Current (A) of custom electric meter 		0 - 65530
0x0213 	R/W 	Divisor for Current (A) of custom electric meter 	10x 	0 - 7
0x0214 	R/W 	Register for Power (W) of custom electric meter 		0 - 65534
0x0215 	R/W 	Divisor for Power (W) of custom electric meter 	        10x 	0 - 7 /
0x0216 	R/W 	Register for Energy (kWh) of custom electric meter 		0 - 65534
0x0217 	R/W 	Divisor for Energy (kWh) of custom electric meter 	10x 	0 - 7
0x0218 	R/W 	Maximum register read (Not implemented)
0x0219 	R/W 	WiFi mode
0x021A 	R/W 	Limit max current draw on MAINS (sum of phases) 	A 	9:Disable / 10 - 200
**/

/**
 * Master requests Node configuration over modbus
 * Master -> Node
 * 
 * @param uint8_t NodeNr (1-7)
 */
void requestNodeConfig(uint8_t NodeNr) {
    ModbusReadInputRequest(NodeNr + 1u, 4, 0x0108, 2);
}

/**
 * EVSE Node Config layout
 *
Reg 	Access 	Description 	                        Unit 	Values
0x0100 	R/W 	Configuration 		                        0:Socket / 1:Fixed Cable
0x0101 	R/W 	Cable lock 		                        0:Disable / 1:Solenoid / 2:Motor
0x0102 	R/W 	MIN Charge Current the EV will accept 	A 	6 - 16
0x0103 	R/W 	MAX Charge Current for this EVSE 	A 	6 - 80
0x0104 	R/W 	Load Balance 		                        0:Disabled / 1:Master / 2-8:Node
0x0105 	R/W 	External Switch on pin SW 		        0:Disabled / 1:Access Push-Button / 2:Access Switch / 3:Smart-Solar Push-Button / 4:Smart-Solar Switch
0x0106 	R/W 	Residual Current Monitor on pin RCM 		0:Disabled / 1:Enabled
0x0107 	R/W 	Use RFID reader 		                0:Disabled / 1:Enabled
0x0108 	R/W 	Type of EV electric meter 		        *
0x0109 	R/W 	Address of EV electric meter 		        10 - 247
**/

/**
 * Master receives Node configuration over modbus
 * Node -> Master
 * 
 * @param uint8_t NodeNr (1-7)
 */
void receiveNodeConfig(uint8_t *buf, uint8_t NodeNr) {
    Node[NodeNr].EVMeter = buf[1];
    Node[NodeNr].EVAddress = buf[3];

    Node[NodeNr].ConfigChanged = 0;                                             // Reset flag on master
    ModbusWriteSingleRequest(NodeNr + 1u, 0x0006, 0);                           // Reset flag on node
}

/**
 * Master requests Node status over modbus
 * Master -> Node
 *
 * @param uint8_t NodeNr (1-7)
 */
void requestNodeStatus(uint8_t NodeNr) {
    if(Node[NodeNr].Online) {
        if(Node[NodeNr].Online-- == 1) {
            // Reset Node state when node is offline
            BalancedState[NodeNr] = STATE_A;
            Balanced[NodeNr] = 0;
        }
    }

    ModbusReadInputRequest(NodeNr + 1u, 4, 0x0000, 8);
}

/** To have full control over the nodes, you will have to read each node's status registers, and see if it requests to charge.
 * for example for node 2:

    Received packet (21 bytes) 03 04 10 00 01 00 00 00 3c 00 01 00 00 00 01 00 01 00 20 4d 8c
    00 01 = state B
    00 00 = no errors
    00 3c = charge current 6.0 A
    00 01 = Smart mode
    etc.

    Here the state changes to STATE_COMM_C (00 06)
    Received packet (21 bytes) 03 04 10 00 06 00 00 00 3c 00 01 00 00 00 01 00 01 00 20 0a 8e
    So the ESVE request to charge.

    You can respond to this request by changing the state of the node to State_C
    03 10 00 00 00 02 04 00 07 00 00 49 D6
    Here it will write 00 07 (STATE_COMM_C_OK) to register 0x0000, and reset the error register 0x0001

    The node will respond to this by switching to STATE_C (Charging).
**/

/**
 * EVSE Node status layout
 *
Regist 	Access  Description 	        Unit 	Values
0x0000 	R/W 	State 		                0:A / 1:B / 2:C / 3:D / 4:Node request B / 5:Master confirm B / 6:Node request C /
                                                7:Master confirm C / 8:Activation mode / 9:B1 / 10:C1
0x0001 	R/W 	Error 	                Bit 	1:LESS_6A / 2:NO_COMM / 4:TEMP_HIGH / 8:EV_NOCOMM / 16:RCD
0x0002 	R/W 	Charging current        0.1 A 	0:no current available / 6-80
0x0003 	R/W 	EVSE mode (without saving)      0:Normal / 1:Smart / 2:Solar
0x0004 	R/W 	Solar Timer 	        s
0x0005 	R/W 	Access bit 		        0:No Access / 1:Access
0x0006 	R/W 	Configuration changed (Not implemented)
0x0007 	R 	Maximum charging current A
0x0008 	R/W 	Number of used phases (Not implemented) 0:Undetected / 1 - 3
0x0009 	R 	Real charging current (Not implemented) 0.1 A
0x000A 	R 	Temperature 	        K
0x000B 	R 	Serial number
0x0020 - 0x0027
        W 	Broadcast charge current. SmartEVSE uses only one value depending on the "Load Balancing" configuration
                                        0.1 A 	0:no current available
0x0028 - 0x0030
        W 	Broadcast MainsMeter currents L1 - L3.
                                        0.1 A
**/

/**
 * Master receives Node status over modbus
 * Node -> Master
 *
 * @param uint8_t NodeAdr (1-7)
 */
void receiveNodeStatus(uint8_t *buf, uint8_t NodeNr) {
    serial_node_status_t parsed;
    if (!serial_parse_node_status(buf, 16, &parsed))
        return;

    Node[NodeNr].Online = 5;

    BalancedState[NodeNr] = parsed.state;
    BalancedError[NodeNr] = parsed.error;
    // Update Mode when changed on Node and not Smart/Solar Switch on the Master
    // Also make sure we are not in the menu.
    Node[NodeNr].Mode = parsed.mode;

    if ((Node[NodeNr].Mode != Mode) && Switch != 4 && !LCDNav && !NodeNewMode) {
        NodeNewMode = Node[NodeNr].Mode + 1;        // Store the new Mode in NodeNewMode, we'll update Mode in 'ProcessAllNodeStates'
#ifndef SMARTEVSE_VERSION //CH32
        printf("@NodeNewMode:%u.\n", Node[NodeNr].Mode + 1); //CH32 sends new value to ESP32
#endif
    }
    Node[NodeNr].SolarTimer = parsed.solar_timer;
    Node[NodeNr].ConfigChanged = parsed.config_changed | Node[NodeNr].ConfigChanged;
    BalancedMax[NodeNr] = parsed.max_current;
    _LOG_D("ReceivedNode[%u]Status State:%u (%s) Error:%u, BalancedMax:%u, Mode:%u, ConfigChanged:%u.\n", NodeNr, BalancedState[NodeNr], StrStateName[BalancedState[NodeNr]], BalancedError[NodeNr], BalancedMax[NodeNr], Node[NodeNr].Mode, Node[NodeNr].ConfigChanged);
}


/**
 * Send Energy measurement request over modbus
 *
 * @param uint8_t Meter
 * @param uint8_t Address
 * @param bool    Export (if exported energy is requested)
 */
void requestEnergyMeasurement(uint8_t Meter, uint8_t Address, bool Export) {
    uint8_t Count = 1;                                                          // by default it only takes 1 register to get the energy measurement
    uint16_t Register = EMConfig[Meter].ERegister;
    if (Export)
        Register = EMConfig[Meter].ERegister_Exp;

    switch (Meter) {
        case EM_FINDER_7E:
        case EM_EASTRON3P:
        case EM_EASTRON1P:
        case EM_WAGO:
            break;
        case EM_SOLAREDGE:
            // Note:
            // - SolarEdge uses 16-bit values, except for this measurement it uses 32bit int format
            // - EM_SOLAREDGE should not be used for EV Energy Measurements
            // fallthrough
        case EM_SINOTIMER:
            // Note:
            // - Sinotimer uses 16-bit values, except for this measurement it uses 32bit int format
            // fallthrough
        case EM_ABB:
            // Note:
            // - ABB uses 64bit values for this register (size 2)
            Count = 2;
            break;
        case EM_EASTRON3P_INV:
            if (Export)
                Register = EMConfig[Meter].ERegister;
            else
                Register = EMConfig[Meter].ERegister_Exp;
            break;
        default:
            if (Export)
                Count = 0; //refuse to do a request on exported energy if the meter doesnt support it
            break;
    }
    if (Count)
        requestMeasurement(Meter, Address, Register, Count);
}

/**
 * Send Power measurement request over modbus
 *
 * @param uint8_t Meter
 * @param uint8_t Address
 */
void requestPowerMeasurement(uint8_t Meter, uint8_t Address, uint16_t PRegister) {
    uint8_t Count = 1;                                                          // by default it only takes 1 register to get power measurement
    switch (Meter) {
        case EM_SINOTIMER:
            // Note:
            // - Sinotimer does not output total power but only individual power of the 3 phases
            Count = 3;
            break;
    }
    requestMeasurement(Meter, Address, PRegister, Count);
}


/**
 * Master checks node status requests, and responds with new state
 * Master -> Node
 *
 * @param uint8_t NodeAdr (1-7)
 * @return uint8_t success
 */
uint8_t processAllNodeStates(uint8_t NodeNr) {
    uint16_t values[5];
    uint8_t current, write = 0, regs = 2;                                       // registers are written when Node needs updating.

    values[0] = BalancedState[NodeNr];

    current = IsCurrentAvailable();
    if (current) {                                                              // Yes enough current
        if (BalancedError[NodeNr] & LESS_6A) {
            BalancedError[NodeNr] &= ~(LESS_6A);                                // Clear Error flags
            write = 1;
        }
    }

    if ((ErrorFlags & CT_NOCOMM) && !(BalancedError[NodeNr] & CT_NOCOMM)) {
        BalancedError[NodeNr] |= CT_NOCOMM;                                     // Send Comm Error on Master to Node
        write = 1;
    }

    // Check EVSE for request to charge states
    switch (BalancedState[NodeNr]) {
        case STATE_A:
            // Reset Node
            Node[NodeNr].IntTimer = 0;
            Node[NodeNr].Timer = 0;
            Node[NodeNr].Phases = 0;
            Node[NodeNr].MinCurrent = 0;
            break;

        case STATE_COMM_B:                                                      // Request to charge A->B
            _LOG_I("Node %u State A->B request ", NodeNr);
            if (current) {                                                      // check if we have enough current
                                                                                // Yes enough current..
                BalancedState[NodeNr] = STATE_B;                                // Mark Node EVSE as active (State B)
                Balanced[NodeNr] = MinCurrent * 10;                             // Initially set current to lowest setting
                values[0] = STATE_COMM_B_OK;
                write = 1;
                _LOG_I("- OK!\n");
            } else {                                                            // We do not have enough current to start charging
                Balanced[NodeNr] = 0;                                           // Make sure the Node does not start charging by setting current to 0
                if ((BalancedError[NodeNr] & LESS_6A) == 0) {                   // Error flags cleared?
                    BalancedError[NodeNr] |= LESS_6A;                           // Normal or Smart Mode: Not enough current available
                    write = 1;
                }
                _LOG_I("- Not enough current!\n");
            }
            break;

        case STATE_COMM_C:                                                      // request to charge B->C
            _LOG_I("Node %u State B->C request\n", NodeNr);
            Balanced[NodeNr] = 0;                                               // For correct baseload calculation set current to zero
            if (current) {                                                      // check if we have enough current
                                                                                // Yes
                BalancedState[NodeNr] = STATE_C;                                // Mark Node EVSE as Charging (State C)
                CalcBalancedCurrent(1);                                         // Calculate charge current for all connected EVSE's
                values[0] = STATE_COMM_C_OK;
                write = 1;
                _LOG_I("- OK!\n");
            } else {                                                            // We do not have enough current to start charging
                if ((BalancedError[NodeNr] & LESS_6A) == 0) {          // Error flags cleared?
                    BalancedError[NodeNr] |= LESS_6A;                      // Normal or Smart Mode: Not enough current available
                    write = 1;
                }
                _LOG_I("- Not enough current!\n");
            }
            break;

        default:
            break;

    }

    // Here we set the Masters Mode to the one we received from a Slave/Node
    if (NodeNewMode) {
        if ((NodeNewMode -1) != Mode) {                                         // Don't call setMode if we are already in the correct Mode
            setMode(NodeNewMode -1);
        }   
        NodeNewMode = 0;
#ifndef SMARTEVSE_VERSION //CH32
        printf("@NodeNewMode:%u.\n", 0); //CH32 sends new value to ESP32
#endif
    }    

    // Error Flags
    values[1] = BalancedError[NodeNr];
    // Charge Current
    values[2] = 0;                                                              // This does nothing for Nodes. Currently the Chargecurrent can only be written to the Master
    // Mode
    if (Node[NodeNr].Mode != Mode) {
        regs = 4;
        write = 1;
    }    
    values[3] = Mode;
    
    // SolarStopTimer
    if (abs((int16_t)SolarStopTimer - (int16_t)Node[NodeNr].SolarTimer) > 3) {  // Write SolarStoptimer to Node if time is off by 3 seconds or more.
        regs = 5;
        write = 1;
        values[4] = SolarStopTimer;
    }    

    if (write) {
        _LOG_D("processAllNode[%u]States State:%u (%s), BalancedError:%u, Mode:%u, SolarStopTimer:%u\n",NodeNr, BalancedState[NodeNr], StrStateName[BalancedState[NodeNr]], BalancedError[NodeNr], Mode, SolarStopTimer);
        ModbusWriteMultipleRequest(NodeNr+1 , 0x0000, values, regs);            // Write State, Error, Charge Current, Mode and Solar Timer to Node
    }

    return write;
}
#endif


#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=40 //CH32 and v4 ESP32
bool ReadIrms(char *SerialBuf) {
    serial_irms_t parsed;
    if (!serial_parse_irms(SerialBuf, &parsed)) {
        if (strstr(SerialBuf, "Irms:"))
            _LOG_A("Received corrupt Irms message:%s.\n", SerialBuf);
        return false;
    }

    if (parsed.address == MainsMeter.Address) {
        for (int x = 0; x < 3; x++)
            MainsMeter.Irms[x] = parsed.irms[x];
        MainsMeter.setTimeout(COMM_TIMEOUT);
        CalcIsum();
    } else if (parsed.address == EVMeter.Address) {
        for (int x = 0; x < 3; x++)
            EVMeter.Irms[x] = parsed.irms[x];
        EVMeter.setTimeout(COMM_EVTIMEOUT);
        EVMeter.CalcImeasured();
    }
    return true;
}


bool ReadPowerMeasured(char *SerialBuf) {
    serial_power_t parsed;
    if (!serial_parse_power(SerialBuf, &parsed)) {
        if (strstr(SerialBuf, "PowerMeasured:"))
            _LOG_A("Received corrupt PowerMeasured message from WCH:%s.\n", SerialBuf);
        return false;
    }

    if (parsed.address == MainsMeter.Address) {
        MainsMeter.PowerMeasured = parsed.power;
    } else if (parsed.address == EVMeter.Address) {
        EVMeter.PowerMeasured = parsed.power;
    }
    return true;
}
#endif


#ifndef SMARTEVSE_VERSION //CH32 version
void ResetModemTimers(void) {
    ToModemWaitStateTimer = 0;
    ToModemDoneStateTimer = 0;
    LeaveModemDoneStateTimer = 0;
    LeaveModemDeniedStateTimer = 0;
    setAccess(OFF);
}


// CH32 receives info from ESP32
void CheckSerialComm(void) {
    static char SerialBuf[512];
    uint16_t len;
    char *ret;

    len = ReadESPdata(SerialBuf);
    RxRdy1 = 0;
#ifndef WCH_VERSION
#define WCH_VERSION 0 //if WCH_VERSION not defined compile time, 0 means this firmware will be overwritten by any other version; it will be re-flashed every boot
//if you compile with
//    PLATFORMIO_BUILD_FLAGS='-DWCH_VERSION='"`date +%s`" pio run -e v4 -t upload
//the current time (in epoch) is compiled via WCH_VERSION in the CH32 firmware
//which will prevent it to be reflashed every reboot
//if you compile with -DWCH_VERSION=0 it will be reflashed every reboot (handy for dev's!)
//if you compile with -DWCH_VERSION=2000000000 if will be reflashed somewhere after 2033
#endif
    // Is it a request?
    char token[64];
    strncpy(token, "version?", sizeof(token));
    ret = strstr(SerialBuf, token);
    if (ret != NULL) printf("@version:%lu\n", (unsigned long) WCH_VERSION);          // Send WCH software version

    uint8_t tmp;
    CALL_ON_RECEIVE_PARAM(State:, setState)
    CALL_ON_RECEIVE_PARAM(SetCPDuty:, SetCPDuty)
    CALL_ON_RECEIVE_PARAM(SetCurrent:, SetCurrent)
    CALL_ON_RECEIVE_PARAM(CalcBalancedCurrent:, CalcBalancedCurrent)
    CALL_ON_RECEIVE_PARAM(setPilot:,setPilot)
    CALL_ON_RECEIVE_PARAM(PowerPanicCtrl:, PowerPanicCtrl)
    CALL_ON_RECEIVE_PARAM(RCmon:, RCmonCtrl);
    CALL_ON_RECEIVE(setStatePowerUnavailable)
    CALL_ON_RECEIVE(OneWireReadCardId)
    CALL_ON_RECEIVE_PARAM(setErrorFlags:, setErrorFlags)
    CALL_ON_RECEIVE_PARAM(clearErrorFlags:, clearErrorFlags)
    CALL_ON_RECEIVE(BroadcastSettings)
    CALL_ON_RECEIVE(ResetModemTimers)

    // these variables are owned by ESP32 and copies are kept in CH32:
    SET_ON_RECEIVE(Config:, Config)
    SET_ON_RECEIVE(Lock:, Lock)
    SET_ON_RECEIVE(CableLock:, CableLock)
    SET_ON_RECEIVE(Mode:, Mode)
    SET_ON_RECEIVE(Access:, tmp); if (ret) AccessStatus = (AccessStatus_t) tmp;
    SET_ON_RECEIVE(OverrideCurrent:, OverrideCurrent)
    SET_ON_RECEIVE(LoadBl:, LoadBl)
    SET_ON_RECEIVE(MaxMains:, MaxMains)
    SET_ON_RECEIVE(MaxSumMains:, MaxSumMains)
    SET_ON_RECEIVE(MaxCurrent:, MaxCurrent)
    SET_ON_RECEIVE(MinCurrent:, MinCurrent)
    SET_ON_RECEIVE(MaxCircuit:, MaxCircuit)
    SET_ON_RECEIVE(Switch:, Switch)
    SET_ON_RECEIVE(StartCurrent:, StartCurrent)
    SET_ON_RECEIVE(StopTime:, StopTime)
    SET_ON_RECEIVE(ImportCurrent:, ImportCurrent)
    SET_ON_RECEIVE(Grid:, Grid)
    SET_ON_RECEIVE(RFIDReader:, RFIDReader)
    SET_ON_RECEIVE(MainsMeterType:, MainsMeter.Type)
    SET_ON_RECEIVE(MainsMAddress:, MainsMeter.Address)
    SET_ON_RECEIVE(EVMeterType:, EVMeter.Type)
    SET_ON_RECEIVE(EVMeterAddress:, EVMeter.Address)
    //code from validate_settings for v4:
    if (LoadBl < 2) {
        Node[0].EVMeter = EVMeter.Type;
        Node[0].EVAddress = EVMeter.Address;
    }

    SET_ON_RECEIVE(EMEndianness:, EMConfig[EM_CUSTOM].Endianness)
    SET_ON_RECEIVE(EMIRegister:, EMConfig[EM_CUSTOM].IRegister)
    SET_ON_RECEIVE(EMIDivisor:, EMConfig[EM_CUSTOM].IDivisor)
    SET_ON_RECEIVE(EMURegister:, EMConfig[EM_CUSTOM].URegister)
    SET_ON_RECEIVE(EMUDivisor:, EMConfig[EM_CUSTOM].UDivisor)
    SET_ON_RECEIVE(EMPRegister:, EMConfig[EM_CUSTOM].PRegister)
    SET_ON_RECEIVE(EMPDivisor:, EMConfig[EM_CUSTOM].PDivisor)
    SET_ON_RECEIVE(EMERegister:, EMConfig[EM_CUSTOM].ERegister)
    SET_ON_RECEIVE(EMEDivisor:, EMConfig[EM_CUSTOM].EDivisor)
    SET_ON_RECEIVE(EMDataType:, tmp); if (ret) EMConfig[EM_CUSTOM].DataType = (mb_datatype) tmp;
    SET_ON_RECEIVE(EMFunction:, EMConfig[EM_CUSTOM].Function)
    SET_ON_RECEIVE(EnableC2:, tmp); if (ret) EnableC2 = (EnableC2_t) tmp;
    SET_ON_RECEIVE(maxTemp:, maxTemp)
    SET_ON_RECEIVE(MainsMeterTimeout:, MainsMeter.Timeout)
    SET_ON_RECEIVE(EVMeterTimeout:, EVMeter.Timeout)
    SET_ON_RECEIVE(ConfigChanged:, ConfigChanged)

    SET_ON_RECEIVE(ModemStage:, ModemStage)
    SET_ON_RECEIVE(homeBatteryCurrent:, homeBatteryCurrent); if (ret) homeBatteryLastUpdate=time(NULL);

    //these variables are owned by CH32 and copies are sent to ESP32:
    SET_ON_RECEIVE(SolarStopTimer:, SolarStopTimer)

    // Wait till initialized is set by ESP
    strncpy(token, "Initialized:", sizeof(token));
    ret = strstr(SerialBuf, token);          //no need to check the value of Initialized since we always send 1
    if (ret != NULL) {
        printf("@Config:OK\n"); //only print this on reception of string
        //we now have initialized the CH32 so here are some setup() like statements:
        Nr_Of_Phases_Charging = Force_Single_Phase_Charging() ? 1 : 3;              // to prevent unnecessary switching after boot
        SEND_TO_ESP32(Nr_Of_Phases_Charging)
    }
#if MODEM
    strncpy(token, "RequiredEVCCID:", sizeof(token));
    ret = strstr(SerialBuf, token);
    if (ret) {
        strncpy(RequiredEVCCID, ret+strlen(token), sizeof(RequiredEVCCID) - 1);
        RequiredEVCCID[sizeof(RequiredEVCCID) - 1] = '\0';
        if (RequiredEVCCID[0] == 0x0a) //empty string was sent
            RequiredEVCCID[0] = '\0';
    }

    strncpy(token, "EVCCID:", sizeof(token));
    ret = strstr(SerialBuf, token);
    if (ret) {
        strncpy(EVCCID, ret+strlen(token), sizeof(EVCCID) - 1);
        EVCCID[sizeof(EVCCID) - 1] = '\0';
        if (EVCCID[0] == 0x0a) //empty string was sent
            EVCCID[0] = '\0';
    }
#endif

    ReadIrms(SerialBuf);
    ReadPowerMeasured(SerialBuf);

    //if (LoadBl) {
    //    printf("Config@OK %u,Lock@%u,Mode@%u,Current@%u,Switch@%u,RCmon@%u,PwrPanic@%u,RFID@%u\n", Config, Lock, Mode, ChargeCurrent, Switch, RCmon, PwrPanic, RFIDReader);
//        ConfigChanged = 1;
    //}

    memset(SerialBuf, 0, len);    // clear SerialBuffer

}
#endif


// Drive the cable lock/unlock actuator with retry logic.
// lock_direction: true = lock, false = unlock.
// Pulses the actuator for 600ms, then checks feedback pin.
// Retries after 5 seconds if feedback indicates the lock hasn't moved.
#ifndef SMARTEVSE_VERSION //CH32: map Arduino-style names used below
#define digitalRead funDigitalRead
#define PIN_LOCK_IN LOCK_IN
#endif
#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40   //CH32 and v3 ESP32
static void actuate_cable_lock(unsigned int *timer, unsigned int *companion,
                                bool lock_direction) {
    if (*timer == 0) {                                          // 600ms pulse
        if (lock_direction) { ACTUATOR_LOCK; }
        else { ACTUATOR_UNLOCK; }
    } else if (*timer == 6) {
        ACTUATOR_OFF;
    }
    if ((*timer)++ > 7) {
        // Check feedback pin: still in wrong position?
        int expected = lock_direction ? (Lock == 2 ? 0 : 1)    // still unlocked...
                                      : (Lock == 2 ? 1 : 0);   // still locked...
        if (digitalRead(PIN_LOCK_IN) == expected) {
            if (*timer > 50) *timer = 0;                        // retry in 5 seconds
        } else {
            *timer = 7;                                         // success, stop
        }
    }
    *companion = 0;
}
#endif

// Task that handles the Cable Lock and modbus
//
// called every 100ms
//
void Timer100ms_singlerun(void) {
#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40   //CH32 and v3 ESP32
static unsigned int locktimer = 0, unlocktimer = 0;
#endif

#ifndef SMARTEVSE_VERSION //CH32
    //Check Serial communication with ESP32
    if (RxRdy1) CheckSerialComm();
#endif

#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40   //CH32 and v3 ESP32
    // Check if the cable lock is used
    if (!Config && Lock) {                                      // Socket used and Cable lock enabled?
        // UnlockCable takes precedence over LockCable
        if ((RFIDReader == 2 && AccessStatus == OFF) ||        // One RFID card can Lock/Unlock the charging socket (like a public charging station)
#if ENABLE_OCPP && defined(SMARTEVSE_VERSION) //run OCPP only on ESP32
        (OcppMode &&!OcppForcesLock) ||
#endif
            State == STATE_A) {                                 // The charging socket is unlocked when unplugged from the EV
            if (CableLock != 1 && Lock != 0) {                  // CableLock is Enabled, do not unlock
                actuate_cable_lock(&unlocktimer, &locktimer, false);
            }
        // Lock Cable
        } else if (State != STATE_A                            // Lock cable when connected to the EV
#if ENABLE_OCPP && defined(SMARTEVSE_VERSION) //run OCPP only on ESP32
        || (OcppMode && OcppForcesLock)
#endif
        ) {
            actuate_cable_lock(&locktimer, &unlocktimer, true);
        }
    }
}

// Sequentially call the Mains/EVmeters, and polls Nodes
// Called by MBHandleError, and MBHandleData response functions.
// Once every two seconds started by Timer1s()
//
void ModbusRequestLoop() {

    static uint8_t PollEVNode = NR_EVSES;
    static uint16_t energytimer = 0;
    static uint8_t NodeOfflineProbe = 1;
    static bool probedThisCycle = false;
    uint8_t updated = 0;
    uint8_t nodeNr;

    // Every 2 seconds, request measurements from modbus meters
        // Slaves all have ModbusRequest at 0 so they never enter here
        switch (ModbusRequest) {                                            // State
            case 1:                                                         // PV kwh meter
                ModbusRequest++;
                // fall through
            case 2:                                                         // Sensorbox or kWh meter that measures -all- currents
                if (MainsMeter.Type && MainsMeter.Type != EM_API && MainsMeter.Type != EM_HOMEWIZARD_P1) { // we don't want modbus meter currents to conflict with EM_API and EM_HOMEWIZARD_P1 currents
                    _LOG_D("ModbusRequest %u: Request MainsMeter Measurement\n", ModbusRequest);
                    requestCurrentMeasurement(MainsMeter.Type, MainsMeter.Address);
                    break;
                }
                ModbusRequest++;
                // fall through
            case 3:
                // Find next online SmartEVSE
                do {
                    PollEVNode++;
                    if (PollEVNode >= NR_EVSES) PollEVNode = 0;
                } while(!Node[PollEVNode].Online);

                // Request Configuration if changed
                if (Node[PollEVNode].ConfigChanged) {
                    _LOG_D("ModbusRequest %u: Request Configuration Node %u\n", ModbusRequest, PollEVNode);
                    // This will do the following:
                    // - Send a modbus request to the Node for it's EVmeter
                    // - Node responds with the Type and Address of the EVmeter
                    // - Master writes configuration flag reset value to Node
                    // - Node acks with the exact same message
                    // This takes around 50ms in total
                    requestNodeConfig(PollEVNode);
                    break;
                }
                ModbusRequest++;
                // fall through
            case 4:                                                         // EV kWh meter, Energy measurement (total charged kWh)
                // Request Energy if EV meter is configured
                if (Node[PollEVNode].EVMeter && Node[PollEVNode].EVMeter != EM_API) {
                    _LOG_D("ModbusRequest %u: Request Energy Node %u\n", ModbusRequest, PollEVNode);
                    requestEnergyMeasurement(Node[PollEVNode].EVMeter, Node[PollEVNode].EVAddress, 0);
                    break;
                }
                ModbusRequest++;
                // fall through
            case 5:                                                         // EV kWh meter, Power measurement (momentary power in Watt)
                // Request Power if EV meter is configured
                if (Node[PollEVNode].EVMeter && Node[PollEVNode].EVMeter != EM_API) {
                    updated = 1;
                    switch(EVMeter.Type) {
                        //these meters all have their power measured via receiveCurrentMeasurement already
                        case EM_EASTRON1P:
                        case EM_EASTRON3P:
                        case EM_EASTRON3P_INV:
                        case EM_ABB:
                        case EM_FINDER_7M:
                        case EM_SCHNEIDER:
                            updated = 0;
                            break;
                        default:
                            requestPowerMeasurement(Node[PollEVNode].EVMeter, Node[PollEVNode].EVAddress,EMConfig[Node[PollEVNode].EVMeter].PRegister);
                            break;
                    }
                    if (updated) break;  // do not break when EVmeter is one of the above types
                }
                ModbusRequest++;
                // fall through
            case 6:                                                         // Node 1
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
                // Request Node Status, skip offline nodes to save time in the loop.
                // Probe One offline Node per cycle.
                if (LoadBl == 1) {
                    if (ModbusRequest == 6) probedThisCycle = false;

                    while (ModbusRequest <= 12) {
                        nodeNr = ModbusRequest - 5u;
                        if (Node[nodeNr].Online || (!probedThisCycle && nodeNr == NodeOfflineProbe)) {
                            if (!Node[nodeNr].Online) {
                                probedThisCycle = true;
                                do { 
                                    if (++NodeOfflineProbe >= NR_EVSES) NodeOfflineProbe = 1;
                                } while (Node[NodeOfflineProbe].Online && NodeOfflineProbe != nodeNr);
                                _LOG_D("Probing offline Node %u\n", nodeNr);
                            }
                            requestNodeStatus(nodeNr);
                            break;
                        }
                        ModbusRequest++;
                    }
                    if (ModbusRequest <= 12) break;
                }
                ModbusRequest = 13;
                // fall through
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
                // Here we write State, Error, Mode and SolarTimer to Online Nodes
                updated = 0;
                if (LoadBl == 1) {
                    do {       
                        if (Node[ModbusRequest - 12u].Online) {             // Skip if not online
                            if (processAllNodeStates(ModbusRequest - 12u) ) {
                                updated = 1;                                // Node updated 
                                break;
                            }
                        }
                    } while (++ModbusRequest < 20);

                } else ModbusRequest = 20;
                if (updated) break;  // break when Node updated
                // fall through
            case 20:                                                         // EV kWh meter, Current measurement
                // Request Current if EV meter is configured
                if (Node[PollEVNode].EVMeter && Node[PollEVNode].EVMeter != EM_API) {
                    _LOG_D("ModbusRequest %u: Request EVMeter Current Measurement Node %u\n", ModbusRequest, PollEVNode);
                    requestCurrentMeasurement(Node[PollEVNode].EVMeter, Node[PollEVNode].EVAddress);
                    break;
                }
                ModbusRequest++;
                // fall through
            case 21:
                // Request active energy if Mainsmeter is configured
                    if (MainsMeter.Type && MainsMeter.Type != EM_API && MainsMeter.Type != EM_HOMEWIZARD_P1 && MainsMeter.Type != EM_SENSORBOX ) { // EM_API, EM_HOMEWIZARD_P1 and Sensorbox do not support energy postings
                    energytimer++; //this ticks approx every second?!?
                    if (energytimer == 30) {
                        _LOG_D("ModbusRequest %u: Request MainsMeter Import Active Energy Measurement\n", ModbusRequest);
                        requestEnergyMeasurement(MainsMeter.Type, MainsMeter.Address, 0);
                        break;
                    }
                    if (energytimer >= 60) {
                        _LOG_D("ModbusRequest %u: Request MainsMeter Export Active Energy Measurement\n", ModbusRequest);
                        requestEnergyMeasurement(MainsMeter.Type, MainsMeter.Address, 1);
                        energytimer = 0;
                        break;
                    }
                }
                ModbusRequest++;
                // fall through
            default:
                // slave never gets here
                // what about normal mode with no meters attached?
                CalcBalancedCurrent(0);
                // No current left, or Overload (2x Maxmains)?
                if (Mode && (NoCurrent > 2 || MainsMeter.Imeasured > (MaxMains * 20))) { // I guess we don't want to set this flag in Normal mode, we just want to charge ChargeCurrent
                    // STOP charging for all EVSE's
                    // Display error message
                    setErrorFlags(LESS_6A); //NOCURRENT;
                    // Broadcast Error code over RS485
                    ModbusWriteSingleRequest(BROADCAST_ADR, 0x0001, ErrorFlags);
                    NoCurrent = 0;
                }
                if (LoadBl == 1 && !(ErrorFlags & CT_NOCOMM) ) BroadcastCurrent();               // When there is no Comm Error, Master sends current to all connected EVSE's

                if ((State == STATE_B || State == STATE_C) && !CPDutyOverride) SetCurrent(Balanced[0]); // set PWM output for Master //mind you, the !CPDutyOverride was not checked in Smart/Solar mode, but I think this was a bug!
                ModbusRequest = 0;
                //_LOG_A("Timer100ms task free ram: %u\n", uxTaskGetStackHighWaterMark( NULL ));
                break;
        } //switch
        if (ModbusRequest) ModbusRequest++;
#endif

#ifndef SMARTEVSE_VERSION //CH32
//not sure this is necessary
#undef digitalRead
#undef PIN_LOCK_IN
#endif
}

#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40   //CH32 and v3 ESP32
// Blink the RGB LED.
//
// NOTE: need to add multiple colour schemes 
//
// Task is called every 10ms
void BlinkLed_singlerun(void) {
    static led_context_t ctx = {0, 0};

    led_state_t snap;
    memset(&snap, 0, sizeof(snap));
    snap.error_flags = ErrorFlags;
    snap.access_status = AccessStatus;
    snap.state = State;
    snap.mode = Mode;
    snap.charge_delay = ChargeDelay;
    snap.custom_button = CustomButton;
    memcpy(snap.color_off, ColorOff, 3);
    memcpy(snap.color_custom, ColorCustom, 3);
    memcpy(snap.color_solar, ColorSolar, 3);
    memcpy(snap.color_smart, ColorSmart, 3);
    memcpy(snap.color_normal, ColorNormal, 3);
#ifndef SMARTEVSE_VERSION //CH32
    snap.is_ch32 = true;
    snap.rcm_test_counter = RCMTestCounter;
#endif

    uint8_t RedPwm, GreenPwm, BluePwm;

#if ENABLE_OCPP && defined(SMARTEVSE_VERSION) //run OCPP only on ESP32
    // OCPP LED overrides (depend on millis() and MicroOcpp types, kept here)
    if (OcppMode && (RFIDReader == 6 || RFIDReader == 0) &&
                millis() - OcppLastRfidUpdate < 200) {
        RedPwm = 128; GreenPwm = 128; BluePwm = 128;
    } else if (OcppMode && (RFIDReader == 6 || RFIDReader == 0) &&
                millis() - OcppLastTxNotification < 1000 && OcppTrackTxNotification == MicroOcpp::TxNotification::Authorized) {
        RedPwm = 0; GreenPwm = 255; BluePwm = 0;
    } else if (OcppMode && (RFIDReader == 6 || RFIDReader == 0) &&
                millis() - OcppLastTxNotification < 2000 && (OcppTrackTxNotification == MicroOcpp::TxNotification::AuthorizationRejected ||
                                                             OcppTrackTxNotification == MicroOcpp::TxNotification::DeAuthorized ||
                                                             OcppTrackTxNotification == MicroOcpp::TxNotification::ReservationConflict)) {
        RedPwm = 255; GreenPwm = 0; BluePwm = 0;
    } else if (OcppMode && (RFIDReader == 6 || RFIDReader == 0) &&
                millis() - OcppLastTxNotification < 300 && (OcppTrackTxNotification == MicroOcpp::TxNotification::AuthorizationTimeout ||
                                                            OcppTrackTxNotification == MicroOcpp::TxNotification::ConnectionTimeout)) {
        RedPwm = 255; GreenPwm = 0; BluePwm = 0;
    } else if (OcppMode && (RFIDReader == 6 || RFIDReader == 0) &&
                getChargePointStatus() == ChargePointStatus_Reserved) {
        RedPwm = 196; GreenPwm = 64; BluePwm = 0;
    } else if (OcppMode && (RFIDReader == 6 || RFIDReader == 0) &&
                (getChargePointStatus() == ChargePointStatus_Unavailable ||
                 getChargePointStatus() == ChargePointStatus_Faulted)) {
        RedPwm = 255; GreenPwm = 0; BluePwm = 0;
    } else
#endif //ENABLE_OCPP
    {
        led_rgb_t rgb = led_compute_color(&snap, &ctx);
        RedPwm = rgb.r;
        GreenPwm = rgb.g;
        BluePwm = rgb.b;
    }

#if SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40
    ledcWrite(RED_CHANNEL, RedPwm);
    ledcWrite(GREEN_CHANNEL, GreenPwm);
    ledcWrite(BLUE_CHANNEL, BluePwm);

#else // CH32
    // somehow the CH32 chokes on 255 values
    if (RedPwm > 254) RedPwm = 254;
    if (GreenPwm > 254) GreenPwm = 254;
    if (BluePwm > 254) BluePwm = 254;

    TIM3->CH1CVR = RedPwm;
    TIM3->CH2CVR = GreenPwm;
    TIM3->CH3CVR = BluePwm;
#endif
}
#endif

#if SMARTEVSE_VERSION >=40
void SendConfigToCH32() {
    // send configuration to WCH IC
    Serial1.printf("@Access:%u\n", AccessStatus);
    Serial1.printf("@MainsMeterType:%u\n", MainsMeter.Type);
    Serial1.printf("@MainsMAddress:%u\n", MainsMeter.Address);
    Serial1.printf("@EVMeterType:%u\n", EVMeter.Type);
    Serial1.printf("@EVMeterAddress:%u\n", EVMeter.Address);
    Serial1.printf("@EMEndianness:%u\n", EMConfig[EM_CUSTOM].Endianness);
    Serial1.printf("@EMIRegister:%u\n", EMConfig[EM_CUSTOM].IRegister);
    Serial1.printf("@EMIDivisor:%u\n", EMConfig[EM_CUSTOM].IDivisor);
    Serial1.printf("@EMURegister:%u\n", EMConfig[EM_CUSTOM].URegister);
    Serial1.printf("@EMUDivisor:%u\n", EMConfig[EM_CUSTOM].UDivisor);
    Serial1.printf("@EMPRegister:%u\n", EMConfig[EM_CUSTOM].PRegister);
    Serial1.printf("@EMPDivisor:%u\n", EMConfig[EM_CUSTOM].PDivisor);
    Serial1.printf("@EMERegister:%u\n", EMConfig[EM_CUSTOM].ERegister);
    Serial1.printf("@EMEDivisor:%u\n", EMConfig[EM_CUSTOM].EDivisor);
    Serial1.printf("@EMDataType:%u\n", EMConfig[EM_CUSTOM].DataType);
    Serial1.printf("@EMFunction:%u\n", EMConfig[EM_CUSTOM].Function);
#if MODEM
    Serial1.printf("@RequiredEVCCID:%s\n", RequiredEVCCID);
#endif
    SEND_TO_CH32(Config)
    SEND_TO_CH32(EnableC2)
    SEND_TO_CH32(Grid)
    SEND_TO_CH32(ImportCurrent)
    SEND_TO_CH32(LoadBl)
    SEND_TO_CH32(Lock)
    SEND_TO_CH32(CableLock)
    SEND_TO_CH32(MaxCircuit)
    SEND_TO_CH32(MaxCurrent)
    SEND_TO_CH32(MaxMains)
    SEND_TO_CH32(MaxSumMains)
    SEND_TO_CH32(MaxSumMainsTime)
    SEND_TO_CH32(maxTemp)
    SEND_TO_CH32(MinCurrent)
    SEND_TO_CH32(Mode)
    SEND_TO_CH32(RCmon)
    SEND_TO_CH32(RFIDReader)
    SEND_TO_CH32(StartCurrent)
    SEND_TO_CH32(StopTime)
    SEND_TO_CH32(Switch)
}


void Handle_ESP32_Message(char *SerialBuf, uint8_t *CommState) {
    char *ret;
    //since we read per separation character we know we have only one token per message,
    //so we can return if we have found one
    //TODO malformed messages when -DDBG_CH32=1 still disturb it all.....
    if (memcmp(SerialBuf, "MSG:", 4) == 0) {
        return;
    }
    if (memcmp(SerialBuf, "!Panic", 6) == 0) {
        PowerPanicESP();
        return;
    }

    char token[64];
    strncpy(token, "ExtSwitch:", sizeof(token));
    ret = strstr(SerialBuf, token);
    if (ret != NULL) {
        ExtSwitch.Pressed = atoi(ret+strlen(token));
        if (ExtSwitch.Pressed)
            ExtSwitch.TimeOfPress = millis();
        ExtSwitch.HandleSwitch();
        return;
    }
    //these variables are owned by ESP32, so if CH32 changes it it has to send copies:
    SET_ON_RECEIVE(NodeNewMode:, NodeNewMode)
    SET_ON_RECEIVE(ConfigChanged:, ConfigChanged)

    CALL_ON_RECEIVE_PARAM(Access:, setAccess)
    CALL_ON_RECEIVE_PARAM(OverrideCurrent:, setOverrideCurrent)
    CALL_ON_RECEIVE_PARAM(Mode:, setMode)
    CALL_ON_RECEIVE(write_settings)
#if MODEM
    CALL_ON_RECEIVE(DisconnectEvent)
#endif
    //these variables do not exist in CH32 so values are sent to ESP32
    SET_ON_RECEIVE(RFIDstatus:, RFIDstatus)
    SET_ON_RECEIVE(GridActive:, GridActive)
    SET_ON_RECEIVE(LCDTimer:, LCDTimer)
    SET_ON_RECEIVE(BacklightTimer:, BacklightTimer)

    //these variables are owned by CH32 and copies are sent to ESP32:
    SET_ON_RECEIVE(Pilot:, pilot)
    SET_ON_RECEIVE(Temp:, TempEVSE)
    SET_ON_RECEIVE(State:, State)
    SET_ON_RECEIVE(IsetBalanced:, IsetBalanced)
    SET_ON_RECEIVE(ChargeCurrent:, ChargeCurrent)
    SET_ON_RECEIVE(IsCurrentAvailable:, Shadow_IsCurrentAvailable)
    SET_ON_RECEIVE(ErrorFlags:, ErrorFlags)
    SET_ON_RECEIVE(ChargeDelay:, ChargeDelay)
    SET_ON_RECEIVE(SolarStopTimer:, SolarStopTimer)
    SET_ON_RECEIVE(Nr_Of_Phases_Charging:, Nr_Of_Phases_Charging)
    SET_ON_RECEIVE(RCMTestCounter:, RCMTestCounter)

    strncpy(token, "version:", sizeof(token));
    ret = strstr(SerialBuf, token);
    if (ret != NULL) {
        unsigned long WCHRunningVersion = atoi(ret+strlen(token));
        _LOG_V("version %lu received\n", WCHRunningVersion);
        SendConfigToCH32();
        Serial1.printf("@Initialized:1\n");      // this finalizes the Config setup phase
        *CommState = COMM_CONFIG_SET;
        return;
    }

    ret = strstr(SerialBuf, "Config:OK");
    if (ret != NULL) {
        _LOG_V("Config set\n");
        *CommState = COMM_STATUS_REQ;
        return;
    }

    strncpy(token, "EnableC2:", sizeof(token));
    ret = strstr(SerialBuf, token);
    if (ret != NULL) {
        EnableC2 = (EnableC2_t) atoi(ret+strlen(token)); //e
        return;
    }

    if (ReadIrms(SerialBuf)) return;
    if (ReadPowerMeasured(SerialBuf)) return;

    strncpy(token, "RFID:", sizeof(token));
    ret = strstr(SerialBuf, token);
    if (ret != NULL) {
        int n = sscanf(ret,"RFID:%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx", &RFID[0], &RFID[1], &RFID[2], &RFID[3], &RFID[4], &RFID[5], &RFID[6], &RFID[7]);
        if (n == 8) {   //success
            CheckRFID();
        } else {
            _LOG_A("Received corrupt %s, n=%d, message from WCH:%s.\n", token, n, SerialBuf);
        }
        return;
    }

    ret = strstr(SerialBuf, "Balanced0:");
    if (ret) {
        Balanced[0] = atoi(ret+strlen("Balanced0:"));
    }

    int32_t temp;
#define READMETER(X) \
    ret = strstr(SerialBuf, #X ":"); \
    if (ret) { \
        short unsigned int Address; \
        int n = sscanf(ret + strlen(#X), ":%03hu,%" SCNd32, &Address, &temp); \
        if (n == 2) { \
            if (Address == MainsMeter.Address) { \
                MainsMeter.X = temp; \
            } else if (Address == EVMeter.Address) { \
                EVMeter.X = temp; \
            } \
        } else { \
            _LOG_A("Received corrupt %s, n=%d, message from WCH:%s.\n", #X, n, SerialBuf); \
        } \
        return; \
    }

    READMETER(Energy);
    READMETER(EnergyMeterStart);
    READMETER(EnergyCharged);
    READMETER(Import_active_energy);
    READMETER(Export_active_energy);
}
#endif




// --- Timer10ms helpers ---

#ifdef SMARTEVSE_VERSION //v3 and v4
// Fade LCD backlight in/out based on BacklightTimer
static void timer10ms_backlight(void) {
    static uint8_t LcdPwm = 0;

    if (BacklightTimer > 1 && BacklightSet != 1) {                      // Enable LCD backlight at max brightness
                                                                        // start only when fully off(0) or when we are dimming the backlight(2)
        LcdPwm = LCD_BRIGHTNESS;
        ledcWrite(LCD_CHANNEL, LcdPwm);
        BacklightSet = 1;                                               // 1: we have set the backlight to max brightness
    }

    if (BacklightTimer == 1 && LcdPwm >= 3) {                           // Last second of Backlight
        LcdPwm -= 3;
        ledcWrite(LCD_CHANNEL, ease8InOutQuad(LcdPwm));                 // fade out
        BacklightSet = 2;                                               // 2: we are dimming the backlight
    }
                                                                        // Note: could be simplified by removing following code if LCD_BRIGHTNESS is multiple of 3
    if (BacklightTimer == 0 && BacklightSet) {                          // End of LCD backlight
        ledcWrite(LCD_CHANNEL, 0);                                      // switch off LED PWM
        BacklightSet = 0;                                               // 0: backlight fully off
    }
}

// Read buttons, update LCD menu and help screen
static void timer10ms_buttons(void) {
    static uint16_t old_sec = 0;
    getButtonState();

    // When one or more button(s) are pressed, we call GLCDMenu
    if (((ButtonState != 0x07) || (ButtonState != OldButtonState)) ) {
        // RCM was tripped, but RCM level is back to normal
        if ((ErrorFlags & RCM_TRIPPED) && (RCMFAULT == LOW || RCmon == 0)) {
            clearErrorFlags(RCM_TRIPPED);         // Clear RCM error bit
        }
        if (!LCDlock) GLCDMenu(ButtonState);    // LCD is unlocked, enter menu
    }

    // Update/Show Helpmenu
    if (LCDNav > MENU_ENTER && (LCDNav < MENU_EXIT || (LCDNav >= MENU_PRIO && LCDNav <= MENU_IDLE_TIMEOUT)) && (!SubMenu)) GLCDHelp();

    if (timeinfo.tm_sec != old_sec) {
        old_sec = timeinfo.tm_sec;
        _GLCD;
    }
}
#endif

#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40 //CH32 and v3
// Track EV meter energy on state transitions
static void timer10ms_ev_metering(uint8_t oldState, uint8_t pilot_val) {
    // Sample Proximity Pin only on A->B transition
    if (State != STATE_A && oldState == STATE_A) {
        MaxCapacity = ProximityPin();
        _LOG_I("Cable limit: %uA  Max: %uA\n", MaxCapacity, MaxCurrent);
    }

    // EVMeter energy tracking (STATE_A pilot 12V disconnect)
    if (State == STATE_A && pilot_val == PILOT_12V && !EVMeter.ResetKwh) {
        EVMeter.ResetKwh = 1;                                               // reset EV kWh meter on next B->C change
    }

    // EVMeter energy start (STATE_B -> STATE_C transition)
    if (State == STATE_C && oldState != STATE_C && EVMeter.Type && EVMeter.ResetKwh) {
        EVMeter.EnergyMeterStart = EVMeter.Energy;
        EVMeter.EnergyCharged = EVMeter.Energy - EVMeter.EnergyMeterStart;
        EVMeter.ResetKwh = 0;
    }
}

// Handle diode check ADC timing and activation mode CP off
static void timer10ms_diode_activation(uint8_t oldState, uint8_t oldDiodeCheck, uint8_t pilot_val) {
    // DiodeCheck ADC timing — original code sets the alarm EVERY tick where
    // pilot == PILOT_DIODE (not just on DiodeCheck 0→1 transition).
    // This is critical after ACTSTART→STATE_B: the STATE_B callback sets
    // the alarm to PWM_95 (950us), but DiodeCheck is already 1, so a
    // transition-only check would never re-set the alarm to PWM_5.
    // Without PWM_5, the ADC samples the LOW phase and Pilot() never
    // returns PILOT_9V/6V, leaving the EVSE stuck in STATE_B.
    if (pilot_val == PILOT_DIODE) {
        if (g_evse_ctx.DiodeCheck == 1 && oldDiodeCheck == 0) {
            _LOG_A("Diode OK\n");
        }
#if DIAG_LOG
        _LOG_A("Alarm -> PWM_5\n");
#endif
#ifdef SMARTEVSE_VERSION
        timerAlarmWrite(timerA, PWM_5, false);                              // Enable Timer alarm, set to start of CP signal (5%)
#else
        TIM1->CH4CVR = PWM_5;
#endif
    }

    // ActivationMode CP off (just entered STATE_ACTSTART)
    if (State == STATE_ACTSTART && oldState != STATE_ACTSTART) {
#ifdef SMARTEVSE_VERSION
        SetCPDuty(0);                                                       // PWM off
#else
        TIM1->CH1CVR = 0;
#endif
    }
}
#endif // CH32 and v3

#if SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40 //ESP32 v3
// RCM fault detection with debounce
static void timer10ms_rcm_check(void) {
    if (RCmon == 1 && digitalRead(PIN_RCM_FAULT) == HIGH) {
        delay(1);
        if (digitalRead(PIN_RCM_FAULT) == HIGH) {
            if (State) setState(STATE_B1);
            setErrorFlags(RCM_TRIPPED);
            LCDTimer = 0;
        }
    }
}
#endif

#if SMARTEVSE_VERSION >= 40 //v4
// Process serial messages from CH32 and drive comm state machine
static void timer10ms_v4_serial(void) {
    static uint16_t idx = 0;
    static char SerialBuf[512];
    static uint8_t CommState = COMM_VER_REQ;
    static uint8_t CommTimeout = 0;

    // ESP32 receives info from CH32
    // each message starts with @, : separates variable name from value, ends with \n
    // so @State:2\n would be a valid message
    while (Serial1.available()) {       // Process ALL available messages in one cycle
        idx = Serial1.readBytesUntil('\n', SerialBuf, sizeof(SerialBuf)-1);
        if (idx > 0) {
            SerialBuf[idx++] = '\n';
            SerialBuf[idx] = '\0';  // Null terminate for safety

            if (SerialBuf[0] == '@') {
                _LOG_D("[(%u)<-] %.*s", idx, idx, SerialBuf);
                Handle_ESP32_Message(SerialBuf, &CommState);
            } else {
                _LOG_W("Invalid message,SerialBuf: [(%u)] %.*s", idx, idx, SerialBuf);
            }
        } else {
            break; // No more complete messages
        }
    }

    // process data from mainboard
    if (CommTimeout == 0 && CommState != COMM_STATUS_RSP) {
        switch (CommState) {

            case COMM_VER_REQ:
                CommTimeout = 10;
                Serial1.print("@version?\n");            // send command to WCH ic
                _LOG_V("[->] version?\n");        // send command to WCH ic
                break;

            case COMM_CONFIG_SET:                       // Set mainboard configuration
                CommTimeout = 10;
                break;

            case COMM_STATUS_REQ:                       // Ready to receive status from mainboard
                CommTimeout = 10;
                Serial1.printf("@PowerPanicCtrl:0\n");
                Serial1.printf("@RCmon:%u\n", RCmon);
                CommState = COMM_STATUS_RSP;
        }
    }

    if (CommTimeout) CommTimeout--;
}
#endif //v4

// --- Timer10ms dispatcher ---

#ifndef SMARTEVSE_VERSION // CH32
#define LOG1S(fmt, ...) \
    if (millis() > log1S + 1000) printf("@MSG: " fmt, ##__VA_ARGS__);
#else
#define LOG1S(fmt, ...) //dummy
#endif

void Timer10ms_singlerun(void) {
#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40   //CH32 and v3 ESP32
    BlinkLed_singlerun();
#endif

#ifndef SMARTEVSE_VERSION //CH32
    static uint32_t log1S = millis();
    if (ModbusRxLen) CheckRS485Comm();
#endif

#ifdef SMARTEVSE_VERSION //v3 and v4
    timer10ms_backlight();
    timer10ms_buttons();
#endif

#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40 //CH32 and v3
    // Core state machine tick
    ExtSwitch.CheckSwitch();
    pilot = Pilot();
    LOG1S("WCH 10ms: state=%d, pilot=%d, ErrorFlags=%d, ChargeDelay=%d, AccessStatus=%d, MainsMeter.Type=%d.\n", State, pilot, ErrorFlags, ChargeDelay, AccessStatus, MainsMeter.Type);

    uint8_t oldState = State;
    uint8_t oldDiodeCheck = g_evse_ctx.DiodeCheck;

    evse_sync_globals_to_ctx();
    evse_tick_10ms(&g_evse_ctx, pilot);
    evse_sync_ctx_to_globals();

    timer10ms_ev_metering(oldState, pilot);

#if DIAG_LOG
    {
        extern volatile uint32_t cpPulseCount;
        static uint8_t diagBCnt = 0;
        if ((State == STATE_B || State == STATE_COMM_C) && ++diagBCnt >= 100) {
            diagBCnt = 0;
            uint32_t ledc_duty = ledcRead(CP_CHANNEL);
            uint32_t pulses = cpPulseCount;
            uint64_t tmr = timerRead(timerA);
            _LOG_A("DIAG B: pilot=%u DC=%u Err=%u CD=%u Acc=%u ledc=%u pulses=%u tmr=%llu lastADC=%u\n",
                   pilot, g_evse_ctx.DiodeCheck, ErrorFlags, ChargeDelay, AccessStatus,
                   ledc_duty, pulses, tmr, adcsample);
        }
    }
#endif

    timer10ms_diode_activation(oldState, oldDiodeCheck, pilot);
#endif // CH32 and v3

#if SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40
    timer10ms_rcm_check();
#endif

#ifndef SMARTEVSE_VERSION //CH32
    if ((ErrorFlags & CT_NOCOMM) && MainsMeter.Timeout == 10) clearErrorFlags(CT_NOCOMM);
    printf("@IsCurrentAvailable:%u\n", IsCurrentAvailable());
    SEND_TO_ESP32(ErrorFlags)
    if (millis() > log1S + 1000) {
        log1S = millis();
    }
#endif

#if SMARTEVSE_VERSION >= 40 //v4
    timer10ms_v4_serial();
#endif
}

#ifdef SMARTEVSE_VERSION //v3 and v4
void Timer10ms(void * parameter) {
    // infinite loop
    while(1) {
        Timer10ms_singlerun();
        // Pause the task for 10ms
        vTaskDelay(10 / portTICK_PERIOD_MS);
    } // while(1) loop
}

void Timer100ms(void * parameter) {
    // infinite loop
    while(1) {
        Timer100ms_singlerun();
        // Pause the task for 100ms
        vTaskDelay(100 / portTICK_PERIOD_MS);
    } // while(1) loop
}

void Timer1S(void * parameter) {
    // infinite loop
    while(1) {
        Timer1S_singlerun();
        // Pause the task for 1000ms
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    } // while(1) loop
}
#endif //SMARTEVSE_VERSION


/**
 * Check minimum and maximum of a value and set the variable
 *
 * @param uint8_t MENU_xxx
 * @param uint16_t value
 * @return uint8_t success
 */
uint8_t setItemValue(uint8_t nav, uint16_t val) {
#ifdef SMARTEVSE_VERSION //TODO THIS SHOULD BE FIXED
    if (nav < MENU_EXIT) {
        if (val < MenuStr[nav].Min || val > MenuStr[nav].Max) return 0;
    }
#endif
    switch (nav) {
//TODO not sure if we have receivers for all ESP32 senders?
#define SETITEM(M, V) \
        case M: \
            V = val; \
            SEND_TO_CH32(V) \
            SEND_TO_ESP32(V) \
            break;
        SETITEM(MENU_MAX_TEMP, maxTemp)
        SETITEM(MENU_CONFIG, Config)
        SETITEM(MENU_MODE, Mode)
        SETITEM(MENU_START, StartCurrent)
        SETITEM(MENU_STOP, StopTime)
        SETITEM(MENU_IMPORT, ImportCurrent)
        SETITEM(MENU_MAINS, MaxMains)
        SETITEM(MENU_SUMMAINS, MaxSumMains)
        SETITEM(MENU_SUMMAINSTIME, MaxSumMainsTime)
        SETITEM(MENU_MIN, MinCurrent)
        SETITEM(MENU_MAX, MaxCurrent)
        SETITEM(MENU_CIRCUIT, MaxCircuit)
        SETITEM(MENU_LOCK, Lock)
        SETITEM(MENU_SWITCH, Switch)
        SETITEM(MENU_GRID, Grid)
        SETITEM(MENU_SB2_WIFI, SB2_WIFImode)
        SETITEM(MENU_MAINSMETER, MainsMeter.Type)
        SETITEM(MENU_MAINSMETERADDRESS, MainsMeter.Address)
        SETITEM(MENU_EVMETER, EVMeter.Type)
        SETITEM(MENU_EVMETERADDRESS, EVMeter.Address)
        SETITEM(MENU_EMCUSTOM_ENDIANESS, EMConfig[EM_CUSTOM].Endianness)
        SETITEM(MENU_EMCUSTOM_FUNCTION, EMConfig[EM_CUSTOM].Function)
        SETITEM(MENU_EMCUSTOM_UREGISTER, EMConfig[EM_CUSTOM].URegister)
        SETITEM(MENU_EMCUSTOM_UDIVISOR, EMConfig[EM_CUSTOM].UDivisor)
        SETITEM(MENU_EMCUSTOM_IREGISTER, EMConfig[EM_CUSTOM].IRegister)
        SETITEM(MENU_EMCUSTOM_IDIVISOR, EMConfig[EM_CUSTOM].IDivisor)
        SETITEM(MENU_EMCUSTOM_PREGISTER, EMConfig[EM_CUSTOM].PRegister)
        SETITEM(MENU_EMCUSTOM_PDIVISOR, EMConfig[EM_CUSTOM].PDivisor)
        SETITEM(MENU_EMCUSTOM_EREGISTER, EMConfig[EM_CUSTOM].ERegister)
        SETITEM(MENU_EMCUSTOM_EDIVISOR, EMConfig[EM_CUSTOM].EDivisor)
        SETITEM(MENU_RFIDREADER, RFIDReader)
        SETITEM(MENU_AUTOUPDATE, AutoUpdate)
        SETITEM(MENU_PRIO, PrioStrategy)
        SETITEM(MENU_ROTATION, RotationInterval)
        SETITEM(MENU_IDLE_TIMEOUT, IdleTimeout)
        SETITEM(STATUS_SOLAR_TIMER, SolarStopTimer)
        SETITEM(STATUS_CONFIG_CHANGED, ConfigChanged)
        case MENU_C2:
            EnableC2 = (EnableC2_t) val;
            CheckSwitchingPhases();
            SEND_TO_CH32(EnableC2)
            SEND_TO_ESP32(EnableC2)
            break;
        case STATUS_MODE:
            if (Mode != val)                                                    // this prevents slave from waking up from OFF mode when Masters'
                                                                                // solarstoptimer starts to count
                setMode(val);
            break;
        case MENU_LOADBL:
#if SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40
            ConfigureModbusMode(val);
#endif
            LoadBl = val;
            break;
        case MENU_EMCUSTOM_DATATYPE:
            EMConfig[EM_CUSTOM].DataType = (mb_datatype)val;
            break;
#ifdef SMARTEVSE_VERSION
        case MENU_APPSERVER:
            MQTTSmartServer = val;
            MQTTSmartServerChanged = true;                                      // Signal network_loop() to handle reconnect
            break;
        case MENU_RCMON:
            RCmon = val;
#if SMARTEVSE_VERSION >= 40 //v4            
            Serial1.printf("@RCmon:%u\n", RCmon);
#endif            
            break;
        case MENU_WIFI:
            WIFImode = val;
            break;
        case MENU_LCDPIN:
            LCDPin = val;
            break;
#endif
        // Status writeable
        case STATUS_STATE:
            if (val != State) setState(val);
            break;
        case STATUS_ERROR:
            //we want ErrorFlags = val so:
            clearErrorFlags(0xFF);
            setErrorFlags(val);
            if (ErrorFlags) {                                                   // Is there an actual Error? Maybe the error got cleared?
                if (ErrorFlags & CT_NOCOMM) MainsMeter.setTimeout(0);           // clear MainsMeter.Timeout on a CT_NOCOMM error, so the error will be immediate.
                setStatePowerUnavailable();
                setChargeDelay(CHARGEDELAY);
                _LOG_V("Error message received!\n");
            } else {
                _LOG_V("Errors Cleared received!\n");
            }
            break;
        case STATUS_CURRENT:
            setOverrideCurrent(val);
            if (LoadBl < 2) MainsMeter.setTimeout(COMM_TIMEOUT);                // reset timeout when register is written
            break;
        case STATUS_ACCESS:
            setAccess((AccessStatus_t) val);
            break;

        default:
            return 0;
    }

    return 1;
}


/**
 * Get the variable
 *
 * @param uint8_t MENU_xxx
 * @return uint16_t value
 */
uint16_t getItemValue(uint8_t nav) {
    switch (nav) {
        case MENU_MAX_TEMP:
            return maxTemp;
        case MENU_C2:
            return EnableC2;
        case MENU_CONFIG:
            return Config;
        case MENU_MODE:
        case STATUS_MODE:
            return Mode;
        case MENU_START:
            return StartCurrent;
        case MENU_STOP:
            return StopTime;
        case MENU_IMPORT:
            return ImportCurrent;
        case MENU_LOADBL:
            return LoadBl;
        case MENU_MAINS:
            return MaxMains;
        case MENU_SUMMAINS:
            return MaxSumMains;
        case MENU_SUMMAINSTIME:
            return MaxSumMainsTime;
        case MENU_MIN:
            return MinCurrent;
        case MENU_MAX:
            return MaxCurrent;
        case MENU_CIRCUIT:
            return MaxCircuit;
        case MENU_LOCK:
            return Lock;
        case MENU_SWITCH:
            return Switch;
        case MENU_GRID:
            return Grid;
        case MENU_SB2_WIFI:
            return SB2_WIFImode;
        case MENU_MAINSMETER:
            return MainsMeter.Type;
        case MENU_MAINSMETERADDRESS:
            return MainsMeter.Address;
        case MENU_EVMETER:
            return EVMeter.Type;
        case MENU_EVMETERADDRESS:
            return EVMeter.Address;
        case MENU_EMCUSTOM_ENDIANESS:
            return EMConfig[EM_CUSTOM].Endianness;
        case MENU_EMCUSTOM_DATATYPE:
            return EMConfig[EM_CUSTOM].DataType;
        case MENU_EMCUSTOM_FUNCTION:
            return EMConfig[EM_CUSTOM].Function;
        case MENU_EMCUSTOM_UREGISTER:
            return EMConfig[EM_CUSTOM].URegister;
        case MENU_EMCUSTOM_UDIVISOR:
            return EMConfig[EM_CUSTOM].UDivisor;
        case MENU_EMCUSTOM_IREGISTER:
            return EMConfig[EM_CUSTOM].IRegister;
        case MENU_EMCUSTOM_IDIVISOR:
            return EMConfig[EM_CUSTOM].IDivisor;
        case MENU_EMCUSTOM_PREGISTER:
            return EMConfig[EM_CUSTOM].PRegister;
        case MENU_EMCUSTOM_PDIVISOR:
            return EMConfig[EM_CUSTOM].PDivisor;
        case MENU_EMCUSTOM_EREGISTER:
            return EMConfig[EM_CUSTOM].ERegister;
        case MENU_EMCUSTOM_EDIVISOR:
            return EMConfig[EM_CUSTOM].EDivisor;
        case MENU_RFIDREADER:
            return RFIDReader;
#ifdef SMARTEVSE_VERSION //not on CH32
        case MENU_WIFI:
            return WIFImode;    
        case MENU_LCDPIN:
            return LCDPin;
#endif
        case MENU_AUTOUPDATE:
            return AutoUpdate;
        case MENU_PRIO:
            return PrioStrategy;
        case MENU_ROTATION:
            return RotationInterval;
        case MENU_IDLE_TIMEOUT:
            return IdleTimeout;

        // Status writeable
        case STATUS_STATE:
            return State;
        case STATUS_ERROR:
            return ErrorFlags;
        case STATUS_CURRENT:
            return Balanced[0];
        case STATUS_SOLAR_TIMER:
            return SolarStopTimer;
        case STATUS_ACCESS:
            return AccessStatus;
        case STATUS_CONFIG_CHANGED:
            return ConfigChanged;

        // Status readonly
        case STATUS_MAX:
            return min(MaxCapacity,MaxCurrent);
        case STATUS_TEMP:
            return (signed int)TempEVSE;
#ifdef SMARTEVSE_VERSION //not on CH32
        case MENU_RCMON:
            return RCmon;
        case MENU_APPSERVER:
            return MQTTSmartServer;  
        case STATUS_SERIAL:
            return serialnr;
#endif
        default:
            return 0;
    }
}

/**
 * Returns the known battery charge rate if the data is not too old.
 * Returns 0 if data is too old.
 * A positive number means charging, a negative number means discharging --> this means the inverse must be used for calculations
 * 
 * Example:
 * homeBatteryCharge == 1000 --> Battery is charging using Solar
 * P1 = -500 --> Solar injection to the net but nut sufficient for charging
 * 
 * If the P1 value is added with the inverse battery charge it will inform the EVSE logic there is enough Solar --> -500 + -1000 = -1500
 * 
 * Note: The user who is posting battery charge data should take this into account, meaning: if he wants a minimum home battery (dis)charge rate he should substract this from the value he is sending.
 */
// 
int16_t getBatteryCurrent(void) {
    uint32_t elapsed = homeBatteryLastUpdate ? (uint32_t)(time(NULL) - homeBatteryLastUpdate) : 0;
    int16_t result = calc_battery_current(elapsed, Mode, MainsMeter.Type, homeBatteryCurrent);
    if (result == 0 && homeBatteryLastUpdate && elapsed > 60) {
        homeBatteryLastUpdate = 0;                      // last update was more then 60s ago, set to 0
        homeBatteryCurrent = 0;
    }
    return result;
}


void CalcIsum(void) {
    phasesLastUpdate = time(NULL);
    phasesLastUpdateFlag = true;                        // Set flag if a new Irms measurement is received.

#if FAKE_SUNNY_DAY
    MainsMeter.Irms[0] -= INJECT_CURRENT_L1 * 10;      //Irms is in units of 100mA
    MainsMeter.Irms[1] -= INJECT_CURRENT_L2 * 10;
    MainsMeter.Irms[2] -= INJECT_CURRENT_L3 * 10;
#endif

    calc_isum_input_t input = {
        .mains_irms = {MainsMeter.Irms[0], MainsMeter.Irms[1], MainsMeter.Irms[2]},
        .battery_current = getBatteryCurrent(),
        .enable_c2 = (uint8_t)EnableC2,
    };
    calc_isum_result_t result = calc_isum(&input);

    for (int x = 0; x < 3; x++) {
        IrmsOriginal[x] = MainsMeter.Irms[x];
        MainsMeter.Irms[x] = result.adjusted_irms[x];
    }
    Isum = result.isum;
    MainsMeter.CalcImeasured();
}

