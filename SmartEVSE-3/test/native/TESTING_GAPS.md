# Native Test Suite — Known Gaps

## 1. Bridge sync concurrency (spinlock protection)

**Added**: Phase 4a (Feb 2025)

**What was implemented**:
`evse_bridge.cpp` wraps `evse_sync_globals_to_ctx()` and
`evse_sync_ctx_to_globals()` in `portENTER_CRITICAL` /
`portEXIT_CRITICAL` to prevent torn reads/writes when FreeRTOS tasks at
different priorities (Timer10ms pri-5, Timer100ms pri-3, Timer1S pri-3)
preempt each other mid-copy.

**Why it is not tested**:
The native test harness is single-threaded host C. The ESP32 spinlock
primitives (`portMUX_TYPE`, `portENTER_CRITICAL`) do not exist on the
host, and the `#ifdef SMARTEVSE_VERSION` guard compiles them out. There
is no way to trigger or observe a race condition in the current setup.

**What a test would look like**:
A pthread-based concurrency test that:
1. Provides a thin shim replacing `portENTER_CRITICAL` /
   `portEXIT_CRITICAL` with `pthread_mutex_lock` / `pthread_mutex_unlock`.
2. Spawns N writer threads calling `evse_sync_globals_to_ctx()` with
   known input patterns (e.g., all fields set to thread ID).
3. Spawns N reader threads calling `evse_sync_ctx_to_globals()` and
   verifying no torn values (e.g., fields from different thread IDs
   mixed in a single read).
4. Runs for a fixed duration or iteration count, flagging any
   inconsistency.

**Complexity**: Medium. Requires:
- A new build target in the Makefile linking with `-lpthread`.
- A `compat/freertos_shim.h` providing `portMUX_TYPE` as
  `pthread_mutex_t` and the critical-section macros as mutex ops.
- The bridge source (`evse_bridge.cpp`) compiled as C++ with the shim
  and test stubs for the extern globals.

**Risk if untested**: Low in practice. The critical sections are very
short (~1-2us of straight memory copies) and `portENTER_CRITICAL`
disables interrupts on the core, which is a well-proven ESP-IDF
primitive. The main risk is a future change that adds blocking calls
(e.g., logging, I/O) inside the critical section, which would cause
watchdog timeouts on the ESP32.

## 2. PROGMEM verification — confirmed not needed

**Added**: Phase 4b (Feb 2025)

**Finding**: ESP32 has a unified address space where `const` data is
automatically placed in flash by the compiler. PROGMEM is an AVR
(ATmega) construct that does not apply to ESP32/ESP-IDF.

All large const arrays are correctly declared:
- `const unsigned char font[][5]` and `font2[][23]` (~3.5-7KB) in `font.cpp`/`font2.cpp`
- `const unsigned char LCD_Flow[]` (~320 bytes) in `glcd.cpp`
- `const char StrStateNameWeb[][17]` etc. in `esp32.cpp`
- `const uint8_t crc_table[]` (256 bytes) in `OneWireESP32.cpp`

**Status**: No action needed. Verified correct.

## 3. NTP `timeinfo` struct — unsynchronized reads

**Added**: Phase 4b (Feb 2025)

**Design**: The `struct tm timeinfo` global is written by the SNTP
callback (runs in LwIP/FreeRTOS task context) and read by Timer10ms,
Timer1S, and GLCD without synchronization. A torn read could produce
briefly garbled hour/minute values.

**Why no fix is needed**: `timeinfo` is only used for LCD display and
log timestamps. It never influences contactor control, CP duty cycle,
state machine transitions, or any safety-critical decision. The worst
case is a single log line or LCD frame showing e.g. "23:60" before
the next correct update.

**Risk**: Cosmetic only. No safety impact.

## 4. Arduino String class — technical debt (partially addressed in Phase 4d)

**Added**: Phase 4b (Feb 2025)
**Updated**: Phase 4d (Feb 2025)

**Finding**: ~200 locations use Arduino `String` across MQTT publishing,
web handlers, JSON building, and network code. Key observations:

- ISRs and the state machine are String-free (confirmed safe)
- String operations run in Arduino loop task and network tasks, not
  in the timer tasks that control CP/contactors
- Heap fragmentation from String is theoretical in this architecture

**What was hardened in Phase 4b**:
- All `sprintf` → `snprintf` conversions across `glcd.cpp`, `esp32.cpp`,
  `network_common.cpp`, and `utils.cpp`
- `printRFID()` and `sprintfl()` now take buffer size parameters
- Defense-in-depth against buffer overflows even where current sizes
  are known to be safe

**What Phase 4d addressed**:
- MQTT command parsing extracted to `mqtt_parser.c` — pure C, no String
- HTTP API validation extracted to `http_api.c` — pure C, no String
- New parsing modules use `const char*`, `sscanf()`, `strcmp()` — no
  Arduino String dependency
- Arduino String remains in the firmware glue layer (callbacks, JSON
  building with ArduinoJson) where it was already working correctly

**Remaining**: ~108 String instances in non-safety code (MQTT publish,
ArduinoJson, WiFi). Replacing these offers no measurable benefit on
ESP32's 320KB RAM and risks payload truncation bugs.

## 5. Timer20ms stack size — needs hardware measurement

**Added**: Phase 4b (Feb 2025)

**Change**: Reduced from 40KB to 10KB (conservative, with 25% margin).
Added one-time `uxTaskGetStackHighWaterMark()` log in `qca.cpp`.

**Why not testable natively**: Timer20ms is v4-only (`SMARTEVSE_VERSION >= 40`)
and uses SPI hardware for QCA700X modem communication. Stack usage
depends on the actual call depth through `SlacManager()` and
`IPv6Manager()` which interact with real modem hardware.

**Verification**: Flash v4 hardware, check serial log for
"Timer20ms stack high water mark: N bytes free". If N < 2048,
increase stack size. If N > 5000, can reduce further.

## 6. MQTT/HTTP endpoint testing — addressed in Phase 4d

**Added**: Phase 4d (Feb 2025)

**Previous gap**: MQTT `mqtt_receive_callback()` and HTTP `handle_URI()`
had zero native tests. These functions parse safety-relevant inputs
(charging current, mode, cable lock) from network payloads.

**What was done**:
- Extracted MQTT parsing to `mqtt_parser.c` — pure C, testable natively
- Extracted HTTP validation to `http_api.c` — pure C, testable natively
- Added `test_mqtt_parser.c` with 50 test cases covering all 16 MQTT
  command topics and their validation boundaries
- Added `test_http_api.c` with 37 test cases covering settings validation,
  color parsing, current limits, and combined validation scenarios

**Status**: Addressed. The parsing/validation logic is fully covered.
The firmware dispatch layer (writing globals, calling `setMode()` etc.)
remains tested via integration testing on hardware.

## 7. Serial message parsing and current calculations — addressed in Phase 4d

**Added**: Phase 4d (Feb 2025)

**Previous gap**: `ReadIrms()`, `ReadPowerMeasured()`, `receiveNodeStatus()`,
`getBatteryCurrent()`, and `CalcIsum()` in `main.cpp` contained interleaved
parsing logic and hardware side effects with zero native tests.

**What was done**:
- Extracted serial parsing to `serial_parser.c` — pure C, testable natively
- `serial_parse_irms()`, `serial_parse_power()`, `serial_parse_node_status()`
  parse structured messages into typed structs without side effects
- `calc_battery_current()` and `calc_isum()` perform pure current calculations
- Added `test_serial_parser.c` with 31 test cases covering all parsers,
  battery current logic, and Isum calculation with battery adjustment

**Status**: Addressed. Parsing and calculation logic fully covered.
Hardware dispatch (writing to Meter objects, setTimeout) remains in main.cpp.

## 8. LED color computation — addressed in Phase 4d

**Added**: Phase 4d (Feb 2025)

**Previous gap**: `BlinkLed_singlerun()` in `main.cpp` (135 lines) mixed
pure state→color logic with hardware PWM writes. The color determination
logic had zero native tests despite handling error indication, mode-based
colors, and breathing animations.

**What was done**:
- Extracted LED color logic to `led_color.c` — pure C, testable natively
- `led_compute_color()` takes a state snapshot and animation context,
  returns RGB values without hardware writes
- Handles CH32 vs ESP32 error detection differences via `is_ch32` flag
- Added `test_led_color.c` with 19 test cases covering error flashing,
  access OFF states, waiting/charge delay blinks, State A/B/C brightness,
  breathing animation, mode colors, and custom button overrides
- OCPP LED overrides remain in `BlinkLed_singlerun()` (depend on millis())

**Status**: Addressed. Base color computation fully covered.
