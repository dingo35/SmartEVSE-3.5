# Load Balancing Stability

This page documents the multi-node load balancing stability improvements added in
this fork. These changes address current oscillation, uneven distribution, and
diagnostic visibility when multiple SmartEVSEs share one mains connection.

Upstream issue addressed:
[#316](https://github.com/dingo35/SmartEVSE-3.5/issues/316) (oscillation in smart
mode power sharing)

## Overview

The upstream load balancing algorithm (`CalcBalancedCurrent`) recalculates current
distribution from scratch every ~2 seconds. It uses asymmetric gain (fast decrease,
slow increase) and has no awareness of measurement noise, control loop dynamics, or
EV charge controller response lag. In multi-node setups (2-8 EVSEs), this causes:

- **Current oscillation** — allocations swing up and down between cycles
- **Uneven distribution** — some EVSEs get more than their fair share
- **Contactor stress** — sudden current jumps stress relays and EV electronics
- **False shortage detection** — measurement noise triggers unnecessary throttling

This fork adds oscillation detection, measurement filtering, per-EVSE rate limiting,
and a diagnostic snapshot for monitoring.

## New features

### Oscillation dampening

Detects when the regulation direction flips between consecutive cycles (sign-flip
on `Idifference`). When oscillation is detected:

- `OscillationCount` increments (max 10)
- The regulation divisor is boosted: `effective_divisor = RampRateDivisor + OscillationCount`
- Result: regulation slows down during oscillation, preventing current hunting
- When stable (no sign flip), `OscillationCount` decrements back toward zero

This is adaptive — it only slows regulation when oscillation is actually detected,
preserving fast response during normal operation.

### EMA filter on Idifference

An exponential moving average filter smooths the raw `Idifference` measurement
before it is used for regulation:

```
IdiffFiltered = (IdiffFiltered * 3 + Idifference) / 4
```

With 25% alpha (3-cycle decay), this dampens spikes from CT measurement noise
and transient loads without significantly delaying convergence on sustained changes.

**Important:** The raw (unfiltered) `Idifference` is still used for:
- Oscillation detection (needs to see actual sign flips)
- Solar safety decrease (must react immediately to grid overload)

### Distribution smoothing (delta clamping)

After the algorithm distributes current across EVSEs, each EVSE's allocation change
is clamped to a maximum of ±3.0A per cycle:

```
MAX_DELTA_PER_CYCLE = 30 deciamps (3.0A)
```

This prevents:
- Contactor relay stress from sudden current jumps
- EV charge controller destabilization (some EVs stall on large steps)
- Cascading oscillation where one EVSE's jump causes another's correction

**Exception:** When a new EVSE joins (`mod=1`), clamping is bypassed to allow
full redistribution.

### Load balancing diagnostic snapshot

Every regulation cycle populates a diagnostic struct (`evse_lb_diag_t`) with:

| Field | Description |
|-------|-------------|
| `IsetBalanced` | Target current after regulation |
| `Idifference_raw` | Raw measurement before filtering |
| `Idifference_filtered` | After EMA filter |
| `Baseload_EV` | Non-EVSE load on EV circuit |
| `Baseload_Other` | Non-EV load on mains |
| `Balanced[n]` | Per-EVSE allocated current |
| `BalancedMax[n]` | Per-EVSE maximum capacity |
| `ScheduleState[n]` | Per-EVSE priority schedule state |
| `ActiveEVSE` | Number of actively charging EVSEs |
| `OscillationCount` | Current oscillation detection counter |
| `NoCurrent` | Shortage tick counter |
| `Shortage` | Boolean: system in shortage |
| `PriorityScheduled` | Boolean: priority rotation active |
| `DeltaClamped` | Boolean: distribution smoothing was applied |

This snapshot is designed for MQTT publishing (deferred to network layer) to
enable real-time monitoring of load balancing behavior.

## How it works together

The three features operate in sequence during each regulation cycle:

```
1. Read mains current measurements
2. Calculate Idifference (mains headroom)
3. Apply EMA filter → IdiffFiltered
4. Detect oscillation (on raw Idifference)
5. Adjust regulation divisor (boost if oscillating)
6. Calculate new IsetBalanced using filtered value and adjusted divisor
7. Distribute across EVSEs (existing algorithm)
8. Apply delta clamping per EVSE (±3A max change)
9. Populate diagnostic snapshot
10. Broadcast to nodes
```

## Settings

These features use the existing `RampRateDivisor` and `EmaAlpha` settings from
the solar/smart mode stability improvements. The oscillation dampening and delta
clamping are always active in Smart and Solar modes with no additional configuration.

| Setting | Effect on load balancing |
|---------|------------------------|
| `RampRateDivisor` | Base regulation speed. Oscillation dampening adds to this value adaptively |
| `EmaAlpha` | Controls the single-measurement EMA. The load balancing Idifference EMA uses a fixed 25% alpha (3/4 weight on previous) |

### Settings that do NOT apply

- Oscillation dampening and EMA filtering are **disabled in Normal mode** —
  Normal mode needs deterministic, immediate behavior
- Delta clamping is **bypassed when a new EVSE joins** (`mod=1`) to allow
  immediate redistribution

## Test coverage

The load balancing improvements are verified by 126 convergence tests covering:

| Category | Tests | Scenarios |
|----------|-------|-----------|
| Single/multi-EVSE convergence | 18 | Smart, Solar, Normal modes; 1-4 EVSEs |
| Capacity limits | 6 | MaxMains, MaxCircuit, MaxSumMains constraints |
| Priority scheduling | 4 | Shortage rotation, fair scheduling |
| Oscillation dampening | 5 | Detection, adaptive gain, decay, noisy loads |
| EMA filter | 4 | Spike dampening, convergence, noise reduction |
| Distribution smoothing | 5 | Increase/decrease clamping, mod=1 bypass |
| Diagnostic snapshot | 4 | Struct population, flags, per-EVSE tracking |
| 8-node configurations | 6 | Full cluster fairness, sequential join/leave |
| Vehicle response model | 4 | 2-cycle delay, 50% ramp, measurement noise |
| Node transitions | 6 | Join, leave, priority changes |

Tests use a `simulate_n_cycles()` helper that feeds back realistic meter
measurements between regulation cycles, simulating real-world control loop dynamics.

## Troubleshooting

| Problem | Indicator | Solution |
|---------|-----------|---------|
| Current oscillates between EVSEs | `OscillationCount` stays high (>5) | Increase `RampRateDivisor` to 6-8 for more aggressive base damping |
| One EVSE gets more than others | Check `Balanced[n]` in diagnostic snapshot | Verify all EVSEs report correct `BalancedMax` and are in STATE_C |
| Slow convergence after EVSE joins | Expected: clamping bypassed for join, but re-enabled immediately after | Wait 4-6 cycles (~10s) for system to stabilize |
| Shortage detected unexpectedly | `NoCurrent` incrementing, `Shortage` flag set | Check if `NoCurrentThreshold` is too low; verify mains meter readings |
| Current jumps on specific EVSE | Delta clamping should prevent >3A jumps | If jumps >3A occur, check that EVSE is not repeatedly joining/leaving (mod=1 bypass) |
