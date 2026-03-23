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
~3,000-line `main.cpp`. This fork restructures the architecture to enable **native host
testing** of the core logic — pure C modules, context structs, a bridge layer, and HAL
callbacks — resulting in 1,100+ automated tests (900+ native C tests across 44 suites,
50 OCPP protocol tests, and 146 Modbus compatibility tests).

See [Quality Engineering](docs/quality.md) for the full architecture, testing
methodology, CI/CD pipeline, and hardening approach. See
[Upstream Differences](docs/upstream-differences.md) for a complete list of changes
compared to the upstream repository.

# What is it?

An open source EVSE (Electric Vehicle Supply Equipment). It supports 1-3 phase
charging, fixed charging cable or charging socket, locking actuator support (5 types),
and it can directly drive a mains contactor for supplying power to the EV. It features
a display for parameter configuration. Up to 8 modules can be connected together to
charge up to eight EVs from one mains connection without overloading it.

# Key Features

| Feature | Highlights |
|---------|-----------|
| **Charging** | 1-3 phase, auto cable detection (13/16/32A), dual contactor outputs, thermal protection |
| **Solar & Smart Mode** | Solar surplus charging, auto 1P/3P switching, EMA smoothing, dead band regulation |
| **Load Balancing** | Up to 8 nodes, priority scheduling, oscillation dampening, convergence testing |
| **OCPP & Authorization** | OCPP 1.6j, RFID (100 cards), pure C logic extraction (85 tests), 50 protocol tests, FreeVend solar safety |
| **MQTT & Home Assistant** | Change-only publishing (70-97% reduction), fixed HA discovery, per-phase power/energy |
| **Metering** | 18 Modbus meter types, 146 compatibility tests, HomeWizard P1, Sensorbox, API/MQTT feed, staleness detection |
| **EVCC Integration** | IEC 61851 state mapping, HTTP phase switching, ready-to-use template |
| **Diagnostics** | Ring buffer events, LittleFS persistence, WebSocket live stream, test replay |
| **ERE Session Logging** | Dutch ERE certificate output, MQTT publish, REST endpoint, zero flash wear |
| **Web UI** | Offline-first, WebSocket updates, dark mode, load balancing dashboard, diagnostic viewer |
| **Privacy** | No cloud, no tracking, open source |

For detailed feature descriptions and fork improvements, see [Features](docs/features.md).

# Getting started

## Connecting to WiFi

Follow the instructions on the [Configuration page](docs/configuration.md#wifi),
WiFi section.

## Updating firmware

Connect to your WiFi network, then browse to `http://smartevse-xxxx.local/update`
(replace `xxxx` with your serial number, shown on the display). Select the
`firmware.bin` and press Update.

# Documentation

## User Documentation

| Document | Description |
|----------|-------------|
| [Hardware installation](docs/installation.md) | Wiring, mounting, contactor setup |
| [Power Input Methods](docs/power-input-methods.md) | Metering options — reliability ranking, setup, troubleshooting |
| [Configuration](docs/configuration.md) | LCD menu settings reference |
| [Settings Reference](docs/configuration-matrix.md) | All settings by access channel with safety flags |
| [Operation](docs/operation.md) | Day-to-day usage guide |

## Feature Documentation

| Document | Description |
|----------|-------------|
| [Features & USPs](docs/features.md) | All features with fork improvements |
| [Upstream Differences](docs/upstream-differences.md) | Complete diff with upstream repo |
| [Solar & Smart Mode Stability](docs/solar-smart-stability.md) | EMA smoothing, dead bands, phase switch timers |
| [Load Balancing Stability](docs/load-balancing-stability.md) | Oscillation dampening, diagnostics |
| [MQTT & Home Assistant](docs/mqtt-home-assistant.md) | Topic reference, HA auto-discovery |
| [EVCC Integration](docs/evcc-integration.md) | EVCC charger template, phase switching API |
| [REST API reference](docs/REST_API.md) | HTTP endpoints for external integration |
| [ERE Session Logging](docs/ere-session-logging.md) | Dutch ERE certificates, HA automation |
| [OCPP setup](docs/ocpp.md) | Provider guides (Tap Electric, Tibber, SteVe) |
| [Priority scheduling](docs/priority-scheduling.md) | Load balancing priority configuration |

## Developer Documentation

| Document | Description |
|----------|-------------|
| [Quality Engineering](docs/quality.md) | Architecture, testing, CI/CD, hardening, interoperability |
| [Building & Flashing](docs/building_flashing.md) | Compiling firmware from source |
| [Coding standards](CODING_STANDARDS.md) | Code conventions for contributors |
| [Contributing](CONTRIBUTING.md) | How to contribute to this project |
| [AI agent instructions](CLAUDE.md) | Multi-agent workflow for Claude Code |

# Roadmap

Completed improvement plans, tracked via
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
| Done | Plan 11: OCPP Compatibility Testing | [#96](https://github.com/basmeerman/SmartEVSE-3.5/pull/96) | — |
| Done | Plan 12: Modbus Compatibility Testing | [#97](https://github.com/basmeerman/SmartEVSE-3.5/pull/97) | — |

All 12 improvement plans are complete. The CI/CD pipeline runs a 10-job quality
gate on every PR, including OCPP interoperability tests (mock CSMS via
[mobilityhouse/ocpp](https://github.com/mobilityhouse/ocpp)) and Modbus
compatibility tests (C decode functions called from Python via ctypes). See
[Quality Engineering](docs/quality.md) for details.

# SmartEVSE App

The SmartEVSE-app can be found [here](https://github.com/SmartEVSE/SmartEVSE-app) or on Google Play
