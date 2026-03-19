/*
 * diag_telemetry.h - Diagnostic telemetry ring buffer
 *
 * Pure C module — no platform dependencies, testable natively.
 * Captures compact state snapshots into a fixed-size circular buffer.
 */

#ifndef DIAG_TELEMETRY_H
#define DIAG_TELEMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Snapshot record — packed to minimize RAM.  Target: 64 bytes. */
typedef struct __attribute__((packed)) {
    uint32_t timestamp;           /* Uptime in seconds                       (4)  */
    /* State machine (5 bytes) */
    uint8_t  state;               /* EVSE state                              (1)  */
    uint8_t  error_flags;         /* ErrorFlags (low byte)                   (1)  */
    uint8_t  charge_delay;        /* ChargeDelay countdown                   (1)  */
    uint8_t  access_status;       /* OFF/ON/PAUSE                            (1)  */
    uint8_t  mode;                /* NORMAL/SMART/SOLAR                      (1)  */
    /* Currents (14 bytes) */
    int16_t  mains_irms[3];      /* Mains L1/L2/L3 in deciamps             (6)  */
    int16_t  ev_irms[3];         /* EV meter L1/L2/L3 in deciamps          (6)  */
    int16_t  isum;               /* Total current sum                       (2)  */
    /* Power allocation (6 bytes) */
    uint16_t charge_current;     /* Allocated charge current                (2)  */
    int16_t  iset_balanced;      /* IsetBalanced                            (2)  */
    uint16_t override_current;   /* OverrideCurrent                         (2)  */
    /* Solar (6 bytes) */
    uint16_t solar_stop_timer;   /* SolarStopTimer countdown                (2)  */
    uint16_t import_current;     /* ImportCurrent setting                   (2)  */
    uint16_t start_current;      /* StartCurrent setting                    (2)  */
    /* Timers (4 bytes) */
    uint8_t  state_timer;        /* StateTimer                              (1)  */
    uint8_t  c1_timer;           /* C1Timer                                 (1)  */
    uint8_t  access_timer;       /* AccessTimer                             (1)  */
    uint8_t  no_current;         /* NoCurrent counter                       (1)  */
    /* Phase switching (3 bytes) */
    uint8_t  nr_phases_charging; /* Nr_Of_Phases_Charging                   (1)  */
    uint8_t  switching_c2;       /* Switching_Phases_C2                     (1)  */
    uint8_t  enable_c2;          /* EnableC2 setting                        (1)  */
    /* Load balancing (4 bytes) */
    uint8_t  load_bl;            /* LoadBl                                  (1)  */
    uint8_t  balanced_state_0;   /* BalancedState[0] (local node)           (1)  */
    uint16_t balanced_0;         /* Balanced[0] (local allocation)          (2)  */
    /* Temperature & safety (3 bytes) */
    int8_t   temp_evse;          /* TempEVSE                                (1)  */
    uint8_t  rc_mon;             /* RCmon                                   (1)  */
    uint8_t  pilot_reading;      /* Last pilot voltage level                (1)  */
    /* Modbus health (4 bytes) */
    uint8_t  mains_meter_timeout; /* MainsMeter.Timeout                     (1)  */
    uint8_t  ev_meter_timeout;    /* EVMeter.Timeout                        (1)  */
    uint8_t  mains_meter_type;   /* MainsMeter.Type                         (1)  */
    uint8_t  ev_meter_type;      /* EVMeter.Type                            (1)  */
    /* Network (2 bytes) */
    int8_t   wifi_rssi;          /* WiFi RSSI (dBm), 0 if disconnected     (1)  */
    uint8_t  mqtt_connected;     /* Bitfield: bit0=user, bit1=SmartEVSE    (1)  */
    /* Padding to 64 bytes */
    uint8_t  _reserved[9];      /* Future use / alignment                   (9)  */
} diag_snapshot_t;

/* Capture profile — determines sampling rate and focus area */
typedef enum {
    DIAG_PROFILE_OFF     = 0,   /* No capture (default)           */
    DIAG_PROFILE_GENERAL = 1,   /* 1 snapshot/second, all fields  */
    DIAG_PROFILE_SOLAR   = 2,   /* 1 snapshot/second, solar focus */
    DIAG_PROFILE_LOADBAL = 3,   /* 1 snapshot/second, LB focus    */
    DIAG_PROFILE_MODBUS  = 4,   /* 1 snapshot/100ms, meter focus  */
    DIAG_PROFILE_FAST    = 5    /* 1 snapshot/100ms, all fields   */
} diag_profile_t;

/* Ring buffer configuration */
#define DIAG_RING_SIZE_DEFAULT   128  /* 128 * 64 = 8192 bytes (8 KB) */
#define DIAG_RING_SIZE_EXTENDED  256  /* 256 * 64 = 16384 bytes (16 KB) */

/* Binary file format */
#define DIAG_FILE_MAGIC      "EVSE"
#define DIAG_FILE_MAGIC_LEN  4
#define DIAG_FILE_VERSION    1

typedef struct __attribute__((packed)) {
    char     magic[4];           /* "EVSE"                    (4)  */
    uint8_t  version;            /* Format version = 1        (1)  */
    uint8_t  profile;            /* Capture profile           (1)  */
    uint16_t snapshot_size;      /* sizeof(diag_snapshot_t)   (2)  */
    uint16_t count;              /* Number of snapshots       (2)  */
    char     firmware_version[16]; /* Firmware version string (16) */
    uint32_t serial_nr;          /* Device serial number      (4)  */
    uint32_t start_uptime;       /* Uptime when capture began (4)  */
} diag_file_header_t;            /* Total: 34 bytes               */

/* Ring buffer state */
typedef struct {
    diag_snapshot_t *buffer;      /* Pointer to snapshot array          */
    uint16_t        capacity;     /* Number of slots                    */
    uint16_t        head;         /* Next write position                */
    uint16_t        count;        /* Number of valid entries            */
    diag_profile_t  profile;      /* Active capture profile             */
    uint8_t         sample_divider; /* Sample every N ticks             */
    uint8_t         tick_counter;   /* Internal counter for divider     */
    uint32_t        start_time;   /* Uptime when capture started        */
    bool            frozen;       /* True = stop capturing (for download) */
} diag_ring_t;

/*
 * API — pure C, no platform dependencies.
 * All functions are safe to call with NULL pointers (no-op / return 0).
 */

/* Initialize ring buffer with external storage. */
void diag_ring_init(diag_ring_t *ring, diag_snapshot_t *buf, uint16_t capacity);

/* Reset ring buffer — clears all entries, keeps capacity and buffer pointer. */
void diag_ring_reset(diag_ring_t *ring);

/* Push a snapshot into the ring.  Overwrites oldest when full.
 * No-op if ring is NULL, frozen, or profile is OFF. */
void diag_ring_push(diag_ring_t *ring, const diag_snapshot_t *snap);

/* Read up to max_count snapshots in chronological order into out[].
 * Returns the number of snapshots actually copied. */
uint16_t diag_ring_read(const diag_ring_t *ring, diag_snapshot_t *out,
                        uint16_t max_count);

/* Set capture profile (also sets sample_divider accordingly). */
void diag_set_profile(diag_ring_t *ring, diag_profile_t profile);

/* Freeze/unfreeze the ring buffer (frozen = no new pushes). */
void diag_ring_freeze(diag_ring_t *ring, bool freeze);

/* Check if a tick should trigger a sample (handles divider counting).
 * Returns true if a sample should be taken this tick. */
bool diag_ring_tick(diag_ring_t *ring);

/* Serialize ring buffer contents to binary .diag format.
 * Writes header + snapshots + CRC32 into buf.
 * fw_version and serial_nr are metadata for the header.
 * Returns bytes written, or 0 if buf is too small. */
size_t diag_ring_serialize(const diag_ring_t *ring, uint8_t *buf, size_t bufsz,
                           const char *fw_version, uint32_t serial_nr);

/* Compute CRC32 (ISO 3309 / zlib). */
uint32_t diag_crc32(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* DIAG_TELEMETRY_H */
