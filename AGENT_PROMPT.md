# Plan 09 — Power Input Methods: Documentation & Feature Completeness

## Your Role: Network & MQTT Specialist + State Machine Specialist

You are implementing Plan 09: comprehensive documentation for all 5 power input
methods, plus feature gap fixes (API staleness detection, HomeWizard P1 energy data,
manual IP fallback, and diagnostic counters).

## Plan File

Read the full plan at: `/Users/basmeerman/Downloads/EVSE-team-planning/plan-09-power-input-methods.md`

## Branch & Workflow

- **Branch:** `work/plan-09`
- **Remote:** Push to `myfork` (basmeerman/SmartEVSE-3.5), NEVER to `origin`
- **Workflow:** Specification-first for code changes (SbE → test → implement → verify)
- **Verify after each code increment:** `cd SmartEVSE-3/test/native && make clean test`
- **Verify firmware builds:** `pio run -e release -d SmartEVSE-3/`
- **Commit and push after each increment** — do not batch multiple increments into one commit

## GitHub Issues (in order)

1. **#71** — Power Input Methods Documentation (Increment 1) — START HERE
2. **#72** — API/MQTT Staleness Detection (Increment 2)
3. **#73** — HomeWizard P1 Energy Data (Increment 3)
4. **#74** — HomeWizard P1 Manual IP Fallback (Increment 4)
5. **#75** — Diagnostic Counters (Increment 5)

## File Ownership — YOU OWN THESE

### New files (create these):
- `docs/power-input-methods.md` — Comprehensive power input guide (Increment 1)
- `SmartEVSE-3/test/native/tests/test_metering_diagnostics.c` — Diagnostic counter tests (Increment 5)

### Files you modify (YOUR sections only):
- `SmartEVSE-3/src/mqtt_parser.c` / `mqtt_parser.h` — Add staleness timeout parsing,
  HomeWizardIP command
- `SmartEVSE-3/src/evse_ctx.h` — Add `api_mains_last_update_ms`, `api_mains_timeout_ms`,
  diagnostic counter fields
- `SmartEVSE-3/src/evse_state_machine.c` — Staleness check in tick function (CRITICAL —
  must have tests)
- `SmartEVSE-3/test/native/Makefile` — Add test_metering_diagnostics build rule
- `README.md` — Add "Power Input Methods" to documentation table

## SHARED FILES — COORDINATE CAREFULLY

These files are also modified by Plan 06 and/or Plan 07:

- **`network_common.cpp`**: You modify `getMainsFromHomeWizardP1()` to read energy
  fields (Increment 3) and add HomeWizardIP manual fallback in mDNS discovery
  (Increment 4). Keep your changes in clearly marked sections with comments like
  `// BEGIN PLAN-09: HomeWizard energy data` and `// END PLAN-09`.

## Architecture Constraints

1. **CRITICAL: `evse_state_machine.c` changes MUST have tests** — this is safety-critical
2. Staleness detection must use pure C, testable natively
3. Use `snprintf` everywhere, never `sprintf`
4. All test functions MUST have SbE annotations (@feature, @req, @scenario, etc.)
5. Use `test_framework.h` (NOT unity.h)
6. New `evse_ctx.h` fields must be documented with comments
7. Never change existing MQTT topic names — only add new ones
8. Never change existing `/settings` JSON field names — only add new fields
9. Requirement prefix: `REQ-MTR-` for metering, `REQ-MQTT-` for MQTT

## Increment 1 Detailed Steps (Start Here — Documentation Only)

1. Read the plan's Section 1 (Power Input Method Analysis) thoroughly
2. Read existing code to verify the data flow descriptions:
   - `main.cpp`: `ModbusRequestLoop()` for Modbus RTU and Sensorbox
   - `esp32.cpp`: `homewizard_loop()` / `getMainsFromHomeWizardP1()` for HomeWizard
   - `network_common.cpp`: MQTT callback for API/MQTT feed
3. Create `docs/power-input-methods.md` with:
   - Reliability ranking table (most reliable first)
   - Decision tree flowchart (text-based)
   - Per-method setup guide (5 methods)
   - Comparison table
   - Troubleshooting section
4. Update `README.md` documentation table with link
5. Commit and push

## Increment 2 Detailed Steps (API/MQTT Staleness — Most Important Code Change)

1. Write SbE scenarios:
   - Given API metering, when no update for 120s, then fall back to MaxMains
   - Given API metering, when update received, then reset staleness timer
   - Given API metering, when staleness detected, then set diagnostic flag
   - Given non-API metering, then staleness check is skipped
2. Add fields to `evse_ctx.h`: `api_mains_last_update_ms`, `api_mains_timeout_ms`
3. Write tests in existing test file or new test file
4. Implement staleness check in `evse_state_machine.c`
5. Add MQTT parser for timeout setting
6. Run `make clean test`
7. Verify firmware builds

## Key References

- MQTT parser pattern: `src/mqtt_parser.c` (see existing `Set/MainsMeter` handler)
- HomeWizard code: search for `HomeWizard` in `esp32.cpp` and `network_common.cpp`
- Modbus metering: `main.cpp` `ModbusRequestLoop()`
- State machine: `src/evse_state_machine.c`
- Context struct: `src/evse_ctx.h`
- Test framework: `test/native/include/test_framework.h`
- Existing meter tests: `test/native/tests/test_meter_decode.c`
