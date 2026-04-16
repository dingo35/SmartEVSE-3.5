# Documentation audience analysis and restructure proposal

**Date:** 2026-04-15
**Status:** Decisions locked — execution starts with `guide-installer.md`

## Decisions (locked 2026-04-15)

| # | Question | Decision |
|---|---|---|
| Q1 | Segment sizing | B (owner) is largest real-world audience. A2 (installer) docs must be markedly better than upstream. Execution order: installer → owner → integrator → contributor → cross-cutting troubleshooting |
| Q2 | Fork identity | (b) production-hardened drop-in SmartEVSE, neutral/factual tone, AI-methodology mentioned in footer only |
| Q3 | Upstream portability | (a) fork-only, no upstream portability tax. Writing style: like a maintainer typing fast, not like polished AI |
| Q4 | Dutch translation | (a) English only + prominent Tweakers link in README footer. Dutch electrical terms as parentheticals. |
| Q5 | Car compatibility page | (b) skeleton + community-maintained with staleness guardrails |

**Writing-style guardrails** (applies to every new doc):

- No bullet lists of 5+ items without reason.
- No meta-paragraphs ("Bottom line:", "In summary:").
- Short declarative sentences.
- Tables > prose for reference material.
- Code examples > conceptual explanations.
- No em-dashes splitting 3+ clauses.
- Fork-specific features referenced freely where they add value; no caveats
  for upstream users.


**Sources consulted:**

- `docs/` current contents (27 markdown files + 3 security docs)
- Root `README.md` (fork) and upstream `dingo35/SmartEVSE-3.5` README
- Fork issue tracker (basmeerman/SmartEVSE-3.5) — 40 most recent
- Upstream issue tracker (dingo35/SmartEVSE-3.5) — 60 most recent
- Tweakers.net GoT forum thread *"Zelfbouw Laadpaal ervaringen"* — the canonical
  Dutch community thread (2017–2025, 92+ pages, ~1M views). Fetched via the
  Wayback Machine snapshot (the site requires consent-gate JS).

---

## 1. User segments observed

Six distinct segments show up repeatedly across the three sources. They overlap
but have very different first-five-minutes needs. Percentages are rough
eyeball estimates based on post/issue volume, not scientific.

### Segment A — The DIY hardware builder (~30%)

Dominant voice on Tweakers. Buys the PCB kit (or components), solders it,
designs the meter-cabinet layout themselves, runs the CP/PE cable, picks a
contactor, selects an RCD (type A vs B debate), picks cable (SFTP CAT6/7 for
EMC). Often cross-posts with photos of their Dutch/Belgian `meterkast`.

**What they land on the repo looking for:**

- A build-of-materials list (PCB, contactor, RCBO, fixed or socket, cable type).
- Hardware installation with wiring diagrams for 1-phase and 3-phase.
- Which meter to buy and how to wire it (Sensorbox vs direct Modbus vs P1).
- How to flash the firmware the first time (USB-C).
- How to commission: set mode, set MaxMains, verify CP signal on scope.

**What they do NOT want on the landing page:**
Architectural notes, pure-C test philosophy, Claude Code framing.

**Dutch-specific concerns:**
DSMR4 vs DSMR5 timing. P1 poort wiring. Salderingsregeling phase-out.
Capacity tariff (Belgian + rolling out NL). 3×25 A standard connection.

### Segment B — The installed-kit owner (~25%)

Has the device mounted and powered. Car plugs in and charges in Normal mode.
Now wants to move to Smart or Solar mode, or hook up Home Assistant, or fix
the one thing that's weird about their specific EV.

**Landing needs:**

- Day-to-day operation: modes, RFID usage, LCD menu, web dashboard.
- Mode tuning: Solar thresholds, 1P/3P switching, ImportCurrent.
- "My car won't start charging" symptom-based troubleshooting.
- Car-compatibility notes (Zoe 2019, MG4, eGolf, Ioniq, Tesla, etc.).

**Complaint pattern in issues:**
"After I changed X, Y no longer works." Usually a configuration issue, not a
bug — but the docs don't guide them back to the answer.

### Segment C — The Home Assistant integrator (~20%)

Forum posts like "ik gebruik ser2net om P1 naar HA te sturen en MQTT
terug naar SmartEVSE" (from 2025-12). Often has Tibber / EnergyZero /
ANWB dynamic pricing, wants the charger to react.

**Landing needs:**

- MQTT topic reference (publish + subscribe).
- HA auto-discovery entity list.
- REST API (`/settings`, `/currents`, `/mode`) — what to POST, when.
- `EM_API` flow: how to feed mains measurements from HA into the charger.
- Rate/timing expectations (the forum shows confusion: "every 1 s required?"
  — answer is "2–10 s").

**Recurring pain:**
Timeout handling, DSMR4 5–10 s telegram cadence vs charger's 10 s API
timeout, HA poll interval defaulting to 60 s. Entity naming / discovery IDs.
OCPP vs internal load balancing conflict.

### Segment D — The multi-charger / load-balancing operator (~10%)

Households with two EVs, small businesses, apartment buildings. One mains
connection, two or more SmartEVSE modules, RFID access. Often adds an OCPP
backend (Tibber, E-Flux, Tap Electric) for billing.

**Landing needs:**

- Master/slave wiring + `LoadBl` setting explained without Modbus jargon.
- When `LoadBl=0` (standalone) vs `LoadBl=1` (master) vs `LoadBl≥2` (slave).
- Priority scheduling. Rotation interval. Idle timeout.
- OCPP provider quickstart (one page per provider would be ideal).
- Subpanel metering (CircuitMeter, MaxCircuit) for shared branch circuits.

**Upstream issues seen:**
`#317` master+slave connection error after update; `#316` oscillation in
smartmode with a few chargers; `#303` incorrect "Smart 1P" during 3-phase;
`#312` modbus 1P/3P control.

### Segment E — The troubleshooter (across all segments)

Not a distinct persona but a *mode* every other user drops into. The device
is misbehaving. They need a decision tree, not a reference manual.

**Landing needs:**

- "Car won't start charging" — diode check, PP resistance, CP PWM, diode fail
  flag, state machine trace.
- "Solar mode oscillates" — dead band, EMA smoothing, SolarStopTimer.
- "No MQTT discovery" — broker status, topic prefix, HA config.
- "Wrong amps reported" — meter selection, CT orientation, phase alignment.
- "Firmware won't upload" — signed vs unsigned, LCD PIN gate, 411 Content-Length.
- The telnet debug log as a first-class diagnostic surface.

### Segment F — The contributor / forker (~10%, mostly fork-side)

This fork's primary audience historically. Upstream contributors are a
smaller slice (dingo35 maintainer + a handful of PR authors).

**Landing needs:**

- Architecture: pure C modules, bridge, state machine, glue layers.
- Testing: native tests, sanitizers, cppcheck, SbE workflow.
- CONTRIBUTING: branch style, commit style, PR review, quality gate.
- CLAUDE.md for AI-agent contributors (already exists).
- Upstream sync process — how to pull from dingo35 safely.

### Segment-adjacent — Specialist roles

**Security-conscious operator.** Emerging from recent work: AuthMode,
rate-limit, signed firmware, Origin/CSRF check. Separate page worth writing.

**EVCC user.** Small, growing. Single-page integration guide already exists
(`docs/evcc-integration.md`) — keep it.

---

## 2. What's wrong with the current entry point

### The fork `README.md` buries the lede

The opening section is about the fork being a *"testbed for IT/OT software
engineering with AI-assisted development"* with a 1,200-test count. That is
correct and load-bearing for contributors (Segment F), but it is the **third**
thing a DIY builder or installer owner wants to read, not the first. Segments
A/B/C/D bounce on this framing.

The Features table is a kitchen-sink matrix: Charging, Solar, Load Balancing,
OCPP, MQTT, Metering, EVCC, Diagnostics, ERE Logging, Capacity Tariff,
CircuitMeter, SoC Injection, Web UI, Privacy — 14 rows. Everything the fork
can do, none of it ordered by what anyone specifically needs first.

"Getting started" == "connect WiFi + flash firmware". A DIY builder has
not soldered the PCB yet.

### The `docs/` folder is organized by topic, not by audience

Current files:
`building_flashing.md`, `configuration-matrix.md`, `configuration.md`,
`ere-session-logging.md`, `evcc-integration.md`, `features.md`,
`installation.md`, `load-balancing-stability.md`, `manual-test-plan.md`,
`mqtt-home-assistant.md`, `ocpp.md`, `operation.md`, `power-input-methods.md`,
`priority-scheduling.md`, `quality.md`, `REST_API.md`, `solar-smart-stability.md`,
`upstream-differences.md`, plus `security/` and `upstream-sync/` subdirs.

This is a *complete* reference but has no on-ramp. A new Tweakers reader
lands on the README, clicks "Configuration", and gets a 500-line LCD menu
reference before learning whether to pick Normal / Smart / Solar mode for
their situation.

### No troubleshooting index

Issues #335 (Zoe solar), #346 (MG4), #306 (smart oscillation), #303 (phase
label wrong), #301 (MG4 3-phase on startup) — all individual reports with
individual resolutions. No `docs/troubleshooting.md` that groups them by
symptom for the next user who hits the same thing.

### The Tweakers thread isn't linked anywhere

That thread has **a million views** since 2017, a decade of accumulated
community wisdom, and the upstream maintainer answers questions in it. The
repo doesn't point to it. New users who find the forum but not the repo
end up with half the picture.

---

## 3. Proposed restructure

Two-layer approach: a **segmented landing page** that routes users to the
right next click, and a **by-audience docs hierarchy** behind it.

### Proposed README.md structure (landing page)

```
SmartEVSE v3 (basmeerman fork)

<hero image + one-paragraph product description, no architecture talk>

Pick your path:
  - I want to BUILD one   → docs/guide-builder.md
  - I OWN one and want to use it → docs/guide-owner.md
  - I'm integrating it with Home Assistant / OCPP → docs/guide-integrator.md
  - Something is WRONG → docs/troubleshooting.md
  - I want to CONTRIBUTE → docs/guide-contributor.md

Community:
  - Dutch forum: Tweakers GoT "Zelfbouw Laadpaal ervaringen"
  - Upstream: dingo35/SmartEVSE-3.5
  - Issues on this fork: ...

About this fork: (1-paragraph, honest — derived work, security hardening,
testable architecture, 1200+ tests. Link to docs/quality.md and
docs/upstream-differences.md for details.)

Quick facts: 1-3φ, up to 8 nodes, MQTT+REST+OCPP 1.6j, DIN 3-module,
110–240 VAC. (What the upstream README opens with — claim the spec.)
```

Total ≤ 100 lines. The current 138-line README isn't far off in length
but is 70% mis-targeted at contributors.

### Proposed `docs/` audience-guide pages (new)

Five new landing pages, each ≤ 200 lines, each linking into the existing
topic-organized docs:

| New file | Audience | Covers |
|---|---|---|
| `docs/guide-builder.md` | Segment A | BOM, wiring, contactor selection, meter choice, first-flash, commissioning, Sensorbox vs Modbus vs P1 |
| `docs/guide-owner.md` | Segment B | Modes explained, LCD menu tour, web UI tour, RFID usage, day-to-day |
| `docs/guide-integrator.md` | Segment C+D | MQTT topics, HA discovery, REST endpoints, EM_API flow, OCPP providers, multi-charger setup |
| `docs/guide-contributor.md` | Segment F | Architecture, build, test workflow, SbE, CLAUDE.md link, branch/PR style |
| `docs/troubleshooting.md` | All | Symptom-indexed: "car won't start charging", "solar oscillates", "MQTT offline", "firmware won't upload", "PIN auth blocks me", with per-symptom triage steps and links to the underlying topic doc |

The existing topic docs (`configuration.md`, `ocpp.md`, `mqtt-home-assistant.md`,
etc.) stay as deep-reference. The new guides do the curation.

### Minor additions

- **`docs/security.md`** — promote the work done this session (AuthMode,
  signed firmware, LCD-PIN rate limit, CSRF check, session timeout) into a
  single operator-facing page. Currently that knowledge is scattered across
  `security/plan-16-http-auth-layer.md` (design doc), `security/security-review-summary-2026-04.md`
  (internal summary), and PRs #146/#151/#152/#153/#155/#156.

- **`docs/car-compatibility.md`** — a table of observed interop notes by
  EV model (Zoe, MG4, Ioniq, Tesla, eGolf). Currently scattered across
  issues #335/#346/#335/#315/#301 and forum posts.

- **Tweakers / community link** — prominent in README footer. One sentence
  acknowledging the forum as the day-to-day support channel for Dutch users.

### What NOT to change

- CLAUDE.md stays. It's for AI agents, and it's load-bearing for the fork's
  contribution workflow.
- The topic-organized docs stay. They're the reference material the guides
  link into. Renaming or moving them would break external links.
- `docs/upstream-sync/` and `docs/security/` stay as internal process docs.

---

## 4. Audience-to-content mapping (current state)

Read as: *"A [segment] user who lands on the README and wants [X], what do
they actually click through?"*

| Segment | First need | Current path | Friction |
|---|---|---|---|
| A Builder | BOM + wiring | README → Hardware installation | Installation.md is good but README doesn't flag it as the first stop for builders |
| A Builder | First firmware flash | README → Building and flashing | Flashing.md assumes PlatformIO fluency |
| B Owner | "which mode should I pick" | README → Operation → Configuration | Has to read two docs to get a clear mode recommendation |
| B Owner | LCD menu reference | README → Configuration matrix | Configuration-matrix.md is dense; no "here are the 5 settings that matter" shortcut |
| C Integrator | MQTT topic list | README → MQTT & Home Assistant | docs/mqtt-home-assistant.md is comprehensive — this one works |
| C Integrator | REST API | README → REST_API.md | Works |
| C Integrator | "feed mains from HA" | No clear path | `power-input-methods.md` buries EM_API; not cross-linked from MQTT doc |
| D Multi-charger | Master/slave setup | README → Load balancing → LoadBl variable mentions | The actual "how to wire and configure a slave" topic is split across installation + configuration |
| D OCPP user | Provider-specific quickstart | README → OCPP | `ocpp.md` is feature-oriented, not provider-oriented |
| E Troubleshoot | "Zoe won't solar charge" | No docs path; fall back to GitHub issue search | High friction |
| F Contributor | Build + test | README → Quality Engineering | Works well |
| F Contributor | Commit + PR style | README → CONTRIBUTING.md | Works |

The gaps (rows marked "friction") are the ones the new guide pages would
close — not by writing new reference material (most of it exists) but by
**routing** from audience-named starting points into the right topic docs.

---

## 5. Open questions before I start writing

1. **Confirm segment sizing.** Is Segment A (hardware builder) really the
   biggest audience you serve? Fork usage telemetry would help, but absent
   that, your gut as the maintainer is the tie-breaker.
2. **Fork identity.** The current README leads with "testbed for AI-assisted
   development". Is that still the primary positioning you want, or should
   the fork present as "a drop-in compatible, security-hardened, better-tested
   SmartEVSE" with the AI-engineering angle as a secondary story? The
   restructure proposal assumes the latter — please confirm.
3. **Upstream coordination.** Some of this (`guide-builder.md`,
   `troubleshooting.md`) could be contributed upstream in a way dingo35 would
   accept. Interested in doing that, or is fork-only fine?
4. **Dutch-language version.** Tweakers is Dutch. A NL translation of the
   builder and troubleshooting guides would broaden reach substantially.
   Scope expansion — flag for later, not blocker.
5. **Car compatibility page.** Useful but requires curation (a living
   document). Willing to maintain, or skip and let forum do it?

---

## 6. Suggested execution order

If you want to act on this:

1. **Write the new README.md** (replace the hero section with a segmented
   landing; move the AI-engineering paragraph lower; keep the specs table).
2. **Write `docs/troubleshooting.md`** — biggest bang-for-buck, closes the
   highest-friction gap, reuses content you already have in PR descriptions.
3. **Write `docs/guide-integrator.md`** — consolidates MQTT/REST/OCPP into
   one audience path, probably reused most by HA users who make up a lot of
   the issue volume.
4. **Write `docs/guide-builder.md`** — hardest one, needs photos and BOMs.
   Partly you can point at upstream's existing images.
5. **Write `docs/guide-owner.md`** and `docs/guide-contributor.md` —
   largely stitching existing docs with a narrative wrapper.
6. **Add `docs/security.md`** — promote your own recent work to
   operator-facing visibility.
7. **Promote the Tweakers link** — one-line footer addition.

Steps 1 + 2 alone would make the docs feel much less labyrinthine. The rest
is fill-in.
