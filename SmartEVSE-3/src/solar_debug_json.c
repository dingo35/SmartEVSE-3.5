/*
 * solar_debug_json.c - Format evse_solar_debug_t as JSON
 *
 * Pure C module — no platform dependencies, testable natively.
 */

#include "solar_debug_json.h"
#include <stdio.h>

int solar_debug_to_json(const evse_solar_debug_t *snap, char *buf, size_t bufsz)
{
    if (!snap || !buf || bufsz == 0)
        return -1;

    int n = snprintf(buf, bufsz,
        "{"
        "\"IsetBalanced\":%d,"
        "\"IsetBalanced_ema\":%d,"
        "\"Idifference\":%d,"
        "\"IsumImport\":%d,"
        "\"Isum\":%d,"
        "\"MainsMeterImeasured\":%d,"
        "\"Balanced0\":%u,"
        "\"SolarStopTimer\":%u,"
        "\"PhaseSwitchTimer\":%u,"
        "\"PhaseSwitchHoldDown\":%u,"
        "\"NoCurrent\":%u,"
        "\"SettlingTimer\":%u,"
        "\"Nr_Of_Phases_Charging\":%u,"
        "\"ErrorFlags\":%u"
        "}",
        (int)snap->IsetBalanced,
        (int)snap->IsetBalanced_ema,
        (int)snap->Idifference,
        (int)snap->IsumImport,
        (int)snap->Isum,
        (int)snap->MainsMeterImeasured,
        (unsigned)snap->Balanced0,
        (unsigned)snap->SolarStopTimer,
        (unsigned)snap->PhaseSwitchTimer,
        (unsigned)snap->PhaseSwitchHoldDown,
        (unsigned)snap->NoCurrent,
        (unsigned)snap->SettlingTimer,
        (unsigned)snap->Nr_Of_Phases_Charging,
        (unsigned)snap->ErrorFlags);

    if (n < 0 || (size_t)n >= bufsz)
        return -1;

    return n;
}
