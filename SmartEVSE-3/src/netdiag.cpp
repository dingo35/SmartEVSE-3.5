// netdiag.cpp - lwIP diagnostics: stats dump + socket table inspection
//
// Reads lwIP internals directly (compiled against the same IDF 4.4.7 sources the
// patched liblwip.a was built from):
//  - lwip_stats            (needs CONFIG_LWIP_STATS=y in the lib)
//  - sockets[]             (struct lwip_sock[NUM_SOCKETS], api/sockets.c)
// Exposes:
//  - netdiag_dump()        full dump (telnet "n")
//  - netdiag_health_line() one-line summary for per-second logging

#include "netdiag.h"
#include <Arduino.h>
#include "lwip/opt.h"
#include "lwip/stats.h"
#include "lwip/priv/sockets_priv.h"
#include "lwip/api.h"
#include <stdio.h>
#include "debug.h"
#include "esp_heap_caps.h"

// Mirror of api/sockets.c state — layout verified by disassembly of the
// 2.0.17 liblwip.a: 32 entries, 24 bytes each (conn ptr + lastdata ptr +
// rcvevent/sendevent/errevent/select_waiting + pad).
extern "C" {
extern struct lwip_sock sockets[NUM_SOCKETS];
}

static int netdiag_count_allocated(void) {
    int n = 0;
    for (int i = 0; i < NUM_SOCKETS; i++) {
        if (sockets[i].conn != NULL) n++;
    }
    return n;
}

int netdiag_health_line(char *out, size_t outlen) {
    int used = netdiag_count_allocated();
    u32_t memerr = 0, drop = 0, err = 0;
#if TCP_STATS
    memerr = lwip_stats.tcp.memerr;
    drop = lwip_stats.tcp.drop;
    err = lwip_stats.tcp.err;
#endif
    // internal-only heap visibility (byte-addressable; WiFi RX buffers
    // allocate from here). Plain INTERNAL is inflated ~40KB by 32-bit-only
    // IRAM heap regions that cannot serve byte data.
    size_t int_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t int_max = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    // default-pool visibility (Mongoose HTTP connection buffers)
    size_t def_free = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    snprintf(out, outlen,
             "netdiag: sock %d/%d int %u/%u def %u memerr %u drop %u err %u",
             used, (int)NUM_SOCKETS,
             (unsigned)int_free, (unsigned)int_max,
             (unsigned)def_free,
             (unsigned)memerr, (unsigned)drop, (unsigned)err);
    return used;
}

void netdiag_dump(void) {
    _LOG_A_NO_FUNC("=== netdiag: socket table (%d slots) ===\r\n", (int)NUM_SOCKETS);
    int used = 0;
    for (int i = 0; i < NUM_SOCKETS; i++) {
        struct lwip_sock *s = &sockets[i];
        if (s->conn == NULL) continue;
        used++;
        struct netconn *nc = s->conn;
        const char *tname =
            nc->type == NETCONN_TCP ? "TCP" :
            nc->type == NETCONN_UDP ? "UDP" :
            nc->type == NETCONN_RAW ? "RAW" : "?";
        _LOG_A_NO_FUNC("  slot %2d: %s state=%d %s errevent=%u selwait=%u\r\n",
                      i, tname, (int)nc->state,
                      nc->pcb.tcp ? "pcb=yes" : "pcb=NULL",
                      (unsigned)s->errevent, (unsigned)s->select_waiting);
    }
    _LOG_A_NO_FUNC("  -> %d/%d sockets in use\r\n", used, (int)NUM_SOCKETS);

#if LWIP_STATS
    _LOG_A_NO_FUNC("=== lwip_stats ===\r\n");
#if TCP_STATS
    _LOG_A_NO_FUNC("tcp: xmit=%u recv=%u drop=%u chkerr=%u lenerr=%u memerr=%u rterr=%u proterr=%u err=%u cachehit=%u\r\n",
        (unsigned)lwip_stats.tcp.xmit, (unsigned)lwip_stats.tcp.recv,
        (unsigned)lwip_stats.tcp.drop, (unsigned)lwip_stats.tcp.chkerr,
        (unsigned)lwip_stats.tcp.lenerr, (unsigned)lwip_stats.tcp.memerr,
        (unsigned)lwip_stats.tcp.rterr, (unsigned)lwip_stats.tcp.proterr,
        (unsigned)lwip_stats.tcp.err, (unsigned)lwip_stats.tcp.cachehit);
#endif
#if MEM_STATS
    _LOG_A_NO_FUNC("mem heap: avail=%u used=%u max=%u err=%u\r\n",
        (unsigned)lwip_stats.mem.avail, (unsigned)lwip_stats.mem.used,
        (unsigned)lwip_stats.mem.max, (unsigned)lwip_stats.mem.err);
#endif
#if IP_STATS
    _LOG_A_NO_FUNC("ip: xmit=%u recv=%u drop=%u err=%u\r\n",
        (unsigned)lwip_stats.ip.xmit, (unsigned)lwip_stats.ip.recv,
        (unsigned)lwip_stats.ip.drop, (unsigned)lwip_stats.ip.err);
#endif
#if UDP_STATS
    _LOG_A_NO_FUNC("udp: xmit=%u recv=%u drop=%u err=%u\r\n",
        (unsigned)lwip_stats.udp.xmit, (unsigned)lwip_stats.udp.recv,
        (unsigned)lwip_stats.udp.drop, (unsigned)lwip_stats.udp.err);
#endif
#endif
}
