/*
 * diag_sampler.cpp - Diagnostic telemetry firmware integration
 *
 * Bridges firmware globals into diag_snapshot_t and pushes them
 * into the static ring buffer.  Call diag_sample() from timer1s.
 */

#if defined(SMARTEVSE_VERSION)  /* ESP32 firmware only */

#include "main.h"
#include "esp32.h"
#include "diag_sampler.h"
#include "diag_modbus.h"
#include "evse_bridge.h"
#include "meter.h"
#include "debug.h"

#include <WiFi.h>
#include <string.h>

/* Extern globals not exposed via headers */
extern uint8_t  State;
extern uint8_t  ErrorFlags;
extern uint8_t  ChargeDelay;
extern uint8_t  Mode;
extern uint16_t ChargeCurrent;
extern int16_t  IsetBalanced;
extern uint16_t OverrideCurrent;
extern uint16_t SolarStopTimer;
extern uint16_t ImportCurrent;
extern uint16_t StartCurrent;
extern uint8_t  NoCurrent;
extern uint8_t  C1Timer;
extern uint8_t  AccessTimer;
extern uint8_t  Nr_Of_Phases_Charging;
extern uint8_t  LoadBl;
extern uint16_t Balanced[];
extern uint8_t  BalancedState[];
extern int8_t   TempEVSE;
extern uint8_t  RCmon;
extern uint8_t  pilot;
extern int16_t  Isum;
extern EnableC2_t EnableC2;
extern AccessStatus_t AccessStatus;
extern Switch_Phase_t Switching_Phases_C2;
extern uint32_t serialnr;

#if MQTT
extern struct MQTTclient_t MQTTclient;
extern struct MQTTclientSmartEVSE_t MQTTclientSmartEVSE;
#endif

/* Static ring buffer — 8 KB at default 128 slots */
static diag_snapshot_t diag_buffer[DIAG_RING_SIZE_DEFAULT];
static diag_ring_t     diag_ring;
static uint32_t        diag_uptime_seconds;

void diag_sampler_init(void)
{
    diag_ring_init(&diag_ring, diag_buffer, DIAG_RING_SIZE_DEFAULT);
    diag_uptime_seconds = 0;
}

diag_ring_t *diag_get_ring(void)
{
    return &diag_ring;
}

extern diag_mb_ring_t g_diag_mb_ring;

void diag_start(diag_profile_t profile)
{
    diag_ring_reset(&diag_ring);
    diag_set_profile(&diag_ring, profile);
    diag_ring.start_time = diag_uptime_seconds;

    /* Enable Modbus event ring for MODBUS/FAST profiles */
    bool mb_enable = (profile == DIAG_PROFILE_MODBUS || profile == DIAG_PROFILE_FAST);
    diag_mb_enable(&g_diag_mb_ring, mb_enable);
    if (mb_enable)
        diag_mb_reset(&g_diag_mb_ring);
}

void diag_stop(void)
{
    /* Reset the profile to OFF so /diag/status reports the capture as
     * stopped. Without this the ring stays "general/solar/loadbal/..."
     * (just frozen), and the web UI's auto-resume on page load
     * (app.js: `if (d.profile && d.profile !== 'off') diagConnectWs()`)
     * keeps re-attaching the WebSocket after every Stop press until
     * the device reboots and diag_sampler_init() clears the ring.
     * Freeze stays for symmetry — diag_start()'s diag_ring_reset()
     * clears it back to false on the next capture. */
    diag_set_profile(&diag_ring, DIAG_PROFILE_OFF);
    diag_ring_freeze(&diag_ring, true);
    diag_mb_enable(&g_diag_mb_ring, false);
}

/* Internal: fill a snapshot from current globals */
static void diag_fill_snapshot(diag_snapshot_t *snap)
{
    memset(snap, 0, sizeof(*snap));

    snap->timestamp = diag_uptime_seconds;

    /* State machine */
    snap->state        = State;
    snap->error_flags  = (uint8_t)(ErrorFlags & 0xFF);
    snap->charge_delay = ChargeDelay;
    snap->access_status = (uint8_t)AccessStatus;
    snap->mode         = Mode;

    /* Currents */
    snap->mains_irms[0] = MainsMeter.Irms[0];
    snap->mains_irms[1] = MainsMeter.Irms[1];
    snap->mains_irms[2] = MainsMeter.Irms[2];
    snap->ev_irms[0]    = EVMeter.Irms[0];
    snap->ev_irms[1]    = EVMeter.Irms[1];
    snap->ev_irms[2]    = EVMeter.Irms[2];
    snap->isum          = Isum;

    /* Power allocation */
    snap->charge_current  = ChargeCurrent;
    snap->iset_balanced   = (int16_t)IsetBalanced;
    snap->override_current = OverrideCurrent;

    /* Solar */
    snap->solar_stop_timer = SolarStopTimer;
    snap->import_current   = ImportCurrent;
    snap->start_current    = StartCurrent;

    /* Timers — StateTimer is internal to evse_ctx_t, read via bridge */
    evse_bridge_lock();
    snap->state_timer = g_evse_ctx.StateTimer;
    evse_bridge_unlock();
    snap->c1_timer      = C1Timer;
    snap->access_timer  = AccessTimer;
    snap->no_current    = NoCurrent;

    /* Phase switching */
    snap->nr_phases_charging = Nr_Of_Phases_Charging;
    snap->switching_c2       = (uint8_t)Switching_Phases_C2;
    snap->enable_c2          = (uint8_t)EnableC2;

    /* Load balancing */
    snap->load_bl         = LoadBl;
    snap->balanced_state_0 = BalancedState[0];
    snap->balanced_0       = Balanced[0];

    /* Temperature & safety */
    snap->temp_evse     = TempEVSE;
    snap->rc_mon        = RCmon;
    snap->pilot_reading = pilot;

    /* Modbus health */
#if !defined(SMARTEVSE_VERSION) || SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40
    snap->mains_meter_timeout = MainsMeter.Timeout;
    snap->ev_meter_timeout    = EVMeter.Timeout;
#endif
    snap->mains_meter_type = MainsMeter.Type;
    snap->ev_meter_type    = EVMeter.Type;

    /* Network */
    snap->wifi_rssi = WiFi.isConnected() ? (int8_t)WiFi.RSSI() : 0;
#if MQTT
    snap->mqtt_connected = (MQTTclient.connected ? 1 : 0)
                         | (MQTTclientSmartEVSE.connected ? 2 : 0);
#endif
}

void diag_sample(void)
{
    diag_uptime_seconds++;

    /* Skip if active profile is FAST/MODBUS — those are sampled from Timer100ms */
    if (diag_ring.profile == DIAG_PROFILE_MODBUS ||
        diag_ring.profile == DIAG_PROFILE_FAST)
        return;

    if (!diag_ring_tick(&diag_ring))
        return;

    diag_snapshot_t snap;
    diag_fill_snapshot(&snap);
    diag_ring_push(&diag_ring, &snap);
    diag_ws_push_snapshot(&snap);
}

void diag_sample_fast(void)
{
    /* Only sample in MODBUS/FAST profiles (10 Hz from Timer100ms) */
    if (diag_ring.profile != DIAG_PROFILE_MODBUS &&
        diag_ring.profile != DIAG_PROFILE_FAST)
        return;

    if (!diag_ring_tick(&diag_ring))
        return;

    diag_snapshot_t snap;
    diag_fill_snapshot(&snap);
    diag_ring_push(&diag_ring, &snap);
    diag_ws_push_snapshot(&snap);
}

int diag_status_json(char *buf, size_t bufsz)
{
    if (!buf || bufsz == 0)
        return -1;

    const char *profile_names[] = {
        "off", "general", "solar", "loadbal", "modbus", "fast"
    };
    const char *pname = "off";
    if (diag_ring.profile >= DIAG_PROFILE_OFF &&
        diag_ring.profile <= DIAG_PROFILE_FAST)
        pname = profile_names[diag_ring.profile];

    int n = snprintf(buf, bufsz,
        "{\"profile\":\"%s\",\"count\":%u,\"capacity\":%u,"
        "\"frozen\":%s,\"uptime\":%u}",
        pname,
        (unsigned)diag_ring.count,
        (unsigned)diag_ring.capacity,
        diag_ring.frozen ? "true" : "false",
        (unsigned)diag_uptime_seconds);

    if (n < 0 || (size_t)n >= bufsz)
        return -1;

    return n;
}

#endif /* SMARTEVSE_VERSION */
