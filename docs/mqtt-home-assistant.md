# MQTT & Home Assistant Integration

This page documents the MQTT improvements in this fork for Home Assistant users.
These changes fix broken discovery payloads, reduce message volume, add missing
entities, and align entity naming with HA 2025.10+ conventions.

Upstream issues addressed:
[#320](https://github.com/dingo35/SmartEVSE-3.5/issues/320) (MaxSumMains via MQTT),
[#294](https://github.com/dingo35/SmartEVSE-3.5/issues/294) (per-phase kWh),
upstream [PR #338](https://github.com/dingo35/SmartEVSE-3.5/pull/338) (energy zero-guard)

## Changes from upstream

### Discovery payload fixes

| Entity | Was | Now | Impact |
|--------|-----|-----|--------|
| EV Energy Charged | `state_class: total_increasing` | `state_class: total` | Fixes corrupted HA long-term statistics (value resets per session) |
| ESP Uptime | `state_class: measurement` | `state_class: total_increasing` | Enables proper HA statistics tracking |
| Mains/EV Import/Export Energy | no `state_class` | `state_class: total_increasing` | Required for HA energy dashboard |
| Solar Stop Timer | no `state_class` | `state_class: measurement` | Enables HA graphs |

### Energy zero-value guard

Energy values (Import/Export Active Energy) are only published when > 0. Publishing
zero to a `total_increasing` sensor corrupts Home Assistant's 24h/7d/30d statistics,
showing phantom consumption. This is the same fix as upstream
[PR #338](https://github.com/dingo35/SmartEVSE-3.5/pull/338).

### Entity naming (HA 2025.10+)

Entity IDs are now generated in `snake_case` for compatibility with HA 2025.10+
auto-generated entity names:

| Old entity ID | New entity ID |
|---------------|---------------|
| `sensor.smartevse_chargecurrent` | `sensor.smartevse_charge_current` |
| `sensor.smartevse_maxcurrent` | `sensor.smartevse_max_current` |
| `sensor.smartevse_evplugstate` | `sensor.smartevse_ev_plug_state` |

MQTT topics are unchanged — only the HA entity IDs change. Existing MQTT automations
continue to work. HA automations using entity IDs may need updating.

### New entities

| Entity | Type | Category | Description |
|--------|------|----------|-------------|
| `MaxSumMains` | number | config | Maximum total mains current (settable) |
| `LoadBl` | sensor | diagnostic | Load balancing mode |
| `PairingPin` | sensor | diagnostic | Pairing PIN for device discovery |
| `FirmwareVersion` | sensor | diagnostic | Current firmware version |
| `FreeHeap` | sensor | diagnostic | ESP32 free heap memory (bytes) |
| `MQTTMsgCount` | sensor | diagnostic | Total MQTT messages published since boot |

Diagnostic entities are disabled by default in HA. Enable them in the entity
settings if you want to monitor system health.

## Change-only publishing

The biggest improvement: SmartEVSE now publishes MQTT messages **only when values
change**, instead of dumping all ~60 topics every 10 seconds.

### Impact

| Metric | Before | After |
|--------|--------|-------|
| Messages per minute (idle) | ~360 | ~1 |
| Messages per minute (charging) | ~360 | ~10-30 |
| Reduction | — | 70-97% |

Unchanged values are re-published at a configurable **heartbeat interval** to keep
Home Assistant long-term statistics alive.

### Settings

| Setting | Default | Range | Description |
|---------|---------|-------|-------------|
| `MQTTChangeOnly` | 1 (enabled) | 0-1 | Enable/disable change-only publishing |
| `MQTTHeartbeat` | 60 | 10-300 | Seconds between forced re-publish of unchanged values. 0 = never re-publish unchanged. |

### Configuration

**Via Web UI:**
Settings page → MQTT section → "Change Only" toggle and "Heartbeat" slider.

**Via MQTT:**
```bash
# Enable change-only with 120s heartbeat
mosquitto_pub -t "SmartEVSE/<serial>/Set/MQTTChangeOnly" -m 1
mosquitto_pub -t "SmartEVSE/<serial>/Set/MQTTHeartbeat" -m 120
```

**Via REST API:**
```bash
curl -X POST http://smartevse-xxxx.local/settings \
  -d "mqtt_change_only=1&mqtt_heartbeat=120"
```

### Recommendations

| Use case | MQTTChangeOnly | MQTTHeartbeat |
|----------|---------------|---------------|
| Standard Home Assistant | 1 (on) | 60s |
| Constrained WiFi / MQTT broker | 1 (on) | 300s |
| Critical solar automations | 1 (on) | 30s |
| Legacy setup (needs all values) | 0 (off) | — |

## Full MQTT topic reference

### Published state topics

All topics use prefix `SmartEVSE/<serial>/`.

| Topic | Type | Unit | state_class | Description |
|-------|------|------|-------------|-------------|
| `/connected` | string | — | — | `online` / `offline` (LWT) |
| `/ChargeCurrent` | int | A (dA) | measurement | Current charge current |
| `/MaxCurrent` | int | A (dA) | measurement | Maximum current setting |
| `/MinCurrent` | int | A (dA) | measurement | Minimum current setting |
| `/MainsCurrentL1` | int | A (dA) | measurement | Mains current phase L1 |
| `/MainsCurrentL2` | int | A (dA) | measurement | Mains current phase L2 |
| `/MainsCurrentL3` | int | A (dA) | measurement | Mains current phase L3 |
| `/EVCurrentL1` | int | A (dA) | measurement | EV current phase L1 |
| `/EVCurrentL2` | int | A (dA) | measurement | EV current phase L2 |
| `/EVCurrentL3` | int | A (dA) | measurement | EV current phase L3 |
| `/MainsImportActiveEnergy` | float | kWh | total_increasing | Grid import energy (zero-guarded) |
| `/MainsExportActiveEnergy` | float | kWh | total_increasing | Grid export energy (zero-guarded) |
| `/EVChargedEnergy` | float | kWh | total | Session charged energy (resets per session) |
| `/EVTotalChargedEnergy` | float | kWh | total_increasing | Total charged energy (zero-guarded) |
| `/SolarStopTimer` | int | s | measurement | Solar stop countdown timer |
| `/CurrentMaxSumMains` | int | A | — | Max sum mains current (number entity) |
| `/FreeHeap` | int | B | measurement | ESP32 free heap memory |
| `/MQTTMsgCount` | int | — | total_increasing | Total MQTT messages published |
| `/MQTTHeartbeat` | int | s | — | Current heartbeat setting |

### Command topics (Set)

| Topic | Type | Range | Description |
|-------|------|-------|-------------|
| `/Set/CurrentOverride` | int | 0-80 | Override charge current (A) |
| `/Set/MainsMeter` | int | 0-x | Set mains meter type |
| `/Set/MaxCurrent` | int | 6-80 | Maximum charge current |
| `/Set/Mode` | int | 0-2 | 0=Normal, 1=Smart, 2=Solar |
| `/Set/MQTTChangeOnly` | int | 0-1 | Enable/disable change-only publishing |
| `/Set/MQTTHeartbeat` | int | 10-300 | Heartbeat interval (seconds) |

## Troubleshooting

| Problem | Solution |
|---------|----------|
| HA energy dashboard shows wrong values | Delete the entity, restart HA, let MQTT re-discover it with corrected `state_class` |
| Entity IDs changed after update | Update automations to use new `snake_case` names |
| Too many MQTT messages | Enable `MQTTChangeOnly=1` and set `MQTTHeartbeat=120` |
| Values not updating in HA | Check `MQTTMsgCount` is incrementing; verify MQTT broker is connected |
| `FreeHeap` / `MQTTMsgCount` not showing | Enable diagnostic entities in HA entity settings (disabled by default) |
