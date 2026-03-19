/*
 * solar_debug_json.h - Format evse_solar_debug_t as JSON
 *
 * Pure C module — no platform dependencies, testable natively.
 */

#ifndef SOLAR_DEBUG_JSON_H
#define SOLAR_DEBUG_JSON_H

#include "evse_ctx.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Format a solar debug snapshot as a JSON object string.
 *
 * @param snap   Pointer to the solar debug snapshot
 * @param buf    Output buffer
 * @param bufsz  Size of output buffer
 * @return       Number of characters written (excluding NUL), or -1 on error
 */
int solar_debug_to_json(const evse_solar_debug_t *snap, char *buf, size_t bufsz);

#ifdef __cplusplus
}
#endif

#endif /* SOLAR_DEBUG_JSON_H */
