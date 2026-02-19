/*
 * mqtt_parser.c — Pure C MQTT command parser
 *
 * Extracts topic parsing, payload validation, and command classification
 * from mqtt_receive_callback() so it can be tested natively without
 * Arduino/ESP32 dependencies.
 */

#include "mqtt_parser.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

const char *mqtt_enable_c2_strings[MQTT_ENABLE_C2_COUNT] = {
    "Not present", "Always Off", "Solar Off", "Always On", "Auto"
};

// Match topic against prefix + suffix. Returns pointer past prefix+suffix, or NULL.
static const char *match_topic(const char *prefix, const char *topic, const char *suffix) {
    size_t plen = strlen(prefix);
    size_t slen = strlen(suffix);
    size_t tlen = strlen(topic);

    if (tlen != plen + slen)
        return NULL;
    if (strncmp(topic, prefix, plen) != 0)
        return NULL;
    if (strcmp(topic + plen, suffix) != 0)
        return NULL;
    return topic + plen + slen;
}

bool mqtt_parse_mains_meter(const char *payload, int32_t *L1, int32_t *L2, int32_t *L3) {
    int n = sscanf(payload, "%d:%d:%d", (int *)L1, (int *)L2, (int *)L3);
    if (n != 3)
        return false;
    // MainsMeter can measure -200A to +200A per phase (in dA)
    if (*L1 <= -2000 || *L1 >= 2000) return false;
    if (*L2 <= -2000 || *L2 >= 2000) return false;
    if (*L3 <= -2000 || *L3 >= 2000) return false;
    return true;
}

bool mqtt_parse_ev_meter(const char *payload, int32_t *L1, int32_t *L2, int32_t *L3,
                         int32_t *W, int32_t *Wh) {
    int n = sscanf(payload, "%d:%d:%d:%d:%d", (int *)L1, (int *)L2, (int *)L3, (int *)W, (int *)Wh);
    return n == 5;
}

bool mqtt_parse_rgb(const char *payload, uint8_t *r, uint8_t *g, uint8_t *b) {
    int32_t R, G, B;
    int n = sscanf(payload, "%d,%d,%d", &R, &G, &B);
    if (n != 3)
        return false;
    if (R < 0 || R > 255 || G < 0 || G > 255 || B < 0 || B > 255)
        return false;
    *r = (uint8_t)R;
    *g = (uint8_t)G;
    *b = (uint8_t)B;
    return true;
}

/*
 * mqtt_parse_command — Parse an MQTT topic+payload into a typed command struct.
 *
 * Matches the topic against known SmartEVSE Set suffixes (after the device
 * prefix), validates the payload, and populates the output command struct.
 *
 * Returns true if the topic matched a known command AND the payload was valid.
 * Returns false if the topic is unrecognized or the payload fails validation.
 *
 * Command groups handled:
 *   - Mode control:     /Set/Mode, /Set/CustomButton
 *   - Current limits:   /Set/CurrentOverride, /Set/CurrentMaxSumMains, /Set/CPPWMOverride
 *   - Meter feeds:      /Set/MainsMeter (L1:L2:L3), /Set/EVMeter (L1:L2:L3:W:Wh)
 *   - Home battery:     /Set/HomeBatteryCurrent
 *   - Vehicle ID:       /Set/RequiredEVCCID
 *   - LED colors:       /Set/Color{Off,Normal,Smart,Solar,Custom} (R,G,B)
 *   - Hardware config:  /Set/CableLock, /Set/EnableC2
 *   - Scheduling:       /Set/PrioStrategy, /Set/RotationInterval, /Set/IdleTimeout
 */
bool mqtt_parse_command(const char *prefix, const char *topic,
                        const char *payload, mqtt_command_t *out) {
    memset(out, 0, sizeof(*out));

    /* Mode control: Off/Normal/Solar/Smart/Pause */
    if (match_topic(prefix, topic, "/Set/Mode")) {
        out->cmd = MQTT_CMD_MODE;
        if (strcmp(payload, "Off") == 0)
            out->mode = MQTT_MODE_OFF;
        else if (strcmp(payload, "Normal") == 0)
            out->mode = MQTT_MODE_NORMAL;
        else if (strcmp(payload, "Solar") == 0)
            out->mode = MQTT_MODE_SOLAR;
        else if (strcmp(payload, "Smart") == 0)
            out->mode = MQTT_MODE_SMART;
        else if (strcmp(payload, "Pause") == 0)
            out->mode = MQTT_MODE_PAUSE;
        else
            return false;
        return true;
    }

    if (match_topic(prefix, topic, "/Set/CustomButton")) {
        out->cmd = MQTT_CMD_CUSTOM_BUTTON;
        out->custom_button = (strcmp(payload, "On") == 0);
        return true;
    }

    /* Current limits and PWM override */
    if (match_topic(prefix, topic, "/Set/CurrentOverride")) {
        out->cmd = MQTT_CMD_CURRENT_OVERRIDE;
        out->current_override = (uint16_t)atoi(payload);
        return true;
    }

    if (match_topic(prefix, topic, "/Set/CurrentMaxSumMains")) {
        out->cmd = MQTT_CMD_MAX_SUM_MAINS;
        int val = atoi(payload);
        if (val == 0 || (val >= 10 && val <= 600)) {
            out->max_sum_mains = (uint16_t)val;
            return true;
        }
        return false;
    }

    if (match_topic(prefix, topic, "/Set/CPPWMOverride")) {
        out->cmd = MQTT_CMD_CP_PWM_OVERRIDE;
        int pwm = atoi(payload);
        if (pwm >= -1 && pwm <= 1024) {
            out->cp_pwm = (int16_t)pwm;
            return true;
        }
        return false;
    }

    /* Meter data feeds: mains (3 phase currents) and EV (3 phase + power + energy) */
    if (match_topic(prefix, topic, "/Set/MainsMeter")) {
        out->cmd = MQTT_CMD_MAINS_METER;
        return mqtt_parse_mains_meter(payload, &out->mains_meter.L1,
                                      &out->mains_meter.L2, &out->mains_meter.L3);
    }

    if (match_topic(prefix, topic, "/Set/EVMeter")) {
        out->cmd = MQTT_CMD_EV_METER;
        return mqtt_parse_ev_meter(payload, &out->ev_meter.L1, &out->ev_meter.L2,
                                   &out->ev_meter.L3, &out->ev_meter.W, &out->ev_meter.Wh);
    }

    /* Home battery current: positive = charging, negative = discharging */
    if (match_topic(prefix, topic, "/Set/HomeBatteryCurrent")) {
        out->cmd = MQTT_CMD_HOME_BATTERY_CURRENT;
        out->home_battery_current = (int16_t)atoi(payload);
        return true;
    }

    /* Vehicle ID for ISO 15118 authorization */
    if (match_topic(prefix, topic, "/Set/RequiredEVCCID")) {
        out->cmd = MQTT_CMD_REQUIRED_EVCCID;
        size_t len = strlen(payload);
        if (len >= sizeof(out->evccid))
            return false;
        strncpy(out->evccid, payload, sizeof(out->evccid) - 1);
        out->evccid[sizeof(out->evccid) - 1] = '\0';
        return true;
    }

    // Color topics: ColorOff, ColorNormal, ColorSmart, ColorSolar, ColorCustom
    static const struct { const char *suffix; uint8_t index; } color_topics[] = {
        { "/Set/ColorOff",    MQTT_COLOR_OFF },
        { "/Set/ColorNormal", MQTT_COLOR_NORMAL },
        { "/Set/ColorSmart",  MQTT_COLOR_SMART },
        { "/Set/ColorSolar",  MQTT_COLOR_SOLAR },
        { "/Set/ColorCustom", MQTT_COLOR_CUSTOM },
    };
    for (int i = 0; i < 5; i++) {
        if (match_topic(prefix, topic, color_topics[i].suffix)) {
            out->cmd = MQTT_CMD_COLOR;
            out->color.index = color_topics[i].index;
            return mqtt_parse_rgb(payload, &out->color.r, &out->color.g, &out->color.b);
        }
    }

    /* Hardware configuration: cable lock and second contactor (C2) */
    if (match_topic(prefix, topic, "/Set/CableLock")) {
        out->cmd = MQTT_CMD_CABLE_LOCK;
        out->cable_lock = (strcmp(payload, "1") == 0) ? 1 : 0;
        return true;
    }

    if (match_topic(prefix, topic, "/Set/EnableC2")) {
        out->cmd = MQTT_CMD_ENABLE_C2;
        if (payload[0] != '\0' && isdigit((unsigned char)payload[0])) {
            int val = atoi(payload);
            if (val >= 0 && val <= 4) {
                out->enable_c2 = (uint8_t)val;
                return true;
            }
            return false;
        }
        // String lookup
        for (int i = 0; i < MQTT_ENABLE_C2_COUNT; i++) {
            if (strcmp(payload, mqtt_enable_c2_strings[i]) == 0) {
                out->enable_c2 = (uint8_t)i;
                return true;
            }
        }
        return false;
    }

    /* Priority scheduling settings (Master only, see priority-scheduling.md) */
    if (match_topic(prefix, topic, "/Set/PrioStrategy")) {
        out->cmd = MQTT_CMD_PRIO_STRATEGY;
        int val = atoi(payload);
        if (val >= 0 && val <= 2) {
            out->prio_strategy = (uint8_t)val;
            return true;
        }
        return false;
    }

    if (match_topic(prefix, topic, "/Set/RotationInterval")) {
        out->cmd = MQTT_CMD_ROTATION_INTERVAL;
        int val = atoi(payload);
        if (val == 0 || (val >= 30 && val <= 1440)) {
            out->rotation_interval = (uint16_t)val;
            return true;
        }
        return false;
    }

    if (match_topic(prefix, topic, "/Set/IdleTimeout")) {
        out->cmd = MQTT_CMD_IDLE_TIMEOUT;
        int val = atoi(payload);
        if (val >= 30 && val <= 300) {
            out->idle_timeout = (uint16_t)val;
            return true;
        }
        return false;
    }

    return false;
}
