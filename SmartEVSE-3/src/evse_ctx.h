/*
 * evse_ctx.h - Shared EVSE context struct, HAL typedefs, and constants
 *
 * This header is the single source of truth for the state machine module's
 * types and constants. It is included by both the firmware build and the
 * native test suite.
 *
 * Each constant block is wrapped with #ifndef guards so that firmware
 * headers (main.h, main_c.h) can coexist without redefinition errors.
 */

#ifndef EVSE_CTX_H
#define EVSE_CTX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifndef NR_EVSES
#define NR_EVSES 8
#endif

// ---- States (from main_c.h) ----
#ifndef STATE_A
#define STATE_A             0
#define STATE_B             1
#define STATE_C             2
#define STATE_D             3
#define STATE_COMM_B        4
#define STATE_COMM_B_OK     5
#define STATE_COMM_C        6
#define STATE_COMM_C_OK     7
#define STATE_ACTSTART      8
#define STATE_B1            9
#define STATE_C1           10
#define STATE_MODEM_REQUEST 11
#define STATE_MODEM_WAIT   12
#define STATE_MODEM_DONE   13
#define STATE_MODEM_DENIED 14
#define NOSTATE           255
#endif

// ---- Modes (from main.h) ----
#ifndef MODE_NORMAL
#define MODE_NORMAL  0
#define MODE_SMART   1
#define MODE_SOLAR   2
#endif

// ---- Error flags (from main_c.h) ----
#ifndef NO_ERROR
#define NO_ERROR     0
#define LESS_6A      1
#define CT_NOCOMM    2
#define TEMP_HIGH    4
#define EV_NOCOMM    8
#define RCM_TRIPPED 16
#define RCM_TEST    32
#define Test_IO     64
#define BL_FLASH   128
#define NO_SUN     256
#endif

// ---- Priority scheduling constants ----
#ifndef PRIO_MODBUS_ADDR
#define PRIO_MODBUS_ADDR      0
#define PRIO_FIRST_CONNECTED  1
#define PRIO_LAST_CONNECTED   2
#endif

#ifndef SCHED_INACTIVE
#define SCHED_INACTIVE  0
#define SCHED_ACTIVE    1
#define SCHED_PAUSED    2
#endif

#define IDLE_CURRENT_THRESHOLD  10  /* 1.0A in deciamps */

// ---- PWM constants ----
#ifndef PWM_5
#define PWM_5   50
#define PWM_95 950
#define PWM_96 960
#define PWM_100 1000
#endif

// ---- Pilot voltage levels (as returned by pilot reading) ----
#ifndef PILOT_12V
#define PILOT_12V   12
#define PILOT_9V     9
#define PILOT_6V     6
#define PILOT_DIODE  1
#define PILOT_SHORT  255
#define PILOT_NOK    0
#endif

// ---- Phase switching states (from main.h Switch_Phase_t) ----
// In C++ firmware builds, these are enum values in Switch_Phase_t (main.h).
// Only define as macros for pure C builds (module, tests).
#ifndef __cplusplus
#ifndef NO_SWITCH
#define NO_SWITCH             0
#define GOING_TO_SWITCH_1P    1
#define GOING_TO_SWITCH_3P    2
#endif
#endif

// ---- Access status ----
// In firmware this is an enum in main.h; we only define it if not already present.
#ifndef EVSE_CTX_ACCESS_DEFINED
#ifndef __cplusplus
typedef enum { OFF = 0, ON = 1, PAUSE = 2 } AccessStatus_t;
#else
// In C++ firmware builds, AccessStatus_t is already defined as an enum in main.h
#endif
#define EVSE_CTX_ACCESS_DEFINED
#endif

// ---- EnableC2 values (from main.h) ----
// Firmware: enum EnableC2_t { NOT_PRESENT, ALWAYS_OFF, SOLAR_OFF, ALWAYS_ON, AUTO };
#ifndef EVSE_CTX_ENABLEC2_DEFINED
#ifndef __cplusplus
typedef enum {
    NOT_PRESENT = 0,
    ALWAYS_OFF  = 1,
    SOLAR_OFF   = 2,
    ALWAYS_ON   = 3,
    AUTO        = 4
} EnableC2_t;
#endif
#define EVSE_CTX_ENABLEC2_DEFINED
#endif

// ---- Default config values (from main.h) ----
#ifndef MAX_MAINS
#define MAX_MAINS          25
#endif
#ifndef MAX_CURRENT
#define MAX_CURRENT        13
#endif
#ifndef MIN_CURRENT
#define MIN_CURRENT         6
#endif
#ifndef MAX_CIRCUIT
#define MAX_CIRCUIT        16
#endif
#ifndef MAX_SUMMAINS
#define MAX_SUMMAINS        0
#endif
#ifndef MAX_SUMMAINSTIME
#define MAX_SUMMAINSTIME    0
#endif
#ifndef START_CURRENT
#define START_CURRENT       4
#endif
#ifndef STOP_TIME
#define STOP_TIME          10
#endif
#ifndef IMPORT_CURRENT
#define IMPORT_CURRENT      0
#endif
#ifndef MAX_TEMPERATURE
#define MAX_TEMPERATURE    65
#endif
#ifndef CHARGEDELAY
#define CHARGEDELAY        60
#endif
#ifndef COMM_TIMEOUT
#define COMM_TIMEOUT       11
#endif
#ifndef COMM_EVTIMEOUT
#define COMM_EVTIMEOUT     64
#endif
#ifndef SOLARSTARTTIME
#define SOLARSTARTTIME     40
#endif
#ifndef RFIDLOCKTIME
#define RFIDLOCKTIME       60
#endif
#ifndef GRID_RELAY_MAX_SUMMAINS
#define GRID_RELAY_MAX_SUMMAINS 18
#endif

// ---- Node info (from main.h) ----
#ifndef EVSE_CTX_NODE_DEFINED
typedef struct {
    uint8_t  Online;
    uint8_t  ConfigChanged;
    uint8_t  EVMeter;
    uint8_t  EVAddress;
    uint8_t  MinCurrent;
    uint8_t  Phases;
    uint32_t Timer;
    uint32_t IntTimer;
    uint16_t SolarTimer;
    uint8_t  Mode;
} evse_node_t;
#define EVSE_CTX_NODE_DEFINED
#endif

// ---- HAL (Hardware Abstraction Layer) ----
typedef struct evse_hal {
    void (*set_cp_duty)(uint32_t duty_cycle);
    void (*contactor1)(bool on);
    void (*contactor2)(bool on);
    void (*set_pilot)(bool connected);
    void (*actuator_lock)(void);
    void (*actuator_unlock)(void);
    void (*actuator_off)(void);
    void (*on_state_change)(uint8_t old_state, uint8_t new_state);
} evse_hal_t;

// ---- The full EVSE state context ----
typedef struct {
    // --- Core state ---
    uint8_t State;

    // --- Operating config ---
    uint8_t Mode;
    uint8_t LoadBl;
    uint8_t Config;     // 0=Socket, 1=Fixed Cable

    // --- Authorization ---
    AccessStatus_t AccessStatus;
    uint8_t RFIDReader;
    bool    OcppMode;
    float   OcppCurrentLimit;
    bool    CPDutyOverride;

    // --- Power limits ---
    uint16_t MaxMains;
    uint16_t MaxCurrent;
    uint16_t MinCurrent;
    uint16_t MaxCircuit;
    uint16_t MaxCapacity;
    uint16_t MaxSumMains;
    uint8_t  MaxSumMainsTime;
    uint16_t GridRelayMaxSumMains;
    bool     GridRelayOpen;

    // --- Current distribution ---
    uint16_t Balanced[NR_EVSES];
    uint16_t BalancedMax[NR_EVSES];
    uint8_t  BalancedState[NR_EVSES];
    uint16_t BalancedError[NR_EVSES];
    uint16_t ChargeCurrent;
    int32_t  IsetBalanced;
    uint16_t OverrideCurrent;

    // --- Priority scheduling ---
    uint8_t  PrioStrategy;              /* PRIO_MODBUS_ADDR / PRIO_FIRST_CONNECTED / PRIO_LAST_CONNECTED */
    uint16_t RotationInterval;          /* 0=disabled, 30-1440 minutes */
    uint16_t IdleTimeout;               /* 30-300 seconds, doubles as anti-flap window */
    uint8_t  Priority[NR_EVSES];        /* Sorted EVSE indices by priority */
    uint32_t ConnectedTime[NR_EVSES];   /* Uptime seconds when EVSE entered STATE_C */
    uint16_t IdleTimer[NR_EVSES];       /* Seconds since activation (counts up) */
    uint16_t RotationTimer;             /* Countdown in seconds for rotation */
    uint8_t  ScheduleState[NR_EVSES];   /* SCHED_INACTIVE / SCHED_ACTIVE / SCHED_PAUSED */
    uint32_t Uptime;                    /* Monotonic seconds counter */

    // --- Meter readings ---
    int16_t  Isum;
    int16_t  MainsMeterIrms[3];
    int16_t  MainsMeterImeasured;
    int16_t  EVMeterIrms[3];
    int16_t  EVMeterImeasured;
    uint8_t  MainsMeterType;
    uint8_t  EVMeterType;
    uint8_t  MainsMeterTimeout;
    uint8_t  EVMeterTimeout;

    // --- Error handling ---
    uint8_t ErrorFlags;
    uint8_t ChargeDelay;
    uint8_t NoCurrent;

    // --- Timers ---
    uint16_t SolarStopTimer;
    uint16_t MaxSumMainsTimer;
    uint8_t  StateTimer;
    uint8_t  AccessTimer;
    uint8_t  C1Timer;

    // --- Phase switching ---
    EnableC2_t EnableC2;
    uint8_t Nr_Of_Phases_Charging;
    uint8_t Switching_Phases_C2;
    bool    phasesLastUpdateFlag;
    bool    LimitedByMaxSumMains;

    // --- Modem ---
    bool    ModemEnabled;
    uint8_t ModemStage;
    uint8_t ToModemWaitStateTimer;
    uint8_t ToModemDoneStateTimer;
    uint8_t LeaveModemDoneStateTimer;
    uint8_t LeaveModemDeniedStateTimer;
    int8_t  DisconnectTimeCounter;
    char    RequiredEVCCID[32];
    char    EVCCID[32];

    // --- Solar config ---
    uint16_t StartCurrent;
    uint16_t StopTime;
    uint16_t ImportCurrent;

    // --- Safety ---
    int8_t  TempEVSE;
    uint16_t maxTemp;
    uint8_t RCmon;
    bool    RCMFault;

    // --- Misc ---
    uint8_t DiodeCheck;
    bool    PilotDisconnected;
    uint8_t PilotDisconnectTime;
    uint8_t ActivationMode;
    uint8_t ActivationTimer;

    // --- Node tracking ---
    evse_node_t Node[NR_EVSES];

    // --- HAL ---
    evse_hal_t hal;

    // --- Test instrumentation (for assertions) ---
#ifdef EVSE_TESTING
    uint32_t last_pwm_duty;
    bool     contactor1_state;
    bool     contactor2_state;
    bool     pilot_connected;
    int      transition_count;
    uint8_t  transition_log[64];
#endif
} evse_ctx_t;

#ifdef __cplusplus
}
#endif

#endif // EVSE_CTX_H
