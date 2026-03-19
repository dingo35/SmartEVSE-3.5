SmartEVSE v3
=========

Smart Electric Vehicle Charge Controller

![Image of SmartEVSE](/pictures/SmartEVSEv3.png)

# About this fork

This repository is a fork of [dingo35/SmartEVSE-3.5](https://github.com/dingo35/SmartEVSE-3.5)
maintained as a testbed for **IT/OT software engineering with AI-assisted development**
(multi-agent software engineering using [Claude Code](https://claude.ai/claude-code)).

The upstream SmartEVSE firmware is a monolithic embedded C++/Arduino codebase where the
state machine, load balancing, MQTT, HTTP, and hardware control all live in a single
~3,000-line `main.cpp`. This makes the safety-critical logic impossible to test without
real hardware.

This fork restructures the architecture to enable **native host testing** of the core
logic:

- **Pure C state machine** (`evse_state_machine.c`) — extracted from `main.cpp`, zero
  platform dependencies, compiles with plain `gcc` on any host
- **Context struct** (`evse_ctx_t`) — all state in one place instead of ~70 scattered
  globals
- **Bridge layer** (`evse_bridge.cpp`) — synchronizes the existing globals with the
  context struct so consumer files (`glcd.cpp`, `modbus.cpp`, `network_common.cpp`, etc.)
  continue to work unchanged. Protected by a FreeRTOS mutex on ESP32
- **HAL callbacks** — hardware operations (contactors, CP duty, pilot signal) are
  abstracted behind function pointers, replaced with no-ops in test builds
- **900+ native tests** across 44 suites with full Specification-by-Example (SbE)
  traceability

The purpose is to demonstrate how AI agents (Claude Code) can be used as collaborative
software engineering partners on real-world embedded/OT systems — performing root cause
analysis, architectural refactoring, test-driven development, and bug fixing on
safety-critical code. All changes in this fork were developed through human-AI
collaboration using the multi-agent workflow described in [CLAUDE.md](CLAUDE.md).

# What is it?

It's an open source EVSE (Electric Vehicle Supply Equipment). It supports 1-3 phase
charging, fixed charging cable or charging socket. Locking actuator support (5 different
types). And it can directly drive a mains contactor for supplying power to the EV.
It features a display from which all module parameters can be configured.<br>
Up to 8 modules can be connected together to charge up to eight EV's from one mains
connection without overloading it.<br>
The mains connection can be monitored by the (optional) sensorbox or a modbus kWh meter.
This allows smart charging.
Communication between the SmartEVSE(s) / Sensorbox or kWh meters is done over
RS485 (Modbus).

# Features

## Charging

- Works with all EV's or plugin hybrids
- 1-3 phase charging, fixed cable or socket with locking actuator (5 types)
- Automatically selects current capacity of the connected cable (13/16/32A)
- Two switched 230VAC contactor outputs — switch between 1 or 3 phase charging
- Powered RS485 communication bus for sensorbox / Modbus kWh meters
- Built-in temperature sensor with thermal protection
- Operating voltage: 110-240 Vac
- Dimensions (W x D x H): 52 x 91 x 58 mm (3 DIN modules)

## Smart & Solar Mode

- **Smart mode**: automatically adjusts charge current based on other household
  consumption to stay within mains capacity
- **Solar mode**: charges from solar surplus, with configurable start/stop thresholds
  and import allowance
- **1P/3P phase switching**: automatic switching between 1-phase and 3-phase based
  on available power (requires CONTACT 2 wiring)

### Fork improvements (addresses upstream [#327](https://github.com/dingo35/SmartEVSE-3.5/issues/327), [#335](https://github.com/dingo35/SmartEVSE-3.5/issues/335), [#316](https://github.com/dingo35/SmartEVSE-3.5/issues/316))

- **EMA current smoothing** — configurable exponential moving average filter to
  dampen oscillation in smart/solar modes
- **Dead band regulation** — suppresses micro-adjustments when current difference
  is within a configurable band
- **Symmetric ramp rates** — equal ramp-up and ramp-down speeds prevent
  overshoot/undershoot oscillation
- **Tiered phase switching timers** — separate fast timer for severe overload,
  configurable hold-down guard to prevent rapid 1P/3P cycling
- **Stop/start cycling prevention** — higher NoCurrent threshold, gradual decay,
  solar minimum run time, and shorter solar charge delay
- **Slow EV compatibility** — settling window and ramp rate limiter for EVs like
  the Renault Zoe that stall on rapid current changes

See [Solar & Smart Mode Stability](docs/solar-smart-stability.md) for all settings
and configuration.

## Load Balancing & Power Sharing

- Up to 8 SmartEVSEs share one mains connection without overloading it
- Priority-based power scheduling with configurable rotation intervals
- Delayed charging support

### Fork improvements (addresses upstream [#316](https://github.com/dingo35/SmartEVSE-3.5/issues/316))

- **Oscillation dampening** — detects current hunting (sign-flip detection on
  Idifference) and adaptively increases the regulation divisor to slow down
  adjustments until the system stabilizes
- **EMA filter on Idifference** — exponential moving average (25% alpha) smooths
  measurement noise from current transducers, reducing false regulation triggers
  while preserving convergence speed
- **Distribution smoothing** — per-EVSE delta clamping limits current changes to
  max 3.0A per cycle, preventing contactor stress and EV charge controller
  destabilization from sudden jumps
- **Load balancing diagnostic snapshot** — per-cycle diagnostic struct captures
  IsetBalanced, filtered Idifference, baseload, per-EVSE allocations, oscillation
  count, and shortage/clamping flags for debugging via MQTT
- **126 convergence tests** — multi-cycle simulation test suite covering 2-8 node
  configurations, priority scheduling under shortage, node join/leave transitions,
  and vehicle response lag modeling

See [Load Balancing Stability](docs/load-balancing-stability.md) for all settings
and configuration.

## RFID, OCPP & Authorization

- RFID reader support — restrict usage to up to 100 registered cards
- OCPP 1.6j support for backend authorization (Tap Electric, Tibber, SteVe, Monta)

### Fork improvements

- **Fixed RFID toggle bug** — AccessStatus is now cleared on all disconnect paths
  (including Tesla C→B→A), preventing the next RFID swipe from toggling OFF
  instead of ON
- **Bridge transaction mutex** — FreeRTOS mutex prevents concurrent task corruption
  of the state context, fixing daily OCPP session failures after Tesla disconnects
- **Pure C OCPP logic extraction** — authorization decisions, connector state
  detection, RFID formatting, and settings validation extracted to `ocpp_logic.c`
  for native testability (85 OCPP-specific tests)
- **LoadBl exclusivity enforcement** — OCPP Smart Charging and internal load
  balancing are mutually exclusive; now enforced at runtime with warnings when
  conflict detected (previously only checked at init)
- **FreeVend solar safety** — auto-authorize (FreeVend) no longer bypasses solar
  surplus checks or ChargeDelay, preventing charging without sunlight
- **OCPP settings validation** — backend URL (`ws://`/`wss://`), ChargeBoxId
  (max 20 chars, printable ASCII), and auth key validated before passing to
  MicroOcpp library
- **OCPP connection telemetry** — WebSocket connect/disconnect counters,
  transaction lifecycle tracking, authorization accept/reject/timeout metrics
- **IEC 61851 → OCPP status mapping** — maps IEC states to OCPP 1.6
  StatusNotification values (Available, Preparing, Charging, SuspendedEV/EVSE,
  Finishing, Faulted)

See [OCPP setup](docs/ocpp.md) for provider-specific guides (Tap Electric,
Tibber, SteVe) and configuration reference.

## MQTT & Home Assistant

- MQTT API for communication with Home Assistant and other software
- Auto-discovery payloads for automatic HA entity setup

### Fork improvements (addresses upstream [#320](https://github.com/dingo35/SmartEVSE-3.5/issues/320), [#294](https://github.com/dingo35/SmartEVSE-3.5/issues/294), [PR #338](https://github.com/dingo35/SmartEVSE-3.5/pull/338))

- **Change-only publishing** — 70-97% message reduction by only publishing changed
  values, with configurable heartbeat interval (default 60s)
- **Fixed HA discovery payloads** — corrected `state_class` for energy sensors,
  preventing corrupted long-term statistics
- **Energy zero-value guard** — energy values only published when > 0, preventing
  phantom consumption in HA energy dashboard
- **Entity naming cleanup** — snake_case entity IDs for HA 2025.10+ compatibility
- **New entities**: MaxSumMains (settable), FreeHeap, MQTTMsgCount, LoadBl,
  PairingPin, FirmwareVersion (diagnostic)
- **Per-phase power via MQTT** — 6 new topics (`MainsPowerL1/L2/L3`,
  `EVPowerL1/L2/L3`) with HA auto-discovery (device_class=power, unit=W)
  ([PR #82](https://github.com/basmeerman/SmartEVSE-3.5/pull/82))
- **Per-phase energy via MQTT** — 6 new topics for per-phase energy data with
  HA auto-discovery (state_class=total_increasing)
  ([PR #82](https://github.com/basmeerman/SmartEVSE-3.5/pull/82))
- **Metering diagnostic counters** — `MeterTimeoutCount`, `MeterRecoveryCount`,
  `ApiStaleCount` published as HA diagnostic entities for monitoring metering health
  ([PR #86](https://github.com/basmeerman/SmartEVSE-3.5/pull/86))

See [MQTT & Home Assistant](docs/mqtt-home-assistant.md) for full topic reference
and configuration.

## Metering & Modbus

- 18 supported meter types via Modbus RTU (Eastron, ABB, Finder, Phoenix Contact,
  Schneider, Chint, Carlo Gavazzi, SolarEdge, WAGO, Sinotimer, and more)
- HomeWizard P1 meter support via WiFi/HTTP
- Sensorbox v1/v2 with CT or P1 input
- API/MQTT external current feed

### Fork improvements

- **New meter types: Orno WE-517 (3P) and WE-516 (1P)** — community-requested
  bidirectional energy meters now officially supported
- **Pure C Modbus frame decoder** — `ModbusDecode()` extracted to `modbus_decode.c`
  for native testability, supporting FC03/04/06/10 and exception frames
- **Pure C meter byte decoder** — `combineBytes()` and `decodeMeasurement()`
  extracted to `meter_decode.c`, covering all 4 endianness modes and 3 data types
  (INT16, INT32, FLOAT32) with 30 test scenarios
- **Pure C HomeWizard P1 parser** — JSON response parsing extracted to `p1_parse.c`
  with sign correction from power direction, tested with real Kaifa P1 responses
- **Meter telemetry counters** — per-meter request/response/CRC-error/timeout
  counters for diagnosing communication issues
- **Modbus frame event logger** — 32-entry ring buffer capturing TX/RX/ERR frames
  with address, function code, register, and timestamp for debugging
- **API/MQTT staleness detection** — configurable timeout (default 120s) for
  API-fed mains current; falls back to MaxMains on expiry instead of stopping
  charging ([PR #86](https://github.com/basmeerman/SmartEVSE-3.5/pull/86))
- **HomeWizard P1 energy data** — reads `total_power_import_kwh` /
  `total_power_export_kwh` for the HA energy dashboard
  ([PR #86](https://github.com/basmeerman/SmartEVSE-3.5/pull/86))
- **HomeWizard P1 manual IP fallback** — `Set/HomeWizardIP` MQTT command bypasses
  mDNS discovery for networks where it's unreliable
  ([PR #86](https://github.com/basmeerman/SmartEVSE-3.5/pull/86))
- **Comprehensive power input guide** — [5-method comparison](docs/power-input-methods.md)
  with reliability ranking, decision tree, setup guides, and troubleshooting

See [Power Input Methods](docs/power-input-methods.md) for choosing and configuring
your metering method.

## EVCC Integration

- REST API integration with [EVCC](https://evcc.io/) energy management system
- WiFi-only setup — no RS485 Modbus wiring required

### Fork improvements

- **IEC 61851-1 state mapping** — pure C function maps internal SmartEVSE states
  to standard IEC 61851 letters (A-F), with correct soft/hard error distinction.
  Hard errors (RCM, overcurrent, temperature) map to 'E'; soft errors (LESS_6A,
  NO_SUN) preserve the underlying state
- **Phase switching via HTTP** — `POST /settings?phases=1|3` triggers safe 1P/3P
  switching with full validation (C2 contactor present, master/standalone only).
  The state machine handles the disconnect-switch-reconnect sequence automatically
- **Charging state derivation** — `charging_enabled` boolean in GET /settings
  response, derived from STATE_C/STATE_C1
- **Ready-to-use EVCC template** — complete `evcc.yaml` custom charger template
  included in documentation

See [EVCC Integration](docs/evcc-integration.md) for setup guide and charger
template.

## Diagnostic Telemetry

### Fork improvements ([PR #84](https://github.com/basmeerman/SmartEVSE-3.5/pull/84))

- **Ring buffer event capture** — captures state machine events, errors, meter
  readings, and load balancing data in a 64-entry ring buffer
- **LittleFS persistence** — diagnostic snapshots survive reboots; auto-triggered
  on errors and configurable profiles (general, solar, loadbal, modbus, fast)
- **WebSocket live stream** — real-time diagnostic events via WebSocket for the
  web UI diagnostic viewer
- **Test replay framework** — replay recorded diagnostic sessions through the
  native test suite for offline debugging
- **MQTT profile control** — `Set/DiagProfile` command to start/stop diagnostic
  capture remotely

## ERE Session Logging

### Fork improvements ([PR #89](https://github.com/basmeerman/SmartEVSE-3.5/pull/89))

- **Charge session tracking** — automatic session recording on every charge cycle
  with start/end timestamps, energy charged (kWh), peak current, phases, and mode
- **ERE-compatible output** — JSON format includes all fields required for Dutch ERE
  (Emissie Reductie Eenheden) certificate submission to an inboekdienstverlener
- **MQTT session publish** — retained JSON message on `<prefix>/Session/Complete`
  on session end; Home Assistant or any MQTT logger collects sessions for annual
  CSV export
- **REST endpoint** — `GET /session/last` returns the last completed session as JSON
  (or 204 if no session yet)
- **OCPP alignment** — sessions are flagged when OCPP is managing the transaction,
  using the same energy meter readings as OCPP StartTransaction/StopTransaction
- **Zero flash wear** — MQTT-only persistence; no flash writes, no RAM ring buffer,
  just 32 bytes for the current session

See [ERE Session Logging](docs/ere-session-logging.md) for setup, HA automation
examples, MID meter requirements, and reporting workflow.

## Web & Connectivity

- WiFi status page with real-time monitoring
- REST API for external integration
- Remote control with Smartphone App
- LCD remote control via WebSockets
- Firmware upgradable through USB-C or built-in webserver

### Fork improvements ([PR #85](https://github.com/basmeerman/SmartEVSE-3.5/pull/85))

- **Offline-first web UI** — all CSS/JS/fonts bundled into firmware, no CDN
  dependencies; works on isolated networks
- **WebSocket data channel** — real-time dashboard updates via WebSocket instead
  of HTTP polling; 1-second refresh with automatic reconnect
- **Dashboard card redesign** — modern card-based layout with power flow diagram
  showing grid → EVSE → EV energy flow
- **Dark mode** — automatic dark/light theme based on system preference, with
  manual toggle
- **Load balancing node overview** — live multi-node status panel showing all
  connected EVSEs with per-node current, state, and priority
- **Diagnostic telemetry viewer** — browse, filter, and replay diagnostic
  captures directly in the web UI
- **LCD widget modernization** — redesigned LCD remote control widget with
  responsive layout

## Privacy

- Works perfectly fine without internet — no cloud dependency
- Does not collect or store usage statistics
- No vendor lock-in — open source firmware
- Fork it, modify it, contribute to make it even better

# Getting started

## Connecting to WiFi

Follow the instructions on the [Configuration page](docs/configuration.md#wifi),
WiFi section.

## Updating firmware

Connect to your WiFi network, then browse to `http://smartevse-xxxx.local/update`
(replace `xxxx` with your serial number, shown on the display). Select the
`firmware.bin` and press Update.

# Documentation

| Document | Description |
|----------|-------------|
| [Hardware installation](docs/installation.md) | Wiring, mounting, contactor setup |
| [Power Input Methods](docs/power-input-methods.md) | Metering options: Modbus, Sensorbox, HomeWizard, MQTT — reliability ranking, setup, troubleshooting |
| [Configuration](docs/configuration.md) | LCD menu settings reference |
| [Operation](docs/operation.md) | Day-to-day usage guide |
| [Solar & Smart Mode Stability](docs/solar-smart-stability.md) | EMA smoothing, dead bands, phase switch timers, cycling prevention |
| [Load Balancing Stability](docs/load-balancing-stability.md) | Oscillation dampening, EMA filter, distribution smoothing, diagnostics |
| [MQTT & Home Assistant](docs/mqtt-home-assistant.md) | Full topic reference, change-only publishing, entity naming |
| [EVCC Integration](docs/evcc-integration.md) | EVCC charger template, IEC 61851 mapping, phase switching API |
| [REST API reference](docs/REST_API.md) | HTTP endpoints for external integration |
| [ERE Session Logging](docs/ere-session-logging.md) | Charge session tracking for Dutch ERE certificates, HA automation, MID requirements |
| [OCPP setup](docs/ocpp.md) | OCPP 1.6j provider guides (Tap Electric, Tibber, SteVe) and configuration |
| [Priority scheduling](docs/priority-scheduling.md) | Load balancing priority configuration |
| [Building & Flashing](docs/building_flashing.md) | Compiling firmware from source |
| [Coding standards](CODING_STANDARDS.md) | Code conventions for contributors |
| [Contributing](CONTRIBUTING.md) | How to contribute to this project |
| [AI agent instructions](CLAUDE.md) | Multi-agent workflow for Claude Code |

# Testing & Quality

The firmware is verified by a comprehensive native test suite that runs on the host
(no hardware required) and an 8-job CI pipeline.

| Metric | Value |
|--------|-------|
| Test suites | 44 |
| Test scenarios | 900+ |
| Features covered | 60+ |
| Requirement traceability | 100% |

**Test areas** include IEC 61851-1 state transitions, load balancing (single and
multi-node with convergence simulation), Smart/Solar operating modes, OCPP
authorization/connector state/settings validation/telemetry, MQTT command parsing
and publishing, HTTP API validation, EVCC IEC 61851 state mapping, Modbus frame
decoding (FC03/04/06/10), meter byte decoding (4 endianness modes), HomeWizard P1
JSON parsing, meter telemetry counters, API staleness detection, metering
diagnostics, diagnostic telemetry, charge session logging (ERE), error handling &
safety, modem/ISO15118 negotiation, phase switching, bridge transaction integrity,
and end-to-end charging flows.

Every test function carries Specification-by-Example (SbE) annotations (`@feature`,
`@req`, `@scenario`, `@given`/`@when`/`@then`) that trace back to requirements. The
CI pipeline generates two reports on every build:

- **[Test Specification](SmartEVSE-3/test/native/test-specification.md)** — all
  scenarios grouped by feature, with Given/When/Then steps
- **[Traceability Report](SmartEVSE-3/test/native/traceability-report.md)** —
  requirement-to-test coverage matrix

To run locally:

```bash
cd SmartEVSE-3/test/native && make clean test
```

# Roadmap

All improvement plans are complete. Tracked via
[GitHub Projects](https://github.com/basmeerman?tab=projects):

| Status | Project | PRs | Upstream issues |
|--------|---------|-----|----------------|
| Done | Plan 01: Solar & Smart Mode Stability | [#65](https://github.com/basmeerman/SmartEVSE-3.5/pull/65), [#83](https://github.com/basmeerman/SmartEVSE-3.5/pull/83) | [#327](https://github.com/dingo35/SmartEVSE-3.5/issues/327), [#335](https://github.com/dingo35/SmartEVSE-3.5/issues/335), [#316](https://github.com/dingo35/SmartEVSE-3.5/issues/316) |
| Done | Plan 02: Multi-Node Load Balancing | [#69](https://github.com/basmeerman/SmartEVSE-3.5/pull/69) | [#316](https://github.com/dingo35/SmartEVSE-3.5/issues/316) |
| Done | Plan 03: OCPP Robustness | [#77](https://github.com/basmeerman/SmartEVSE-3.5/pull/77), [#79](https://github.com/basmeerman/SmartEVSE-3.5/pull/79), [#80](https://github.com/basmeerman/SmartEVSE-3.5/pull/80) | — |
| Done | Plan 04: EVCC Integration | [#70](https://github.com/basmeerman/SmartEVSE-3.5/pull/70) | [EVCC #13852](https://github.com/evcc-io/evcc/pull/13852) |
| Done | Plan 05: Meter Compatibility & Modbus | [#76](https://github.com/basmeerman/SmartEVSE-3.5/pull/76) | — |
| Done | Plan 06: Diagnostic Telemetry | [#84](https://github.com/basmeerman/SmartEVSE-3.5/pull/84) | — |
| Done | Plan 07: Web UI Modernization | [#85](https://github.com/basmeerman/SmartEVSE-3.5/pull/85) | — |
| Done | Plan 08: HA MQTT Integration | [#64](https://github.com/basmeerman/SmartEVSE-3.5/pull/64), [#68](https://github.com/basmeerman/SmartEVSE-3.5/pull/68), [#82](https://github.com/basmeerman/SmartEVSE-3.5/pull/82) | [#320](https://github.com/dingo35/SmartEVSE-3.5/issues/320), [#294](https://github.com/dingo35/SmartEVSE-3.5/issues/294), [PR #338](https://github.com/dingo35/SmartEVSE-3.5/pull/338) |
| Done | Plan 09: Power Input Methods | [#86](https://github.com/basmeerman/SmartEVSE-3.5/pull/86) | — |
| Done | Plan 10: ERE Session Logging | [#89](https://github.com/basmeerman/SmartEVSE-3.5/pull/89) | — |

# SmartEVSE App

The SmartEVSE-app can be found [here](https://github.com/SmartEVSE/SmartEVSE-app) or on Google Play
