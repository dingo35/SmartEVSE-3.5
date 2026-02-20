SmartEVSE v3
=========

Smart Electric Vehicle Charge Controller

![Image of SmartEVSE](/pictures/SmartEVSEv3.png)

# What is it?

It's an open source EVSE (Electric Vehicle Supply Equipment). It supports 1-3 phase charging, fixed charging cable or charging socket. Locking actuator support (5 different types). And it can directly drive a mains contactor for supplying power to the EV. It features a display from which all module parameters can be configured.<br>
Up to 8 modules can be connected together to charge up to eight EV's from one mains connection without overloading it.<br>
The mains connection can be monitored by the (optional) sensorbox or a modbus kWh meter. This allows smart charging.
Communication between the SmartEVSE(s) / Sensorbox or kWh meters is done over RS485(modbus).


# Features

- Works with all EV's or plugin hybrids.
- Measures the current consumption of other appliances, and automatically lowers or increases the charging current to the EV. (sensorbox required)
- The power sharing feature let's you connect up to 8 SmartEVSE's to one mains supply.
- Two switched 230VAC outputs, for contactors. Switch between 1 or 3 phase charging.
- Powered RS485 communication bus for sensorbox / Modbus kWh Meters.
- Can be used with fixed cable, or socket and charging cable.
- Automatically selects current capacity of the connected cable (13/16/32A)
- Locking actuator support, locks the charging cable in the socket.
- RFID reader support, restrict the use of the charging station to max 100 RFID cards.
- An optional modbus kWh meter will measure power and energy, and display this on the LCD.
- Built-in temperature sensor.
- RGB led output for status information while charging.
- All module parameters can be configured using the display and buttons.
- WiFi status page.
- Firmware upgradable through USB-C port or through the built in webserver.
- MQTT API for communication with external software (e.g. HomeAssistant)
- REST API
- Remote control with Smartphone App 
- Rudimentary support for home batteries
- Supports delayed charging
- OCPP 1.6j support
- Operating voltage: 110-240 Vac
- Dimensions (W x D x H):  52 x 91 x 58 mm (width: 3 DIN modules)

# Privacy first

- SmartEVSE will work perfectly fine without a internet connection.
- The controller does not collect or store usage statistics. Your data remains yours.
- No vendor lock-in. History has shown this can result in non-functional EV chargers.
- Open Source Firmware. Fork it, modify it, contribute to make it even better.

# Connecting the SmartEVSE to WiFi

For connecting your device to your WiFi, follow the detailed instructions
on the [Configuration page](docs/configuration.md#wifi) page, WIFI section.

# Updating Firmware

Connect the SmartEVSE controller to your WiFi network (using the menu of the SmartEVSE), and then browse to http://IPaddress/update where IPaddress is the IP which is shown on the display.
You can also use http://smartevse-xxxx.local/update where xxxx is the serial nr of your controller.<br>
Here you can select the firmware.bin and press update to update the firmware.<br>
After updating the firmware, you can access the status page again using the normal url: http://smartevse-xxxx.local  (replace xxxx with the serial nr of your controller)<br>

# Documentation

[Hardware installation](docs/installation.md)<br>
[Configuration](docs/configuration.md)<br>
[Operation](docs/operation.md)<br>
[Building and Flashing the firmware](docs/building_flashing.md)<br>
[REST API reference](docs/REST_API.md)<br>
[Coding standards](CODING_STANDARDS.md)<br>
[Contributing](CONTRIBUTING.md)<br>
[AI agent instructions (Claude Code)](CLAUDE.md)<br>
[AI agent instructions (GitHub Copilot)](.github/copilot-instructions.md)<br>

# Testing & Quality

The firmware is verified by a comprehensive native test suite that runs on the host
(no hardware required) and an 8-job CI pipeline.

| Metric | Value |
|--------|-------|
| Test suites | 19 |
| Test scenarios | 410 |
| Features covered | 31 |
| Requirement traceability | 100% |

**Test areas** include IEC 61851-1 state transitions, load balancing (single and
multi-node), Smart/Solar operating modes, OCPP current limiting, MQTT command
parsing, HTTP API validation, error handling & safety, modem/ISO15118 negotiation,
phase switching, and end-to-end charging flows.

Every test function carries Specification-by-Example (SbE) annotations (`@feature`,
`@req`, `@scenario`, `@given`/`@when`/`@then`) that trace back to requirements. The
CI pipeline generates two reports on every build:

- **[Test Specification](SmartEVSE-3/test/native/test-specification.md)** — Markdown
  document listing all scenarios grouped by feature, with requirement IDs and
  Given/When/Then steps. Auto-regenerated and committed on every merge to master.
- **[Traceability Report](SmartEVSE-3/test/native/traceability-report.md)** — Requirement-to-test
  matrix showing which tests cover each requirement, grouped by feature. Auto-regenerated
  and committed on every merge to master. Also attached to every GitHub release.

Additional CI artifacts:

| Artifact | Description |
|----------|-------------|
| `coverage-report` | Line coverage for the state machine module (lcov) |
| `traceability-reports` | HTML + Markdown specification reports |
| `bdd-report` | BDD feature test results (pytest-bdd HTML) |

To run the test suite locally:

```bash
cd SmartEVSE-3/test/native
make clean test
```

To generate the specification and traceability reports locally:

```bash
cd SmartEVSE-3/test/native
python3 scripts/extract_traceability.py --html traceability-report.html --markdown test-specification.md --markdown-report traceability-report.md
```

# SmartEVSE App

The SmartEVSE-app can be found [here](https://github.com/SmartEVSE/SmartEVSE-app) or on Google Play
