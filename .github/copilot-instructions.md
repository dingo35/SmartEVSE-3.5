# GitHub Copilot — Project Instructions

These instructions apply to GitHub Copilot, Copilot Chat, Copilot Workspace, and
any Copilot-powered agent working on the SmartEVSE codebase.

## Project Context

SmartEVSE is safety-critical embedded firmware for an EV charging controller running
on ESP32 and CH32 microcontrollers. Incorrect code can damage vehicles, trip breakers,
or cause electrical hazards. Treat every change as if it controls a contactor
switching 32A at 400V — because it does.

## Mandatory Standards

### Coding Conventions

These are enforced by CI. Violations fail the build.

- **Functions**: `snake_case` — `evse_tick_10ms()`, `mqtt_parse_command()`
- **Global variables**: `CamelCase` — `ChargeDelay`, `ErrorFlags`, `State`
- **Macros/constants**: `UPPER_SNAKE_CASE` — `MAX_MAINS`, `STATE_A`, `MODE_SOLAR`
- **Typedefs/enums**: `CamelCase` — `EvseState`, `mqtt_command_t`
- **C standard**: C11 for `.c` files
- **Never use `sprintf`** — always `snprintf(buf, sizeof(buf), ...)`
- **Never allocate heap in ISRs or critical sections** — no `malloc`, `new`, `String`
- **Compiler warnings**: `-Wall -Wextra -Wunused-variable`, zero warnings

### Architecture Rules

1. **`evse_state_machine.c` is pure C.** No Arduino, ESP-IDF, or FreeRTOS includes.
   It must compile with plain `gcc` on any host. All hardware interaction goes
   through `evse_ctx_t` and `evse_hal_t` callbacks.

2. **Bridge pattern for globals.** Firmware globals are synced to/from `evse_ctx_t`
   via `evse_bridge.cpp` inside spinlock-protected critical sections. Never read or
   write `evse_ctx_t` from outside the bridge without synchronization.

3. **Pure C modules for parsing/validation.** `mqtt_parser.c` and `http_api.c` have
   no Arduino dependencies. New parsing or validation logic must follow the same
   pattern — pure C with `extern "C"` guards in the header.

4. **Thin glue layer.** `esp32.cpp` dispatches MQTT/HTTP callbacks to pure modules.
   Push logic into the pure modules, keep the glue minimal.

5. **Platform guards.** Use `#ifdef SMARTEVSE_VERSION` only in bridge/glue layers,
   never in `evse_state_machine.c` or parser modules.

### Test-First Workflow

Every code change must follow this sequence:

```
1. Write SbE specification (Given/When/Then)
2. Write the test with @feature, @req, @scenario annotations
3. Implement the code change
4. Run: cd SmartEVSE-3/test/native && make clean test
5. Verify: pio run -e release -d SmartEVSE-3/
```

**Never write implementation code without a corresponding test.**

### SbE Test Annotations

Every test function must have this comment block:

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

Requirement ID prefixes: `REQ-SM-` (state machine), `REQ-ERR-` (error/safety),
`REQ-LB-` (load balancing), `REQ-SOL-` (solar), `REQ-MQTT-` (MQTT),
`REQ-API-` (HTTP), `REQ-AUTH-` (authorization), `REQ-MOD-` (modem),
`REQ-PH-` (phase switching), `REQ-PWR-` (power), `REQ-E2E-` (end-to-end).

Check `SmartEVSE-3/test/native/test-specification.md` for existing IDs.

### Defensive Programming

- Validate all external inputs (MQTT payloads, HTTP parameters, Modbus data)
  before passing to the state machine or writing to globals.
- Use bounds checking on array indices, especially `BalancedState[]` (0-7 nodes),
  mains phase arrays (0-2), and RFID arrays.
- Check return values from `snprintf` — if it returns >= buffer size, the output
  was truncated.
- Never trust payload lengths from network sources. Always cap to buffer size.

### Memory Awareness

- ESP32: 1,640KB flash (95% budget), 288KB RAM (90% budget)
- CH32: 61KB flash (95% budget), 18KB RAM (90% budget)
- New static buffers must be justified. Prefer stack allocation for temporary buffers.
- Test code compiles only in the native test harness, never in firmware builds.
- Arduino `String` is acceptable in non-safety glue code (MQTT publish, JSON).
  Do not convert existing `String` usage to `char[]` — it risks truncation bugs.

## File Safety Levels

| Risk | Files | Rule |
|------|-------|------|
| CRITICAL | `evse_state_machine.c`, `evse_ctx.h` | Must add tests for any change |
| HIGH | `evse_bridge.cpp`, test files, Makefile | Review carefully |
| MEDIUM | `mqtt_parser.c`, `http_api.c`, `esp32.cpp` | Follow module patterns |
| LOW | `glcd.cpp`, `network_common.cpp` | Standard care |

## Code Generation Guidelines

When generating code for this project:

- Match the style of surrounding code exactly
- Use `Arrange / Act / Assert` pattern in tests with clear comments
- Include units in variable names or comments: `current_dA` (deci-amps),
  `power_W` (watts), `energy_Wh` (watt-hours)
- Prefer explicit state over implicit: set all relevant context fields in tests
- Add negative test cases for every positive case (invalid input, boundary values)
- When suggesting fixes to `evse_state_machine.c`, always include the test diff

## Multi-Agent Coordination

When Copilot Workspace or multiple agents operate concurrently:

- One agent must act as **Quality Guardian** — reviewing all changes for standard
  compliance, running tests, and verifying builds. It does not write implementation
  code.
- Implementation agents work on non-overlapping files.
- No agent modifies `evse_state_machine.c` without also modifying test files.
- All agents must read `CODING_STANDARDS.md` and `CONTRIBUTING.md` before starting.
- Changes are merged sequentially, not simultaneously, to avoid conflicts.

## Quick Reference

```bash
# Run tests
cd SmartEVSE-3/test/native && make clean test

# Build firmware
pio run -e release -d SmartEVSE-3/
pio run -e ch32 -d SmartEVSE-3/

# Regenerate test spec
cd SmartEVSE-3/test/native && python3 scripts/extract_traceability.py \
  --markdown test-specification.md

# Run with memory sanitizers
cd SmartEVSE-3/test/native && make clean test \
  CFLAGS_EXTRA="-fsanitize=address,undefined -fno-omit-frame-pointer"
```
