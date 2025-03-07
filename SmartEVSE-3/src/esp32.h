/*
;    Project: Smart EVSE v3
;
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
 */

#ifndef __EVSE_ESP32

#define __EVSE_ESP32


#include <Arduino.h>
#include "main.h"
#include "glcd.h"
#include "meter.h"

// Pin definitions left side ESP32
#define PIN_TEMP 36
#define PIN_CP_IN 39
#define PIN_PP_IN 34
#define PIN_LOCK_IN 35
#define PIN_SSR 32
#define PIN_LCD_SDO_B3 33                                                       // = SPI_MOSI
#define PIN_LCD_CLK 26                                                          // = SPI_SCK
#define PIN_SSR2 27
#define PIN_LCD_LED 14
#define PIN_LEDB 12
#define PIN_RCM_FAULT 13 //TODO ok for v4?

// Pin definitions right side ESP32
#define PIN_RS485_RX 23
#define PIN_RS485_DIR 22
//#define PIN_RXD 
//#define PIN_TXD
#define PIN_RS485_TX 21
#define PIN_CP_OUT 19
#define PIN_ACTB 18
#define PIN_ACTA 17
#define PIN_SW_IN 16
#define PIN_LEDG 4
#define PIN_IO0_B1 0
#define PIN_LEDR 2
#define PIN_CPOFF 15

#if SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40
#define PIN_LCD_A0_B2 25
#define PIN_LCD_RST 5
#define SPI_MOSI 33                                                             // SPI connections to LCD
#define SPI_MISO -1
#define SPI_SCK 26
#define SPI_SS -1

#define CP_CHANNEL 0
#define RED_CHANNEL 2                                                           // PWM channel 2 (0 and 1 are used by CP signal)
#define GREEN_CHANNEL 3
#define BLUE_CHANNEL 4
#define LCD_CHANNEL 5                                                           // LED Backlight LCD
#else
#define PIN_LCD_A0_B2 40
#define PIN_LCD_RST 42
#include "funconfig.h"
#endif //SMARTEVSE_VERSION


#define STATE_A 0                                                               // A Vehicle not connected
#define STATE_B 1                                                               // B Vehicle connected / not ready to accept energy
#define STATE_C 2                                                               // C Vehicle connected / ready to accept energy / ventilation not required
#define STATE_D 3                                                               // D Vehicle connected / ready to accept energy / ventilation required (not implemented)
#define STATE_COMM_B 4                                                          // E State change request A->B (set by node)
#define STATE_COMM_B_OK 5                                                       // F State change A->B OK (set by master)
#define STATE_COMM_C 6                                                          // G State change request B->C (set by node)
#define STATE_COMM_C_OK 7                                                       // H State change B->C OK (set by master)
#define STATE_ACTSTART 8                                                        // I Activation mode in progress
#define STATE_B1 9                                                              // J Vehicle connected / EVSE not ready to deliver energy: no PWM signal
#define STATE_C1 10                                                             // K Vehicle charging / EVSE not ready to deliver energy: no PWM signal (temp state when stopping charge from EVSE)
//#if SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40 TODO
#define STATE_MODEM_REQUEST 11                                                          // L Vehicle connected / requesting ISO15118 communication, 0% duty
#define STATE_MODEM_WAIT 12                                                          // M Vehicle connected / requesting ISO15118 communication, 5% duty
#define STATE_MODEM_DONE 13                                                // Modem communication succesful, SoCs extracted. Here, re-plug vehicle
#define STATE_MODEM_DENIED 14                                                // Modem access denied based on EVCCID, re-plug vehicle and try again
//#else
//#define STATE_E 11                  // disconnected pilot / powered down
//#define STATE_F 12                  // -12V Fault condition
//#endif

#define NOSTATE 255

#define _RSTB_0 digitalWrite(PIN_LCD_RST, LOW);
#define _RSTB_1 digitalWrite(PIN_LCD_RST, HIGH);
#define _A0_0 digitalWrite(PIN_LCD_A0_B2, LOW);
#define _A0_1 digitalWrite(PIN_LCD_A0_B2, HIGH);

extern portMUX_TYPE rtc_spinlock;   //TODO: Will be placed in the appropriate position after the rtc module is finished.

#define RTC_ENTER_CRITICAL()    portENTER_CRITICAL(&rtc_spinlock)
#define RTC_EXIT_CRITICAL()     portEXIT_CRITICAL(&rtc_spinlock)


extern char SmartConfigKey[];
extern struct tm timeinfo;


extern uint8_t Mode;                                                            // EVSE mode
extern uint8_t LoadBl;                                                          // Load Balance Setting (Disable, Master or Node)
extern uint8_t Grid;
#if FAKE_RFID
extern uint8_t Show_RFID;
#endif

extern uint8_t State;
extern uint8_t NextState;

extern int16_t Isum;
extern uint16_t Balanced[NR_EVSES];                                             // Amps value per EVSE

extern uint8_t LCDTimer;
extern uint16_t BacklightTimer;                                                 // remaining seconds the LCD backlight is active
extern uint8_t ButtonState;                                                     // Holds latest push Buttons state (LSB 2:0)
extern uint8_t OldButtonState;                                                  // Holds previous push Buttons state (LSB 2:0)
extern SemaphoreHandle_t buttonMutex;
extern uint8_t ButtonStateOverride;                                             // Override the state via API
extern uint32_t LastBtnOverrideTime;                                            // Prevent hanging buttons
extern uint32_t ScrollTimer;
extern uint8_t ChargeDelay;                                                     // Delays charging in seconds.
extern uint8_t TestState;
extern uint8_t Access_bit;
extern uint16_t CardOffset;

extern uint8_t GridActive;                                                      // When the CT's are used on Sensorbox2, it enables the GRID menu option.
extern uint16_t SolarStopTimer;
extern int32_t EnergyCapacity;
extern uint8_t RFIDstatus;
extern uint8_t OcppMode;
extern bool LocalTimeSet;
extern uint32_t serialnr;

extern const char StrEnableC2[5][12];
extern Single_Phase_t Switching_To_Single_Phase;
extern uint8_t Nr_Of_Phases_Charging;

const struct {
    char LCD[10];
    char Desc[52];
    uint16_t Min;
    uint16_t Max;
    uint16_t Default;
} MenuStr[MENU_EXIT + 1] = {
    {"", "Not in menu", 0, 0, 0},
    {"", "Hold 2 sec", 0, 0, 0},

    // Node specific configuration
    /* LCD,       Desc,                                                 Min, Max, Default */
    {"CONFIG",  "Fixed Cable or Type 2 Socket",                       0, 1, CONFIG},
    {"LOCK",    "Cable locking actuator type",                        0, 2, LOCK},
    {"MIN",     "MIN Charge Current the EV will accept (per phase)",  MIN_CURRENT, 16, MIN_CURRENT},
    {"MAX",     "MAX Charge Current for this EVSE (per phase)",       6, 80, MAX_CURRENT},
    {"PWR SHARE", "Share Power between multiple SmartEVSEs (2-8)",    0, NR_EVSES, LOADBL},
    {"SWITCH",  "Switch function control on pin SW",                  0, 7, SWITCH},
    {"RCMON",   "Residual Current Monitor on pin RCM",                0, 1, RC_MON},
    {"RFID",    "RFID reader, learn/remove cards",                    0, 5 + (ENABLE_OCPP ? 1 : 0), RFID_READER},
    {"EV METER","Type of EV electric meter",                          0, EM_CUSTOM, EV_METER},
    {"EV ADDR", "Address of EV electric meter",                       MIN_METER_ADDRESS, MAX_METER_ADDRESS, EV_METER_ADDRESS},

    // System configuration
    /* LCD,       Desc,                                                 Min, Max, Default */
    {"MODE",    "Normal, Smart or Solar EVSE mode",                   0, 2, MODE},
    {"CIRCUIT", "EVSE Circuit max Current",                           10, 160, MAX_CIRCUIT},
    {"GRID",    "Grid type to which the Sensorbox is connected",      0, 1, GRID},
    {"SB2 WIFI","Connect Sensorbox-2 to WiFi",                        0, 2, SB2_WIFI_MODE},
    {"MAINS",   "Max MAINS Current (per phase)",                      10, 200, MAX_MAINS},
    {"START",   "Surplus energy start Current (sum of phases)",       0, 48, START_CURRENT},
    {"STOP",    "Stop solar charging at 6A after this time",          0, 60, STOP_TIME},
    {"IMPORT",  "Allow grid power when solar charging (sum of phase)",0, 48, IMPORT_CURRENT},
    {"MAINS MET","Type of mains electric meter",                       0, EM_CUSTOM, MAINS_METER},
    {"MAINS ADR","Address of mains electric meter",                    MIN_METER_ADDRESS, MAX_METER_ADDRESS, MAINS_METER_ADDRESS},
    {"BYTE ORD","Byte order of custom electric meter",                0, 3, EMCUSTOM_ENDIANESS},
    {"DATA TYPE","Data type of custom electric meter",                 0, MB_DATATYPE_MAX - 1, EMCUSTOM_DATATYPE},
    {"FUNCTION","Modbus Function of custom electric meter",           3, 4, EMCUSTOM_FUNCTION},
    {"VOL REGI","Register for Voltage (V) of custom electric meter",  0, 65530, EMCUSTOM_UREGISTER},
    {"VOL DIVI","Divisor for Voltage (V) of custom electric meter",   0, 7, EMCUSTOM_UDIVISOR},
    {"CUR REGI","Register for Current (A) of custom electric meter",  0, 65530, EMCUSTOM_IREGISTER},
    {"CUR DIVI","Divisor for Current (A) of custom electric meter",   0, 7, EMCUSTOM_IDIVISOR},
    {"POW REGI","Register for Power (W) of custom electric meter",    0, 65534, EMCUSTOM_PREGISTER},
    {"POW DIVI","Divisor for Power (W) of custom electric meter",     0, 7, EMCUSTOM_PDIVISOR},
    {"ENE REGI","Register for Energy (kWh) of custom electric meter", 0, 65534, EMCUSTOM_EREGISTER},
    {"ENE DIVI","Divisor for Energy (kWh) of custom electric meter",  0, 7, EMCUSTOM_EDIVISOR},
    {"READ MAX","Max register read at once of custom electric meter", 3, 255, 3},
    {"WIFI",    "Connect SmartEVSE to WiFi",                          0, 2, WIFI_MODE},
    {"AUTOUPDAT","Automatic Firmware Update",                         0, 1, AUTOUPDATE},
    {"CONTACT 2","Contactor2 (C2) behaviour",                          0, sizeof(StrEnableC2) / sizeof(StrEnableC2[0])-1, ENABLE_C2},
    {"MAX TEMP","Maximum temperature for the EVSE module",            40, 75, MAX_TEMPERATURE},
    {"CAPACITY","Capacity Rate limit on sum of MAINS Current (A)",    0, 600, MAX_SUMMAINS},
    {"CAP STOP","Stop Capacity Rate limit charging after X minutes",    0, 60, MAX_SUMMAINSTIME},
    {"", "Hold 2 sec to stop charging", 0, 0, 0},
    {"", "Hold 2 sec to start charging", 0, 0, 0},

    {"EXIT", "EXIT", 0, 0, 0}
};


struct DelayedTimeStruct {
    uint32_t epoch2;        // in case of Delayed Charging the StartTime in epoch2; if zero we are NOT Delayed Charging
                            // epoch2 is the number of seconds since 1/1/2023 00:00 UTC, which equals epoch 1672531200
                            // we avoid using epoch so we don't need expensive 64bits arithmetics with difftime
                            // and we can store dates until 7/2/2159
    int32_t diff;           // StartTime minus current time in seconds
};

#define EPOCH2_OFFSET 1672531200

extern struct DelayedTimeStruct DelayedStartTime;

void read_settings();
void write_settings(void);
void setSolarStopTimer(uint16_t Timer);
void setState(uint8_t NewState);
void setAccess(bool Access);
void setOverrideCurrent(uint16_t Current);
void SetCPDuty(uint32_t DutyCycle);
uint8_t setItemValue(uint8_t nav, uint16_t val);
uint16_t getItemValue(uint8_t nav);
void ConfigureModbusMode(uint8_t newmode);

void setMode(uint8_t NewMode) ;

#if ENABLE_OCPP
void ocppUpdateRfidReading(const unsigned char *uuid, size_t uuidLen);
bool ocppIsConnectorPlugged();

bool ocppHasTxNotification();
MicroOcpp::TxNotification ocppGetTxNotification();

bool ocppLockingTxDefined();
#endif //ENABLE_OCPP

#if SMARTEVSE_VERSION >= 40
// Pin definitions
#define PIN_QCA700X_INT 9           // SPI connections to QCA7000X
#define PIN_QCA700X_CS 11           // on ESP-S3 with OCTAL flash/PSRAM, GPIO pins 33-37 can not be used!
#define SPI_MOSI 13
#define SPI_MISO 12
#define SPI_SCK 10
#define PIN_QCA700X_RESETN 45

#define USART_TX 43                 // comm bus to mainboard
#define USART_RX 44

#define BUTTON1 0                   // Navigation buttons
//#define BUTTON2 1                   // renamed from prototype!
#define BUTTON3 2

#define RTC_SDA 6                   // RTC interface
#define RTC_SCL 7
#define RTC_INT 16

// New top board
#define WCH_NRST 8                  // microcontroller program interface
#define WCH_SWDIO 17                // unconnected!!! pin on 16pin connector is used for LCD power
#define WCH_SWCLK 18

// Old prototype top board
//#define WCH_NRST 18                  // microcontroller program interface
//#define WCH_SWDIO 8
//#define WCH_SWCLK 17

#define LCD_SDA 38                  // LCD interface
#define LCD_SCK 39
#define LCD_LED 41
#define LCD_CS 1

#define LCD_CHANNEL 5               // PWM channel


// RTC power sources
#define BATTERY 0x0C                // Trickle charger (TCE) disabled, Level Switching Mode (LSM) enabled.
#define SUPERCAP 0x24               // Trickle charger (TCE) enabled, Direct Switching Mode (DSM) enabled.




// ESP-WCH Communication States
#define COMM_OFF 0
#define COMM_VER_REQ 1              // Version Reqest           ESP -> WCH
#define COMM_VER_RSP 2              // Version Response         ESP <- WCH
#define COMM_CONFIG_SET 3           // Configuration Set        ESP -> WCH
#define COMM_CONFIG_CNF 4           // Configuration confirm.   ESP <- WCH
#define COMM_STATUS_REQ 5           // Status Request
#define COMM_STATUS_RSP 6           // Status Response

/*====================================================================*
 *   SPI registers QCA700X
 *--------------------------------------------------------------------*/

#define QCA7K_SPI_READ (1 << 15)                // MSB(15) of each command (16 bits) is the read(1) or write(0) bit.
#define QCA7K_SPI_WRITE (0 << 15)
#define QCA7K_SPI_INTERNAL (1 << 14)            // MSB(14) sets the Internal Registers(1) or Data Buffer(0)
#define QCA7K_SPI_EXTERNAL (0 << 14)

#define	SPI_REG_BFR_SIZE        0x0100
#define SPI_REG_WRBUF_SPC_AVA   0x0200
#define SPI_REG_RDBUF_BYTE_AVA  0x0300
#define SPI_REG_SPI_CONFIG      0x0400
#define SPI_REG_INTR_CAUSE      0x0C00
#define SPI_REG_INTR_ENABLE     0x0D00
#define SPI_REG_RDBUF_WATERMARK 0x1200
#define SPI_REG_WRBUF_WATERMARK 0x1300
#define SPI_REG_SIGNATURE       0x1A00
#define SPI_REG_ACTION_CTRL     0x1B00

#define QCASPI_GOOD_SIGNATURE   0xAA55
#define QCA7K_BUFFER_SIZE       3163

#define SPI_INT_WRBUF_BELOW_WM (1 << 10)
#define SPI_INT_CPU_ON         (1 << 6)
#define SPI_INT_ADDR_ERR       (1 << 3)
#define SPI_INT_WRBUF_ERR      (1 << 2)
#define SPI_INT_RDBUF_ERR      (1 << 1)
#define SPI_INT_PKT_AVLBL      (1 << 0)

/*====================================================================*
 *   Modem States
 *--------------------------------------------------------------------*/

#define MODEM_POWERUP 0
#define MODEM_WRITESPACE 1
#define MODEM_CM_SET_KEY_REQ 2
#define MODEM_CM_SET_KEY_CNF 3
#define MODEM_CONFIGURED 10
#define SLAC_PARAM_REQ 20
#define SLAC_PARAM_CNF 30
#define MNBC_SOUND 40
#define ATTEN_CHAR_IND 50
#define ATTEN_CHAR_RSP 60
#define SLAC_MATCH_REQ 70

#define MODEM_LINK_STATUS 80
#define MODEM_WAIT_LINK 90
#define MODEM_GET_SW_REQ 100
#define MODEM_WAIT_SW 110
#define MODEM_LINK_READY 120


/*====================================================================*
 *   SLAC commands
 *--------------------------------------------------------------------*/

#define CM_SET_KEY 0x6008
#define CM_GET_KEY 0x600C
#define CM_SC_JOIN 0x6010
#define CM_CHAN_EST 0x6014
#define CM_TM_UPDATE 0x6018
#define CM_AMP_MAP 0x601C
#define CM_BRG_INFO 0x6020
#define CM_CONN_NEW 0x6024
#define CM_CONN_REL 0x6028
#define CM_CONN_MOD 0x602C
#define CM_CONN_INFO 0x6030
#define CM_STA_CAP 0x6034
#define CM_NW_INFO 0x6038
#define CM_GET_BEACON 0x603C
#define CM_HFID 0x6040
#define CM_MME_ERROR 0x6044
#define CM_NW_STATS 0x6048
#define CM_SLAC_PARAM 0x6064
#define CM_START_ATTEN_CHAR 0x6068
#define CM_ATTEN_CHAR 0x606C
#define CM_PKCS_CERT 0x6070
#define CM_MNBC_SOUND 0x6074
#define CM_VALIDATE 0x6078
#define CM_SLAC_MATCH 0x607C
#define CM_SLAC_USER_DATA 0x6080
#define CM_ATTEN_PROFILE 0x6084
#define CM_GET_SW 0xA000
#define CM_LINK_STATUS 0xA0B8

#define MMTYPE_REQ 0x0000   // request
#define MMTYPE_CNF 0x0001   // confirmation = +1
#define MMTYPE_IND 0x0002
#define MMTYPE_RSP 0x0003

// Frametypes

#define FRAME_IPV6 0x86DD
#define FRAME_HOMEPLUG 0x88E1

/* V2GTP */
#define V2GTP_HEADER_SIZE 8 /* header has 8 bytes */

struct rtcTime {
    uint8_t Status;
    uint8_t Hour;
    uint8_t Minute;
    uint8_t Second;
    uint8_t Date;
    uint8_t Month;
    uint16_t Year;
} ;

#endif //SMARTEVSE_VERSION

#endif
