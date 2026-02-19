#ifndef HTTP_API_H
#define HTTP_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Settings update request — parsed from POST /settings parameters.
// Each field has a has_* flag and a value. Only fields with has_*=true were present.
typedef struct {
    bool has_mode;              int mode;           // 0=OFF, 1=Normal, 2=Solar, 3=Smart, 4=Pause
    bool has_backlight;         int backlight;
    bool has_current_min;       int current_min;
    bool has_max_sum_mains;     int max_sum_mains;
    bool has_max_sum_mains_time; int max_sum_mains_time;
    bool has_disable_override;
    bool has_custom_button;     int custom_button;
    bool has_enable_c2;         int enable_c2;
    bool has_stop_timer;        int stop_timer;
    bool has_override_current;  int override_current;
    bool has_solar_start;       int solar_start_current;
    bool has_solar_max_import;  int solar_max_import;
    bool has_lcd_lock;          int lcd_lock;
    bool has_cable_lock;        int cable_lock;
    bool has_prio_strategy;     int prio_strategy;
    bool has_rotation_interval; int rotation_interval;
    bool has_idle_timeout;      int idle_timeout;
} http_settings_request_t;

// Validation result — NULL means valid, non-NULL is an error message.
typedef struct {
    const char *field;
    const char *error;
} http_validation_error_t;

// Validate individual settings fields against allowed ranges.
// Returns the number of errors written to errors[] (0 = all valid).
// max_errors: size of the errors[] array.
int http_api_validate_settings(const http_settings_request_t *req,
                               int min_current, int max_current,
                               int load_bl, int current_mode,
                               http_validation_error_t *errors, int max_errors);

// Parse "R,G,B" color parameters into components. Returns true on valid input.
bool http_api_parse_color(int r_val, int g_val, int b_val,
                          uint8_t *r_out, uint8_t *g_out, uint8_t *b_out);

// Validate a current override value.
// Returns NULL on success, or a static error message on failure.
const char *http_api_validate_override_current(int value, int min_current,
                                               int max_current, int load_bl);

// Validate a current_min value.
const char *http_api_validate_current_min(int value, int load_bl);

// Validate a max_sum_mains value.
const char *http_api_validate_max_sum_mains(int value, int load_bl);

// Validate a stop_timer value (0..60).
const char *http_api_validate_stop_timer(int value);

// Validate solar_start_current (0..48).
const char *http_api_validate_solar_start(int value);

// Validate solar_max_import (0..48).
const char *http_api_validate_solar_max_import(int value);

// Validate prio_strategy (0..2). Only valid on master (load_bl <= 1).
const char *http_api_validate_prio_strategy(int value, int load_bl);

// Validate rotation_interval (0 or 30..1440). Only valid on master.
const char *http_api_validate_rotation_interval(int value, int load_bl);

// Validate idle_timeout (30..300). Only valid on master.
const char *http_api_validate_idle_timeout(int value, int load_bl);

#ifdef __cplusplus
}
#endif

#endif
