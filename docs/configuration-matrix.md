# Settings Reference

Complete reference of all SmartEVSE settings — what they do, where you can read
or change them, and which are safety-relevant.

For how to configure each setting, see [Configuration](configuration.md).

## Access channels

| Channel | Abbreviation | Description |
|---------|-------------|-------------|
| **LCD** | L | Physical buttons on the device |
| **Web UI** | W | Browser-based dashboard (`http://smartevse-xxxx.local`) |
| **REST API** | R | HTTP GET/POST to `/settings`, `/currents` |
| **MQTT** | M | MQTT `Set/` topics (Home Assistant, Node-RED, etc.) |

**Legend:** R = read, W = write, RW = read + write, — = not available

## Safety notice

Settings marked with **⚠** are safety-relevant — they directly affect contactor
control, current limits, or protection features. Changing these remotely (via
MQTT, REST API, or web UI) is possible but should be done with care. For maximum
safety, configure these on the device LCD during initial installation.

## Charging control

| Setting | Range | Default | LCD | Web | REST | MQTT | Saved | ⚠ | Notes |
|---------|-------|---------|-----|-----|------|------|-------|---|-------|
| MODE | Normal / Smart / Solar | Normal | RW | RW | RW | `Set/Mode` | Yes | **⚠** | Also accepts Off, Pause via MQTT |
| MAX (charge current) | 6–80 A | 13 A | RW | RW | RW | via CurrentOverride | Yes | **⚠** | Per-phase limit, capped by cable |
| MIN (charge current) | 6–16 A | 6 A | RW | RW | RW | — | Yes | **⚠** | Minimum per-phase current |
| Override current | 0–MAX×10 dA | 0 | — | RW | RW | `Set/CurrentOverride` | No | **⚠** | Temporary; resets on power loss |
| Custom button | 0 / 1 | 0 | — | RW | RW | `Set/CustomButton` | No | | Software button override |
| CP PWM override | -1 to 1024 | -1 | — | — | RW | `Set/CPPWMOverride` | No | **⚠** | Testing only; -1=normal |

## Current limits and power sharing

| Setting | Range | Default | LCD | Web | REST | MQTT | Saved | ⚠ | Notes |
|---------|-------|---------|-----|-----|------|------|-------|---|-------|
| MAINS (max mains) | 10–200 A | 25 A | RW | RW | RW | — | Yes | **⚠** | Total mains capacity per phase |
| CIRCUIT (breaker) | 10–160 A | 16 A | RW | RW | RW | — | Yes | **⚠** | EVSE sub-panel breaker limit |
| MaxSumMains (capacity) | 0, 10–600 A | 0 | RW | RW | RW | `Set/CurrentMaxSumMains` | Yes | **⚠** | EU capacity rate; 0=disabled |
| Sum Mains timeout | 0–60 min | 0 | RW | RW | RW | — | Yes | | Wait time when exceeded |
| CapacityLimit (peak) | 0–25000 W | 0 | RW | RW | RW | `Set/CapacityLimit` | Yes | | 15-min peak tracking; 0=disabled |
| PWR SHARE (load bal) | Disabled / Master / Node 1–7 | Disabled | RW | RW | RW | — | Yes | **⚠** | Multi-EVSE power sharing role |

## Solar and smart mode

| Setting | Range | Default | LCD | Web | REST | MQTT | Saved | ⚠ | Notes |
|---------|-------|---------|-----|-----|------|------|-------|---|-------|
| START (solar start) | 0–48 A | 4 A | RW | RW | RW | — | Yes | | Surplus threshold to begin |
| STOP (solar stop time) | 0–60 min | 10 min | RW | RW | RW | — | Yes | | Minutes at MinCurrent before stop |
| IMPORT (max import) | 0–48 A | 0 A | RW | RW | RW | — | Yes | | Allowed grid import in solar |
| Home battery current | ±200 A | 0 | — | R | R | `Set/HomeBatteryCurrent` | No | | Positive=charging, negative=discharging |
| Solar debug | 0 / 1 | 0 | — | — | — | `Set/SolarDebug` | No | | Verbose solar logging |

## Access and security

| Setting | Range | Default | LCD | Web | REST | MQTT | Saved | ⚠ | Notes |
|---------|-------|---------|-----|-----|------|------|-------|---|-------|
| LOCK (cable lock) | Disabled / Solenoid / Motor | Disabled | RW | RW | RW | `Set/CableLock` | Yes | **⚠** | Socket only |
| RFID | Disabled / Enabled / Learn / Delete | Disabled | RW | RW | RW | — | Yes | | RFID reader mode |
| LCD PIN | 0–9999 | 0 | RW | RW | RW | — | Yes | | Web LCD remote PIN; 0=no lock |
| Required EVCCID | up to 32 chars | — | — | RW | RW | `Set/RequiredEVCCID` | Yes | **⚠** | ISO 15118 vehicle ID filter |

## Phase control

| Setting | Range | Default | LCD | Web | REST | MQTT | Saved | ⚠ | Notes |
|---------|-------|---------|-----|-----|------|------|-------|---|-------|
| CONTACT 2 (C2) | Not present / Always Off / Solar Off / Always On / Auto | Always On | RW | RW | RW | `Set/EnableC2` | Yes | **⚠** | Controls 1P/3P switching |
| Phase switch request | 1 / 3 | — | — | RW | RW | — | No | **⚠** | Requires C2 present + master |

## Meter configuration

| Setting | Range | Default | LCD | Web | REST | MQTT | Saved | ⚠ | Notes |
|---------|-------|---------|-----|-----|------|------|-------|---|-------|
| MAINS MET (type) | Disabled / Sensorbox / API / meter types | Disabled | RW | RW | RW | — | Yes | | See [Power Input Methods](power-input-methods.md) |
| MAINS ADR | 10–247 | 10 | RW | RW | RW | — | Yes | | Modbus address |
| MainsMeter timeout | 0, 10–3600 s | 120 s | — | — | — | `Set/MainsMeterTimeout` | No | **⚠** | API staleness; 0=disabled |
| MainsMeter feed | L1:L2:L3 dA | — | — | R | R | `Set/MainsMeter` | No | | Real-time current feed |
| EV METER (type) | Disabled / meter types | Disabled | RW | RW | RW | — | Yes | | EV meter selection |
| EV ADR | 11–247 | 12 | RW | RW | RW | — | Yes | | Modbus address |
| EVMeter feed | L1:L2:L3:W:Wh | — | — | R | R | `Set/EVMeter` | No | | Real-time current/power/energy |
| GRID | 4Wire / 3Wire | 4Wire | RW | RW | RW | — | Yes | | Sensorbox CT orientation |
| SB2 WIFI | Disabled / Enabled / Portal | Disabled | RW | RW | RW | — | Yes | | Sensorbox v2 WiFi control |
| HomeWizard IP | up to 15 chars | — | — | — | — | `Set/HomeWizardIP` | No | | Manual IP; empty=mDNS |
| Custom meter registers | various | 0 | RW | — | RW | — | Yes | | 8 register config settings |

## Hardware

| Setting | Range | Default | LCD | Web | REST | MQTT | Saved | ⚠ | Notes |
|---------|-------|---------|-----|-----|------|------|-------|---|-------|
| CONFIG | Socket / Fixed | Socket | RW | RW | RW | — | Yes | | Cable type |
| SWITCH | Disabled / Access / Smart-Solar / ... | Disabled | RW | RW | RW | — | Yes | | External switch function |
| RCMON | Disabled / Enabled | Disabled | RW | RW | RW | — | Yes | **⚠** | Residual current monitor |
| MAX TEMP | 40–75 °C | 65 °C | RW | RW | RW | — | Yes | **⚠** | Thermal protection threshold |

## Multi-node scheduling (master only)

| Setting | Range | Default | LCD | Web | REST | MQTT | Saved | ⚠ | Notes |
|---------|-------|---------|-----|-----|------|------|-------|---|-------|
| Priority strategy | Address / First / Last | Address | RW | RW | RW | `Set/PrioStrategy` | Yes | | How EVSEs are prioritized |
| Rotation interval | 0, 30–1440 min | 0 | RW | RW | RW | `Set/RotationInterval` | Yes | | 0=disabled; 1P rotation |
| Idle timeout | 30–300 s | 60 s | RW | RW | RW | `Set/IdleTimeout` | Yes | | Anti-flap timer |

## Network and MQTT

| Setting | Range | Default | LCD | Web | REST | MQTT | Saved | ⚠ | Notes |
|---------|-------|---------|-----|-----|------|------|-------|---|-------|
| WIFI | Disabled / Enabled / Portal | Disabled | RW | RW | RW | — | Yes | | WiFi mode |
| Auto update | Disabled / Enabled | Disabled | RW | RW | RW | — | Yes | | OTA firmware updates |
| MQTT host | hostname | — | — | RW | — | — | Yes | | Broker address |
| MQTT port | 1–65535 | 1883 | — | RW | — | — | Yes | | Broker port |
| MQTT TLS | 0 / 1 | 0 | — | RW | — | — | Yes | | Encrypted connection |
| MQTT username | string | — | — | RW | — | — | Yes | | Broker auth |
| MQTT password | string | — | — | W | — | — | Yes | | Write-only |
| MQTT heartbeat | 10–300 s | 60 s | — | RW | RW | `Set/MQTTHeartbeat` | Yes | | Force re-publish interval |
| MQTT change-only | 0 / 1 | 1 | — | RW | RW | `Set/MQTTChangeOnly` | Yes | | Only publish on change |

## OCPP

| Setting | Range | Default | LCD | Web | REST | MQTT | Saved | ⚠ | Notes |
|---------|-------|---------|-----|-----|------|------|-------|---|-------|
| OCPP mode | Disabled / Enabled | Disabled | — | RW | RW | — | Yes | | OCPP 1.6j protocol |
| Backend URL | ws:// or wss:// | — | — | RW | RW | — | Yes | | WebSocket endpoint |
| Charge Box ID | up to 20 chars | — | — | RW | RW | — | Yes | | Unique charger ID |

See [OCPP setup](ocpp.md) for provider-specific guides.

## LED colors

| Setting | Range | Default | LCD | Web | REST | MQTT | Saved | ⚠ | Notes |
|---------|-------|---------|-----|-----|------|------|-------|---|-------|
| Color Off | R,G,B 0–255 | 0,0,0 | — | RW | RW | `Set/ColorOff` | Yes | | LED when disabled |
| Color Normal | R,G,B 0–255 | 0,255,0 | — | RW | RW | `Set/ColorNormal` | Yes | | LED in Normal mode |
| Color Smart | R,G,B 0–255 | 0,255,0 | — | RW | RW | `Set/ColorSmart` | Yes | | LED in Smart mode |
| Color Solar | R,G,B 0–255 | 255,170,0 | — | RW | RW | `Set/ColorSolar` | Yes | | LED in Solar mode |
| Color Custom | R,G,B 0–255 | 0,0,255 | — | RW | RW | `Set/ColorCustom` | Yes | | LED for custom button |

## Diagnostics

| Setting | Range | Default | LCD | Web | REST | MQTT | Saved | ⚠ | Notes |
|---------|-------|---------|-----|-----|------|------|-------|---|-------|
| Diag profile | off / general / solar / loadbal / modbus / fast | off | — | — | — | `Set/DiagProfile` | No | | Diagnostic capture mode |

## Read-only status values

These values are published via MQTT and available in the REST API (`GET /settings`)
but cannot be changed by the user.

| Value | LCD | Web | REST | MQTT | Notes |
|-------|-----|-----|------|------|-------|
| State (A/B/C/D/E/F) | R | R | R | Published | IEC 61851 charge state |
| Error flags | R | R | R | Published | Bitmask: CT_NOCOMM, TEMP_HIGH, etc. |
| Charge current | R | R | R | Published | Current charging rate (dA) |
| Temperature | R | R | R | Published | EVSE module temp (°C) |
| Nr of phases | — | R | R | Published | Phases currently in use |
| Serial number | — | R | R | Published | Device unique ID |
| WiFi SSID / RSSI / BSSID | — | R | R | Published | Connection info |
| Firmware version | — | R | R | Published | Running firmware |
| Free heap | — | — | R | Published | ESP32 free memory |
| MQTT message count | — | — | R | Published | Messages since boot |
| Meter timeout count | — | — | — | Published | CT_NOCOMM events since boot |
| Meter recovery count | — | — | — | Published | CT_NOCOMM recoveries since boot |
| API stale count | — | — | — | Published | API staleness events since boot |
| Solar stop timer | — | R | R | Published | Seconds until solar stop |
| OCPP status | — | R | R | Published | Connection state |
| OCPP current limit | — | R | R | Published | Backend charging limit |

## Persistence

- **Saved = Yes**: Setting is stored in non-volatile storage (NVS) and survives
  power loss and firmware updates.
- **Saved = No**: Setting is temporary — it resets to default on reboot. This
  includes override current, access status, diagnostic profiles, and real-time
  meter feeds.

## Safety-relevant settings summary

The following settings directly affect electrical safety. They should be carefully
configured during installation, preferably via the LCD menu on the physical device:

| Setting | Why it's safety-relevant |
|---------|------------------------|
| **MODE** | Controls whether contactors engage and how current is regulated |
| **MAX / MIN current** | Defines the current range — too high risks overcurrent, too low may cause EV errors |
| **MAINS** | Total mains capacity — overestimating risks breaker trips or fire |
| **CIRCUIT** | Sub-panel breaker limit — must match physical breaker |
| **MaxSumMains** | EU capacity rate limit — incorrect value causes billing issues |
| **PWR SHARE** | Multi-node mode — incorrect role causes unbalanced loads |
| **CONTACT 2** | Phase switching — incorrect setting can switch phases under load |
| **LOCK** | Cable lock type — must match installed actuator |
| **RCMON** | Residual current monitor — disabling removes ground fault protection |
| **MAX TEMP** | Thermal shutdown — raising above 65°C may allow overheating |
| **Override current** | Bypasses normal regulation — use with care |
| **CP PWM override** | Directly controls pilot signal — testing only |
| **MainsMeter timeout** | Staleness detection — too long may use outdated current data |
| **Required EVCCID** | Vehicle whitelist — blocks unauthorized vehicles |

> **Recommendation:** Configure all safety-relevant settings via the LCD during
> installation. Use remote access (MQTT, REST API, web UI) only for operational
> changes like mode switching and current overrides.
