/*
 * serial_parser.c - Pure C serial message parsing and current calculations
 *
 * Extracted from main.cpp ReadIrms(), ReadPowerMeasured(), receiveNodeStatus(),
 * getBatteryCurrent(), and CalcIsum(). No platform dependencies.
 */

#include "serial_parser.h"
#include <string.h>
#include <stdio.h>

/* EnableC2 enum values (must match main.h EnableC2_t) */
#define ENABLE_C2_ALWAYS_OFF 1

/* Mode and meter type constants for battery current logic */
#define SP_MODE_SOLAR 2
#define SP_EM_API     9

bool serial_parse_irms(const char *buf, serial_irms_t *out) {
    if (!buf || !out)
        return false;

    const char *p = strstr(buf, "Irms:");
    if (!p)
        return false;

    unsigned short address;
    short irms0, irms1, irms2;
    int n = sscanf(p, "Irms:%03hu,%hi,%hi,%hi", &address, &irms0, &irms1, &irms2);
    if (n != 4)
        return false;

    out->address = address;
    out->irms[0] = irms0;
    out->irms[1] = irms1;
    out->irms[2] = irms2;
    return true;
}

bool serial_parse_power(const char *buf, serial_power_t *out) {
    if (!buf || !out)
        return false;

    const char *p = strstr(buf, "PowerMeasured:");
    if (!p)
        return false;

    unsigned short address;
    short power;
    int n = sscanf(p, "PowerMeasured:%03hu,%hi", &address, &power);
    if (n != 2)
        return false;

    out->address = address;
    out->power = power;
    return true;
}

bool serial_parse_node_status(const uint8_t *buf, uint8_t buf_len,
                               serial_node_status_t *out) {
    if (!buf || !out || buf_len < 16)
        return false;

    out->state = buf[1];
    out->error = buf[3];
    out->mode = buf[7];
    out->solar_timer = ((uint16_t)buf[8] << 8) | buf[9];
    out->config_changed = buf[13];
    out->max_current = (uint16_t)buf[15] * 10;
    return true;
}

calc_isum_result_t calc_isum(const calc_isum_input_t *input) {
    calc_isum_result_t result;
    int16_t battery_per_phase = input->battery_current / 3;

    result.isum = 0;
    for (int x = 0; x < 3; x++) {
        result.adjusted_irms[x] = input->mains_irms[x];
        if (input->enable_c2 != ENABLE_C2_ALWAYS_OFF) {
            result.adjusted_irms[x] -= battery_per_phase;
        } else {
            if (x == 0) {
                result.adjusted_irms[x] -= input->battery_current;
            }
        }
        result.isum += result.adjusted_irms[x];
    }
    return result;
}

int16_t calc_battery_current(uint32_t time_since_update, uint8_t mode,
                              uint8_t mains_meter_type __attribute__((unused)),
                              int16_t battery_current) {
    /* Never updated (time_since_update == 0 with no prior update) */
    if (time_since_update == 0)
        return 0;

    /* Stale data: last update was more than 60s ago */
    if (time_since_update > 60)
        return 0;

    /* Only use battery current in Solar mode */
    if (mode == SP_MODE_SOLAR)
        return battery_current;

    return 0;
}
