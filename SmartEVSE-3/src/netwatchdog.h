// netwatchdog.h — one link watchdog, one recovery.
//
// Layered probes every 20 s (2 consecutive failures = dead path):
//   - TCP connect to our own station IP (lwIP loopback, no radio): catches
//     lwIP/PCB/listener wedges.
//   - TCP connect to the gateway (full radio path): catches ARP death and
//     the classic WiFi RX/TX wedge. A RST counts as alive (a reply came
//     back); only timeouts count as dead.
// A recovery is reported to the caller (netwatchdog_tick returns 2), which
// performs the single shared wifiDriverReload(). The WiFi-stall detector in
// network_common.cpp owns the "no IP at all" state; this watchdog only runs
// while the station is connected.
#ifndef NETWATCHDOG_H
#define NETWATCHDOG_H

#ifdef __cplusplus
extern "C" {
#endif

// Call from network_loop() once per second. Returns 2 when a recovery was
// triggered this call (for logging).
int netwatchdog_tick(void);
unsigned long netwatchdog_ticks(void);
void netwatchdog_selftest_counters(unsigned *ok, unsigned *fails, unsigned *recoveries);
#ifdef __cplusplus
}
#endif
#endif // NETWATCHDOG_H
