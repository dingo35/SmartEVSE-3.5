# Manual Test Plan — SmartEVSE Firmware

**Scope:** All features from Plan 01 through Plan 15 plus cross-plan hardening (PR #91).
Only tests that require real hardware, real MQTT broker, real vehicle, or real browser.
Automated native tests (50 suites, 1,096 scenarios) are excluded.

**Version:** Generated 2026-03-20

---

## Prerequisites

| Item | Required for |
|------|-------------|
| SmartEVSE v3 or v4 with latest firmware flashed | All tests |
| EV or EVSE simulator (CP pilot load) | Scenarios 3-5 |
| MQTT broker (Mosquitto) | Scenarios 1-3, 5, 7 |
| Home Assistant instance | Scenario 2 (HA tests) |
| Web browser (desktop + mobile) | Scenario 2 (UI tests) |
| MID-certified EV meter (Eastron SDM630 or similar) | Session logging / ERE tests |
| Second SmartEVSE node | Scenario 6 (load balancing) |
| OCPP backend (SteVe or similar) | OCPP tests |
| EVCC instance | EVCC tests |
| HomeWizard P1 meter | P1/manual IP tests |
| `curl`, `mosquitto_sub`, `mosquitto_pub` | API/MQTT tests |

## Notation

- **Priority:** P1 = must test before release, P2 = should test, P3 = nice to have
- **HW req:** additional hardware beyond basic single-EVSE + EV setup
- **API-only:** can be tested without a physical vehicle connected

---

## Scenario 1: Cold Start / Boot

Power-cycle the SmartEVSE, then verify all boot-time behavior in sequence.

| ID | Description | Steps | Expected Result | Ref | Pri |
|----|-------------|-------|-----------------|-----|-----|
| MT-001 | NTP time sync on boot | 1. Power-cycle SmartEVSE. 2. Wait 30s for WiFi. 3. `curl http://<ip>/settings \| jq .` | Timestamps in responses are current UTC (not epoch 0). Clock within 2s of actual time. | Plan 10/PR #89 | P1 |
| MT-002 | MQTT broker connection on boot | 1. Start `mosquitto_sub -t "SmartEVSE-<serial>/#" -v`. 2. Power-cycle SmartEVSE. 3. Wait 60s. | MQTT messages appear. HA discovery payloads published on `homeassistant/` topics. | Plan 08/PR #64 | P1 |
| MT-003 | Session ID counter resets on boot | 1. Power-cycle SmartEVSE. 2. `curl http://<ip>/session/last` | Returns `204 No Content` (no session since reboot). | Plan 10/PR #89 | P2 |
| MT-004 | Web UI loads without internet | 1. Disconnect SmartEVSE from internet (keep local WiFi). 2. Open `http://<ip>/` in browser. | Full UI renders with no CDN errors. All CSS/JS inline or packed. | Plan 07/PR #85 | P1 |
| MT-005 | Diagnostic system ready on boot | 1. Power-cycle. 2. Wait 30s. 3. `curl http://<ip>/diag/status` | Returns JSON with `"capturing": false`, ring buffer size > 0. | Plan 06/PR #84 | P2 |
| MT-006 | FreeHeap entity published on boot | 1. Subscribe: `mosquitto_sub -t "SmartEVSE-<serial>/FreeHeap" -v`. 2. Power-cycle. | FreeHeap value > 100000 (reasonable ESP32 heap). | PR #68 | P3 |

---

## Scenario 2: Idle State (No Vehicle)

SmartEVSE is powered, connected to WiFi/MQTT, no vehicle plugged in.
All tests in this scenario are **API-only** (no vehicle needed).

### 2A — REST API Verification

| ID | Description | Steps | Expected Result | Ref | Pri |
|----|-------------|-------|-----------------|-----|-----|
| MT-010 | GET /settings returns full JSON | 1. `curl -s http://<ip>/settings \| jq .` | Valid JSON with all documented fields: `version`, `mode`, `evse`, `settings`, `mqtt`, `ev_meter`, `mains_meter`, `phase_currents`. | PR #10 | P1 |
| MT-011 | IEC 61851 state field present | 1. `curl -s http://<ip>/settings \| jq .evse.iec61851_state` | Returns `"A"` (no vehicle connected). | Plan 04/PR #70 | P1 |
| MT-012 | Charging enabled field present | 1. `curl -s http://<ip>/settings \| jq .evse.charging_enabled` | Returns boolean (`true` or `false`). | Plan 04/PR #70 | P1 |
| MT-013 | POST mode change via API | 1. `curl -X POST "http://<ip>/settings?mode=2" -d ''` 2. `curl -s http://<ip>/settings \| jq .mode_id` | Mode changes to 2 (Solar). Second call confirms `"mode_id": 2`. | REST API | P1 |
| MT-014 | POST invalid mode rejected | 1. `curl -X POST "http://<ip>/settings?mode=99" -d ''` | Returns error response (not 200 OK with mode=99). | PR #91 | P2 |
| MT-015 | POST override_current | 1. Set mode=1 (Normal). 2. `curl -X POST "http://<ip>/settings?override_current=100" -d ''` 3. GET /settings | `override_current` shows 100 (10.0A). | REST API | P2 |
| MT-016 | POST phase switch request (no vehicle) | 1. Ensure enable_C2 != 0. 2. `curl -X POST "http://<ip>/settings?phases=1" -d ''` | Returns JSON with `"phases": 1`. No crash. | Plan 04/PR #70 | P2 |
| MT-017 | POST priority strategy | 1. `curl -X POST "http://<ip>/settings?prio_strategy=1" -d ''` 2. GET /settings | Priority strategy updated. | Plan 02/PR #11 | P2 |
| MT-018 | GET /session/last with no session | 1. (After fresh boot, no charge done) `curl -s -o /dev/null -w "%{http_code}" http://<ip>/session/last` | Returns HTTP 204. | Plan 10/PR #89 | P2 |
| MT-019 | POST /reboot | 1. `curl -X POST "http://<ip>/reboot" -d ''` 2. Wait 15s. 3. `curl http://<ip>/settings` | Device reboots and comes back online. Settings response returns. | REST API | P2 |
| MT-020 | POST /rfid simulation | 1. Enable RFID in settings. 2. `curl -X POST "http://<ip>/rfid?rfid=112233445566" -d ''` | Returns JSON with `rfid_status` field. | REST API | P3 |

### 2B — MQTT Verification

| ID | Description | Steps | Expected Result | Ref | Pri |
|----|-------------|-------|-----------------|-----|-----|
| MT-030 | Change-only publishing (idle) | 1. `mosquitto_sub -t "SmartEVSE-<serial>/#" -v > /tmp/mqtt.log`. 2. Wait 120s. 3. Count messages. | Far fewer than 360 msg/min. Expect ~1/min idle (heartbeat only). Values only re-published at heartbeat interval. | Plan 08/PR #64 | P1 |
| MT-031 | Heartbeat re-publish | 1. Set `MQTTHeartbeat` to 30 via MQTT: `mosquitto_pub -t "SmartEVSE-<serial>/Set/MQTTHeartbeat" -m "30"`. 2. Monitor all topics for 60s. | All topics re-published at least once within 30s even though values unchanged. | Plan 08/PR #64 | P2 |
| MT-032 | MQTT Set commands work | 1. `mosquitto_pub -t "SmartEVSE-<serial>/Set/Mode" -m "2"`. 2. Check mode via REST. | Mode changed to Solar (2). | PR #10 | P1 |
| MT-033 | Solar debug MQTT topic | 1. `mosquitto_pub -t "SmartEVSE-<serial>/Set/SolarDebug" -m "1"`. 2. `mosquitto_sub -t "SmartEVSE-<serial>/Debug/Solar" -C 1` | JSON payload with solar debug fields (ImportCurrent, SunCurrent, etc.). | Plan 01/PR #83 | P2 |
| MT-034 | HomeWizardIP via MQTT | 1. `mosquitto_pub -t "SmartEVSE-<serial>/Set/HomeWizardIP" -m "192.168.1.50"`. 2. Verify via REST or observe behavior. | IP accepted. (Clear with empty payload afterwards.) | Plan 09/PR #86 | P3 |
| MT-035 | MainsMeterTimeout via MQTT | 1. `mosquitto_pub -t "SmartEVSE-<serial>/Set/MainsMeterTimeout" -m "120"`. | Accepted (no error). Value below 10 or above 3600 should be rejected. | Plan 09/PR #86 | P3 |
| MT-036 | Per-phase energy MQTT topics | 1. Subscribe: `mosquitto_sub -t "SmartEVSE-<serial>/ev_meter/+energy+" -v`. 2. Wait for heartbeat cycle. | Per-phase energy topics published (L1/L2/L3 import/export energy). | Plan 08/PR #82 | P2 |
| MT-037 | MQTTMsgCount increments | 1. Subscribe to `SmartEVSE-<serial>/MQTTMsgCount`. 2. Observe over 60s. | Counter increments with each publish cycle. | PR #68 | P3 |

### 2C — Home Assistant Discovery

**HW req:** Home Assistant with MQTT integration configured.

| ID | Description | Steps | Expected Result | Ref | Pri |
|----|-------------|-------|-----------------|-----|-----|
| MT-040 | HA auto-discovers SmartEVSE | 1. Restart MQTT integration in HA. 2. Check Devices. | SmartEVSE device appears with all entities. Entity IDs use `snake_case` (e.g., `sensor.smartevse_charge_current`). | Plan 08/PR #64 | P1 |
| MT-041 | Energy entities on HA energy dashboard | 1. Add `EV Import Active Energy` to HA energy dashboard. | Entity has `state_class: total_increasing`, value > 0 (not zero on startup). | Plan 08/PR #64 | P1 |
| MT-042 | MaxSumMains number entity bidirectional | 1. In HA, find `MaxSumMains` number entity. 2. Change value via HA UI slider. 3. Verify via REST API. | Value propagates to SmartEVSE. REST shows updated `current_max_sum_mains`. | Plan 08/PR #64 | P2 |
| MT-043 | Diagnostic entities disabled by default | 1. Check HA for `FreeHeap` and `MQTTMsgCount` entities. | Entities exist but are disabled by default (HA shows "disabled entity" in device page). | PR #68 | P3 |
| MT-044 | Solar Stop Timer has measurement state class | 1. Check HA entity details for Solar Stop Timer. | `state_class: measurement`, enables HA graph. | Plan 08/PR #64 | P3 |

### 2D — Web UI

| ID | Description | Steps | Expected Result | Ref | Pri |
|----|-------------|-------|-----------------|-----|-----|
| MT-050 | Dashboard loads and shows status | 1. Open `http://<ip>/` in desktop browser. | Dashboard displays: mode, state, temperature, currents, meter readings. | Plan 07/PR #85 | P1 |
| MT-051 | WebSocket real-time updates | 1. Open dashboard. 2. Look for green connection indicator. 3. Change mode via MQTT. | Green dot visible. Mode change reflected in UI within 2s without page reload. | Plan 07/PR #85 | P1 |
| MT-052 | WebSocket fallback to polling | 1. Open dashboard. 2. Block WebSocket port (or use browser that doesn't support WS). | Red dot shown. Data still updates via polling (slower, ~5s intervals). | Plan 07/PR #85 | P2 |
| MT-053 | Dark mode toggle | 1. Open dashboard. 2. Click dark mode toggle (sun/moon icon in header). | UI switches to dark theme. Preference persists across page reload (localStorage). | Plan 07/PR #85 | P2 |
| MT-054 | Dark mode system preference | 1. Set OS to dark mode. 2. Open dashboard (fresh, no localStorage pref). | UI auto-detects and uses dark theme. | Plan 07/PR #85 | P3 |
| MT-055 | Mobile layout | 1. Open dashboard on mobile device (or browser at 375px width). | Single-column layout. Bottom navigation bar. All controls accessible. No horizontal scroll. | Plan 07/PR #85 | P1 |
| MT-056 | Mode buttons work | 1. Open UI. 2. Click each mode button (OFF, Normal, Solar, Smart). 3. Verify via REST. | Mode changes confirmed via API for each click. | Plan 07/PR #85 | P1 |
| MT-057 | Settings page — solar parameters | 1. Navigate to settings page. 2. Change SolarStartCurrent. 3. Save. 4. Reload page. | Value persists after reload. Matches REST API value. | Plan 07/PR #85 | P2 |
| MT-058 | Settings page — MQTT configuration | 1. Navigate to MQTT settings. 2. Verify host, port, prefix fields visible. | All MQTT settings editable. Password field shows "set" indicator (not plaintext). | Plan 07/PR #85 | P2 |
| MT-059 | LCD remote widget | 1. Open UI. 2. Find LCD widget. 3. Press arrow keys or click buttons. | LCD widget responds. Keyboard shortcuts (arrows, enter) work. Widget scales responsively. | Plan 07/PR #85 | P3 |
| MT-060 | Reboot button in UI | 1. Open UI. 2. Click reboot. 3. Confirm dialog. | Device reboots. UI reconnects after ~15s. | Plan 07/PR #85 | P2 |

### 2E — Diagnostic Telemetry

| ID | Description | Steps | Expected Result | Ref | Pri |
|----|-------------|-------|-----------------|-----|-----|
| MT-070 | Start diagnostic capture via REST | 1. `curl -X POST "http://<ip>/diag/start?profile=general"`. 2. `curl http://<ip>/diag/status` | Status shows `"capturing": true`, profile = "general". | Plan 06/PR #84 | P2 |
| MT-071 | Start diagnostic capture via MQTT | 1. `mosquitto_pub -t "SmartEVSE-<serial>/Set/DiagProfile" -m "general"`. 2. Check status via REST. | Capture started. | Plan 06/PR #84 | P2 |
| MT-072 | Download diagnostic dump | 1. Start capture. 2. Wait 10s. 3. `curl http://<ip>/diag/download -o dump.diag`. | File downloads. Non-empty binary content. | Plan 06/PR #84 | P2 |
| MT-073 | Decode diagnostic dump with Python | 1. Download a .diag file. 2. `python3 diag_decode.py dump.diag` | Decoded output shows timestamped telemetry entries. | Plan 06/PR #84 | P3 |
| MT-074 | Diagnostic viewer in UI | 1. Open UI. 2. Navigate to diagnostics page. 3. Click "Start Capture". | Live stream of telemetry entries in browser. Start/stop buttons work. | Plan 07/PR #85 | P2 |
| MT-075 | Diagnostic viewer download | 1. With capture running in UI, click "Download". | .diag file downloads from browser. | Plan 07/PR #85 | P3 |
| MT-076 | List diagnostic files | 1. (After at least one auto-dump or manual capture.) `curl http://<ip>/diag/files` | JSON array of file entries with names and sizes. | Plan 06/PR #84 | P3 |

---

## Scenario 3: Vehicle Connect + Charge (Normal Mode)

Set SmartEVSE to Normal mode (mode=1), then connect a vehicle (or simulator).
Verify multiple features during a single charge session.

**Before starting:** Start MQTT subscriber: `mosquitto_sub -t "SmartEVSE-<serial>/#" -v > /tmp/charge-mqtt.log`

| ID | Description | Steps | Expected Result | Ref | Pri |
|----|-------------|-------|-----------------|-----|-----|
| MT-100 | IEC 61851 state B on connect | 1. Connect vehicle (don't start charging yet). 2. `curl -s http://<ip>/settings \| jq .evse.iec61851_state` | Returns `"B"` (connected, not charging). | Plan 04/PR #70 | P1 |
| MT-101 | MQTT publishes state change on connect | 1. (While connecting vehicle) observe MQTT log. | State topic changes from idle to connected. Change-only publishing triggers immediately on state change (not waiting for heartbeat). | Plan 08 | P1 |
| MT-102 | IEC 61851 state C when charging | 1. Vehicle starts drawing current. 2. `curl -s http://<ip>/settings \| jq .evse.iec61851_state` | Returns `"C"` (charging). | Plan 04/PR #70 | P1 |
| MT-103 | Session start tracked | 1. Note the time when charging begins. 2. (Verify later in MT-200 after disconnect.) | Session start timestamp matches actual charge start time. | Plan 10/PR #89 | P1 |
| MT-104 | EV meter readings during charge | 1. While charging: `curl -s http://<ip>/settings \| jq .ev_meter` | Shows non-zero `import_active_power`, `currents` with actual values, `total_kwh` incrementing. | Plan 05/PR #76 | P1 |
| MT-105 | Per-phase current in MQTT | 1. Check MQTT log for phase current topics. | Per-phase currents published (L1, L2, L3) with non-zero values matching actual charge. | Plan 05/PR #76 | P2 |
| MT-106 | Power flow diagram in UI | 1. Open web UI while charging. | Power flow diagram animates showing energy flow. Current and power values displayed. | Plan 07/PR #85 | P2 |
| MT-107 | Charge current regulation | 1. `curl -X POST "http://<ip>/settings?override_current=80" -d ''` (8A). 2. Observe charge current via meter. | EV charge current adjusts to approximately 8A within 30s. | REST API | P1 |
| MT-108 | Diagnostic auto-dump on error | 1. (If possible) trigger a fault condition during charge. 2. Check `/diag/files`. | Auto-dump file created with timestamp of the error. | Plan 06/PR #84 | P3 |

### 3A — OCPP During Charge

**HW req:** OCPP backend (SteVe or similar) configured.

| ID | Description | Steps | Expected Result | Ref | Pri |
|----|-------------|-------|-----------------|-----|-----|
| MT-110 | OCPP StartTransaction on charge start | 1. Configure OCPP backend URL. 2. Connect vehicle, start charge. 3. Check OCPP backend logs. | StartTransaction message received by backend with correct meter values. | Plan 03/PR #77 | P1 |
| MT-111 | OCPP MeterValues during charge | 1. While charging, check OCPP backend. | Periodic MeterValues messages with energy readings. | Plan 03/PR #79 | P2 |
| MT-112 | OCPP and LB mutually exclusive | 1. Enable OCPP. 2. Try to enable Load Balancing (set LoadBl to Master). | Either: setting rejected, or clear warning that LB is disabled when OCPP active. | Plan 03/PR #79 | P2 |
| MT-113 | OCPP FreeVend mode guard | 1. Enable OCPP. 2. Check that FreeVend does not bypass OCPP authorization. | FreeVend is blocked when OCPP is active. Charge requires OCPP Authorize. | Plan 03/PR #79 | P2 |

### 3B — EVCC Integration During Charge

**HW req:** EVCC instance on network.

| ID | Description | Steps | Expected Result | Ref | Pri |
|----|-------------|-------|-----------------|-----|-----|
| MT-120 | EVCC status polling | 1. Configure EVCC with SmartEVSE template. 2. Connect vehicle. | EVCC shows vehicle as "connected" (state B) then "charging" (state C). | Plan 04/PR #70 | P1 |
| MT-121 | EVCC current control | 1. From EVCC, set max current to 10A. 2. Observe SmartEVSE. | `override_current` set to 100 (10A * 10). Charge current adjusts. | Plan 04/PR #70 | P2 |
| MT-122 | EVCC phase switching | 1. From EVCC, request 1-phase. 2. Observe SmartEVSE state machine. | SmartEVSE executes disconnect-switch-reconnect sequence. C2 contactor toggles. | Plan 04/PR #70 | P2 |
| MT-123 | EVCC enable/disable | 1. From EVCC, disable charging (mode=0). 2. Re-enable (mode=1). | Charging stops and resumes. No errors. | Plan 04/PR #70 | P2 |

---

## Scenario 4: Vehicle Disconnect

Disconnect the vehicle after charging (continuing from Scenario 3).

| ID | Description | Steps | Expected Result | Ref | Pri |
|----|-------------|-------|-----------------|-----|-----|
| MT-200 | Session complete MQTT message | 1. Disconnect vehicle. 2. Check MQTT log for `Session/Complete` topic. | Retained JSON message with: `session_id` = 1, valid ISO 8601 `start`/`end` timestamps, `kwh` > 0, `start_energy_wh` < `end_energy_wh`, `mode` = "normal", `phases` = 1 or 3. | Plan 10/PR #89 | P1 |
| MT-201 | GET /session/last returns session | 1. `curl -s http://<ip>/session/last \| jq .` | Returns 200 with JSON matching MQTT payload. Same session_id, timestamps, kwh. | Plan 10/PR #89 | P1 |
| MT-202 | Session kwh matches meter delta | 1. Compare `kwh` from session with `end_energy_wh - start_energy_wh`. | Values match (kwh = delta / 1000, within rounding tolerance). | Plan 10/PR #89 | P1 |
| MT-203 | IEC 61851 state returns to A | 1. `curl -s http://<ip>/settings \| jq .evse.iec61851_state` | Returns `"A"` (disconnected). | Plan 04/PR #70 | P1 |
| MT-204 | MQTT state topic updates on disconnect | 1. Check MQTT log. | State changes to disconnected/idle. Published immediately (change-only). | Plan 08 | P2 |
| MT-205 | Session MQTT message is retained | 1. Start new MQTT subscriber after disconnect: `mosquitto_sub -t "SmartEVSE-<serial>/Session/Complete" -C 1` | Immediately receives the last session (retained message). | Plan 10/PR #89 | P2 |
| MT-206 | Second charge increments session_id | 1. Connect vehicle again, charge briefly, disconnect. 2. Check `Session/Complete`. | `session_id` = 2. | Plan 10/PR #89 | P2 |
| MT-207 | OCPP StopTransaction on disconnect | 1. (With OCPP active) check backend logs. | StopTransaction message received with final meter value. `ocpp_tx_id` in session JSON is non-null. | Plan 03/PR #77 | P2 |

---

## Scenario 5: Solar Mode Charging

Set SmartEVSE to Solar mode. Requires actual solar production or a way to inject
mains meter readings via the API (`POST /currents` with L1/L2/L3) to simulate solar.

**Setup:** Set mode=2 (Solar). Configure `MainsMeter = API` to allow current injection.
Set `SolarStartCurrent`, `SolarMaxImport`, `SolarStopTime` to reasonable values.

| ID | Description | Steps | Expected Result | Ref | Pri |
|----|-------------|-------|-----------------|-----|-----|
| MT-300 | Solar start on surplus | 1. Connect vehicle. 2. Inject export currents: `curl -X POST "http://<ip>/currents?L1=-100&L2=-100&L3=-100" -d ''` (30A export). 3. Wait for SolarStartTimer. | Charging starts when export exceeds `SolarStartCurrent`. IEC state goes B then C. | Plan 01/PR #65 | P1 |
| MT-301 | Solar stop on insufficient power | 1. While solar charging, inject import: `curl -X POST "http://<ip>/currents?L1=100&L2=100&L3=100" -d ''`. 2. Wait for SolarStopTimer. | Charging stops after `SolarStopTime` seconds of import exceeding `SolarMaxImport`. | Plan 01/PR #65 | P1 |
| MT-302 | EMA smoothing reduces oscillation | 1. Set `EmaAlpha=50` via MQTT. 2. Inject rapidly alternating currents (export/import every 2s). 3. Observe charge current via MQTT. | Charge current changes smoothly, no rapid oscillation. Compare with `EmaAlpha=100` (no smoothing). | Plan 01/PR #65 | P1 |
| MT-303 | Dead band suppresses small adjustments | 1. Set `SmartDeadBand=10` (1.0A). 2. Inject currents that fluctuate within 1A. | Charge current remains stable. No adjustments for changes < 1A. | Plan 01/PR #65 | P2 |
| MT-304 | Ramp rate limits current changes | 1. Set `RampRateDivisor=8`. 2. Inject sudden large surplus (0 to 30A export). 3. Observe current ramp. | Current ramps up gradually over multiple ticks, not instantly. | Plan 01/PR #65 | P2 |
| MT-305 | Solar debug JSON during solar charge | 1. Enable solar debug: `mosquitto_pub -t "SmartEVSE-<serial>/Set/SolarDebug" -m "1"`. 2. Subscribe to `Debug/Solar`. | JSON shows `ImportCurrent`, `SunCurrent`, regulation decisions. Updates at rate-limited interval. | Plan 01/PR #83 | P2 |
| MT-306 | Phase switching 1P to 3P on high surplus | 1. Set `EnableC2=4` (Auto). 2. Start solar charging on 1P with moderate surplus. 3. Inject large surplus (>3x MinCurrent). 4. Wait for switch timer. | SmartEVSE switches from 1P to 3P. Contactor sequence: stop charge, toggle C2, restart. | Plan 01/PR #65 | P1 |
| MT-307 | Phase switching 3P to 1P on low surplus | 1. While charging 3P solar, reduce surplus to below MinCurrent * 3. 2. Wait for SevereTime + hold-down. | SmartEVSE switches from 3P to 1P. Charge continues on single phase. | Plan 01/PR #65 | P1 |
| MT-308 | Phase switch hold-down timer | 1. After 3P→1P switch (MT-307), immediately inject large surplus. 2. Observe. | 1P→3P switch does NOT happen until `PhaseSwitchHoldDownTime` (default 300s) expires. Prevents rapid cycling. | Plan 01/PR #65 | P2 |
| MT-309 | SolarMinRunTime prevents false stop | 1. Set `SolarMinRunTime=60`. 2. Start solar charge. 3. Immediately reduce surplus to trigger NoCurrent within 10s. | NoCurrent does NOT trigger LESS_6A error until 60s minimum run time elapsed. | Plan 01/PR #65 | P2 |
| MT-310 | SolarChargeDelay after stop | 1. Trigger solar stop (MT-301). 2. Immediately inject large surplus. 3. Observe restart delay. | Charge does not restart for `SolarChargeDelay` seconds (default 15s). | Plan 01/PR #65 | P2 |
| MT-311 | NoCurrentThreshold prevents premature stop | 1. Set `NoCurrentThreshold=10`. 2. During solar charge, inject brief shortage (2-3 ticks). | Charging continues (threshold not reached). Only stops after 10 shortage ticks. | Plan 01/PR #65 | P3 |
| MT-312 | Session logged in solar mode | 1. Complete a solar charge session (start + disconnect). 2. Check `/session/last`. | Session JSON has `"mode": "solar"`, correct kwh and timestamps. | Plan 10/PR #89 | P2 |
| MT-313 | API mains staleness fallback | 1. Set `MainsMeterTimeout=30` via MQTT. 2. Start solar charge with API currents. 3. Stop sending currents for 30s. | After timeout, mains readings fall back to MaxMains (conservative). Charging may stop or reduce. | Plan 09/PR #86 | P2 |

---

## Scenario 6: Load Balancing (Multi-EVSE)

**HW req:** Two or more SmartEVSE nodes connected via RS485. One configured as Master.

| ID | Description | Steps | Expected Result | Ref | Pri |
|----|-------------|-------|-----------------|-----|-----|
| MT-400 | Master discovers slave | 1. Power on Master + Slave. 2. Check Master REST API. | Master shows slave node in load balancing status. | Plan 02 | P1 |
| MT-401 | Balanced current distribution | 1. Connect vehicles to both EVSEs. 2. Start charging on both. 3. Observe currents. | Total current stays within MaxMains. Current distributed between nodes. | Plan 02/PR #69 | P1 |
| MT-402 | Priority: first-connected strategy | 1. Set `prio_strategy=1` on Master. 2. Connect vehicle to EVSE-A first, then EVSE-B. | EVSE-A gets higher current allocation. EVSE-B gets remainder. | Plan 02/PR #11 | P2 |
| MT-403 | Convergence without oscillation | 1. Both vehicles charging. 2. Observe current over 60s. | Current values stabilize within 10s, no oscillation > 1A between readings. | Plan 02/PR #69 | P1 |
| MT-404 | Node overview in master web UI | 1. Open Master web UI. | Node overview card shows both EVSE nodes with status, current, state. | Plan 07/PR #85 | P2 |
| MT-405 | Rotation interval | 1. Set `rotation_interval=30` (30 min). 2. Wait or simulate time passage. | Priority rotates between nodes at configured interval. | Plan 02/PR #11 | P3 |

---

## Scenario 7: Error Scenarios and Recovery

| ID | Description | Steps | Expected Result | Ref | Pri |
|----|-------------|-------|-----------------|-----|-----|
| MT-500 | MQTT broker disconnect/reconnect | 1. While SmartEVSE is connected, stop MQTT broker. 2. Wait 30s. 3. Start MQTT broker. 4. Subscribe to topics. | SmartEVSE reconnects automatically. Retained topics re-published. Discovery payloads re-sent. No crash. | Plan 08/PR #91 | P1 |
| MT-501 | Session during MQTT outage | 1. Stop MQTT broker. 2. Connect vehicle, charge, disconnect. 3. Start MQTT broker. 4. Check `Session/Complete`. | Session complete message is published on reconnect (or: tester documents that session is lost during outage). | Plan 10/PR #89 | P2 |
| MT-502 | WiFi disconnect/reconnect | 1. Disable WiFi AP briefly. 2. Re-enable. | SmartEVSE reconnects to WiFi, then MQTT. Charging (if active) continues uninterrupted. | General | P1 |
| MT-503 | Meter communication timeout | 1. Disconnect EV meter RS485 cable. 2. Wait for timeout. 3. Reconnect. | Error flag set (CT_NOCOMM or similar). After reconnect, error clears and readings resume. | Plan 05/PR #76 | P2 |
| MT-504 | API mains data timeout | 1. Set MainsMeter=API. 2. Send currents once. 3. Stop sending for > MainsMeterTimeout seconds. | Stale flag set. Mains falls back to MaxMains. After resuming data, recovery occurs. | Plan 09/PR #86 | P2 |
| MT-505 | Temperature over-limit | 1. (If possible) heat sensor or wait for high temp. 2. Check error flags. | Temperature error flag set. Charge current reduced or stopped. Recovery when temp drops. | General | P3 |

---

## Scenario 8: Security Hardening

All tests are **API-only** (no vehicle needed). These validate the cross-plan hardening from PR #91.

| ID | Description | Steps | Expected Result | Ref | Pri |
|----|-------------|-------|-----------------|-----|-----|
| MT-600 | Path traversal on /diag/file | 1. `curl -s -o /dev/null -w "%{http_code}" "http://<ip>/diag/file/../etc/passwd"` | Returns 400 (Bad Request). Does NOT return file contents. | PR #91 | P1 |
| MT-601 | Path traversal with encoded slashes | 1. `curl -s -o /dev/null -w "%{http_code}" "http://<ip>/diag/file/..%2F..%2Fetc%2Fpasswd"` | Returns 400. | PR #91 | P1 |
| MT-602 | Path traversal with backslash | 1. `curl -s -o /dev/null -w "%{http_code}" "http://<ip>/diag/file/..\\etc\\passwd"` | Returns 400. | PR #91 | P1 |
| MT-603 | Non-printable chars in diag filename | 1. `curl -s -o /dev/null -w "%{http_code}" "http://<ip>/diag/file/test%00.diag"` | Returns 400 (null byte rejected). | PR #91 | P1 |
| MT-604 | JSON injection in /diag/files | 1. (If possible) create a diag file with quotes in name. 2. `curl http://<ip>/diag/files` | Filenames are properly escaped in JSON output. No JSON syntax errors. | PR #91 | P2 |
| MT-605 | Invalid MQTT boundary values | 1. `mosquitto_pub -t "SmartEVSE-<serial>/Set/Current" -m "2000"` (200A). 2. Check current. | Value accepted (200A is within +-200A boundary per PR #91 fix). | PR #91 | P2 |
| MT-606 | MQTT out-of-range rejection | 1. `mosquitto_pub -t "SmartEVSE-<serial>/Set/Current" -m "9999"`. | Value rejected (beyond valid range). Current unchanged. | PR #91 | P2 |
| MT-607 | Invalid REST parameters | 1. `curl -X POST "http://<ip>/settings?mode=abc" -d ''` | Error response. Mode unchanged. | PR #91 | P2 |
| MT-608 | Oversized payload | 1. `curl -X POST "http://<ip>/settings?override_current=99999999999" -d ''` | Error or clamped. No crash or memory corruption. | PR #91 | P2 |
| MT-609 | OCPP URL CRLF injection | 1. (If OCPP configurable via API) attempt URL with `%0d%0a` in OCPP URL field. | URL rejected by character whitelist validation. | PR #91 | P3 |
| MT-610 | Concurrent WebSocket connections | 1. Open web UI in 5 browser tabs simultaneously. | All tabs receive data. No crash. WebSocket connections managed with spinlock protection. | PR #91 | P2 |
| MT-611 | POST /settings with empty body | 1. `curl -X POST "http://<ip>/settings" -d ''` | Returns current settings or empty success. No crash. | General | P3 |

---

## Scenario 9: HomeWizard P1 Meter Integration

**HW req:** HomeWizard P1 meter on local network.

| ID | Description | Steps | Expected Result | Ref | Pri |
|----|-------------|-------|-----------------|-----|-----|
| MT-700 | P1 mDNS auto-discovery | 1. Set MainsMeter to HomeWizard P1. 2. Ensure P1 meter on same VLAN. 3. Reboot SmartEVSE. | SmartEVSE discovers P1 meter via mDNS. Mains readings appear in `/settings`. | Plan 09 | P2 |
| MT-701 | P1 manual IP fallback | 1. Set HomeWizardIP via MQTT to P1 meter's IP. 2. Verify readings. | Readings arrive using direct IP instead of mDNS. | Plan 09/PR #86 | P2 |
| MT-702 | P1 manual IP clear | 1. `mosquitto_pub -t "SmartEVSE-<serial>/Set/HomeWizardIP" -m ""`. | Clears manual IP. Falls back to mDNS discovery. | Plan 09/PR #86 | P3 |

---

## Execution Order Recommendation

For a single tester with one EVSE and one vehicle, execute scenarios in this order
to minimize physical actions:

1. **Scenario 1** (Cold Start) — power-cycle once, verify boot behavior
2. **Scenario 2** (Idle) — all API/MQTT/UI tests, no vehicle needed
3. **Scenario 8** (Security) — all curl-based, no vehicle needed
4. **Scenario 2E** (Diagnostics) — start captures before connecting vehicle
5. **Scenario 3** (Vehicle Connect + Charge) — single connect, verify during charge
6. **Scenario 4** (Vehicle Disconnect) — single disconnect, verify session data
7. **Scenario 5** (Solar Mode) — set to solar, use API current injection
8. **Scenario 7** (Error Recovery) — disconnect broker/WiFi, observe recovery
9. **Scenario 6** (Load Balancing) — only if multi-EVSE hardware available
10. **Scenario 9** (P1 Meter) — only if HomeWizard hardware available

**Estimated time:** ~3 hours for Scenarios 1-5, 7-8 (single EVSE).
Add ~1 hour each for Scenarios 6 and 9 with additional hardware.

---

## Test Summary

| Priority | Count | Description |
|----------|-------|-------------|
| P1 | 30 | Must-test: core charging, session logging, solar regulation, UI, security |
| P2 | 42 | Should-test: secondary features, recovery, diagnostics, MQTT details |
| P3 | 17 | Nice-to-have: edge cases, cosmetic, rarely-used features |
| **Total** | **89** | |

| Hardware Required | Tests |
|-------------------|-------|
| API-only (no vehicle) | MT-001 to MT-076, MT-600 to MT-611 (62 tests) |
| Vehicle or simulator | MT-100 to MT-313 (27 tests) |
| Multi-EVSE | MT-400 to MT-405 (6 tests) |
| OCPP backend | MT-110 to MT-113, MT-207 (5 tests) |
| EVCC instance | MT-120 to MT-123 (4 tests) |
| HomeWizard P1 | MT-700 to MT-702 (3 tests) |
| Home Assistant | MT-040 to MT-044 (5 tests) |
