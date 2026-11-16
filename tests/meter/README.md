# OCPP meter startup regression (#421)

Run `sh tests/meter/run.sh` from the repository root. This host test compiles
the production `meter.cpp` with hardware dependencies stubbed out. It checks
startup, current/power/export-only responses, a non-response, a valid non-zero
import reading, a valid zero, reboot and reversed Eastron wiring.

Build the firmware using `pio run -d SmartEVSE-3`. Also build with
`PLATFORMIO_BUILD_FLAGS='-DDBG=1' pio run -d SmartEVSE-3`, as in CI.

## Test-bench checks

Use a test bench, not an installed charger, following the warning in
`SmartEVSE-3/test/tests.sh`. The host test does not exercise contactors or OCPP.

1. Configure Eastron3P and OCPP auto-authorization; connect an EV simulator.
   Reboot while delaying the import-register response. Keep current responses
   flowing. Check that no StartTransaction is sent and the contactor stays open.
2. Supply a known cumulative import reading with zero export. Check that
   StartTransaction uses that value as meterStart and charging can start.
   Repeat with a measured zero.
3. Repeat with the meter disconnected: charging must remain pending.
4. Repeat with Eastron3P inverted and with local RFID authorization. Also test
   MicroOcpp's TxStartOnPowerPathClosed setting; it must not bypass the gate.
5. For API metering, send current-only updates, then an energy update through
   REST and MQTT. Only the energy update should release the gate. Check the
   same startup sequence with a HomeWizard meter.
6. Stop after a known energy increase. Check meterStop - meterStart. Repeat
   with a second session and an offline OCPP connection after authorization.
7. With OCPP disabled, check that Normal mode can still charge without a
   network connection. With OCPP enabled but no EV meter configured, check
   that session startup retains its existing behavior.

The flag records the first import reading since boot. It does not replace the
existing communication timeout or add per-transaction freshness monitoring.
The OCPP callback still returns `EVMeter.Energy` (import minus export); changing
that accounting convention is outside this startup fix. The MQTT/API
zero-value suppression introduced in #406 is unchanged.
