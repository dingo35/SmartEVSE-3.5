SmartEVSE v3
=========

Smart Electric Vehicle Charge Controller — open-source firmware for a
DIN-rail AC charger.

<img src="/pictures/SmartEVSEv3.png" alt="SmartEVSE v3 hardware" width="500">

<img src="/pictures/SmartEVSEv3-WebUI.jpg" alt="SmartEVSE v3 Web UI" width="500">

# What it is

An EVSE (Electric Vehicle Supply Equipment) — the controller between
your house wiring and an electric vehicle. It supports:

- 1-phase and 3-phase AC charging, 6–32 A.
- Fixed charging cable or Type 2 socket.
- 5 locking actuator types.
- Direct drive of a mains contactor.
- Up to 8 modules sharing one mains supply (load balancing).
- RFID authorization (100 cards).
- 18 supported Modbus kWh meters + Sensorbox + HomeWizard P1 + REST API.
- MQTT + Home Assistant auto-discovery.
- REST API for scripts and dashboards.
- OCPP 1.6j for public / billed charging.
- Offline-first Web UI, dark mode, live WebSocket updates.
- LCD + button configuration on the device itself.

Operating voltage 110–240 VAC. DIN-mount, 3 modules wide (52 × 91 × 58 mm).

# Pick your path

| You are | Start here |
|---|---|
| Installing the hardware (you have the PCB in hand) | [**Installer guide**](docs/guide-installer.md) |
| Using an already-installed charger | [**Owner guide**](docs/guide-owner.md) |
| Integrating with Home Assistant / MQTT / REST / OCPP | [**Integrator guide**](docs/guide-integrator.md) |
| Something is wrong | [**Troubleshooting**](docs/troubleshooting.md) |
| Adding a feature, fixing a bug, submitting a PR | [**Contributor guide**](docs/guide-contributor.md) |
| Wondering about LAN security / firmware signing / auth | [**Security**](docs/security.md) |

# Community

- **Dutch-language forum:** [Tweakers GoT — *Zelfbouw Laadpaal ervaringen*](https://gathering.tweakers.net/forum/list_messages/1648387).
  The canonical community thread with a decade of accumulated buying,
  installation, and integration experience. Maintainers and experienced
  users read and answer there.
- **Issues:** [GitHub Issues](https://github.com/basmeerman/SmartEVSE-3.5/issues).
- **Releases:** [Releases page](https://github.com/basmeerman/SmartEVSE-3.5/releases).

# Privacy and longevity

- Works fully offline. No cloud account required.
- Does not collect or transmit usage statistics.
- Open-source firmware. Fork it, modify it, keep it running long after
  commercial chargers go EOL.

# Reference documentation

For topic-organised reference material (the deep detail behind the
audience guides):

| Area | Document |
|---|---|
| Hardware installation | [installation.md](docs/installation.md) |
| Power-input methods (Sensorbox / P1 / Modbus / API) | [power-input-methods.md](docs/power-input-methods.md) |
| LCD menu configuration reference | [configuration.md](docs/configuration.md) |
| Settings matrix with access channels | [configuration-matrix.md](docs/configuration-matrix.md) |
| Operation guide | [operation.md](docs/operation.md) |
| MQTT topics + HA discovery | [mqtt-home-assistant.md](docs/mqtt-home-assistant.md) |
| REST API reference | [REST_API.md](docs/REST_API.md) |
| OCPP 1.6j coverage | [ocpp.md](docs/ocpp.md) |
| Solar / Smart mode tuning | [solar-smart-stability.md](docs/solar-smart-stability.md) |
| Load balancing behaviour | [load-balancing-stability.md](docs/load-balancing-stability.md) |
| Priority scheduling (multi-node) | [priority-scheduling.md](docs/priority-scheduling.md) |
| EVCC integration | [evcc-integration.md](docs/evcc-integration.md) |
| ERE session logging | [ere-session-logging.md](docs/ere-session-logging.md) |
| Building and flashing firmware | [building_flashing.md](docs/building_flashing.md) |
| Feature catalogue | [features.md](docs/features.md) |
| Quality engineering + CI/CD | [quality.md](docs/quality.md) |

# About this repository

This repository is maintained independently. It diverges from the
reference codebase primarily in two areas: (1) a restructured
architecture that enables native host testing of the core logic
(1,200+ automated tests across 50 suites — state machine, parsers,
validators, OCPP, Modbus), and (2) security hardening of the Web UI and
firmware update paths (signed firmware, per-endpoint auth gate, PIN
rate limiter, CSRF/Origin check, secret redaction).

The refactor used multi-agent AI-assisted development (Claude Code) as
a real-world test of the methodology on safety-critical embedded
firmware. That's how the test suite and the security work landed so
quickly. Details in [quality.md](docs/quality.md) for those interested;
it doesn't affect how you install, use, or contribute.

See [upstream-differences.md](docs/upstream-differences.md) for the
complete diff against the reference codebase.

# Getting started

## Connecting to WiFi

See [configuration.md, WiFi section](docs/configuration.md#wifi).

## Updating firmware

Connect to WiFi, then browse to
`http://smartevse-<serial>.local/update` (serial shown on the LCD).
Select `firmware.signed.bin` from the releases page and upload.

For the full install flow from boxed PCB to commissioned charger, see
the [installer guide](docs/guide-installer.md).
