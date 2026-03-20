/*
 * p1_parse.c - Pure C HomeWizard P1 meter JSON parsing
 *
 * Targeted JSON field extractor for the P1 meter response format.
 * No platform dependencies.
 */

#include "p1_parse.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

/*
 * Find a JSON key in a flat JSON object and extract its numeric value.
 * Searches for the pattern: "key":number
 * Handles positive/negative integers and decimals.
 */
uint8_t p1_json_find_number(const char *json, uint16_t json_len,
                            const char *key, float *out)
{
    if (!json || !key || !out || json_len == 0) return 0;

    size_t key_len = strlen(key);
    if (key_len == 0) return 0;

    /*
     * Search for "key": pattern. We need at least:
     * 1 (quote) + key_len + 1 (quote) + 1 (colon) = key_len + 3
     */
    const char *p = json;
    const char *end = json + json_len;

    while (p < end) {
        /* Find next quote */
        p = memchr(p, '"', (size_t)(end - p));
        if (!p) return 0;
        p++; /* skip opening quote */

        /* Check if key matches */
        if ((size_t)(end - p) < key_len + 1) return 0;
        if (memcmp(p, key, key_len) != 0 || p[key_len] != '"') {
            /* Not our key, skip past this quote and continue */
            p = memchr(p, '"', (size_t)(end - p));
            if (!p) return 0;
            p++;
            continue;
        }

        /* Found matching key. Advance past closing quote */
        p += key_len + 1;

        /* Skip whitespace and colon */
        while (p < end && (*p == ' ' || *p == '\t' || *p == ':')) p++;

        if (p >= end) return 0;

        /* Parse the numeric value using strtof for correct float parsing */
        char *num_end = NULL;
        *out = strtof(p, &num_end);
        if (num_end == p) return 0; /* no valid number */

        /* Reject NaN and Infinity — strtof parses "NaN", "Infinity" etc. */
        if (isnan(*out) || isinf(*out)) return 0;

        return 1;
    }

    return 0;
}

p1_result_t p1_parse_response(const char *json, uint16_t json_len)
{
    p1_result_t result;
    memset(&result, 0, sizeof(result));

    if (!json || json_len == 0) return result;

    static const char *current_keys[3] = {
        "active_current_l1_a",
        "active_current_l2_a",
        "active_current_l3_a"
    };
    static const char *power_keys[3] = {
        "active_power_l1_w",
        "active_power_l2_w",
        "active_power_l3_w"
    };

    float currents[3] = {0, 0, 0};
    float powers[3] = {0, 0, 0};
    uint8_t phases = 0;

    /* Extract current values */
    for (int i = 0; i < 3; i++) {
        if (p1_json_find_number(json, json_len, current_keys[i], &currents[i])) {
            phases++;
        }
    }

    if (phases == 0) return result;

    /* Extract power values (used for sign correction) */
    for (int i = 0; i < 3; i++) {
        p1_json_find_number(json, json_len, power_keys[i], &powers[i]);
    }

    /* Convert currents to deci-amps with sign correction from power */
    for (uint8_t i = 0; i < phases; i++) {
        /* Current from P1 is absolute; power sign gives direction */
        float abs_current = currents[i];
        if (abs_current < 0) abs_current = -abs_current;

        float current_da_f = abs_current * 10.0f;

        /* Clamp to int16_t range — values > 3276.7A are physically impossible */
        if (current_da_f > (float)INT16_MAX || current_da_f < (float)INT16_MIN) {
            /* Overflow: mark result invalid */
            result.valid = 0;
            result.phases = 0;
            return result;
        }

        int16_t current_da = (int16_t)current_da_f;

        /* Apply sign from power: negative power = feeding in */
        if (powers[i] < 0) {
            current_da = (int16_t)(-current_da);
        }

        result.current_da[i] = current_da;

        /* Clamp power to int16_t range */
        if (powers[i] > (float)INT16_MAX || powers[i] < (float)INT16_MIN) {
            result.valid = 0;
            result.phases = 0;
            return result;
        }
        result.power_w[i] = (int16_t)powers[i];
    }

    result.phases = phases;
    result.valid = 1;
    return result;
}
