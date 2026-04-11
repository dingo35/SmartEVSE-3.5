# Upstream Sync State

Tracks integration status of upstream commits from `dingo35/SmartEVSE-3.5`.

**Last synced to:** `40e78a2` (2026-02-25, merged via PR #123 base)
**Current upstream HEAD:** `ecd088b` (2026-03-29)
**Pending commits:** 2 (was 5; #1 + #2 integrated, #3 rejected)

---

## Sync: 2026-03-29 — Triage

| # | Hash | Date | Author | Title | Classification | Priority | Fork PR | Notes |
|---|------|------|--------|-------|---------------|----------|---------|-------|
| 1 | `ecd088b` | 2026-03-29 | stegen | OCPP: recover from silent session loss (#345) | **Integrated** | P2 | (PR pending) | Logic extracted to `ocpp_silence_decide()` in ocpp_logic.c, 10 unit tests |
| 2 | `05c7fc2` | 2026-03-27 | stegen | OCPP: prevent actuator unlock/relock jitter | **Integrated** | P2 | (PR pending) | Logic extracted to `ocpp_should_force_lock()` in ocpp_logic.c, 11 unit tests |
| 3 | `02dafa2` | 2026-03-27 | stegen | Fix: Solar 1P stop timer | **Rejected** | P1 | #119 (alt) | Same bug as our PR #119; upstream's fix is incorrect — see analysis |
| 4 | `190777f` | 2026-03-25 | stegen | Add OCPP firmware update functionality | New feature | P3 | — | esp32.cpp + network_common.h, 62 lines added |
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

### #4: `190777f` — Add OCPP firmware update functionality

**Summary:** Allows OCPP backend to trigger firmware updates via the OCPP
FirmwareUpdate message. Adds download + flash logic triggered by OCPP.

**Files:** esp32.cpp (60 lines), network_common.h (2 lines)
**Fork mapping:** esp32.cpp, may interact with firmware_manager.cpp.
**Risk:** Interacts with OTA update (firmware_manager). Must verify it works
with multi-key validation. Medium risk.
**Action:** Evaluate — may need adaptation for multi-key signing.

### #5: `c0c6b16` — Improve integrations section

**Summary:** Adds ESPHome YAML configurations for various smart meter modules.
Documentation only, no firmware changes.

**Action:** Skip — integrations/ directory is not fork-specific.

---

## Next Actions

1. [x] **#3 (Solar 1P):** Rejected. Documented as conscious divergence in
        `upstream-differences.md`. `Broadcast = 4` sub-change deferred (P4).
2. [x] **#1 + #2 (OCPP resilience):** Integrated as bundled fork PR with
        pure C extraction and 21 unit tests.
3. [ ] **#4 (OCPP FW update):** Evaluate interaction with multi-key validation.
4. [ ] **#5 (Docs):** Skip.
