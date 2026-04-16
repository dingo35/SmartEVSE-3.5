# Owner guide

Your charger is installed and reaches state A → B → C with a car plugged
in. This guide covers the day-to-day: picking a mode, starting a
session, reading the dashboard, managing RFID, and updating the
firmware.

If your charger isn't installed yet, see
[guide-installer.md](guide-installer.md). If something doesn't work the
way this guide describes, the
[troubleshooting guide](troubleshooting.md) is the first stop.

---

## 1. The two interfaces you'll actually use

### Web UI — `http://smartevse-<serial>.local/`

Your daily driver. Works on phone and desktop. Shows live currents, lets
you change modes, start/stop, adjust limits. No login needed by default.
Set an LCD PIN and enable AuthMode to require authentication — see
[security.md](security.md).

### LCD + buttons — on the device itself

Needed for first-time configuration (WiFi, meter selection, electrical
parameters). After that, most owners rarely touch it. Good for when the
Web UI is unreachable or when you're standing in the meter cabinet
already.

> [!TIP]
> Everything on the LCD is also available in the Web UI. Everything in
> the Web UI is **not** available on the LCD (some advanced settings are
> web-only). If in doubt, use the web.

### Which one for what

| Task | LCD | Web UI |
|---|---|---|
| First-time WiFi setup | ✅ required | ❌ |
| Select meter type / address | ✅ | ✅ |
| Change mode (Normal/Smart/Solar) | ✅ | ✅ |
| Start/stop a charge session | ✅ | ✅ |
| Adjust current on the fly | ❌ (reboot required) | ✅ |
| Schedule a future charge | ❌ | ✅ |
| Add RFID cards | ❌ | ✅ |
| Read per-phase current | limited | ✅ full |
| Firmware update | ❌ | ✅ |
| MQTT / HA configuration | ❌ | ✅ |
| OCPP configuration | ❌ | ✅ |

---

## 2. The five modes

Pick one based on what you want the charger to do when you plug in.

| Mode | What it does | Use when |
|---|---|---|
| **Off** | Never charges, ignores the car | You're going away, want to be sure nothing starts |
| **Normal** | Charges at a fixed current you set (Override Current) | You always want the same rate, don't care about solar or grid load |
| **Smart** | Charges as fast as possible without overloading the grid connection | You want fast charging but respect the house main (3×25 A etc.) |
| **Solar** | Charges only from excess solar production, matches PV surplus | You have solar, want to self-consume before exporting |
| **Pause** | Mid-session pause, keeps the car connection alive | You want to interrupt a running session without unplugging |

### Normal mode in detail

- Set **Override Current** (e.g. 16 A). The charger asks the car for
  that much, full stop.
- Ignores the mains meter — will pull the full rate even if it trips the
  house main. **Only safe if the branch is isolated from shared house
  load, or if the charger current is well below the house main rating.**
- The simplest mode. Works without any mains meter at all.

### Smart mode in detail

- Needs a working **MainsMeter**. Without it, the charger doesn't know
  how loaded the house is.
- Tracks `MaxMains` (the house main rating) and subtracts the current
  house consumption. Charges with whatever's left, up to `MaxCurrent`.
- Reacts to changes in ~2–10 seconds depending on meter source (Modbus
  meter fastest, DSMR4 P1 meter slowest).
- Will drop below 6 A? Pauses with `LESS_6A` until more power is
  available. This is not a bug — 6 A is the Mode-3 minimum.

### Solar mode in detail

- Needs the MainsMeter **and** requires that the meter sees export (negative Isum).
- Starts charging only when solar export exceeds `StartCurrent` (A per
  phase) for `StopTime` minutes.
- Stops (or downshifts) when export drops below zero for long enough.
- `ImportCurrent` lets you top up from grid to reach at least 6 A total —
  useful for marginal-solar days where pure-solar can't sustain 6 A.
- The most-discussed mode on the forum. Tuning is car-dependent. See
  §10 and the [solar-smart-stability.md](solar-smart-stability.md) deep
  dive.

### Off and Pause

- **Off**: the charger is running, communicates with the car, but will
  never close the contactors. Use when you're going away and want to be
  sure nothing unexpected happens.
- **Pause**: interrupts an active session without disconnecting the
  car. Tap again (or hit Resume) to continue.

---

## 3. Starting a charge session

### Plug-and-charge (no RFID)

Default if you haven't configured RFID. Plug in, the car wakes up, and
the charger starts per the current mode. No Web UI interaction needed.

### RFID-protected

Plug in → car waits. Swipe an enrolled card → charger authorizes → car
starts charging. See §5 for card management.

### From the Web UI (manual start in Normal mode)

1. Plug the car in (state A → B).
2. On the Web UI, click **Normal** (or the mode you want).
3. Adjust **Override Current** with the slider if needed.
4. Car transitions to state C, contactor closes, charging starts.

### Scheduled / delayed

Web UI → set **Start Time** and **Stop Time** → pick the mode → save.
The charger waits until the start time, then runs in the chosen mode
until the stop time.

Useful for overnight off-peak billing (Tibber, EnergyZero, ANWB
dynamic-pricing etc.) where you want charging between 01:00 and 05:00
regardless of when the car was plugged in.

The schedule repeats daily if you tick "daily repeat", otherwise it's
one-shot.

---

## 4. Reading the dashboard

The Web UI shows several numbers. Here's what each one means when
something looks wrong.

### State

IEC 61851-1 state letter:

| State | Meaning |
|---|---|
| A | No car connected |
| B | Car connected, not ready to charge |
| B1 | Car connected, charging disabled (pause / access off) |
| C | Charging |
| C1 | Charging, but car reported 1-phase to 3-phase transition needed |
| D | Ventilation required (rare, mostly forklifts) |
| E | Hard error — contactor stays open |
| F | No CP signal — wiring problem or car disconnected unexpectedly |

If the state stays B and never goes to C when you expect charging, the
car doesn't see a valid pilot signal. See troubleshooting.

### Mains currents (L1 / L2 / L3 / Isum)

What the MainsMeter reports right now. Positive = import, negative =
export (solar).

- **Normal mode** ignores these.
- **Smart mode** uses them to stay under `MaxMains`.
- **Solar mode** waits for `Isum` to go sufficiently negative before
  starting.

### EV currents (L1 / L2 / L3)

What the charger is actually delivering right now. If you set 16 A and
the EV current shows 8 A, either the car chose to draw less (most EVs
can slow down below the offered current) or PP detected a 16 A cable
and you're on a 32 A setpoint that the cable caps.

### Charge current / Override Current

What the charger is *offering* the car via CP PWM. The car may draw
less. If EV current ≪ offered, that's usually car behavior, not a
charger fault.

### Solar state (Solar mode only)

- `SolarStopTimer`: seconds remaining before solar mode gives up on
  insufficient export.
- `StartCurrent` / `StopTime` / `ImportCurrent`: the thresholds you
  configured. See §10.

---

## 5. RFID card management

### Enable RFID

Web UI → RFID reader section → set reader type. Supported readers listed
in [configuration.md](configuration.md).

### Add a card

1. Web UI → RFID → click **Add**.
2. Swipe the card on the reader within the prompt window.
3. The card UID appears. Optionally label it (e.g. "Piet's car").
4. Save.

### Remove a card

Web UI → RFID → click the trash icon next to the card.

### Bulk enrolment

Upload `rfid.txt` via the `/update` page. Each line is one UID,
hex-formatted (e.g. `04A1B2C3D4E5F6`). Up to 100 cards.

### Forgotten which card is which

UIDs are displayed on the Web UI during authorization attempts and in
the OCPP logs. Swipe each candidate, watch the UI, label accordingly.

### Auto-auth (OCPP FreeVend)

If you have OCPP enabled and want no-card-needed authorization, enable
**Auto-authorize** with a default idTag. Every plug-in is auto-approved
with that tag. Use at your own risk on public chargers — anyone who
plugs in draws on your OCPP contract.

---

## 6. Firmware updates

> [!IMPORTANT]
> Read the release notes before updating. Some updates require a
> one-time re-configuration (e.g. when a setting's NVS key format
> changes). Most don't.

### Standard flow

1. Download `firmware.signed.bin` from the
   [releases page](https://github.com/basmeerman/SmartEVSE-3.5/releases).
2. Web UI → `/update` (or click the Update link).
3. If AuthMode is on, the UI prompts for the LCD PIN first.
4. Choose the file → upload.
5. The device reboots. Web UI becomes reachable again in ~20 seconds.

### Debug builds

If a maintainer or contributor sends you a debug build (for diagnosing
a specific issue), it's named `firmware.debug.signed.bin`. Same upload
flow. Debug builds run telnet-accessible verbose logging — useful for
capturing a bug report, not for everyday use.

### Something went wrong after update

Roll back: download the previous release version, upload it. NVS
settings usually survive downgrades but not always — if behaviour
looks corrupted, do a settings reset (see [configuration.md](configuration.md)).

Firmware updates will **not** reset your WiFi, RFID cards, or your LCD
PIN. Those survive reflashing.

---

## 7. Common adjustments

### "I got a new electricity contract with a different house rating"

Change `MaxMains`. LCD menu → Electrical → MaxMains. Or Web UI →
Settings → Current Main. Set to the new rating minus 1–2 A of margin.
Takes effect immediately, no reboot.

### "Solar mode isn't starting even when I see export"

Check `StartCurrent`. Default is 4 A/phase — that's 4 × 230 = ~920 W/phase
of sustained export needed before it will engage. On a cloudy day with
fluctuating export, the threshold may never be met for the required
time. Lower `StartCurrent` to 2 A (requires ~460 W/phase). Trade-off:
more false starts, more cycling of the contactor.

### "Solar mode cycles on/off every few minutes"

Increase `StopTime` (minutes of below-threshold before stopping). Default
is often 10 minutes; if your solar fluctuates heavily, try 15 or 20. The
contactor will stay closed through short cloud gaps.

See [solar-smart-stability.md](solar-smart-stability.md) for the full
tuning guide.

### "Smart mode is pulling too much, tripping something"

Check `MaxMains` first. If that's correct, check that the MainsMeter is
actually reporting accurate current (Web UI → Mains currents — do they
match a clamp-meter reading on the main cable?). A meter in error
state (stale data, wrong CT orientation) will make Smart mode overshoot.

### "I want to charge with less current for a night"

Web UI → Normal mode → set Override Current to what you want (e.g. 10 A).
Valid during that session only. Revert next time by picking Smart or
Solar again.

### "The car charges on one phase when I want three"

- Check whether your car actually supports 3-phase AC charging
  (many don't — Zoe original, older Kona, some Ioniqs are 1-phase AC
  only despite 3-phase looking physically available).
- If it does, check `EnableC2`. If set to "Always Off", the second
  contactor is off and you're forced to 1-phase.
- If your EV is correctly 3-phase-capable and `EnableC2` is "Always On"
  but the car still chooses 1-phase, that's the car's choice — some
  cars pick 1-phase below a certain offered current. Offer 16 A/phase
  (3×16 A = 11 kW) to get 3-phase on most cars.

---

## 8. Access control

Three layers of "who can do what":

### Car-side access

- **RFID reader present + enrolled card required**: only enrolled cards
  can start a session.
- **No RFID**: plug-and-charge.

### Web UI access

- **AuthMode = 0 (default)**: Web UI is open to anyone on your LAN.
- **AuthMode = 1**: Web UI requires LCD-PIN verification for mutating
  actions. The PIN is stored on the device (`LCDPin` setting). After a
  correct PIN, the session stays authenticated for 30 minutes idle.

See [security.md](security.md) for the full access-control model. For routine home use, AuthMode=0 is fine. For shared LAN
(apartment, office), enable AuthMode=1 and set a non-zero PIN.

### OCPP-side access

If OCPP is enabled and configured with a backend (Tibber, E-Flux,
Tap Electric, Monta etc.), authorization is delegated to the backend.
An unknown idTag returns `Invalid` and the session doesn't start.

---

## 9. Multi-charger setup (master/slave)

If you have two or more chargers on one house connection, one is the
master, the others are nodes (slaves). They share the MainsMeter reading
and coordinate so that neither exceeds `MaxMains`.

Quick primer:

- The **master** is the one with `LoadBl=1`. It has the MainsMeter
  attached.
- Each **node** has `LoadBl=2, 3, 4…` (one per node, sequential).
- All modules are wired to the same RS485 bus (A↔A, B↔B, GND↔GND).

Day-to-day you mostly interact with the master's Web UI — it knows what
all nodes are doing. Each node has its own Web UI but shows its own
state, not the cluster state.

Detailed wiring in [guide-installer.md](guide-installer.md) §12.
Priority allocation (who gets current first when there's not enough for
everyone) in [priority-scheduling.md](priority-scheduling.md).

---

## 10. Solar mode tuning — first-time setup

For most houses, the defaults are close enough. Tune only if the mode
misbehaves.

### The three knobs

| Setting | What it does | Default | Lower this if | Raise this if |
|---|---|---|---|---|
| `StartCurrent` (A/phase) | How much export sustained before starting | 4 A | You want to catch more marginal export | False starts on brief sun gaps |
| `StopTime` (minutes) | Below-threshold duration before stopping | 10 min | You want quick response to clouds | Too much cycling on/off |
| `ImportCurrent` (A) | Grid top-up to reach 6 A minimum charge | 0 A | You want to guarantee 6 A even with less solar | You want pure-solar only |

### Common pattern on Dutch summer day

`StartCurrent=3 A`, `StopTime=15 min`, `ImportCurrent=0`. Will start
around noon, run through mid-afternoon, survive short cloud periods,
stop cleanly when sun drops below the threshold at ~17:00.

### Common pattern on marginal-solar winter day

`StartCurrent=2 A`, `StopTime=20 min`, `ImportCurrent=2 A`. Will grab
whatever the array produces and top up to 6 A from grid so charging
actually happens. Accept you're not 100% self-consumption, you're
~80%+.

### Car-specific quirks

Some EVs don't like being offered less than 6 A/phase for extended
periods — they drop the session. Others handle it fine. If your car
keeps disconnecting in solar mode, raise `ImportCurrent` so the minimum
offered stays ≥ 6 A.

See [solar-smart-stability.md](solar-smart-stability.md) for the deep
dive, including phase-switching (1P↔3P) and the `EnableC2=AUTO` option.

---

## 11. Home Assistant / MQTT / REST — overview

If you run Home Assistant or want scripting, see
[guide-integrator.md](guide-integrator.md). Short version:

- **MQTT**: the charger publishes every change and auto-discovers HA
  entities. Topic prefix configurable. See
  [mqtt-home-assistant.md](mqtt-home-assistant.md).
- **REST**: all settings exposed via `/settings` (GET/POST), currents
  via `/currents`, modes via `/mode`. See
  [REST_API.md](REST_API.md).
- **OCPP**: WebSocket backend integration. See [ocpp.md](ocpp.md).

Owner-scoped note: for home use where you're the only user, MQTT + HA
covers 95% of what you'd want integration-wise. OCPP is mainly for
public-charger setups and ToU-billing integrations.

---

## 12. Backup and restore

The device stores configuration in flash NVS. There is **no one-button
export**. If you reflash or replace the PCB:

- WiFi password: re-enter via LCD.
- MaxMains / MaxCurrent / MaxCircuit / meter selection: re-enter via LCD.
- RFID cards: re-upload via `rfid.txt`.
- MQTT / OCPP settings: re-enter via Web UI.

Keep a copy of your settings somewhere (a text file, a printout of the
Web UI's settings panel). The LCD menu has a compact summary screen
showing the main values; photograph it.

A factory reset (clears everything) is available via LCD menu →
Settings → Reset. Use before hand-off if selling the device.

---

## 13. When things go wrong

| Symptom | First check | Doc |
|---|---|---|
| Car plugged in but never charges | State on the LCD / Web UI | [troubleshooting.md](troubleshooting.md) |
| Solar mode cycles on/off | `StartCurrent` + `StopTime` | [solar-smart-stability.md](solar-smart-stability.md) |
| Smart mode trips the main | `MaxMains` value, MainsMeter reading accuracy | [troubleshooting.md](troubleshooting.md) |
| No MQTT entities in HA | Topic prefix, broker reachability, discovery prefix | [mqtt-home-assistant.md](mqtt-home-assistant.md) |
| Web UI rejects your firmware upload | LCD PIN not verified / unsigned file | [security.md](security.md) |
| Diag panel shows "Capturing" forever | Press Stop | known-fixed bug (recent release) |
| LCD button does nothing | LCD PIN lock active | enter PIN via Web UI |

For telnet debug logs (verbose runtime output), the charger runs a
telnet server on port 23 by default — useful when filing a bug report.
Connect with any telnet client.

---

## 14. Where next

- **[guide-integrator.md](guide-integrator.md)** — if you run Home Assistant or want REST/MQTT/OCPP.
- **[configuration.md](configuration.md)** — full LCD menu reference.
- **[solar-smart-stability.md](solar-smart-stability.md)** — solar mode
  deep dive.
- **[Tweakers "Zelfbouw Laadpaal" thread](https://gathering.tweakers.net/forum/list_messages/1648387)** —
  Dutch community support.
