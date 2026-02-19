/*
 * evse_bridge.h - Bridge between firmware globals and evse_ctx_t
 *
 * Synchronizes the existing global variables with the module's context
 * struct before/after each module call, so consumer files (glcd.cpp,
 * modbus.cpp, network_common.cpp, etc.) continue to work unchanged.
 */

#ifndef EVSE_BRIDGE_H
#define EVSE_BRIDGE_H

#include "evse_state_machine.h"

#ifdef __cplusplus
extern "C" {
#endif

extern evse_ctx_t g_evse_ctx;

void evse_bridge_init(void);
void evse_sync_globals_to_ctx(void);
void evse_sync_ctx_to_globals(void);

#ifdef __cplusplus
}
#endif

#endif // EVSE_BRIDGE_H
