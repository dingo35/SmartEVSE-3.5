/*
 * capacity_peak.c — 15-minute quarter-peak averaging for EU capacity tariffs
 *
 * Pure C implementation — no platform dependencies.
 * Tracks rolling 15-minute average power and monthly peaks for the Belgian
 * capaciteitstarief model.
 */

#include "capacity_peak.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>

void capacity_init(capacity_state_t *state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
}

void capacity_set_limit(capacity_state_t *state, int32_t limit_w) {
    if (!state) return;
    state->limit_w = limit_w;
}

void capacity_tick_1s(capacity_state_t *state, int32_t total_power_w,
                      uint8_t month, uint8_t year_offset) {
    if (!state) return;

    /* Detect month rollover — reset monthly peak */
    if (month != state->monthly.peak_month ||
        year_offset != state->monthly.peak_year_offset) {
        state->monthly.monthly_peak_w = 0;
        state->monthly.peak_month = month;
        state->monthly.peak_year_offset = year_offset;
    }

    /* Accumulate watt-seconds */
    state->window.accumulated_ws += total_power_w;
    state->window.window_elapsed_s++;

    /* Window complete? */
    if (state->window.window_elapsed_s >= CAPACITY_WINDOW_SECONDS) {
        state->window.window_avg_w =
            (int32_t)(state->window.accumulated_ws / CAPACITY_WINDOW_SECONDS);
        state->window.window_valid = 1;

        /* Update monthly peak if this window is a new high */
        if (state->window.window_avg_w > state->monthly.monthly_peak_w) {
            state->monthly.monthly_peak_w = state->window.window_avg_w;
        }

        /* Reset window for next 15-minute period */
        state->window.accumulated_ws = 0;
        state->window.window_elapsed_s = 0;
    }
}

int32_t capacity_get_headroom_w(const capacity_state_t *state) {
    if (!state) return 0;
    if (state->limit_w == 0) return INT32_MAX;

    /*
     * Calculate how much additional power can be consumed in the remainder
     * of this window without the window average exceeding the limit.
     *
     * The window average at completion will be:
     *   avg = (accumulated_ws + future_ws) / CAPACITY_WINDOW_SECONDS
     *
     * For avg <= limit_w:
     *   accumulated_ws + future_ws <= limit_w * CAPACITY_WINDOW_SECONDS
     *
     * remaining_seconds = CAPACITY_WINDOW_SECONDS - window_elapsed_s
     * If we add P watts for all remaining seconds:
     *   future_ws = P * remaining_seconds
     *
     * So the max P that keeps avg <= limit_w:
     *   P = (limit_w * CAPACITY_WINDOW_SECONDS - accumulated_ws) / remaining_seconds
     *
     * This gives the instantaneous headroom: how much power can be drawn
     * for the rest of this window without exceeding the limit.
     */
    uint16_t elapsed = state->window.window_elapsed_s;
    if (elapsed == 0) {
        /* Window just started — full headroom available */
        return state->limit_w;
    }

    int64_t budget_ws = (int64_t)state->limit_w * CAPACITY_WINDOW_SECONDS;
    int64_t remaining_ws = budget_ws - state->window.accumulated_ws;
    uint16_t remaining_s = CAPACITY_WINDOW_SECONDS - elapsed;

    if (remaining_s == 0) {
        /*
         * Window is about to complete (elapsed == 900 shouldn't happen since
         * tick resets it, but guard against it). Return based on current avg.
         */
        int32_t current_avg =
            (int32_t)(state->window.accumulated_ws / CAPACITY_WINDOW_SECONDS);
        return state->limit_w - current_avg;
    }

    int32_t headroom = (int32_t)(remaining_ws / remaining_s);
    return headroom;
}

int32_t capacity_get_window_avg_w(const capacity_state_t *state) {
    if (!state) return 0;
    return state->window.window_avg_w;
}

int32_t capacity_get_monthly_peak_w(const capacity_state_t *state) {
    if (!state) return 0;
    return state->monthly.monthly_peak_w;
}

int16_t capacity_headroom_to_da(int32_t headroom_w, uint8_t phases) {
    if (phases == 0) return 0;
    /* Convert watts to deciamps: P / (V * phases) * 10 = P * 10 / (230 * phases) */
    return (int16_t)(headroom_w * 10 / (230 * phases));
}

int capacity_to_json(const capacity_state_t *state, char *buf, size_t bufsz) {
    if (!state || !buf || bufsz == 0) {
        return -1;
    }

    /* Calculate running average for display */
    int32_t running_avg = 0;
    if (state->window.window_elapsed_s > 0) {
        running_avg = (int32_t)(state->window.accumulated_ws /
                                state->window.window_elapsed_s);
    }

    int32_t headroom = capacity_get_headroom_w(state);

    int n = snprintf(buf, bufsz,
        "{\"limit_w\":%d,"
        "\"window_avg_w\":%d,"
        "\"running_avg_w\":%d,"
        "\"window_elapsed_s\":%u,"
        "\"window_valid\":%u,"
        "\"monthly_peak_w\":%d,"
        "\"headroom_w\":%d}",
        (int)state->limit_w,
        (int)state->window.window_avg_w,
        (int)running_avg,
        (unsigned)state->window.window_elapsed_s,
        (unsigned)state->window.window_valid,
        (int)state->monthly.monthly_peak_w,
        (int)headroom);

    if (n < 0 || (size_t)n >= bufsz) {
        return -1;
    }

    return n;
}
