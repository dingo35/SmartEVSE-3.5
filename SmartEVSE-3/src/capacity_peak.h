/*
 * capacity_peak.h — 15-minute quarter-peak averaging for EU capacity tariffs
 *
 * Pure C module — no platform dependencies. Tracks rolling 15-minute average
 * power consumption, records monthly peaks, and calculates headroom for the
 * Belgian capaciteitstarief (and similar EU capacity tariff structures).
 *
 * Architecture: Called once per second with total mains power. Maintains a
 * 15-minute sliding window of accumulated watt-seconds. At window completion,
 * computes average power and updates monthly peak if new high.
 */

#ifndef CAPACITY_PEAK_H
#define CAPACITY_PEAK_H

#include <stdint.h>
#include <stddef.h>

#define CAPACITY_WINDOW_SECONDS 900  /* 15 minutes */

#ifdef __cplusplus
extern "C" {
#endif

/* 15-minute window state */
typedef struct {
    int64_t  accumulated_ws;     /* Watt-seconds accumulated in current window */
    uint16_t window_elapsed_s;   /* Seconds elapsed in current 15-min window */
    int32_t  window_avg_w;       /* Average power (W) for last completed window */
    uint8_t  window_valid;       /* 1 if at least one window completed */
} capacity_window_t;

/* Monthly peak tracking */
typedef struct {
    int32_t  monthly_peak_w;     /* Highest 15-min avg this month (W) */
    uint8_t  peak_month;         /* Month number (1-12) of current tracking */
    uint8_t  peak_year_offset;   /* Year - 2024 */
} capacity_monthly_t;

/* Full capacity tariff state */
typedef struct {
    capacity_window_t window;
    capacity_monthly_t monthly;
    int32_t  limit_w;            /* User-configured capacity limit (W), 0=disabled */
} capacity_state_t;

/* Initialize capacity state — zeroes all fields. */
void capacity_init(capacity_state_t *state);

/* Set the capacity limit in watts. 0 = disabled (no constraint). */
void capacity_set_limit(capacity_state_t *state, int32_t limit_w);

/*
 * Called every second with total mains power (sum of all phases, watts).
 * Accumulates watt-seconds, completes windows every 900s, and tracks
 * monthly peaks. Month (1-12) and year_offset (year - 2024) are used
 * to detect month rollovers and reset the monthly peak.
 */
void capacity_tick_1s(capacity_state_t *state, int32_t total_power_w,
                      uint8_t month, uint8_t year_offset);

/*
 * Get current headroom — how many more watts can be consumed in the
 * remainder of this 15-minute window without exceeding the limit.
 * Returns INT32_MAX if limit is disabled (limit_w == 0).
 */
int32_t capacity_get_headroom_w(const capacity_state_t *state);

/* Get last completed window average power in watts. */
int32_t capacity_get_window_avg_w(const capacity_state_t *state);

/* Get this month's peak 15-minute average power in watts. */
int32_t capacity_get_monthly_peak_w(const capacity_state_t *state);

/*
 * Convert headroom in watts to current headroom in deciamps for N phases.
 * Returns 0 if phases == 0. Uses 230V nominal voltage.
 */
int16_t capacity_headroom_to_da(int32_t headroom_w, uint8_t phases);

/*
 * Format capacity state as JSON into buf.
 * Returns the number of bytes written (excluding NUL), or -1 on error
 * (NULL state, NULL buf, or bufsz == 0).
 */
int capacity_to_json(const capacity_state_t *state, char *buf, size_t bufsz);

#ifdef __cplusplus
}
#endif

#endif /* CAPACITY_PEAK_H */
