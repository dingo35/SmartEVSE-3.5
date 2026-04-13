# Upstream Differences

This document tracks all differences between this fork
([basmeerman/SmartEVSE-3.5](https://github.com/basmeerman/SmartEVSE-3.5)) and the
upstream repository ([dingo35/SmartEVSE-3.5](https://github.com/dingo35/SmartEVSE-3.5)).

For feature details and configuration, see [Features](features.md).

---

## Architecture Changes

These are structural changes that affect the entire codebase — not specific features.

| Change | Upstream state | Fork state |
|--------|---------------|------------|
| State machine location | Inline in `main.cpp` (~3,000 lines) | Extracted to `evse_state_machine.c` (pure C) |
| State representation | ~70 scattered globals | `evse_ctx_t` context struct |
| Hardware abstraction | Direct GPIO calls in logic | Function pointers via HAL callbacks |
| Global synchronization | No protection | `evse_bridge.cpp` with spinlock/mutex |
| Native testability | Not possible (Arduino dependencies) | 1,096 tests compile with plain `gcc` |
| CI pipeline | PlatformIO build only | 10-job pipeline (tests, sanitizers, valgrind, cppcheck, builds, BDD, traceability, OCPP, Modbus) |
| Test methodology | None | Specification-by-Example (SbE) with traceability |

---

## Feature Differences by Area

### Smart & Solar Mode

Addresses upstream issues
[#327](https://github.com/dingo35/SmartEVSE-3.5/issues/327),
[#335](https://github.com/dingo35/SmartEVSE-3.5/issues/335),
[#316](https://github.com/dingo35/SmartEVSE-3.5/issues/316).

| Improvement | Why | Details |
|-------------|-----|---------|
| EMA current smoothing | Oscillation in smart/solar modes | [Features: Solar & Smart Mode](features.md#solar--smart-mode) |
| Dead band regulation | Micro-adjustments cause unnecessary switching | [Features: Solar & Smart Mode](features.md#solar--smart-mode) |
| Symmetric ramp rates | Overshoot/undershoot from asymmetric regulation | [Features: Solar & Smart Mode](features.md#solar--smart-mode) |
| Tiered phase switching timers | Rapid 1P/3P cycling | [Features: Solar & Smart Mode](features.md#solar--smart-mode) |
| Stop/start cycling prevention | Solar mode stops and restarts unnecessarily | [Features: Solar & Smart Mode](features.md#solar--smart-mode) |
| Multi-node SolarStopTimer fix | Upstream threshold scales with `ActiveEVSE`, unreachable for 2+ nodes (commit `94ca08e`). Upstream attempted a different fix in `02dafa2` that we evaluated and rejected — it reproduces the multi-node scaling bug and causes stop/start cycling for fixed 3-phase. See [analysis](upstream-sync/analysis-02dafa2-solar-stop-threshold.md). | [PR #119](https://github.com/basmeerman/SmartEVSE-3.5/pull/119) |
| Slave mode sync via setMode() | Upstream `SETITEM(MENU_MODE)` skips phase switching and error clearing on slaves | [PR #121](https://github.com/basmeerman/SmartEVSE-3.5/pull/121) |
| Slow EV compatibility | Renault Zoe stalls on rapid current changes | [Features: Solar & Smart Mode](features.md#solar--smart-mode) |
| ChargeDelay re-set on solar loss | Solar mode ChargeDelay could expire without solar, causing charging-without-solar oscillation — integrated upstream `74e20c8` (master) + `3ab1cee` (node-side) with 3 unit tests | upstream `74e20c8`, `3ab1cee` |

### Load Balancing

Addresses upstream issue
[#316](https://github.com/dingo35/SmartEVSE-3.5/issues/316).

| Improvement | Why | Details |
|-------------|-----|---------|
| Oscillation dampening | Current hunting between nodes | [Features: Load Balancing](features.md#load-balancing--power-sharing) |
| EMA filter on Idifference | Measurement noise causes false triggers | [Features: Load Balancing](features.md#load-balancing--power-sharing) |
| Distribution smoothing | Sudden jumps stress contactors and EV controllers | [Features: Load Balancing](features.md#load-balancing--power-sharing) |
| Diagnostic snapshot | No visibility into load balancing decisions | [Features: Load Balancing](features.md#load-balancing--power-sharing) |
| 126 convergence tests | Algorithm changes blocked by regression risk | [Features: Load Balancing](features.md#load-balancing--power-sharing) |
| CAPACITY fluctuation fix | `MaxSumMains` overwrote per-phase Idifference with a sum-of-phases value, causing current fluctuations when the EU capacity limit was configured — integrated upstream `a54b07f` (fixes #327) with 3 unit tests | upstream `a54b07f` |

### RFID, OCPP & Authorization

| Improvement | Why | Details |
|-------------|-----|---------|
| RFID toggle bug fix | Next RFID swipe toggles OFF instead of ON after Tesla disconnect | [Features: OCPP & Authorization](features.md#rfid-ocpp--authorization) |
| Bridge transaction mutex | Daily OCPP session failures from concurrent task corruption | [Features: OCPP & Authorization](features.md#rfid-ocpp--authorization) |
| Pure C OCPP logic extraction | OCPP logic untestable (85 tests added) | [Features: OCPP & Authorization](features.md#rfid-ocpp--authorization) |
| LoadBl exclusivity enforcement | OCPP limits silently ignored when LoadBl toggled at runtime | [Features: OCPP & Authorization](features.md#rfid-ocpp--authorization) |
| FreeVend solar safety | Auto-authorize bypasses solar surplus checks | [Features: OCPP & Authorization](features.md#rfid-ocpp--authorization) |
| OCPP settings validation | Invalid URLs/IDs accepted silently | [Features: OCPP & Authorization](features.md#rfid-ocpp--authorization) |
| OCPP connection telemetry | No diagnostics for connection drops | [Features: OCPP & Authorization](features.md#rfid-ocpp--authorization) |
| IEC 61851 → OCPP status mapping | EVCC integration needs standard status codes | [Features: OCPP & Authorization](features.md#rfid-ocpp--authorization) |
| Silent OCPP session loss recovery | WebSocket pings keep transport alive but don't prove backend is processing OCPP messages — integrated upstream `ecd088b` with timing logic extracted to pure C and 10 unit tests | upstream `ecd088b` |
| Atomic connector lock decision | Upstream `ocppLoop()` briefly flipped `OcppForcesLock` false→true within one iteration, causing actuator unlock/relock jitter — integrated upstream `05c7fc2` with the decision extracted to pure C and 11 unit tests | upstream `05c7fc2` |
| OCPP Finishing-before-Available sequence | CSMS missed the Finishing state because OccupiedInput went false immediately after StopTx — integrated upstream `afd72a8` with the decision extracted to pure C (`ocpp_should_report_occupied`) and 6 unit tests | upstream `afd72a8` |
| Cable disconnect detection under PAUSE | CP sampling timer left disabled after STATE_B single-shot fired — integrated upstream `e6110b1` (timerAlarmEnable in STATE_A/STATE_C1 paths) | upstream `e6110b1` |
| Public charging station LED scheme | Public-charger color semantics (available/connected/charging/reserved/faulted) selectable via new `LedMode` menu — integrated upstream `3679fe3` with the decision tree extracted to pure C (`led_public_compute()`) and 14 unit tests. Fork keeps `MENU_LEDMODE=51` to avoid renumber cascade. | upstream `3679fe3` |

### MQTT & Home Assistant

Addresses upstream issues
[#320](https://github.com/dingo35/SmartEVSE-3.5/issues/320),
[#294](https://github.com/dingo35/SmartEVSE-3.5/issues/294),
[PR #338](https://github.com/dingo35/SmartEVSE-3.5/pull/338).

| Improvement | Why | Details |
|-------------|-----|---------|
| Change-only publishing | 70-97% MQTT message reduction | [Features: MQTT & HA](features.md#mqtt--home-assistant) |
| Fixed HA discovery payloads | Corrupted long-term statistics | [Features: MQTT & HA](features.md#mqtt--home-assistant) |
| Energy zero-value guard | Phantom consumption in HA dashboard | [Features: MQTT & HA](features.md#mqtt--home-assistant) |
| Entity naming cleanup | HA 2025.10+ compatibility | [Features: MQTT & HA](features.md#mqtt--home-assistant) |
| New entities | Missing diagnostics (FreeHeap, MQTTMsgCount, etc.) | [Features: MQTT & HA](features.md#mqtt--home-assistant) |
| Per-phase power/energy via MQTT | No per-phase visibility | [Features: MQTT & HA](features.md#mqtt--home-assistant) |
| Metering diagnostic counters | No insight into meter communication health | [Features: MQTT & HA](features.md#mqtt--home-assistant) |

### Metering & Modbus

| Improvement | Why | Details |
|-------------|-----|---------|
| Orno WE-517/516 meter support | Community-requested meters | [Features: Metering](features.md#metering--modbus) |
| Modbus broadcast timeout handling | Timeout on a broadcast address would advance the request loop and skip the next legitimate slave response — integrated upstream `b104576` (1-line guard) | upstream `b104576` |
| Pure C Modbus frame decoder | Modbus logic untestable | [Features: Metering](features.md#metering--modbus) |
| Pure C meter byte decoder | 30 test scenarios for all endianness/data types | [Features: Metering](features.md#metering--modbus) |
| Pure C HomeWizard P1 parser | P1 parsing untestable | [Features: Metering](features.md#metering--modbus) |
| Meter telemetry counters | No visibility into communication errors | [Features: Metering](features.md#metering--modbus) |
| Modbus frame event logger | No debugging capability for Modbus issues | [Features: Metering](features.md#metering--modbus) |
| API/MQTT staleness detection | Stale API data causes incorrect charging | [Features: Metering](features.md#metering--modbus) |
| HomeWizard P1 energy data | HA energy dashboard incomplete | [Features: Metering](features.md#metering--modbus) |
| HomeWizard P1 manual IP fallback | mDNS unreliable on some networks | [Features: Metering](features.md#metering--modbus) |

### EVCC Integration

| Improvement | Why | Details |
|-------------|-----|---------|
| IEC 61851-1 state mapping | EVCC needs standard state letters (A-F) | [Features: EVCC](features.md#evcc-integration) |
| Phase switching via HTTP | EVCC needs to control 1P/3P switching | [Features: EVCC](features.md#evcc-integration) |
| Charging state derivation | EVCC needs `charging_enabled` boolean | [Features: EVCC](features.md#evcc-integration) |
| Ready-to-use EVCC template | No documentation for EVCC setup | [Features: EVCC](features.md#evcc-integration) |

### Diagnostic Telemetry (New in Fork)

| Feature | Purpose | Details |
|---------|---------|---------|
| Ring buffer event capture | Captures state machine events, errors, meter readings | [Features: Diagnostics](features.md#diagnostic-telemetry) |
| LittleFS persistence | Diagnostics survive reboots | [Features: Diagnostics](features.md#diagnostic-telemetry) |
| WebSocket live stream | Real-time diagnostic viewer in web UI | [Features: Diagnostics](features.md#diagnostic-telemetry) |
| Test replay framework | Replay recorded sessions through test suite | [Features: Diagnostics](features.md#diagnostic-telemetry) |
| MQTT profile control | Remote diagnostic capture control | [Features: Diagnostics](features.md#diagnostic-telemetry) |

### ERE Session Logging (New in Fork)

| Feature | Purpose | Details |
|---------|---------|---------|
| Charge session tracking | Automatic per-charge session recording | [Features: ERE](features.md#ere-session-logging) |
| ERE-compatible output | Dutch ERE certificate submission format | [Features: ERE](features.md#ere-session-logging) |
| MQTT session publish | Retained JSON on session complete | [Features: ERE](features.md#ere-session-logging) |
| REST endpoint | GET /session/last for integrations | [Features: ERE](features.md#ere-session-logging) |
| Zero flash wear | MQTT-only persistence, no flash writes | [Features: ERE](features.md#ere-session-logging) |

### Capacity Tariff Peak Tracking (New in Fork)

| Feature | Purpose | Details |
|---------|---------|---------|
| 15-minute quarter-peak averaging | Matches Belgian DSO metering interval | [Features: Capacity Tariff](features.md#capacity-tariff-peak-tracking) |
| Monthly peak tracking | Records highest 15-min average per month | [Features: Capacity Tariff](features.md#capacity-tariff-peak-tracking) |
| Automatic current reduction | Clamps IsetBalanced to stay under limit | [Features: Capacity Tariff](features.md#capacity-tariff-peak-tracking) |
| LCD/Web/MQTT/REST configuration | Full configuration from all interfaces | [Features: Capacity Tariff](features.md#capacity-tariff-peak-tracking) |
| Home Assistant integration | 4 auto-discovered entities | [Features: Capacity Tariff](features.md#capacity-tariff-peak-tracking) |

### CircuitMeter — Subpanel Metering (New in Fork)

| Feature | Purpose | Details |
|---------|---------|---------|
| Subpanel breaker protection | Limits EV charging to stay within breaker rating | [Features: CircuitMeter](features.md#circuitmeter--subpanel-metering) |
| ERE 2027 compliance support | Circuit-level energy measurement for Dutch ERE Path B | [Features: CircuitMeter](features.md#circuitmeter--subpanel-metering) |
| Reuses existing Meter class | Supports all 19 meter types with zero new meter code | [Features: CircuitMeter](features.md#circuitmeter--subpanel-metering) |
| MQTT + HA auto-discovery | Circuit current, power, energy, and MaxCircuitMains | [Features: CircuitMeter](features.md#circuitmeter--subpanel-metering) |

### SoC Injection via MQTT (New in Fork)

| Feature | Purpose | Details |
|---------|---------|---------|
| MQTT SoC topics | InitialSoC, FullSoC, EnergyCapacity, EnergyRequest, EVCCID | [Features: MQTT & HA](features.md#mqtt--home-assistant) |
| WiCAN OBD-II integration | Direct SoC reading from car CAN bus | [MQTT docs: WiCAN](mqtt-home-assistant.md#wican-obd-ii-integration) |
| Session-scoped values | Auto-clear on EV disconnect | [MQTT docs: SoC Injection](mqtt-home-assistant.md#soc-injection-via-mqtt) |

### Web & Connectivity

| Improvement | Why | Details |
|-------------|-----|---------|
| Offline-first web UI | CDN dependencies break isolated networks | [Features: Web](features.md#web--connectivity) |
| WebSocket data channel | HTTP polling is slow and wasteful | [Features: Web](features.md#web--connectivity) |
| Dashboard card redesign | Outdated UI | [Features: Web](features.md#web--connectivity) |
| Dark mode | Community demand | [Features: Web](features.md#web--connectivity) |
| Load balancing node overview | No multi-node visibility | [Features: Web](features.md#web--connectivity) |
| Diagnostic telemetry viewer | No way to view diagnostics in browser | [Features: Web](features.md#web--connectivity) |
| LCD widget modernization | Old layout, not responsive | [Features: Web](features.md#web--connectivity) |

### Security Hardening (Fork-only)

Findings from the security review (see internal report; most issues are inherited from upstream).

| Fix | Why | Details |
|-----|-----|---------|
| Unsigned firmware upload rejected at `POST /update` | Upstream accepts `firmware.bin` / `firmware.debug.bin` uploads over unauthenticated HTTP with no signature check → any LAN client can flash arbitrary firmware (unauthenticated RCE). Fork accepts only `*.signed.bin` — verified via the multi-key RSA validator from PR #125 | Security fix C-1 |
| OCPP auth_key never exposed via `/settings` GET | Upstream returns the plaintext OCPP backend basic-auth key to any client calling `GET /settings`. Fork emits only `auth_key_set: bool` and the Web UI shows a placeholder | Security fix C-2 |
| MQTT private password redacted from boot log | Upstream logs the full EC-private-key-hash on every boot. Fork logs a 4-char prefix then `[redacted]` | Security fix C-5 |
| `strncpy(RequiredEVCCID, ...)` always NUL-terminated | Upstream pattern leaves the buffer unterminated when source fills it; subsequent `%s` walks past end | Security fix H-5 |
| OCPP URL validator rejects SSRF targets | Upstream accepts `wss://127.0.0.1/`, `wss://[::1]/`, `wss://169.254.x/`, `wss://user:pass@host/`. Fork rejects loopback + link-local + embedded credentials. RFC1918 still allowed for self-hosted CSMS | Security fix H-4 |
| Full partition erase on signature failure | Upstream erases only the first `ENCRYPTED_BLOCK_SIZE` (4 KB) on sig-fail, leaving >4 KB of attacker bytes in flash. Fork erases the entire partition | Security fix M-4 |
| Hash buffer zeroed before free in `validate_sig` | Hygiene: keeps debug-memory dumps from trivially revealing the firmware fingerprint | Security fix M-3 |

---

## Contributing Upstream

When contributing improvements back to upstream:

- **Submit small, focused PRs** — dingo35 is conservative about large changes
- **Do not bundle test infrastructure with feature changes**
- **Follow upstream conventions** — they may differ from this fork
- **Never modify upstream repos without explicit user approval**
