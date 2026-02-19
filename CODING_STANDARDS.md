# Coding Standards

This document codifies the conventions used across the SmartEVSE codebase.
See [CONTRIBUTING.md](CONTRIBUTING.md) for workflow and submission guidelines.

## C Code Style

- **Standard**: C11 (`-std=c11`)
- **Functions**: `snake_case` — e.g. `evse_state_machine_run()`, `get_charge_current()`
- **Global variables**: `CamelCase` — e.g. `ChargeDelay`, `ErrorFlags`, `State`
- **Macros / constants**: `UPPER_SNAKE_CASE` — e.g. `MAX_MAINS`, `STATE_A`
- **Typedefs / enums**: `CamelCase` — e.g. `EvseState`, `ProximityPin`
- **Compiler warnings**: `-Wall -Wextra -Wunused-variable` enabled for all targets
- **Formatting**: match surrounding code; no hard line-length limit but keep it readable

## Buffer Safety

- Use `snprintf` instead of `sprintf` — all buffer-writing functions must take a size
  parameter to prevent overflows.
- String formatting functions follow the pattern:
  ```c
  int format_status(char *buf, size_t len, ...);
  ```
- Never assume a buffer is "big enough" — always pass and check the size.

## FreeRTOS Conventions

- **Short critical sections**: use `portENTER_CRITICAL` / `portEXIT_CRITICAL` with
  spinlocks for shared-variable access (< 10 µs).
- **No heap allocation in ISRs or timer callbacks**: avoid `String`, `malloc`, or any
  function that may block.
- **Task priorities**: follow existing priority assignments; document deviations.
- **Spinlock pattern** (ESP32):
  ```c
  static portMUX_TYPE my_mux = portMUX_INITIALIZER_UNLOCKED;
  portENTER_CRITICAL(&my_mux);
  shared_var = new_value;
  portEXIT_CRITICAL(&my_mux);
  ```

## Platform Guards

The codebase targets both ESP32 (v3/v4) and CH32. Use the established guard pattern:

```c
#ifdef SMARTEVSE_VERSION
    // ESP32-specific code (SMARTEVSE_VERSION is 30 or 40)
#else
    // CH32-specific code
#endif
```

The bridge layer (`evse_state_machine.c` + `evse_bridge.h`) keeps platform-specific
code out of core logic. New state-machine logic should go through bridge functions
rather than using `#ifdef` directly.

## Module Extraction Pattern

When extracting testable modules from monolithic files, follow this pattern:

1. **Pure C modules for parsing/validation** — no Arduino or ESP32 dependencies:
   ```c
   // mqtt_parser.h — data types and pure functions
   bool mqtt_parse_command(const char *prefix, const char *topic,
                           const char *payload, mqtt_command_t *out);
   ```
   These compile and test natively with GCC on the host.

2. **Firmware glue layer** — thin dispatchers that call the pure module:
   ```cpp
   // In esp32.cpp — the callback becomes a thin wrapper
   void mqtt_receive_callback(const String topic, const String payload) {
       mqtt_command_t cmd;
       if (!mqtt_parse_command(prefix, topic.c_str(), payload.c_str(), &cmd))
           return;
       switch (cmd.cmd) { /* dispatch to setMode(), setOverrideCurrent(), etc. */ }
   }
   ```
   The glue layer handles Arduino types, globals, and hardware I/O.

3. **Snapshot structs** for decoupling from globals:
   ```c
   // http_api.h — snapshot of state needed by pure functions
   typedef struct {
       bool has_mode;  int mode;
       bool has_current; int current;
   } http_settings_request_t;
   ```
   Pure functions operate on snapshots, not global variables.

**Benefits**: Pure C modules are testable without hardware, have explicit
dependencies, and their validation logic can be verified at compile time
rather than discovered via field failures.

## Testing

- **Framework**: native C tests compiled with GCC, run on the host (no hardware needed).
- **SbE annotations**: all test functions must include Specification-by-Example tags:
  ```c
  // @feature State Machine
  // @req REQ-SM-001
  // @scenario Normal charging cycle
  // @given Vehicle connected in state B
  // @when Pilot duty cycle allows charging
  // @then State transitions to C and contactor closes
  void test_normal_charge_cycle(void) { ... }
  ```
- **All tests must pass** before submitting a PR: `make clean test`
- **Safety-critical changes** (state machine, contactors, current limiting) require
  thorough test coverage — add both positive and negative test cases.
- **BDD tests**: feature files in `test/native/features/` with step definitions in
  `test/native/steps/`.

## Versioning

- Format: `vMAJOR.MINOR.PATCH[-prerelease]` (e.g. `v3.11.0`, `v3.11.1-rc1`)
- The `VERSION` macro is defined in `platformio.ini` (`-DVERSION=...`) and falls back
  to a build timestamp in `debug.h` if not set.
- The `-dev` suffix marks unreleased development builds.
- CI validates that git tags match the VERSION in `platformio.ini` on release.
- See [CONTRIBUTING.md](CONTRIBUTING.md#versioning) for bump rules and release flow.
