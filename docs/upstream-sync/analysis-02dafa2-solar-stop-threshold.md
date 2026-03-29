# Analysis: Upstream Commit 02dafa2 vs Fork PR #119

## Solar Stop Timer Threshold Comparison

Both fixes address the same bug: the SolarStopTimer threshold was unreachable
in multi-node setups. They take different approaches.

### What the EVSE Knows

The EVSE has ONE measurement from the mains meter:

- **Isum** = L1 + L2 + L3 at the grid connection (sum of all phases)
  - **Positive** = importing from grid (drawing more than solar produces)
  - **Negative** = exporting to grid (solar surplus)
  - **Zero** = perfectly balanced

Everything behind the meter (house loads, solar panels, EV charger) is
invisible to the EVSE — it only sees the net result at the grid point.

**Key settings:**
- **MinCurrent** = 6A per phase (minimum charge current, IEC 61851)
- **StartCurrent** = 4A (minimum export needed to START a charge session)

### Solar Mode Behavior

The EVSE regulates its charging current to keep Isum near zero:
- Isum going negative → solar surplus → can increase charge current
- Isum going positive → grid import → must reduce charge current
- Isum stays positive too long → stop charging

### Phase Switching Happens FIRST

When the EVSE detects shortage (grid importing), the sequence is:

```
Isum positive (importing) → shortage
  ↓
Still on 3 phases? (EnableC2=AUTO)
  YES → Switch to 1 phase first (PhaseSwitchTimer)
        This reduces the EVSE's draw from 18A to 6A
        Isum drops by ~12A
  ↓
Already on 1 phase → is Isum STILL too high?
  YES → Start SolarStopTimer (eventually stop charging)
  NO  → Keep charging on 1 phase
```

**By the time the SolarStopTimer fires, the car is on 1 phase (6A).**

### The Threshold Question

"If I stop the last active car, would it immediately restart?"

The car draws 6A (1-phase) from the house side of the meter. If we stop it,
Isum drops by 6A. After stopping:

```
Isum_after = Isum - 6A
```

**Restart condition** (code line 469): charging starts when `Isum < -4A`
(grid exporting more than 4A = StartCurrent).

- At `Isum = 3A`: stop → `Isum_after = -3A`. Is -3 < -4? NO. Won't restart. ✓
- At `Isum = 1A`: stop → `Isum_after = -5A`. Is -5 < -4? YES. Would restart! ✗

**Correct threshold = 2A** (6A draw − 4A start tolerance).
Timer should start when `Isum > 2A`.

### The Three Formulas (after phase switch to 1ph)

| Formula | 1 EVSE | 2 EVSEs | 4 EVSEs | 8 EVSEs |
|---------|--------|---------|---------|---------|
| **Original (buggy)** | 2A | 8A | 20A | 44A |
| **Upstream fix (02dafa2)** | 2A | 8A | 20A | 44A |
| **Our fix (PR #119)** | 2A | 2A | 2A | 2A |

Original and upstream give **identical results** after phase switch
(Nr_Of_Phases_Charging=1 eliminates the only difference between them).

---

## Scenario: What the EVSE Sees

All scenarios assume EnableC2=AUTO, car has already switched to 1 phase.
Priority scheduling has paused all but 1 EVSE.
The active EVSE draws 6A on 1 phase.

### One EVSE plugged in

| Isum (grid) | Meaning | Stop → Isum_after | Restart? | Original (2A) | Upstream (2A) | Ours (2A) |
|---|---|---|---|---|---|---|
| -5A | Exporting 5A | -11A | Yes | No ✓ | No ✓ | No ✓ |
| -2A | Exporting 2A | -8A | Yes | No ✓ | No ✓ | No ✓ |
| 0A | Balanced | -6A | Yes | No ✓ | No ✓ | No ✓ |
| 1A | Importing 1A | -5A | Yes (-5<-4) | No ✓ | No ✓ | No ✓ |
| 2A | Importing 2A | -4A | No (-4≥-4) | No ✓ | No ✓ | No ✓ |
| 3A | Importing 3A | -3A | No | Yes ✓ | Yes ✓ | Yes ✓ |
| 5A | Importing 5A | -1A | No | Yes ✓ | Yes ✓ | Yes ✓ |

**All three formulas agree for 1 EVSE.** Threshold is 2A in all cases.

### Two EVSEs plugged in (1 active after scheduling, 1 paused)

The active EVSE still draws 6A. Isum is the same measurement.
But `ActiveEVSE = 2` (counted before priority scheduling runs).

| Isum (grid) | Stop → Isum_after | Restart? | Original (8A) | Upstream (8A) | Ours (2A) |
|---|---|---|---|---|---|
| 0A | -6A | Yes | No ✓ | No ✓ | No ✓ |
| 1A | -5A | Yes | No ✓ | No ✓ | No ✓ |
| 2A | -4A | No | No **✗** | No **✗** | No ✓ |
| 3A | -3A | No | No **✗** | No **✗** | Yes ✓ |
| 5A | -1A | No | No **✗** | No **✗** | Yes ✓ |
| 7A | 1A | No | No **✗** | No **✗** | Yes ✓ |

**At Isum=3A:** The grid is importing 3A. The EVSE draws 6A on 1 phase.
Stopping would give Isum=-3A (export 3A). That's not enough to restart
(need export > 4A). So stopping is final → timer should start.

- **Ours (2A):** 3 > 2 → timer starts ✓
- **Original/Upstream (8A):** 3 < 8 → timer doesn't start **✗**
  The car keeps drawing 6A from the grid in "solar mode."

**At Isum=7A:** Only 1A of the 6A draw is covered by solar. Grid supplies 7A.
Stopping gives Isum=1A (still importing!). Obviously should stop.

- **Ours (2A):** 7 > 2 → timer starts ✓
- **Original/Upstream (8A):** 7 < 8 → timer STILL doesn't start **✗**

### Four EVSEs plugged in (1 active, 3 paused)

Same 6A draw, same Isum values. But `ActiveEVSE = 4`.

| Isum (grid) | Restart? | **Original (20A)** | **Upstream (20A)** | **Ours (2A)** |
|---|---|---|---|---|
| 3A | No | No **✗** | No **✗** | Yes ✓ |
| 5A | No | No **✗** | No **✗** | Yes ✓ |
| 8A | No | No **✗** | No **✗** | Yes ✓ |

With 4 EVSEs, Original and Upstream need Isum > 20A to fire. But one car
on 1 phase + any house load can only produce ~8A of import. The timer
**never fires**. The car charges from the grid indefinitely.

### Eight EVSEs plugged in (1 active, 7 paused)

| | Original | Upstream | Ours |
|---|---|---|---|
| Threshold | **44A** | **44A** | **2A** |
| Max possible Isum (1 car, 1ph) | ~8A | ~8A | ~8A |
| Timer ever fires? | **Never** | **Never** | Yes, at Isum > 2A |

---

## Fixed 3-Phase (EnableC2=NOT_PRESENT)

When phase switching is not possible, the car stays on 3 phases (18A total).
The SolarStopTimer fires with `Nr_Of_Phases_Charging = 3`.

| Formula | 1 EVSE | 2 EVSEs |
|---------|--------|---------|
| **Original** | 14A | 32A |
| **Upstream** | 2A | 8A |
| **Ours** | 14A | 14A |

Correct threshold for 3-phase: `18A - 4A = 14A`.

| Isum | Stop → after | Restart? | Original 1EV (14A) | Upstream 1EV (2A) | Ours (14A) |
|---|---|---|---|---|---|
| 5A | -13A | Yes (-13<-4) | No ✓ | Yes **✗ cycling!** | No ✓ |
| 10A | -8A | Yes (-8<-4) | No ✓ | Yes **✗ cycling!** | No ✓ |
| 14A | -4A | No (-4≥-4) | No ✓ | Yes ✓ | No ✓ |
| 15A | -3A | No | Yes ✓ | Yes ✓ | Yes ✓ |

**Upstream at Isum=5A (fixed 3ph):** Timer starts (5>2). Car stops.
Isum drops to -13A (exporting 13A). Restart condition: -13 < -4 → YES.
Car immediately restarts. Isum rises back. Timer starts again.
**Stop/start cycling.**

---

## Priority Scheduling + Rotation

When solar is enough for 1 car but not all:

```
 3 EVSEs, all 1-phase after switch. Isum hovers around 0A.

 Time     Active    Isum    Other EVSEs      SolarStopTimer
 ──────   ──────    ────    ──────────────   ──────────────
 0:00     EVSE #1   ~0A     #2,#3 NO_SUN     No (0<2)
 0:30     EVSE #2   ~0A     #1,#3 NO_SUN     No (rotation)
 1:00     EVSE #3   ~0A     #1,#2 NO_SUN     No (rotation)
```

Timer doesn't start because Isum ≈ 0A (solar covers the single car).
Rotation gives each car fair charging time. All correct.

When solar drops further (Isum rises above 2A persistently):

```
 3 EVSEs, 1-phase. Isum = 4A (grid importing).

 Time     Active    Isum    SolarStopTimer (ours, 2A)
 ──────   ──────    ────    ──────────────────────────
 0:00     EVSE #1   4A      STARTS (4>2) → 10 min countdown
 ...
 10:00    —         —       EXPIRES → LESS_6A → all stop
```

Correct: grid is importing 4A, stopping gives Isum=-2A, restart blocked
(-2 ≥ -4). So stopping is permanent — timer was right to fire.

---

## Summary

| Configuration | Correct threshold | Original | Upstream | Ours |
|---|---|---|---|---|
| 1 EVSE, 1 phase | 2A | 2A ✓ | 2A ✓ | 2A ✓ |
| 2 EVSEs, 1 phase | 2A | 8A **✗** | 8A **✗** | 2A ✓ |
| 4 EVSEs, 1 phase | 2A | 20A **✗** | 20A **✗** | 2A ✓ |
| 8 EVSEs, 1 phase | 2A | 44A **✗** | 44A **✗** | 2A ✓ |
| 1 EVSE, fixed 3ph | 14A | 14A ✓ | 2A **✗ cycling** | 14A ✓ |
| 2 EVSEs, fixed 3ph | 14A | 32A **✗** | 8A **✗ cycling** | 14A ✓ |

**Our fix is the only one correct for all configurations** because:
1. It uses `Nr_Of_Phases_Charging` → adapts to actual phase configuration (1ph=2A, 3ph=14A)
2. It removes `ActiveEVSE` → constant threshold regardless of EVSE count
3. Priority scheduling + rotation handles multi-node distribution separately

**Upstream fix errors:**
1. Removes phase awareness → causes stop/start cycling for fixed 3-phase
2. Keeps ActiveEVSE → same scaling bug as original for multi-node

### Recommendation

Keep our fix (PR #119). Document as conscious divergence from upstream.
