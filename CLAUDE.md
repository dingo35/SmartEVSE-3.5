# Claude Code — Project Instructions

This file configures Claude Code (and any Claude-based agent) when working on the
SmartEVSE codebase. Read this entire file before making any changes.

## Project Overview

SmartEVSE is an open-source Electric Vehicle Supply Equipment (EVSE) controller.
The firmware runs on ESP32 (v3/v4) and CH32 microcontrollers. This is safety-critical
embedded software — incorrect behavior can damage vehicles, trip breakers, or cause
electrical hazards.

## Required Reading — MANDATORY

Before writing ANY code, agents MUST read and understand these documents:

| Document | Path | Purpose |
|----------|------|---------|
| This file | `CLAUDE.md` | Agent workflow, specialist roles, quality guardian |
| Quality Engineering | `docs/quality.md` | Architecture, testing methodology, CI/CD pipeline, hardening, interoperability testing |
| Coding Standards | `CODING_STANDARDS.md` | Naming conventions, buffer safety, FreeRTOS patterns |
| Features | `docs/features.md` | Feature details and fork improvements — understand what exists before changing it |
| Upstream Differences | `docs/upstream-differences.md` | What changed from upstream and why — avoid undoing or conflicting with existing work |
| Contributing | `CONTRIBUTING.md` | Workflow, SbE format, submission process, guardrails |

**Skipping required reading is not permitted.** When spawning sub-agents, include
a reference to these documents in the prompt so the sub-agent reads them before
working.

## Deviation from Guardrails — HARD RULE

All rules in this file and in `CONTRIBUTING.md` are **non-negotiable by default**.
If an agent encounters a situation where a rule needs to be bent (e.g., modifying
`evse_state_machine.c` for a refactor that temporarily breaks a test), the agent
MUST:

1. **Stop** — do not proceed with the deviation
2. **Explain** — describe which rule needs to be deviated from and why
3. **Propose** — suggest safeguards (e.g., "I will restore the test within this
   same commit")
4. **Wait for explicit approval** from the user before proceeding

Agents must NEVER silently deviate from guardrails, even if they believe the
deviation is safe. The cost of asking is low; the cost of a safety regression
in embedded firmware is high.

## Critical Rules

### Ownership Boundary — HARD RULE

- **Never make changes outside repositories owned by `basmeerman`.** Do not create
  PRs, push branches, open issues, comment, or make any modifications to repositories
  owned by other GitHub users or organizations (including `dingo35/SmartEVSE-3.5`).
  All work happens exclusively within `basmeerman`-owned repos unless the user
  explicitly instructs otherwise for a specific action.

### Safety First

- **Never modify `evse_state_machine.c` without adding or updating tests.** This
  module controls contactors, CP pilot signals, and current limiting. Every change
  must have a corresponding test in `SmartEVSE-3/test/native/tests/`.
- **Never bypass error checks or safety validations.** Do not remove or weaken
  bounds checking, overcurrent protection, or state machine guard conditions.
- **Never use `sprintf`.** Always use `snprintf` with explicit buffer sizes.
- **Never allocate heap memory in ISRs, timer callbacks, or critical sections.**
  No `malloc`, `new`, Arduino `String`, or any blocking call inside
  `portENTER_CRITICAL` / `portEXIT_CRITICAL` blocks.
- **Never add `#ifdef` platform guards in core logic.** Use the bridge layer
  (`evse_bridge.h`) to keep platform-specific code out of `evse_state_machine.c`.

### Specification-First Workflow

This project follows a strict test-first workflow. When implementing any change:

1. **Write the SbE specification first** — describe the expected behavior using
   Given/When/Then before writing any implementation code.
2. **Write the test** — create or extend a test file in
   `SmartEVSE-3/test/native/tests/` with full SbE annotations.
3. **Then implement** — make the code change so the test passes.
4. **Verify** — run `cd SmartEVSE-3/test/native && make clean test` to confirm
   all tests pass (not just the new one).

Never skip the test. Never write implementation code without a corresponding test.

### SbE Annotation Format

Every test function MUST have a block comment with these annotations:

```c
/*
 * @feature Feature Name
 * @req REQ-AREA-NNN
 * @scenario What this test verifies
 * @given Precondition
 * @when Action or trigger
 * @then Expected outcome
 */
void test_descriptive_name(void) { ... }
```

Use existing feature names and requirement prefixes from
`SmartEVSE-3/test/native/test-specification.md`. Requirement ID prefixes:

- `REQ-SM-` State machine, `REQ-ERR-` Error/safety, `REQ-LB-` Load balancing
- `REQ-SOL-` Solar, `REQ-OCPP-` OCPP, `REQ-MQTT-` MQTT, `REQ-API-` HTTP API
- `REQ-AUTH-` Authorization, `REQ-MOD-` Modem/ISO15118, `REQ-PH-` Phase switching
- `REQ-MTR-` Metering, `REQ-PWR-` Power, `REQ-E2E-` End-to-end
- `REQ-DUAL-` Dual-EVSE, `REQ-MULTI-` Multi-node

## Architecture

### Module Structure

```
evse_state_machine.c    Pure C state machine — NO platform dependencies
evse_ctx.h              Context struct (evse_ctx_t) — all state in one place
evse_bridge.h/cpp       Syncs firmware globals <-> evse_ctx_t (spinlock-protected)
mqtt_parser.h/c         Pure C MQTT command parsing — no Arduino dependency
http_api.h/c            Pure C HTTP validation — no Arduino dependency
firmware_manager.h/cpp  OTA update logic — extracted from network_common.cpp
esp32.cpp               Firmware glue: callbacks, dispatchers, hardware I/O
network_common.cpp      WiFi, MQTT client, HTTP server (Mongoose)
```

### Key Architectural Principles

1. **Pure C modules for testability.** Parsing, validation, and state machine logic
   must compile and test natively with `gcc` on any host. No Arduino, ESP-IDF, or
   FreeRTOS dependencies in these modules.

2. **Thin glue layer.** `esp32.cpp` and `network_common.cpp` are thin dispatchers
   that convert Arduino types to C types and call pure modules. Keep glue code
   minimal — push logic into the pure modules.

3. **Snapshot structs.** Pure functions operate on struct snapshots (e.g.,
   `evse_ctx_t`, `http_settings_request_t`, `mqtt_command_t`), never on globals
   directly.

4. **Bridge pattern.** `evse_bridge.cpp` syncs globals to/from `evse_ctx_t` inside
   spinlock-protected critical sections. Never access `evse_ctx_t` fields from
   outside the bridge without synchronization.

5. **Platform guards.** Use `#ifdef SMARTEVSE_VERSION` (30 or 40) only in the bridge
   and glue layers, never in `evse_state_machine.c` or parser modules.

### Memory Budget

| Target | Flash budget | RAM budget | Current |
|--------|-------------|-----------|---------|
| ESP32  | 95% (1,640KB) | 90% (288KB) | ~84% / ~21% |
| CH32   | 95% (61KB)   | 90% (18KB)  | ~59% / ~19% |

Before merging, verify: `pio run -e release -d SmartEVSE-3/` and
`pio run -e ch32 -d SmartEVSE-3/` both compile within budget. CI enforces this.

## Coding Conventions

Read `CODING_STANDARDS.md` for the full reference. Key rules:

- **Functions**: `snake_case` — `evse_tick_10ms()`, `mqtt_parse_command()`
- **Globals**: `CamelCase` — `ChargeDelay`, `ErrorFlags`, `State`
- **Macros/constants**: `UPPER_SNAKE_CASE` — `MAX_MAINS`, `STATE_A`, `MODE_SOLAR`
- **Typedefs/enums**: `CamelCase` — `EvseState`, `mqtt_command_t`
- **C standard**: C11 (`-std=c11`) for `.c` files
- **Compiler warnings**: `-Wall -Wextra -Wunused-variable` — zero warnings policy
- **Buffer writes**: always `snprintf(buf, sizeof(buf), ...)`, never `sprintf`
- **Critical sections**: < 10 us, no blocking calls, use spinlocks

## Verification Commands

```bash
# Run all native tests (must pass before any PR)
cd SmartEVSE-3/test/native && make clean test

# Run with sanitizers (catches memory bugs)
cd SmartEVSE-3/test/native && make clean test \
  CFLAGS_EXTRA="-fsanitize=address,undefined -fno-omit-frame-pointer"

# Static analysis (matches CI cppcheck config exactly)
cppcheck --enable=warning,style,performance \
  --error-exitcode=1 \
  --suppress=missingIncludeSystem \
  --inline-suppr \
  -I SmartEVSE-3/src \
  -I SmartEVSE-3/test/native/include \
  SmartEVSE-3/src/evse_state_machine.c \
  SmartEVSE-3/src/session_log.c \
  SmartEVSE-3/src/mqtt_parser.c \
  SmartEVSE-3/src/mqtt_publish.c \
  SmartEVSE-3/src/http_api.c \
  SmartEVSE-3/src/serial_parser.c \
  SmartEVSE-3/src/led_color.c \
  SmartEVSE-3/src/meter_telemetry.c \
  SmartEVSE-3/src/modbus_decode.c \
  SmartEVSE-3/src/meter_decode.c \
  SmartEVSE-3/src/p1_parse.c \
  SmartEVSE-3/src/modbus_log.c \
  SmartEVSE-3/src/ocpp_logic.c \
  SmartEVSE-3/src/ocpp_telemetry.c \
  SmartEVSE-3/src/solar_debug_json.c \
  SmartEVSE-3/src/diag_telemetry.c \
  SmartEVSE-3/src/diag_modbus.c

# Build ESP32 firmware
pio run -e release -d SmartEVSE-3/

# Build CH32 firmware
pio run -e ch32 -d SmartEVSE-3/

# Regenerate test specification
cd SmartEVSE-3/test/native && python3 scripts/extract_traceability.py \
  --markdown test-specification.md --html traceability-report.html
```

### Pre-Push Verification — HARD RULE

**Never push or create a PR without running the full verification sequence.**
Native tests alone are NOT sufficient — they only compile pure C modules and
cannot detect type errors, missing includes, or linking issues in `esp32.cpp`
and other firmware files that depend on the ESP32 toolchain.

Before every `git push`, run ALL of the following in order:

```bash
# 1. Native tests
cd SmartEVSE-3/test/native && make clean test

# 2. Memory sanitizers
make clean test CFLAGS_EXTRA="-fsanitize=address,undefined -fno-omit-frame-pointer"

# 3. Static analysis (matches CI cppcheck config)
cppcheck --enable=warning,style,performance \
  --error-exitcode=1 \
  --suppress=missingIncludeSystem \
  --inline-suppr \
  -I SmartEVSE-3/src \
  -I SmartEVSE-3/test/native/include \
  SmartEVSE-3/src/evse_state_machine.c \
  SmartEVSE-3/src/session_log.c \
  SmartEVSE-3/src/mqtt_parser.c \
  SmartEVSE-3/src/mqtt_publish.c \
  SmartEVSE-3/src/http_api.c \
  SmartEVSE-3/src/serial_parser.c \
  SmartEVSE-3/src/led_color.c \
  SmartEVSE-3/src/meter_telemetry.c \
  SmartEVSE-3/src/modbus_decode.c \
  SmartEVSE-3/src/meter_decode.c \
  SmartEVSE-3/src/p1_parse.c \
  SmartEVSE-3/src/modbus_log.c \
  SmartEVSE-3/src/ocpp_logic.c \
  SmartEVSE-3/src/ocpp_telemetry.c \
  SmartEVSE-3/src/solar_debug_json.c \
  SmartEVSE-3/src/diag_telemetry.c \
  SmartEVSE-3/src/diag_modbus.c

# 4. ESP32 firmware build
pio run -e release -d SmartEVSE-3/

# 5. CH32 firmware build
pio run -e ch32 -d SmartEVSE-3/
```

Do not skip any step. Do not assume "tests pass, so it's fine."

- **Steps 1-2** catch logic errors, memory bugs, and undefined behavior.
- **Step 3** catches uninitialized variables, style issues, and performance
  problems that compilers miss. This mirrors the CI `static-analysis` job.
  Install cppcheck locally: `brew install cppcheck` (macOS) or
  `apt install cppcheck` (Linux).
- **Steps 4-5** catch type mismatches, missing symbols, and Arduino/ESP-IDF
  API misuse that native tests cannot reach.

Skipping steps has caused CI failures on PRs that were trivially preventable.

## Multi-Agent Workflow

When multiple AI agents work on this codebase simultaneously, one agent MUST act
as the **Quality Guardian**. This applies to Claude Code with parallel Task agents,
multi-agent orchestration, or any setup where more than one agent writes code.

### Specialist Agent Roles

Each implementation agent SHOULD be assigned a specialist role that matches the
task. Specialist roles carry domain knowledge, file ownership, and standards
awareness specific to their area. When spawning agents, include the relevant
specialist context in the prompt.

#### State Machine Specialist

**Domain:** IEC 61851-1 CP pilot states, contactor control, current limiting,
error handling, charge session lifecycle.

- **Owns:** `evse_state_machine.c`, `evse_ctx.h`, `evse_bridge.cpp`
- **Tests:** `test/native/tests/test_state_machine*.c`
- **Req prefixes:** `REQ-SM-`, `REQ-ERR-`
- **Must know:** CP states A-F and their voltage levels, proximity pilot (PP)
  resistance values, IEC 61851-1 timing requirements (e.g., 3s state C→B
  transition), diode check behavior, contactor welding detection.
- **Key constraint:** This code MUST remain pure C with zero platform
  dependencies. Every change requires a test. The bridge layer syncs globals
  via spinlock — never access `evse_ctx_t` from outside the bridge.
- **Community context:** Solar mode stability and phase switching (1P↔3P) are
  the #1 user pain point. The state machine must handle graceful degradation
  when solar power drops below MinCurrent thresholds.

#### Load Balancing Specialist

**Domain:** Multi-node power sharing, current distribution across up to 8 EVSE
nodes, mains capacity protection, priority-based scheduling.

- **Owns:** Load balancing logic in `main.cpp` (candidate for extraction),
  `evse_state_machine.c` (balanced current application)
- **Tests:** `test/native/tests/test_load_balancing*.c`
- **Req prefixes:** `REQ-LB-`, `REQ-MULTI-`, `REQ-PWR-`
- **Must know:** Master/slave Modbus communication, `IsumImport` / `Isum`
  calculations, `ActiveEVSE` counting, `StartCurrent` vs `MinCurrent`
  thresholds, the `calcbalancedcurrent` algorithm.
- **Key constraint:** Load balancing logic in `main.cpp` (~2,835 lines) is the
  top extraction candidate. New work should push logic into a pure C module
  that can be tested natively.
- **Community context:** Multi-node oscillation is a known bug. An unreleased
  algorithm rewrite exists upstream (dingo35's `calcbalancedcurrent` branch)
  but is blocked by lack of test coverage. Belgian capacity-based billing
  makes fair distribution critical for users.

#### Solar & Smart Mode Specialist

**Domain:** Solar surplus charging, smart mode grid import optimization,
1-phase to 3-phase switching logic, charge current regulation.

- **Owns:** Solar/smart mode logic in `main.cpp` and `evse_state_machine.c`
- **Tests:** `test/native/tests/test_solar*.c`, `test/native/tests/test_phase*.c`
- **Req prefixes:** `REQ-SOL-`, `REQ-PH-`
- **Must know:** `MODE_SOLAR` / `MODE_SMART` behavior, `ImportCurrent`
  calculation, `SolarStopTimer` / `SolarStartTimer` thresholds, C2 contactor
  control for phase switching, `EnableC2 == AUTO` logic, `Nr_Of_Phases_Charging`
  transitions, `GOING_TO_SWITCH_1P` / `GOING_TO_SWITCH_3P` states.
- **Key constraint:** Phase switching involves contactor timing — never switch
  phases while current is flowing. The state machine must complete a full
  stop→switch→restart cycle.
- **Community context:** This is the most complained-about feature. Specific
  issues: current oscillation in smart mode, slow 3P→1P switching, Renault Zoe
  compatibility with 1-phase solar, stop/start cycling with marginal solar.

#### OCPP Specialist

**Domain:** OCPP 1.6/2.0.1 protocol, charge point backend communication,
authorization, transaction handling, remote control.

- **Owns:** OCPP-related code in `network_common.cpp`, OCPP library integration
- **Tests:** `test/native/tests/test_ocpp*.c` (to be created)
- **Req prefixes:** `REQ-OCPP-`
- **Must know:** OCPP message types (BootNotification, Authorize, StartTransaction,
  MeterValues, StatusNotification), WebSocket transport, idTag handling,
  interaction between OCPP authorization and local RFID, OCPP vs internal
  load balancing (must be mutually exclusive).
- **Key constraint:** OCPP code currently lives in `network_common.cpp` and is
  not yet extracted to a pure C module. Extraction is needed before meaningful
  test coverage is possible.
- **Community context:** Growing adoption but documentation is poor. Users
  struggle with provider setup (Tap Electric, Tibber, E-Flux). Dual-charger
  OCPP setups have known issues. EVCC integration via OCPP is an emerging
  use case — requires IEC 61851 status codes in the API.

#### Network & MQTT Specialist

**Domain:** WiFi connectivity, MQTT pub/sub, HTTP REST API (Mongoose),
WebSocket communication, mDNS discovery, OTA updates.

- **Owns:** `network_common.cpp`, `mqtt_parser.c`, `http_api.c`,
  `firmware_manager.cpp`
- **Tests:** `test/native/tests/test_mqtt*.c`, `test/native/tests/test_http*.c`
- **Req prefixes:** `REQ-MQTT-`, `REQ-API-`
- **Must know:** Mongoose HTTP/WebSocket server API, MQTT topic structure
  (`SmartEVSE/<serial>/Set/...`), REST API endpoints (`/settings`, `/currents`),
  Home Assistant MQTT discovery payloads, mDNS for HomeWizard P1 meter
  detection.
- **Key constraint:** `mqtt_parser.c` and `http_api.c` are already extracted
  to pure C. New MQTT/API features should follow the same pattern: parse in
  pure C, dispatch in glue layer.
- **Community context:** Home Assistant is the dominant integration target.
  Users want MQTT to publish only changed values (reduce message volume).
  HomeWizard P1 meter mDNS detection has intermittent issues. EVCC integration
  needs IEC 61851 status code and phase switching command in the API.

#### Modbus & Metering Specialist

**Domain:** Modbus RTU over RS485, energy meter communication, Sensorbox
protocol, current transformer (CT) readings.

- **Owns:** `modbus.cpp` (926 lines, candidate for extraction), meter-related
  code in `esp32.cpp`
- **Tests:** `test/native/tests/test_modbus*.c` (to be created)
- **Req prefixes:** `REQ-MTR-`, `REQ-MULTI-`
- **Must know:** Modbus RTU framing, register maps for supported meters
  (Eastron SDM630/SDM120, Finder, ABB B23, Phoenix Contact, Custom),
  Sensorbox v2 protocol, EV meter vs Mains meter distinction, meter address
  configuration, `InvEastron` setting.
- **Key constraint:** `modbus.cpp` is not yet extracted to pure C. The v3.1
  hardware has known Modbus communication issues with certain Eastron meters.
  New meter type support must not break existing meter compatibility.
- **Community context:** Meter compatibility issues (especially Eastron on
  v3.1 hardware) are a recurring support topic. Users request new meter types
  (ABB EV3, Orno WE-517) and more meter data via MQTT (per-phase kWh).

#### Web UI Specialist

**Domain:** Dashboard, configuration pages, mobile responsiveness, WebSocket
real-time updates, packed filesystem.

- **Owns:** Web UI source files (packed into `packed_fs.c`), WebSocket
  handlers in `network_common.cpp`
- **Req prefixes:** `REQ-API-` (for API endpoints serving the UI)
- **Must know:** How the web UI is compiled into `packed_fs.c`, Mongoose
  WebSocket API, current dashboard layout, mobile viewport requirements.
- **Key constraint:** The UI is packed into a C array (`packed_fs.c`) for
  the ESP32 filesystem. Changes to web files require regenerating this file.
  Keep JavaScript/CSS minimal — every byte counts against the flash budget.
- **Community context:** The web UI is considered dated. Community members
  have proposed redesigns. LCD remote control via WebSockets was recently
  added (PR #331). Mobile UX improvements are in demand.

#### QA & Test Infrastructure Specialist

**Domain:** Native test framework, SbE traceability, CI/CD pipeline,
sanitizer builds, test coverage expansion.

- **Owns:** `test/native/tests/`, `test/native/Makefile`,
  `test/native/scripts/`, `.github/workflows/`
- **Must know:** Unity test framework, SbE annotation format, traceability
  extraction script, Makefile structure for adding new test suites, how to
  mock hardware dependencies for native testing, address/UB sanitizer usage.
- **Key constraint:** All test suites must compile and run on the host with
  `gcc` — no ESP-IDF or Arduino dependencies. New test files must be added
  to the Makefile. Every test function must have complete SbE annotations.
- **Strategic priority:** Test coverage for load balancing and solar mode is
  the highest-leverage work — it directly unblocks upstream algorithm rewrites
  that are currently blocked by regression risk.

### Quality Guardian Role

The Quality Guardian agent does NOT write implementation code. Its responsibilities:

1. **Review every code change** before it is committed:
   - Naming conventions match (`snake_case` functions, `CamelCase` globals)
   - No `sprintf`, no heap allocation in critical paths
   - State machine changes have corresponding tests
   - SbE annotations are present and well-formed on all test functions
   - No direct global access bypassing the bridge layer
   - `extern "C"` guards present on all `.h` files included from both C and C++

2. **Run the test suite** after each implementation agent finishes:
   ```bash
   cd SmartEVSE-3/test/native && make clean test
   ```

3. **Run the firmware build** to verify compilation:
   ```bash
   pio run -e release -d SmartEVSE-3/
   pio run -e ch32 -d SmartEVSE-3/
   ```

4. **Regenerate and verify traceability**:
   ```bash
   cd SmartEVSE-3/test/native && python3 scripts/extract_traceability.py \
     --markdown test-specification.md
   ```
   Verify new tests appear with correct feature names and requirement IDs.

5. **Check memory budget** — flash and RAM usage must stay within budget limits
   shown above.

6. **Reject changes that violate these standards.** The Quality Guardian has veto
   authority. Non-compliant code must be fixed before merging.

### Implementation Agent Rules

Agents that write code must:

- Read this file (`CLAUDE.md`) and `CODING_STANDARDS.md` before starting
- Be assigned a specialist role (see above) matching the task scope
- Stay within the file ownership boundaries of their specialist role
- Follow the specification-first workflow (SbE -> test -> code)
- Not modify files outside their assigned scope without coordination
- Not commit directly — submit changes for Quality Guardian review
- Not modify `evse_state_machine.c` without adding tests in the same changeset
- Run `make clean test` locally before declaring work complete
- Reference the community context from their specialist role to understand
  real-world impact and user expectations for the feature area

### Coordination Protocol

When dividing work across agents:

- Assign non-overlapping files to each implementation agent
- The Quality Guardian reviews all changes in sequence, not in parallel
- If two agents need to modify the same file, one waits for the other to finish
- Merge conflicts are resolved by the Quality Guardian, preserving both changes
- Each agent's changes are verified independently before combining

## Project-Driven Development Workflow

This project uses **GitHub Projects (v2)** to manage all improvement work. Each
improvement plan (stored in the `EVSE-team-planning/` directory) corresponds to a
GitHub Project board. Agents discover, execute, and close work through this system.

### Project Structure

Each GitHub Project uses a **Board** layout with these columns:

| Column | Meaning | Who moves items here |
|--------|---------|---------------------|
| **Backlog** | Issues created from plan increments, not yet started | Human or agent during planning |
| **Spec & Test** | SbE specification written, test being created | Agent picking up work |
| **In Progress** | Implementation underway (code being written) | Agent after tests are written |
| **Review** | PR created, Quality Guardian reviewing | Agent after `make clean test` passes |
| **Done** | Merged to master, tests green, docs regenerated | Quality Guardian after approval |

### Issue Format

Each plan increment becomes a GitHub Issue:

- **Title**: `[Plan-NN] Increment N: <short description>`
- **Labels**: `plan-NN`, specialist role, priority (`P1`-`P4`)
- **Body**: SbE scenarios, acceptance criteria, file scope, dependency links
- **Linked**: to the GitHub Project board and to the plan file

### How Agents Pick Up Work

1. **Check the project boards** — find the highest-priority unblocked issue in
   the Backlog column (no unresolved dependencies)
2. **Read the plan file** — the full plan in `EVSE-team-planning/plan-NN-*.md`
   provides root cause analysis, implementation strategy, and test scenarios
3. **Read CLAUDE.md** — always re-read before starting work
4. **Claim the issue** — move to "Spec & Test" column

### Agent Execution Cycle (per issue)

```
1. READ    — Read issue, plan file, CLAUDE.md, docs/quality.md, CONTRIBUTING.md, and relevant source files
2. SPEC    — Write Given/When/Then scenarios → move issue to "Spec & Test"
3. TEST    — Write failing tests in test/native/tests/
4. CODE    — Implement until tests pass
5. VERIFY  — make clean test + pio build checks (ESP32 + CH32)
6. PR      — Create PR on basmeerman/SmartEVSE-3.5, linked to the issue
7. REVIEW  — Quality Guardian reviews → move issue to "Review"
8. MERGE   — If approved, merge and close issue → move to "Done"
9. RETRO   — Log lessons learned (see Learning Loop below)
```

### Learning Loop

After completing each issue, the agent evaluates three questions:

1. **What worked?** — Patterns, approaches, or tools that should be repeated
2. **What didn't?** — Mistakes, false starts, or wasted effort to avoid
3. **What changed?** — New understanding of the codebase, constraints, or domain

Learnings are persisted based on their nature:

| Learning type | Where to persist | Example |
|--------------|-----------------|---------|
| New rule or convention | **CLAUDE.md** (propose change, human approves) | "Always check AccessTimer when modifying AccessStatus transitions" |
| Domain/project context | **Memory file** (project type) | "Belgian capacity-based billing makes fair load distribution critical" |
| User preference | **Memory file** (feedback type) | "User prefers concise PR descriptions with root cause in body" |
| Codebase insight | **Memory file** (reference type) | "OCPP auth flow: CheckRFID → ocppUpdateRfidReading → backend → setAccess" |

**Updating the way of working itself**: If an agent identifies a process improvement
(not just a code pattern), it proposes the change to the human. If approved, the
agent updates this section of CLAUDE.md. The way of working is a living document
that evolves through use.

### Priority Order

Projects are executed in this priority order (re-prioritize with human approval):

| Priority | Plan | Rationale |
|----------|------|-----------|
| P1 | Plan 01 — Solar Mode Stability | Most complained-about feature, daily user impact |
| P1 | Plan 02 — Load Balancing | Unblocks upstream algorithm rewrite |
| P2 | Plan 08 — HA MQTT Integration | Quick wins, broken HA discovery |
| P2 | Plan 03 — OCPP Robustness | Active use case, freshly debugged |
| P3 | Plan 04 — EVCC Integration | Small scope, high community demand |
| P3 | Plan 05 — Meter Compatibility | Medium scope, extraction work |
| P4 | Plan 06 — Diagnostic Telemetry | Enables debugging 01/02/05 in production |
| P4 | Plan 07 — Web UI Modernization | Largest scope, least safety-critical |
| P4 | Plan 09 — Power Input Methods | Documentation + feature gaps, back of backlog |
| P3 | Plan 11 — OCPP Compatibility Testing | CI/CD interoperability testing with mock CSMS |
| P3 | Plan 12 — Modbus Compatibility Testing | CI/CD meter register validation across all 15+ types |

### Dependency Rules

- Plan 06 (Telemetry) should ideally start after Plan 01/02 to capture real
  diagnostic needs from the stabilization work
- Plan 07 (Web UI) depends on Plan 06 for the diagnostic viewer component
- Plan 04 (EVCC) depends on Plan 03 (OCPP) for shared API validation patterns
- Plan 11 (OCPP Compat) complements Plan 03 (OCPP Robustness): Plan 03 tests
  logic correctness, Plan 11 tests protocol compliance
- Plan 12 (Modbus Compat) complements Plan 05 (Meter Compatibility): Plan 05
  extracts code, Plan 12 tests it against real meter register layouts
- All plans depend on the test infrastructure being stable (currently: 44 suites, 900+ tests)

## Files You Should Know

| File | Purpose | Safety level |
|------|---------|-------------|
| `evse_state_machine.c` | Core state machine | CRITICAL — test every change |
| `evse_ctx.h` | State context struct | CRITICAL — field changes affect everything |
| `evse_bridge.cpp` | Global-to-context sync | HIGH — spinlock-protected |
| `esp32.cpp` | Firmware glue, callbacks | MEDIUM — dispatchers only |
| `mqtt_parser.c` | MQTT command parsing | MEDIUM — input validation |
| `http_api.c` | HTTP request validation | MEDIUM — input validation |
| `network_common.cpp` | WiFi, MQTT, HTTP server | LOW — network plumbing |
| `glcd.cpp` | LCD display | LOW — cosmetic only |
| `test/native/tests/` | All native test suites | HIGH — test integrity |
| `test/native/Makefile` | Test build system | HIGH — must compile all suites |

## What NOT to Do

- Do not refactor Arduino `String` to `char[]` in non-safety code — it works, and
  replacing it risks payload truncation with no benefit on ESP32's 320KB RAM.
- Do not add `#include` dependencies from `evse_state_machine.c` to Arduino or
  ESP-IDF headers — this module must compile natively.
- Do not add FreeRTOS task creation or stack allocation without checking the memory
  budget and documenting stack size rationale.
- Do not remove or weaken existing test assertions — tests are the safety net.
- Do not "improve" code that wasn't part of the assigned task. Stay focused.
- Do not manually commit generated files (`traceability-report.html`,
  `test-specification.md`) — CI auto-generates and commits both on every merge
  to master. Both are tracked in the repo.
