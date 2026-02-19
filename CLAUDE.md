# Claude Code — Project Instructions

This file configures Claude Code (and any Claude-based agent) when working on the
SmartEVSE codebase. Read this entire file before making any changes.

## Project Overview

SmartEVSE is an open-source Electric Vehicle Supply Equipment (EVSE) controller.
The firmware runs on ESP32 (v3/v4) and CH32 microcontrollers. This is safety-critical
embedded software — incorrect behavior can damage vehicles, trip breakers, or cause
electrical hazards.

## Critical Rules

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

# Build ESP32 firmware
pio run -e release -d SmartEVSE-3/

# Build CH32 firmware
pio run -e ch32 -d SmartEVSE-3/

# Regenerate test specification
cd SmartEVSE-3/test/native && python3 scripts/extract_traceability.py \
  --markdown test-specification.md --html traceability-report.html

# Run with sanitizers (catches memory bugs)
cd SmartEVSE-3/test/native && make clean test \
  CFLAGS_EXTRA="-fsanitize=address,undefined -fno-omit-frame-pointer"
```

## Multi-Agent Workflow

When multiple AI agents work on this codebase simultaneously, one agent MUST act
as the **Quality Guardian**. This applies to Claude Code with parallel Task agents,
multi-agent orchestration, or any setup where more than one agent writes code.

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
- Follow the specification-first workflow (SbE -> test -> code)
- Not modify files outside their assigned scope without coordination
- Not commit directly — submit changes for Quality Guardian review
- Not modify `evse_state_machine.c` without adding tests in the same changeset
- Run `make clean test` locally before declaring work complete

### Coordination Protocol

When dividing work across agents:

- Assign non-overlapping files to each implementation agent
- The Quality Guardian reviews all changes in sequence, not in parallel
- If two agents need to modify the same file, one waits for the other to finish
- Merge conflicts are resolved by the Quality Guardian, preserving both changes
- Each agent's changes are verified independently before combining

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
