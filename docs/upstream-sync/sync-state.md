# Upstream Sync State

Tracks integration status of upstream commits from `dingo35/SmartEVSE-3.5`.

**Last synced to:** `40e78a2` (2026-02-25, merged via PR #123 base)
**Current upstream HEAD:** `ecd088b` (2026-03-29)
**Pending commits:** 5

---

## Sync: 2026-03-29 — Triage

| # | Hash | Date | Author | Title | Classification | Priority | Fork PR | Notes |
|---|------|------|--------|-------|---------------|----------|---------|-------|
| 1 | `ecd088b` | 2026-03-29 | stegen | OCPP: recover from silent session loss (#345) | New feature | P2 | — | esp32.cpp + main.cpp, OCPP resilience |
| 2 | `05c7fc2` | 2026-03-27 | stegen | OCPP: prevent actuator unlock/relock jitter | Bug fix — non-safety | P2 | — | esp32.cpp only, actuator control |
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

### #1: `ecd088b` — OCPP: recover from silent session loss

**Summary:** Detects when an OCPP backend silently drops a transaction (no
StopTransaction response) and recovers by ending the local session.

**Files:** esp32.cpp (32 lines), main.cpp (2 lines)
**Fork mapping:** esp32.cpp is firmware glue (same file in fork). The 2 lines in
main.cpp may need to go in the bridge or state machine depending on what they do.
**Risk:** Non-safety, OCPP-only. Low regression risk.
**Action:** Implement — OCPP Specialist role.

### #2: `05c7fc2` — OCPP: prevent actuator unlock/relock jitter

**Summary:** Prevents the cable lock actuator from rapidly cycling when OCPP
authorization state changes during an active charge session.

**Files:** esp32.cpp (5 lines changed)
**Fork mapping:** Same file, firmware glue.
**Risk:** Non-safety but affects physical actuator. Low regression risk.
**Action:** Implement — OCPP Specialist role. Can bundle with #1.

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
2. [ ] **#1 + #2 (OCPP):** Bundle and implement — OCPP Specialist.
3. [ ] **#4 (OCPP FW update):** Evaluate interaction with multi-key validation.
4. [ ] **#5 (Docs):** Skip.
