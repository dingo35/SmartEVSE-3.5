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
| 3 | `02dafa2` | 2026-03-27 | stegen | Fix: Solar 1P stop timer | **Conflicts with fork** | P1 | — | Same bug as our PR #119 but different fix — see analysis below |
| 4 | `190777f` | 2026-03-25 | stegen | Add OCPP firmware update functionality | New feature | P3 | — | esp32.cpp + network_common.h, 62 lines added |
| 5 | `c0c6b16` | 2026-02-25 | hmmbob | Improve integrations section (#334) | Docs only | P4 | — | ESPHome configs, no firmware |

---

## Commit Analyses

### #3: `02dafa2` — Fix: Solar 1P stop timer (CONFLICTS WITH FORK)

**Summary:** Upstream fixed the same SolarStopTimer threshold bug that our PR #119
addressed. Both fixes target the same line but take different approaches.

**Upstream fix:**
```c
// Before (buggy):
Isum > (ActiveEVSE * MinCurrent * Nr_Of_Phases_Charging - StartCurrent) * 10
// After (upstream fix):
Isum > (ActiveEVSE * MinCurrent - StartCurrent) * 10
```
Upstream removed `Nr_Of_Phases_Charging`, reasoning that "Isum and StartCurrent
are both sum-of-phases, so no phase multiplication needed."

**Our fix (PR #119):**
```c
// After (our fix):
Isum > (MinCurrent * Nr_Of_Phases_Charging - StartCurrent) * 10
```
We removed `ActiveEVSE`, reasoning that priority scheduling handles multi-node
distribution and the timer should check single-EVSE viability.

**Comparison of thresholds** (6A min, 3-phase, StartCurrent=4, 2 EVSEs):

| Fix | Formula | Threshold |
|-----|---------|-----------|
| Buggy (original) | (2 * 6 * 3 - 4) * 10 | 320 dA (32A) |
| Upstream | (2 * 6 - 4) * 10 | 80 dA (8A) |
| Our fork | (6 * 3 - 4) * 10 | 140 dA (14A) |

**Analysis:** Both fixes make the threshold reachable. The upstream fix still scales
with `ActiveEVSE` (80 for 2 EVSEs, 200 for 4) while ours is constant (140).

The upstream approach is arguably more aggressive (lower threshold = stops sooner)
but reintroduces the `ActiveEVSE` scaling problem at high node counts. With 8 EVSEs:
upstream = (8*6-4)*10 = 440, ours = 140.

**Decision:** Keep our fix (PR #119). It is more correct for large multi-node setups
and was thoroughly tested (24 tests). Note this as a conscious divergence.

**Additional upstream change:** `static uint8_t Broadcast = 1` → `= 4`. This is a
separate timing fix (broadcast settings more frequently on startup). Should be
evaluated independently.

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

1. [ ] **#3 (Solar 1P):** Mark as conscious divergence in upstream-differences.md.
        Evaluate the `Broadcast = 4` sub-change separately.
2. [ ] **#1 + #2 (OCPP):** Bundle and implement — OCPP Specialist.
3. [ ] **#4 (OCPP FW update):** Evaluate interaction with multi-key validation.
4. [ ] **#5 (Docs):** Skip.
