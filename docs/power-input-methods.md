# Power Input Methods

SmartEVSE supports five methods for obtaining mains current data — the critical
input that drives load balancing, smart mode, and solar mode. This guide helps
you choose the right method and configure it correctly.

**Why this matters:** When any metering method fails, SmartEVSE stops charging
as a safety precaution — it cannot regulate current without knowing mains load.
The reliability of your metering method directly determines how robustly your
car charges.

---

## 1. Reliability Ranking

Methods are listed from most reliable to most error-prone. Choose the highest-ranked
method your installation supports.

| Rank | Method | Failure dependencies | Failure modes | Recovery | Timeout |
|------|--------|---------------------|---------------|----------|---------|
| 1 (most reliable) | **Modbus RTU over RS485** | Wired bus only | CRC error, cable fault, address conflict | Auto-retry every ~2s, `CT_NOCOMM` error flag after 11s | 11s |
| 2 | **Sensorbox v1/v2** | Wired bus only | Same as Modbus + CT saturation (v2) | Same as Modbus, fixed address 0x0A | 11s |
| 3 | **HomeWizard P1 (WiFi)** | WiFi, local HTTP, mDNS | WiFi dropout, mDNS failure, HTTP timeout, meter reboot | 30s mDNS retry, 1.95s HTTP poll, 11s meter timeout | 11s |
| 4 | **Battery Current (MQTT)** | WiFi, MQTT broker, external publisher | Broker down, publisher crash, WiFi dropout | 60s timeout → value zeroed | 60s |
| 5 (most error-prone) | **API/MQTT external feed** | WiFi, MQTT broker, external automation, external meter | Broker down, automation error, meter offline, WiFi dropout | 11s generic meter timeout, no dedicated staleness detection | 11s |

**Wired methods (rank 1–2)** only stop charging on physical cable faults. WiFi
and software are not in the data path.

**Local WiFi methods (rank 3)** add WiFi, HTTP, and mDNS as dependencies. WiFi
drops are the #1 cause of metering gaps in community reports.

**MQTT-dependent methods (rank 4–5)** add the MQTT broker plus an external
publisher (e.g., Home Assistant automation) as dependencies. Any failure in the
WiFi → broker → publisher chain stops charging.

---

## 2. Decision Tree

```
Do you have a dedicated energy meter on the mains connection?
├── Yes, wired via RS485
│   ├── Is it a supported meter type? (see Section 3.1)
│   │   ├── Yes → Use Modbus RTU (Method 1) ★ most reliable
│   │   └── No → Use Custom meter (type 19) with manual register config
│   └── Is it a Sensorbox with CT clamps?
│       └── Yes → Use Sensorbox (Method 2) ★ very reliable
│
├── Yes, it's a HomeWizard P1 meter (WiFi)
│   └── Use HomeWizard P1 (Method 3) — reliable on stable WiFi
│
├── No dedicated meter, but I have Home Assistant / external system
│   ├── Is charging robustness critical? (e.g., overnight unattended)
│   │   ├── Yes → Consider adding a wired meter (Method 1 or 2)
│   │   └── No → Use API/MQTT feed (Method 5) — least robust
│   └── Do you have a home battery?
│       └── Yes → Also configure Battery Current (Method 4) for solar mode
│
└── No meter at all
    └── Use "Disabled" — SmartEVSE charges at configured MaxCurrent
        (no load balancing, no solar/smart mode)
```

---

## 3. Per-Method Setup Guides

### 3.1 Modbus RTU over RS485 (Rank 1 — Most Reliable)

**What it is:** A wired RS485 connection from SmartEVSE to a DIN-rail energy meter
in your electrical panel. The meter measures mains current on all three phases.
SmartEVSE polls it every ~2 seconds via the Modbus RTU protocol.

**Hardware requirements:**
- A supported energy meter (see table below) installed on the mains feed
- RS485 cable (twisted pair, typically Cat5/Cat6 or dedicated 2-wire) from
  SmartEVSE to the meter
- Correct wiring: A+ to A+, B- to B-, with 120Ω termination resistors at
  both ends for cable runs over 10m

**Supported meter types:**

| ID | Meter | Phases | Notes |
|----|-------|--------|-------|
| 2 | Phoenix Contact EEM-350-D-MCB | 3P | |
| 3 | Finder 7E.78 | 3P | Max 127 registers per read |
| 4 | Eastron SDM630 | 3P | Most commonly used |
| 5 | Eastron SDM630 (inverted) | 3P | For reversed CT/wiring polarity |
| 6 | ABB B23 212-100 | 3P | Note: reversed RS485 A/B wiring |
| 7 | SolarEdge SunSpec | 3P | Via SolarEdge inverter meter |
| 8 | WAGO 879-30x0 | 3P | |
| 10 | Eastron SDM120/SDM630 | 1P | Single-phase variant |
| 11 | Finder 7M.38 | 3P | Max 20 registers per read |
| 12 | Sinotimer DTS6619 | 3P | |
| 14 | Schneider iEM3x5x | 3P | |
| 15 | Chint DTSU666 | 3P | |
| 16 | Carlo Gavazzi EM340 | 3P | |
| 17 | Orno OR-WE-517 | 3P | Bidirectional |
| 18 | Orno OR-WE-516 | 1P | Bidirectional |
| 19 | Custom | Any | User-defined register map |

**Configuration:**
1. Wire the RS485 connection (A+, B-, GND)
2. Set the meter's Modbus address (default varies by manufacturer)
3. In SmartEVSE: set `MainsMeter` to the meter type (LCD menu or web UI)
4. Set `MainsMeterAddress` to the meter's Modbus address
5. For ABB B23: note that RS485 A/B wiring is reversed compared to other meters

**Data provided:** Per-phase current (Irms), power, import energy, export energy
(varies by meter — most provide all fields).

**Failure modes and recovery:**
- CRC error on received data → silently discarded, re-polled next cycle (~2s)
- No response for 11 seconds (`COMM_TIMEOUT`) → `CT_NOCOMM` error flag set,
  charging stops
- Address conflict (two devices on same address) → corrupted responses, CRC errors

**Verification:**
- Web UI dashboard shows per-phase mains current updating every ~2 seconds
- MQTT topics `SmartEVSE/<serial>/MainsMeterIrms` show non-zero values
- No `CT_NOCOMM` error on the display

**Troubleshooting:**
| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| `CT_NOCOMM` error | Wiring fault, wrong address, wrong meter type | Check A+/B- wiring, verify Modbus address, try swapping A/B |
| Readings are zero | Meter not powered, wrong register set | Verify meter has power, check meter type selection |
| Readings are negative | CT direction reversed or use type 5 (inverted) | Swap CT orientation or select inverted meter type |
| Intermittent errors | Long cable without termination | Add 120Ω termination resistors at both ends |
| ABB B23 not responding | Reversed A/B convention | Swap A+ and B- connections (ABB uses opposite convention) |

---

### 3.2 Sensorbox v1/v2 (Rank 2 — Very Reliable)

**What it is:** A companion device (also from the SmartEVSE project) that connects
via RS485 at fixed Modbus address 0x0A. The Sensorbox measures mains current using
either CT (current transformer) clamps (v2) or a P1 smart meter connection.

**Hardware requirements:**
- Sensorbox v1 or v2 module
- RS485 cable from SmartEVSE to Sensorbox
- CT clamps (v2) installed on mains supply wires, or P1 cable to Dutch smart meter

**v1 vs v2 differences:**

| Feature | Sensorbox v1 | Sensorbox v2+ |
|---------|-------------|--------------|
| Input | P1 smart meter only | CT clamps or P1 |
| WiFi status | Not available | WiFi mode, IP, portal control |
| Registers | 20 registers | 32 registers |
| Grid config | Not available | 4-wire / 3-wire selectable |

**Configuration:**
1. Wire RS485 from SmartEVSE to Sensorbox
2. Install CT clamps on the three mains phase wires (v2), or connect P1 cable
3. Set `MainsMeter` to type 1 (Sensorbox) — address 0x0A is automatic
4. For 3-wire installations (no neutral): configure grid type via register 0x0202

**Data provided:** Per-phase current only. No energy or power data.

**CT scaling note:** The Sensorbox v2 has hardcoded CT calibration values.
If `MaxMains` is set above 100A, SmartEVSE assumes 200A:50mA CTs and doubles
the measured current. Users with non-standard CTs cannot adjust calibration
without firmware changes.

**Failure modes and recovery:**
- Same as Modbus RTU (wired RS485, same timeout behavior)
- CT saturation at very high currents → readings may clip
- Small currents between -0.1A and +0.1A are zeroed out (noise suppression)

**Verification:**
- Same as Modbus RTU: check web UI dashboard and MQTT for non-zero current readings
- Sensorbox v2: WiFi status visible in SmartEVSE web UI

**Troubleshooting:**
| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| `CT_NOCOMM` error | RS485 wiring issue | Check wiring, Sensorbox is always address 0x0A |
| All phases read zero | CT clamps not installed or not closed | Ensure CT clamps are fully closed around each phase wire |
| Current reads half expected | Wrong CT ratio assumption | Check MaxMains setting (>100A triggers 200A CT mode) |
| Current direction wrong | CT clamps installed backwards | Reverse CT clamp orientation on the wire |

---

### 3.3 HomeWizard P1 via WiFi (Rank 3 — Reliable on Stable WiFi)

**What it is:** SmartEVSE reads per-phase current from a HomeWizard P1 meter on
your local WiFi network. The P1 meter connects to a Dutch smart meter's P1 port
and exposes data via a local HTTP REST API.

**Hardware requirements:**
- HomeWizard P1 meter (Wi-Fi Energy Socket does NOT work — must be the P1 meter)
- Both SmartEVSE and HomeWizard P1 on the same WiFi network / VLAN
- mDNS must work on your network (most home routers support this)

**Configuration:**
1. Install and set up the HomeWizard P1 meter via the HomeWizard app
2. Enable the "Local API" in the HomeWizard app settings
3. Set SmartEVSE `MainsMeter` to type 13 (HomeWizard P1)
4. SmartEVSE will automatically discover the P1 meter via mDNS

**How discovery works:**
- SmartEVSE searches for `_hwenergy._tcp` mDNS services on the local network
- Filters results to devices with hostname starting with `p1meter-`
- Caches the discovered hostname/IP
- If discovery fails, retries every 30 seconds
- On HTTP connection failure, clears the cache to trigger immediate rediscovery

**Data flow:**
- Polls `http://<p1meter>/api/v1/data` every 1.95 seconds
- Reads: `active_current_l1_a`, `active_current_l2_a`, `active_current_l3_a`
- Uses `active_power_l1_w` etc. to determine current direction (import vs export)
- Does NOT read energy data (`total_power_import_kwh`, `total_power_export_kwh`)
  — this is a known limitation

**Data provided:** Per-phase current with direction correction. No energy data.

**Failure modes and recovery:**
- WiFi dropout → HTTP requests fail → meter times out after 11 seconds → charging stops
- mDNS failure (router doesn't forward multicast) → cannot discover meter → no data
- P1 meter reboot → temporary data gap until next successful HTTP poll
- HTTP timeout (1.5s per request, 5 attempts per 11s timeout window)

**Verification:**
- Web UI shows mains current values updating
- MQTT topic `SmartEVSE/<serial>/MainsMeterIrms` shows non-zero values
- Check mDNS: on your network, `p1meter-*.local` should be resolvable

**Troubleshooting:**
| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| No readings, no error | mDNS not working | Check that devices are on same VLAN, try `ping p1meter-<id>.local` from a computer on the same network |
| Intermittent dropouts | WiFi instability | Move P1 meter closer to WiFi AP, or use a WiFi repeater |
| All readings zero | P1 meter local API disabled | Enable "Local API" in HomeWizard app |
| `CT_NOCOMM` after 11s | HTTP connection failures | Check WiFi signal strength, network congestion |
| Discovery works but no data | P1 meter firmware outdated | Update P1 meter firmware via the HomeWizard app |

---

### 3.4 Battery Current Injection via MQTT (Rank 4 — Solar Mode Only)

**What it is:** An external system (typically Home Assistant) publishes your home
battery's charge/discharge current to SmartEVSE via MQTT. This allows solar mode
to account for battery storage when calculating available surplus current.

**This method is NOT a mains meter replacement.** It supplements one of the other
methods (1–3 or 5) by providing battery current data for solar calculations.

**Requirements:**
- An MQTT broker (e.g., Mosquitto) accessible to both SmartEVSE and the publisher
- SmartEVSE connected to the MQTT broker
- An automation that publishes battery current (e.g., Home Assistant, Node-RED)

**Configuration:**
1. Configure SmartEVSE MQTT connection (broker IP, port, credentials)
2. Create an automation that publishes to `SmartEVSE/<serial>/Set/HomeBatteryCurrent`
3. Payload: signed integer in deci-Amperes (0.1A units)
   - Positive = battery charging (consuming from grid)
   - Negative = battery discharging (feeding to grid/house)
   - Example: `50` = 5.0A charging, `-100` = 10.0A discharging

**Staleness detection:**
- 60-second hard timeout
- If no update received within 60 seconds, value is zeroed out
- Battery current is excluded from calculations until next valid update

**Failure modes:**
- MQTT broker down → no updates → value zeroed after 60 seconds
- Publisher automation crashes → same as above
- Incorrect sign convention → solar calculations will be wrong

**Verification:**
- MQTT topic `SmartEVSE/<serial>/HomeBatteryCurrent` shows the last received value
- Solar mode calculations adjust when battery is charging/discharging

**Troubleshooting:**
| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| Battery current always 0 | Publisher not running, wrong topic | Check MQTT topic matches `SmartEVSE/<serial>/Set/HomeBatteryCurrent` |
| Solar mode ignores battery | Value zeroed (stale) | Ensure publisher sends updates more often than every 60 seconds |
| Solar charges when battery is full | Wrong sign convention | Verify: positive = charging, negative = discharging |

---

### 3.5 API/MQTT External Feed (Rank 5 — Least Robust)

**What it is:** An external system publishes per-phase mains current to SmartEVSE
via MQTT or HTTP. This is used when you have an energy meter that SmartEVSE cannot
read directly (e.g., it's only accessible via Home Assistant, a cloud API, or a
non-Modbus protocol).

**Requirements:**
- An MQTT broker or HTTP access to SmartEVSE
- An automation that reads your energy meter and publishes current values
- SmartEVSE `MainsMeter` set to type 9 (API)

**Configuration:**

*Via MQTT:*
1. Set `MainsMeter` to type 9 (API)
2. Publish to `SmartEVSE/<serial>/Set/MainsMeter` with payload `L1:L2:L3`
3. Values are in deci-Amperes (0.1A units), range -2000 to +2000 (-200A to +200A)
4. All three phases must be provided; partial updates are rejected
5. Example: `100:50:-30` = L1: 10.0A import, L2: 5.0A import, L3: 3.0A export

*Via HTTP REST API:*
1. Set `MainsMeter` to type 9 (API)
2. POST to `http://<smartevse-ip>/currents?L1=<val>&L2=<val>&L3=<val>`
3. Values in deci-Amperes, same range as MQTT
4. Example: `curl -X POST "http://192.168.1.100/currents?L1=100&L2=50&L3=30" -d ''`

**Important considerations:**
- **Staleness detection** (enabled by default): If no update arrives within the
  configured timeout (default 120 seconds), SmartEVSE falls back to MaxMains on
  all phases as a safe default. Charging continues at the safe limit rather than
  stopping entirely. Configure via MQTT: publish to
  `SmartEVSE/<serial>/Set/MainsMeterTimeout` with a value in seconds (0 = disabled,
  10–3600). When staleness detection is disabled (timeout = 0), the generic 11-second
  meter timeout applies and charging stops on timeout.
- Publish updates at regular intervals (recommended: every 5–10 seconds).
- Only works when `MainsMeter` is set to API (type 9).
- Does not work in multi-node slave mode (`LoadBl >= 2`).

**Data provided:** Per-phase current only. No energy, no power data.

**Failure modes:**
- MQTT broker restart → no updates → meter timeout after 11s → charging stops
- Home Assistant restart → automation stops → same as above
- WiFi dropout → MQTT disconnects → same as above
- External meter goes offline → automation publishes stale/zero values → incorrect
  current limiting (this is the key risk — SmartEVSE cannot distinguish stale
  from fresh data)

**Why this is ranked lowest:** This method has the longest dependency chain
(WiFi → MQTT broker → external automation → external meter). Each link is a
potential failure point that SmartEVSE has zero visibility into. While the 11-second
generic timeout will eventually catch a complete data loss, it cannot detect stale
or incorrect values from a malfunctioning automation.

**Verification:**
- Web UI shows mains current updating
- MQTT topic `SmartEVSE/<serial>/MainsMeterIrms` shows values matching your meter
- No `CT_NOCOMM` error on the display

**Troubleshooting:**
| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| `CT_NOCOMM` error | No MQTT/HTTP updates for >11s | Check automation is running, MQTT broker is up |
| Values don't update | MainsMeter not set to type 9 (API) | Set MainsMeter to API in web UI |
| Only L1 updates | Payload format wrong | Must send all 3 phases: `L1:L2:L3` |
| Charging limited unexpectedly | Stale high current values | Check your automation is reading the external meter correctly |
| Values rejected | Out of range | Each phase must be between -2000 and +2000 (deci-Amperes) |

---

## 4. Comparison Table

| Feature | Modbus RTU | Sensorbox | HomeWizard P1 | Battery (MQTT) | API/MQTT Feed |
|---------|-----------|-----------|---------------|----------------|---------------|
| **Reliability tier** | Tier 1 (wired) | Tier 1 (wired) | Tier 2 (WiFi) | Tier 3 (MQTT) | Tier 3 (MQTT) |
| **Connection** | RS485 wired | RS485 wired | WiFi HTTP | WiFi MQTT | WiFi MQTT/HTTP |
| **Polling** | ~2s (SmartEVSE polls) | ~2s (SmartEVSE polls) | 1.95s (SmartEVSE polls) | External push | External push |
| **Timeout** | 11s | 11s | 11s | 60s | 11s |
| **Per-phase current** | Yes | Yes | Yes | N/A (single value) | Yes |
| **Energy data** | Yes (most meters) | No | No | No | No |
| **Power data** | Yes (most meters) | No | Direction only | No | No |
| **Auto-discovery** | No (manual address) | Yes (fixed 0x0A) | Yes (mDNS) | No | No |
| **Multi-node compatible** | Yes | Yes | Yes (master only) | Master only | Master only |
| **Error reporting** | `CT_NOCOMM` flag | `CT_NOCOMM` flag | `CT_NOCOMM` flag | Silent zero-out | `CT_NOCOMM` flag |
| **Replaces mains meter** | Yes | Yes | Yes | No (supplement) | Yes |
| **Setup complexity** | Medium (wiring) | Medium (wiring + CTs) | Low (WiFi only) | Medium (automation) | Medium (automation) |

---

## 5. Migration Guide

### Switching from one method to another

1. **Stop charging** — ensure no vehicle is actively charging before changing
   metering configuration
2. **Change `MainsMeter` type** — via LCD menu, web UI, or MQTT
   (`SmartEVSE/<serial>/Set/MainsMeter` with numeric type value)
3. **Set meter address** (Modbus only) — configure `MainsMeterAddress`
4. **Verify data flow** — check web UI dashboard shows current values updating
5. **Test under load** — start a charge session and verify current readings are
   reasonable

### Common migration paths

**HomeWizard P1 → Modbus RTU** (upgrading reliability):
- Install a DIN-rail meter on the mains feed
- Wire RS485 from SmartEVSE to the meter
- Change `MainsMeter` from type 13 to the appropriate meter type
- Set `MainsMeterAddress` to the meter's Modbus address
- Energy data becomes available (was not with HomeWizard P1)

**API/MQTT → HomeWizard P1** (reducing dependencies):
- Install a HomeWizard P1 meter on your Dutch smart meter's P1 port
- Enable the Local API in the HomeWizard app
- Change `MainsMeter` from type 9 to type 13
- Remove or disable the MQTT/HTTP automation (no longer needed)

**No meter → Sensorbox** (adding solar/smart mode):
- Install Sensorbox with CT clamps on mains wires
- Wire RS485 from SmartEVSE to Sensorbox
- Change `MainsMeter` to type 1
- Solar mode and smart mode become available

---

## 6. General Troubleshooting

### Common issues across all methods

| Symptom | Possible causes | Diagnostic steps |
|---------|----------------|-----------------|
| `CT_NOCOMM` error | Meter not responding within 11 seconds | Check wiring (Modbus/Sensorbox), WiFi (HomeWizard), automation (API/MQTT) |
| Current reads as 0 on all phases | Meter not measuring, CT clamps open, automation not publishing | Verify meter independently, check CT installation, check MQTT messages |
| Current values seem wrong | Wrong meter type selected, inverted CTs, wrong Modbus address | Compare readings with meter's own display, check type selection |
| Charging stops unexpectedly | Intermittent meter communication | Check for `CT_NOCOMM` in error log, monitor MQTT for gaps |
| Load balancing not working | Meter on wrong location (must be on mains, not sub-circuit) | Verify meter measures total mains consumption including EVSE |

### Understanding current values

- All current values in SmartEVSE are in **deci-Amperes** (0.1A units)
- Positive values = importing from grid (consumption)
- Negative values = exporting to grid (solar feed-in)
- The sum of all three phases (`Isum`) drives load balancing and solar mode
