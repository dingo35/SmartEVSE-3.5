# Priority-Based Power Scheduling

When multiple SmartEVSEs share one mains connection (Power Share), there may not
be enough power for all of them to charge simultaneously. Without priority
scheduling, all EVSEs stop at the same time, restart, see insufficient power, and
stop again — creating an oscillation loop where nobody charges.

Priority scheduling solves this by **keeping some EVSEs charging while pausing
others**, then rotating who gets to charge so every car eventually gets its turn.

## How it works

### The problem

Imagine two EVSEs on a shared 25A connection. Each needs at least 6A (the IEC
61851 minimum). A dishwasher kicks in, leaving only 11A available. That is enough
for one car (6A) but not two (12A). Without scheduling, the system sees "not
enough for everyone" and cuts power to both — even though one car could have kept
charging.

### The solution

When available power drops below what all EVSEs need:

1. **Sort** the EVSEs by priority (see [strategies](#priority-strategies) below).
2. **Allocate** the minimum current (6A) to the highest-priority EVSE first, then
   the next, and so on until power runs out.
3. **Distribute surplus** — any power left over after giving everyone their minimum
   is split equally among the active EVSEs (respecting each EVSE's maximum).
4. **Pause** the remaining EVSEs that didn't get power. They are not in error —
   they are deliberately waiting for their turn.

When power increases again (the dishwasher stops), paused EVSEs are immediately
reactivated in priority order.

### Example

Three EVSEs (A, B, C) on a 25A connection, 1A baseload, MinCurrent = 6A:

| Scenario | Available | A (prio 1) | B (prio 2) | C (prio 3) |
|----------|-----------|------------|------------|------------|
| Plenty of power | 24A | 8A | 8A | 8A |
| Some shortage | 14A | 7A | 7A | **paused** |
| More shortage | 8A | 8A | **paused** | **paused** |
| Total shortage | 0A | **paused** | **paused** | **paused** |

When EVSE C is paused, it does not trigger an error or count towards a shutdown.
Only when even the highest-priority EVSE cannot get its minimum (total shortage)
does the system flag a real power problem.

---

## Settings

These settings appear on the LCD menu and web interface when **PWR SHARE = Master**.

### PRIORITY (LCD: `PRIORITY`)

Choose how EVSEs are ordered when power is scarce.

| Value | Name | Description |
|-------|------|-------------|
| 0 | **Modbus Address** | Lower Modbus address = higher priority. Simple and predictable — Master always charges first, then Node 1, Node 2, etc. |
| 1 | **First Connected** | The car that plugged in first gets priority. Fair for shared parking where arrival order matters. |
| 2 | **Last Connected** | The most recently plugged-in car gets priority. Useful when the latest arrival is most urgent. |

**Default**: Modbus Address (0)

### ROTATION (LCD: `ROTATION`)

How often to rotate which EVSE is actively charging, in minutes.

| Value | Meaning |
|-------|---------|
| 0 | **Disabled** — no rotation. The highest-priority EVSE charges until done or disconnected. |
| 30–1440 | Rotate every N minutes. After this time, the currently charging EVSE is paused and the next one in priority order starts. |

**Default**: 0 (disabled)

**Tip**: Set this to 60 (1 hour) if you want all cars to get some charge during
a long parking session. Set it to 0 if you always want the highest-priority car to
finish first.

### IDLE TMO (LCD: `IDLE TMO`)

How many seconds to wait before deciding an EVSE is idle and skipping to the next
one.

| Value | Meaning |
|-------|---------|
| 30–300 | Seconds. If an EVSE is allocated power but draws less than 1A for this long, it is considered idle and the next EVSE gets its turn. |

**Default**: 60 seconds

This also acts as **anti-flap protection**: no EVSE can be paused or rotated away
within the first `IDLE TMO` seconds after activation. This prevents rapid
on/off cycling that could confuse cars or wear relays.

---

## Timers and behavior

Each EVSE goes through this cycle when it gets activated:

```
         activate
            |
            v
    +-----------------+
    |   Idle Check    |  <-- IdleTimer counts up from 0
    |  (IDLE TMO sec) |
    +-----------------+
            |
     drawing power?
      /          \
    NO            YES
    |              |
    v              v
  pause       +------------------+
  next EVSE   |  Rotation Timer  |  <-- counts down from ROTATION * 60
              |  (if enabled)    |
              +------------------+
                       |
                  timer expires
                       |
                       v
                 pause this EVSE
                 activate next
```

### Step by step

1. **EVSE is activated** — it gets allocated current and its idle timer starts at 0.

2. **Idle timer counts up** — every second, the timer increments. During this
   window, the EVSE cannot be rotated away (anti-flap protection).

3. **At IDLE TMO seconds:**
   - If the EVSE is drawing **less than 1A** → it's idle (car finished charging,
     or not accepting current). Skip to the next EVSE in priority order.
   - If the EVSE is drawing **1A or more** → it's actively charging. Start the
     rotation timer (if rotation is enabled).

4. **Rotation timer counts down** from `ROTATION * 60` seconds. When it reaches
   zero, this EVSE is paused and the next EVSE in priority order is activated.
   The cycle repeats from step 1 for the newly activated EVSE.

5. **Wrap-around** — when the last EVSE in priority order finishes its turn, the
   first EVSE gets reactivated. Disconnected EVSEs (no car plugged in) are
   automatically skipped.

### Power increase

If available power increases (e.g., the oven turns off), paused EVSEs are
immediately reactivated in priority order — no need to wait for a timer. Their
idle timers are reset so they get a fresh anti-flap window.

### Solar mode

In Solar mode, paused EVSEs show "No Sun" instead of "Less 6A" since the
shortage is caused by insufficient solar production, not a mains overload.

---

## MQTT topics

When connected to an MQTT broker, the following topics are available:

### Settings (read/write)

| Topic | Values | Description |
|-------|--------|-------------|
| `SmartEVSE-xxxxx/Set/PrioStrategy` | 0, 1, 2 | Set priority strategy |
| `SmartEVSE-xxxxx/Set/RotationInterval` | 0, 30–1440 | Set rotation interval (minutes) |
| `SmartEVSE-xxxxx/Set/IdleTimeout` | 30–300 | Set idle timeout (seconds) |

### Status (read-only, published by the Master)

| Topic | Example | Description |
|-------|---------|-------------|
| `SmartEVSE-xxxxx/PrioStrategy` | `ModbusAddr` | Current strategy name |
| `SmartEVSE-xxxxx/RotationInterval` | `60` | Current rotation interval |
| `SmartEVSE-xxxxx/IdleTimeout` | `60` | Current idle timeout |
| `SmartEVSE-xxxxx/RotationTimer` | `1423` | Seconds remaining until next rotation |
| `SmartEVSE-xxxxx/ScheduleState` | `Active,Paused,Inactive` | Per-EVSE schedule state (comma-separated) |

### Home Assistant

All settings and status values are automatically discovered by Home Assistant
via MQTT discovery. You will find them under the SmartEVSE device on the
[MQTT integration page](https://my.home-assistant.io/redirect/integration/?domain=mqtt).

---

## REST API

### GET /settings

The response includes scheduling settings and status:

```json
{
  "settings": {
    "prio_strategy": 0,
    "rotation_interval": 60,
    "idle_timeout": 60
  },
  "schedule": {
    "state": ["Active", "Paused", "Inactive"],
    "rotation_timer": 1423
  }
}
```

The `schedule` object is only present when PWR SHARE = Master.

### POST /settings

| Parameter | Values | Description |
|-----------|--------|-------------|
| `prio_strategy` | 0–2 | Set priority strategy |
| `rotation_interval` | 0, 30–1440 | Set rotation interval (minutes) |
| `idle_timeout` | 30–300 | Set idle timeout (seconds) |

Example:
```
curl -X POST 'http://ipaddress/settings?prio_strategy=1&rotation_interval=60' -d ''
```

These settings are only accepted when PWR SHARE = Disabled or Master. Nodes
cannot change scheduling settings (they are controlled by the Master).

---

## Web interface

When PWR SHARE = Master, the web interface shows a **Scheduling** section with:

- **Priority Strategy** — dropdown to select Modbus Address, First Connected, or
  Last Connected
- **Rotation Interval** — number input (0 = disabled, 30–1440 minutes)
- **Idle Timeout** — number input (30–300 seconds)
- **Schedule State** — read-only display showing each EVSE's current state
- **Rotation Timer** — read-only countdown in seconds

---

## Frequently asked questions

**Q: What happens if I only have one EVSE?**
A: Nothing changes. Priority scheduling only activates when PWR SHARE = Master
and there are multiple EVSEs. A single EVSE (PWR SHARE = Disabled) works exactly
as before.

**Q: Can I change priority strategy while cars are charging?**
A: Yes. The new strategy takes effect on the next scheduling cycle (within
seconds). Active EVSEs are not immediately disrupted.

**Q: What if rotation is disabled and one car finishes charging?**
A: The idle detection kicks in. After IDLE TMO seconds of the car drawing less
than 1A, the next car in priority order is activated.

**Q: Does priority scheduling work in Solar mode?**
A: Yes. When solar production drops, only lower-priority EVSEs are paused. The
highest-priority EVSEs keep charging as long as there is enough solar power for
them.

**Q: What about the existing NoCurrent error?**
A: NoCurrent only triggers when even the highest-priority EVSE cannot get its
minimum current (a true hard shortage). Deliberate pauses of lower-priority
EVSEs do not trigger NoCurrent, preventing the oscillation bug.
