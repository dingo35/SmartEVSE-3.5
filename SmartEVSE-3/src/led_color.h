/*
 * led_color.h - Pure C LED color computation from system state
 *
 * Extracted from BlinkLed_singlerun() in main.cpp. Computes RGB values
 * from EVSE state without hardware dependencies.
 */

#ifndef LED_COLOR_H
#define LED_COLOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LED brightness constants (must match main.h) */
#ifndef STATE_A_LED_BRIGHTNESS
#define STATE_A_LED_BRIGHTNESS 40
#define STATE_B_LED_BRIGHTNESS 255
#define ERROR_LED_BRIGHTNESS   255
#define WAITING_LED_BRIGHTNESS 255
#endif

/* Snapshot of state needed for LED color computation */
typedef struct {
    uint8_t error_flags;        /* ErrorFlags bitmask */
    uint8_t access_status;      /* 0=OFF, 1=ON, 2=PAUSE */
    uint8_t state;              /* STATE_A..STATE_MODEM_DENIED */
    uint8_t mode;               /* MODE_NORMAL, MODE_SMART, MODE_SOLAR */
    uint8_t charge_delay;       /* ChargeDelay > 0 means waiting */
    bool custom_button;         /* CustomButton state */
    uint8_t color_off[3];       /* ColorOff RGB */
    uint8_t color_custom[3];    /* ColorCustom RGB */
    uint8_t color_solar[3];     /* ColorSolar RGB */
    uint8_t color_smart[3];     /* ColorSmart RGB */
    uint8_t color_normal[3];    /* ColorNormal RGB */
    /* CH32-specific error detection */
    bool is_ch32;               /* true for CH32 platform */
    uint8_t rcm_test_counter;   /* RCMTestCounter (CH32 only) */
} led_state_t;

/* LED computation internal state (persistent across calls) */
typedef struct {
    uint8_t led_count;          /* Raw counter for animation */
    uint8_t led_pwm;            /* Computed PWM brightness */
} led_context_t;

/* RGB output */
typedef struct {
    uint8_t r, g, b;
} led_rgb_t;

/*
 * Compute LED RGB values from system state.
 * ctx persists across calls (for animation counters).
 * Returns the RGB values to write to hardware.
 */
led_rgb_t led_compute_color(const led_state_t *state, led_context_t *ctx);

/* ---- Public charging station LED scheme (upstream commit 3679fe3) ---- */

/*
 * OCPP ChargePointStatus values the Public scheme cares about.
 * Mirror of MicroOcpp::ChargePointStatus — caller converts.
 */
typedef enum {
    LED_CP_STATUS_OTHER       = 0,  /* Any status we don't color specially */
    LED_CP_STATUS_RESERVED    = 1,
    LED_CP_STATUS_UNAVAILABLE = 2,
    LED_CP_STATUS_FAULTED     = 3
} led_cp_status_t;

/*
 * Snapshot for the Public scheme. Caller pre-computes timing booleans
 * from millis() and MicroOcpp types so the pure function stays testable.
 */
typedef struct {
    uint8_t  error_flags;        /* ErrorFlags */
    uint8_t  charge_delay;       /* ChargeDelay */
    uint8_t  state;              /* STATE_A / B / B1 / C / MODEM_* */

    /* RFID-read grey flash: (millis() - OcppLastRfidUpdate) < 200 */
    bool     rfid_read_flash;

    /* Tx-notification flashes — caller pre-computes (age + enum check). */
    bool     tx_authorized_flash;  /* < 1000ms && Authorized */
    bool     tx_rejected_flash;    /* < 2000ms && {Rejected, DeAuthorized, ReservationConflict} */
    bool     tx_timeout_flash;     /* <  300ms && {AuthorizationTimeout, ConnectionTimeout} */

    /* ChargePointStatus (from getChargePointStatus()) */
    led_cp_status_t cp_status;
} led_public_state_t;

/*
 * Compute RGB for the Public (public charging station) scheme. Applied only
 * when the LedMode setting is 1; callers keep using led_compute_color() for
 * the Standard scheme otherwise. ctx persists across calls (animation).
 */
led_rgb_t led_public_compute(const led_public_state_t *state,
                             led_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* LED_COLOR_H */
