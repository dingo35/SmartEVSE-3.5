# Integrator guide

You want to connect the charger to Home Assistant, a custom script, an
OCPP backend, or run multiple chargers on one mains connection. This
guide routes you through the options and covers the parts where people
most often trip.

For the reference manuals see
[mqtt-home-assistant.md](mqtt-home-assistant.md),
[REST_API.md](REST_API.md),
[ocpp.md](ocpp.md),
[priority-scheduling.md](priority-scheduling.md), and
[load-balancing-stability.md](load-balancing-stability.md). This guide
picks what to read from each depending on what you're building.

---

## 1. Pick your integration path

| Goal | Mechanism | Complexity | Docs |
|---|---|---|---|
| Dashboards + simple automations in Home Assistant | MQTT + HA auto-discovery | low | §3 |
| Feed mains current from HA (no physical mains meter) | REST `EM_API` | low–medium | §5 |
| Custom script / Node-RED / non-HA platform | REST | medium | §4 |
| Dynamic-pricing charging (Tibber, EnergyZero, ANWB) | REST or MQTT, triggered by HA automation | medium | §4 + §8 |
| Public / shared / billed charging | OCPP 1.6j | high | §6 |
| Two or more chargers on one house connection | RS485 master/slave | medium | §7 |
| Subpanel protection (shared branch) | No integration needed — `MaxCircuit` setting | low | [guide-installer.md §11](guide-installer.md) |

Multiple paths can co-exist. MQTT + REST + OCPP all run simultaneously.
OCPP + internal load balancing are **mutually exclusive** — see §6.

---

## 2. Before you start

Two preconditions for every integration:

1. **The charger must be on your network and reachable.** `http://smartevse-<serial>.local/` or by IP. Pingable.
2. **You need to decide on authentication.** By default the Web UI and REST API are open on the LAN (AuthMode=0). If you set AuthMode=1 every mutating REST call needs the `X-Auth-PIN` header. For home use AuthMode=0 is fine; for shared LAN it isn't. See [security.md](security.md).

If you're feeding the charger from another system (HA sending mains
current), enable that system first and verify it's reading values
correctly before wiring it into the charger.

---

## 3. Home Assistant via MQTT

The path of least resistance. The charger publishes every state change
to MQTT and announces its entities via HA auto-discovery. No YAML
required on the HA side — add the MQTT integration, point the charger
at your broker, entities appear.

### Setup

1. Web UI → **MQTT Configuration**.
2. Host = your broker IP / hostname.
3. Port = 1883 (plain) or 8883 (TLS).
4. Username / password if the broker requires them.
5. Topic prefix — defaults to `smartevse-<serial>`. Change if you run
   multiple chargers and want a common root.
6. Save. The **Status** line should flip to "Connected" within a few seconds.
7. In HA, entities appear under the device "SmartEVSE <serial>".

### What you get out of the box

- Current per phase (mains + EV), power, energy.
- State, mode, error flags.
- Charging active / enabled.
- Override current, limits.
- Solar state.

All as sensors. Mode switching, override current, mains-current
injection, MQTT command topics — all writable. Full reference:
[mqtt-home-assistant.md](mqtt-home-assistant.md).

### The two things people get wrong

**Wrong topic prefix.** If HA shows no entities, the most common cause
is that the charger publishes to `smartevse-1234/` and your HA MQTT
integration filters on a different prefix. Match them.

**Broker doesn't accept the client.** If the Web UI shows
"Connected" → "Disconnected" flapping, check broker logs. Common causes:
ACL not allowing the client, auth failing silently, TLS cert mismatch.

### TLS

If your broker requires TLS, upload the CA PEM in the MQTT panel. On
Let's Encrypt brokers the default CA bundle usually works — leave the
field empty. If you run your own CA, paste its PEM into the CA
Certificate field.

Note: TLS on the charger costs ~20–25 KB of RAM. Plain MQTT is fine on
a trusted LAN.

### Change-only publishing

By default the charger publishes every 10 seconds even if nothing
changed. With **Change-Only** enabled, it publishes only when a value
changes (plus a heartbeat every N seconds to confirm liveness).
Reduces MQTT traffic by 70–95%. Recommended.

---

## 4. REST API — when MQTT isn't enough

When you need synchronous request/response, non-HA integration, or fine
control over timing, use REST.

### Anatomy

- `GET /settings` — complete device state as JSON. Passwords redacted.
- `POST /settings?<param>=<value>&...` — change settings. Body empty.
- `GET /currents` — live phase currents.
- `POST /currents?<param>=...` — **feed** mains or EV currents (see §5).
- `POST /mode?<mode>&override_current=...` — change charging mode.
- `GET /diag/status`, `/diag/start`, `/diag/stop` — diagnostic capture.

Full endpoint catalogue: [REST_API.md](REST_API.md).

### Auth

If AuthMode=1, every `POST` needs a valid session. Clients obtain one by
`POST /lcd-verify-password` first. `GET` endpoints are unprotected.

On AuthMode=0 (default), no auth required. Anyone on the LAN can POST.
That's why AuthMode=1 is worth enabling on anything shared.

### Rate limits

The REST server accepts up to 12 concurrent HTTP connections on v4
hardware, 8 on v3. A browser typically opens 2–3; leave headroom. Don't
spawn parallel POSTs at high rate — serialise.

`POST /lcd-verify-password` is rate-limited (escalating backoff on
failures). Five wrong PIN attempts in succession lock you out for 5
minutes, six lock you out for 30.

### Typical non-HA script pattern

```
# Dynamic-pricing charging with EnergyZero prices
# Check price, decide target current, POST it.

hourly_price = fetch_energyzero_price_for_current_hour()
if hourly_price < 0.15:
    current = 32   # full speed, cheap hour
elif hourly_price < 0.25:
    current = 16
else:
    current = 0    # don't charge

POST http://smartevse-1234.local/mode?mode=1&override_current=${current*10}
```

Values in `override_current` are tenths of an amp — `160` means 16.0 A.
Rounding to whole amps is standard.

---

## 5. Feeding mains current from Home Assistant (`EM_API`)

The use case: your house has a smart meter or HA energy dashboard, but
the charger has no physical mains meter. Instead of buying a Sensorbox,
let HA push the current readings.

### The plumbing

1. Web UI → MainsMeter → set type to **API** (`EM_API`).
2. HA reads phase currents from your smart meter (DSMR, HomeWizard,
   Shelly Pro 3EM, etc.).
3. HA automation POSTs to `/currents`:

   ```
   POST /currents?battery=1&L1=<amps>&L2=<amps>&L3=<amps>
   ```

   Values in amps (float). Signed: positive = import, negative = export.

4. The charger treats this as if a physical meter reported those values.

### The timing trap

**The charger's API has a 10-second timeout.** If HA hasn't posted a
fresh reading within 10 seconds, the charger marks the meter as stale
and disables Smart/Solar mode.

DSMR4 smart meters emit telegrams every 10 seconds. HA's DSMR
integration receives them, but then has to run an automation that POSTs
to the charger. Small delays add up. With DSMR4 + slow HA, you'll see
intermittent stale-meter errors.

Mitigations:
- DSMR5 smart meters emit every 1 second. With DSMR5 + fast HA, you're
  well within budget.
- Run the reader → POST chain on a small dedicated device (Raspberry Pi
  running a short Python loop), not through HA's full automation graph.
- Cache the last telegram in HA and replay it every 5 seconds if no new
  data arrived. Doesn't help solar accuracy but keeps the charger happy.

### Don't set the charger's internal poll faster than this

The charger polls its own meter ~every 2 seconds (Modbus path). The
`EM_API` path depends on what you push. Pushing **more often than every
2 seconds** doesn't buy you anything — the charger's control loop
updates that fast, no faster.

### Worked example — HA automation

```yaml
# Reads three HomeWizard P1 phase-current sensors, POSTs every 5 seconds
- alias: Feed SmartEVSE mains currents
  trigger:
    platform: time_pattern
    seconds: "/5"
  action:
    - service: rest_command.smartevse_currents
      data:
        l1: "{{ states('sensor.p1_current_l1') }}"
        l2: "{{ states('sensor.p1_current_l2') }}"
        l3: "{{ states('sensor.p1_current_l3') }}"
```

```yaml
rest_command:
  smartevse_currents:
    url: "http://smartevse-1234.local/currents?battery=1&L1={{ l1 }}&L2={{ l2 }}&L3={{ l3 }}"
    method: POST
```

For deeper treatment see [power-input-methods.md](power-input-methods.md).

---

## 6. OCPP backends

OCPP 1.6j over secure WebSocket. Authorization, transactions, and
charge-profile control delegated to a cloud backend. Use when:

- You want public / shared charging with per-user billing.
- Your energy provider (Tibber, Vandebron, etc.) requires OCPP for
  dynamic tariff control.
- You want centralised management of multiple chargers across sites.

### What works, what's experimental

| Feature | Status |
|---|---|
| BootNotification | stable |
| Heartbeat | stable |
| Authorize + StartTransaction + StopTransaction | stable |
| MeterValues (energy reporting) | stable |
| RemoteStart / RemoteStop | stable |
| Smart charging profiles (CL-limit from backend) | stable |
| FreeVend / AutoAuth (plug-and-charge with no RFID) | stable |
| TLS WebSocket (wss://) | stable |
| Reset / GetConfiguration / FirmwareUpdate | basic |

Full reference: [ocpp.md](ocpp.md).

### Setup

1. Web UI → **OCPP Configuration**.
2. Enable OCPP.
3. Backend URL — your provider's WebSocket endpoint.
4. Charge Box Id — the identifier your provider gave you.
5. Password (auth_key) — your provider's token, if they use SP2 basic auth.
6. Save.
7. The **WS Status** should flip to "Connected" within ~20 seconds.

### OCPP vs internal load balancing

> [!IMPORTANT]
> When OCPP is enabled **and** the backend sends a charging profile
> (current limit), the internal Smart/Solar-mode load balancing is
> **overridden**. The backend's limit wins. This is by design — an OCPP
> backend operator expects full control.
>
> If you want both — solar-adaptive charging **and** OCPP billing —
> configure the backend to send no-limit profiles (or a profile larger
> than `MaxMains`) and let the charger's Smart/Solar mode do the
> adaptation. Check with your provider; not all support no-profile mode.

### Provider notes (from user reports)

| Provider | Works? | Notes |
|---|---|---|
| Tibber | yes | Smart charging profiles via the Tibber-Pulse integration |
| EnergyZero | yes | Native MQTT preferred over OCPP for most users |
| E-Flux | yes | Standard SP2 auth; works with `auth_key` set |
| Tap Electric | yes | Plug-and-charge with FreeVend idTag |
| Monta | yes | Reported working; requires specific URL format |
| ANWB Energy | not tested | Community reports mixed |
| Public networks (Allego, Vattenfall) | not applicable | OCPP is the wrong protocol for end-user connection to public networks |

Reports change over time. Check recent GitHub issues or the Tweakers
thread before committing.

### Authentication detail

The `auth_key` field is the Basic Auth password used at the WebSocket
handshake. It is never returned in `GET /settings` — only
`auth_key_set: true/false`. When changing other OCPP settings, leave
the `auth_key` field blank (or at the `••••••••` placeholder) to keep
the stored value. Typing a new value replaces it.

---

## 7. Multi-charger (master/slave) configuration

Wiring is in [guide-installer.md §12](guide-installer.md). This section
covers configuration and priority.

### Basic setup

| Charger | `LoadBl` | MainsMeter | Notes |
|---|---|---|---|
| Master | 1 | configured on this module | Has the sensorbox / Modbus meter / P1 reader |
| Node 1 | 2 | disabled | Inherits mains data from master via RS485 |
| Node 2 | 3 | disabled | |
| Node 3 | 4 | disabled | |
| … | up to 8 | | |

Each node's own `MaxCurrent` determines its branch limit. The master
coordinates allocation so the **sum** across nodes never exceeds
`MaxMains`.

### Priority (who charges first when there isn't enough)

Four strategies:

| Strategy | Behaviour |
|---|---|
| **First-come** (default) | The first car to connect gets full current; later cars share leftover |
| **Fair** | All active cars get equal share of available current |
| **Rotating** | After `RotationInterval` minutes, cycles priority among connected cars |
| **Manual** | Admin sets per-node priority (not user-facing) |

Set on the master's Web UI → Load Balancing → Priority Strategy. Full
semantics: [priority-scheduling.md](priority-scheduling.md).

### Idle timeout

If a car stays in state B (connected, not charging) for longer than
`IdleTimeout` minutes, the master reclaims its current allocation and
gives it to other charging cars. Prevents one "parked-but-plugged" car
from blocking others.

Default 30 minutes. Set to 0 to disable.

### Master without Load Balancing

Special case: one master with `LoadBl=1` and **zero** nodes is valid.
It's effectively a standalone `LoadBl=0` master with identical behaviour
— the load-balancing code runs but has nothing to allocate to. Useful
when you plan to add nodes later without reconfiguring.

---

## 8. Timing and cadence — common traps

Three different clocks, all independent:

| Clock | Interval | Who sets it |
|---|---|---|
| Modbus meter poll | ~2 s | Firmware (fixed) |
| `EM_API` push from HA | whatever you configure | You. 5 s is a reasonable default. |
| MQTT publish | ~10 s (periodic) or on-change | Firmware (configurable) |
| DSMR telegram from smart meter | 1 s (DSMR5) or 10 s (DSMR4) | Meter hardware |
| HA automation tick | 1 s granularity | HA config |

### The misconception

"The charger needs data every 1 second." Wrong. The charger polls every
2 seconds internally and times out at 10 seconds. Any cadence in that
range works. Push more often than every 2 seconds and you're burning
CPU for no benefit.

### The real constraint

- **Push < 2 s** — redundant, HTTP overhead negligible but wasteful.
- **Push 2–5 s** — ideal for Smart/Solar control responsiveness.
- **Push 5–10 s** — fine for Smart mode, marginal for Solar (slower
  reaction to cloud events).
- **Push > 10 s** — meter times out, mode disabled until fresh data.

---

## 9. Debugging integration issues

### Nothing at all from MQTT / broker shows no connections

1. Check the charger's Web UI MQTT status line.
2. If "Disconnected": check broker host:port reachable from the
   charger's IP. SSH into something on the same LAN and `telnet
   broker-host 1883`.
3. If TLS: check the CA cert matches the broker's certificate chain.
4. If authenticated: check the username/password were saved (you'll see
   `password_set: true` after save).

### HA entities appear but stale

- Check **Change-Only** is on for reasonable traffic, **off** if you
  want forced periodic updates.
- Check broker retain/QoS — some brokers drop messages under load.
- Check HA's MQTT integration is actually subscribed to the topic
  prefix. Match with the charger's configured prefix.

### REST POST returns 401

AuthMode=1, missing or expired session. Call `POST /lcd-verify-password`
first, then repeat the call with the session cookie.

### REST POST returns 403

Origin / CSRF check failed. Only triggers on browser requests with a
wrong `Origin:` header. Server-to-server calls without `Origin` are
fine. If you're calling from a browser extension or cross-origin app,
the charger is rejecting correctly.

### REST POST returns 429

Rate limit. Only on `/lcd-verify-password`. Wait the `Retry-After`
seconds and try again. Don't retry in a tight loop.

### OCPP shows Disconnected after a reboot

Backend URL misconfigured, or backend auth rejected. Enable verbose
logging (telnet 23) and watch the WebSocket handshake fail reason.
Common: wrong `auth_key`, wrong URL path, TLS cert mismatch.

### The diag panel auto-starts on page load

Known-fixed in recent release. Press Stop once, reload — the backend
now clears the profile to "off" on Stop so it won't re-attach.

---

## 10. Where next

- **[mqtt-home-assistant.md](mqtt-home-assistant.md)** — full MQTT topic reference + HA entity list.
- **[REST_API.md](REST_API.md)** — every endpoint, every parameter.
- **[ocpp.md](ocpp.md)** — OCPP 1.6j message coverage.
- **[priority-scheduling.md](priority-scheduling.md)** — priority-strategy semantics.
- **[power-input-methods.md](power-input-methods.md)** — Sensorbox / P1 / Modbus / API comparison.
- **[load-balancing-stability.md](load-balancing-stability.md)** — multi-node behaviour and edge cases.
- **[Tweakers thread](https://gathering.tweakers.net/forum/list_messages/1648387)** — Dutch community, many HA / P1 / OCPP integration war stories.
