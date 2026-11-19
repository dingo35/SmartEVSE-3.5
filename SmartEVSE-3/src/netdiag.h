// netdiag.h - lwIP diagnostics: stats dump + socket table inspection
// Build requires liblwip built with CONFIG_LWIP_STATS=y
// (see agent-home/tasks/wifi-socket-exhaustion-arduino2x/build_lwip.bat stats)
#ifndef NETDIAG_H
#define NETDIAG_H

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

// Dump lwip_stats + netconn/socket table to the Arduino log (USB serial).
// Registered as telnet command "n" via RemoteDebug.
void netdiag_dump(void);

// One-line health summary suitable for per-second logging.
// Returns number of lwIP sockets currently allocated (0 = stats unavailable).
int netdiag_health_line(char *out, size_t outlen);

#ifdef __cplusplus
}
#endif
#endif // NETDIAG_H
