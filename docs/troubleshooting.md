# Troubleshooting

Symptom-indexed. Start at the symptom, work through the triage steps.
If nothing here matches, capture a telnet debug log (§11) and open an
issue or ask on the
[Tweakers thread](https://gathering.tweakers.net/forum/list_messages/1648387).

---

## 1. Car plugged in but never starts charging

State on the LCD / Web UI stays at B.

| Check | How | If bad |
|---|---|---|
| Car is authorised to charge | Web UI: Access = ON? RFID swiped? | Enable access, swipe card, or disable RFID gate |
| Mode is not OFF or PAUSE | Web UI top mode indicator | Switch to Normal / Smart / Solar |
| In Smart/Solar: enough power available | Web UI → Mains currents | Lower `MaxMains` if already at limit; in Solar mode wait for export |
| In Smart mode: `MaxMains` is not 0 | Web UI → Settings | Set to actual house main rating |
| CP PWM is driving the pilot | Scope on CP pin, state B should show ±12 V 1 kHz PWM | If flat +12 V, CP is not being driven — possible wiring fault or hardware issue |
| PP cable resistor detected | Web UI → cable limit shows expected A | If cable limit is wrong, check PP resistor (220 Ω = 32 A, 680 Ω = 20 A, 1.5 kΩ = 13 A) |
| Car supports AC charging at offered current | Car's own display / app | Some cars refuse offers below 6 A; some drop session below 4 A sustained |

**If the state goes B → C briefly then back to B**, the car rejected
the session after starting. Usually a CP signal issue (noise on long
unshielded cables — see [guide-installer.md §6](guide-installer.md)) or
the car's own fault (battery full, thermal limit).

---

## 2. Charging stops randomly mid-session

Short (seconds) interruptions, then resumes.

| Check | Cause |
|---|---|
| CP cable length > 20 m, unshielded | Electromagnetic noise triggers car to disconnect |
| Contactor is a silent / AC-DC type | Chattering contactor opens briefly |
| RCD trips intermittently | Car's own DC leakage approaches the 30 mA threshold; Type A RCD is borderline |
| House brownout pulls mains below tolerance | Session drops until power stabilises |
| In Smart mode, mains current spiked above `MaxMains` | Charger cut the EV side to protect the house main |

The most common cause on long-run installs: **unshielded CP cable**.
Replace with SFTP CAT6/7, shield bonded to PE at the meter-cabinet
side only. See [guide-installer.md §6](guide-installer.md).

---

## 3. Solar mode cycles on/off

Charging starts, runs for 1–3 minutes, stops. Repeats.

| Check | Fix |
|---|---|
| `StopTime` too short for cloud events | Raise to 15–20 minutes |
| `StartCurrent` too high for your array | Lower. 2–3 A/phase is a reasonable floor |
| Car won't accept < 6 A offered | Set `ImportCurrent` so the minimum stays ≥ 6 A |
| Some cars need 6 A × 3 phases = 18 A min | Lower pseudo-single-phase with `EnableC2=AUTO` if hardware supports C2 |
| Sensorbox CT clamps on wrong phases | Verify with kettle / dryer — one appliance should show on one phase only |

See [solar-smart-stability.md](solar-smart-stability.md) for the full
tuning guide.

---

## 4. Solar mode never starts

Export visible on the smart meter, but charger doesn't engage.

| Check | Cause |
|---|---|
| Mode is Solar on the Web UI | Easy miss — confirm it's not still Normal/Smart |
| Mains meter shows negative Isum during export | If positive, CT clamp orientation is wrong — flip it |
| `StartCurrent` threshold not reached | Lower `StartCurrent` |
| Export was transient (< `StopTime` threshold) | Wait longer, or lower `StopTime` |
| Car is in state B but `Access` is Off | Enable access or swipe RFID |
| `HomeBatteryCurrent` feeds falsely low export | If you're pushing HA battery data, check the sign and path |

---

## 5. Smart mode trips the house main

The charger pulls too much and the main breaker pops.

| Check | Cause / Fix |
|---|---|
| `MaxMains` value matches your main rating minus margin | Set to rating − 1–2 A (e.g. 23 A on a 3×25 A supply) |
| MainsMeter is actually reporting real data | Web UI currents — compare to clamp meter |
| Meter poll cadence is fast enough | Modbus: 2 s native. API-fed: must be < 10 s between updates |
| Non-EV load spiked faster than control loop | Control loop responds in ~2–5 s; a 10 kW surge from electric shower can outrun it |
| Meter shows wrong phase mapping | A kettle on L1 should appear on L1 in the charger's display; if not, CT to phase wiring is swapped |

In practice, leave `MaxMains` at `rating − 2 A` to give the control
loop headroom for fast transients.

---

## 6. Wrong or missing current values

Mains or EV currents show zero, wrong sign, or obviously wrong magnitude.

### Zero on all phases

| Cause | Fix |
|---|---|
| Meter type set to None | Web UI → select your meter type |
| Modbus address wrong | Meter default 1 collides with SmartEVSE cluster (1–10). Set meter to 11+ |
| RS485 wiring A↔B swapped | Verify with a multimeter in DC-volts mode (idle line has ~2.5 V differential) |
| Cat5 run not twisted-pair for A/B | Redo with one twisted pair for A+B (green + green/white etc.) |
| Wrong meter type selected | Check the actual meter model — generic "Modbus" is not valid, pick specific type |

### Correct magnitude, wrong sign (import shown as export or vice versa)

| Cause | Fix |
|---|---|
| CT clamp arrow points toward grid | Flip the clamp. Arrow should point toward the load (house) |
| Eastron meter fed from below (Dutch cabinet) | Change meter type from "Eastron" to "Inverted Eastron" |
| Wrong phase mapped | Swap CT secondary wires at the Sensorbox |

### Currents roughly right but one phase wildly off

| Cause | Fix |
|---|---|
| CT clamp not fully closed on the wire | Open and re-close until the halves click together |
| CT clamp on wrong phase | Rewire to correct phase |
| One CT wire loose at the Sensorbox terminal | Retighten |

---

## 7. Contactor chatters or welds

Contactor buzzes, rapid-clicks, or stays closed after session ends.

**Almost always**: the contactor is a silent / AC-DC / energy-efficient
type not compatible with this firmware's coil drive.

| Contactor | Compatible? |
|---|---|
| Iskra IKA432-40 | Yes |
| Hager ESC440 (no "S" suffix) | Yes |
| Chint NC1-4025 | Yes |
| Siemens 3RT20xx-xBBx0 | Yes |
| ABB ESB series | No — silent, won't work |
| Hager ESCxxx**S** (S = silent) | No |
| Any "AC-DC" branded coil | No |

Welded contacts from a compatible contactor that's been run beyond its
life cycles: replace the contactor. Contactor lifetime is finite —
40 A AC-1 contactors are rated for ~100,000 switch cycles. A
solar-cycling install that opens/closes 10× per day reaches that in
~27 years, so most welds are from incompatible coil drive, not wear.

---

## 8. MQTT / Home Assistant entities missing or stale

### Nothing appears in HA

| Check | Fix |
|---|---|
| Web UI MQTT status = Connected | If Disconnected, check broker host/port/creds |
| Topic prefix matches HA's MQTT integration filter | Match prefixes exactly |
| HA's MQTT integration is enabled | Settings → Integrations → MQTT → present and green |
| Broker ACL permits the client | Check broker logs |

### Connection drops repeatedly

| Cause | Fix |
|---|---|
| TLS enabled but CA cert wrong | Upload correct CA PEM or disable TLS for testing |
| Client ID conflict (two devices with same serial) | Shouldn't happen, but check broker |
| Complex password with special chars | Known issue — some brokers mishandle. Try a simple password first, work up |
| Broker rate-limits or kicks idle clients | Reduce `Heartbeat` interval; enable Change-Only |

### Entities appear but never update

| Cause | Fix |
|---|---|
| `Change-Only` on, broker dropped retained message | Toggle a value to force republish |
| HA discovery prefix mismatch | `homeassistant/` is the HA default; make sure the charger uses it |

See [mqtt-home-assistant.md](mqtt-home-assistant.md) for the full topic
layout.

---

## 9. OCPP backend stays Disconnected

### WebSocket never connects

| Check | Cause |
|---|---|
| Backend URL correct | Path matters — some providers need `/ocpp/<cbid>` suffix |
| Charge Box Id matches what provider issued | Copy-paste from provider portal |
| `auth_key` set (if SP2 auth) | Verify via `auth_key_set: true` in /settings |
| Firmware supports `wss://` | Yes, TLS WebSocket supported — check CA cert if it's self-signed |
| Network firewall blocks outbound 443 | Test from a LAN device: `curl -v wss://backend/...` |

### Connects but then disconnects after BootNotification

Backend rejected the boot. Check backend logs. Common causes:

- Unknown charge point ID (not provisioned on backend side)
- Firmware version reported doesn't meet backend's minimum
- Backend's OCPP configuration requires capabilities this firmware doesn't advertise

### Works for a few minutes then drops

Keepalive / heartbeat misconfigured. Backend's heartbeat interval
should match the charger's. Typically 300 s. A value of 60 s or less
on a bad network will trip every network hiccup.

---

## 10. Firmware upload fails

### "Unsigned firmware rejected"

Default policy on release builds. Options:

1. Upload `firmware.signed.bin` from the releases page.
2. If running a debug build (`DBG=1`), set an LCD PIN and verify it
   via the Web UI first. Then unsigned `firmware.bin` /
   `firmware.debug.bin` upload is accepted (debug builds only; release
   builds always require signed).

See [security.md](security.md) for the model.

### 411 Length Required

Some POSTs from older clients omit Content-Length. Use a modern
browser or explicitly set `Content-Length: 0` on curl POSTs. Fixed in
all recent Web UI builds.

### Too many connections (9), rejecting new connection

Too many concurrent HTTP connections. Usually your browser opened
multiple tabs, or a client is leaking connections. Close extras. If it
persists, reboot the charger.

### Signature valid but device won't boot new version

Unusual. The partition was written and validated but the new firmware
failed to boot. Device auto-rolls back to the previous good partition.
Check telnet log for the exact failure (stack trace, abort reason).

---

## 11. Capturing a debug log

The device runs a telnet server on port 23 with verbose runtime
logging. Connect with any client:

```
telnet smartevse-<serial>.local
```

Or by IP:

```
telnet 192.168.1.42
```

### What to collect for a bug report

- The telnet output covering the problem's reproduction.
- The Web UI's **settings** JSON: `http://smartevse-<serial>.local/settings`.
  Passwords are redacted.
- Firmware version (LCD or Web UI footer).
- Hardware version (v3 or v3.5 — on the PCB silkscreen).
- What you did, what you expected, what happened.

Paste on the Tweakers thread or as a GitHub issue. Without telnet
output, 80% of bug reports get stuck on "please provide more detail".

### Debug command shortcuts

Once connected to telnet:

| Key | Action |
|---|---|
| `?` or `h` | Help |
| `v` | Verbose log level |
| `d` | Debug log level |
| `i` | Info log level (default) |
| `w` | Warnings only |
| `e` | Errors only |
| `s` | Toggle silence |
| `t` | Show uptime (ms) |
| `m` | Show free heap |
| `q` | Quit |

---

## 12. Web UI locks out with PIN rate-limit

You got the PIN wrong too many times. Server returns 429 with
`Retry-After: <seconds>`. Wait it out.

| Consecutive failures | Cooldown |
|---|---|
| 1–2 | None |
| 3 | 10 s |
| 4 | 60 s |
| 5 | 5 min |
| 6+ | 30 min (cap) |

A successful PIN clears the counter. An idle period > 10 minutes also
clears if no cooldown is active.

If you truly forgot the PIN: set a new one via the LCD menu (physical
access required). The LCD bypasses the rate limit. If you lost both
physical access and the PIN, the only recovery is a full firmware
reflash via USB + factory-reset.

---

## 13. Diagnostics panel shows "Capturing" on every page load

Known-fixed. On recent releases, pressing **Stop** correctly clears the
diag profile and the panel won't auto-resume.

If you're running an older build: reboot the charger once to clear the
in-memory profile state, then update to the latest release.

---

## 14. Phase-switching not working (1P ↔ 3P)

On `EnableC2=AUTO`, the charger should drop to 1-phase when solar drops
below ~4 kW and return to 3-phase when solar exceeds ~4 kW.

| Check | Fix |
|---|---|
| C2 contactor physically present | The second contactor is required for phase switching |
| C2 wired per [guide-installer.md §5](guide-installer.md) | Neutral never switched alone — critical for safety |
| `EnableC2` set to AUTO (not Always Off / Always On) | Via LCD or Web UI |
| Car supports on-the-fly phase switching | **Many don't.** Tesla Model 3 MY2019 has documented issues; some Ioniqs and Zoes also |
| Solar threshold is actually being crossed | Watch Web UI during a cloudy hour |

Car compatibility is a moving target — the
[Tweakers thread](https://gathering.tweakers.net/forum/list_messages/1648387)
has current per-model reports from other owners.

---

## 15. Display light stays on after charger "off"

Known behaviour. The LCD backlight is separately controlled and may
stay on in some idle states. Not a bug — intentional so the device
appears reachable. If you want it off, set **LCD Backlight** to
"on during charging only" in the settings.

---

## 16. RFID reader not responding

| Check | Fix |
|---|---|
| Reader type set in Web UI | Match the model of your reader |
| Reader wiring correct | Check power + data lines per reader datasheet |
| Card enrolled | Add via Web UI → RFID → Add, swipe within window |
| Access mode not set to "auto" (FreeVend) | In FreeVend any swipe is bypassed — disable FreeVend for strict control |

---

## 17. Nothing above matches

Capture:

1. A telnet log covering the problem (see §11).
2. `GET /settings` JSON output.
3. Firmware version.
4. PCB version (v3 vs v3.5).

File at:

- [GitHub Issues](https://github.com/basmeerman/SmartEVSE-3.5/issues)
- [Tweakers "Zelfbouw Laadpaal" thread](https://gathering.tweakers.net/forum/list_messages/1648387) (Dutch, maintainers read it)

Clear reproduction steps plus the telnet log is what separates a
resolved issue from one that sits open for months.
