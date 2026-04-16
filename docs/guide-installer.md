# Installer guide

You have the hardware. This guide takes you from a boxed module to a
working charger: choosing the contactor and RCD, sizing the branch
circuit, shielding the CP cable, commissioning without a car connected.

If you have not yet bought hardware, the Dutch
[Tweakers forum](https://gathering.tweakers.net/forum/list_messages/1648387)
is the canonical community resource and has years of accumulated buying
and integration advice. This guide assumes you have the v3 or v3.5 PCB
in hand.

**Audience:** competent-DIY to electrician. If you have never wired a
meter cabinet (Dutch: *meterkast*, Flemish: *zekeringkast*) and don't
know the difference between an MCB and an RCBO, hire an electrician.
Doing a 32 A branch wrong kills people.

> [!WARNING]
> All wiring work is done with **full mains power disconnected at the
> house main switch**. Verify with a non-contact tester. Do not rely
> on the RCD being tripped — the neutral may still be live.

---

## 1. What you need before you start

### Hardware checklist

| Item | Notes |
|---|---|
| Charger module (v3 or v3.5 PCB) | 3-DIN width, 52 × 91 × 58 mm. Check you got the connector block + CP/PP terminals + RGB LED |
| Contactor | See [§5 Contactor selection](#5-contactor-selection). **Do not** use silent/AC-DC contactors |
| Residual-current device | See [§4 RCD selection](#4-rcd-selection). Type A with internal DC-leak detection is standard on this PCB |
| MCB / circuit breaker | Sized to branch cable + connector rating (16 A or 32 A typical) |
| Meter (one of) | See [§7 Meter selection](#7-meter-selection). Sensorbox, HomeWizard P1, DIN-rail Modbus meter, or API-fed |
| Branch cable | 2.5 mm² (16 A) or 6 mm² (32 A), H07V-K or XMvK. Length drives the sizing — derate for long runs |
| Control cable | Cat5 or better for RS485 meter bus. CAT6/7 SFTP if CP runs > 30 m (see [§6](#6-control-wiring-and-cp-cable)) |
| Connector | Type 2 socket or fixed cable; PP resistor matters — see [§8 Cable and connector](#8-cable-and-connector) |
| Enclosure (outdoor) | IP54+ if mounted outside; cable glands sized to cable OD |

### Tools

- Multimeter with continuity and AC voltage modes
- Non-contact voltage tester
- Torque screwdriver (terminal torque matters for 32 A connections — manufacturer spec is usually 1.2–2.0 Nm)
- Crimping tool for ferrules if using stranded cable into cage-clamp terminals
- Oscilloscope *optional but useful* for CP signal verification at commissioning

### What you do NOT need

- PlatformIO or any dev environment. You can install and commission the
  hardware without touching code. Firmware updates happen via the web UI.
- A car. The first-boot checks in §10 run without an EV connected.

---

## 2. Safety — non-negotiables

Short list. If any of this is news, stop and hire an electrician.

1. **De-energize before wiring.** Main switch off, verify with a tester,
   tag it off. Both line and neutral.
2. **PE continuity** from the SmartEVSE chassis screw all the way back to
   the installation earth rod. Test with a low-ohm meter (< 0.5 Ω is a
   reasonable target for a domestic setup).
3. **RCD test.** After power-up, press the test button on every RCD in
   the meter cabinet. If any fails to trip, fix before connecting a car.
4. **Torque.** Under-torqued 32 A terminals overheat and melt. Use a
   torque screwdriver and check every terminal.
5. **Phase rotation.** For 3-phase installs, verify L1/L2/L3 rotation
   matches existing house wiring. Wrong rotation can damage the
   connected EV or confuse the meter's phase mapping.

---

## 3. Meter cabinet planning

### Dutch 3×25 A typical layout

The common domestic Dutch supply (NL *aansluiting*) is **3×25 A**, shared
with the house consumer unit. Belgian domestic is often **3×40 A**. Your
SmartEVSE + car is a meaningful fraction of that budget. A 32 A charger
on a 3×25 A supply will trip the house main within seconds in worst-case
load — this is exactly what load balancing is for.

```
                 Grid
                  │
      ┌────────── Kasko / hoofdschakelaar (house main switch) ─────────┐
      │                                                                 │
  [Hoofdzekering 3×25A / 3×35A / 3×40A]                                  │
      │                                                                 │
  [Electricity meter (P1 port for DSMR smart meter)]                    │
      │                                                                 │
  ┌───┴────────────┐                                                    │
  │ Hoofdschakelaar│                                                    │
  │  (main switch) │                                                    │
  └───┬────────────┘                                                    │
      │                                                                 │
      ├──── Existing house groups (via existing ALS/RCBO)                │
      │                                                                 │
      └──── NEW: SmartEVSE branch ──── RCBO/MCB ──── Contactor ─── Socket/Cable
```

### Space budget

| Component | DIN modules | Notes |
|---|---|---|
| SmartEVSE PCB | 3 | Fits standard DIN |
| Contactor 4-pole, 40 A | 3–4 | IKA432-40 is 3 modules |
| 3-pole MCB / RCBO | 3 | Sized to branch |
| Meter (if DIN Modbus) | 3–4 | Eastron SDM630 is 4 modules |
| Wiring space | +20% | Terminal access, dressing |

Budget **at least 9–10 DIN modules of free space** for a fresh install.
If the existing meter cabinet is full, you need an extension box
(*onderkast* / *aanbouwkast*).

### Electronics next to socket, or in the meter cabinet?

Both work. The Tweakers thread leans toward **electronics in the meter
cabinet**, power cable out to a socket on the wall. The alternative —
SmartEVSE PCB in a pole or wall housing at the charge point — requires a
bigger outdoor enclosure and more weather sealing.

The control cable (RS485 + CP) between meter cabinet and charge point is
the length-critical bit. Up to ~30 m is forgiving; longer than that,
shield the CP cable. See §6.

---

## 4. RCD selection

The forum argues about this more than anything else. The short version:

> [!NOTE]
> The SmartEVSE v3 PCB has **built-in 6 mA DC-leakage detection
> (EN 62955)**. This is what elevates a Type A RCD to meeting the
> Mode-3 charger DC-fault requirement. You do NOT need an external
> Type B RCD on this hardware — a Type A **on top of** the PCB's DC
> detection is the standard, approved-by-norm combination.

### The options

| RCD class | Trips on | Needed on SmartEVSE v3? | Cost |
|---|---|---|---|
| Type AC | AC faults only | **Never** — not compliant for EV charging | Cheap |
| Type A | AC faults + pulsing DC (< 6 mA DC blinding threshold) | **Yes** — this is what you buy | €15–40 |
| Type B | AC + smooth DC up to 10 mA | **Not required** because PCB does DC detect | €100–250 |
| Type B+ (Doepke EV / KNX variants) | B-rated with MID metering | Overkill for home install | €300+ |

### Combined RCBO or separate RCD + MCB

Two ways to wire the branch:

**Option 1 — one RCBO on the EV branch.** Combined device, 3 modules.
Example: Hager ADS932D (4-pole, 32 A, 30 mA type A). Cleanest install.

**Option 2 — RCD on the common feed + MCB on each branch.** One Type A
RCD feeds multiple branches (EV + solar + other), then a plain MCB on
each branch. Cheaper per-branch if you have 2–3 branches behind one RCD.
But one faulty device trips them all.

Option 1 is the default recommendation for a single new EV branch. Option 2
becomes cheaper once you're adding solar inverters on the same RCD.

### Common mistakes

- **Using a Type AC RCD** on the EV branch. Non-compliant. Fails insurance
  check. Replace with Type A.
- **Assuming "Type B+ is safer so buy that"**. Type B on SmartEVSE v3 is
  double-protection that may cause nuisance trips because the PCB's
  own DC detect can interact with a downstream Type B. Type A is
  specified for a reason.
- **Under-sized RCD current rating.** A 25 A RCD fronting a 32 A branch
  will overheat. RCD rating ≥ branch rating.

---

## 5. Contactor selection

The single biggest gotcha. From the product documentation and a decade
of forum experience:

> [!WARNING]
> Do **not** use silent contactors, AC-DC contactors, or energy-efficient
> contactors (ABB ESB series, Hager ESCxxx**S** with the "silent" S suffix).
> The SmartEVSE drives the contactor coil with a low-hold / high-pull
> waveform that does not work on silent contactors — the contactor will
> chatter, weld, or fail to close.

### Recommended parts

| Part | Rating | Poles | Notes |
|---|---|---|---|
| Iskra IKA432-40 | 40 A, AC-1 | 4 (3P + N) | Default recommendation. Widely available. |
| Hager ESC440 (without "S") | 40 A, AC-1 | 4 | Equivalent to Iskra. Verify it's the non-silent variant |
| Chint NC1-4025 | 40 A | 4 | Budget option, confirmed working on the forum |
| Siemens 3RT2026-1BB40 | 25 A, AC-3 | 4 | More expensive but heavier-duty |

### Sizing

**16 A charger installation:** 25 A AC-1 contactor is fine.
**32 A charger installation:** 40 A AC-1 is the default. Do not undersize —
contactors derate with ambient temperature and lifetime cycles.

### Pole count

For 3-phase charging use a **4-pole contactor** (3P + N). Switching the
neutral matters for two reasons:

1. **Safety.** Phase-only switching leaves L1 potentially live on the
   CCS connector if the neutral is interrupted elsewhere in the
   installation. Code prohibits this in most jurisdictions.
2. **Phase switching (EnableC2).** If you want to add a second
   contactor to switch between 1-phase and 3-phase on the fly (see §9),
   the first contactor must already be 4-pole so C2 can break L2+L3
   without exposing dangerous neutral-only conditions.

For fixed 1-phase installs, a 2-pole (L + N) contactor is correct.

---

## 6. Control wiring and CP cable

Three cable runs come out of the SmartEVSE PCB:

1. **RS485 meter bus** — to the mains meter (Sensorbox, Modbus DIN
   meter). Cat5 or better, one twisted pair for A/B, one pair for GND.
2. **CP + PP signal** — to the Type 2 socket or the fixed cable. Carries
   the ~1 kHz PWM pilot signal.
3. **Contactor coil** — 230 VAC, switched by the PCB.

### CP cable: shielded vs unshielded

The CP signal is a ±12 V PWM at 1 kHz. It is more noise-sensitive than
people expect. **Unshielded control cable over 20–30 m will cause
intermittent charge aborts** — the car sees noise on CP, interprets it
as a cable removal, stops charging mid-session.

| CP run length | Minimum cable |
|---|---|
| ≤ 5 m (PCB next to socket) | Any 0.75 mm² flex is fine |
| 5–30 m | Cat5 is adequate in most houses, twisted pair for CP/PE |
| > 30 m | **SFTP CAT6/7, shield bonded to PE at ONE end only (the meter cabinet side)** |
| Outdoor / long run > 50 m | Consider not doing it. Move the PCB closer. |

The Tweakers thread has documented cases of SVV 10×0.8 (common
telephone wire) causing 3–5 s charge aborts that disappeared the moment
the cable was replaced with CAT6 SFTP.

### RS485 meter bus

| Parameter | Value |
|---|---|
| Baud rate | 9600 |
| Parity | None |
| Stop bits | 1 |
| Address range for meters | **11 or higher** — addresses 1–10 are reserved for SmartEVSE node + Sensorbox |
| Twisted pair | A/B must share one twisted pair (e.g. green + green/white on Cat5) |
| GND | Bond the meter GND to the SmartEVSE GND terminal |

Two meters sharing the bus? Different addresses, daisy-chained A to A,
B to B. No star topology on RS485.

---

## 7. Meter selection

The charger needs to know the **mains current** (to avoid overloading the
house connection) and *optionally* the **EV current** (for energy
billing / subpanel protection). Four ways to feed it:

| Method | Hardware | Telegram rate | Reliability | Cost |
|---|---|---|---|---|
| **Sensorbox v2** | SmartEVSE companion, 3× CT clamps | ~1 s | High — purpose-built | €60–80 |
| **HomeWizard P1 meter** | P1 to wifi, reads DSMR smart meter | 1 s (DSMR5) or 10 s (DSMR4) | High on DSMR5, marginal on DSMR4 | €40 |
| **Modbus DIN meter** | Eastron SDM630 etc., CT or direct | 1–2 s | Highest | €100–180 |
| **API-fed** (`EM_API`) | Home Assistant / custom script pushes via REST | Anything slower than 10 s = charger timeout | Depends on your HA uptime | Free (if you already run HA) |

### Decision tree

- **Dutch house with DSMR5 smart meter + you already run Home Assistant**
  → HomeWizard P1 or HA-fed. P1 is simpler, HA-fed is more flexible.
- **Older DSMR4 meter (10 s telegrams)** → Sensorbox or Modbus. API-fed
  will timeout-loop the charger.
- **No smart meter / commercial install / subpanel protection** →
  Modbus DIN meter direct-inline.
- **Budget constrained, short run, single-phase** → Sensorbox.

For a comprehensive reference see [power-input-methods.md](power-input-methods.md).

### CT clamp orientation

CT clamps have an **arrow printed on the case** indicating current
direction. Arrow points **toward the load** (away from the grid). Wrong
direction = negative current values in the charger = load balancing
thinks you're exporting when you're consuming, and it will happily add
32 A on top.

**Verification after first boot:**

1. Turn off any solar inverter.
2. Watch the Web UI's current readings with one house appliance running
   (kettle, dryer).
3. Values should be positive (~8–10 A for a kettle on one phase).
4. If negative, flip the CT clamp or swap the two secondary wires at
   the Sensorbox terminals.

### Modbus meter address

The charger reserves Modbus addresses 1–10 for its own use
(SmartEVSE nodes + Sensorbox). **Meters must be at address 11 or higher**,
or they will conflict with node communication.

Common cause of "no meter data" after install: meter left at factory
default address 1. Set it to 11 or 12 on the meter's own menu before
wiring.

### Inverted wiring (Eastron fed from below)

Dutch meter cabinets wire the supply from the bottom. Eastron 3-phase
meters assume top-feed, so in a bottom-fed cabinet the current
direction is reversed. Select **"Inverted Eastron"** meter type in the
SmartEVSE menu, not plain "Eastron". Confirm the fix by checking that
current values are positive under load (see CT orientation check above).

---

## 8. Cable and connector

### Fixed cable vs socket

**Fixed cable (Type 2 plug dangling from the wall):**

- User plugs the car in directly.
- Proximity Pilot resistor is internal; the EVSE knows the cable's
  rating and doesn't need to negotiate.
- Slightly simpler install, better for single-user home.

**Type 2 socket (user brings their own cable):**

- Future-proof, cable replaceable.
- PP resistance detected by the EVSE — a 13 A cable caps charging at
  13 A even if the socket/branch can do 32 A.
- Required for commercial/public installs.

### PP resistor values (IEC 62196)

If you're wiring a fixed cable and need to set the PP resistor:

| Current rating | Resistor (PP → PE) |
|---|---|
| 13 A | 1.5 kΩ |
| 20 A | 680 Ω |
| 32 A | 220 Ω |
| 63 A | 100 Ω |

Fixed-cable setups without a PP resistor will report "no cable" —
always install the resistor.

---

## 9. Mounting and enclosure

### DIN rail, indoor

Most installs. Standard 35 mm DIN rail in the meter cabinet. No special
sealing needed. Good airflow — avoid mounting directly under a hot
inverter.

### Outdoor / pole

If the SmartEVSE PCB lives outside:

- **IP54 minimum** for the enclosure, IP65 if exposed to driven rain.
- **Cable glands** sized to cable OD, not the hole size. Wrong-sized
  glands leak, water gets into the PCB, PCB dies.
- **UV resistance** on the enclosure if south-facing.
- **Ventilation.** Sealed enclosures trap heat. Specs say the PCB
  operates to +50 °C ambient; in summer sun an unventilated black
  plastic box exceeds that easily.

### Outdoor socket height and protection

Dutch convention: 1.0–1.2 m above ground. Higher in areas with snow
drifts. Rain cover over the socket. Do not mount directly in the drip
line of the garage roof.

---

## 10. First boot and commissioning — **without a car connected**

> [!IMPORTANT]
> Do **not** connect a car until every step here passes. Connecting an
> EV to a miswired charger can brick the car's onboard charger.
> Repairs are out-of-warranty and cost €2000+.

### Step 1 — Power on, no car

1. Close the branch MCB / RCBO.
2. LCD should boot. Version number appears.
3. Check the LCD for **State: A** (no car connected).
4. RGB LED should glow white or green (idle, no error).

If the LCD does not boot: check 230 V at the module's mains terminals.
If it boots and shows an error: note the error code and see
[troubleshooting.md](troubleshooting.md).

### Step 2 — WiFi and web UI

Follow [configuration.md#wifi](configuration.md) to join your WiFi. Then
browse to `http://smartevse-<serial>.local/`.

Set an **LCD PIN** immediately. Several security features (web UI auth
gate, rate-limited PIN verify, debug-firmware upload control) rely on
having a PIN set. Default PIN = 0 means no auth.

### Step 3 — Meter verification

With the car unplugged and one house appliance running (dryer, kettle):

1. On the web UI, look at **Mains currents** (L1/L2/L3).
2. Values should be **positive** and roughly match the appliance load.
3. If you see the kettle on a different phase than expected, your
   CT-clamp-to-phase mapping is swapped. Fix at the Sensorbox, not in
   firmware.
4. If you see negative values, CT clamp is backwards. Flip it.

### Step 4 — CP signal verification (optional, recommended)

With an oscilloscope on the CP pin to PE:

| State | CP waveform | DC level |
|---|---|---|
| No car (A) | +12 V flat | +12 V |
| Car connected, not ready (B) | ±12 V, 10 % duty 1 kHz PWM | ~+9 V mean |
| Car drawing power (C) | ±12 V, duty matches current setpoint | ~+6 V mean |

If you see a flat DC level with no PWM in state B, the LEDC channel
isn't driving (firmware issue, not wiring).

### Step 5 — Mode and current limits

On the LCD or web UI, set:

| Setting | Value |
|---|---|
| Mode | **Normal** for the first plug-in test. Switch to Smart/Solar later |
| MaxMains | Your house main rating, minus a margin. On a 3×25 A = set 23 A |
| MaxCurrent | Charger branch rating. 16 A or 32 A |
| MaxCircuit | 0 (disabled) unless you're on a subpanel (see §11) |
| LoadBl | 0 (standalone). Set to 1+ only for multi-charger installs (see §12) |

Save. Reboot if the LCD prompts.

### Step 6 — First car plug-in

1. Plug in your car. State should transition **A → B** within a second.
2. Press the RFID card (if configured) or accept in Normal mode.
3. State transitions **B → C**. Contactor should close — audible click.
4. Measure current on L1 with a clamp meter. Should match the current
   setpoint on the LCD (e.g. 16 A).

If the contactor clicks in and then out immediately, see §13 gotchas.

If the car never exits state B, the car doesn't see a valid CP PWM. Go
back to step 4 — CP signal is likely broken.

### Step 7 — RCD test

Press the RCD test button on the EV branch. The charger drops out. LCD
shows state A. Reset the RCD. Plug the car back in. Charging resumes
from state A → B → C. If it doesn't resume cleanly, check for welded
contactor (§13).

---

## 11. Subpanel / "garage" configuration

If the SmartEVSE branch is behind a subpanel (garage, outbuilding) that
also feeds other loads (workshop tools, lighting), you need
**MaxCircuit** to protect the subpanel feed.

```
                             mains
                               │
                   [house main breaker 3×25 A]
                               │
                        [Mains meter]
                               │
              ┌────────────────┴─────────────────┐
              │                                  │
   [house groups, 16 A]            [subpanel feed, 16 A]
                                                 │
                                          [EV meter]
                                                 │
                                     ┌───────────┴──────────┐
                                     │                      │
                           [workshop 16 A]       [SmartEVSE 16 A]
```

**Configuration:**

| Setting | Value |
|---|---|
| MaxMains | 23 A (the house main, minus margin) |
| MaxCircuit | 15 A (the subpanel feed, minus margin for the other workshop loads) |
| Mode | **Smart** or **Solar** (Normal mode ignores MaxCircuit) |

The charger will clamp its own current so that neither the 23 A mains
nor the 15 A subpanel is exceeded. You do **not** need Load Balancing
(LoadBl) for this — that's only for multiple SmartEVSE modules sharing
one supply.

---

## 12. Multi-charger (master/slave) wiring

Up to 8 SmartEVSE modules on one mains supply. One master, up to 7
slaves (nodes). They share one meter reading and coordinate current
allocation.

### Wiring

- A, B, GND from the **master** to **all nodes**. Daisy-chain A→A, B→B,
  GND→GND. Not a star.
- Sensorbox (if used) connects to the **same bus** as the master — the
  +12 V wire goes to **only one** SmartEVSE (the master).

### Configuration

| Role | LoadBl value |
|---|---|
| Standalone (only charger) | 0 |
| Master | 1 |
| First slave | 2 |
| Second slave | 3 |
| … up to 8 modules | up to 8 |

Only the master has MaxMains / MaxCircuit configured. Slaves inherit
from the master via Modbus broadcast.

### Common wiring mistake

The Tweakers thread's recurring version of this: "I have two
SmartEVSEs but LoadBl=0 on both, and they both try to pull 32 A." This
is not load balancing — it's two standalone chargers on the same bus
fighting over the meter. Set one to LoadBl=1, the other to LoadBl=2.

---

## 13. Common installation gotchas

| Symptom | Cause | Fix |
|---|---|---|
| Contactor chatters / fails to close | Silent/AC-DC contactor | Replace with AC-1 type (Iskra IKA432-40 etc.) |
| Currents shown negative in web UI | CT clamp arrow backwards | Flip the clamp around the wire |
| No meter data / timeout errors | Modbus address conflict (meter at 1–10) | Set meter address to 11+ |
| No meter data / wrong phase mapping | Eastron fed from below | Select "Inverted Eastron" not "Eastron" |
| No meter data / wrong poles | A/B swapped | ABB and Carlo Gavazzi have A/B reversed vs. most meters. Swap. |
| Charging aborts every 3–5 seconds | Unshielded CP cable > 20 m | Replace with SFTP CAT6/7, shield to PE one side |
| Car state A → B but never C | Car not seeing valid CP PWM | Check CP wiring, verify PWM on scope |
| Car charges at 16 A when set to 32 A | PP cable limit (13 A or 16 A cable) | Upgrade cable or set MaxCurrent to match |
| PCB boots but web UI rejects firmware upload | Unsigned firmware requires a verified LCD PIN (debug builds only); release builds always require a signed `.signed.bin` | Either upload a signed build, or set + verify LCD PIN first. See [security.md](security.md) |
| LCD shows phase error on 3-phase | L1/L2/L3 rotation wrong | Swap two phase wires at the contactor input |

---

## 14. Firmware first-flash

The PCB ships with a firmware on it. It probably isn't the latest. To
update:

1. Join WiFi (§10 step 2).
2. Browse to `http://smartevse-<serial>.local/update`.
3. Upload the latest `firmware.signed.bin` from the
   [releases page](https://github.com/basmeerman/SmartEVSE-3.5/releases).
4. Unsigned `firmware.bin` is rejected by default on release builds.
   For local development iteration see
   [guide-contributor.md](guide-contributor.md).

Full build + flash instructions in [building_flashing.md](building_flashing.md).

---

## 15. Next steps

Once the charger is installed, powered, and state A → B → C works with
your car in Normal mode:

- **[guide-owner.md](guide-owner.md)** — day-to-day usage, switching to Smart/Solar mode, LCD menu tour.
- **[guide-integrator.md](guide-integrator.md)** — MQTT topics, Home Assistant, REST API, OCPP providers.
- **[configuration.md](configuration.md)** — LCD menu reference.
- **[power-input-methods.md](power-input-methods.md)** — detailed meter setup.
- **[mqtt-home-assistant.md](mqtt-home-assistant.md)** — MQTT integration.

If something is wrong, the
[troubleshooting guide](troubleshooting.md) is the first stop.

For Dutch-language community support, the
[Tweakers "Zelfbouw Laadpaal ervaringen" thread](https://gathering.tweakers.net/forum/list_messages/1648387)
is the canonical resource — maintainers and experienced users read and
answer there.
