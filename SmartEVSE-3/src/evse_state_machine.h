/*
 * evse_state_machine.h - Public API for the EVSE state machine module
 *
 * Pure C interface. All hardware interactions go through HAL callbacks
 * in evse_ctx_t. No platform dependencies.
 */

#ifndef EVSE_STATE_MACHINE_H
#define EVSE_STATE_MACHINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "evse_ctx.h"

// ---- Initialization ----
void evse_init(evse_ctx_t *ctx, evse_hal_t *hal);

// ---- Core state machine functions ----
void evse_set_state(evse_ctx_t *ctx, uint8_t new_state);
void evse_tick_10ms(evse_ctx_t *ctx, uint8_t pilot_reading);
void evse_tick_1s(evse_ctx_t *ctx);

// ---- Power availability ----
int evse_is_current_available(evse_ctx_t *ctx);

// ---- Current distribution ----
void evse_calc_balanced_current(evse_ctx_t *ctx, int mod);

// ---- Current to PWM conversion ----
uint32_t evse_current_to_duty(uint16_t current);

// ---- Authorization ----
void evse_set_access(evse_ctx_t *ctx, AccessStatus_t access);

// ---- Error management ----
void evse_set_error_flags(evse_ctx_t *ctx, uint8_t flags);
void evse_clear_error_flags(evse_ctx_t *ctx, uint8_t flags);

// ---- Power unavailable ----
void evse_set_power_unavailable(evse_ctx_t *ctx);

// ---- Phase switching ----
uint8_t evse_force_single_phase(evse_ctx_t *ctx);
void evse_check_switching_phases(evse_ctx_t *ctx);

// ---- Priority scheduling ----
void evse_sort_priority(evse_ctx_t *ctx);
void evse_schedule_tick_1s(evse_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // EVSE_STATE_MACHINE_H
