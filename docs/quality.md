# Quality Engineering

This document describes the quality philosophy, architecture, processes, and tooling
that govern the SmartEVSE firmware. It serves as the authoritative reference for
anyone contributing to or evaluating the quality of this project.

SmartEVSE is **safety-critical embedded software** — incorrect behavior can damage
vehicles, trip breakers, or cause electrical hazards. Every design decision documented
here is driven by that reality.

---

## 1. Quality Architecture

### 1.1 Separation of Concerns

The firmware is structured into layers with strict dependency rules:

```
┌────────────────────────────────────────────────────┐
│              Application / Glue Layer               │
│  esp32.cpp · network_common.cpp · glcd.cpp          │
│  Arduino/ESP-IDF types, WiFi, MQTT client, display  │
└─────────────────────┬──────────────────────────────┘
                      │ calls pure C functions
┌─────────────────────┴──────────────────────────────┐
│              Pure C Logic Modules                    │
│  evse_state_machine.c · mqtt_parser.c · http_api.c  │
│  ocpp_logic.c · modbus_decode.c · meter_decode.c    │
│  p1_parse.c · session_log.c · led_color.c           │
│  Zero platform dependencies — compiles with gcc     │
└─────────────────────┬──────────────────────────────┘
                      │ operates on snapshot structs
┌─────────────────────┴──────────────────────────────┐
│              Bridge / HAL Layer                      │
│  evse_bridge.cpp · evse_ctx.h                       │
│  Synchronizes globals ↔ context struct              │
│  Spinlock/mutex protected critical sections         │
└─────────────────────┬──────────────────────────────┘
                      │ abstracts hardware
┌─────────────────────┴──────────────────────────────┐
│              Hardware Abstraction                    │
│  Contactors · CP pilot · LED · RS485 · GPIO         │
│  Function pointers, replaced with no-ops in tests   │
└────────────────────────────────────────────────────┘
```

**Key principle:** Safety-critical logic (state machine, current limiting, load
balancing) lives in pure C modules with zero platform dependencies. This makes
them testable on any host machine without embedded toolchains or real hardware.

### 1.2 Hardware Abstraction Layer (HAL)

Hardware operations are abstracted behind function pointers in `evse_ctx.h`:

- **Contactor control** — open/close main and C2 contactors
- **CP duty cycle** — set PWM duty for pilot signal
- **Pilot voltage read** — sample CP voltage for state detection
- **LED control** — set RGB status indicator

In production, these point to ESP32 GPIO functions. In test builds, they are
replaced with no-ops or recording stubs, allowing the full state machine to
execute natively on the developer's machine.

### 1.3 Snapshot Struct Pattern

Pure C functions never access global variables directly. Instead, the bridge layer
captures a consistent snapshot of all relevant state into `evse_ctx_t`, which is
passed to the pure functions. This eliminates race conditions between FreeRTOS
tasks and makes every function call deterministic and reproducible.

```c
// Bridge captures snapshot under spinlock
portENTER_CRITICAL(&bridge_mux);
ctx.state = State;
ctx.charge_current = ChargeCurrent;
ctx.error_flags = ErrorFlags;
portEXIT_CRITICAL(&bridge_mux);

// Pure C function operates on snapshot — deterministic, testable
evse_state_machine_tick(&ctx);
```

### 1.4 State Machine Integrity

The IEC 61851-1 state machine (`evse_state_machine.c`) is the most critical module.
It controls:

- CP pilot states A through F and their transitions
- Contactor engagement/disengagement timing
- Current limiting and overcurrent protection
- Error detection and safety shutdown

**Hard rule:** Every change to `evse_state_machine.c` MUST include corresponding
tests. This module has the highest test density in the project.

---

## 2. Quality Approach

### 2.1 Specification-First Development (SbE)

The project follows a strict **Specification by Example** workflow:

```
Requirement → SbE Specification → Failing Test → Implementation → Passing Test → PR
```

Every test function carries structured annotations:

```c
/*
 * @feature State Machine
 * @req REQ-SM-001
 * @scenario Normal charging cycle
 * @given Vehicle connected in state B
 * @when Pilot duty cycle allows charging
 * @then State transitions to C and contactor closes
 */
void test_normal_charge_cycle(void) { ... }
```

This creates a **traceable chain** from requirement to test to code, verified
automatically by CI.

### 2.2 Requirement Traceability

Every requirement has a unique ID following the pattern `REQ-{AREA}-{NNN}`:

| Prefix | Area |
|--------|------|
| `REQ-SM-` | State machine transitions |
| `REQ-ERR-` | Error handling & safety |
| `REQ-LB-` | Load balancing |
| `REQ-SOL-` | Solar mode / solar balancing |
| `REQ-OCPP-` | OCPP integration |
| `REQ-MQTT-` | MQTT command parsing |
| `REQ-API-` | HTTP REST API |
| `REQ-AUTH-` | Authorization & access control |
| `REQ-MOD-` | Modem / ISO 15118 |
| `REQ-PH-` | Phase switching |
| `REQ-MTR-` | Metering |
| `REQ-PWR-` | Power availability |
| `REQ-E2E-` | End-to-end charging flows |
| `REQ-DUAL-` | Dual-EVSE scenarios |
| `REQ-MULTI-` | Multi-node load balancing |

CI generates two reports on every build:

- **[Test Specification](../SmartEVSE-3/test/native/test-specification.md)** — all
  scenarios grouped by feature with Given/When/Then steps
- **[Traceability Report](../SmartEVSE-3/test/native/traceability-report.md)** —
  requirement-to-test coverage matrix

### 2.3 Test Metrics

| Metric | Value |
|--------|-------|
| Test suites | 44 |
| Test scenarios | 900+ |
| Features covered | 60+ |
| Requirement traceability | 100% |

---

## 3. CI/CD Pipeline & Quality Gates

### 3.1 Pipeline Overview

Every push and pull request triggers an 8-job CI pipeline:

| Job | Gate | What it catches |
|-----|------|----------------|
| **native-tests** | 900+ tests must pass | Logic errors, regressions, state machine bugs |
| **static-analysis** | cppcheck + GCC stack analysis | Uninitialized variables, style issues, stack overflow risk |
| **memory-sanitizers** | ASan + UBSan zero violations | Buffer overflows, use-after-free, undefined behavior |
| **valgrind** | Zero leaks | Memory leaks, invalid reads/writes |
| **firmware-build** | ESP32 + CH32 compile, memory budget | Type errors, missing symbols, flash/RAM budget violations |
| **traceability** | Report generation + auto-commit | SbE annotation completeness, requirement coverage |
| **bdd-tests** | Python pytest feature tests | Higher-level behavioral validation |
| **version-check** | Tag matches platformio.ini (releases only) | Version consistency |

### 3.2 Quality Gate Flow

```
Developer writes code
        │
        ▼
┌─────────────────┐    FAIL → Fix code, new commit
│  Native Tests    │──────────────────────────────┐
│  (900+ scenarios)│                              │
└────────┬────────┘                              │
         │ PASS                                   │
         ▼                                        │
┌─────────────────┐    FAIL → Fix warnings        │
│ Static Analysis  │──────────────────────────────┤
│ (cppcheck)       │                              │
└────────┬────────┘                              │
         │ PASS                                   │
         ▼                                        │
┌─────────────────┐    FAIL → Fix memory bugs     │
│ Memory Sanitizers│──────────────────────────────┤
│ (ASan + UBSan)   │                              │
└────────┬────────┘                              │
         │ PASS                                   │
         ▼                                        │
┌─────────────────┐    FAIL → Fix leaks           │
│  Valgrind        │──────────────────────────────┤
└────────┬────────┘                              │
         │ PASS                                   │
         ▼                                        │
┌─────────────────┐    FAIL → Fix build/budget    │
│ Firmware Build   │──────────────────────────────┤
│ (ESP32 + CH32)   │                              │
└────────┬────────┘                              │
         │ PASS                                   │
         ▼                                        │
┌─────────────────┐                              │
│  PR Mergeable    │◄─────────────────────────────┘
└─────────────────┘          (all gates green)
```

### 3.3 Pre-Push Verification (Local)

Before pushing, developers must run the full verification sequence locally:

```bash
# 1. Native tests
cd SmartEVSE-3/test/native && make clean test

# 2. Memory sanitizers
make clean test CFLAGS_EXTRA="-fsanitize=address,undefined -fno-omit-frame-pointer"

# 3. Static analysis
cppcheck --enable=warning,style,performance --error-exitcode=1 \
  --suppress=missingIncludeSystem --inline-suppr \
  -I SmartEVSE-3/src -I SmartEVSE-3/test/native/include \
  SmartEVSE-3/src/evse_state_machine.c \
  SmartEVSE-3/src/mqtt_parser.c \
  SmartEVSE-3/src/http_api.c \
  SmartEVSE-3/src/modbus_decode.c \
  SmartEVSE-3/src/meter_decode.c \
  SmartEVSE-3/src/ocpp_logic.c \
  # ... (see CLAUDE.md for full list)

# 4. ESP32 firmware build
pio run -e release -d SmartEVSE-3/

# 5. CH32 firmware build
pio run -e ch32 -d SmartEVSE-3/
```

Skipping any step is not permitted. Native tests alone are insufficient — they
cannot detect type errors, missing includes, or linking issues in firmware files.

---

## 4. Hardening & Defensive Programming

### 4.1 Buffer Safety

- **`sprintf` is banned** — all buffer writes use `snprintf` with explicit sizes
- All formatting functions take a `(char *buf, size_t len)` signature
- Buffer overflow is the #1 embedded vulnerability; this rule has zero exceptions

### 4.2 Memory Safety

- **No heap allocation in ISRs, timer callbacks, or critical sections** — no
  `malloc`, `new`, Arduino `String`, or blocking calls inside
  `portENTER_CRITICAL` / `portEXIT_CRITICAL` blocks
- **Spinlock-protected shared state** — all cross-task variable access goes
  through the bridge layer with spinlock protection (< 10 µs critical sections)
- **Static allocation preferred** — ring buffers, diagnostic structs, and telemetry
  counters use fixed-size static allocations

### 4.3 Memory Budget Enforcement

The CI pipeline enforces strict memory budgets:

| Target | Flash budget | RAM budget | Current usage |
|--------|-------------|-----------|---------------|
| ESP32 | 95% (1,640 KB) | 90% (288 KB) | ~84% / ~21% |
| CH32 | 95% (61 KB) | 90% (18 KB) | ~59% / ~19% |

Every firmware build in CI reports flash and RAM usage. Builds that exceed the
budget are rejected. This prevents creeping memory growth from accumulating
across many small changes.

### 4.4 Stack Usage Analysis

The CI `static-analysis` job includes GCC stack usage checking. Functions with
excessive stack frames are flagged. FreeRTOS task stack sizes are documented
and monitored.

### 4.5 Undefined Behavior Detection

The CI `memory-sanitizers` job compiles the test suite with:
- **AddressSanitizer (ASan)** — detects buffer overflows, use-after-free,
  double-free, stack buffer overflows
- **UndefinedBehaviorSanitizer (UBSan)** — detects signed integer overflow,
  null pointer dereference, misaligned access, shift overflow

Any violation fails the build immediately.

### 4.6 Leak Detection

The CI `valgrind` job runs the full test suite under Valgrind's memcheck tool.
Any memory leak, invalid read, or invalid write fails the build.

---

## 5. Resilience & Error Handling

### 5.1 Communication Timeout Architecture

The firmware monitors all external communication channels with configurable
timeouts:

| Channel | Timeout | Recovery action |
|---------|---------|----------------|
| Mains Modbus meter | 11 seconds | `CT_NOCOMM` error flag, current limiting |
| EV Modbus meter | 8 × NR_EVSES seconds | `EV_NOCOMM` error flag |
| HomeWizard P1 HTTP | 11 seconds | mDNS rediscovery, cache invalidation |
| API/MQTT mains feed | 120 seconds (configurable) | Fall back to MaxMains |
| OCPP WebSocket | MicroOcpp internal | Automatic reconnect with backoff |
| Load balancing nodes | Per-node timeout | Node marked offline, current redistributed |

### 5.2 Graceful Degradation

When communication fails, the system degrades gracefully rather than shutting
down:

- **Mains meter loss** → charges at MaxMains (conservative safe limit)
- **EV meter loss** → continues charging, energy reporting stops
- **OCPP disconnect** → offline transaction queue, local authorization fallback
- **WiFi loss** → LCD control still works, charging continues uninterrupted
- **Node communication loss** → master redistributes capacity among remaining nodes

### 5.3 Safety Interlocks

The state machine enforces safety interlocks that cannot be bypassed:

- **Overcurrent protection** — current reduced or charging stopped when mains
  sum exceeds MaxSumMains
- **Thermal protection** — built-in temperature sensor triggers current reduction
  and eventual shutdown
- **Contactor welding detection** — state machine detects if contactor fails to
  open and enters permanent error state
- **Diode check** — validates pilot signal diode at startup
- **Phase switch safety** — never switches phases while current is flowing;
  full stop → switch → restart cycle enforced

### 5.4 Watchdog & Recovery

- ESP32 hardware watchdog prevents firmware hangs
- FreeRTOS task watchdog monitors task execution
- Diagnostic telemetry captures pre-crash state for post-mortem analysis
- LittleFS persistence survives reboots

---

## 6. Interoperability Testing

### 6.1 Current State

The existing test suite validates protocol logic at the unit level:

- **OCPP**: 85 tests for authorization, connector state, RFID formatting,
  settings validation, LoadBl exclusivity, telemetry (pure C in `ocpp_logic.c`)
- **Modbus**: 30+ tests for frame decoding (FC03/04/06/10), meter byte
  interpretation (4 endianness modes, 3 data types), P1 JSON parsing
- **MQTT**: command parsing and publish formatting tests
- **HTTP API**: request validation tests

These tests use static inputs (canned byte arrays, hardcoded JSON). They verify
that SmartEVSE's *internal* logic is correct, but they do not verify that
SmartEVSE correctly communicates with *external* systems.

### 6.2 Planned: OCPP Compatibility Testing (Plan 11)

**Goal:** Verify that SmartEVSE's OCPP messages are accepted by a real Central
System, and that SmartEVSE correctly handles the full range of CSMS responses.

**Approach:**
- **Tool:** [mobilityhouse/ocpp](https://github.com/mobilityhouse/ocpp) (Python,
  MIT license) as a configurable mock CSMS
- **Charge point:** MicroOcppSimulator compiled natively, using the same MicroOcpp
  library version as SmartEVSE firmware
- **Transport:** localhost WebSocket, no Docker required
- **CI job:** `ocpp-compatibility` — runs in GitHub Actions, < 3 minutes

**Coverage:** 44 test scenarios across boot/connection, authorization, transaction
lifecycle, MeterValues, StatusNotification, Smart Charging, remote control, error
handling (CALLERROR, malformed JSON, reconnection), and provider-specific profiles
(Tap Electric, Tibber, SteVe).

See [Plan 11](../EVSE-team-planning/plan-11-ocpp-compatibility-testing.md) for
full details.

### 6.3 Planned: Modbus Compatibility Testing (Plan 12)

**Goal:** Verify that SmartEVSE correctly reads current, power, and energy from
all 15+ supported energy meter types by testing against realistic Modbus register
data.

**Approach:**
- **Tool:** [PyModbus](https://github.com/pymodbus-dev/pymodbus) (Python, BSD
  license) as a simulated energy meter slave
- **Bridge:** C decode functions (`modbus_decode.c`, `meter_decode.c`) compiled
  as a shared library, called from Python via ctypes
- **Register data:** Per-meter profiles with test vectors validated against
  official datasheets
- **CI job:** `modbus-compatibility` — runs in GitHub Actions, < 1 minute

**Coverage:** 41 test scenarios across register map validation (all 15 meter
types), endianness handling (HBF_HWF, HBF_LWF, LBF_LWF), data types (FLOAT32,
INT32, INT16), phase mapping, value scaling (A→mA, kWh→Wh), error responses
(Modbus exceptions, CRC errors, truncated frames), and edge values.

See [Plan 12](../EVSE-team-planning/plan-12-modbus-compatibility-testing.md) for
full details.

### 6.4 Interoperability Testing Philosophy

The interoperability tests complement, not replace, the unit tests:

| Layer | What it validates | Tools |
|-------|------------------|-------|
| **Unit tests** (existing) | Internal logic correctness | Native C test suite |
| **Protocol tests** (Plan 11/12) | Message format, sequencing, error handling | mobilityhouse/ocpp, PyModbus |
| **Integration tests** (future) | Full stack end-to-end | EVerest car_simulator (if needed) |
| **Certification** (manual) | Formal standards compliance | OCA OCTT (commercial, cloud) |

The guiding principle: **automate what can run in CI/CD, and reserve manual
testing for what truly requires hardware or commercial certification tools.**

---

## 7. Code Quality Tooling

### 7.1 Static Analysis

**cppcheck** runs on every CI build with `--enable=warning,style,performance`:
- Detects uninitialized variables
- Flags unreachable code
- Identifies performance anti-patterns
- Catches missing return statements

The analysis covers all pure C modules — the same files that contain the
safety-critical logic.

### 7.2 CodeQL Security Scanning

GitHub CodeQL runs on every push via `.github/workflows/codeql.yaml`:
- Detects common vulnerability patterns (buffer overflow, injection, etc.)
- Covers C/C++ specific security issues
- Results visible in GitHub Security tab

### 7.3 Compiler Warnings

All targets compile with `-Wall -Wextra -Wunused-variable`. The project
maintains a **zero warnings policy** — any new warning fails the build.

---

## 8. Multi-Agent Quality Assurance

When multiple AI agents work on this codebase (Claude Code, GitHub Copilot, or
any other agent), the **Quality Guardian** pattern applies:

- One designated agent reviews all code changes without writing implementation
- Reviews check: naming conventions, SbE annotations, buffer safety, test coverage,
  bridge layer compliance, memory budget
- Quality Guardian runs tests and firmware builds after each implementation agent
- Quality Guardian has **veto authority** — non-compliant code is rejected

See [CLAUDE.md](../CLAUDE.md) for the full multi-agent workflow and specialist
role definitions.

---

## 9. Continuous Improvement

Quality is not static. The project uses a **Learning Loop** after each completed
issue:

1. **What worked?** — patterns and approaches to repeat
2. **What didn't?** — mistakes and false starts to avoid
3. **What changed?** — new understanding of the codebase or domain

Learnings feed back into this document, CLAUDE.md, and CODING_STANDARDS.md.
The quality process itself evolves through use.
