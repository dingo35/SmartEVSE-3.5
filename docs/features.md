# Features & Unique Selling Points

This document details the features of the SmartEVSE firmware and the specific
improvements made in this fork. For a summary of differences with the upstream
repository, see [Upstream Differences](upstream-differences.md).

---

## Charging (Core)

SmartEVSE is a versatile EVSE controller that works with all EVs and plug-in
hybrids:

- **1-3 phase charging** — fixed cable or socket with locking actuator (5 types
  supported)
- **Automatic cable detection** — selects current capacity of the connected
  cable (13/16/32A) via proximity pilot (PP) resistance
- **Dual contactor outputs** — two switched 230VAC outputs enable automatic
  1-phase to 3-phase switching
- **RS485 communication bus** — powered bus for Sensorbox and Modbus kWh meters
- **Thermal protection** — built-in temperature sensor with automatic current
  reduction and shutdown
- **Wide input voltage** — operates on 110-240 VAC
- **Compact form factor** — 52 × 91 × 58 mm (3 DIN modules)

---

## Solar & Smart Mode

### Smart Mode

Automatically adjusts charge current based on other household consumption to
stay within mains capacity. Reads real-time consumption from the mains meter
(Modbus, Sensorbox, HomeWizard P1, or API) and calculates available headroom.

### Solar Mode

Charges from solar surplus with configurable start/stop thresholds and import
allowance. Supports automatic 1P/3P phase switching based on available power
(requires CONTACT 2 wiring).

### Fork Improvements

These address oscillation and stability issues reported in upstream
[#327](https://github.com/dingo35/SmartEVSE-3.5/issues/327),
[#335](https://github.com/dingo35/SmartEVSE-3.5/issues/335),
[#316](https://github.com/dingo35/SmartEVSE-3.5/issues/316):

- **EMA current smoothing** — configurable exponential moving average filter
  dampens oscillation by smoothing noisy meter readings. Higher alpha values
  respond faster but smooth less; lower values are more stable but slower.
- **Dead band regulation** — suppresses micro-adjustments when the current
  difference is within a configurable band (e.g., ±0.5A). Prevents unnecessary
  contactor cycling from measurement noise.
- **Symmetric ramp rates** — equal ramp-up and ramp-down speeds prevent the
  overshoot/undershoot oscillation caused by the upstream's asymmetric
  regulation (fast up, slow down).
- **Tiered phase switching timers** — separate fast timer for severe overload
  (e.g., 30s) and configurable hold-down guard (e.g., 5 minutes) to prevent
  rapid 1P/3P cycling.
- **Stop/start cycling prevention** — higher NoCurrent threshold, gradual
  current decay before stopping, solar minimum run time, and shorter solar
  charge delay reduce unnecessary stop/start cycles.
- **Slow EV compatibility** — settling window and ramp rate limiter for EVs
  like the Renault Zoe that stall on rapid current changes.

Configuration: [Solar & Smart Mode Stability](solar-smart-stability.md)

---

## Load Balancing & Power Sharing

Up to 8 SmartEVSEs share one mains connection without overloading it. Supports
priority-based power scheduling with configurable rotation intervals and
delayed charging.

### Fork Improvements

Addresses upstream [#316](https://github.com/dingo35/SmartEVSE-3.5/issues/316):

- **Oscillation dampening** — detects current hunting via sign-flip detection
  on Idifference, adaptively increases the regulation divisor to slow down
  adjustments until the system stabilizes.
- **EMA filter on Idifference** — exponential moving average (25% alpha)
  smooths measurement noise from current transducers, reducing false regulation
  triggers while preserving convergence speed.
- **Distribution smoothing** — per-EVSE delta clamping limits current changes
  to max 3.0A per cycle, preventing contactor stress and EV charge controller
  destabilization from sudden jumps.
- **Load balancing diagnostic snapshot** — per-cycle diagnostic struct captures
  IsetBalanced, filtered Idifference, baseload, per-EVSE allocations,
  oscillation count, and shortage/clamping flags. Available via MQTT for
  debugging.
- **126 convergence tests** — multi-cycle simulation test suite covering 2-8
  node configurations, priority scheduling under shortage, node join/leave
  transitions, and vehicle response lag modeling.

Configuration: [Load Balancing Stability](load-balancing-stability.md)

---

## RFID, OCPP & Authorization

### Base Features

- **RFID reader** — restrict usage to up to 100 registered cards
- **OCPP 1.6j** — backend authorization with Tap Electric, Tibber, SteVe, Monta

### Fork Improvements

- **Fixed RFID toggle bug** — `AccessStatus` is now cleared on all disconnect
  paths (including Tesla C→B→A), preventing the next RFID swipe from toggling
  OFF instead of ON.
- **Bridge transaction mutex** — FreeRTOS mutex prevents concurrent task
  corruption of the state context, fixing daily OCPP session failures after
  Tesla disconnects.
- **Pure C OCPP logic extraction** — authorization decisions, connector state
  detection, RFID formatting, and settings validation extracted to
  `ocpp_logic.c` for native testability (85 OCPP-specific tests).
- **LoadBl exclusivity enforcement** — OCPP Smart Charging and internal load
  balancing are mutually exclusive; now enforced at runtime with warnings when
  conflict detected (previously only checked at init).
- **FreeVend solar safety** — auto-authorize (FreeVend) no longer bypasses
  solar surplus checks or ChargeDelay, preventing charging without sunlight.
- **OCPP settings validation** — backend URL (`ws://`/`wss://`), ChargeBoxId
  (max 20 chars, printable ASCII), and auth key validated before passing to
  MicroOcpp library.
- **OCPP connection telemetry** — WebSocket connect/disconnect counters,
  transaction lifecycle tracking, authorization accept/reject/timeout metrics.
- **IEC 61851 → OCPP status mapping** — maps IEC states to OCPP 1.6
  StatusNotification values (Available, Preparing, Charging, SuspendedEV/EVSE,
  Finishing, Faulted).

Configuration: [OCPP setup](ocpp.md)

---

## MQTT & Home Assistant

### Base Features

- **MQTT API** — communication with Home Assistant and other software
- **Auto-discovery** — automatic HA entity setup

### Fork Improvements

Addresses upstream
[#320](https://github.com/dingo35/SmartEVSE-3.5/issues/320),
[#294](https://github.com/dingo35/SmartEVSE-3.5/issues/294),
[PR #338](https://github.com/dingo35/SmartEVSE-3.5/pull/338):

- **Change-only publishing** — 70-97% message reduction by only publishing
  changed values, with configurable heartbeat interval (default 60s).
- **Fixed HA discovery payloads** — corrected `state_class` for energy sensors,
  preventing corrupted long-term statistics.
- **Energy zero-value guard** — energy values only published when > 0,
  preventing phantom consumption in HA energy dashboard.
- **Entity naming cleanup** — snake_case entity IDs for HA 2025.10+
  compatibility.
- **New entities** — MaxSumMains (settable), FreeHeap, MQTTMsgCount, LoadBl,
  PairingPin, FirmwareVersion (diagnostic).
- **Per-phase power via MQTT** — 6 new topics (`MainsPowerL1/L2/L3`,
  `EVPowerL1/L2/L3`) with HA auto-discovery.
- **Per-phase energy via MQTT** — 6 new topics for per-phase energy data with
  HA auto-discovery (`state_class=total_increasing`).
- **Metering diagnostic counters** — `MeterTimeoutCount`, `MeterRecoveryCount`,
  `ApiStaleCount` published as HA diagnostic entities.

Configuration: [MQTT & Home Assistant](mqtt-home-assistant.md)

---

## Metering & Modbus

### Base Features

- **18 supported meter types** via Modbus RTU — Eastron SDM630/SDM120, ABB B23,
  Finder 7E/7M, Phoenix Contact, Schneider, Chint, Carlo Gavazzi, SolarEdge,
  WAGO, Sinotimer, Orno WE-517/516, and Custom
- **HomeWizard P1** — WiFi/HTTP smart meter support
- **Sensorbox v1/v2** — CT or P1 input
- **API/MQTT external feed** — external current data via REST or MQTT

### Fork Improvements

- **New meter types: Orno WE-517 (3P) and WE-516 (1P)** — community-requested
  bidirectional energy meters.
- **Pure C Modbus frame decoder** — `ModbusDecode()` extracted to
  `modbus_decode.c` for native testability, supporting FC03/04/06/10 and
  exception frames.
- **Pure C meter byte decoder** — `combineBytes()` and `decodeMeasurement()`
  extracted to `meter_decode.c`, covering all 4 endianness modes and 3 data
  types (INT16, INT32, FLOAT32) with 30 test scenarios.
- **Pure C HomeWizard P1 parser** — JSON response parsing extracted to
  `p1_parse.c` with sign correction from power direction.
- **Meter telemetry counters** — per-meter request/response/CRC-error/timeout
  counters for diagnosing communication issues.
- **Modbus frame event logger** — 32-entry ring buffer capturing TX/RX/ERR
  frames with address, function code, register, and timestamp.
- **API/MQTT staleness detection** — configurable timeout (default 120s) for
  API-fed mains current; falls back to MaxMains on expiry.
- **HomeWizard P1 energy data** — reads `total_power_import_kwh` /
  `total_power_export_kwh` for the HA energy dashboard.
- **HomeWizard P1 manual IP fallback** — `Set/HomeWizardIP` MQTT command
  bypasses mDNS discovery for unreliable networks.

Configuration: [Power Input Methods](power-input-methods.md)

---

## EVCC Integration

REST API integration with [EVCC](https://evcc.io/) energy management system.
WiFi-only setup — no RS485 Modbus wiring required.

### Fork Improvements

- **IEC 61851-1 state mapping** — pure C function maps internal SmartEVSE
  states to standard IEC 61851 letters (A-F), with correct soft/hard error
  distinction.
- **Phase switching via HTTP** — `POST /settings?phases=1|3` triggers safe
  1P/3P switching with full validation.
- **Charging state derivation** — `charging_enabled` boolean in GET /settings
  response, derived from STATE_C/STATE_C1.
- **Ready-to-use EVCC template** — complete `evcc.yaml` custom charger
  template included in documentation.

Configuration: [EVCC Integration](evcc-integration.md)

---

## Diagnostic Telemetry

*New in this fork.*
([PR #84](https://github.com/basmeerman/SmartEVSE-3.5/pull/84))

- **Ring buffer event capture** — captures state machine events, errors, meter
  readings, and load balancing data in a 64-entry ring buffer.
- **LittleFS persistence** — diagnostic snapshots survive reboots;
  auto-triggered on errors and configurable profiles.
- **WebSocket live stream** — real-time diagnostic events via WebSocket for
  the web UI diagnostic viewer.
- **Test replay framework** — replay recorded diagnostic sessions through the
  native test suite for offline debugging.
- **MQTT profile control** — `Set/DiagProfile` command to start/stop
  diagnostic capture remotely.

---

## ERE Session Logging

*New in this fork.*
([PR #89](https://github.com/basmeerman/SmartEVSE-3.5/pull/89))

- **Charge session tracking** — automatic session recording on every charge
  cycle with start/end timestamps, energy charged (kWh), peak current, phases,
  and mode.
- **ERE-compatible output** — JSON format includes all fields required for
  Dutch ERE (Emissie Reductie Eenheden) certificate submission.
- **MQTT session publish** — retained JSON message on `<prefix>/Session/Complete`
  on session end.
- **REST endpoint** — `GET /session/last` returns the last completed session.
- **OCPP alignment** — sessions flagged when OCPP manages the transaction.
- **Zero flash wear** — MQTT-only persistence; no flash writes.

Configuration: [ERE Session Logging](ere-session-logging.md)

---

## Web & Connectivity

### Base Features

- WiFi status page with real-time monitoring
- REST API for external integration
- Remote control with Smartphone App
- LCD remote control via WebSockets
- Firmware upgradable through USB-C or built-in webserver

### Fork Improvements

([PR #85](https://github.com/basmeerman/SmartEVSE-3.5/pull/85))

- **Offline-first web UI** — all CSS/JS/fonts bundled into firmware, no CDN
  dependencies; works on isolated networks.
- **WebSocket data channel** — real-time dashboard updates via WebSocket
  instead of HTTP polling; 1-second refresh with automatic reconnect.
- **Dashboard card redesign** — modern card-based layout with power flow
  diagram showing grid → EVSE → EV energy flow.
- **Dark mode** — automatic dark/light theme based on system preference,
  with manual toggle.
- **Load balancing node overview** — live multi-node status panel showing all
  connected EVSEs with per-node current, state, and priority.
- **Diagnostic telemetry viewer** — browse, filter, and replay diagnostic
  captures directly in the web UI.
- **LCD widget modernization** — redesigned LCD remote control widget with
  responsive layout.

---

## Privacy

- Works perfectly fine without internet — no cloud dependency
- Does not collect or store usage statistics
- No vendor lock-in — open source firmware
- Fork it, modify it, contribute to make it even better
