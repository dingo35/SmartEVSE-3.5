# Upstream Sync State

Tracks integration status of upstream commits from `dingo35/SmartEVSE-3.5`.

**Last synced to:** `ecd088b` (2026-03-29, 2026-03-29 triage closed)
**Current upstream HEAD:** `790f2a9` (2026-04-10)
**Open pending commits (2026-04-13 window):** 1 (EtherLCD deferred to long-lived branch per user direction) + 3 deferred-to-Plan-07 (UI). 9 integrated, 1 rejected-pre-fix, 1 already-fixed.

Prior sync window (2026-03-29, now closed): 2 integrated (PR #130), 1 rejected,
1 evaluated/deferred (190777f), 1 skipped.

---

## Sync: 2026-04-13 — Triage (13 commits since `ecd088b`)

| # | Hash | Date | Author | Title | Classification | Priority | Fork PR | Notes |
|---|------|------|--------|-------|----------------|----------|---------|-------|
| 1 | `e6110b1` | 2026-03-31 | stegen | Fix cable disconnect not detected when switching to PAUSE (fixes #347) | **Integrated** | **P1** | #133 | Applied in `evse_bridge.cpp` STATE_A/B1 and STATE_C1 paths (2-line fix) |
| 2 | `cdc8f67` | 2026-03-31 | dingo35 | Prevent `[Mains\|EV]Meter.[Im\|Ex]port_active_energy` exported when zero | **Already fixed** | — | — | Fork already has broader `> 0` guards at `esp32.cpp:1257-1300` + Circuit meter. No action. |
| 3 | `b104576` | 2026-03-31 | stegen | `modbus.cpp`: do not advance request loop on broadcast timeouts | **Integrated** | P2 | (P2 bundle) | Verbatim; 1-line guard on `BROADCAST_ADR` |
| 4 | `4e6c06d` | 2026-04-01 | dingo35 | `update2.html`: warning message + layout | **Integrated** | P4 | (P4 batch) | Applied verbatim to fork's update2.html (label clarity, HTTPS-upload warning, button caps) |
| 5 | `543af26` | 2026-04-01 | stegen | EtherLCD support: Ethernet add-on board that replaces the LCD board (#349) | New feature — hardware | P3 | — | **1768 lines**, 11 files, hardware-specific. Evaluate whether fork users want this; substantial review. |
| 6 | `afd72a8` | 2026-04-03 | stegen | OCPP: send Finishing state before Available (fixes #348) | **Integrated** | P2 | (P2 bundle) | Decision extracted to `ocpp_should_report_occupied()` in ocpp_logic.c; 6 unit tests |
| 7 | `74e20c8` | 2026-04-07 | stegen | `main.cpp`: reset ChargeDelay countdown when solar power disappears (master) | **Integrated** | P2 | (P2 bundle) | Ported into pure C `evse_tick_1s()`; 3 unit tests in test_tick_1s.c |
| 8 | `2c015fb` | 2026-04-08 | Juurlink | Improved Raw Settings view: formatted JSON + Download button (#353) | **Evaluated — defer to Plan 07** | P3 | — | 280/25 lines lands in fork's 500-line diverged `index.html`; stylesheet rename collision (`styling.css` vs fork `style.css`); `packfs.py` path differs. No functional regression from keeping existing Raw Data link. See [analysis](analysis-2c015fb-raw-settings-ui.md). |
| 9 | `3ab1cee` | 2026-04-08 | stegen | `main.cpp`: reset Node ChargeDelay countdown when solar power disappears | **Integrated** | P2 | (P2 bundle) | Applied in `processAllNodeStates()` master-side slave-node error tracking |
| 10 | `a54b07f` | 2026-04-09 | stegen | `main.cpp`: prevent current fluctuations when CAPACITY is used (fixes #327) | **Integrated** | P2 | (P2 bundle) | Applied in pure C `evse_calc_balanced_current()`; 3 unit tests. `test_s9_maxsummains_limits` updated to use larger exceedance (Isum 350→600) so per-phase reduction crosses fork's SmartDeadBand — documents the gentler, correct per-phase semantics. |
| 11 | `92d42eb` | 2026-04-10 | Juurlink | Refactor tooltips: centralize styles in `styling.css` + a11y (#301) | **Evaluated — defer to Plan 07** | P4 | — | Same `styling.css` vs fork's `style.css` naming collision as #8 (2c015fb). Tooltip refactor + a11y improvements are nice-to-have; fold into Plan 07 Web UI Modernization when executed. No functional regression. |
| 12 | `3679fe3` | 2026-04-10 | stegen | OCPP: public charging station LED colour scheme when OCPP is enabled (#351) | **Integrated** | P3 | (this PR) | Public scheme extracted to `led_public_compute()` in `led_color.c`; 14 unit tests. `MENU_LEDMODE=51` (fork avoids renumber cascade). |
| 13 | `790f2a9` | 2026-04-10 | stegen | `docs`: update OCPP documentation | **Integrated (adapted)** | P4 | (P4 batch) | Applied the restructured OCPP section to `docs/configuration.md`. Omitted the "Remote firmware updates over OCPP" claim — that's the 190777f feature which is deferred in the fork. |

### Suggested batching

1. **Safety / P1 first:**
   - #1 `e6110b1` cable-disconnect-on-PAUSE — needs fork state-machine analysis
2. **Bug-fix bundle (P2) — all solar/load-balancing/OCPP stability:**
   - #3 `b104576` Modbus broadcast timeout
   - #6 `afd72a8` OCPP Finishing→Available sequence
   - #7 + #9 `74e20c8` + `3ab1cee` ChargeDelay reset (bundled, both sides)
   - #10 `a54b07f` CAPACITY current fluctuation — review for overlap with Plan 13
3. **Features (P3) — each as separate PR:**
   - #12 `3679fe3` OCPP LED scheme — adapt into `led_color.c`
   - #8 `2c015fb` Raw Settings UI — **evaluated and deferred** to Plan 07 (Web UI Modernization); see analysis
   - #5 `543af26` EtherLCD — biggest item, stand-alone evaluation
4. **Cosmetic / docs (P4) — processed:**
   - #4 `4e6c06d` update2.html — **integrated**
   - #11 `92d42eb` tooltip CSS — **deferred to Plan 07** (styling.css name collision)
   - #13 `790f2a9` OCPP docs update — **integrated (adapted)**
5. **No action (already fixed):**
   - #2 `cdc8f67` energy zero-value guard — noted, no action

---

## Sync: 2026-03-29 — Triage (CLOSED)

| # | Hash | Date | Author | Title | Classification | Priority | Fork PR | Notes |
|---|------|------|--------|-------|---------------|----------|---------|-------|
| 1 | `ecd088b` | 2026-03-29 | stegen | OCPP: recover from silent session loss (#345) | **Integrated** | P2 | #130 | Logic extracted to `ocpp_silence_decide()` in ocpp_logic.c, 10 unit tests |
| 2 | `05c7fc2` | 2026-03-27 | stegen | OCPP: prevent actuator unlock/relock jitter | **Integrated** | P2 | #130 | Logic extracted to `ocpp_should_force_lock()` in ocpp_logic.c, 11 unit tests |
| 3 | `02dafa2` | 2026-03-27 | stegen | Fix: Solar 1P stop timer | **Rejected** | P1 | #119 (alt) | Same bug as our PR #119; upstream's fix is incorrect — see analysis |
| 4 | `190777f` | 2026-03-25 | stegen | Add OCPP firmware update functionality | **Evaluated — adopt later** | P3 | — | Multi-key compatible (validation path unchanged); deferred to separate PR — see [analysis](analysis-190777f-ocpp-firmware-update.md) |
| 5 | `c0c6b16` | 2026-02-25 | hmmbob | Improve integrations section (#334) | Docs only | P4 | — | ESPHome configs, no firmware |

---

## Commit Analyses

### #3: `02dafa2` — Fix: Solar 1P stop timer (CONFLICTS WITH FORK — REJECTED)

**Summary:** Upstream fixed the same SolarStopTimer threshold bug that our PR #119
addressed, but with a different (and incorrect) approach.

**Decision:** **Reject upstream change.** Keep PR #119. Documented as a conscious
divergence in `docs/upstream-differences.md`.

**Full analysis:** [analysis-02dafa2-solar-stop-threshold.md](analysis-02dafa2-solar-stop-threshold.md).

**One-line rationale:** Upstream removed `Nr_Of_Phases_Charging` but kept
`ActiveEVSE`. Our fix removed `ActiveEVSE` but kept `Nr_Of_Phases_Charging`.
Working from the EVSE's actual perspective (it only sees `Isum` from the mains
meter, not house/solar separately), and tracing the code path through phase
switching, the upstream formula:

- Reproduces the original `ActiveEVSE` scaling bug for multi-node setups (timer
  threshold grows with node count and becomes unreachable)
- Causes stop/start cycling for fixed 3-phase configurations (`EnableC2 != AUTO`),
  because the threshold becomes 2A when the actual single-EVSE 3-phase draw is 18A

Our formula adapts correctly via `Nr_Of_Phases_Charging` (which is set by the
phase-switch logic that runs *before* SolarStopTimer fires) and is constant
regardless of node count.

**Sub-change in same upstream commit:** `static uint8_t Broadcast = 1` → `= 4` in
`timer1s_modbus_broadcast()`. Delays the first Modbus broadcast from ~1s to ~4s
after boot (one-shot init delay). Low value, low risk. Tracked as **P4** —
evaluate independently if/when we touch Modbus init timing.

### #1 + #2: `ecd088b` + `05c7fc2` — OCPP resilience (INTEGRATED)

**Bundled** as one fork PR. Both touch only `esp32.cpp` (firmware glue) plus
one line in `main.cpp` (global declaration).

**#1 — `ecd088b` — Silent session loss recovery**
The MicroOcpp WebSocket layer keeps the transport alive with ping/pong frames,
but those don't prove the OCPP backend is still processing application
messages. Upstream's fix sends periodic Heartbeat probes and forces a WebSocket
reconnect when the backend stays silent past a timeout. In the fork, the timing
decision was extracted into `ocpp_silence_decide()` (pure C in `ocpp_logic.c`)
so the (now/last_response/last_probe → action) mapping can be unit-tested
without millis() or MicroOcpp. The glue layer in `ocppLoop()` calls the pure
function and dispatches `sendRequest("Heartbeat")` / `reloadConfigs()`.

  - 10 unit tests in `test_ocpp_resilience.c` (REQ-OCPP-100..104)
  - Constants `OCPP_PROBE_INTERVAL_MS = 90000` and `OCPP_SILENCE_TIMEOUT_MS = 300000`
    match upstream
  - Cold-boot guard: `last_response_ms == 0` cannot trigger reconnect
  - Reconnect priority over probe verified by test

**#2 — `05c7fc2` — Actuator unlock/relock jitter**
Upstream bug: `OcppForcesLock` was reset to false unconditionally and then
conditionally set to true within the same `ocppLoop()` iteration. The actuator
dispatcher could sample mid-flip and translate the brief false→true into rapid
unlock/relock cycling. The fix is to compute the lock decision once and assign
once. In the fork, the decision is now `ocpp_should_force_lock()` (pure C);
the glue layer assigns the result in a single statement, achieving the same
atomicity and gaining exhaustive unit-test coverage.

  - 11 unit tests in `test_ocpp_connector.c` (REQ-OCPP-110..113)
  - Boundary tests for `PILOT_3V` and `PILOT_9V`
  - Tests for both lock conditions independently and combined
  - All-false baseline asserted

**Verification:** Full 5-step pre-push pipeline (native tests, ASan+UBSan,
cppcheck, ESP32 release build, CH32 build) all green. Traceability spec
regenerated.

### #4: `190777f` — Add OCPP firmware update functionality (EVALUATED — DEFER)

**Multi-key compatibility:** ✅ **fully compatible, no adaptation needed.**
Upstream calls `forceUpdate(url, /*validate=*/true)` which the fork already
routes through `validate_sig()` — and PR #125 made `validate_sig()` a
multi-key loop. Both upstream-signed and fork-signed firmware can be pushed
via OCPP without code changes.

**Other concerns** (not blockers, but should be addressed when integrating):

1. **Stack budget** — upstream creates a 4096-byte FreeRTOS task; CLAUDE.md
   requires a memory-budget check + rationale for new task creation.
2. **`OcppFwStatus` race** — written by download task, read by OCPP loop
   task; uses `volatile int` only. Acceptable on Xtensa but worth wrapping
   for consistency.
3. **`shouldReboot` reuse** — the install-status callback reads `shouldReboot`
   which is also set by web-UI updates and other paths; could report
   `Installed` for the OCPP transaction even when the OCPP install never ran.
4. **Concurrent update guard** — races with web-UI update path on
   `downloadProgress`.

**Decision:** Defer to a separate P3 PR. Needs on-device CSMS verification
(push fork-signed, upstream-signed, unsigned, and corrupted firmware).

**Full analysis:** [analysis-190777f-ocpp-firmware-update.md](analysis-190777f-ocpp-firmware-update.md).

### #5: `c0c6b16` — Improve integrations section

**Summary:** Adds ESPHome YAML configurations for various smart meter modules.
Documentation only, no firmware changes.

**Action:** Skip — integrations/ directory is not fork-specific.

---

## Next Actions

1. [x] **#3 (Solar 1P):** Rejected. Documented as conscious divergence in
        `upstream-differences.md`. `Broadcast = 4` sub-change deferred (P4).
2. [x] **#1 + #2 (OCPP resilience):** Integrated as bundled fork PR with
        pure C extraction and 21 unit tests (PR #130, merged).
3. [x] **#4 (OCPP FW update):** Evaluated — multi-key compatible. Deferred to
        separate P3 PR (needs on-device CSMS verification + memory budget review).
4. [x] **#5 (Docs):** Skipped — ESPHome integrations dir not fork-specific.
