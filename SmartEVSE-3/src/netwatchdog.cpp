// netwatchdog.cpp — see netwatchdog.h
#include "netwatchdog.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_netif.h>
#include <errno.h>
#include <fcntl.h>
#include "network_common.h"

#if SMARTEVSE_VERSION >= 30

#include <lwip/sockets.h>

static uint32_t nw_recoveries = 0;
static uint32_t nw_last_recovery = 0;
static uint32_t nw_ticks = 0;

static uint32_t nw_tcp_fails = 0;
static uint32_t nw_tcp_ok = 0;
static uint32_t nw_last_tcp = 0;

void netwatchdog_selftest_counters(uint32_t *ok, uint32_t *fails, uint32_t *recoveries) {
    if (ok) *ok = nw_tcp_ok;
    if (fails) *fails = nw_tcp_fails;
    if (recoveries) *recoveries = nw_recoveries;
}

unsigned long netwatchdog_ticks(void) { return nw_ticks; }

// TCP probe to a LAN destination. Non-blocking; a wedged path times out in
// the select window. A refusal (RST) also proves the path — a reply came
// back through the radio. Used for two targets: our own station IP (lwIP
// loops it back internally — catches PCB/listener wedges without radio) and
// the gateway (full radio path — catches ARP death, RX/TX wedge).
static bool nw_tcp_test(uint32_t dst_ip) {
    int s = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) return false;
    // MUST be non-blocking: a blocking connect to a wedged listen queue would
    // stall the whole network loop for the TCP retry period.
    int fl = lwip_fcntl(s, F_GETFL, 0);
    lwip_fcntl(s, F_SETFL, fl | O_NONBLOCK);
    struct sockaddr_in dst = {};
    dst.sin_family = AF_INET;
    dst.sin_port = lwip_htons(80);
    dst.sin_addr.s_addr = dst_ip;
    int r = lwip_connect(s, (struct sockaddr *)&dst, sizeof(dst));
    if (r != 0 && errno == EINPROGRESS) {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(s, &wfds);
        struct timeval tv = { 1, 500000 };  // 1.5s
        r = lwip_select(s + 1, NULL, &wfds, NULL, &tv);
        if (r <= 0) { lwip_close(s); return false; }
        int err = 0; socklen_t el = sizeof(err);
        lwip_getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &el);
        // connected or refused both prove the path (a RST is a reply through
        // the radio); anything else means the path is dead
        if (err != 0 && err != ECONNREFUSED) { lwip_close(s); return false; }
    } else if (r != 0 && errno != ECONNREFUSED) {
        lwip_close(s);
        return false;
    }
    lwip_close(s);
    return true;
}

int netwatchdog_tick(void) {
    nw_ticks++;
    if (WIFImode != 1 || !WiFi.isConnected()) {
        // not our watch; reset counters
        nw_tcp_fails = 0;
        return 0;
    }

    // Layered link probes every 20 s, one shared fail counter, 2 consecutive
    // failures = dead path:
    //   own station IP — loopback, no radio: catches lwIP/PCB/listener wedges
    //   gateway        — full radio path: catches ARP death, RX/TX wedge
    // No IP: not an error here — the WiFi-stall detector owns that state.
    uint32_t now2 = millis();
    if (now2 - nw_last_tcp >= 20000) {
        nw_last_tcp = now2;
        esp_netif_ip_info_t info;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        bool ok = true;
        if (netif && esp_netif_get_ip_info(netif, &info) == ESP_OK && info.ip.addr != 0) {
            ok = nw_tcp_test(info.ip.addr) &&
                 (info.gw.addr == 0 || nw_tcp_test(info.gw.addr));
        }
        if (!ok) {
            nw_tcp_fails++;
            if (nw_tcp_fails >= 2) {
                nw_tcp_fails = 0;
                uint32_t since = millis() - nw_last_recovery;
                // rate limit: at most one recovery per 60 s
                if (nw_last_recovery == 0 || since > 60000) {
                    nw_recoveries++;
                    nw_last_recovery = millis();
                    return 2;
                }
            }
        } else {
            nw_tcp_fails = 0;
            nw_tcp_ok++;
        }
    }

    return 0;
}
#endif // SMARTEVSE_VERSION
