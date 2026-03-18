# Solar & Smart Mode Stability

This page documents the solar and smart mode stability improvements added in this
fork. These changes address current oscillation, stop/start cycling, slow phase
switching, and compatibility with EVs like the Renault Zoe.

Upstream issues addressed:
[#335](https://github.com/dingo35/SmartEVSE-3.5/issues/335),
[#327](https://github.com/dingo35/SmartEVSE-3.5/issues/327),
[#316](https://github.com/dingo35/SmartEVSE-3.5/issues/316)

## Overview

The upstream solar/smart regulation algorithm uses fixed-step adjustments on a
fast tick (100ms) with no damping, no hysteresis, and no awareness of measurement
latency. This causes oscillation, unnecessary phase switches, stop/start cycling,
and poor behavior with vehicles that respond slowly to current changes.

This fork adds configurable smoothing, dead bands, ramp rate limiting, improved
phase switching timers, and stop/start cycling prevention.

## New settings

All new settings default to backward-compatible values. You do not need to change
anything for existing installations to keep working. Tune these if you experience
instability.

### Current regulation

| Setting | Default | Range | Description |
|---------|---------|-------|-------------|
| `EmaAlpha` | 100 | 0-100 | EMA smoothing weight. 100 = no smoothing (original behavior). Lower = more damping. Recommended: 50-80 for oscillation issues. |
| `SmartDeadBand` | 10 (1.0A) | 0-50 | Dead band for smart mode regulation in 0.1A units. Adjustments smaller than this are suppressed. |
| `RampRateDivisor` | 4 | 1-16 | Symmetric ramp rate for current changes. Higher = slower, smoother changes in both directions. |
| `SolarFineDeadBand` | 5 (0.5A) | 0-20 | Dead band for solar fine regulation in 0.1A units. Upstream was effectively 3 (0.3A). |

### Phase switching timers

| Setting | Default | Range | Description |
|---------|---------|-------|-------------|
| `PhaseSwitchHoldDownTime` | 300s (5 min) | 0-3600 | After switching from 3P to 1P, wait this long before allowing a switch back to 3P. Prevents rapid cycling. |
| `PhaseSwitchSevereTime` | 30s | 10-600 | Timer for severe shortage (import >= MinCurrent * 10). Fast 3P-to-1P switch when heavily overloaded. |

### Stop/start cycling prevention

| Setting | Default | Range | Description |
|---------|---------|-------|-------------|
| `NoCurrentThreshold` | 10 | 1-50 | Number of shortage ticks before triggering LESS_6A error. Upstream was 3 (too sensitive). |
| `SolarChargeDelay` | 15s | 5-60 | Charge delay after solar stop. Upstream was 60s (too long for solar recovery). |
| `SolarMinRunTime` | 60s | 0-600 | Minimum charge time before NoCurrent can trigger LESS_6A. Prevents false stops during startup. |

## How it works

### EMA smoothing

The Exponential Moving Average filter smooths the `IsetBalanced` target current:

```
IsetBalanced_ema = (alpha * IsetBalanced + (100 - alpha) * IsetBalanced_ema) / 100
```

With `EmaAlpha=100`, the filter is a pass-through (original behavior). With
`EmaAlpha=50`, each measurement contributes 50% to the output, damping oscillation
within 2-3 cycles.

### Symmetric ramp rates

Previously, smart mode increased current slowly (`/4`) but decreased instantly
(full step). This asymmetry caused overshoot-undershoot oscillation. Both directions
now use the same `RampRateDivisor`, making regulation predictable.

### Tiered phase switching

Previously, the 3P-to-1P switch used the `SolarStopTimer` (typically 10 minutes).
Now the timers are separate and tiered:

- **Severe shortage** (mains import >= MinCurrent * 10): switch in `PhaseSwitchSevereTime` (30s)
- **Mild shortage** (import > 0 but < severe): use the standard timer
- **Hold-down guard**: after switching 3P→1P, block 1P→3P for `PhaseSwitchHoldDownTime` (5 min)

### NoCurrent decay

Instead of resetting `NoCurrent` to 0 when current returns, it now decrements by 1
each cycle. This means brief solar dips build up gradually and recover gradually,
instead of snapping between "no problem" and "emergency stop."

### Slow EV compatibility (Renault Zoe)

EVs like the Zoe draw 0A for several seconds after a current change command. The
regulation algorithm now includes:

- **Settling window**: after a current change, suppress regulation adjustments
  for a configurable period
- **Ramp rate limiter**: cap the maximum change per cycle to prevent large steps
  that trigger slow-EV stalls

## Configuring via MQTT

All settings can be changed via MQTT `Set` topics:

```
SmartEVSE/<serial>/Set/EmaAlpha         50
SmartEVSE/<serial>/Set/SmartDeadBand    10
SmartEVSE/<serial>/Set/RampRateDivisor  4
```

## Configuring via REST API

```bash
curl -X POST http://smartevse-xxxx.local/settings \
  -d "ema_alpha=50&smart_deadband=10&ramp_rate_divisor=4"
```

## Troubleshooting

| Problem | Recommended settings |
|---------|---------------------|
| Current oscillates in smart mode | `EmaAlpha=50`, `SmartDeadBand=15`, `RampRateDivisor=6` |
| Rapid 1P/3P switching | `PhaseSwitchHoldDownTime=600` (10 min) |
| Stop/start cycling on marginal solar | `NoCurrentThreshold=15`, `SolarMinRunTime=120`, `SolarChargeDelay=20` |
| Renault Zoe 1-phase solar issues | `EmaAlpha=40`, `RampRateDivisor=8` (slower, smoother changes) |
| Grid import spikes during 3P→1P switch | `PhaseSwitchSevereTime=15` (faster switch on severe shortage) |
