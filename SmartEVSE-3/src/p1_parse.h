/*
 * p1_parse.h - Pure C HomeWizard P1 meter JSON parsing
 *
 * Extracts active current and power values from P1 meter JSON response.
 * Applies sign correction (power direction determines current sign).
 * No platform dependencies — testable natively with gcc.
 */

#ifndef P1_PARSE_H
#define P1_PARSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Result of parsing a P1 JSON response */
typedef struct {
    int16_t  current_da[3]; /* Per-phase current in deci-amps (A * 10) */
    int16_t  power_w[3];    /* Per-phase power in watts (for diagnostics) */
    uint8_t  phases;        /* Number of phases detected (0, 1, or 3) */
    uint8_t  valid;         /* 1 if parsing succeeded, 0 on error */
} p1_result_t;

/*
 * Parse a HomeWizard P1 meter JSON response string.
 *
 * Extracts fields:
 *   - active_current_l1_a, active_current_l2_a, active_current_l3_a
 *   - active_power_l1_w, active_power_l2_w, active_power_l3_w
 *
 * Current values are converted to deci-amps (A * 10).
 * Sign is determined by the corresponding power field:
 *   - positive power → positive current (consuming)
 *   - negative power → negative current (feeding in)
 *
 * @param json    Null-terminated JSON string from P1 API /api/v1/data
 * @param json_len Length of JSON string (excluding null terminator)
 * @return        Parsed result with phase count and corrents
 */
p1_result_t p1_parse_response(const char *json, uint16_t json_len);

/*
 * Extract a numeric value for a specific key from a JSON string.
 * Returns 1 if found, 0 if not found. Value is written to *out.
 *
 * This is a targeted extractor for flat JSON objects (no nesting).
 * Handles integer and decimal values (positive and negative).
 *
 * @param json     Null-terminated JSON string
 * @param json_len Length of JSON string
 * @param key      Key to search for (without quotes)
 * @param out      Output: parsed numeric value as float
 * @return         1 if key found and parsed, 0 otherwise
 */
uint8_t p1_json_find_number(const char *json, uint16_t json_len,
                            const char *key, float *out);

#ifdef __cplusplus
}
#endif

#endif /* P1_PARSE_H */
