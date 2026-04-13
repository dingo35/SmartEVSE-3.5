/*
 * led_color.c - Pure C LED color computation
 *
 * Extracted from BlinkLed_singlerun() in main.cpp.
 * Determines RGB LED values from system state snapshot.
 */

#include "led_color.h"

/* State constants (from evse_ctx.h) */
#ifndef STATE_A
#define STATE_A             0
#define STATE_B             1
#define STATE_C             2
#define STATE_B1            9
#define STATE_MODEM_REQUEST 11
#define STATE_MODEM_WAIT    12
#define STATE_MODEM_DENIED  14
#endif

/* Mode constants */
#ifndef MODE_NORMAL
#define MODE_NORMAL 0
#define MODE_SMART  1
#define MODE_SOLAR  2
#endif

/* Error flag bits */
#ifndef RCM_TRIPPED
#define CT_NOCOMM    2
#define TEMP_HIGH    4
#define EV_NOCOMM    8
#define RCM_TRIPPED 16
#define RCM_TEST    32
#endif

/* Access status */
#define ACCESS_OFF 0

/* Local copies of animation helpers (originals in utils.cpp which is C++) */
static uint8_t led_triwave8(uint8_t in) {
    if (in & 0x80)
        in = 255u - in;
    return in << 1;
}

static uint8_t led_scale8(uint8_t i, uint8_t scale) {
    return (((uint16_t)i) * (1 + (uint16_t)scale)) >> 8;
}

static uint8_t led_ease8InOutQuad(uint8_t i) {
    uint8_t j = i;
    if (j & 0x80)
        j = 255u - j;
    uint8_t jj = led_scale8(j, j);
    uint8_t jj2 = jj << 1;
    if (i & 0x80)
        jj2 = 255u - jj2;
    return jj2;
}

/* Apply color scaling: (pwm * color[c]) / 255 for each channel */
static led_rgb_t apply_mode_color(uint8_t pwm, const led_state_t *state) {
    led_rgb_t rgb;
    const uint8_t *color;

    if (state->custom_button)
        color = state->color_custom;
    else if (state->mode == MODE_SOLAR)
        color = state->color_solar;
    else if (state->mode == MODE_SMART)
        color = state->color_smart;
    else
        color = state->color_normal;

    rgb.r = (uint8_t)((uint16_t)pwm * color[0] / 255);
    rgb.g = (uint8_t)((uint16_t)pwm * color[1] / 255);
    rgb.b = (uint8_t)((uint16_t)pwm * color[2] / 255);
    return rgb;
}

/*
 * Determine if error condition should trigger red flashing.
 * CH32: checks (CT_NOCOMM|EV_NOCOMM|TEMP_HIGH) or RCM mismatch when not testing
 * ESP32: checks (RCM_TRIPPED|CT_NOCOMM|EV_NOCOMM|TEMP_HIGH)
 */
static bool has_error_condition(const led_state_t *state) {
    if (state->is_ch32) {
        if (state->error_flags & (CT_NOCOMM | EV_NOCOMM | TEMP_HIGH))
            return true;
        /* RCM mismatch: RCM_TRIPPED != RCM_TEST, but only if not in test */
        if (((state->error_flags & RCM_TRIPPED) != (state->error_flags & RCM_TEST))
            && !state->rcm_test_counter)
            return true;
        return false;
    }
    /* ESP32 */
    return (state->error_flags & (RCM_TRIPPED | CT_NOCOMM | EV_NOCOMM | TEMP_HIGH)) != 0;
}

led_rgb_t led_compute_color(const led_state_t *state, led_context_t *ctx) {
    led_rgb_t rgb = {0, 0, 0};

    if (has_error_condition(state)) {
        /* Very rapid flashing red */
        ctx->led_count += 20;
        if (ctx->led_count > 128)
            ctx->led_pwm = ERROR_LED_BRIGHTNESS;
        else
            ctx->led_pwm = 0;
        rgb.r = ctx->led_pwm;
        rgb.g = 0;
        rgb.b = 0;

    } else if (state->access_status == ACCESS_OFF && state->custom_button) {
        rgb.r = state->color_custom[0];
        rgb.g = state->color_custom[1];
        rgb.b = state->color_custom[2];

    } else if (state->access_status == ACCESS_OFF || state->state == STATE_MODEM_DENIED) {
        rgb.r = state->color_off[0];
        rgb.g = state->color_off[1];
        rgb.b = state->color_off[2];

    } else if (state->error_flags || state->charge_delay) {
        /* Waiting for solar power or not enough current */
        ctx->led_count += 2;
        if (ctx->led_count > 230)
            ctx->led_pwm = WAITING_LED_BRIGHTNESS;
        else
            ctx->led_pwm = 0;
        rgb = apply_mode_color(ctx->led_pwm, state);

    } else {
        /* State A, B, or C */
        if (state->state == STATE_A) {
            ctx->led_pwm = STATE_A_LED_BRIGHTNESS;
        } else if (state->state == STATE_B || state->state == STATE_B1
                   || state->state == STATE_MODEM_REQUEST
                   || state->state == STATE_MODEM_WAIT) {
            ctx->led_pwm = STATE_B_LED_BRIGHTNESS;
            ctx->led_count = 128;
        } else if (state->state == STATE_C) {
            if (state->mode == MODE_SOLAR)
                ctx->led_count++;
            else
                ctx->led_count += 2;
            ctx->led_pwm = led_ease8InOutQuad(led_triwave8(ctx->led_count));
        }
        rgb = apply_mode_color(ctx->led_pwm, state);
    }

    return rgb;
}

/* ---- Public charging station scheme (upstream commit 3679fe3) ---- */

led_rgb_t led_public_compute(const led_public_state_t *state,
                             led_context_t *ctx) {
    led_rgb_t rgb = {0, 0, 0};

    /* Decision tree mirrors upstream main.cpp Public block. Order matters —
     * flashes for recent events win over steady-state indicators. */

    if (state->rfid_read_flash) {
        /* Grey flash — RFID read in progress */
        rgb.r = 128; rgb.g = 128; rgb.b = 128;
    } else if (state->tx_authorized_flash) {
        /* Green flash — authorization granted */
        rgb.r = 0; rgb.g = 255; rgb.b = 0;
    } else if (state->tx_rejected_flash) {
        /* Red flash — authorization rejected */
        rgb.r = 255; rgb.g = 0; rgb.b = 0;
    } else if (state->tx_timeout_flash) {
        /* Red flash — auth/connection timeout */
        rgb.r = 255; rgb.g = 0; rgb.b = 0;
    } else if (state->cp_status == LED_CP_STATUS_RESERVED) {
        /* Orange — reserved */
        rgb.r = 255; rgb.g = 128; rgb.b = 0;
    } else if (state->cp_status == LED_CP_STATUS_UNAVAILABLE ||
               state->cp_status == LED_CP_STATUS_FAULTED) {
        /* Red — unavailable / faulted */
        rgb.r = 255; rgb.g = 0; rgb.b = 0;
    } else if (state->error_flags || state->charge_delay) {
        /* Slow orange blink — waiting for power / delayed */
        ctx->led_count += 2;
        if (ctx->led_count > 230)
            ctx->led_pwm = WAITING_LED_BRIGHTNESS;
        else
            ctx->led_pwm = 0;
        rgb.r = ctx->led_pwm;
        rgb.g = ctx->led_pwm / 2;
        rgb.b = 0;
    } else if (state->state == STATE_A) {
        /* Green (dim) — Available */
        ctx->led_pwm = STATE_A_LED_BRIGHTNESS;
        rgb.r = 0;
        rgb.g = ctx->led_pwm;
        rgb.b = 0;
    } else if (state->state == STATE_B || state->state == STATE_B1 ||
               state->state == STATE_MODEM_REQUEST || state->state == STATE_MODEM_WAIT) {
        /* Blue static — EV connected */
        ctx->led_pwm = STATE_B_LED_BRIGHTNESS;
        ctx->led_count = 128;  /* seed fade animation for the STATE_C transition */
        rgb.r = 0;
        rgb.g = 0;
        rgb.b = ctx->led_pwm;
    } else if (state->state == STATE_C) {
        /* Blue fading — Charging */
        ctx->led_count += 2;
        ctx->led_pwm = led_ease8InOutQuad(led_triwave8(ctx->led_count));
        rgb.r = 0;
        rgb.g = 0;
        rgb.b = ctx->led_pwm;
    }
    /* else: rgb stays {0,0,0} — nothing to display */

    return rgb;
}
