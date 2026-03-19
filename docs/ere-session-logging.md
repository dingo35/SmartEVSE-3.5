# ERE Session Logging

SmartEVSE supports charge session logging for Dutch ERE (Emissie Reductie Eenheden)
certificate reporting. Each completed charge session is published via MQTT with all
fields required for ERE submission to an inboekdienstverlener.

## What is ERE?

ERE certificates are issued for electric vehicle charging using renewable energy in
the Netherlands. Chargepoint operators report charged kWh per session to an
inboekdienstverlener, who submits the data to the Dutch Emissions Authority (NEa).
A MID-certified EV meter is required for eligible sessions.

## How it works

SmartEVSE tracks charge sessions automatically based on vehicle connection state:

1. **Session start** — when the vehicle begins charging (CP state B to C transition)
2. **Session end** — when the vehicle disconnects (CP state returns to A with 12V pilot)
3. **MQTT publish** — a retained JSON message is published to MQTT on session completion

There is no configuration needed. Session logging is always active when MQTT is
connected.

## MQTT topic

On session completion, a retained JSON message is published to:

```
<mqtt_prefix>/Session/Complete
```

Where `<mqtt_prefix>` is your configured MQTT topic prefix (visible in `/settings`
under `mqtt.topic_prefix`).

### Payload format

```json
{
  "session_id": 1,
  "start": "2026-03-19T14:30:00Z",
  "end": "2026-03-19T18:45:00Z",
  "kwh": 12.345,
  "start_energy_wh": 142300,
  "end_energy_wh": 154645,
  "max_current_a": 16.0,
  "phases": 3,
  "mode": "solar",
  "ocpp_tx_id": null
}
```

### Field reference

| Field | Type | Description |
|-------|------|-------------|
| `session_id` | integer | Auto-incrementing session counter (resets on reboot) |
| `start` | string | Session start time in ISO 8601 UTC format |
| `end` | string | Session end time in ISO 8601 UTC format |
| `kwh` | number | Total energy charged in kWh (3 decimal places) |
| `start_energy_wh` | integer | EV meter reading at session start (Wh) |
| `end_energy_wh` | integer | EV meter reading at session end (Wh) |
| `max_current_a` | number | Peak charge current during session (amps) |
| `phases` | integer | Number of phases at session end (1 or 3) |
| `mode` | string | Charging mode: `"normal"`, `"smart"`, or `"solar"` |
| `ocpp_tx_id` | integer or null | OCPP transaction ID when OCPP is active, otherwise `null` |

### ERE field mapping

| ERE requirement | MQTT field |
|-----------------|------------|
| Start time | `start` |
| End time | `end` |
| Energy charged (kWh) | `kwh` |
| Chargepoint ID | Use your device serial number (from `/settings` → `serialnr`) |

## REST API

The last completed session is also available via REST:

```
GET /session/last
```

**Responses:**
- `200 OK` with JSON body (same format as MQTT payload) — last session available
- `204 No Content` — no session has completed since last reboot

Example:
```bash
curl -s http://smartevse-XXXX.local/session/last | jq .
```

## Collecting sessions with Home Assistant

Since the MQTT message is **retained**, Home Assistant will receive the last session
on connect. To build a session history for ERE reporting, create an automation that
triggers on the MQTT topic and appends each session to a file or database.

### Example automation

```yaml
automation:
  - alias: "Log EVSE charge session"
    trigger:
      - platform: mqtt
        topic: "SmartEVSE-XXXX/Session/Complete"
    action:
      - service: notify.file
        data:
          message: "{{ trigger.payload }}"
          title: ""
```

This appends each session's JSON to a notification file. At the end of the year,
convert the collected sessions to CSV for submission to your inboekdienstverlener.

### MQTT sensor for current session data

```yaml
mqtt:
  sensor:
    - name: "EVSE Last Session kWh"
      state_topic: "SmartEVSE-XXXX/Session/Complete"
      value_template: "{{ value_json.kwh }}"
      unit_of_measurement: "kWh"
      device_class: energy

    - name: "EVSE Last Session Duration"
      state_topic: "SmartEVSE-XXXX/Session/Complete"
      value_template: >
        {% set start = as_datetime(value_json.start) %}
        {% set end = as_datetime(value_json.end) %}
        {{ (end - start).total_seconds() | int // 60 }} min
```

Replace `SmartEVSE-XXXX` with your device's MQTT prefix.

## OCPP alignment

When OCPP is active, charge sessions are aligned with OCPP transactions:

- The `ocpp_tx_id` field is set (non-null) to indicate OCPP was managing the session
- Session boundaries match the OCPP StartTransaction/StopTransaction lifecycle
- Energy readings use the same `EVMeter.Import_active_energy` source as OCPP meter values

OCPP users already receive session data in their OCPP backend. The MQTT session
message provides a local copy for verification or backup purposes.

## Session boundaries

| Scenario | Session start | Session end |
|----------|---------------|-------------|
| Normal charge | Vehicle starts drawing current (state B→C) | Vehicle disconnects (state → A, pilot 12V) |
| Interrupted charge | Same as above | Same as above (partial session is recorded) |
| Multiple charge cycles | Each B→C transition starts a new session | Each return to state A ends the session |
| OCPP controlled | Aligned with `beginTransaction()` | Aligned with `endTransaction()` |

## Requirements for ERE eligibility

To use SmartEVSE session data for ERE certificate submission:

- **MID-certified EV meter** — the EV energy meter connected to SmartEVSE must be
  MID-certified (e.g., Eastron SDM630-MCT MID, ABB B23 MID). Without MID
  certification, the session kWh values are not accepted by the NEa.
- **Accurate time** — SmartEVSE synchronizes its clock via NTP when connected to WiFi.
  Ensure your network allows NTP traffic so timestamps are accurate.
- **MQTT backend** — sessions must be collected and stored externally (Home Assistant,
  Node-RED, or any MQTT logger). SmartEVSE does not store session history on-device.

## Considerations

### Data persistence

SmartEVSE does **not** store session history in flash or RAM beyond the single
last-completed session. The MQTT retained message ensures the most recent session
survives MQTT broker restarts, but historical sessions must be collected by an
external system (Home Assistant, database, file logger) as they are published.

If your MQTT broker or Home Assistant is offline when a session completes, that
session's MQTT message will be lost. For reliable ERE reporting, ensure your MQTT
infrastructure has high availability, or consider using OCPP with a cloud backend
as the primary data source.

### Session ID counter

The `session_id` is an auto-incrementing counter that resets to 1 on every SmartEVSE
reboot. It is intended for correlation within a single uptime period, not as a
globally unique identifier. For ERE reporting, use the combination of `start` timestamp
and your chargepoint serial number as the unique session identifier.

### Time accuracy

Session timestamps depend on the ESP32's system clock, which is synchronized via NTP
on WiFi connection. If NTP is unavailable (no internet, DNS failure), timestamps may
drift or be incorrect. The ESP32 does not have a battery-backed real-time clock, so
after a power cycle without NTP, the clock starts from epoch zero until NTP succeeds.

Verify your timestamps are reasonable before submitting ERE data.

### Energy measurement accuracy

The `kwh` value is calculated as `end_energy_wh - start_energy_wh` from the connected
EV meter. The accuracy depends entirely on the meter hardware:

- MID-certified meters (Class B, 1% accuracy) are required for ERE eligibility
- Non-MID meters may still provide useful session data for personal tracking but are
  not accepted for certificate submission
- If no EV meter is configured, energy values will be zero

### Partial sessions

If the SmartEVSE reboots during an active charge session (power outage, firmware
update), the in-progress session is lost. No session-complete message is published.
The energy consumed during the interrupted session is still recorded by the EV meter's
cumulative register but is not captured as a discrete session.

### Multiple vehicles / dual EVSE

Each SmartEVSE unit tracks its own sessions independently. In a dual-EVSE setup, each
unit publishes to its own MQTT prefix. Ensure your collection system subscribes to
both prefixes.

### OCPP as alternative

If you use an OCPP backend (e.g., SteVe, Open E-Mobility, a commercial provider),
session data is already reported via OCPP's `StartTransaction` / `StopTransaction`
messages. The MQTT session logging provides a local redundant copy but is not the
primary data source in OCPP setups. Verify with your inboekdienstverlener which data
source they accept.

### Annual reporting workflow

A typical ERE reporting workflow with SmartEVSE:

1. **Collect** — Home Assistant automation appends each session JSON to a log file
   throughout the year
2. **Export** — At year end, convert the collected JSON sessions to CSV with columns:
   date, start time, end time, kWh, chargepoint ID, meter serial number
3. **Submit** — Upload the CSV to your inboekdienstverlener's portal
4. **Verify** — Cross-check total kWh against the MID meter's cumulative reading
