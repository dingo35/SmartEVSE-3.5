/*
 * session_log.c — Charge session tracking for ERE certificate reporting
 *
 * Pure C implementation — no platform dependencies.
 * Maintains a single active session and a single last-completed session.
 */

#include "session_log.h"
#include <stdio.h>
#include <string.h>

/* Internal state */
static session_record_t s_active;       /* Session currently in progress */
static session_record_t s_last;         /* Last completed session */
static uint8_t s_session_active;        /* 1 if session in progress */
static uint8_t s_has_last;              /* 1 if s_last contains valid data */
static uint32_t s_next_id;             /* Next session ID to assign */

void session_init(void) {
    memset(&s_active, 0, sizeof(s_active));
    memset(&s_last, 0, sizeof(s_last));
    s_session_active = 0;
    s_has_last = 0;
    s_next_id = 1;
}

void session_start(uint32_t timestamp, int32_t start_energy_wh, uint8_t mode) {
    /* Reject garbage timestamps before NTP has synced */
    if (timestamp < SESSION_MIN_VALID_TIME) {
        return;
    }
    /* Discard any active session — caller's responsibility to end first */
    memset(&s_active, 0, sizeof(s_active));
    s_active.session_id = s_next_id++;
    s_active.start_time = timestamp;
    s_active.start_energy_wh = start_energy_wh;
    s_active.mode = mode;
    s_session_active = 1;
}

void session_end(uint32_t timestamp, int32_t end_energy_wh,
                 uint16_t max_current_da, uint8_t phases) {
    if (!s_session_active) {
        return; /* No active session — ignore */
    }

    s_active.end_time = timestamp;

    /* Discard sessions shorter than minimum duration (rapid reconnect filter) */
    if ((timestamp - s_active.start_time) < SESSION_MIN_DURATION_S) {
        s_session_active = 0;
        memset(&s_active, 0, sizeof(s_active));
        return;
    }

    s_active.end_energy_wh = end_energy_wh;
    s_active.energy_charged_wh = end_energy_wh - s_active.start_energy_wh;
    s_active.max_current_da = max_current_da;
    s_active.phases = phases;

    /* Move to last-completed slot */
    memcpy(&s_last, &s_active, sizeof(s_last));
    s_has_last = 1;
    s_session_active = 0;
    memset(&s_active, 0, sizeof(s_active));
}

void session_set_ocpp_id(uint32_t ocpp_transaction_id) {
    if (!s_session_active) {
        return;
    }
    s_active.session_id = ocpp_transaction_id;
    s_active.ocpp_active = 1;
}

void session_set_circuit_energy(int32_t start_wh, int32_t end_wh) {
    if (!s_session_active) {
        return;
    }
    if (start_wh != 0) {
        s_active.circuit_start_energy_wh = start_wh;
    }
    if (end_wh != 0) {
        s_active.circuit_end_energy_wh = end_wh;
        s_active.circuit_energy_wh = end_wh - s_active.circuit_start_energy_wh;
    }
}

uint8_t session_is_active(void) {
    return s_session_active;
}

const session_record_t *session_get_last(void) {
    if (!s_has_last) {
        return NULL;
    }
    return &s_last;
}

/*
 * Format epoch seconds as ISO 8601 UTC string: "YYYY-MM-DDThh:mm:ssZ"
 * Minimal implementation — no mktime/gmtime dependency for portability.
 */
static void epoch_to_iso8601(uint32_t epoch, char *buf, size_t bufsz) {
    if (bufsz < 21) {
        if (bufsz > 0) buf[0] = '\0';
        return;
    }

    /* Days since 1970-01-01 */
    uint32_t secs = epoch;
    uint32_t sec_of_day = secs % 86400;
    uint32_t days = secs / 86400;

    uint32_t hour = sec_of_day / 3600;
    uint32_t min  = (sec_of_day % 3600) / 60;
    uint32_t sec  = sec_of_day % 60;

    /* Civil date from day count (Euclidean affine algorithm) */
    /* https://howardhinnant.github.io/date_algorithms.html */
    days += 719468;
    uint32_t era = days / 146097;
    uint32_t doe = days - era * 146097;                          /* [0, 146096] */
    uint32_t yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365; /* [0, 399] */
    uint32_t y   = yoe + era * 400;
    uint32_t doy = doe - (365*yoe + yoe/4 - yoe/100);           /* [0, 365] */
    uint32_t mp  = (5*doy + 2) / 153;                           /* [0, 11] */
    uint32_t d   = doy - (153*mp + 2) / 5 + 1;                  /* [1, 31] */
    uint32_t m   = mp < 10 ? mp + 3 : mp - 9;                   /* [1, 12] */
    if (m <= 2) y++;

    snprintf(buf, bufsz, "%04u-%02u-%02uT%02u:%02u:%02uZ",
             (unsigned)y, (unsigned)m, (unsigned)d,
             (unsigned)hour, (unsigned)min, (unsigned)sec);
}

static const char *mode_string(uint8_t mode) {
    switch (mode) {
        case 0:  return "normal";
        case 1:  return "smart";
        case 2:  return "solar";
        default: return "unknown";
    }
}

int session_to_json(const session_record_t *rec, char *buf, size_t bufsz) {
    if (!rec || !buf || bufsz == 0) {
        return -1;
    }

    char start_iso[24];
    char end_iso[24];
    epoch_to_iso8601(rec->start_time, start_iso, sizeof(start_iso));
    epoch_to_iso8601(rec->end_time, end_iso, sizeof(end_iso));

    /* kWh with 3 decimal places from Wh */
    int32_t kwh_int = rec->energy_charged_wh / 1000;
    int32_t kwh_frac = rec->energy_charged_wh % 1000;
    if (kwh_frac < 0) kwh_frac = -kwh_frac;

    /* max_current in amps with 1 decimal from deciamps */
    uint16_t amps_int = rec->max_current_da / 10;
    uint16_t amps_frac = rec->max_current_da % 10;

    /* Format ocpp_tx_id: numeric ID when OCPP active, null otherwise */
    char ocpp_field[16];
    if (rec->ocpp_active) {
        snprintf(ocpp_field, sizeof(ocpp_field), "%u", (unsigned)rec->session_id);
    } else {
        snprintf(ocpp_field, sizeof(ocpp_field), "null");
    }

    int n = snprintf(buf, bufsz,
        "{\"session_id\":%u,"
        "\"start\":\"%s\","
        "\"end\":\"%s\","
        "\"kwh\":%d.%03d,"
        "\"start_energy_wh\":%d,"
        "\"end_energy_wh\":%d,"
        "\"max_current_a\":%u.%u,"
        "\"phases\":%u,"
        "\"mode\":\"%s\","
        "\"ocpp_tx_id\":%s",
        (unsigned)rec->session_id,
        start_iso,
        end_iso,
        (int)kwh_int, (int)kwh_frac,
        (int)rec->start_energy_wh,
        (int)rec->end_energy_wh,
        (unsigned)amps_int, (unsigned)amps_frac,
        (unsigned)rec->phases,
        mode_string(rec->mode),
        ocpp_field);

    if (n < 0 || (size_t)n >= bufsz) {
        return -1;
    }

    /* Append circuit_kwh when CircuitMeter was active */
    if (rec->circuit_energy_wh != 0) {
        int32_t ckwh_int = rec->circuit_energy_wh / 1000;
        int32_t ckwh_frac = rec->circuit_energy_wh % 1000;
        if (ckwh_frac < 0) ckwh_frac = -ckwh_frac;

        int extra = snprintf(buf + n, bufsz - (size_t)n,
            ",\"circuit_kwh\":%d.%03d}",
            (int)ckwh_int, (int)ckwh_frac);
        if (extra < 0 || (size_t)(n + extra) >= bufsz) {
            return -1;
        }
        n += extra;
    } else {
        /* Close the JSON object */
        if ((size_t)n + 1 >= bufsz) {
            return -1;
        }
        buf[n] = '}';
        n++;
        buf[n] = '\0';
    }

    return n;
}
