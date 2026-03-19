# SmartEVSE-3 Test Specification

**63 features** | **945 scenarios** | **945 with requirement IDs**

---

## Table of Contents

1. [API Mains Staleness Detection](#api-mains-staleness-detection)
2. [HomeWizard P1 Manual IP Fallback](#homewizard-p1-manual-ip-fallback)
3. [Authorization & Access Control](#authorization-&-access-control)
4. [Bridge Transaction Integrity](#bridge-transaction-integrity)
5. [Diagnostic Telemetry](#diagnostic-telemetry)
6. [Dual-EVSE Load Balancing](#dual-evse-load-balancing)
7. [End-to-End Charging](#end-to-end-charging)
8. [Error Handling & Safety](#error-handling-&-safety)
9. [Fidelity: DisconnectTimeCounter](#fidelity:-disconnecttimecounter)
10. [Fidelity: PilotDisconnectTime](#fidelity:-pilotdisconnecttime)
11. [Fidelity: Fall-through behavior](#fidelity:-fall-through-behavior)
12. [Fidelity: ACTSTART no pilot check](#fidelity:-actstart-no-pilot-check)
13. [Fidelity: Modem states not in tick_10ms](#fidelity:-modem-states-not-in-tick_10ms)
14. [Fidelity: Handler ordering](#fidelity:-handler-ordering)
15. [Fidelity: Config field](#fidelity:-config-field)
16. [HTTP API Color Parsing](#http-api-color-parsing)
17. [HTTP API Input Validation](#http-api-input-validation)
18. [HTTP API Validation](#http-api-validation)
19. [HTTP API Settings Validation](#http-api-settings-validation)
20. [EVCC IEC 61851 State Mapping](#evcc-iec-61851-state-mapping)
21. [EVCC Charging Enabled](#evcc-charging-enabled)
22. [EVCC Phase Switch Validation](#evcc-phase-switch-validation)
23. [LB Convergence](#lb-convergence)
24. [LED Status Indication](#led-status-indication)
25. [LED Color Configuration](#led-color-configuration)
26. [Load Balancing](#load-balancing)
27. [Meter Decoding](#meter-decoding)
28. [Meter Timeout & Recovery](#meter-timeout-&-recovery)
29. [Meter Telemetry](#meter-telemetry)
30. [Metering Diagnostics](#metering-diagnostics)
31. [Modbus Frame Decoding](#modbus-frame-decoding)
32. [Modbus Frame Logging](#modbus-frame-logging)
33. [Modem / ISO15118 Negotiation](#modem--iso15118-negotiation)
34. [MQTT Command Parsing](#mqtt-command-parsing)
35. [MQTT Input Validation](#mqtt-input-validation)
36. [MQTT Meter Parsing](#mqtt-meter-parsing)
37. [MQTT Color Parsing](#mqtt-color-parsing)
38. [Solar Debug Telemetry](#solar-debug-telemetry)
39. [MQTT Change-Only Publishing](#mqtt-change-only-publishing)
40. [Multi-Node Load Balancing](#multi-node-load-balancing)
41. [OCPP Current Limiting](#ocpp-current-limiting)
42. [OCPP Authorization](#ocpp-authorization)
43. [OCPP Connector State](#ocpp-connector-state)
44. [OCPP IEC 61851 Status Mapping](#ocpp-iec-61851-status-mapping)
45. [OCPP Load Balancing Exclusivity](#ocpp-load-balancing-exclusivity)
46. [OCPP RFID Formatting](#ocpp-rfid-formatting)
47. [OCPP Settings Validation](#ocpp-settings-validation)
48. [OCPP Telemetry](#ocpp-telemetry)
49. [Operating Modes](#operating-modes)
50. [P1 Meter Parsing](#p1-meter-parsing)
51. [Phase Switching](#phase-switching)
52. [Power Availability](#power-availability)
53. [Priority-Based Power Scheduling](#priority-based-power-scheduling)
54. [Serial Message Parsing](#serial-message-parsing)
55. [Serial Input Validation](#serial-input-validation)
56. [Battery Current Calculation](#battery-current-calculation)
57. [Current Sum Calculation](#current-sum-calculation)
58. [Charge Session Logging](#charge-session-logging)
59. [Charge Session JSON Export](#charge-session-json-export)
60. [Solar Balancing](#solar-balancing)
61. [IEC 61851-1 State Transitions](#iec-61851-1-state-transitions)
62. [10ms Tick Processing](#10ms-tick-processing)
63. [1-Second Tick Processing](#1-second-tick-processing)

## API Mains Staleness Detection

### Staleness timer counts down each second and sets stale flag on expiry

**Requirement:** `REQ-MTR-020`

- **Given** EVSE in Smart mode with MainsMeterType=EM_API_METER and api_mains_timeout=3
- **When** 3 seconds elapse with no data update
- **Then** api_mains_stale is set to true

> Test: `test_staleness_timer_countdown` in `test_api_staleness.c:1`

### When API data goes stale, all phases fall back to MaxMains

**Requirement:** `REQ-MTR-021`

- **Given** EVSE in Smart mode with API metering, MaxMains=25, current readings are 10A/phase
- **When** Staleness timer expires
- **Then** MainsMeterIrms for all 3 phases is set to MaxMains * 10 (250 dA)

> Test: `test_staleness_fallback_to_maxmains` in `test_api_staleness.c:58`

### Stale flag is cleared when staleness timer is reset (data received)

**Requirement:** `REQ-MTR-022`

- **Given** EVSE in API mode with stale data (api_mains_stale=true, timer=0)
- **When** External code resets api_mains_staleness_timer to a positive value
- **Then** api_mains_stale is cleared on the next tick

> Test: `test_staleness_recovery_on_timer_reset` in `test_api_staleness.c:92`

### Staleness check is skipped for non-API metering modes

**Requirement:** `REQ-MTR-023`

- **Given** EVSE in Smart mode with MainsMeterType=1 (Sensorbox) and api_mains_timeout=120
- **When** Staleness timer is 0
- **Then** api_mains_stale remains false

> Test: `test_staleness_skipped_for_non_api` in `test_api_staleness.c:118`

### Staleness detection is disabled when api_mains_timeout is 0

**Requirement:** `REQ-MTR-024`

- **Given** EVSE in Smart mode with API metering and api_mains_timeout=0
- **When** Staleness timer reaches 0
- **Then** api_mains_stale remains false, normal CT_NOCOMM applies

> Test: `test_staleness_disabled_when_timeout_zero` in `test_api_staleness.c:142`

### CT_NOCOMM is suppressed when API staleness detection is active

**Requirement:** `REQ-MTR-025`

- **Given** EVSE in Smart mode with API metering, staleness enabled, MainsMeterTimeout=0
- **When** A 1-second tick occurs
- **Then** CT_NOCOMM is NOT set (staleness mechanism handles the timeout instead)

> Test: `test_ct_nocomm_suppressed_for_api_with_staleness` in `test_api_staleness.c:166`

### CT_NOCOMM fires normally when staleness detection is disabled for API mode

**Requirement:** `REQ-MTR-026`

- **Given** EVSE in Smart mode with API metering, api_mains_timeout=0, MainsMeterTimeout=0
- **When** A 1-second tick occurs
- **Then** CT_NOCOMM IS set (normal timeout behavior)

> Test: `test_ct_nocomm_fires_when_staleness_disabled` in `test_api_staleness.c:190`

### Stale flag is set only once, not repeatedly overwriting Irms each tick

**Requirement:** `REQ-MTR-027`

- **Given** EVSE in API mode, already stale
- **When** Another tick occurs while stale
- **Then** Irms values are not overwritten again (flag already set)

> Test: `test_staleness_only_fires_once` in `test_api_staleness.c:214`

### Parse MainsMeterTimeout MQTT command with valid value

**Requirement:** `REQ-MQTT-030`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/MainsMeterTimeout with payload "120"
- **Then** Command type is MQTT_CMD_MAINS_METER_TIMEOUT with value 120

> Test: `test_mqtt_parse_staleness_timeout_valid` in `test_api_staleness.c:247`

### Parse MainsMeterTimeout MQTT command with 0 (disabled)

**Requirement:** `REQ-MQTT-031`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/MainsMeterTimeout with payload "0"
- **Then** Command type is MQTT_CMD_MAINS_METER_TIMEOUT with value 0

> Test: `test_mqtt_parse_staleness_timeout_disabled` in `test_api_staleness.c:262`

### Reject MainsMeterTimeout values outside valid range

**Requirement:** `REQ-MQTT-032`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/MainsMeterTimeout with payload "5" (below minimum 10)
- **Then** Parsing returns false

> Test: `test_mqtt_parse_staleness_timeout_too_low` in `test_api_staleness.c:277`

### Reject MainsMeterTimeout values above maximum

**Requirement:** `REQ-MQTT-033`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/MainsMeterTimeout with payload "4000" (above maximum 3600)
- **Then** Parsing returns false

> Test: `test_mqtt_parse_staleness_timeout_too_high` in `test_api_staleness.c:290`

---

## HomeWizard P1 Manual IP Fallback

### Parse HomeWizardIP MQTT command with valid IP

**Requirement:** `REQ-MQTT-034`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/HomeWizardIP with payload "192.168.1.50"
- **Then** Command type is MQTT_CMD_HOMEWIZARD_IP with the IP string

> Test: `test_mqtt_parse_homewizard_ip_valid` in `test_api_staleness.c:305`

### Parse HomeWizardIP with empty string clears manual IP (re-enable mDNS)

**Requirement:** `REQ-MQTT-035`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/HomeWizardIP with empty payload
- **Then** Command type is MQTT_CMD_HOMEWIZARD_IP with empty string

> Test: `test_mqtt_parse_homewizard_ip_empty` in `test_api_staleness.c:320`

### Reject HomeWizardIP that exceeds buffer size

**Requirement:** `REQ-MQTT-036`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/HomeWizardIP with a 16+ character payload
- **Then** Parsing returns false

> Test: `test_mqtt_parse_homewizard_ip_too_long` in `test_api_staleness.c:335`

---

## Authorization & Access Control

### Setting access to ON stores the authorization status

**Requirement:** `REQ-AUTH-001`

- **Given** The EVSE is initialised in basic configuration
- **When** evse_set_access is called with ON
- **Then** AccessStatus is set to ON

> Test: `test_set_access_on` in `test_authorization.c:1`

### Setting access to OFF stores the authorization status

**Requirement:** `REQ-AUTH-002`

- **Given** The EVSE is initialised in basic configuration
- **When** evse_set_access is called with OFF
- **Then** AccessStatus is set to OFF

> Test: `test_set_access_off` in `test_authorization.c:40`

### Revoking access during charging suspends the session (C -> C1)

**Requirement:** `REQ-AUTH-003`

- **Given** The EVSE is in STATE_C (charging) with AccessStatus ON
- **When** evse_set_access is called with OFF
- **Then** The state transitions to STATE_C1 and AccessStatus is OFF

> Test: `test_set_access_off_from_C_goes_C1` in `test_authorization.c:54`

### Pausing access during charging suspends the session (C -> C1)

**Requirement:** `REQ-AUTH-004`

- **Given** The EVSE is in STATE_C (charging) with AccessStatus ON
- **When** evse_set_access is called with PAUSE
- **Then** The state transitions to STATE_C1 and AccessStatus is PAUSE

> Test: `test_set_access_pause_from_C_goes_C1` in `test_authorization.c:73`

### Revoking access in STATE_B moves to waiting state (B -> B1)

**Requirement:** `REQ-AUTH-005`

- **Given** The EVSE is in STATE_B (connected) with AccessStatus ON
- **When** evse_set_access is called with OFF
- **Then** The state transitions to STATE_B1 (waiting)

> Test: `test_set_access_off_from_B_goes_B1` in `test_authorization.c:91`

### Revoking access during modem request aborts to B1

**Requirement:** `REQ-AUTH-006`

- **Given** The EVSE is in STATE_MODEM_REQUEST with AccessStatus ON
- **When** evse_set_access is called with OFF
- **Then** The state transitions to STATE_B1 (waiting)

> Test: `test_set_access_off_from_modem_request_goes_B1` in `test_authorization.c:108`

### Revoking access during modem wait aborts to B1

**Requirement:** `REQ-AUTH-007`

- **Given** The EVSE is in STATE_MODEM_WAIT with AccessStatus ON
- **When** evse_set_access is called with OFF
- **Then** The state transitions to STATE_B1 (waiting)

> Test: `test_set_access_off_from_modem_wait_goes_B1` in `test_authorization.c:125`

### Revoking access in STATE_A has no state transition side effect

**Requirement:** `REQ-AUTH-008`

- **Given** The EVSE is in STATE_A (disconnected)
- **When** evse_set_access is called with OFF
- **Then** The state remains STATE_A

> Test: `test_set_access_off_from_A_stays_A` in `test_authorization.c:142`

### Granting access in STATE_B1 does not auto-recover to STATE_B

**Requirement:** `REQ-AUTH-009`

- **Given** The EVSE is in STATE_B1 (waiting)
- **When** evse_set_access is called with ON
- **Then** The state remains STATE_B1 (no automatic recovery)

> Test: `test_set_access_on_from_B1_does_not_auto_recover` in `test_authorization.c:157`

### OCPP current limit below MinCurrent blocks current availability

**Requirement:** `REQ-AUTH-010`

- **Given** OCPP mode is enabled with OcppCurrentLimit=3A and MinCurrent=6A
- **When** evse_is_current_available is called
- **Then** The function returns 0 (current not available)

> Test: `test_ocpp_blocks_current_availability` in `test_authorization.c:175`

### OCPP current limit above MinCurrent allows current availability

**Requirement:** `REQ-AUTH-011`

- **Given** OCPP mode is enabled with OcppCurrentLimit=10A and MinCurrent=6A
- **When** evse_is_current_available is called
- **Then** The function returns 1 (current available)

> Test: `test_ocpp_allows_when_limit_sufficient` in `test_authorization.c:192`

### Negative OCPP current limit is ignored (not set)

**Requirement:** `REQ-AUTH-012`

- **Given** OCPP mode is enabled with OcppCurrentLimit=-1 (unset)
- **When** evse_is_current_available is called
- **Then** The function returns 1 (limit not applied)

> Test: `test_ocpp_negative_limit_ignored` in `test_authorization.c:210`

### Access timer counts down each second while in STATE_A

**Requirement:** `REQ-AUTH-014`

- **Given** The EVSE has AccessStatus ON, RFIDReader=2, and AccessTimer=0
- **Given** The EVSE is in STATE_A with AccessTimer=5
- **When** A 12V pilot signal is received (EV disconnects)
- **When** One second tick occurs
- **Then** AccessTimer is set to RFIDLOCKTIME
- **Then** AccessTimer decrements to 4

> Test: `test_access_timer_counts_down_in_state_A` in `test_authorization.c:229`

### Access timer expiry revokes authorization

**Requirement:** `REQ-AUTH-015`

- **Given** The EVSE is in STATE_A with AccessTimer=1 and AccessStatus ON
- **When** One second tick occurs (timer reaches 0)
- **Then** AccessStatus is set to OFF (authorization revoked)

> Test: `test_access_timer_expires_turns_off` in `test_authorization.c:266`

### Access timer is cleared when EVSE is not in STATE_A

**Requirement:** `REQ-AUTH-016`

- **Given** The EVSE is in STATE_B with AccessTimer=30
- **When** One second tick occurs
- **Then** AccessTimer is reset to 0

> Test: `test_access_timer_cleared_when_not_in_A` in `test_authorization.c:284`

### No STATE_A to STATE_B transition without authorization

**Requirement:** `REQ-AUTH-017`

- **Given** The EVSE is in STATE_A with AccessStatus OFF
- **When** A 9V pilot signal is received (vehicle connected)
- **Then** The state remains STATE_A (transition blocked)

> Test: `test_no_A_to_B_without_access` in `test_authorization.c:303`

### No STATE_B to STATE_C transition after access revoked mid-session

**Requirement:** `REQ-AUTH-018`

- **Given** The EVSE is in STATE_B with DiodeCheck passed but AccessStatus revoked to OFF
- **When** A 6V pilot signal is sustained for 500ms
- **Then** The state does NOT transition to STATE_C

> Test: `test_no_B_to_C_without_access` in `test_authorization.c:318`

### AccessStatus is cleared immediately when a charging session ends

**Requirement:** `REQ-AUTH-019`

- **Given** EVSE is in STATE_C with AccessStatus ON and RFID reader enabled (EnableOne)
- **When** PILOT_12V is received (car disconnects — e.g. Tesla door handle)
- **Then** AccessStatus is OFF and AccessTimer is 0 immediately upon transition to STATE_A

> Test: `test_access_status_cleared_on_session_end` in `test_authorization.c:340`

### AccessStatus is cleared immediately when session ends from STATE_C1

**Requirement:** `REQ-AUTH-020`

- **Given** EVSE is in STATE_C1 (winding down charge) with AccessStatus ON
- **When** PILOT_12V is received (car disconnects during C1 wind-down)
- **Then** AccessStatus is OFF and AccessTimer is 0 on transition to STATE_A

> Test: `test_access_status_cleared_on_session_end_from_c1` in `test_authorization.c:365`

### AccessStatus cleared on Tesla-style disconnect (C → B → A)

**Requirement:** `REQ-AUTH-021`

- **Given** EVSE is in STATE_C with AccessStatus ON and RFID reader enabled
- **When** Car transitions to STATE_B (CP → 9V), then to STATE_A (CP → 12V)
- **Then** AccessStatus is OFF and AccessTimer is 0 upon reaching STATE_A

> Test: `test_access_status_cleared_on_tesla_disconnect_c_b_a` in `test_authorization.c:390`

### AccessStatus cleared on solar-stop disconnect (C1 → B1 → A)

**Requirement:** `REQ-AUTH-022`

- **Given** EVSE is in STATE_B1 (transitioned from C1 after solar stop) with AccessStatus ON
- **When** Pilot disconnect timer expires, then car disconnects (CP → 12V)
- **Then** AccessStatus is OFF and AccessTimer is 0 upon reaching STATE_A

> Test: `test_access_status_cleared_on_disconnect_from_b1` in `test_authorization.c:420`

### Tesla disconnect then new car + RFID swipe starts session correctly

**Requirement:** `REQ-AUTH-023`

- **Given** EVSE charged Car A in STATE_C, RFIDReader=EnableOne, AccessStatus=ON
- **When** Car A does Tesla-style disconnect (C→B→A), Car B plugs in, user swipes RFID
- **Then** Car B is blocked until RFID swipe, then RFID swipe sets AccessStatus ON and charging starts

> Test: `test_tesla_disconnect_then_new_car_rfid_starts_session` in `test_authorization.c:452`

---

## Bridge Transaction Integrity

### Bridge lock/unlock API is callable

**Requirement:** `REQ-SM-100`

- **Given** The test environment (native build, no FreeRTOS)
- **When** evse_bridge_lock and evse_bridge_unlock are called
- **Then** They complete without error (no-ops in native builds)

> Test: `test_bridge_lock_unlock_callable` in `test_bridge_transaction.c:1`

### AccessTimer counts down to zero over 60 sequential tick_1s calls

**Requirement:** `REQ-AUTH-024`

- **Given** EVSE in STATE_A with AccessStatus ON and AccessTimer armed to 60
- **When** tick_1s is called 60 times (simulating 60 seconds with no concurrent interference)
- **Then** AccessTimer reaches 0 and AccessStatus is set to OFF

> Test: `test_access_timer_full_countdown` in `test_bridge_transaction.c:54`

### AccessTimer countdown survives interleaved tick_10ms calls

**Requirement:** `REQ-AUTH-025`

- **Given** EVSE in STATE_A with AccessTimer=2, pilot at 12V
- **When** tick_10ms and tick_1s are called in alternating sequence
- **Then** AccessTimer still reaches 0 and AccessStatus is cleared

> Test: `test_access_timer_survives_interleaved_ticks` in `test_bridge_transaction.c:83`

### AccessTimer is not re-armed after expiry

**Requirement:** `REQ-AUTH-026`

- **Given** EVSE in STATE_A after AccessTimer just expired (AccessStatus=OFF, AccessTimer=0)
- **When** tick_10ms runs with PILOT_12V
- **Then** AccessTimer stays 0 (re-arm requires AccessStatus==ON)

> Test: `test_access_timer_not_rearmed_after_expiry` in `test_bridge_transaction.c:123`

### OCPP setAccess(ON) triggers A→B transition on next tick_10ms

**Requirement:** `REQ-AUTH-027`

- **Given** EVSE in STATE_A with AccessStatus=OFF, car plugged in (pilot 9V)
- **When** setAccess(ON) is called, then tick_10ms runs
- **Then** State transitions from A to B (AccessStatus=ON enables the transition)

> Test: `test_set_access_on_enables_a_to_b` in `test_bridge_transaction.c:153`

### AccessStatus ON persists through multiple tick_10ms cycles

**Requirement:** `REQ-AUTH-028`

- **Given** EVSE in STATE_B with AccessStatus=ON (car connected, authorized)
- **When** tick_10ms is called repeatedly with PILOT_9V
- **Then** AccessStatus remains ON (not corrupted by tick processing)

> Test: `test_access_status_persists_through_ticks` in `test_bridge_transaction.c:181`

### Full OCPP charge cycle: authorize → charge → disconnect → new authorize

**Requirement:** `REQ-AUTH-029`

- **Given** EVSE idle in STATE_A, OCPP mode, car not connected
- **When** First user charges and disconnects, then second user plugs in and authorizes
- **Then** Second user's authorization succeeds and A→B transition occurs

> Test: `test_ocpp_full_cycle_two_users` in `test_bridge_transaction.c:205`

### AccessTimer full countdown with interleaved 10ms ticks (real timing)

**Requirement:** `REQ-AUTH-030`

- **Given** EVSE in STATE_A, AccessStatus=ON, AccessTimer=60, pilot 12V
- **When** 60 rounds of (100x tick_10ms + 1x tick_1s) simulate real-world timing
- **Then** AccessTimer reaches 0 and AccessStatus transitions to OFF

> Test: `test_access_timer_real_world_timing` in `test_bridge_transaction.c:277`

---

## Diagnostic Telemetry

### Modbus event ring initializes empty and disabled

**Requirement:** `REQ-E2E-049`

- **Given** A diag_mb_ring_t
- **When** diag_mb_init is called
- **Then** count is 0, head is 0, enabled is false

> Test: `test_mb_init` in `test_diag_modbus.c:1`

### Disabled ring rejects records

**Requirement:** `REQ-E2E-049`

- **Given** An initialized but disabled Modbus ring
- **When** diag_mb_record is called
- **Then** count stays 0

> Test: `test_mb_disabled_rejects` in `test_diag_modbus.c:27`

### Record and read a single event

**Requirement:** `REQ-E2E-049`

- **Given** An enabled Modbus ring
- **When** One SENT event is recorded
- **Then** diag_mb_read returns 1 event with correct fields

> Test: `test_mb_record_and_read` in `test_diag_modbus.c:42`

### Ring wraps around after 32 events

**Requirement:** `REQ-E2E-049`

- **Given** An enabled Modbus ring
- **When** 35 events are recorded
- **Then** count is 32 and oldest 3 events are overwritten

> Test: `test_mb_wrap_around` in `test_diag_modbus.c:68`

### Reset clears all events

**Requirement:** `REQ-E2E-049`

- **Given** A ring with 5 events
- **When** diag_mb_reset is called
- **Then** count is 0 and read returns 0

> Test: `test_mb_reset` in `test_diag_modbus.c:95`

### Error events record error code

**Requirement:** `REQ-E2E-049`

- **Given** An enabled Modbus ring
- **When** An ERROR event with error code 0xE2 is recorded
- **Then** The event has event_type ERROR and error_code 0xE2

> Test: `test_mb_error_event` in `test_diag_modbus.c:118`

### Event struct is exactly 8 bytes

**Requirement:** `REQ-E2E-049`

- **Given** The diag_mb_event_t struct
- **When** sizeof is checked
- **Then** The size is 8 bytes

> Test: `test_mb_event_size` in `test_diag_modbus.c:139`

### NULL ring pointer is safe for all operations

**Requirement:** `REQ-E2E-049`

- **Given** A NULL ring pointer
- **When** init, record, read, reset, enable are called
- **Then** No crash occurs

> Test: `test_mb_null_safety` in `test_diag_modbus.c:152`

### Create synthetic capture with known parameters

**Requirement:** `REQ-E2E-050`

- **Given** A request for 10 snapshots starting at uptime 1000
- **When** diag_create_synthetic is called with GENERAL profile
- **Then** 10 snapshots are created with timestamps 1000..1009

> Test: `test_synthetic_capture` in `test_diag_replay.c:1`

### Load capture from serialized binary buffer

**Requirement:** `REQ-E2E-050`

- **Given** A ring buffer with 3 snapshots serialized to binary
- **When** diag_load_buffer is called
- **Then** All 3 snapshots are loaded with correct timestamps and CRC is valid

> Test: `test_load_from_serialized_buffer` in `test_diag_replay.c:40`

### Load from buffer detects corrupt magic

**Requirement:** `REQ-E2E-050`

- **Given** A binary buffer with invalid magic bytes
- **When** diag_load_buffer is called
- **Then** Returns false

> Test: `test_load_corrupt_magic` in `test_diag_replay.c:84`

### Load from NULL buffer is safe

**Requirement:** `REQ-E2E-050`

- **Given** NULL data pointer
- **When** diag_load_buffer is called
- **Then** Returns false without crash

> Test: `test_load_null_safe` in `test_diag_replay.c:101`

### Synthetic capture with zero count fails

**Requirement:** `REQ-E2E-050`

- **Given** A request for 0 snapshots
- **When** diag_create_synthetic is called
- **Then** Returns false

> Test: `test_synthetic_zero_count` in `test_diag_replay.c:115`

### Replay snapshots into evse_ctx_t and verify field mapping

**Requirement:** `REQ-E2E-051`

- **Given** A synthetic capture with 5 snapshots containing known current values
- **When** Each snapshot's fields are mapped to evse_ctx_t
- **Then** The ctx fields match the snapshot values

> Test: `test_replay_field_mapping` in `test_diag_replay.c:130`

### Replay sequence tracks state transitions across snapshots

**Requirement:** `REQ-E2E-051`

- **Given** A capture with STATE_A→STATE_B→STATE_C→STATE_C→STATE_C transition
- **When** Snapshots are replayed in sequence
- **Then** Each snapshot's state matches the expected transition

> Test: `test_replay_state_transitions` in `test_diag_replay.c:180`

### Advisory replay detects solar current oscillation pattern

**Requirement:** `REQ-E2E-051`

- **Given** A capture with charge_current oscillating between 0 and 80
- **When** Snapshots are analyzed for oscillation
- **Then** The oscillation is detected (>2 zero-crossings in the window)

> Test: `test_replay_solar_oscillation_detection` in `test_diag_replay.c:207`

### Round-trip: serialize ring → load → compare snapshots

**Requirement:** `REQ-E2E-051`

- **Given** A ring buffer with 4 snapshots serialized to binary
- **When** The binary is loaded back via diag_load_buffer
- **Then** All snapshot fields match the originals exactly

> Test: `test_roundtrip_serialize_load` in `test_diag_replay.c:239`

### Ring buffer initializes with zero entries

**Requirement:** `REQ-E2E-040`

- **Given** A diag_ring_t and a buffer of 8 slots
- **When** diag_ring_init is called
- **Then** count is 0, head is 0, profile is OFF, frozen is false

> Test: `test_ring_init` in `test_diag_telemetry.c:1`

### Ring buffer init with NULL pointer is safe

**Requirement:** `REQ-E2E-040`

- **Given** A NULL ring pointer
- **When** diag_ring_init is called with NULL
- **Then** No crash occurs

> Test: `test_ring_init_null` in `test_diag_telemetry.c:58`

### Push a single snapshot and read it back

**Requirement:** `REQ-E2E-041`

- **Given** An initialized ring with GENERAL profile and capacity 8
- **When** One snapshot with timestamp=42 is pushed
- **Then** diag_ring_read returns 1 snapshot with timestamp=42

> Test: `test_push_and_read_single` in `test_diag_telemetry.c:75`

### Push fills buffer to capacity

**Requirement:** `REQ-E2E-041`

- **Given** An initialized ring with capacity 4 and GENERAL profile
- **When** 4 snapshots are pushed (timestamps 10,20,30,40)
- **Then** count is 4 and read returns all 4 in chronological order

> Test: `test_push_fills_to_capacity` in `test_diag_telemetry.c:98`

### Ring buffer wraps around, overwriting oldest entries

**Requirement:** `REQ-E2E-042`

- **Given** An initialized ring with capacity 4 and GENERAL profile
- **When** 6 snapshots are pushed (timestamps 1..6)
- **Then** count stays at 4 and read returns timestamps 3,4,5,6 (oldest overwritten)

> Test: `test_wrap_around` in `test_diag_telemetry.c:130`

### Read with smaller output buffer than ring count

**Requirement:** `REQ-E2E-042`

- **Given** A ring with 4 entries
- **When** diag_ring_read is called with max_count=2
- **Then** Only the 2 oldest snapshots are returned

> Test: `test_read_partial` in `test_diag_telemetry.c:162`

### Reset clears all entries but preserves capacity

**Requirement:** `REQ-E2E-043`

- **Given** A ring with 3 entries and capacity 8
- **When** diag_ring_reset is called
- **Then** count is 0, head is 0, capacity is still 8, profile is OFF

> Test: `test_reset` in `test_diag_telemetry.c:192`

### Reset with NULL is safe

**Requirement:** `REQ-E2E-043`

- **Given** A NULL ring pointer
- **When** diag_ring_reset is called
- **Then** No crash occurs

> Test: `test_reset_null` in `test_diag_telemetry.c:223`

### Frozen ring rejects new pushes

**Requirement:** `REQ-E2E-044`

- **Given** A ring with 2 entries and GENERAL profile, then frozen
- **When** A new snapshot is pushed
- **Then** count remains 2 (push rejected)

> Test: `test_frozen_rejects_push` in `test_diag_telemetry.c:239`

### Unfreezing allows pushes again

**Requirement:** `REQ-E2E-044`

- **Given** A frozen ring with 2 entries
- **When** The ring is unfrozen and a snapshot is pushed
- **Then** count increases to 3

> Test: `test_unfreeze_allows_push` in `test_diag_telemetry.c:268`

### Frozen ring still allows reads

**Requirement:** `REQ-E2E-044`

- **Given** A frozen ring with 2 entries
- **When** diag_ring_read is called
- **Then** Both entries are returned correctly

> Test: `test_frozen_allows_read` in `test_diag_telemetry.c:297`

### Setting GENERAL profile sets divider to 1

**Requirement:** `REQ-E2E-045`

- **Given** An initialized ring
- **When** diag_set_profile is called with DIAG_PROFILE_GENERAL
- **Then** profile is GENERAL and sample_divider is 1

> Test: `test_profile_general` in `test_diag_telemetry.c:324`

### Setting FAST profile sets divider to 1 (sampled from 100ms tick)

**Requirement:** `REQ-E2E-045`

- **Given** An initialized ring
- **When** diag_set_profile is called with DIAG_PROFILE_FAST
- **Then** profile is FAST and sample_divider is 1

> Test: `test_profile_fast` in `test_diag_telemetry.c:343`

### Setting OFF profile prevents pushes

**Requirement:** `REQ-E2E-045`

- **Given** An initialized ring with OFF profile
- **When** A snapshot is pushed
- **Then** count remains 0 (push rejected)

> Test: `test_profile_off_rejects_push` in `test_diag_telemetry.c:362`

### Setting profile resets tick counter

**Requirement:** `REQ-E2E-045`

- **Given** A ring with tick_counter at 5
- **When** diag_set_profile is called
- **Then** tick_counter is reset to 0

> Test: `test_profile_resets_tick_counter` in `test_diag_telemetry.c:382`

### Tick with divider=1 returns true every call

**Requirement:** `REQ-E2E-046`

- **Given** A ring with GENERAL profile (divider=1)
- **When** diag_ring_tick is called 3 times
- **Then** All 3 calls return true

> Test: `test_tick_divider_1` in `test_diag_telemetry.c:403`

### Tick with divider=10 returns true every 10th call

**Requirement:** `REQ-E2E-046`

- **Given** A ring with sample_divider manually set to 10
- **When** diag_ring_tick is called 20 times
- **Then** Returns true on calls 10 and 20 only

> Test: `test_tick_divider_10` in `test_diag_telemetry.c:423`

### Tick with OFF profile always returns false

**Requirement:** `REQ-E2E-046`

- **Given** A ring with OFF profile
- **When** diag_ring_tick is called
- **Then** Returns false

> Test: `test_tick_off_profile` in `test_diag_telemetry.c:449`

### diag_snapshot_t is exactly 64 bytes

**Requirement:** `REQ-E2E-040`

- **Given** The diag_snapshot_t struct definition
- **When** sizeof is checked
- **Then** The size is exactly 64 bytes

> Test: `test_snapshot_size` in `test_diag_telemetry.c:469`

### Serialize empty ring produces valid header with zero snapshots

**Requirement:** `REQ-E2E-047`

- **Given** An initialized ring with GENERAL profile and 0 entries
- **When** diag_ring_serialize is called
- **Then** Output contains valid header with count=0 and CRC32

> Test: `test_serialize_empty` in `test_diag_telemetry.c:484`

### Serialize ring with entries produces correct binary

**Requirement:** `REQ-E2E-047`

- **Given** A ring with 2 snapshots (timestamps 100, 200)
- **When** diag_ring_serialize is called
- **Then** Output contains header + 2 snapshots + CRC32

> Test: `test_serialize_with_data` in `test_diag_telemetry.c:518`

### Serialize returns 0 when buffer is too small

**Requirement:** `REQ-E2E-047`

- **Given** A ring with 2 snapshots
- **When** diag_ring_serialize is called with a 10-byte buffer
- **Then** Returns 0 (insufficient space)

> Test: `test_serialize_buffer_too_small` in `test_diag_telemetry.c:551`

### Serialized data has valid CRC32

**Requirement:** `REQ-E2E-047`

- **Given** A ring with 1 snapshot serialized to binary
- **When** CRC32 is computed over header+snapshots and compared to stored CRC
- **Then** The CRC values match

> Test: `test_serialize_crc_valid` in `test_diag_telemetry.c:574`

### CRC32 of empty data returns initial value

**Requirement:** `REQ-E2E-047`

- **Given** An empty byte array
- **When** diag_crc32 is called with length 0
- **Then** Returns 0 (CRC of empty data)

> Test: `test_crc32_empty` in `test_diag_telemetry.c:607`

### CRC32 of known data matches expected value

**Requirement:** `REQ-E2E-047`

- **Given** The string "123456789"
- **When** diag_crc32 is computed
- **Then** Returns 0xCBF43926 (standard CRC32 test vector)

> Test: `test_crc32_known_value` in `test_diag_telemetry.c:621`

### Push with NULL snapshot is safe

**Requirement:** `REQ-E2E-041`

- **Given** An initialized ring with GENERAL profile
- **When** diag_ring_push is called with NULL snapshot pointer
- **Then** No crash, count stays 0

> Test: `test_push_null_snap` in `test_diag_telemetry.c:639`

### Read from empty ring returns 0

**Requirement:** `REQ-E2E-041`

- **Given** An initialized ring with 0 entries
- **When** diag_ring_read is called
- **Then** Returns 0

> Test: `test_read_empty` in `test_diag_telemetry.c:658`

### Wrap-around preserves all snapshot fields

**Requirement:** `REQ-E2E-042`

- **Given** A ring with capacity 2 and GENERAL profile
- **When** 3 snapshots are pushed with distinct state and current fields
- **Then** The surviving 2 snapshots have all fields intact

> Test: `test_wrap_preserves_fields` in `test_diag_telemetry.c:677`

### File header struct is 34 bytes

**Requirement:** `REQ-E2E-047`

- **Given** The diag_file_header_t struct definition
- **When** sizeof is checked
- **Then** The size is exactly 34 bytes

> Test: `test_file_header_size` in `test_diag_telemetry.c:723`

### DiagProfile set to general via MQTT

**Requirement:** `REQ-E2E-048`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/DiagProfile with payload "general"
- **Then** The parser returns true with diag_profile = 1

> Test: `test_diag_profile_general` in `test_mqtt_parser.c:948`

### DiagProfile set to solar via MQTT

**Requirement:** `REQ-E2E-048`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/DiagProfile with payload "solar"
- **Then** The parser returns true with diag_profile = 2

> Test: `test_diag_profile_solar` in `test_mqtt_parser.c:962`

### DiagProfile set to off via MQTT

**Requirement:** `REQ-E2E-048`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/DiagProfile with payload "off"
- **Then** The parser returns true with diag_profile = 0

> Test: `test_diag_profile_off` in `test_mqtt_parser.c:976`

### DiagProfile set via numeric value

**Requirement:** `REQ-E2E-048`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/DiagProfile with payload "5"
- **Then** The parser returns true with diag_profile = 5 (FAST)

> Test: `test_diag_profile_numeric` in `test_mqtt_parser.c:990`

### DiagProfile rejects invalid payload

**Requirement:** `REQ-E2E-048`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/DiagProfile with payload "invalid"
- **Then** The parser returns false

> Test: `test_diag_profile_invalid` in `test_mqtt_parser.c:1004`

---

## Dual-EVSE Load Balancing

### Both EVSEs get equal share of MaxCircuit in Normal mode

**Requirement:** `REQ-DUAL-S1A`

- **Given** Two EVSEs both in STATE_C, Normal mode, MaxCircuit=32A
- **When** evse_calc_balanced_current(mod=1) is called
- **Then** Each EVSE gets 160 (16A), equal split of 320 IsetBalanced

> Test: `test_s1_both_start_equal_split` in `test_dual_evse.c:1`

### IsetBalanced equals MaxCircuit * 10 in Normal mode

**Requirement:** `REQ-DUAL-S1B`

- **Given** Two EVSEs in Normal mode
- **When** evse_calc_balanced_current(mod=1)
- **Then** IsetBalanced = MaxCircuit * 10 = 320

> Test: `test_s1_isetbalanced_equals_max_circuit` in `test_dual_evse.c:67`

### EV meter baseload reduces available current

**Requirement:** `REQ-DUAL-S1C`

- **Given** Two EVSEs, EV meter reads 200 (other loads on circuit)
- **When** evse_calc_balanced_current(mod=1)
- **Then** IsetBalanced reduced by baseload

> Test: `test_s1_ev_meter_baseload` in `test_dual_evse.c:85`

### Master reduces current when slave joins

**Requirement:** `REQ-DUAL-S2A`

- **Given** Master alone at 160, slave not active
- **When** Slave enters STATE_C and calc runs with mod=1
- **Then** Both get equal share (160 each with MaxCircuit=32)

> Test: `test_s2_slave_joins_master_reduces` in `test_dual_evse.c:112`

### Slave node sends COMM_B on car connect

**Requirement:** `REQ-DUAL-S2B`

- **Given** Slave EVSE (LoadBl=2) in STATE_A with 9V pilot
- **When** tick_10ms with PILOT_9V
- **Then** Transitions to STATE_COMM_B (requests master permission)

> Test: `test_s2_slave_sends_comm_b` in `test_dual_evse.c:145`

### MaxCircuit reduction redistributes current equally

**Requirement:** `REQ-DUAL-S3A`

- **Given** Both at 160 with MaxCircuit=32
- **When** MaxCircuit reduced to 20
- **Then** Each gets 100 (200 / 2)

> Test: `test_s3_maxcircuit_reduction` in `test_dual_evse.c:170`

### MaxCircuit at exactly 2 * MinCurrent gives each MinCurrent

**Requirement:** `REQ-DUAL-S3B`

- **Given** Both in STATE_C, MaxCircuit=12
- **When** evse_calc_balanced_current
- **Then** Each gets exactly 60 (MinCurrent * 10)

> Test: `test_s3_maxcircuit_to_mincurrent` in `test_dual_evse.c:191`

### Slave disconnects, master gets capped at BalancedMax

**Requirement:** `REQ-DUAL-S4A`

- **Given** Both at 160, slave disconnects (STATE_A)
- **When** Recalculation runs
- **Then** Master gets BalancedMax[0]=160, slave gets 0

> Test: `test_s4_slave_disconnects` in `test_dual_evse.c:215`

### Master with higher MaxCurrent gets more after slave disconnect

**Requirement:** `REQ-DUAL-S4B`

- **Given** Master MaxCurrent=32, MaxCircuit=40, slave disconnects
- **When** Recalculation with mod=0
- **Then** Master gets up to 320 (BalancedMax[0]=ChargeCurrent=320)

> Test: `test_s4_master_absorbs_full_capacity` in `test_dual_evse.c:240`

### Smart mode: new EVSE joining recalculates from mains headroom

**Requirement:** `REQ-DUAL-S5A`

- **Given** Master in MODE_SMART, both in STATE_C, MaxMains=32A
- **When** evse_calc_balanced_current(mod=1)
- **Then** IsetBalanced based on (MaxMains*10) - Baseload

> Test: `test_s5_smart_mode_new_join` in `test_dual_evse.c:273`

### Smart mode: surplus increases IsetBalanced gradually

**Requirement:** `REQ-DUAL-S5B`

- **Given** MODE_SMART, low mains load, IsetBalanced=200
- **When** evse_calc_balanced_current(mod=0)
- **Then** IsetBalanced increases by Idifference/4

> Test: `test_s5_smart_surplus_increases` in `test_dual_evse.c:297`

### Smart mode: overload decreases IsetBalanced immediately

**Requirement:** `REQ-DUAL-S5C`

- **Given** MODE_SMART, high mains load
- **When** evse_calc_balanced_current(mod=0)
- **Then** IsetBalanced decreases by full Idifference

> Test: `test_s5_smart_overload_decreases` in `test_dual_evse.c:321`

### Solar mode: both EVSEs in startup get MinCurrent

**Requirement:** `REQ-DUAL-S6A`

- **Given** MODE_SOLAR, both in STATE_C with IntTimer < SOLARSTARTTIME
- **When** evse_calc_balanced_current
- **Then** Both receive exactly MinCurrent * 10 = 60

> Test: `test_s6_solar_both_in_startup` in `test_dual_evse.c:348`

### Solar mode: insufficient solar starts SolarStopTimer

**Requirement:** `REQ-DUAL-S6B`

- **Given** MODE_SOLAR, high grid import, past startup
- **When** evse_calc_balanced_current
- **Then** SolarStopTimer is set

> Test: `test_s6_solar_insufficient_starts_timer` in `test_dual_evse.c:376`

### Zero available power pauses all EVSEs via priority scheduling

**Requirement:** `REQ-DUAL-S7A`

- **Given** Smart mode, MaxMains=5A, heavy mains load (IsetBalanced drops to 0)
- **When** evse_calc_balanced_current
- **Then** Both EVSEs paused (Balanced=0), NoCurrent increments (true hard shortage)

> Test: `test_s7_mincurrent_violation` in `test_dual_evse.c:412`

### Exactly 2 * MinCurrent — no shortage

**Requirement:** `REQ-DUAL-S7B`

- **Given** Normal mode, MaxCircuit=12
- **When** evse_calc_balanced_current
- **Then** Each gets 60, NoCurrent stays 0

> Test: `test_s7_barely_enough` in `test_dual_evse.c:440`

### Slave error → master absorbs capacity

**Requirement:** `REQ-DUAL-S8A`

- **Given** Both at 160 each, slave enters B1
- **When** Recalculation
- **Then** Master gets 160 (capped by BalancedMax)

> Test: `test_s8_slave_error_master_absorbs` in `test_dual_evse.c:465`

### Slave recovers, current redistributed

**Requirement:** `REQ-DUAL-S8B`

- **Given** Master alone at 160, slave re-enters STATE_C
- **When** evse_calc_balanced_current(mod=1)
- **Then** Both get equal share

> Test: `test_s8_slave_recovers` in `test_dual_evse.c:488`

### MaxSumMains overridden Idifference limits IsetBalanced

**Requirement:** `REQ-DUAL-S9A`

- **Given** MODE_SMART, MaxSumMains=30, Isum close to limit
- **When** evse_calc_balanced_current(mod=0)
- **Then** IsetBalanced constrained by MaxSumMains

> Test: `test_s9_maxsummains_limits` in `test_dual_evse.c:520`

### MaxSumMains timer expiry stops charging

**Requirement:** `REQ-DUAL-S9B`

- **Given** STATE_C with MaxSumMainsTimer=1
- **When** evse_tick_1s (timer expires)
- **Then** C → C1, LESS_6A set

> Test: `test_s9_maxsummains_timer_expiry` in `test_dual_evse.c:546`

### Normal mode forces 3P when currently on 1P

**Requirement:** `REQ-DUAL-S10A`

- **Given** Normal mode, Nr_Of_Phases_Charging=1
- **When** evse_calc_balanced_current
- **Then** Switching_Phases_C2 = GOING_TO_SWITCH_3P

> Test: `test_s10_normal_forces_3p` in `test_dual_evse.c:578`

### STATE_C entry applies 1P switch

**Requirement:** `REQ-DUAL-S10B`

- **Given** Switching_Phases_C2=GOING_TO_SWITCH_1P, EnableC2=ALWAYS_OFF
- **When** evse_set_state(STATE_C)
- **Then** Nr_Of_Phases_Charging=1, contactor2 off

> Test: `test_s10_state_c_applies_1p` in `test_dual_evse.c:597`

### Smart mode with AUTO forces back to 3P

**Requirement:** `REQ-DUAL-S10C`

- **Given** MODE_SMART, EnableC2=AUTO, Nr_Of_Phases_Charging=1
- **When** evse_check_switching_phases
- **Then** Switching_Phases_C2 = GOING_TO_SWITCH_3P

> Test: `test_s10_smart_auto_forces_3p` in `test_dual_evse.c:617`

---

## End-to-End Charging

### Complete standalone charge cycle with DiodeCheck

**Requirement:** `REQ-E2E-001`

- **Given** Standalone EVSE, authorized, Normal mode
- **When** Car connects (9V) → DiodeCheck (DIODE) → requests charge (6V) → stops (9V) → disconnects (12V)
- **Then** Full cycle: A → B → (DiodeCheck) → C → B → A with correct contactor states

> Test: `test_e2e_standalone_happy_path` in `test_e2e_charging.c:1`

### Slave EVSE full charge cycle with master handshake

**Requirement:** `REQ-E2E-002`

- **Given** Slave EVSE (LoadBl=2), authorized
- **When** Car connects → COMM_B → master approves → B → DiodeCheck → 6V → COMM_C → master approves → C → disconnect
- **Then** Full slave cycle: A → COMM_B → COMM_B_OK → B → COMM_C → COMM_C_OK → C → B → A

> Test: `test_e2e_slave_happy_path` in `test_e2e_charging.c:99`

### OCPP grants access mid-session, car starts charging

**Requirement:** `REQ-E2E-003`

- **Given** Standalone EVSE with OcppMode=true, AccessStatus=OFF
- **When** Car connects, OCPP grants access, car charges, OCPP revokes
- **Then** A (blocked) → A (OCPP grants) → B → C → C1 (revoked) → B1

> Test: `test_e2e_ocpp_authorization_flow` in `test_e2e_charging.c:152`

### OCPP denied — car connects but stays in STATE_A

**Requirement:** `REQ-E2E-004`

- **Given** OCPP mode with AccessStatus=OFF
- **When** Car repeatedly presents 9V pilot
- **Then** State stays A for 100+ ticks

> Test: `test_e2e_ocpp_denied_stays_in_a` in `test_e2e_charging.c:200`

### Two full charge cycles — verify no stale state leaks

**Requirement:** `REQ-E2E-005`

- **Given** Standalone EVSE
- **When** First cycle: connect → charge → disconnect. Second cycle: connect → charge → disconnect.
- **Then** Both cycles complete successfully, all state is clean between them

> Test: `test_e2e_reconnect_after_disconnect` in `test_e2e_charging.c:224`

### Temperature spike during STATE_C, recovery after cooldown

**Requirement:** `REQ-E2E-006`

- **Given** EVSE charging in STATE_C
- **When** Temperature exceeds maxTemp, then cools below hysteresis
- **Then** C → C1 → B1 (TEMP_HIGH), cooldown clears error

> Test: `test_e2e_temp_error_during_charge` in `test_e2e_charging.c:283`

### Meter communication lost during STATE_C

**Requirement:** `REQ-E2E-007`

- **Given** EVSE charging in Smart mode with MainsMeter
- **When** Meter timeout reaches 0
- **Then** CT_NOCOMM set, power unavailable, C → C1

> Test: `test_e2e_ct_nocomm_during_charge` in `test_e2e_charging.c:322`

### TEMP_HIGH + CT_NOCOMM simultaneously during charge

**Requirement:** `REQ-E2E-008`

- **Given** EVSE charging in Smart mode
- **When** Temperature spikes AND meter fails at same time
- **Then** Both errors set, both must clear for full recovery

> Test: `test_e2e_multiple_errors_during_charge` in `test_e2e_charging.c:359`

### 6V pilot without DiodeCheck does not transition to STATE_C

**Requirement:** `REQ-E2E-009`

- **Given** EVSE in STATE_B, DiodeCheck=0
- **When** 55 ticks of 6V pilot
- **Then** State stays STATE_B (DiodeCheck blocks B→C)

> Test: `test_e2e_no_charge_without_diode` in `test_e2e_charging.c:397`

### ChargeDelay > 0 blocks A→B transition, sends to B1

**Requirement:** `REQ-E2E-010`

- **Given** Standalone EVSE with ChargeDelay=10
- **When** Car connects (9V)
- **Then** Goes to B1 (not B), must wait for delay to expire

> Test: `test_e2e_charge_delay_blocks_charging` in `test_e2e_charging.c:424`

### StateTimer is properly reset between charge sessions

**Requirement:** `REQ-E2E-011`

- **Given** EVSE in STATE_C with accumulated StateTimer
- **When** Car stops (9V → B), then requests charge again (6V)
- **Then** Debounce starts from 0, requiring full 500ms

> Test: `test_e2e_state_timer_reset_on_c_to_b` in `test_e2e_charging.c:450`

### Power unavailable during charge suspends charging

**Requirement:** `REQ-E2E-012`

- **Given** EVSE charging in STATE_C
- **When** evse_set_power_unavailable is called
- **Then** C → C1 (PWM off) → B1 (contactors open after C1Timer)

> Test: `test_e2e_power_unavailable_c_to_c1_to_b1` in `test_e2e_charging.c:492`

### Reconnect after Tesla-style disconnect requires fresh RFID swipe

**Requirement:** `REQ-E2E-013`

- **Given** EVSE charging in STATE_C, RFIDReader=EnableOne, AccessStatus=ON
- **When** Car disconnects (CP → 12V), then reconnects within RFIDLOCKTIME seconds
- **Then** Auto A→B is blocked (AccessStatus cleared on disconnect); new RFID swipe restarts session

> Test: `test_e2e_rfid_reconnect_after_tesla_disconnect` in `test_e2e_charging.c:522`

---

## Error Handling & Safety

### Setting an error flag stores it in ErrorFlags

**Requirement:** `REQ-ERR-001`

- **Given** The EVSE is initialised with no errors
- **When** evse_set_error_flags is called with TEMP_HIGH
- **Then** TEMP_HIGH bit is set in ErrorFlags

> Test: `test_set_error_flags` in `test_error_handling.c:1`

### Multiple error flags can be set simultaneously

**Requirement:** `REQ-ERR-002`

- **Given** The EVSE is initialised with no errors
- **When** evse_set_error_flags is called with TEMP_HIGH then CT_NOCOMM
- **Then** Both TEMP_HIGH and CT_NOCOMM bits are set in ErrorFlags

> Test: `test_set_multiple_error_flags` in `test_error_handling.c:46`

### Clearing an error flag removes only the specified flag

**Requirement:** `REQ-ERR-003`

- **Given** The EVSE has TEMP_HIGH and CT_NOCOMM error flags set
- **When** evse_clear_error_flags is called with TEMP_HIGH
- **Then** TEMP_HIGH is cleared but CT_NOCOMM remains set

> Test: `test_clear_error_flags` in `test_error_handling.c:62`

### Clearing one flag preserves all other active flags

**Requirement:** `REQ-ERR-004`

- **Given** The EVSE has TEMP_HIGH, LESS_6A, and CT_NOCOMM error flags set
- **When** evse_clear_error_flags is called with LESS_6A
- **Then** TEMP_HIGH and CT_NOCOMM remain set, LESS_6A is cleared

> Test: `test_clear_preserves_other_flags` in `test_error_handling.c:78`

### ChargeDelay decrements each second

**Requirement:** `REQ-ERR-005`

- **Given** The EVSE has ChargeDelay set to 10
- **When** One second tick occurs
- **Then** ChargeDelay decrements to 9

> Test: `test_charge_delay_counts_down` in `test_error_handling.c:97`

### ChargeDelay does not underflow past zero

**Requirement:** `REQ-ERR-006`

- **Given** The EVSE has ChargeDelay set to 1
- **When** Two second ticks occur
- **Then** ChargeDelay reaches 0 and stays at 0

> Test: `test_charge_delay_stops_at_zero` in `test_error_handling.c:112`

### Temperature exceeding maxTemp triggers TEMP_HIGH error

**Requirement:** `REQ-ERR-008`

- **Given** The EVSE is in STATE_A with ChargeDelay=5 and AccessStatus ON
- **Given** The EVSE is charging with TempEVSE=70 and maxTemp=65
- **When** A 9V pilot signal is received (vehicle connected)
- **When** One second tick occurs
- **Then** The state transitions to STATE_B1 instead of STATE_B
- **Then** TEMP_HIGH error flag is set in ErrorFlags

> Test: `test_temp_high_triggers_error` in `test_error_handling.c:129`

### Overtemperature shuts down active charging session

**Requirement:** `REQ-ERR-009`

- **Given** The EVSE is in STATE_C (charging) with TempEVSE=70 and maxTemp=65
- **When** One second tick occurs triggering temperature protection
- **Then** The state transitions out of STATE_C (charging suspended)

> Test: `test_temp_high_shuts_down_charging` in `test_error_handling.c:165`

### Temperature recovery requires 10-degree hysteresis below maxTemp

**Requirement:** `REQ-ERR-010`

- **Given** The EVSE has TEMP_HIGH error with maxTemp=65
- **When** Temperature drops to 60 (within hysteresis) then to 50 (below threshold)
- **Then** TEMP_HIGH persists at 60 but clears at 50 (below maxTemp-10)

> Test: `test_temp_recovery_with_hysteresis` in `test_error_handling.c:182`

### Temperature recovery boundary: exactly at threshold does not clear

**Requirement:** `REQ-ERR-011`

- **Given** The EVSE has TEMP_HIGH error with maxTemp=65
- **When** Temperature is exactly 55 (maxTemp-10) then drops to 54
- **Then** TEMP_HIGH persists at 55 but clears at 54 (strictly below threshold)

> Test: `test_temp_recovery_boundary` in `test_error_handling.c:206`

### Mains meter communication timeout sets CT_NOCOMM error

**Requirement:** `REQ-ERR-012`

- **Given** The EVSE is in MODE_SMART with MainsMeterTimeout=0 (timed out)
- **When** One second tick occurs
- **Then** CT_NOCOMM error flag is set

> Test: `test_mains_meter_timeout_sets_ct_nocomm` in `test_error_handling.c:232`

### Mains meter timeout counter decrements each second

**Requirement:** `REQ-ERR-013`

- **Given** The EVSE is in MODE_SMART with MainsMeterTimeout=5
- **When** One second tick occurs
- **Then** MainsMeterTimeout decrements to 4

> Test: `test_mains_meter_timeout_counts_down` in `test_error_handling.c:250`

### Normal mode ignores mains meter timeout (no CT_NOCOMM)

**Requirement:** `REQ-ERR-014`

- **Given** The EVSE is in MODE_NORMAL with MainsMeterTimeout=0
- **When** One second tick occurs
- **Then** CT_NOCOMM error flag is NOT set

> Test: `test_mains_meter_normal_mode_ignores_timeout` in `test_error_handling.c:268`

### No mains meter configured resets timeout to COMM_TIMEOUT

**Requirement:** `REQ-ERR-015`

- **Given** The EVSE has MainsMeterType=0 (no meter) with MainsMeterTimeout=3
- **When** One second tick occurs
- **Then** MainsMeterTimeout is reset to COMM_TIMEOUT

> Test: `test_no_mains_meter_resets_timeout` in `test_error_handling.c:287`

### EV meter communication timeout sets EV_NOCOMM error

**Requirement:** `REQ-ERR-016`

- **Given** The EVSE has EVMeterType=1 with EVMeterTimeout=0 (timed out)
- **When** One second tick occurs
- **Then** EV_NOCOMM error flag is set

> Test: `test_ev_meter_timeout_sets_ev_nocomm` in `test_error_handling.c:306`

### No EV meter configured resets timeout to COMM_EVTIMEOUT

**Requirement:** `REQ-ERR-017`

- **Given** The EVSE has EVMeterType=0 (no meter) with EVMeterTimeout=3
- **When** One second tick occurs
- **Then** EVMeterTimeout is reset to COMM_EVTIMEOUT

> Test: `test_no_ev_meter_resets_timeout` in `test_error_handling.c:323`

### CT_NOCOMM error clears when mains meter communication resumes

**Requirement:** `REQ-ERR-018`

- **Given** The EVSE has CT_NOCOMM error with MainsMeterTimeout=5 (restored)
- **When** One second tick occurs
- **Then** CT_NOCOMM error flag is cleared

> Test: `test_ct_nocomm_recovers_on_communication` in `test_error_handling.c:341`

### EV_NOCOMM error clears when EV meter communication resumes

**Requirement:** `REQ-ERR-019`

- **Given** The EVSE has EV_NOCOMM error with EVMeterTimeout=5 (restored)
- **When** One second tick occurs
- **Then** EV_NOCOMM error flag is cleared

> Test: `test_ev_nocomm_recovers_on_communication` in `test_error_handling.c:357`

### LESS_6A error auto-recovers when sufficient current becomes available

**Requirement:** `REQ-ERR-020`

- **Given** The EVSE is in MODE_NORMAL standalone with LESS_6A error and AccessStatus ON
- **When** One second tick occurs (normal mode always has current available)
- **Then** LESS_6A error flag is cleared

> Test: `test_less_6a_recovers_when_current_available` in `test_error_handling.c:375`

### LESS_6A error persists when current is still unavailable

**Requirement:** `REQ-ERR-021`

- **Given** The EVSE is in MODE_SMART with LESS_6A error and mains heavily loaded
- **When** One second tick occurs
- **Then** LESS_6A error flag remains set

> Test: `test_less_6a_stays_when_current_unavailable` in `test_error_handling.c:394`

### Node EVSEs (LoadBl >= 2) do not auto-recover LESS_6A

**Requirement:** `REQ-ERR-022`

- **Given** The EVSE is configured as a node (LoadBl=3) with LESS_6A error
- **When** One second tick occurs
- **Then** LESS_6A error flag remains set (nodes rely on master for recovery)

> Test: `test_less_6a_no_recovery_for_nodes` in `test_error_handling.c:414`

### Power unavailable during charging suspends to STATE_C1

**Requirement:** `REQ-ERR-023`

- **Given** The EVSE is in STATE_C (charging)
- **When** evse_set_power_unavailable is called
- **Then** The state transitions to STATE_C1 (charging suspended)

> Test: `test_power_unavailable_from_C_goes_C1` in `test_error_handling.c:434`

### Power unavailable in STATE_B moves to waiting state B1

**Requirement:** `REQ-ERR-024`

- **Given** The EVSE is in STATE_B (connected)
- **When** evse_set_power_unavailable is called
- **Then** The state transitions to STATE_B1 (waiting)

> Test: `test_power_unavailable_from_B_goes_B1` in `test_error_handling.c:448`

### Power unavailable in STATE_A has no effect

**Requirement:** `REQ-ERR-025`

- **Given** The EVSE is in STATE_A (disconnected)
- **When** evse_set_power_unavailable is called
- **Then** The state remains STATE_A

> Test: `test_power_unavailable_from_A_stays_A` in `test_error_handling.c:464`

### Power unavailable in STATE_B1 remains in B1 (already waiting)

**Requirement:** `REQ-ERR-026`

- **Given** The EVSE is in STATE_B1 (waiting)
- **When** evse_set_power_unavailable is called
- **Then** The state remains STATE_B1

> Test: `test_power_unavailable_from_B1_stays_B1` in `test_error_handling.c:479`

### Power unavailable in STATE_C1 remains in C1 (already suspended)

**Requirement:** `REQ-ERR-027`

- **Given** The EVSE is in STATE_C1 (charging suspended)
- **When** evse_set_power_unavailable is called
- **Then** The state remains STATE_C1

> Test: `test_power_unavailable_from_C1_stays_C1` in `test_error_handling.c:494`

### Entering STATE_B1 disconnects pilot when authorized

**Requirement:** `REQ-ERR-028`

- **Given** The EVSE has AccessStatus ON and PilotDisconnected is false
- **When** The state is set to STATE_B1
- **Then** PilotDisconnected is true and pilot_connected is false

> Test: `test_pilot_disconnect_on_B1_entry` in `test_error_handling.c:511`

### Pilot reconnects after PilotDisconnectTime expires

**Requirement:** `REQ-ERR-029`

- **Given** The EVSE has PilotDisconnectTime=2 with pilot disconnected
- **When** Two second ticks occur
- **Then** PilotDisconnected is cleared and pilot_connected is restored

> Test: `test_pilot_reconnect_after_timer` in `test_error_handling.c:528`

### MaxSumMains timer expiry stops charging with LESS_6A error

**Requirement:** `REQ-ERR-030`

- **Given** The EVSE is charging with MaxSumMainsTimer=1 and mains heavily loaded
- **When** One second tick occurs (timer expires)
- **Then** The state transitions to STATE_C1 and LESS_6A error flag is set

> Test: `test_maxsummains_timer_stops_charging` in `test_error_handling.c:560`

---

## Fidelity: DisconnectTimeCounter

### Module tick_1s does not increment DisconnectTimeCounter

**Requirement:** `REQ-FID-D1A`

- **Given** ModemEnabled=true, DisconnectTimeCounter=0
- **When** tick_1s is called
- **Then** Counter stays 0 (firmware wrapper handles increment + pilot check)

> Test: `test_fid_disconnect_counter_not_in_module` in `test_fidelity.c:1`

### Module correctly sets counter via set_state for STATE_A

**Requirement:** `REQ-FID-D1B`

- **Given** ModemEnabled=true, DisconnectTimeCounter=-1
- **When** evse_set_state to STATE_A
- **Then** Counter is set to 0 (start counting)

> Test: `test_fid_disconnect_counter_starts_on_state_a` in `test_fidelity.c:59`

### Module disables counter on MODEM_REQUEST entry

**Requirement:** `REQ-FID-D1C`

- **Given** DisconnectTimeCounter=5
- **When** evse_set_state to STATE_MODEM_REQUEST
- **Then** Counter is set to -1 (disabled)

> Test: `test_fid_disconnect_counter_disabled_on_modem_request` in `test_fidelity.c:75`

---

## Fidelity: PilotDisconnectTime

### tick_1s only decrements timer, does not reconnect

**Requirement:** `REQ-FID-D2A`

- **Given** PilotDisconnectTime=1, PilotDisconnected=true
- **When** tick_1s is called (timer reaches 0)
- **Then** PilotDisconnected is still true (reconnect happens in tick_10ms)

> Test: `test_fid_pilot_disconnect_no_reconnect_in_tick_1s` in `test_fidelity.c:95`

### tick_10ms reconnects when PilotDisconnectTime reaches 0

**Requirement:** `REQ-FID-D2B`

- **Given** PilotDisconnected=true, PilotDisconnectTime=0, State=B1
- **When** tick_10ms is called with PILOT_9V
- **Then** PilotDisconnected=false, pilot_connected=true

> Test: `test_fid_pilot_disconnect_reconnects_in_tick_10ms` in `test_fidelity.c:116`

---

## Fidelity: Fall-through behavior

### COMM_B_OK transitions to STATE_B and B handler runs same tick

**Requirement:** `REQ-FID-D3A`

- **Given** State=COMM_B_OK, DiodeCheck=0
- **When** tick_10ms with PILOT_DIODE
- **Then** State=STATE_B, and DiodeCheck=1 (B handler fired in same tick)

> Test: `test_fid_comm_b_ok_falls_through_to_b_handler` in `test_fidelity.c:141`

### COMM_B_OK → B → 12V disconnect in same tick

**Requirement:** `REQ-FID-D3B`

- **Given** State=COMM_B_OK
- **When** tick_10ms with PILOT_12V
- **Then** State=STATE_A (B handler detects disconnect in same tick)

> Test: `test_fid_comm_b_ok_to_b_then_disconnect` in `test_fidelity.c:161`

### A→B transition allows B handler to run and detect PILOT_DIODE

**Requirement:** `REQ-FID-D4A`

- **Given** State=STATE_A, PILOT_DIODE would trigger A→B on 9V but we pass 9V
- **When** tick_10ms with PILOT_9V in STATE_A with access
- **Then** State=STATE_B, ActivationMode=30, and StateTimer reset to 0

> Test: `test_fid_a_to_b_falls_through_to_b_handler` in `test_fidelity.c:183`

### COMM_C_OK transitions to STATE_C and C handler runs same tick

**Requirement:** `REQ-FID-D5A`

- **Given** State=COMM_C_OK
- **When** tick_10ms with PILOT_6V
- **Then** State=STATE_C, StateTimer=0 (C handler's else resets StateTimer)

> Test: `test_fid_comm_c_ok_falls_through_to_c_handler` in `test_fidelity.c:208`

### COMM_C_OK → C → immediate 12V disconnect in same tick

**Requirement:** `REQ-FID-D5B`

- **Given** State=COMM_C_OK
- **When** tick_10ms with PILOT_12V
- **Then** State=STATE_A (C handler detects disconnect immediately)

> Test: `test_fid_comm_c_ok_to_c_then_disconnect` in `test_fidelity.c:230`

---

## Fidelity: ACTSTART no pilot check

### ACTSTART ignores PILOT_12V (original has no pilot check here)

**Requirement:** `REQ-FID-D6A`

- **Given** State=ACTSTART with timer running (3 seconds)
- **When** tick_10ms with PILOT_12V
- **Then** State stays ACTSTART (timer must expire → B → detects 12V → A)

> Test: `test_fid_actstart_no_pilot_12v_check` in `test_fidelity.c:252`

### ACTSTART timer expiry leads to STATE_B (B handler is before ACTSTART

**Requirement:** `REQ-FID-D6B`

- **Given** State=ACTSTART, ActivationTimer=0
- **When** tick_10ms with PILOT_12V, then another tick_10ms with PILOT_12V
- **Then** First tick: ACTSTART → B. Second tick: B → A (12V detected)

> Test: `test_fid_actstart_timer_then_disconnect` in `test_fidelity.c:270`

---

## Fidelity: Modem states not in tick_10ms

### Modem states are invisible to tick_10ms

**Requirement:** `REQ-FID-D7A`

- **Given** EVSE in each modem state
- **When** tick_10ms with any pilot value
- **Then** State does not change (modem managed entirely by tick_1s)

> Test: `test_fid_modem_states_invisible_to_tick_10ms` in `test_fidelity.c:299`

---

## Fidelity: Handler ordering

### B→ACTSTART falls through to ACTSTART handler in same tick

**Requirement:** `REQ-FID-D8A`

- **Given** State=STATE_B, ActivationMode=0 (expired)
- **When** tick_10ms with PILOT_9V
- **Then** State=STATE_ACTSTART, ActivationTimer=3

> Test: `test_fid_b_to_actstart_falls_through` in `test_fidelity.c:327`

---

## Fidelity: Config field

### Socket mode (Config=0) caps ChargeCurrent by MaxCapacity

**Requirement:** `REQ-FID-CFG-A`

- **Given** Config=0, MaxCurrent=25, MaxCapacity=16, STATE_C
- **When** calc_balanced_current is called
- **Then** ChargeCurrent=160 (capped by MaxCapacity)

> Test: `test_fid_config_socket_caps_by_maxcapacity` in `test_fidelity.c:351`

### Fixed cable mode (Config=1) does NOT cap by MaxCapacity

**Requirement:** `REQ-FID-CFG-B`

- **Given** Config=1, MaxCurrent=25, MaxCapacity=16, STATE_C
- **When** calc_balanced_current is called
- **Then** ChargeCurrent=250 (MaxCurrent * 10, not capped)

> Test: `test_fid_config_fixed_cable_no_maxcapacity_cap` in `test_fidelity.c:372`

---

## HTTP API Color Parsing

### Valid RGB color values are accepted

**Requirement:** `REQ-API-001`

- **Given** Integer values for R, G, B
- **When** All values are in 0..255
- **Then** http_api_parse_color returns true with correct output

> Test: `test_color_valid` in `test_http_api.c:1`

### Zero RGB values are accepted

**Requirement:** `REQ-API-001`


> Test: `test_color_zero` in `test_http_api.c:31`

### Maximum RGB values are accepted

**Requirement:** `REQ-API-001`


> Test: `test_color_max` in `test_http_api.c:42`

---

## HTTP API Input Validation

### RGB value above 255 is rejected

**Requirement:** `REQ-API-002`


> Test: `test_color_out_of_range` in `test_http_api.c:53`

### Negative RGB value is rejected

**Requirement:** `REQ-API-002`


> Test: `test_color_negative` in `test_http_api.c:63`

### Override current below minimum is rejected

**Requirement:** `REQ-API-004`


> Test: `test_override_current_below_min` in `test_http_api.c:112`

### Override current above maximum is rejected

**Requirement:** `REQ-API-004`


> Test: `test_override_current_above_max` in `test_http_api.c:121`

### Override current on slave is rejected

**Requirement:** `REQ-API-004`


> Test: `test_override_current_slave` in `test_http_api.c:130`

### Current min below 6A is rejected

**Requirement:** `REQ-API-005`


> Test: `test_current_min_too_low` in `test_http_api.c:159`

### Current min above 16A is rejected

**Requirement:** `REQ-API-005`


> Test: `test_current_min_too_high` in `test_http_api.c:168`

### Current min on slave is rejected

**Requirement:** `REQ-API-005`


> Test: `test_current_min_slave` in `test_http_api.c:177`

### Max sum mains between 1-9 is rejected

**Requirement:** `REQ-API-006`


> Test: `test_max_sum_mains_gap` in `test_http_api.c:215`

### Max sum mains above 600 is rejected

**Requirement:** `REQ-API-006`


> Test: `test_max_sum_mains_too_high` in `test_http_api.c:224`

### Max sum mains on slave is rejected

**Requirement:** `REQ-API-006`


> Test: `test_max_sum_mains_slave` in `test_http_api.c:233`

### Stop timer above 60 is rejected

**Requirement:** `REQ-API-007`


> Test: `test_stop_timer_too_high` in `test_http_api.c:262`

### Negative stop timer is rejected

**Requirement:** `REQ-API-007`


> Test: `test_stop_timer_negative` in `test_http_api.c:271`

### Solar start current above 48 is rejected

**Requirement:** `REQ-API-008`


> Test: `test_solar_start_too_high` in `test_http_api.c:300`

### Solar max import above 48 is rejected

**Requirement:** `REQ-API-009`


> Test: `test_solar_import_too_high` in `test_http_api.c:320`

### PrioStrategy value 3 is rejected

**Requirement:** `REQ-API-011`


> Test: `test_prio_strategy_too_high` in `test_http_api.c:361`

### PrioStrategy negative value is rejected

**Requirement:** `REQ-API-011`


> Test: `test_prio_strategy_negative` in `test_http_api.c:370`

### PrioStrategy on slave is rejected

**Requirement:** `REQ-API-011`

- **Given** A slave EVSE (load_bl=2)
- **When** prio_strategy is 0 (valid value)
- **Then** Validation fails because slaves cannot set scheduling

> Test: `test_prio_strategy_slave` in `test_http_api.c:379`

### RotationInterval in gap (1-29) is rejected

**Requirement:** `REQ-API-012`


> Test: `test_rotation_interval_gap` in `test_http_api.c:423`

### RotationInterval above maximum is rejected

**Requirement:** `REQ-API-012`


> Test: `test_rotation_interval_too_high` in `test_http_api.c:432`

### RotationInterval on slave is rejected

**Requirement:** `REQ-API-012`


> Test: `test_rotation_interval_slave` in `test_http_api.c:441`

### IdleTimeout below minimum (29) is rejected

**Requirement:** `REQ-API-013`


> Test: `test_idle_timeout_too_low` in `test_http_api.c:482`

### IdleTimeout above maximum (301) is rejected

**Requirement:** `REQ-API-013`


> Test: `test_idle_timeout_too_high` in `test_http_api.c:491`

### IdleTimeout on slave is rejected

**Requirement:** `REQ-API-013`


> Test: `test_idle_timeout_slave` in `test_http_api.c:500`

---

## HTTP API Validation

### Override current zero is always valid (disables override)

**Requirement:** `REQ-API-003`


> Test: `test_override_current_zero` in `test_http_api.c:75`

### Override current within range is valid

**Requirement:** `REQ-API-003`


> Test: `test_override_current_valid` in `test_http_api.c:84`

### Override current at minimum boundary is valid

**Requirement:** `REQ-API-003`


> Test: `test_override_current_at_min` in `test_http_api.c:94`

### Override current at maximum boundary is valid

**Requirement:** `REQ-API-003`


> Test: `test_override_current_at_max` in `test_http_api.c:103`

### Current min at boundary (6A) is valid

**Requirement:** `REQ-API-005`


> Test: `test_current_min_valid` in `test_http_api.c:141`

### Current min at 16A is valid

**Requirement:** `REQ-API-005`


> Test: `test_current_min_max` in `test_http_api.c:150`

### Max sum mains zero disables limit

**Requirement:** `REQ-API-006`


> Test: `test_max_sum_mains_zero` in `test_http_api.c:188`

### Max sum mains at minimum (10A) is valid

**Requirement:** `REQ-API-006`


> Test: `test_max_sum_mains_min` in `test_http_api.c:197`

### Max sum mains at maximum (600A) is valid

**Requirement:** `REQ-API-006`


> Test: `test_max_sum_mains_max` in `test_http_api.c:206`

### Stop timer at zero is valid

**Requirement:** `REQ-API-007`


> Test: `test_stop_timer_zero` in `test_http_api.c:244`

### Stop timer at max (60) is valid

**Requirement:** `REQ-API-007`


> Test: `test_stop_timer_max` in `test_http_api.c:253`

### Solar start current at 0 is valid

**Requirement:** `REQ-API-008`


> Test: `test_solar_start_zero` in `test_http_api.c:282`

### Solar start current at 48 is valid

**Requirement:** `REQ-API-008`


> Test: `test_solar_start_max` in `test_http_api.c:291`

### Solar max import at 0 is valid

**Requirement:** `REQ-API-009`


> Test: `test_solar_import_zero` in `test_http_api.c:311`

### PrioStrategy MODBUS_ADDR (0) is valid on master

**Requirement:** `REQ-API-011`

- **Given** A master EVSE (load_bl=0)
- **When** prio_strategy is 0
- **Then** Validation passes

> Test: `test_prio_strategy_valid_0` in `test_http_api.c:331`

### PrioStrategy FIRST_CONNECTED (1) is valid

**Requirement:** `REQ-API-011`


> Test: `test_prio_strategy_valid_1` in `test_http_api.c:343`

### PrioStrategy LAST_CONNECTED (2) is valid

**Requirement:** `REQ-API-011`


> Test: `test_prio_strategy_valid_2` in `test_http_api.c:352`

### RotationInterval 0 (disabled) is valid

**Requirement:** `REQ-API-012`

- **Given** A master EVSE (load_bl=0)
- **When** rotation_interval is 0
- **Then** Validation passes

> Test: `test_rotation_interval_zero` in `test_http_api.c:393`

### RotationInterval at minimum (30) is valid

**Requirement:** `REQ-API-012`


> Test: `test_rotation_interval_min` in `test_http_api.c:405`

### RotationInterval at maximum (1440) is valid

**Requirement:** `REQ-API-012`


> Test: `test_rotation_interval_max` in `test_http_api.c:414`

### IdleTimeout at minimum (30) is valid

**Requirement:** `REQ-API-013`

- **Given** A master EVSE (load_bl=0)
- **When** idle_timeout is 30
- **Then** Validation passes

> Test: `test_idle_timeout_min` in `test_http_api.c:452`

### IdleTimeout at default (60) is valid

**Requirement:** `REQ-API-013`


> Test: `test_idle_timeout_default` in `test_http_api.c:464`

### IdleTimeout at maximum (300) is valid

**Requirement:** `REQ-API-013`


> Test: `test_idle_timeout_max` in `test_http_api.c:473`

---

## HTTP API Settings Validation

### Valid settings request passes validation

**Requirement:** `REQ-API-010`

- **Given** A settings request with valid current_min and override_current
- **When** Validated against current limits
- **Then** No errors are returned

> Test: `test_validate_settings_valid` in `test_http_api.c:511`

### Invalid current_min in combined request

**Requirement:** `REQ-API-010`


> Test: `test_validate_settings_invalid_min` in `test_http_api.c:532`

### Multiple invalid fields

**Requirement:** `REQ-API-010`


> Test: `test_validate_settings_multiple_errors` in `test_http_api.c:549`

### Empty request passes validation

**Requirement:** `REQ-API-010`


> Test: `test_validate_settings_empty` in `test_http_api.c:567`

### Slave restrictions applied

**Requirement:** `REQ-API-010`


> Test: `test_validate_settings_slave_restrictions` in `test_http_api.c:581`

### Valid scheduling settings in combined request

**Requirement:** `REQ-API-014`

- **Given** A settings request with valid scheduling fields
- **When** Validated on master (load_bl=1)
- **Then** No errors are returned

> Test: `test_validate_settings_scheduling_valid` in `test_http_api.c:597`

### Invalid scheduling settings on slave

**Requirement:** `REQ-API-014`

- **Given** A settings request with scheduling fields
- **When** Validated on slave (load_bl=2)
- **Then** All three scheduling fields produce errors

> Test: `test_validate_settings_scheduling_slave` in `test_http_api.c:620`

---

## EVCC IEC 61851 State Mapping

### STATE_A maps to IEC 61851 state A (standby)

**Requirement:** `REQ-API-020`

- **Given** The EVSE is in STATE_A with no errors
- **When** evse_state_to_iec61851 is called
- **Then** It returns 'A'

> Test: `test_iec61851_state_a` in `test_http_api.c:645`

### STATE_B maps to IEC 61851 state B (vehicle detected)

**Requirement:** `REQ-API-020`

- **Given** The EVSE is in STATE_B with no errors
- **When** evse_state_to_iec61851 is called
- **Then** It returns 'B'

> Test: `test_iec61851_state_b` in `test_http_api.c:657`

### STATE_C maps to IEC 61851 state C (charging)

**Requirement:** `REQ-API-020`

- **Given** The EVSE is in STATE_C with no errors
- **When** evse_state_to_iec61851 is called
- **Then** It returns 'C'

> Test: `test_iec61851_state_c` in `test_http_api.c:669`

### STATE_D maps to IEC 61851 state D (ventilation required)

**Requirement:** `REQ-API-020`

- **Given** The EVSE is in STATE_D with no errors
- **When** evse_state_to_iec61851 is called
- **Then** It returns 'D'

> Test: `test_iec61851_state_d` in `test_http_api.c:681`

### STATE_B1 maps to IEC 61851 state B (connected, EVSE not ready)

**Requirement:** `REQ-API-021`

- **Given** The EVSE is in STATE_B1 (no PWM signal) with no errors
- **When** evse_state_to_iec61851 is called
- **Then** It returns 'B' because the vehicle is connected

> Test: `test_iec61851_state_b1` in `test_http_api.c:693`

### STATE_C1 maps to IEC 61851 state C (charge stopping)

**Requirement:** `REQ-API-021`

- **Given** The EVSE is in STATE_C1 (stopping) with no errors
- **When** evse_state_to_iec61851 is called
- **Then** It returns 'C' because charging session is still active

> Test: `test_iec61851_state_c1` in `test_http_api.c:705`

### Communication and modem states map to B (connected)

**Requirement:** `REQ-API-021`

- **Given** The EVSE is in various communication/modem states
- **When** evse_state_to_iec61851 is called for each
- **Then** All return 'B' because the vehicle is connected but not yet charging

> Test: `test_iec61851_comm_modem_states` in `test_http_api.c:717`

### Modem denied maps to E (error)

**Requirement:** `REQ-API-022`

- **Given** The EVSE is in STATE_MODEM_DENIED
- **When** evse_state_to_iec61851 is called
- **Then** It returns 'E' because access was denied

> Test: `test_iec61851_modem_denied` in `test_http_api.c:736`

### Hard error flags override state to E (error)

**Requirement:** `REQ-API-022`

- **Given** The EVSE is in STATE_C (charging) with RCM_TRIPPED error
- **When** evse_state_to_iec61851 is called
- **Then** It returns 'E' because a hard error takes priority

> Test: `test_iec61851_hard_error_overrides_state` in `test_http_api.c:748`

### Soft errors (LESS_6A, NO_SUN) do NOT override state

**Requirement:** `REQ-API-022`

- **Given** The EVSE is in STATE_C with LESS_6A or STATE_A with NO_SUN
- **When** evse_state_to_iec61851 is called
- **Then** It returns the state-based letter, not 'E'

> Test: `test_iec61851_soft_errors_no_override` in `test_http_api.c:763`

### NOSTATE and unknown values map to F (not available)

**Requirement:** `REQ-API-023`

- **Given** The EVSE is in NOSTATE or an unrecognized state value
- **When** evse_state_to_iec61851 is called
- **Then** It returns 'F' indicating EVSE not available

> Test: `test_iec61851_nostate_and_unknown` in `test_http_api.c:777`

---

## EVCC Charging Enabled

### STATE_C means charging is enabled

**Requirement:** `REQ-API-025`

- **Given** The EVSE is in STATE_C (charging)
- **When** evse_charging_enabled is called
- **Then** It returns true

> Test: `test_charging_enabled_state_c` in `test_http_api.c:792`

### STATE_C1 means charging is enabled (stopping phase)

**Requirement:** `REQ-API-025`

- **Given** The EVSE is in STATE_C1 (charge stopping)
- **When** evse_charging_enabled is called
- **Then** It returns true because energy is still being delivered

> Test: `test_charging_enabled_state_c1` in `test_http_api.c:804`

### Non-charging states return false

**Requirement:** `REQ-API-025`

- **Given** The EVSE is in STATE_A, STATE_B, or other non-charging states
- **When** evse_charging_enabled is called
- **Then** It returns false

> Test: `test_charging_enabled_non_charging_states` in `test_http_api.c:816`

---

## EVCC Phase Switch Validation

### Valid 1-phase switch request on standalone with C2 contactor

**Requirement:** `REQ-API-024`

- **Given** A standalone EVSE (load_bl=0) with EnableC2=AUTO
- **When** A phase switch to 1 phase is requested
- **Then** Validation passes (returns NULL)

> Test: `test_phase_switch_valid_1p` in `test_http_api.c:838`

### Valid 3-phase switch request on master with C2 contactor

**Requirement:** `REQ-API-024`

- **Given** A master EVSE (load_bl=1) with EnableC2=ALWAYS_ON
- **When** A phase switch to 3 phases is requested
- **Then** Validation passes (returns NULL)

> Test: `test_phase_switch_valid_3p` in `test_http_api.c:851`

### Invalid phase count (2) is rejected

**Requirement:** `REQ-API-025`

- **Given** A standalone EVSE with EnableC2=AUTO
- **When** A phase switch to 2 phases is requested
- **Then** Validation fails with error message

> Test: `test_phase_switch_invalid_phase_count` in `test_http_api.c:864`

### Phase switch rejected when C2 contactor not present

**Requirement:** `REQ-API-025`

- **Given** An EVSE with EnableC2=NOT_PRESENT (no C2 hardware)
- **When** A phase switch to 1 phase is requested
- **Then** Validation fails because hardware cannot switch phases

> Test: `test_phase_switch_no_c2_hardware` in `test_http_api.c:877`

### Phase switch rejected on slave node

**Requirement:** `REQ-API-025`

- **Given** A slave EVSE (load_bl=2) with EnableC2=AUTO
- **When** A phase switch to 3 phases is requested
- **Then** Validation fails because slaves cannot initiate phase switching

> Test: `test_phase_switch_slave_rejected` in `test_http_api.c:890`

### Phase switch with zero phases is rejected

**Requirement:** `REQ-API-025`

- **Given** A standalone EVSE with EnableC2=AUTO
- **When** A phase switch to 0 phases is requested
- **Then** Validation fails

> Test: `test_phase_switch_zero_phases` in `test_http_api.c:903`

### Phase switch valid with all non-NOT_PRESENT EnableC2 values

**Requirement:** `REQ-API-024`

- **Given** A standalone EVSE with various EnableC2 settings (ALWAYS_OFF, SOLAR_OFF, ALWAYS_ON, AUTO)
- **When** A phase switch to 1 phase is requested
- **Then** Validation passes for all C2 configurations that have hardware present

> Test: `test_phase_switch_all_c2_configs` in `test_http_api.c:916`

---

## LB Convergence

### Standalone Smart mode converges to target within 20 cycles

**Requirement:** `REQ-LB-020`

- **Given** A standalone EVSE in Smart mode with 25A mains limit and 50dA baseload
- **When** 20 regulation cycles are simulated with meter feedback
- **Then** IsetBalanced stabilizes within 5 deciamps of the expected target (200dA)

> Test: `test_smart_standalone_converges_to_target` in `test_lb_convergence.c:1`

### Smart mode converges monotonically when starting below target

**Requirement:** `REQ-LB-021`

- **Given** A standalone EVSE in Smart mode starting at MinCurrent (60dA) with headroom
- **When** 10 regulation cycles are simulated
- **Then** IsetBalanced increases each cycle (monotonic convergence upward)

> Test: `test_smart_standalone_monotonic_increase` in `test_lb_convergence.c:166`

### Smart mode recovers when mains load increases mid-session

**Requirement:** `REQ-LB-022`

- **Given** A standalone EVSE converged to 200dA with 5A baseload
- **When** Baseload suddenly increases by 100dA (10A) reducing available capacity
- **Then** After 20 more cycles, IsetBalanced settles near the new target (100dA)

> Test: `test_smart_standalone_recovers_from_load_increase` in `test_lb_convergence.c:199`

### Smart mode recovers when mains load decreases mid-session

**Requirement:** `REQ-LB-023`

- **Given** A standalone EVSE converged with 15A baseload
- **When** Baseload drops by 100dA (10A) increasing available capacity
- **Then** After 20 more cycles, IsetBalanced settles near the new higher target

> Test: `test_smart_standalone_recovers_from_load_decrease` in `test_lb_convergence.c:221`

### Two EVSEs in Normal mode converge to equal distribution

**Requirement:** `REQ-LB-024`

- **Given** Master with 2 EVSEs in Normal mode, MaxCircuit=32A, no baseload
- **When** 5 regulation cycles are simulated
- **Then** Both EVSEs receive equal current within 1 deciamp

> Test: `test_two_evse_normal_converges_equal` in `test_lb_convergence.c:246`

### Two EVSEs in Smart mode converge to fair sharing

**Requirement:** `REQ-LB-025`

- **Given** Master with 2 EVSEs in Smart mode, 25A mains limit, 5A baseload
- **When** 20 regulation cycles are simulated with meter feedback
- **Then** Both EVSEs receive current within 5dA of each other

> Test: `test_two_evse_smart_converges_fair` in `test_lb_convergence.c:294`

### Four EVSEs in Smart mode converge with sufficient headroom

**Requirement:** `REQ-LB-026`

- **Given** Master with 4 EVSEs in Smart mode, 50A mains limit, 5A baseload
- **When** 30 regulation cycles are simulated with meter feedback
- **Then** All 4 EVSEs receive current within 10dA of each other

> Test: `test_four_evse_smart_converges` in `test_lb_convergence.c:316`

### Third EVSE joining mid-session causes redistribution

**Requirement:** `REQ-LB-027`

- **Given** Master with 2 EVSEs converged in Smart mode
- **When** A third EVSE starts charging (mod=1) and 20 more cycles run
- **Then** All 3 EVSEs converge to fair sharing within 10dA

> Test: `test_third_evse_joining_reconverges` in `test_lb_convergence.c:343`

### EVSE disconnecting causes fair redistribution to remaining

**Requirement:** `REQ-LB-028`

- **Given** Master with 3 EVSEs converged in Smart mode
- **When** EVSE 2 disconnects and 20 more cycles run
- **Then** Remaining 2 EVSEs converge to fair sharing, each getting more than before

> Test: `test_evse_disconnect_reconverges` in `test_lb_convergence.c:382`

### MaxMains limits total EVSE allocation in Smart mode

**Requirement:** `REQ-LB-029`

- **Given** Standalone EVSE in Smart mode, MaxMains=10A, 5A baseload
- **When** 20 regulation cycles are simulated
- **Then** IsetBalanced converges to MinCurrent (60dA) since available (50dA) < MinCurrent

> Test: `test_maxmains_caps_convergence` in `test_lb_convergence.c:414`

### Tight capacity with 4 EVSEs triggers priority scheduling

**Requirement:** `REQ-LB-030`

- **Given** Master with 4 EVSEs, only enough power for 2 at MinCurrent
- **When** 10 regulation cycles are simulated
- **Then** NoCurrent stays at 0 (priority scheduling handles shortage gracefully)

> Test: `test_tight_capacity_four_evse_priority` in `test_lb_convergence.c:436`

### Hard shortage with single EVSE triggers NoCurrent

**Requirement:** `REQ-LB-031`

- **Given** Standalone EVSE in Smart mode, mains heavily overloaded
- **When** Multiple regulation cycles are simulated with baseload exceeding MaxMains
- **Then** NoCurrent counter increments indicating sustained shortage

> Test: `test_hard_shortage_standalone_triggers_nocurrent` in `test_lb_convergence.c:463`

### MaxCircuit limits per-EVSE allocation independently of MaxMains

**Requirement:** `REQ-LB-032`

- **Given** Standalone EVSE in Smart mode, MaxCircuit=10A, MaxMains=50A, 3A baseload
- **When** 20 regulation cycles are simulated
- **Then** Balanced[0] does not exceed MaxCircuit*10 (100dA)

> Test: `test_maxcircuit_limits_convergence` in `test_lb_convergence.c:481`

### MaxSumMains limit overrides MaxMains when configured

**Requirement:** `REQ-LB-033`

- **Given** Standalone EVSE in Smart mode with MaxSumMains=15A
- **When** 20 regulation cycles are simulated with Isum near the limit
- **Then** IsetBalanced is constrained by MaxSumMains, not just MaxMains

> Test: `test_maxsummains_constrains_convergence` in `test_lb_convergence.c:499`

### Solar mode converges to export surplus

**Requirement:** `REQ-LB-034`

- **Given** Standalone EVSE in Solar mode, large export (Isum negative)
- **When** 30 regulation cycles are simulated with meter feedback
- **Then** IsetBalanced increases to absorb available solar surplus

> Test: `test_solar_standalone_converges_to_surplus` in `test_lb_convergence.c:522`

### Solar mode with ImportCurrent allows partial grid import

**Requirement:** `REQ-LB-035`

- **Given** Standalone EVSE in Solar mode with ImportCurrent=6A
- **When** 30 regulation cycles run with modest solar export
- **Then** EVSE charges above pure-solar level due to allowed import

> Test: `test_solar_import_current_allows_grid_use` in `test_lb_convergence.c:541`

### Converged Smart mode EVSE remains stable (no oscillation)

**Requirement:** `REQ-LB-036`

- **Given** A standalone EVSE converged in Smart mode
- **When** 20 additional regulation cycles run with constant conditions
- **Then** IsetBalanced varies by no more than 5dA across all cycles

> Test: `test_smart_stability_no_oscillation` in `test_lb_convergence.c:564`

### Two-EVSE Smart mode remains stable after convergence

**Requirement:** `REQ-LB-037`

- **Given** Master with 2 EVSEs converged in Smart mode
- **When** 20 additional regulation cycles run with constant conditions
- **Then** Balanced[0] and Balanced[1] each vary by no more than 5dA

> Test: `test_two_evse_stability_no_oscillation` in `test_lb_convergence.c:595`

### Oscillation detection increments OscillationCount on sign flip

**Requirement:** `REQ-LB-038`

- **Given** Standalone EVSE in Smart mode with alternating positive/negative Idifference
- **When** Regulation cycles produce sign flips in Idifference
- **Then** OscillationCount increments, indicating detected oscillation

> Test: `test_oscillation_detected_on_sign_flip` in `test_lb_convergence.c:632`

### Adaptive gain increases effective divisor during oscillation

**Requirement:** `REQ-LB-039`

- **Given** Standalone EVSE in Smart mode with OscillationCount > 0
- **When** Regulation cycle runs with positive Idifference
- **Then** IsetBalanced increases by less than Idifference/RampRateDivisor

> Test: `test_adaptive_gain_reduces_step_during_oscillation` in `test_lb_convergence.c:669`

### OscillationCount decays when no sign flip occurs

**Requirement:** `REQ-LB-040`

- **Given** Standalone EVSE in Smart mode with OscillationCount = 5
- **When** Multiple consecutive regulation cycles have same-sign Idifference
- **Then** OscillationCount decays back toward 0

> Test: `test_oscillation_count_decays_when_stable` in `test_lb_convergence.c:703`

### Adaptive gain improves convergence under alternating load

**Requirement:** `REQ-LB-041`

- **Given** Standalone EVSE in Smart mode with alternating baseload (simulating noisy grid)
- **When** 40 regulation cycles are simulated with alternating +-20dA baseload noise
- **Then** IsetBalanced peak-to-peak oscillation is less than 30dA (dampened)

> Test: `test_adaptive_gain_dampens_noisy_load` in `test_lb_convergence.c:732`

### Normal mode is unaffected by adaptive gain

**Requirement:** `REQ-LB-042`

- **Given** Standalone EVSE in Normal mode
- **When** Regulation cycles run
- **Then** OscillationCount remains 0 (adaptive gain only applies to Smart/Solar)

> Test: `test_normal_mode_no_adaptive_gain` in `test_lb_convergence.c:766`

### EMA filter smooths Idifference spikes

**Requirement:** `REQ-LB-043`

- **Given** Standalone EVSE in Smart mode converged at stable load
- **When** A single large Idifference spike occurs (sudden mains change)
- **Then** The filtered Idifference used for regulation is less than the raw spike

> Test: `test_ema_filter_smooths_spike` in `test_lb_convergence.c:803`

### EMA filter preserves convergence (no regression)

**Requirement:** `REQ-LB-044`

- **Given** Standalone EVSE in Smart mode with EMA filtering active
- **When** 30 regulation cycles run
- **Then** IsetBalanced converges to target within 10 dA (may be slower but still converges)

> Test: `test_ema_filter_still_converges` in `test_lb_convergence.c:834`

### EMA filter reduces peak-to-peak swing under noisy measurements

**Requirement:** `REQ-LB-045`

- **Given** Standalone EVSE in Smart mode converged
- **When** 40 cycles with +-30dA measurement noise are simulated
- **Then** Peak-to-peak IsetBalanced swing is at most 30dA (~50% of raw 60dA noise)

> Test: `test_ema_filter_reduces_noise_swing` in `test_lb_convergence.c:853`

### EMA filter tracks sustained load change within 10 cycles

**Requirement:** `REQ-LB-046`

- **Given** Standalone EVSE in Smart mode converged at 5A baseload
- **When** Baseload increases permanently by 100dA (10A)
- **Then** After 10 cycles, IsetBalanced has moved at least 50% toward new target

> Test: `test_ema_filter_tracks_sustained_change` in `test_lb_convergence.c:883`

### Distribution smoothing clamps per-EVSE current change

**Requirement:** `REQ-LB-047`

- **Given** Master with 2 EVSEs in Smart mode, converged to 100dA each
- **When** IsetBalanced suddenly jumps to 320dA (large headroom increase)
- **Then** Each EVSE Balanced[] changes by at most MAX_DELTA_PER_CYCLE (30dA) per cycle

> Test: `test_distribution_smoothing_clamps_increase` in `test_lb_convergence.c:914`

### Distribution smoothing clamps per-EVSE current decrease

**Requirement:** `REQ-LB-048`

- **Given** Master with 2 EVSEs in Smart mode, converged to 200dA each
- **When** IsetBalanced suddenly drops (mains overloaded)
- **Then** Each EVSE Balanced[] decreases by at most MAX_DELTA_PER_CYCLE per cycle

> Test: `test_distribution_smoothing_clamps_decrease` in `test_lb_convergence.c:944`

### Distribution smoothing is skipped for mod=1 (new EVSE joining)

**Requirement:** `REQ-LB-049`

- **Given** Master with 2 EVSEs, EVSE 1 just joined with mod=1
- **When** Balanced current is calculated with mod=1
- **Then** Balanced[] values are NOT clamped (full redistribution allowed)

> Test: `test_distribution_smoothing_skipped_on_mod1` in `test_lb_convergence.c:973`

### Distribution smoothing still converges within 20 cycles

**Requirement:** `REQ-LB-050`

- **Given** Master with 2 EVSEs in Smart mode, starting from MinCurrent
- **When** 20 regulation cycles with distribution smoothing
- **Then** Both EVSEs converge to fair sharing within 10dA

> Test: `test_distribution_smoothing_still_converges` in `test_lb_convergence.c:1007`

### BalancedPrev tracks previous cycle values

**Requirement:** `REQ-LB-051`

- **Given** Master with 2 EVSEs after a regulation cycle
- **When** A second regulation cycle runs
- **Then** BalancedPrev[] matches the Balanced[] values from the previous cycle

> Test: `test_balanced_prev_tracks_previous` in `test_lb_convergence.c:1026`

### LB diagnostic snapshot populated after regulation cycle

**Requirement:** `REQ-LB-052`

- **Given** Master with 2 EVSEs in Smart mode after regulation
- **When** evse_calc_balanced_current completes
- **Then** lb_diag contains correct IsetBalanced, ActiveEVSE, and Balanced[] values

> Test: `test_lb_diag_snapshot_populated` in `test_lb_convergence.c:1058`

### LB diagnostic captures shortage state

**Requirement:** `REQ-LB-053`

- **Given** Master with 4 EVSEs in Smart mode under hard shortage
- **When** Regulation cycle detects insufficient power
- **Then** lb_diag.Shortage is true and lb_diag.NoCurrent > 0

> Test: `test_lb_diag_captures_shortage` in `test_lb_convergence.c:1076`

### LB diagnostic captures oscillation count

**Requirement:** `REQ-LB-054`

- **Given** Standalone EVSE with OscillationCount elevated
- **When** Regulation cycle completes
- **Then** lb_diag.OscillationCount matches ctx.OscillationCount

> Test: `test_lb_diag_captures_oscillation` in `test_lb_convergence.c:1094`

### LB diagnostic captures delta clamping state

**Requirement:** `REQ-LB-055`

- **Given** Master with 2 EVSEs where distribution smoothing will clamp
- **When** Large current change triggers clamping
- **Then** lb_diag.DeltaClamped is true

> Test: `test_lb_diag_captures_delta_clamped` in `test_lb_convergence.c:1116`

### Eight EVSEs in Normal mode receive fair distribution

**Requirement:** `REQ-LB-056`

- **Given** Master with 8 EVSEs all in STATE_C, MaxCircuit=64A
- **When** Regulation cycles complete
- **Then** All 8 EVSEs receive equal current (80dA = 8A each)

> Test: `test_eight_evse_normal_fair` in `test_lb_convergence.c:1147`

### Eight EVSEs in Smart mode converge with sufficient headroom

**Requirement:** `REQ-LB-057`

- **Given** Master with 8 EVSEs in Smart mode, 80A mains, 5A baseload
- **When** 40 regulation cycles are simulated
- **Then** All 8 EVSEs receive current within 10dA of each other

> Test: `test_eight_evse_smart_converges` in `test_lb_convergence.c:1188`

### Eight EVSEs with varying BalancedMax distribute fairly

**Requirement:** `REQ-LB-058`

- **Given** Master with 8 EVSEs, each with different BalancedMax (60-320dA)
- **When** Regulation cycles complete in Normal mode
- **Then** Each EVSE is capped at its BalancedMax, total equals IsetBalanced

> Test: `test_eight_evse_varying_max` in `test_lb_convergence.c:1214`

### Eight EVSEs: sequential join cycle

**Requirement:** `REQ-LB-059`

- **Given** Master starts with 2 EVSEs, then adds one per cycle up to 8
- **When** Each new EVSE joins with mod=1 followed by 5 regulation cycles
- **Then** After all 8 are active, distribution is fair within 10dA

> Test: `test_eight_evse_sequential_join` in `test_lb_convergence.c:1259`

### Eight EVSEs: sequential leave cycle

**Requirement:** `REQ-LB-060`

- **Given** Master with 8 EVSEs converged in Smart mode
- **When** EVSEs disconnect one by one (7 down to 2)
- **Then** Remaining EVSEs get progressively more current

> Test: `test_eight_evse_sequential_leave` in `test_lb_convergence.c:1304`

### Eight EVSEs under tight capacity: priority scheduling

**Requirement:** `REQ-LB-061`

- **Given** Master with 8 EVSEs, only enough power for 3 at MinCurrent
- **When** Regulation cycles run
- **Then** At most 3 EVSEs are active, others are paused, NoCurrent stays 0

> Test: `test_eight_evse_tight_capacity_priority` in `test_lb_convergence.c:1330`

### EVSE converges with 2-cycle vehicle response delay

**Requirement:** `REQ-LB-062`

- **Given** Standalone EVSE in Smart mode with simulated vehicle response lag
- **When** 80 regulation cycles with vehicle response model
- **Then** IsetBalanced converges to target within 30dA despite lag

> Test: `test_vehicle_response_delay_converges` in `test_lb_convergence.c:1359`

### Vehicle lag with noise does not cause LESS_6A error

**Requirement:** `REQ-LB-063`

- **Given** Standalone EVSE with vehicle response model and 5dA noise
- **When** 40 cycles run after convergence
- **Then** No LESS_6A error is triggered and EVSE keeps charging

> Test: `test_vehicle_response_stable_with_noise` in `test_lb_convergence.c:1426`

### Two EVSEs converge with vehicle response model

**Requirement:** `REQ-LB-064`

- **Given** Master with 2 EVSEs, both with vehicle response lag
- **When** 80 regulation cycles with vehicle response simulation
- **Then** Both EVSEs receive equal current and are above MinCurrent

> Test: `test_two_evse_vehicle_response_converges` in `test_lb_convergence.c:1452`

### Vehicle response model with load step recovers

**Requirement:** `REQ-LB-065`

- **Given** Standalone EVSE converged with vehicle model
- **When** Baseload suddenly increases by 100dA
- **Then** After 30 cycles with vehicle lag, IsetBalanced settles near new target

> Test: `test_vehicle_response_load_step_recovery` in `test_lb_convergence.c:1475`

### Heavy measurement noise with vehicle lag doesn't cause NoCurrent

**Requirement:** `REQ-LB-066`

- **Given** Standalone EVSE with vehicle model and 10dA measurement noise
- **When** 50 regulation cycles run
- **Then** NoCurrent stays below NoCurrentThreshold (no false LESS_6A errors)

> Test: `test_vehicle_response_noise_no_false_shortage` in `test_lb_convergence.c:1498`

---

## LED Status Indication

### RCM tripped error produces red flashing pattern on ESP32

**Requirement:** `REQ-LED-001`

- **Given** ErrorFlags has RCM_TRIPPED set on ESP32 platform
- **When** led_compute_color is called repeatedly
- **Then** LED alternates between red and off

> Test: `test_error_rcm_tripped_esp32` in `test_led_color.c:1`

### CT_NOCOMM error shows red flashing

**Requirement:** `REQ-LED-001`

- **Given** ErrorFlags has CT_NOCOMM set
- **When** led_compute_color is called multiple times
- **Then** LED flashes red

> Test: `test_error_ct_nocomm` in `test_led_color.c:82`

### TEMP_HIGH error shows red flashing

**Requirement:** `REQ-LED-001`

- **Given** ErrorFlags has TEMP_HIGH set
- **When** led_compute_color is called
- **Then** LED flashes red

> Test: `test_error_temp_high` in `test_led_color.c:105`

### CH32 RCM mismatch with no test counter shows error

**Requirement:** `REQ-LED-001`

- **Given** CH32 platform with RCM_TRIPPED set but not RCM_TEST, counter=0
- **When** led_compute_color is called
- **Then** LED shows red (error condition)

> Test: `test_error_ch32_rcm_mismatch` in `test_led_color.c:126`

### CH32 RCM test in progress does not show error flash

**Requirement:** `REQ-LED-001`

- **Given** CH32 platform with RCM_TRIPPED set, counter > 0 (test running)
- **When** led_compute_color is called
- **Then** LED does NOT show rapid red error flash (enters waiting blink instead)

> Test: `test_no_error_ch32_rcm_test_active` in `test_led_color.c:149`

### Access OFF shows off color

**Requirement:** `REQ-LED-002`

- **Given** Access status is OFF, no custom button
- **When** led_compute_color is called
- **Then** LED shows ColorOff values

> Test: `test_access_off_default` in `test_led_color.c:181`

### MODEM_DENIED state shows off color

**Requirement:** `REQ-LED-002`

- **Given** State is STATE_MODEM_DENIED, access ON
- **When** led_compute_color is called
- **Then** LED shows ColorOff values (same as access OFF)

> Test: `test_modem_denied_shows_off` in `test_led_color.c:224`

### Solar mode with charge delay shows slow blink

**Requirement:** `REQ-LED-003`

- **Given** Solar mode, ChargeDelay > 0, no errors
- **When** led_compute_color is called repeatedly
- **Then** LED blinks with solar color (orange)

> Test: `test_waiting_solar_blink` in `test_led_color.c:247`

### Smart mode waiting shows smart color blink

**Requirement:** `REQ-LED-003`

- **Given** Smart mode, ChargeDelay > 0
- **When** led_compute_color is called when LED is on
- **Then** Color matches ColorSmart

> Test: `test_waiting_smart_color` in `test_led_color.c:271`

### State A shows dimmed LED

**Requirement:** `REQ-LED-005`

- **Given** State A, Normal mode, no errors
- **When** led_compute_color is called
- **Then** LED shows dimmed green (STATE_A_LED_BRIGHTNESS)

> Test: `test_state_a_dimmed` in `test_led_color.c:320`

### State B shows full brightness LED

**Requirement:** `REQ-LED-005`

- **Given** State B, Normal mode, no errors
- **When** led_compute_color is called
- **Then** LED shows full brightness green

> Test: `test_state_b_full_brightness` in `test_led_color.c:340`

### State B1 shows full brightness (same as B)

**Requirement:** `REQ-LED-005`

- **Given** State B1, Normal mode
- **When** led_compute_color is called
- **Then** LED shows full brightness green

> Test: `test_state_b1_full_brightness` in `test_led_color.c:359`

### State B sets led_count to 128 for smooth C transition

**Requirement:** `REQ-LED-005`

- **Given** State B entered
- **When** led_compute_color is called
- **Then** led_count is set to 128

> Test: `test_state_b_sets_count_128` in `test_led_color.c:378`

### State C shows breathing animation

**Requirement:** `REQ-LED-006`

- **Given** State C, Normal mode
- **When** led_compute_color is called multiple times
- **Then** LED brightness varies (breathing effect)

> Test: `test_state_c_breathing` in `test_led_color.c:395`

### State C Solar mode has slower breathing

**Requirement:** `REQ-LED-006`

- **Given** State C, Solar mode
- **When** led_compute_color is called
- **Then** led_count increments by 1 per call (vs 2 for other modes)

> Test: `test_state_c_solar_slower` in `test_led_color.c:418`

---

## LED Color Configuration

### Custom button active when access OFF shows custom color

**Requirement:** `REQ-LED-004`

- **Given** Access OFF and CustomButton is true
- **When** led_compute_color is called
- **Then** LED shows ColorCustom values

> Test: `test_access_off_custom_button` in `test_led_color.c:204`

### Custom button waiting shows custom color

**Requirement:** `REQ-LED-004`

- **Given** Custom button active, ChargeDelay > 0
- **When** led_compute_color is called when LED is on
- **Then** Color matches ColorCustom

> Test: `test_waiting_custom_button` in `test_led_color.c:294`

### Solar mode State A shows solar orange (dimmed)

**Requirement:** `REQ-LED-004`

- **Given** State A, Solar mode
- **When** led_compute_color is called
- **Then** LED shows orange tint at STATE_A brightness

> Test: `test_state_a_solar_color` in `test_led_color.c:445`

### Custom button overrides mode color in State B

**Requirement:** `REQ-LED-004`

- **Given** State B, Normal mode, CustomButton true
- **When** led_compute_color is called
- **Then** LED shows custom blue at full brightness

> Test: `test_state_b_custom_override` in `test_led_color.c:466`

---

## Load Balancing

### Single standalone EVSE receives full MaxCurrent allocation

**Requirement:** `REQ-LB-001`

- **Given** A standalone EVSE (LoadBl=0) in STATE_C with MaxCurrent=16A
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced is set to 160 (16A in tenths)

> Test: `test_single_evse_gets_full_current` in `test_load_balancing.c:1`

### Two EVSEs receive equal current distribution

**Requirement:** `REQ-LB-002`

- **Given** Two EVSEs are charging as master (LoadBl=1) with equal BalancedMax
- **When** evse_calc_balanced_current is called
- **Then** Both EVSEs receive equal Balanced current allocations

> Test: `test_two_evse_equal_distribution` in `test_load_balancing.c:66`

### Two EVSEs respect MaxCircuit total capacity limit

**Requirement:** `REQ-LB-003`

- **Given** Two EVSEs are charging with MaxCircuit=16A total
- **When** evse_calc_balanced_current is called
- **Then** Each EVSE receives at most half the circuit capacity

> Test: `test_two_evse_respects_max_circuit` in `test_load_balancing.c:82`

### Individual EVSE BalancedMax caps its current allocation

**Requirement:** `REQ-LB-004`

- **Given** Two EVSEs are charging with EVSE 1 having BalancedMax=60 (6A)
- **When** evse_calc_balanced_current is called
- **Then** EVSE 1 Balanced is capped at its BalancedMax of 60

> Test: `test_balanced_max_caps_individual` in `test_load_balancing.c:101`

### Each active EVSE receives at least MinCurrent

**Requirement:** `REQ-LB-006`

- **Given** Two EVSEs are both in STATE_A (disconnected) as master
- **Given** Two EVSEs are charging with MinCurrent=6A and limited total capacity
- **When** evse_calc_balanced_current is called
- **When** evse_calc_balanced_current is called
- **Then** NoCurrent and SolarStopTimer are reset to 0
- **Then** Each charging EVSE with non-zero allocation gets at least MinCurrent*10

> Test: `test_minimum_current_enforced` in `test_load_balancing.c:121`

### New EVSE joining (mod=1) triggers full recalculation

**Requirement:** `REQ-LB-007`

- **Given** Two EVSEs in MODE_SMART with existing current distribution
- **When** evse_calc_balanced_current is called with mod=1 (new EVSE joining)
- **Then** IsetBalanced is recalculated from scratch (different from previous value)

> Test: `test_mod1_new_evse_recalculates` in `test_load_balancing.c:167`

### OCPP current limit reduces ChargeCurrent below MaxCurrent

**Requirement:** `REQ-LB-008`

- **Given** A standalone EVSE with OcppCurrentLimit=10A and MaxCurrent=16A
- **When** evse_calc_balanced_current is called
- **Then** ChargeCurrent is capped at 100 (10A in tenths) or below

> Test: `test_ocpp_limit_reduces_charge_current` in `test_load_balancing.c:189`

### OCPP current limit below MinCurrent zeros out ChargeCurrent

**Requirement:** `REQ-LB-009`

- **Given** A standalone EVSE with OcppCurrentLimit=3A and MinCurrent=6A
- **When** evse_calc_balanced_current is called
- **Then** ChargeCurrent is set to 0 (below minimum, cannot charge)

> Test: `test_ocpp_limit_below_min_zeros_current` in `test_load_balancing.c:215`

### OverrideCurrent takes precedence over calculated ChargeCurrent

**Requirement:** `REQ-LB-010`

- **Given** A standalone EVSE with OverrideCurrent=80 (8A) and MaxCurrent=16A
- **When** evse_calc_balanced_current is called
- **Then** ChargeCurrent is set to 80 (override value)

> Test: `test_override_current_takes_precedence` in `test_load_balancing.c:242`

### Current shortage increments NoCurrent counter

**Requirement:** `REQ-LB-011`

- **Given** Two EVSEs in MODE_SMART with mains heavily loaded and low MaxMains
- **When** evse_calc_balanced_current is called with insufficient capacity
- **Then** NoCurrent counter is incremented above 0

> Test: `test_shortage_increments_nocurrent` in `test_load_balancing.c:269`

### No current shortage decays NoCurrent counter

**Requirement:** `REQ-LB-012`

- **Given** Two EVSEs in MODE_SMART with low mains load and high MaxMains
- **When** evse_calc_balanced_current is called with sufficient capacity
- **Then** NoCurrent counter decays by 1 (gradual recovery)

> Test: `test_no_shortage_clears_nocurrent` in `test_load_balancing.c:293`

### Open grid relay caps IsetBalanced at GridRelayMaxSumMains per phase

**Requirement:** `REQ-LB-013`

- **Given** A standalone EVSE in MODE_SMART with GridRelayOpen=true and 3 phases
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced is capped at GridRelayMaxSumMains*10/3

> Test: `test_grid_relay_limits_current` in `test_load_balancing.c:317`

### Node EVSE requests COMM_C instead of transitioning directly to STATE_C

**Requirement:** `REQ-LB-014`

- **Given** The EVSE is configured as a node (LoadBl=2) in STATE_B with DiodeCheck passed
- **When** A 6V pilot signal is sustained for 500ms (vehicle requests charge)
- **Then** The state transitions to STATE_COMM_C (requesting master permission to charge)

> Test: `test_node_requests_comm_c` in `test_load_balancing.c:353`

### Socket mode (Config=0) caps ChargeCurrent by MaxCapacity

**Requirement:** `REQ-LB-F1A`

- **Given** Config=0 (Socket), MaxCurrent=25, MaxCapacity=16, STATE_C
- **When** evse_calc_balanced_current is called
- **Then** ChargeCurrent is capped at 160 (MaxCapacity * 10)

> Test: `test_config_socket_caps_by_maxcapacity` in `test_load_balancing.c:381`

### Fixed Cable mode (Config=1) does NOT cap by MaxCapacity

**Requirement:** `REQ-LB-F1B`

- **Given** Config=1 (Fixed Cable), MaxCurrent=25, MaxCapacity=16, STATE_C
- **When** evse_calc_balanced_current is called
- **Then** ChargeCurrent is 250 (MaxCurrent * 10), not capped by MaxCapacity

> Test: `test_config_fixed_cable_no_maxcapacity_cap` in `test_load_balancing.c:408`

---

## Meter Decoding

### Register size returns correct byte count per data type

**Requirement:** `REQ-MTR-040`

- **Given** The three supported data types
- **When** meter_register_size is called for each
- **Then** INT16 returns 2, INT32 returns 4, FLOAT32 returns 4

> Test: `test_register_size` in `test_meter_decode.c:1`

### HBF_HWF INT32: big-endian 0x00000064 decodes to 100

**Requirement:** `REQ-MTR-041`

- **Given** 4 bytes in big-endian order representing value 100
- **When** meter_combine_bytes is called with ENDIANNESS_HBF_HWF, INT32
- **Then** Output equals 100

> Test: `test_combine_hbf_hwf_int32` in `test_meter_decode.c:41`

### HBF_HWF INT16: big-endian 0x00C8 decodes to 200

**Requirement:** `REQ-MTR-042`

- **Given** 2 bytes in big-endian order representing value 200
- **When** meter_combine_bytes is called with ENDIANNESS_HBF_HWF, INT16
- **Then** Output equals 200

> Test: `test_combine_hbf_hwf_int16` in `test_meter_decode.c:56`

### HBF_HWF FLOAT32: big-endian IEEE 754 42.5f decodes correctly

**Requirement:** `REQ-MTR-043`

- **Given** 4 bytes representing float 42.5 in big-endian
- **When** meter_combine_bytes is called with ENDIANNESS_HBF_HWF, FLOAT32
- **Then** Output float equals 42.5

> Test: `test_combine_hbf_hwf_float32` in `test_meter_decode.c:71`

### LBF_LWF INT32: little-endian 0x64000000 in bytes decodes to 100

**Requirement:** `REQ-MTR-044`

- **Given** 4 bytes in little-endian order representing value 100
- **When** meter_combine_bytes is called with ENDIANNESS_LBF_LWF, INT32
- **Then** Output equals 100

> Test: `test_combine_lbf_lwf_int32` in `test_meter_decode.c:90`

### LBF_LWF INT16: little-endian 0xC800 in bytes decodes to 200

**Requirement:** `REQ-MTR-045`

- **Given** 2 bytes in little-endian order representing value 200
- **When** meter_combine_bytes is called with ENDIANNESS_LBF_LWF, INT16
- **Then** Output equals 200

> Test: `test_combine_lbf_lwf_int16` in `test_meter_decode.c:105`

### HBF_LWF INT32: word-swapped big-endian decodes correctly

**Requirement:** `REQ-MTR-046`

- **Given** 4 bytes: low word [0x00, 0x01] then high word [0x00, 0x00] = value 1
- **When** meter_combine_bytes is called with ENDIANNESS_HBF_LWF, INT32
- **Then** Output equals 1

> Test: `test_combine_hbf_lwf_int32` in `test_meter_decode.c:122`

### LBF_HWF INT32: word-swapped little-endian decodes correctly

**Requirement:** `REQ-MTR-047`

- **Given** 4 bytes: high word [0x00, 0x00] then low word [0x64, 0x00] = value 100
- **When** meter_combine_bytes is called with ENDIANNESS_LBF_HWF, INT32
- **Then** Output equals 100

> Test: `test_combine_lbf_hwf_int32` in `test_meter_decode.c:145`

### INT32 with zero divisor returns raw value

**Requirement:** `REQ-MTR-048`

- **Given** Big-endian INT32 buffer with value 12345
- **When** meter_decode_value is called with divisor=0
- **Then** Result value is 12345

> Test: `test_decode_int32_divisor_zero` in `test_meter_decode.c:166`

### INT32 with positive divisor divides by power of 10

**Requirement:** `REQ-MTR-049`

- **Given** Big-endian INT32 buffer with value 12345
- **When** meter_decode_value is called with divisor=2 (divide by 100)
- **Then** Result value is 123

> Test: `test_decode_int32_positive_divisor` in `test_meter_decode.c:183`

### INT32 with negative divisor multiplies by power of 10

**Requirement:** `REQ-MTR-050`

- **Given** Big-endian INT32 buffer with value 42
- **When** meter_decode_value is called with divisor=-3 (multiply by 1000)
- **Then** Result value is 42000

> Test: `test_decode_int32_negative_divisor` in `test_meter_decode.c:199`

### INT16 positive value with divisor

**Requirement:** `REQ-MTR-051`

- **Given** Big-endian INT16 buffer with value 2500
- **When** meter_decode_value is called with divisor=1 (divide by 10)
- **Then** Result value is 250

> Test: `test_decode_int16_positive` in `test_meter_decode.c:218`

### INT16 sign extension: negative value 0xFFCE = -50

**Requirement:** `REQ-MTR-052`

- **Given** Big-endian INT16 buffer with value -50 (0xFFCE)
- **When** meter_decode_value is called with divisor=0
- **Then** Result value is -50 (sign-extended to 32-bit)

> Test: `test_decode_int16_sign_extension` in `test_meter_decode.c:235`

### FLOAT32 with zero divisor returns truncated integer

**Requirement:** `REQ-MTR-053`

- **Given** Big-endian FLOAT32 buffer with value 230.5
- **When** meter_decode_value is called with divisor=0
- **Then** Result value is 230 (truncated from 230.5)

> Test: `test_decode_float32_divisor_zero` in `test_meter_decode.c:254`

### FLOAT32 with negative divisor: multiply 2.345 by 1000

**Requirement:** `REQ-MTR-054`

- **Given** Big-endian FLOAT32 buffer with value 2.345
- **When** meter_decode_value is called with divisor=-3 (multiply by 1000)
- **Then** Result value is 2345

> Test: `test_decode_float32_negative_divisor` in `test_meter_decode.c:271`

### FLOAT32 with positive divisor: divide 23450.0 by 10

**Requirement:** `REQ-MTR-055`

- **Given** Big-endian FLOAT32 buffer with value 23450.0
- **When** meter_decode_value is called with divisor=1 (divide by 10)
- **Then** Result value is 2345

> Test: `test_decode_float32_positive_divisor` in `test_meter_decode.c:288`

### Index parameter selects correct register from buffer

**Requirement:** `REQ-MTR-056`

- **Given** A buffer with 3 INT32 values in big-endian: [100, 200, 300]
- **When** meter_decode_value is called with index=0, 1, and 2
- **Then** Returns 100, 200, 300 respectively

> Test: `test_decode_index_offset` in `test_meter_decode.c:307`

### INT16 index offset uses 2-byte stride

**Requirement:** `REQ-MTR-057`

- **Given** A buffer with 3 INT16 values in big-endian: [10, 20, 30]
- **When** meter_decode_value is called with index=0, 1, and 2
- **Then** Returns 10, 20, 30 respectively

> Test: `test_decode_int16_index_offset` in `test_meter_decode.c:333`

### Negative INT32 value decodes correctly

**Requirement:** `REQ-MTR-058`

- **Given** Big-endian INT32 buffer with value -1000 (0xFFFFFC18)
- **When** meter_decode_value is called with divisor=0
- **Then** Result value is -1000

> Test: `test_decode_int32_negative` in `test_meter_decode.c:357`

### Negative FLOAT32 value decodes correctly

**Requirement:** `REQ-MTR-059`

- **Given** Big-endian FLOAT32 buffer with value -5.0
- **When** meter_decode_value is called with divisor=0
- **Then** Result value is -5

> Test: `test_decode_float32_negative` in `test_meter_decode.c:376`

### NULL buffer returns invalid result

**Requirement:** `REQ-MTR-060`

- **Given** A NULL buffer pointer
- **When** meter_decode_value is called
- **Then** Result valid is 0

> Test: `test_decode_null_buffer` in `test_meter_decode.c:395`

### Invalid datatype returns invalid result

**Requirement:** `REQ-MTR-061`

- **Given** A valid buffer but datatype=METER_DATATYPE_MAX (out of range)
- **When** meter_decode_value is called
- **Then** Result valid is 0

> Test: `test_decode_invalid_datatype` in `test_meter_decode.c:409`

### Divisor out of pow10 range returns invalid result

**Requirement:** `REQ-MTR-062`

- **Given** A valid buffer with divisor=10 (exceeds pow10_table size)
- **When** meter_decode_value is called
- **Then** Result valid is 0

> Test: `test_decode_divisor_out_of_range` in `test_meter_decode.c:424`

### NULL pointer to meter_combine_bytes does not crash

**Requirement:** `REQ-MTR-063`

- **Given** NULL out and buf pointers
- **When** meter_combine_bytes is called
- **Then** No crash occurs

> Test: `test_combine_null_safety` in `test_meter_decode.c:439`

### Phoenix Contact meter HBF_LWF INT32 current reading

**Requirement:** `REQ-MTR-064`

- **Given** Phoenix Contact response with current 23.12A encoded as 23120 mA in HBF_LWF INT32
- **When** meter_decode_value is called with divisor=3 (divide by 1000 to get 0.1A units)
- **Then** Result value is 23 (23.12A in deciAmpere after /1000 = 23)

> Test: `test_phoenix_contact_current` in `test_meter_decode.c:458`

### Eastron SDM630 HBF_HWF FLOAT32 current reading

**Requirement:** `REQ-MTR-065`

- **Given** Eastron response with 16.5A encoded as IEEE 754 float in HBF_HWF
- **When** meter_decode_value is called with divisor=-3 (multiply by 1000 for mA)
- **Then** Result value is 16500 (mA)

> Test: `test_eastron_float_current` in `test_meter_decode.c:477`

### Orno WE-517 3-phase current reading at register 0x0C

**Requirement:** `REQ-MTR-066`

- **Given** Orno response with 3 phase currents [8.5A, 12.3A, 6.7A] as FLOAT32 HBF_HWF
- **When** meter_decode_value is called for indices 0, 1, 2 with divisor=0
- **Then** Returns 8, 12, 6 (truncated integer amps)

> Test: `test_orno3p_current` in `test_meter_decode.c:496`

### Orno WE-517 total active power reading

**Requirement:** `REQ-MTR-067`

- **Given** Orno response with total power 3456.7W as FLOAT32 HBF_HWF at register 0x1C
- **When** meter_decode_value is called with divisor=0
- **Then** Returns 3456

> Test: `test_orno3p_power` in `test_meter_decode.c:522`

### Orno WE-517 import energy reading in kWh

**Requirement:** `REQ-MTR-068`

- **Given** Orno response with 1234.567 kWh as FLOAT32 HBF_HWF at register 0x0100
- **When** meter_decode_value is called with divisor=-3 (multiply by 1000 to get Wh)
- **Then** Returns 1234567 (Wh)

> Test: `test_orno3p_energy` in `test_meter_decode.c:539`

### Orno WE-517 negative power during export (solar feed-in)

**Requirement:** `REQ-MTR-069`

- **Given** Orno response with -1500.0W as FLOAT32 HBF_HWF
- **When** meter_decode_value is called with divisor=0
- **Then** Returns -1500

> Test: `test_orno3p_negative_power` in `test_meter_decode.c:557`

---

## Meter Timeout & Recovery

### CT_NOCOMM error is set on timeout and cleared when communication restores

**Requirement:** `REQ-METER-001`

- **Given** EVSE is in Smart mode standalone with MainsMeterType=1 and MainsMeterTimeout=0
- **When** A 1-second tick sets CT_NOCOMM, then MainsMeterTimeout is restored to 5 and another tick occurs
- **Then** CT_NOCOMM is set after the first tick and cleared after the second tick

> Test: `test_ct_nocomm_set_then_restored` in `test_meter_recovery.c:1`

### EV_NOCOMM error is set on timeout and cleared when communication restores

**Requirement:** `REQ-METER-002`

- **Given** EVSE is in Smart mode with EVMeterType=1 and EVMeterTimeout=0
- **When** A 1-second tick sets EV_NOCOMM, then EVMeterTimeout is restored to 10 and another tick occurs
- **Then** EV_NOCOMM is set after the first tick and cleared after the second tick

> Test: `test_ev_nocomm_set_then_restored` in `test_meter_recovery.c:51`

### Both CT_NOCOMM and EV_NOCOMM can be set simultaneously and recover independently

**Requirement:** `REQ-METER-003`

- **Given** EVSE is in Smart mode with both MainsMeterTimeout=0 and EVMeterTimeout=0
- **When** Both timeouts expire, then mains meter is restored first, then EV meter is restored
- **Then** Each NOCOMM flag is set and cleared independently as its respective meter recovers

> Test: `test_both_ct_and_ev_nocomm_simultaneously` in `test_meter_recovery.c:77`

### Mains meter timeout during STATE_C triggers transition to STATE_C1

**Requirement:** `REQ-METER-004`

- **Given** EVSE is in Smart mode standalone in STATE_C with high mains load and MainsMeterTimeout=0
- **When** A 1-second tick occurs
- **Then** CT_NOCOMM is set and EVSE transitions from STATE_C to STATE_C1 (charging suspended)

> Test: `test_mains_timeout_during_state_c` in `test_meter_recovery.c:118`

### EV meter timeout during STATE_C triggers transition to STATE_C1

**Requirement:** `REQ-METER-005`

- **Given** EVSE is in Smart mode standalone in STATE_C with EVMeterType=1 and EVMeterTimeout=0
- **When** A 1-second tick occurs
- **Then** EV_NOCOMM is set and EVSE transitions from STATE_C to STATE_C1 (charging suspended)

> Test: `test_ev_timeout_during_state_c` in `test_meter_recovery.c:145`

### MainsMeter timeout on node sets CT_NOCOMM regardless of operating mode

**Requirement:** `REQ-METER-006`

- **Given** EVSE is a node (LoadBl=3) in Normal mode with MainsMeterTimeout=0
- **When** A 1-second tick occurs
- **Then** CT_NOCOMM is set because nodes do not have the MODE_NORMAL guard for timeout checks

> Test: `test_mains_timeout_on_node` in `test_meter_recovery.c:175`

### MainsMeter timeout on standalone in Normal mode does not set CT_NOCOMM

**Requirement:** `REQ-METER-007`

- **Given** EVSE is standalone (LoadBl=0) in MODE_NORMAL with MainsMeterType=1 and MainsMeterTimeout=0
- **When** A 1-second tick occurs
- **Then** CT_NOCOMM is not set because the MODE_NORMAL guard skips the timeout check for master/standalone

> Test: `test_mains_timeout_master_normal_mode_ignored` in `test_meter_recovery.c:196`

### No EV meter installed continuously resets EVMeterTimeout to COMM_EVTIMEOUT

**Requirement:** `REQ-METER-008`

- **Given** EVSE has EVMeterType=0 (no EV meter installed) with EVMeterTimeout artificially lowered
- **When** 1-second ticks occur even with EVMeterTimeout set to 0
- **Then** EVMeterTimeout is always reset to COMM_EVTIMEOUT and EV_NOCOMM is never set

> Test: `test_no_ev_meter_resets_timeout_continuously` in `test_meter_recovery.c:221`

### No mains meter type and standalone resets MainsMeterTimeout to COMM_TIMEOUT

**Requirement:** `REQ-METER-009`

- **Given** EVSE has MainsMeterType=0 and LoadBl=0 with MainsMeterTimeout artificially lowered to 3
- **When** A 1-second tick occurs
- **Then** MainsMeterTimeout is reset to COMM_TIMEOUT because no mains meter is configured

> Test: `test_no_mains_meter_resets_timeout_continuously` in `test_meter_recovery.c:248`

### Temperature recovery requires strictly below hysteresis boundary

**Requirement:** `REQ-METER-010`

- **Given** EVSE has TEMP_HIGH error with maxTemp=65 and TempEVSE=55 (exactly at maxTemp-10)
- **When** A 1-second tick occurs
- **Then** TEMP_HIGH error remains set because recovery requires TempEVSE < (maxTemp - 10), not <=

> Test: `test_temp_recovery_exactly_at_boundary` in `test_meter_recovery.c:269`

### Temperature recovery clears TEMP_HIGH when one degree below hysteresis boundary

**Requirement:** `REQ-METER-011`

- **Given** EVSE has TEMP_HIGH error with maxTemp=65 and TempEVSE=54 (one below maxTemp-10)
- **When** A 1-second tick occurs
- **Then** TEMP_HIGH error is cleared because 54 < 55 (maxTemp - 10) satisfies the recovery condition

> Test: `test_temp_recovery_one_below_boundary` in `test_meter_recovery.c:288`

### Mains meter countdown sequence from 3 to 0 then CT_NOCOMM on next tick

**Requirement:** `REQ-METER-012`

- **Given** EVSE is in Smart mode standalone with MainsMeterType=1 and MainsMeterTimeout=3
- **When** Four consecutive 1-second ticks occur decrementing the timeout
- **Then** CT_NOCOMM remains clear during countdown (3 to 0) and is set on the tick after reaching 0

> Test: `test_mains_meter_countdown_to_error` in `test_meter_recovery.c:309`

---

## Meter Telemetry

### Initialization zeros all counters and metadata

**Requirement:** `REQ-MTR-001`

- **Given** An uninitialized meter_telemetry_t struct
- **When** meter_telemetry_init is called
- **Then** All counters, meter types, and addresses are zero

> Test: `test_init_zeros_all` in `test_meter_telemetry.c:1`

### Configure sets meter type and address for a slot

**Requirement:** `REQ-MTR-002`

- **Given** An initialized telemetry struct
- **When** meter_telemetry_configure is called with type=4 (Eastron3P) and address=10
- **Then** The slot reflects the configured type and address

> Test: `test_configure_sets_type_and_address` in `test_meter_telemetry.c:50`

### Each counter increments independently

**Requirement:** `REQ-MTR-003`

- **Given** An initialized telemetry struct with mains slot configured
- **When** request, response, crc_error, and timeout are each incremented different numbers of times
- **Then** Each counter reflects only its own increments

> Test: `test_increment_counters_independently` in `test_meter_telemetry.c:71`

### Counters on different slots are independent

**Requirement:** `REQ-MTR-004`

- **Given** An initialized telemetry struct with mains and EV slots configured
- **When** Mains slot gets 5 requests and EV slot gets 2 requests
- **Then** Each slot reflects only its own request count

> Test: `test_slots_are_independent` in `test_meter_telemetry.c:98`

### Counter saturates at UINT32_MAX instead of wrapping

**Requirement:** `REQ-MTR-005`

- **Given** A telemetry struct with request_count set to UINT32_MAX - 1
- **When** request is incremented twice
- **Then** Counter reaches UINT32_MAX and stays there

> Test: `test_counter_saturates_at_max` in `test_meter_telemetry.c:124`

### Reset slot zeros counters but preserves type and address

**Requirement:** `REQ-MTR-006`

- **Given** A mains slot with type=4, address=10, and non-zero counters
- **When** meter_telemetry_reset_slot is called
- **Then** All counters are zero but meter_type=4 and meter_address=10 are preserved

> Test: `test_reset_slot_preserves_config` in `test_meter_telemetry.c:147`

### Reset all zeros counters on every slot but preserves each slot's config

**Requirement:** `REQ-MTR-007`

- **Given** Mains and EV slots are configured with non-zero counters
- **When** meter_telemetry_reset_all is called
- **Then** All counters are zero and both slots retain their type and address

> Test: `test_reset_all_preserves_config` in `test_meter_telemetry.c:172`

### Out-of-range slot returns NULL and does not crash

**Requirement:** `REQ-MTR-008`

- **Given** An initialized telemetry struct
- **When** get, increment, configure, and reset are called with slot=METER_TELEMETRY_MAX_METERS
- **Then** get returns NULL and no crash occurs

> Test: `test_out_of_range_slot_safe` in `test_meter_telemetry.c:200`

### Error rate is zero when no requests have been sent

**Requirement:** `REQ-MTR-009`

- **Given** An initialized telemetry struct with zero request count
- **When** meter_telemetry_error_rate is called
- **Then** Returns 0

> Test: `test_error_rate_zero_requests` in `test_meter_telemetry.c:227`

### Error rate calculated correctly from CRC errors and timeouts

**Requirement:** `REQ-MTR-010`

- **Given** 100 requests with 3 CRC errors and 7 timeouts
- **When** meter_telemetry_error_rate is called
- **Then** Returns 10 (10%)

> Test: `test_error_rate_mixed_errors` in `test_meter_telemetry.c:240`

### Error rate caps at 100% when errors exceed requests

**Requirement:** `REQ-MTR-011`

- **Given** 10 requests with 15 timeout errors (more errors than requests due to counter manipulation)
- **When** meter_telemetry_error_rate is called
- **Then** Returns 100 (capped)

> Test: `test_error_rate_caps_at_100` in `test_meter_telemetry.c:257`

### Error rate for out-of-range slot returns 0

**Requirement:** `REQ-MTR-012`

- **Given** An initialized telemetry struct
- **When** meter_telemetry_error_rate is called with an invalid slot
- **Then** Returns 0

> Test: `test_error_rate_invalid_slot` in `test_meter_telemetry.c:273`

### All functions handle NULL pointer without crashing

**Requirement:** `REQ-MTR-013`

- **Given** A NULL meter_telemetry_t pointer
- **When** All API functions are called with NULL
- **Then** No crash occurs, get returns NULL, error_rate returns 0

> Test: `test_null_pointer_safety` in `test_meter_telemetry.c:288`

---

## Metering Diagnostics

### All diagnostic counters are zero after initialization

**Requirement:** `REQ-MTR-030`

- **Given** A freshly initialized EVSE context
- **When** evse_init completes
- **Then** meter_timeout_count, meter_recovery_count, and api_stale_count are all 0

> Test: `test_counters_zero_after_init` in `test_metering_diagnostics.c:1`

### meter_timeout_count increments when CT_NOCOMM is set

**Requirement:** `REQ-MTR-031`

- **Given** EVSE in Smart mode with MainsMeterType=1 and MainsMeterTimeout=0
- **When** A 1-second tick triggers CT_NOCOMM
- **Then** meter_timeout_count increments by 1

> Test: `test_timeout_count_increments_on_ct_nocomm` in `test_metering_diagnostics.c:37`

### meter_timeout_count increments for node (LoadBl > 1)

**Requirement:** `REQ-MTR-032`

- **Given** EVSE as node with LoadBl=2 and MainsMeterTimeout=0
- **When** A 1-second tick triggers CT_NOCOMM
- **Then** meter_timeout_count increments by 1

> Test: `test_timeout_count_increments_on_node_ct_nocomm` in `test_metering_diagnostics.c:59`

### meter_recovery_count increments when CT_NOCOMM clears

**Requirement:** `REQ-MTR-033`

- **Given** EVSE with CT_NOCOMM set and MainsMeterTimeout restored to >0
- **When** A 1-second tick clears CT_NOCOMM
- **Then** meter_recovery_count increments by 1

> Test: `test_recovery_count_increments` in `test_metering_diagnostics.c:80`

### api_stale_count increments when API data goes stale

**Requirement:** `REQ-MTR-034`

- **Given** EVSE in API mode with staleness timer about to expire
- **When** Timer reaches 0
- **Then** api_stale_count increments by 1

> Test: `test_api_stale_count_increments` in `test_metering_diagnostics.c:109`

### Counters accumulate across multiple events

**Requirement:** `REQ-MTR-035`

- **Given** EVSE that has already had one timeout and recovery
- **When** Another timeout and recovery cycle occurs
- **Then** Counters show 2 timeouts and 2 recoveries

> Test: `test_counters_are_cumulative` in `test_metering_diagnostics.c:135`

### meter_timeout_count does NOT increment when CT_NOCOMM is suppressed for API mode

**Requirement:** `REQ-MTR-036`

- **Given** EVSE in API mode with staleness enabled and MainsMeterTimeout=0
- **When** A 1-second tick occurs (CT_NOCOMM suppressed)
- **Then** meter_timeout_count remains 0

> Test: `test_timeout_count_not_incremented_when_suppressed` in `test_metering_diagnostics.c:172`

---

## Modbus Frame Decoding

### Frame init zeros all fields and sets Type to MODBUS_INVALID

**Requirement:** `REQ-MTR-020`

- **Given** An uninitialized modbus_frame_t
- **When** modbus_frame_init is called
- **Then** All fields are zero and Type is MODBUS_INVALID

> Test: `test_frame_init` in `test_modbus_decode.c:1`

### FC04 read input register request is parsed correctly

**Requirement:** `REQ-MTR-021`

- **Given** A 6-byte FC04 request: address=0x0A, register=0x0006, count=12
- **When** modbus_decode is called
- **Then** Type=MODBUS_REQUEST, Address=0x0A, Function=0x04, Register=0x0006, RegisterCount=12

> Test: `test_fc04_read_request` in `test_modbus_decode.c:53`

### FC03 read holding register request is parsed the same as FC04

**Requirement:** `REQ-MTR-022`

- **Given** A 6-byte FC03 request: address=0x01, register=0x5B0C, count=16
- **When** modbus_decode is called
- **Then** Type=MODBUS_REQUEST, Function=0x03, Register=0x5B0C, RegisterCount=16

> Test: `test_fc03_read_request` in `test_modbus_decode.c:74`

### FC04 response with 4 bytes of data is parsed correctly

**Requirement:** `REQ-MTR-023`

- **Given** A FC04 response: address=0x0A, bytecount=4, data=[0x00,0x64,0x00,0xC8]
- **When** modbus_decode is called with a pending request for register 0x0006
- **Then** Type=MODBUS_RESPONSE, DataLength=4, Register=0x0006, Data points to payload

> Test: `test_fc04_response` in `test_modbus_decode.c:95`

### FC06 write single register as initial request (no pending request)

**Requirement:** `REQ-MTR-024`

- **Given** A 6-byte FC06 packet: address=0x02, register=0x0100, value=0x0020
- **When** modbus_decode is called with no pending request
- **Then** Type=MODBUS_REQUEST, Register=0x0100, Value=0x0020

> Test: `test_fc06_as_request` in `test_modbus_decode.c:129`

### FC06 echo treated as response when matching pending request

**Requirement:** `REQ-MTR-025`

- **Given** A 6-byte FC06 packet with a pending request matching address and function
- **When** modbus_decode is called
- **Then** Type=MODBUS_RESPONSE (disambiguated from MODBUS_OK)

> Test: `test_fc06_as_response` in `test_modbus_decode.c:150`

### FC06 to broadcast address is always treated as request

**Requirement:** `REQ-MTR-026`

- **Given** A 6-byte FC06 packet to broadcast address 0x09 with matching pending request
- **When** modbus_decode is called
- **Then** Type=MODBUS_REQUEST (broadcast is never a response)

> Test: `test_fc06_broadcast_is_request` in `test_modbus_decode.c:174`

### FC10 response (6 bytes) is parsed correctly

**Requirement:** `REQ-MTR-027`

- **Given** A 6-byte FC10 response: address=0x01, register=0x0020, count=8
- **When** modbus_decode is called
- **Then** Type=MODBUS_RESPONSE, Register=0x0020, RegisterCount=8

> Test: `test_fc10_response` in `test_modbus_decode.c:195`

### FC10 request with data payload is parsed correctly

**Requirement:** `REQ-MTR-028`

- **Given** An FC10 request: address=0x09, register=0x0020, count=2, bytecount=4, data=[0x00,0x3C,0x00,0x50]
- **When** modbus_decode is called
- **Then** Type=MODBUS_REQUEST, DataLength=4, Data points to the 4-byte payload

> Test: `test_fc10_request_with_data` in `test_modbus_decode.c:213`

### 3-byte exception frame is parsed correctly

**Requirement:** `REQ-MTR-029`

- **Given** A 3-byte exception: address=0x0A, function=0x84, exception=0x02
- **When** modbus_decode is called
- **Then** Type=MODBUS_EXCEPTION, Exception=0x02

> Test: `test_exception_frame` in `test_modbus_decode.c:241`

### Buffer too short (< 3 bytes) results in MODBUS_INVALID

**Requirement:** `REQ-MTR-030`

- **Given** A 2-byte buffer
- **When** modbus_decode is called
- **Then** Type=MODBUS_INVALID

> Test: `test_too_short_buffer` in `test_modbus_decode.c:262`

### 4-byte buffer (between exception and minimum data) results in MODBUS_INVALID

**Requirement:** `REQ-MTR-031`

- **Given** A 4-byte buffer that is neither an exception nor a valid data packet
- **When** modbus_decode is called
- **Then** Type=MODBUS_INVALID

> Test: `test_four_byte_buffer_invalid` in `test_modbus_decode.c:278`

### FC04 response with mismatched byte count results in MODBUS_INVALID

**Requirement:** `REQ-MTR-032`

- **Given** A FC04 response where bytecount (10) does not match actual data length
- **When** modbus_decode is called
- **Then** Type=MODBUS_INVALID because DataLength != len - 3

> Test: `test_fc04_response_length_mismatch` in `test_modbus_decode.c:294`

### NULL pointer arguments do not crash

**Requirement:** `REQ-MTR-033`

- **Given** NULL frame or buffer pointer
- **When** modbus_decode is called
- **Then** No crash occurs

> Test: `test_null_safety` in `test_modbus_decode.c:311`

### Unknown function code results in MODBUS_INVALID

**Requirement:** `REQ-MTR-034`

- **Given** A 6-byte frame with function code 0x01 (read coils, not supported)
- **When** modbus_decode is called
- **Then** Type=MODBUS_INVALID

> Test: `test_unknown_function_code` in `test_modbus_decode.c:330`

---

## Modbus Frame Logging

### Init zeros all ring buffer fields

**Requirement:** `REQ-MTR-090`

- **Given** An uninitialized modbus_log_t
- **When** modbus_log_init is called
- **Then** count=0, head=0, total_logged=0

> Test: `test_log_init` in `test_modbus_log.c:1`

### Append and retrieve a single entry

**Requirement:** `REQ-MTR-091`

- **Given** An initialized ring buffer
- **When** One entry is appended with timestamp=1000, addr=0x0A, func=0x04, reg=0x0006
- **Then** count=1, get(0) returns the entry with correct fields

> Test: `test_log_single_entry` in `test_modbus_log.c:42`

### Multiple entries maintain chronological order

**Requirement:** `REQ-MTR-092`

- **Given** An initialized ring buffer
- **When** 3 entries are appended with timestamps 100, 200, 300
- **Then** get(0) returns t=100, get(1) returns t=200, get(2) returns t=300

> Test: `test_log_order` in `test_modbus_log.c:68`

### Ring buffer wraps around and overwrites oldest entries

**Requirement:** `REQ-MTR-093`

- **Given** An initialized ring buffer
- **When** MODBUS_LOG_SIZE + 5 entries are appended
- **Then** count equals MODBUS_LOG_SIZE, oldest entries are overwritten

> Test: `test_log_wraparound` in `test_modbus_log.c:90`

### Ring buffer at exactly MODBUS_LOG_SIZE entries

**Requirement:** `REQ-MTR-094`

- **Given** An initialized ring buffer
- **When** Exactly MODBUS_LOG_SIZE entries are appended
- **Then** count equals MODBUS_LOG_SIZE, all entries accessible in order

> Test: `test_log_exact_fill` in `test_modbus_log.c:123`

### Clear resets count but preserves total_logged

**Requirement:** `REQ-MTR-095`

- **Given** A ring buffer with 10 entries
- **When** modbus_log_clear is called
- **Then** count=0, total_logged=10, get(0) returns NULL

> Test: `test_log_clear` in `test_modbus_log.c:150`

### Clear then refill works correctly

**Requirement:** `REQ-MTR-096`

- **Given** A ring buffer that was filled, cleared, then has 3 new entries
- **When** Entries are read
- **Then** count=3, entries reflect only the new data, total includes both batches

> Test: `test_log_clear_then_refill` in `test_modbus_log.c:173`

### Out-of-range index returns NULL

**Requirement:** `REQ-MTR-097`

- **Given** A ring buffer with 3 entries
- **When** get is called with index=3 and index=MODBUS_LOG_SIZE
- **Then** Both return NULL

> Test: `test_log_out_of_range` in `test_modbus_log.c:201`

### All functions handle NULL pointer without crashing

**Requirement:** `REQ-MTR-098`

- **Given** NULL modbus_log_t pointer
- **When** All API functions are called with NULL
- **Then** No crash, count returns 0, get returns NULL, total returns 0

> Test: `test_log_null_safety` in `test_modbus_log.c:221`

### TX, RX, and ERR direction values are stored correctly

**Requirement:** `REQ-MTR-099`

- **Given** An initialized ring buffer
- **When** Three entries with different directions are appended
- **Then** Each entry reflects its correct direction

> Test: `test_log_directions` in `test_modbus_log.c:241`

---

## Modem / ISO15118 Negotiation

### Entering MODEM_REQUEST disconnects the pilot signal

**Requirement:** `REQ-MODEM-001`

- **Given** The EVSE is initialised with basic configuration
- **When** The state is set to STATE_MODEM_REQUEST
- **Then** pilot_connected is false

> Test: `test_modem_request_disconnects_pilot` in `test_modem_states.c:1`

### Entering MODEM_REQUEST sets PWM to off (+12V)

**Requirement:** `REQ-MODEM-002`

- **Given** The EVSE is initialised with basic configuration
- **When** The state is set to STATE_MODEM_REQUEST
- **Then** PWM duty is set to 1024 (off / +12V constant)

> Test: `test_modem_request_pwm_off` in `test_modem_states.c:38`

### Entering MODEM_REQUEST ensures contactors are open

**Requirement:** `REQ-MODEM-003`

- **Given** The EVSE is initialised with basic configuration
- **When** The state is set to STATE_MODEM_REQUEST
- **Then** Both contactor1 and contactor2 are off (open)

> Test: `test_modem_request_contactors_off` in `test_modem_states.c:52`

### MODEM_REQUEST transitions to MODEM_WAIT after timer expires

**Requirement:** `REQ-MODEM-004`

- **Given** The EVSE is in STATE_MODEM_REQUEST with ToModemWaitStateTimer=0
- **When** One second tick occurs
- **Then** The state transitions to STATE_MODEM_WAIT

> Test: `test_modem_request_to_wait_on_timer` in `test_modem_states.c:67`

### Modem states are NOT handled in tick_10ms (original behavior)

**Requirement:** `REQ-MODEM-005`

- **Given** The EVSE is in STATE_MODEM_REQUEST
- **When** A 12V pilot signal is received during tick_10ms
- **Then** The state stays MODEM_REQUEST (modem is managed only by tick_1s timers)

> Test: `test_modem_request_to_A_on_disconnect` in `test_modem_states.c:83`

### Entering MODEM_WAIT sets 5% PWM duty cycle for ISO15118 signalling

**Requirement:** `REQ-MODEM-006`

- **Given** The EVSE is initialised with basic configuration
- **When** The state is set to STATE_MODEM_WAIT
- **Then** PWM duty is set to 51 (5% duty cycle)

> Test: `test_modem_wait_5pct_duty` in `test_modem_states.c:100`

### Entering MODEM_WAIT reconnects the pilot signal

**Requirement:** `REQ-MODEM-007`

- **Given** The EVSE is initialised with basic configuration
- **When** The state is set to STATE_MODEM_WAIT
- **Then** pilot_connected is true

> Test: `test_modem_wait_pilot_connected` in `test_modem_states.c:114`

### Entering MODEM_WAIT sets ToModemDoneStateTimer to 60 seconds

**Requirement:** `REQ-MODEM-008`

- **Given** The EVSE is initialised with basic configuration
- **When** The state is set to STATE_MODEM_WAIT
- **Then** ToModemDoneStateTimer is set to 60

> Test: `test_modem_wait_timer_set` in `test_modem_states.c:128`

### MODEM_WAIT transitions to MODEM_DONE after 60-second timeout

**Requirement:** `REQ-MODEM-009`

- **Given** The EVSE is in STATE_MODEM_WAIT with 60-second timer
- **When** 61 second ticks occur (60 to decrement to 0, 1 more to fire transition)
- **Then** The state transitions to STATE_MODEM_DONE

> Test: `test_modem_wait_to_done_after_timeout` in `test_modem_states.c:142`

### MODEM_WAIT is NOT handled in tick_10ms (original behavior)

**Requirement:** `REQ-MODEM-010`

- **Given** The EVSE is in STATE_MODEM_WAIT
- **When** A 12V pilot signal is received during tick_10ms
- **Then** The state stays MODEM_WAIT (modem is managed only by tick_1s timers)

> Test: `test_modem_wait_to_A_on_disconnect` in `test_modem_states.c:160`

### Entering MODEM_DONE disconnects the pilot signal

**Requirement:** `REQ-MODEM-011`

- **Given** The EVSE is initialised with basic configuration
- **When** The state is set to STATE_MODEM_DONE
- **Then** pilot_connected is false

> Test: `test_modem_done_disconnects_pilot` in `test_modem_states.c:177`

### Entering MODEM_DONE sets LeaveModemDoneStateTimer to 5 seconds

**Requirement:** `REQ-MODEM-012`

- **Given** The EVSE is initialised with basic configuration
- **When** The state is set to STATE_MODEM_DONE
- **Then** LeaveModemDoneStateTimer is set to 5

> Test: `test_modem_done_timer_set` in `test_modem_states.c:191`

### MODEM_DONE transitions to STATE_B after 5-second timer with ModemStage=1

**Requirement:** `REQ-MODEM-013`

- **Given** The EVSE is in STATE_MODEM_DONE with 5-second timer
- **When** 6 second ticks occur (5 to decrement to 0, 1 more to fire transition)
- **Then** The state transitions to STATE_B and ModemStage is set to 1

> Test: `test_modem_done_to_B_after_timer` in `test_modem_states.c:205`

### MODEM_DONE is NOT handled in tick_10ms (original behavior)

**Requirement:** `REQ-MODEM-014`

- **Given** The EVSE is in STATE_MODEM_DONE
- **When** A 12V pilot signal is received during tick_10ms
- **Then** The state stays MODEM_DONE (modem is managed only by tick_1s timers)

> Test: `test_modem_done_to_A_on_disconnect` in `test_modem_states.c:223`

### MODEM_DENIED transitions to STATE_A after timer expires

**Requirement:** `REQ-MODEM-015`

- **Given** The EVSE is in STATE_MODEM_DENIED with LeaveModemDeniedStateTimer=3
- **When** 4 second ticks occur (3 to decrement to 0, 1 more to fire transition)
- **Then** The state transitions to STATE_A

> Test: `test_modem_denied_to_A_after_timer` in `test_modem_states.c:240`

### MODEM_DENIED is NOT handled in tick_10ms (original behavior)

**Requirement:** `REQ-MODEM-016`

- **Given** The EVSE is in STATE_MODEM_DENIED with timer still running
- **When** A 12V pilot signal is received during tick_10ms
- **Then** The state stays MODEM_DENIED (modem is managed only by tick_1s timers)

> Test: `test_modem_denied_to_A_on_disconnect` in `test_modem_states.c:258`

### MODEM_WAIT timer=1 does not transition immediately on decrement

**Requirement:** `REQ-MODEM-M1A`

- **Given** EVSE is in STATE_MODEM_WAIT with ToModemDoneStateTimer=1
- **When** One second tick occurs (timer decrements to 0)
- **When** Another second tick occurs (timer is now 0, else branch fires)
- **Then** The EVSE stays in STATE_MODEM_WAIT (does not yet transition to MODEM_DONE)
- **Then** The EVSE transitions to STATE_MODEM_DONE

> Test: `test_modem_wait_timer_1_no_immediate_transition` in `test_modem_states.c:276`

### MODEM_DONE timer=1 does not transition immediately on decrement

**Requirement:** `REQ-MODEM-M1B`

- **Given** EVSE is in STATE_MODEM_DONE with LeaveModemDoneStateTimer=1
- **When** One second tick occurs (timer decrements to 0)
- **When** Another second tick occurs
- **Then** The EVSE stays in STATE_MODEM_DONE
- **Then** The EVSE transitions to STATE_B (EVCCID accepted)

> Test: `test_modem_done_timer_1_no_immediate_transition` in `test_modem_states.c:299`

### MODEM_DENIED timer=1 does not transition immediately on decrement

**Requirement:** `REQ-MODEM-M1C`

- **Given** EVSE is in STATE_MODEM_DENIED with LeaveModemDeniedStateTimer=1
- **When** One second tick occurs (timer decrements to 0)
- **When** Another second tick occurs
- **Then** The EVSE stays in STATE_MODEM_DENIED
- **Then** The EVSE transitions to STATE_A

> Test: `test_modem_denied_timer_1_no_immediate_transition` in `test_modem_states.c:320`

### DisconnectTimeCounter starts on STATE_A entry when modem enabled

**Requirement:** `REQ-MODEM-M2A`

- **Given** ModemEnabled=true, DisconnectTimeCounter=-1 (disabled)
- **When** State is set to STATE_A
- **Then** DisconnectTimeCounter is set to 0 (started)

> Test: `test_disconnect_counter_starts_on_state_a` in `test_modem_states.c:343`

### DisconnectTimeCounter disabled on MODEM_REQUEST entry

**Requirement:** `REQ-MODEM-M2B`

- **Given** DisconnectTimeCounter=5 (running)
- **When** State is set to STATE_MODEM_REQUEST
- **Then** DisconnectTimeCounter is set to -1 (disabled)

> Test: `test_disconnect_counter_disabled_on_modem_request` in `test_modem_states.c:358`

### DisconnectTimeCounter disabled on MODEM_DONE entry

**Requirement:** `REQ-MODEM-M2C`

- **Given** DisconnectTimeCounter=5 (running)
- **When** State is set to STATE_MODEM_DONE
- **Then** DisconnectTimeCounter is set to -1 (disabled)

> Test: `test_disconnect_counter_disabled_on_modem_done` in `test_modem_states.c:373`

### DisconnectTimeCounter is NOT incremented in tick_1s (handled by firmware wrapper)

**Requirement:** `REQ-MODEM-M2D`

- **Given** ModemEnabled=true, DisconnectTimeCounter=0
- **When** tick_1s occurs
- **Then** DisconnectTimeCounter stays 0 (firmware wrapper handles increment + pilot check)

> Test: `test_disconnect_counter_increments_in_tick_1s` in `test_modem_states.c:388`

### DisconnectTimeCounter does not increment when disabled (-1)

**Requirement:** `REQ-MODEM-M2E`

- **Given** ModemEnabled=true, DisconnectTimeCounter=-1
- **When** tick_1s occurs
- **Then** DisconnectTimeCounter remains -1

> Test: `test_disconnect_counter_stays_disabled` in `test_modem_states.c:403`

### Empty RequiredEVCCID allows any vehicle

**Requirement:** `REQ-MODEM-EVCCID-001`

- **Given** MODEM_DONE, LeaveModemDoneStateTimer expired, RequiredEVCCID=""
- **When** tick_1s processes MODEM_DONE timer expiry
- **Then** Transitions to STATE_B with ModemStage=1

> Test: `test_evccid_empty_required_allows_any` in `test_modem_states.c:420`

### Matching EVCCID passes validation

**Requirement:** `REQ-MODEM-EVCCID-002`

- **Given** MODEM_DONE, timer expired, RequiredEVCCID matches EVCCID
- **When** tick_1s processes timer expiry
- **Then** Transitions to STATE_B with ModemStage=1

> Test: `test_evccid_matching_passes` in `test_modem_states.c:441`

### Mismatched EVCCID triggers MODEM_DENIED

**Requirement:** `REQ-MODEM-EVCCID-003`

- **Given** MODEM_DONE, timer expired, RequiredEVCCID != EVCCID
- **When** tick_1s processes timer expiry
- **Then** Transitions to MODEM_DENIED, ModemStage=0, LeaveModemDeniedStateTimer=60

> Test: `test_evccid_mismatch_denied` in `test_modem_states.c:462`

### Full flow: EVCCID mismatch → DENIED → timeout → STATE_A

**Requirement:** `REQ-MODEM-EVCCID-004`

- **Given** Modem flow reaches MODEM_DONE with wrong EVCCID
- **When** Timer expires and EVCCID doesn't match, then DENIED timer expires
- **Then** MODEM_DENIED → STATE_A after 60+1 seconds

> Test: `test_evccid_mismatch_full_flow_to_a` in `test_modem_states.c:485`

### Full modem negotiation flow: REQUEST -> WAIT -> DONE -> STATE_B

**Requirement:** `REQ-MODEM-017`

- **Given** The EVSE is initialised with basic configuration
- **When** The modem negotiation proceeds through all stages with timers expiring
- **Then** The EVSE transitions REQUEST->WAIT->DONE->B with ModemStage=1 and correct PWM/pilot at each stage

> Test: `test_full_modem_flow` in `test_modem_states.c:515`

---

## MQTT Command Parsing

### Set mode to Normal via MQTT

**Requirement:** `REQ-MQTT-001`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/Mode with payload "Normal"
- **Then** Command type is MQTT_CMD_MODE with mode MQTT_MODE_NORMAL

> Test: `test_mode_normal` in `test_mqtt_parser.c:1`

### Set mode to Solar via MQTT

**Requirement:** `REQ-MQTT-001`


> Test: `test_mode_solar` in `test_mqtt_parser.c:32`

### Set mode to Smart via MQTT

**Requirement:** `REQ-MQTT-001`


> Test: `test_mode_smart` in `test_mqtt_parser.c:43`

### Set mode to Off via MQTT

**Requirement:** `REQ-MQTT-001`


> Test: `test_mode_off` in `test_mqtt_parser.c:54`

### Set mode to Pause via MQTT

**Requirement:** `REQ-MQTT-001`


> Test: `test_mode_pause` in `test_mqtt_parser.c:65`

### CustomButton set to On

**Requirement:** `REQ-MQTT-003`


> Test: `test_custom_button_on` in `test_mqtt_parser.c:90`

### CustomButton set to Off

**Requirement:** `REQ-MQTT-003`


> Test: `test_custom_button_off` in `test_mqtt_parser.c:101`

### CP PWM override normal mode (-1)

**Requirement:** `REQ-MQTT-006`


> Test: `test_cp_pwm_normal` in `test_mqtt_parser.c:192`

### CP PWM override disconnect (0)

**Requirement:** `REQ-MQTT-006`


> Test: `test_cp_pwm_disconnect` in `test_mqtt_parser.c:203`

### CP PWM override max value (1024)

**Requirement:** `REQ-MQTT-006`


> Test: `test_cp_pwm_max` in `test_mqtt_parser.c:213`

### Home battery current set

**Requirement:** `REQ-MQTT-009`


> Test: `test_home_battery_current` in `test_mqtt_parser.c:370`

### Home battery current negative (discharging)

**Requirement:** `REQ-MQTT-009`


> Test: `test_home_battery_current_negative` in `test_mqtt_parser.c:381`

### Cable lock enabled

**Requirement:** `REQ-MQTT-011`


> Test: `test_cable_lock_enable` in `test_mqtt_parser.c:473`

### Cable lock disabled

**Requirement:** `REQ-MQTT-011`


> Test: `test_cable_lock_disable` in `test_mqtt_parser.c:484`

### Cable lock any non-"1" disables

**Requirement:** `REQ-MQTT-011`


> Test: `test_cable_lock_any_other` in `test_mqtt_parser.c:494`

### EnableC2 numeric value

**Requirement:** `REQ-MQTT-012`


> Test: `test_enable_c2_numeric` in `test_mqtt_parser.c:506`

### EnableC2 string value

**Requirement:** `REQ-MQTT-012`


> Test: `test_enable_c2_string` in `test_mqtt_parser.c:517`

### RequiredEVCCID set

**Requirement:** `REQ-MQTT-013`


> Test: `test_required_evccid` in `test_mqtt_parser.c:547`

### PrioStrategy set to MODBUS_ADDR (0) via MQTT

**Requirement:** `REQ-MQTT-015`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/PrioStrategy with payload "0"
- **Then** Command type is MQTT_CMD_PRIO_STRATEGY with value 0

> Test: `test_prio_strategy_modbus_addr` in `test_mqtt_parser.c:570`

### PrioStrategy set to FIRST_CONNECTED (1)

**Requirement:** `REQ-MQTT-015`


> Test: `test_prio_strategy_first_connected` in `test_mqtt_parser.c:584`

### PrioStrategy set to LAST_CONNECTED (2)

**Requirement:** `REQ-MQTT-015`


> Test: `test_prio_strategy_last_connected` in `test_mqtt_parser.c:594`

### RotationInterval set to 0 (disabled) via MQTT

**Requirement:** `REQ-MQTT-016`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/RotationInterval with payload "0"
- **Then** Command type is MQTT_CMD_ROTATION_INTERVAL with value 0

> Test: `test_rotation_interval_zero` in `test_mqtt_parser.c:627`

### RotationInterval set to minimum (30 minutes)

**Requirement:** `REQ-MQTT-016`


> Test: `test_rotation_interval_min` in `test_mqtt_parser.c:641`

### RotationInterval set to maximum (1440 minutes = 24h)

**Requirement:** `REQ-MQTT-016`


> Test: `test_rotation_interval_max` in `test_mqtt_parser.c:651`

### IdleTimeout set to minimum (30 seconds) via MQTT

**Requirement:** `REQ-MQTT-017`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/IdleTimeout with payload "30"
- **Then** Command type is MQTT_CMD_IDLE_TIMEOUT with value 30

> Test: `test_idle_timeout_min` in `test_mqtt_parser.c:684`

### IdleTimeout set to default (60 seconds)

**Requirement:** `REQ-MQTT-017`


> Test: `test_idle_timeout_default` in `test_mqtt_parser.c:698`

### IdleTimeout set to maximum (300 seconds)

**Requirement:** `REQ-MQTT-017`


> Test: `test_idle_timeout_max` in `test_mqtt_parser.c:708`

### MQTTHeartbeat set to valid value via MQTT

**Requirement:** `REQ-MQTT-023`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/MQTTHeartbeat with payload "60"
- **Then** Command type is MQTT_CMD_MQTT_HEARTBEAT with mqtt_heartbeat = 60

> Test: `test_mqtt_heartbeat_valid` in `test_mqtt_parser.c:824`

### MQTTChangeOnly enabled via MQTT with payload "1"

**Requirement:** `REQ-MQTT-024`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/MQTTChangeOnly with payload "1"
- **Then** Command type is MQTT_CMD_MQTT_CHANGE_ONLY with mqtt_change_only = true

> Test: `test_mqtt_change_only_enable` in `test_mqtt_parser.c:864`

### MQTTChangeOnly disabled via MQTT with payload "0"

**Requirement:** `REQ-MQTT-024`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/MQTTChangeOnly with payload "0"
- **Then** Command type is MQTT_CMD_MQTT_CHANGE_ONLY with mqtt_change_only = false

> Test: `test_mqtt_change_only_disable` in `test_mqtt_parser.c:878`

---

## MQTT Input Validation

### Invalid mode string is rejected

**Requirement:** `REQ-MQTT-002`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/Mode with payload "Invalid"
- **Then** The parser returns false

> Test: `test_mode_invalid` in `test_mqtt_parser.c:76`

### Current override with valid value

**Requirement:** `REQ-MQTT-004`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/CurrentOverride with payload "100"
- **Then** Command has current_override = 100

> Test: `test_current_override_valid` in `test_mqtt_parser.c:114`

### Current override zero resets override

**Requirement:** `REQ-MQTT-004`


> Test: `test_current_override_zero` in `test_mqtt_parser.c:128`

### Current override with max value

**Requirement:** `REQ-MQTT-004`


> Test: `test_current_override_max` in `test_mqtt_parser.c:139`

### Max sum mains valid value

**Requirement:** `REQ-MQTT-005`


> Test: `test_max_sum_mains_valid` in `test_mqtt_parser.c:151`

### Max sum mains zero disables

**Requirement:** `REQ-MQTT-005`


> Test: `test_max_sum_mains_zero` in `test_mqtt_parser.c:162`

### Max sum mains below minimum rejected

**Requirement:** `REQ-MQTT-005`


> Test: `test_max_sum_mains_below_min` in `test_mqtt_parser.c:172`

### Max sum mains above maximum rejected

**Requirement:** `REQ-MQTT-005`


> Test: `test_max_sum_mains_above_max` in `test_mqtt_parser.c:181`

### CP PWM override out of range rejected

**Requirement:** `REQ-MQTT-006`


> Test: `test_cp_pwm_out_of_range` in `test_mqtt_parser.c:223`

### CP PWM override below -1 rejected

**Requirement:** `REQ-MQTT-006`


> Test: `test_cp_pwm_below_neg1` in `test_mqtt_parser.c:232`

### Mains meter out of range rejected (>=2000)

**Requirement:** `REQ-MQTT-007`


> Test: `test_mains_meter_out_of_range` in `test_mqtt_parser.c:272`

### Mains meter out of range rejected (<=-2000)

**Requirement:** `REQ-MQTT-007`


> Test: `test_mains_meter_out_of_range_neg` in `test_mqtt_parser.c:282`

### Mains meter missing fields rejected

**Requirement:** `REQ-MQTT-007`


> Test: `test_mains_meter_missing_fields` in `test_mqtt_parser.c:292`

### EV meter partial data rejected

**Requirement:** `REQ-MQTT-008`


> Test: `test_ev_meter_partial` in `test_mqtt_parser.c:345`

### RGB color out of range rejected

**Requirement:** `REQ-MQTT-010`


> Test: `test_rgb_out_of_range` in `test_mqtt_parser.c:406`

### RGB color negative rejected

**Requirement:** `REQ-MQTT-010`


> Test: `test_rgb_negative` in `test_mqtt_parser.c:416`

### RGB color missing component rejected

**Requirement:** `REQ-MQTT-010`


> Test: `test_rgb_missing` in `test_mqtt_parser.c:426`

### EnableC2 out of range rejected

**Requirement:** `REQ-MQTT-012`


> Test: `test_enable_c2_out_of_range` in `test_mqtt_parser.c:527`

### EnableC2 invalid string rejected

**Requirement:** `REQ-MQTT-012`


> Test: `test_enable_c2_invalid_string` in `test_mqtt_parser.c:536`

### RequiredEVCCID too long rejected

**Requirement:** `REQ-MQTT-013`


> Test: `test_required_evccid_too_long` in `test_mqtt_parser.c:558`

### PrioStrategy value 3 is rejected (out of range)

**Requirement:** `REQ-MQTT-015`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/PrioStrategy with payload "3"
- **Then** The parser returns false

> Test: `test_prio_strategy_out_of_range` in `test_mqtt_parser.c:604`

### PrioStrategy negative value is rejected

**Requirement:** `REQ-MQTT-015`


> Test: `test_prio_strategy_negative` in `test_mqtt_parser.c:616`

### RotationInterval in gap (1-29) is rejected

**Requirement:** `REQ-MQTT-016`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/RotationInterval with payload "15"
- **Then** The parser returns false

> Test: `test_rotation_interval_gap` in `test_mqtt_parser.c:661`

### RotationInterval above maximum is rejected

**Requirement:** `REQ-MQTT-016`


> Test: `test_rotation_interval_too_high` in `test_mqtt_parser.c:673`

### IdleTimeout below minimum (29) is rejected

**Requirement:** `REQ-MQTT-017`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/IdleTimeout with payload "29"
- **Then** The parser returns false

> Test: `test_idle_timeout_too_low` in `test_mqtt_parser.c:718`

### IdleTimeout above maximum (301) is rejected

**Requirement:** `REQ-MQTT-017`


> Test: `test_idle_timeout_too_high` in `test_mqtt_parser.c:730`

### IdleTimeout zero is rejected (minimum is 30)

**Requirement:** `REQ-MQTT-017`


> Test: `test_idle_timeout_zero` in `test_mqtt_parser.c:739`

### Max sum mains at lower boundary (10) is accepted

**Requirement:** `REQ-MQTT-005`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/CurrentMaxSumMains with payload "10"
- **Then** Command is accepted with max_sum_mains = 10

> Test: `test_max_sum_mains_boundary_10` in `test_mqtt_parser.c:750`

### Max sum mains at upper boundary (600) is accepted

**Requirement:** `REQ-MQTT-005`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/CurrentMaxSumMains with payload "600"
- **Then** Command is accepted with max_sum_mains = 600

> Test: `test_max_sum_mains_boundary_600` in `test_mqtt_parser.c:764`

### Negative current override is accepted (atoi converts, no range check)

**Requirement:** `REQ-MQTT-004`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/CurrentOverride with payload "-10"
- **Then** Command is accepted (parser does not reject; dispatch layer validates)

> Test: `test_current_override_negative` in `test_mqtt_parser.c:778`

### Empty payload is rejected for Mode command

**Requirement:** `REQ-MQTT-002`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/Mode with empty payload ""
- **Then** The parser returns false

> Test: `test_empty_payload_mode_rejected` in `test_mqtt_parser.c:810`

### MQTTHeartbeat below minimum (9) is rejected

**Requirement:** `REQ-MQTT-023`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/MQTTHeartbeat with payload "9"
- **Then** The parser returns false

> Test: `test_mqtt_heartbeat_too_low` in `test_mqtt_parser.c:838`

### MQTTHeartbeat above maximum (301) is rejected

**Requirement:** `REQ-MQTT-023`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/MQTTHeartbeat with payload "301"
- **Then** The parser returns false

> Test: `test_mqtt_heartbeat_too_high` in `test_mqtt_parser.c:850`

### MQTTChangeOnly rejects invalid payload

**Requirement:** `REQ-MQTT-024`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/MQTTChangeOnly with payload "2"
- **Then** The parser returns false

> Test: `test_mqtt_change_only_invalid` in `test_mqtt_parser.c:892`

### Unrecognized topic returns false

**Requirement:** `REQ-MQTT-014`


> Test: `test_unrecognized_topic` in `test_mqtt_parser.c:1018`

### Wrong prefix returns false

**Requirement:** `REQ-MQTT-014`


> Test: `test_wrong_prefix` in `test_mqtt_parser.c:1027`

---

## MQTT Meter Parsing

### Mains meter format L1:L2:L3 is parsed correctly

**Requirement:** `REQ-MQTT-007`

- **Given** Valid three-phase mains meter data
- **When** Payload is "100:200:300"
- **Then** L1=100, L2=200, L3=300

> Test: `test_mains_meter_valid` in `test_mqtt_parser.c:243`

### Mains meter with negative values

**Requirement:** `REQ-MQTT-007`


> Test: `test_mains_meter_negative` in `test_mqtt_parser.c:259`

### Mains meter via full command parse

**Requirement:** `REQ-MQTT-007`


> Test: `test_mains_meter_command` in `test_mqtt_parser.c:302`

### EV meter format L1:L2:L3:W:WH is parsed correctly

**Requirement:** `REQ-MQTT-008`


> Test: `test_ev_meter_valid` in `test_mqtt_parser.c:317`

### EV meter with unknown values (-1)

**Requirement:** `REQ-MQTT-008`


> Test: `test_ev_meter_unknown_values` in `test_mqtt_parser.c:332`

### EV meter via full command parse

**Requirement:** `REQ-MQTT-008`


> Test: `test_ev_meter_command` in `test_mqtt_parser.c:355`

### Mains meter with extra trailing fields after L1:L2:L3 is accepted

**Requirement:** `REQ-MQTT-007`

- **Given** A valid MQTT prefix
- **When** Payload is "100:200:300:extra"
- **Then** L1=100, L2=200, L3=300 (extra data ignored by sscanf)

> Test: `test_mains_meter_extra_fields_ignored` in `test_mqtt_parser.c:794`

---

## MQTT Color Parsing

### Valid RGB color parsed

**Requirement:** `REQ-MQTT-010`


> Test: `test_rgb_valid` in `test_mqtt_parser.c:393`

### ColorOff topic parsed correctly

**Requirement:** `REQ-MQTT-010`


> Test: `test_color_off_command` in `test_mqtt_parser.c:436`

### ColorSolar topic parsed correctly

**Requirement:** `REQ-MQTT-010`


> Test: `test_color_solar_command` in `test_mqtt_parser.c:450`

### ColorCustom topic parsed correctly

**Requirement:** `REQ-MQTT-010`


> Test: `test_color_custom_command` in `test_mqtt_parser.c:461`

---

## Solar Debug Telemetry

### SolarDebug enable via MQTT

**Requirement:** `REQ-SOL-020`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/SolarDebug with payload "1"
- **Then** The parser returns true with solar_debug = true

> Test: `test_solar_debug_enable` in `test_mqtt_parser.c:906`

### SolarDebug disable via MQTT

**Requirement:** `REQ-SOL-020`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/SolarDebug with payload "0"
- **Then** The parser returns true with solar_debug = false

> Test: `test_solar_debug_disable` in `test_mqtt_parser.c:920`

### SolarDebug rejects invalid payload

**Requirement:** `REQ-SOL-020`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/SolarDebug with payload "2"
- **Then** The parser returns false

> Test: `test_solar_debug_invalid` in `test_mqtt_parser.c:934`

### Format solar debug snapshot as JSON with all fields

**Requirement:** `REQ-SOL-020`

- **Given** A solar debug snapshot with known values
- **When** solar_debug_to_json is called with a sufficiently large buffer
- **Then** All 14 fields appear in the JSON output with correct values

> Test: `test_solar_debug_to_json_all_fields` in `test_solar_debug_json.c:1`

### JSON output starts with { and ends with }

**Requirement:** `REQ-SOL-020`

- **Given** A solar debug snapshot
- **When** solar_debug_to_json is called
- **Then** The output is valid JSON object framing

> Test: `test_solar_debug_to_json_valid_framing` in `test_solar_debug_json.c:56`

### Buffer too small for JSON output

**Requirement:** `REQ-SOL-020`

- **Given** A solar debug snapshot
- **When** solar_debug_to_json is called with a buffer that is too small
- **Then** The function returns -1

> Test: `test_solar_debug_to_json_buffer_too_small` in `test_solar_debug_json.c:75`

### Null pointer arguments

**Requirement:** `REQ-SOL-020`

- **Given** NULL snap or buf pointer
- **When** solar_debug_to_json is called
- **Then** The function returns -1

> Test: `test_solar_debug_to_json_null_args` in `test_solar_debug_json.c:92`

### Zero-initialized snapshot produces valid JSON

**Requirement:** `REQ-SOL-020`

- **Given** A zero-initialized solar debug snapshot
- **When** solar_debug_to_json is called
- **Then** All fields are zero in the output

> Test: `test_solar_debug_to_json_zeroed` in `test_solar_debug_json.c:110`

### Negative values are correctly represented

**Requirement:** `REQ-SOL-020`

- **Given** A snapshot with negative Isum and IsetBalanced
- **When** solar_debug_to_json is called
- **Then** Negative values appear with minus sign

> Test: `test_solar_debug_to_json_negative_values` in `test_solar_debug_json.c:129`

### Return value matches actual string length

**Requirement:** `REQ-SOL-020`

- **Given** A solar debug snapshot
- **When** solar_debug_to_json is called
- **Then** The return value equals strlen of the output

> Test: `test_solar_debug_to_json_return_value_matches_strlen` in `test_solar_debug_json.c:151`

---

## MQTT Change-Only Publishing

### First integer publish always goes through

**Requirement:** `REQ-MQTT-020`

- **Given** An empty cache with heartbeat 60s
- **When** An integer value is checked for slot MQTT_SLOT_ESP_TEMP
- **Then** mqtt_should_publish_int returns true (first time)

> Test: `test_int_first_publish` in `test_mqtt_publish.c:1`

### Unchanged integer is suppressed

**Requirement:** `REQ-MQTT-020`

- **Given** A cache with MQTT_SLOT_ESP_TEMP previously published as 42
- **When** The same value 42 is checked at now_s=110 (before heartbeat)
- **Then** mqtt_should_publish_int returns false

> Test: `test_int_unchanged_suppressed` in `test_mqtt_publish.c:29`

### Changed integer value triggers publish

**Requirement:** `REQ-MQTT-020`

- **Given** A cache with MQTT_SLOT_ESP_TEMP previously published as 42
- **When** A different value 43 is checked
- **Then** mqtt_should_publish_int returns true

> Test: `test_int_changed_publishes` in `test_mqtt_publish.c:43`

### Heartbeat forces re-publish of unchanged integer

**Requirement:** `REQ-MQTT-021`

- **Given** A cache with MQTT_SLOT_ESP_TEMP published at t=100 with heartbeat 60s
- **When** The same value is checked at t=160 (heartbeat elapsed)
- **Then** mqtt_should_publish_int returns true

> Test: `test_int_heartbeat_republish` in `test_mqtt_publish.c:57`

### First string publish always goes through

**Requirement:** `REQ-MQTT-020`

- **Given** An empty cache with heartbeat 60s
- **When** A string value "Normal" is checked for slot MQTT_SLOT_MODE
- **Then** mqtt_should_publish_str returns true

> Test: `test_str_first_publish` in `test_mqtt_publish.c:74`

### Changed string value triggers publish

**Requirement:** `REQ-MQTT-020`

- **Given** A cache with MQTT_SLOT_MODE previously published as "Normal"
- **When** A different string "Solar" is checked
- **Then** mqtt_should_publish_str returns true

> Test: `test_str_changed_publishes` in `test_mqtt_publish.c:87`

### mqtt_cache_force_all marks all entries stale

**Requirement:** `REQ-MQTT-022`

- **Given** A cache with MQTT_SLOT_ESP_TEMP published (unchanged value)
- **When** mqtt_cache_force_all is called then the same value is checked
- **Then** mqtt_should_publish_int returns true (forced)

> Test: `test_force_all_triggers_publish` in `test_mqtt_publish.c:104`

### CRC16 produces consistent non-zero hashes

**Requirement:** `REQ-MQTT-020`

- **Given** Known string inputs
- **When** mqtt_crc16 is called
- **Then** Different strings produce different hashes and same strings produce same hash

> Test: `test_crc16_consistency` in `test_mqtt_publish.c:122`

### Out-of-range slot is rejected

**Requirement:** `REQ-MQTT-020`

- **Given** A valid cache
- **When** mqtt_should_publish_int is called with slot >= MQTT_CACHE_MAX_SLOTS
- **Then** Returns false (no crash, no publish)

> Test: `test_invalid_slot_rejected` in `test_mqtt_publish.c:143`

### MQTT_SLOT_COUNT fits within MQTT_CACHE_MAX_SLOTS

**Requirement:** `REQ-MQTT-020`

- **Given** The enum definition
- **When** MQTT_SLOT_COUNT is compared to MQTT_CACHE_MAX_SLOTS
- **Then** MQTT_SLOT_COUNT is less than or equal to MQTT_CACHE_MAX_SLOTS

> Test: `test_slot_count_within_bounds` in `test_mqtt_publish.c:159`

---

## Multi-Node Load Balancing

### Four EVSEs receive equal current distribution

**Requirement:** `REQ-MULTI-001`

- **Given** Master with 4 EVSEs all in STATE_C, MaxCircuit=64A, no EV meter baseload
- **When** Balanced current is calculated
- **Then** Each EVSE receives 160 deciamps (640 / 4 = 160) with all values equal

> Test: `test_four_evse_fair_distribution` in `test_multi_node.c:1`

### Master EVSE BalancedMax is derived from ChargeCurrent

**Requirement:** `REQ-MULTI-002`

- **Given** Master with 4 EVSEs all in STATE_C, master ChargeCurrent limited to 200 deciamps (20A)
- **When** Balanced current is calculated
- **Then** Master EVSE (Balanced[0]) does not exceed its ChargeCurrent limit of 200 deciamps

> Test: `test_four_evse_master_max_from_chargecurrent` in `test_multi_node.c:74`

### One EVSE with low max capacity, remainder shared by others

**Requirement:** `REQ-MULTI-003`

- **Given** Master with 3 EVSEs in STATE_C, node 1 limited to BalancedMax=60 deciamps (6A)
- **When** Balanced current is calculated
- **Then** Node 1 receives at most 60 deciamps while nodes 0 and 2 share the remainder equally

> Test: `test_one_evse_low_max_others_share` in `test_multi_node.c:102`

### Node going offline causes redistribution to remaining active nodes

**Requirement:** `REQ-MULTI-004`

- **Given** Master with 3 EVSEs in STATE_C receiving equal distribution
- **When** Node 2 goes offline (BalancedState changes to STATE_A) and current is recalculated
- **Then** Remaining active nodes each receive more current than before and offline node gets 0

> Test: `test_node_goes_offline_redistributes` in `test_multi_node.c:130`

### Priority scheduling allocates available power to highest-priority EVSE during shortage

**Requirement:** `REQ-MULTI-005`

- **Given** Master with 4 EVSEs in STATE_C in Smart mode, available power ~105 dA (enough for 1 EVSE, not 4)
- **When** Balanced current is calculated with insufficient power for all nodes
- **Then** Highest-priority EVSE gets all available power, others are paused

> Test: `test_all_nodes_mincurrent_during_shortage` in `test_multi_node.c:159`

### MaxCircuit limits total current distribution across all nodes

**Requirement:** `REQ-MULTI-006`

- **Given** Master with 4 EVSEs in STATE_C in Normal mode with MaxCircuit=24A
- **When** Balanced current is calculated
- **Then** Total distributed current across all nodes does not exceed 240 deciamps (MaxCircuit * 10)

> Test: `test_maxcircuit_limits_total_distribution` in `test_multi_node.c:189`

### MaxCircuit accounts for EV meter baseload in distribution

**Requirement:** `REQ-MULTI-007`

- **Given** Master with 2 EVSEs in STATE_C, MaxCircuit=20A, EV meter measuring 250 deciamps total
- **When** Balanced current is calculated with EV meter baseload subtracted
- **Then** Total distributed current does not exceed (MaxCircuit * 10) minus baseload

> Test: `test_maxcircuit_with_ev_meter_baseload` in `test_multi_node.c:217`

### Six EVSEs in large cluster receive fair distribution

**Requirement:** `REQ-MULTI-008`

- **Given** Master with 6 EVSEs all in STATE_C, MaxCircuit=64A
- **When** Balanced current is calculated
- **Then** All 6 EVSEs receive equal current within 1 deciamp tolerance (integer division rounding)

> Test: `test_six_evse_fair_distribution` in `test_multi_node.c:243`

### NoCurrent counter increments during hard power shortage

**Requirement:** `REQ-MULTI-009`

- **Given** Master with 4 EVSEs in Smart mode, 50A mains measured against 25A limit, IsetBalanced too low
- **When** Balanced current is calculated and total MinCurrent demand exceeds available power
- **Then** NoCurrent counter increments above 0 indicating sustained shortage

> Test: `test_nocurrent_increments_on_hard_shortage` in `test_multi_node.c:269`

### NoCurrent counter clears when sufficient power is available

**Requirement:** `REQ-MULTI-010`

- **Given** Master with 2 EVSEs in Smart mode, low mains load, IsetBalanced=400, NoCurrent previously at 5
- **When** Balanced current is calculated with plenty of available power
- **Then** NoCurrent counter decays by 1 (gradual recovery)

> Test: `test_nocurrent_zero_when_sufficient` in `test_multi_node.c:296`

### Node in STATE_B does not participate in current distribution

**Requirement:** `REQ-MULTI-011`

- **Given** Master with 3 EVSEs, nodes 0 and 2 in STATE_C, node 1 in STATE_B (waiting)
- **When** Balanced current is calculated
- **Then** Only active STATE_C nodes (0 and 2) receive distributed current, each getting 320 deciamps

> Test: `test_state_b_node_gets_no_current` in `test_multi_node.c:321`

### IsetBalanced is capped at the sum of all active node maximums

**Requirement:** `REQ-MULTI-012`

- **Given** Master with 2 EVSEs in STATE_C, node 1 limited to BalancedMax=80 deciamps (8A)
- **When** Balanced current is calculated with IsetBalanced exceeding ActiveMax (320+80=400)
- **Then** IsetBalanced is capped to 400, node 0 gets 320 and node 1 gets 80

> Test: `test_isetbalanced_capped_at_active_max` in `test_multi_node.c:346`

### Three EVSEs with all different BalancedMax values

**Requirement:** `REQ-MULTI-013`

- **Given** Master with 3 EVSEs: max 320, 160, 80 deciamps, MaxCircuit=64A
- **When** Balanced current is calculated
- **Then** Each EVSE capped at its max, remainder redistributed to uncapped EVSEs

> Test: `test_three_evse_all_different_max` in `test_multi_node.c:371`

### Tight circuit with unequal max: surplus from small EVSE redistributed

**Requirement:** `REQ-MULTI-014`

- **Given** 2 EVSEs: EVSE[0] max 320, EVSE[1] max 60 (MinCurrent), MaxCircuit=25A
- **When** Balanced current is calculated
- **Then** EVSE[1] gets 60, EVSE[0] gets remainder (250-60=190)

> Test: `test_unequal_max_tight_circuit` in `test_multi_node.c:402`

---

## OCPP Current Limiting

### OCPP current limit exactly at MinCurrent boundary is accepted

**Requirement:** `REQ-OCPP-001`

- **Given** EVSE is standalone in STATE_C with OcppMode enabled and MinCurrent=6A
- **When** OcppCurrentLimit is set to 6.0A (exactly MinCurrent) and balanced current is calculated
- **Then** ChargeCurrent is set to 60 deciamps (6A) because the limit equals MinCurrent

> Test: `test_ocpp_limit_equal_to_mincurrent` in `test_ocpp.c:1`

### OCPP current limit exactly at MaxCurrent does not reduce current

**Requirement:** `REQ-OCPP-002`

- **Given** EVSE is standalone in STATE_C with OcppMode enabled and MaxCurrent=16A
- **When** OcppCurrentLimit is set to 16.0A (exactly MaxCurrent) and balanced current is calculated
- **Then** ChargeCurrent remains at 160 deciamps (16A) because OCPP limit does not cap below MaxCurrent

> Test: `test_ocpp_limit_equal_to_maxcurrent` in `test_ocpp.c:60`

### OCPP current limit above MaxCurrent does not increase current beyond MaxCurrent

**Requirement:** `REQ-OCPP-003`

- **Given** EVSE is standalone in STATE_C with OcppMode enabled and MaxCurrent=16A
- **When** OcppCurrentLimit is set to 32.0A (well above MaxCurrent) and balanced current is calculated
- **Then** ChargeCurrent stays at 160 deciamps (MaxCurrent) because OCPP cannot raise current above hardware limit

> Test: `test_ocpp_limit_above_maxcurrent_no_increase` in `test_ocpp.c:78`

### OCPP current limit is ignored when LoadBl is set to master

**Requirement:** `REQ-OCPP-004`

- **Given** EVSE is a master (LoadBl=1) in STATE_C with OcppMode enabled
- **When** OcppCurrentLimit is set to 3.0A (below MinCurrent) and balanced current is calculated
- **Then** ChargeCurrent remains at 160 deciamps because OCPP limit requires standalone mode (LoadBl=0)

> Test: `test_ocpp_ignored_when_loadbl_master` in `test_ocpp.c:96`

### OCPP current limit is ignored when LoadBl is set to node

**Requirement:** `REQ-OCPP-005`

- **Given** EVSE is a node (LoadBl=2) in STATE_C with OcppMode enabled
- **When** OcppCurrentLimit is set to 3.0A (below MinCurrent) and balanced current is calculated
- **Then** ChargeCurrent remains at 160 deciamps because OCPP limit requires standalone mode (LoadBl=0)

> Test: `test_ocpp_ignored_when_loadbl_node` in `test_ocpp.c:115`

### OverrideCurrent takes precedence over OCPP limit

**Requirement:** `REQ-OCPP-006`

- **Given** EVSE is standalone in STATE_C with OcppCurrentLimit=10.0A and OverrideCurrent=80 deciamps
- **When** Balanced current is calculated
- **Then** ChargeCurrent is set to 80 deciamps because OverrideCurrent is applied after OCPP capping

> Test: `test_override_current_overrides_ocpp` in `test_ocpp.c:134`

### OverrideCurrent restores charging even when OCPP would zero the current

**Requirement:** `REQ-OCPP-007`

- **Given** EVSE is standalone in STATE_C with OcppCurrentLimit=3.0A (below MinCurrent) and OverrideCurrent=120
- **When** Balanced current is calculated
- **Then** ChargeCurrent is set to 120 deciamps because OverrideCurrent overrides the OCPP-zeroed value

> Test: `test_override_current_overrides_ocpp_zero` in `test_ocpp.c:151`

### OCPP current limit of 0.0A zeros the charge current

**Requirement:** `REQ-OCPP-008`

- **Given** EVSE is standalone in STATE_C with OcppMode enabled
- **When** OcppCurrentLimit is set to 0.0A and balanced current is calculated
- **Then** ChargeCurrent is zeroed because 0.0A is below MinCurrent (6A)

> Test: `test_ocpp_limit_zero_zeros_current` in `test_ocpp.c:170`

### Negative OCPP current limit is treated as no limit set

**Requirement:** `REQ-OCPP-009`

- **Given** EVSE is standalone in STATE_C with OcppMode enabled
- **When** OcppCurrentLimit is set to -1.0A (default init value meaning no limit)
- **Then** ChargeCurrent remains at 160 deciamps because the OCPP capping block is skipped

> Test: `test_ocpp_negative_limit_no_restriction` in `test_ocpp.c:188`

### OCPP limit of 0.0A blocks current availability check

**Requirement:** `REQ-OCPP-010`

- **Given** EVSE is standalone with OcppMode enabled and OcppCurrentLimit=0.0A
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because OCPP limit is below MinCurrent

> Test: `test_ocpp_blocks_current_available_at_zero` in `test_ocpp.c:206`

### OCPP limit at MinCurrent allows current availability

**Requirement:** `REQ-OCPP-011`

- **Given** EVSE is standalone with OcppMode enabled and OcppCurrentLimit=6.0A (MinCurrent)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because 6.0A is not less than MinCurrent

> Test: `test_ocpp_allows_current_available_at_mincurrent` in `test_ocpp.c:223`

### Negative OCPP limit does not block current availability

**Requirement:** `REQ-OCPP-012`

- **Given** EVSE is standalone with OcppMode enabled and OcppCurrentLimit=-1.0A (no limit)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because negative limit skips the OCPP availability check

> Test: `test_ocpp_negative_limit_allows_current_available` in `test_ocpp.c:241`

---

## OCPP Authorization

### Auth path selection returns OCPP-controlled for RFIDReader=6

**Requirement:** `REQ-OCPP-025`

- **Given** RFIDReader is set to 6 (Rmt/OCPP mode)
- **When** ocpp_select_auth_path is called
- **Then** Returns OCPP_AUTH_PATH_OCPP_CONTROLLED because OCPP controls Access_bit

> Test: `test_auth_path_rfid_reader_6` in `test_ocpp_auth.c:1`

### Auth path selection returns OCPP-controlled for RFIDReader=0

**Requirement:** `REQ-OCPP-026`

- **Given** RFIDReader is set to 0 (Disabled)
- **When** ocpp_select_auth_path is called
- **Then** Returns OCPP_AUTH_PATH_OCPP_CONTROLLED because OCPP controls Access_bit when RFID is disabled

> Test: `test_auth_path_rfid_reader_0` in `test_ocpp_auth.c:26`

### Auth path selection returns builtin-RFID for RFIDReader=1..5

**Requirement:** `REQ-OCPP-027`

- **Given** RFIDReader is set to 1 (built-in RFID store)
- **When** ocpp_select_auth_path is called
- **Then** Returns OCPP_AUTH_PATH_BUILTIN_RFID because built-in RFID controls charging

> Test: `test_auth_path_rfid_reader_1` in `test_ocpp_auth.c:38`

### Auth path selection returns builtin-RFID for RFIDReader=5

**Requirement:** `REQ-OCPP-027`

- **Given** RFIDReader is set to 5
- **When** ocpp_select_auth_path is called
- **Then** Returns OCPP_AUTH_PATH_BUILTIN_RFID

> Test: `test_auth_path_rfid_reader_5` in `test_ocpp_auth.c:50`

### Auth path selection returns builtin-RFID for RFIDReader=3

**Requirement:** `REQ-OCPP-027`

- **Given** RFIDReader is set to 3
- **When** ocpp_select_auth_path is called
- **Then** Returns OCPP_AUTH_PATH_BUILTIN_RFID

> Test: `test_auth_path_rfid_reader_3` in `test_ocpp_auth.c:62`

### OCPP sets Access_bit on rising edge of permits_charge

**Requirement:** `REQ-OCPP-020`

- **Given** Previous permits_charge was false
- **When** Current permits_charge transitions to true
- **Then** ocpp_should_set_access returns true (set Access_bit ON)

> Test: `test_should_set_access_on_rising_edge` in `test_ocpp_auth.c:76`

### OCPP does not re-set Access_bit if already permitted

**Requirement:** `REQ-OCPP-022`

- **Given** Previous permits_charge was already true
- **When** Current permits_charge is still true
- **Then** ocpp_should_set_access returns false (Access_bit already set once)

> Test: `test_should_not_set_access_when_already_permitted` in `test_ocpp_auth.c:88`

### OCPP does not set Access_bit when charge not permitted

**Requirement:** `REQ-OCPP-020`

- **Given** Previous permits_charge was false
- **When** Current permits_charge is still false
- **Then** ocpp_should_set_access returns false

> Test: `test_should_not_set_access_when_not_permitted` in `test_ocpp_auth.c:100`

### OCPP does not set Access_bit on falling edge

**Requirement:** `REQ-OCPP-020`

- **Given** Previous permits_charge was true
- **When** Current permits_charge transitions to false
- **Then** ocpp_should_set_access returns false

> Test: `test_should_not_set_access_on_falling_edge` in `test_ocpp_auth.c:112`

### OCPP clears Access_bit when permission revoked and access is ON

**Requirement:** `REQ-OCPP-021`

- **Given** AccessStatus is ON (1) and permits_charge is false
- **When** ocpp_should_clear_access is called
- **Then** Returns true (clear Access_bit)

> Test: `test_should_clear_access_when_revoked_and_on` in `test_ocpp_auth.c:126`

### OCPP does not clear Access_bit when permission is still granted

**Requirement:** `REQ-OCPP-021`

- **Given** AccessStatus is ON (1) and permits_charge is true
- **When** ocpp_should_clear_access is called
- **Then** Returns false (Access_bit should stay)

> Test: `test_should_not_clear_access_when_still_permitted` in `test_ocpp_auth.c:138`

### OCPP does not clear Access_bit when access is already OFF

**Requirement:** `REQ-OCPP-021`

- **Given** AccessStatus is OFF (0) and permits_charge is false
- **When** ocpp_should_clear_access is called
- **Then** Returns false (Access_bit already cleared)

> Test: `test_should_not_clear_access_when_already_off` in `test_ocpp_auth.c:150`

### OCPP does not clear Access_bit when access is PAUSE

**Requirement:** `REQ-OCPP-021`

- **Given** AccessStatus is PAUSE (2) and permits_charge is false
- **When** ocpp_should_clear_access is called
- **Then** Returns false because the clear logic only triggers on ON, not PAUSE

> Test: `test_should_not_clear_access_when_paused` in `test_ocpp_auth.c:162`

### FreeVend + Solar mode with NO_SUN defers Access_bit

**Requirement:** `REQ-OCPP-028`

- **Given** Mode is Solar, ErrorFlags has NO_SUN set, ChargeDelay=0
- **When** ocpp_should_defer_access is called
- **Then** Returns true because Solar mode has no surplus available

> Test: `test_defer_access_solar_no_sun` in `test_ocpp_auth.c:176`

### FreeVend + Solar mode without NO_SUN does not defer

**Requirement:** `REQ-OCPP-028`

- **Given** Mode is Solar, ErrorFlags is clear (surplus available), ChargeDelay=0
- **When** ocpp_should_defer_access is called
- **Then** Returns false because solar surplus is available

> Test: `test_no_defer_access_solar_with_surplus` in `test_ocpp_auth.c:188`

### FreeVend + ChargeDelay active defers Access_bit

**Requirement:** `REQ-OCPP-029`

- **Given** Mode is Normal, ChargeDelay=60 (delay active), ErrorFlags clear
- **When** ocpp_should_defer_access is called
- **Then** Returns true because ChargeDelay is active

> Test: `test_defer_access_charge_delay_active` in `test_ocpp_auth.c:200`

### FreeVend + ChargeDelay=0 does not defer in Normal mode

**Requirement:** `REQ-OCPP-029`

- **Given** Mode is Normal, ChargeDelay=0, ErrorFlags clear
- **When** ocpp_should_defer_access is called
- **Then** Returns false because no deferral conditions are met

> Test: `test_no_defer_access_normal_no_delay` in `test_ocpp_auth.c:212`

### FreeVend + Solar mode with ChargeDelay defers (both conditions)

**Requirement:** `REQ-OCPP-028`

- **Given** Mode is Solar, ChargeDelay=30, ErrorFlags has NO_SUN
- **When** ocpp_should_defer_access is called
- **Then** Returns true because both Solar/NO_SUN and ChargeDelay trigger deferral

> Test: `test_defer_access_solar_delay_and_no_sun` in `test_ocpp_auth.c:224`

### Smart mode with ChargeDelay defers Access_bit

**Requirement:** `REQ-OCPP-029`

- **Given** Mode is Smart, ChargeDelay=10, ErrorFlags clear
- **When** ocpp_should_defer_access is called
- **Then** Returns true because ChargeDelay is active regardless of mode

> Test: `test_defer_access_smart_with_delay` in `test_ocpp_auth.c:236`

### Smart mode without delay or errors does not defer

**Requirement:** `REQ-OCPP-028`

- **Given** Mode is Smart, ChargeDelay=0, ErrorFlags clear
- **When** ocpp_should_defer_access is called
- **Then** Returns false because Smart mode without ChargeDelay has no deferral

> Test: `test_no_defer_access_smart_no_delay` in `test_ocpp_auth.c:248`

### Normal mode with NO_SUN error does not defer (only Solar checks NO_SUN)

**Requirement:** `REQ-OCPP-028`

- **Given** Mode is Normal, ErrorFlags has NO_SUN, ChargeDelay=0
- **When** ocpp_should_defer_access is called
- **Then** Returns false because NO_SUN deferral only applies in Solar mode

> Test: `test_no_defer_access_normal_with_no_sun` in `test_ocpp_auth.c:260`

---

## OCPP Connector State

### CP voltage PILOT_3V indicates connector plugged

**Requirement:** `REQ-OCPP-040`

- **Given** CP voltage is PILOT_3V (3V, State C/D)
- **When** ocpp_is_connector_plugged is called
- **Then** Returns true because PILOT_3V is within plugged range

> Test: `test_connector_plugged_at_3v` in `test_ocpp_connector.c:1`

### CP voltage PILOT_6V indicates connector plugged

**Requirement:** `REQ-OCPP-040`

- **Given** CP voltage is PILOT_6V (6V, State C)
- **When** ocpp_is_connector_plugged is called
- **Then** Returns true because PILOT_6V is within plugged range

> Test: `test_connector_plugged_at_6v` in `test_ocpp_connector.c:26`

### CP voltage PILOT_9V indicates connector plugged

**Requirement:** `REQ-OCPP-040`

- **Given** CP voltage is PILOT_9V (9V, State B)
- **When** ocpp_is_connector_plugged is called
- **Then** Returns true because PILOT_9V is within plugged range

> Test: `test_connector_plugged_at_9v` in `test_ocpp_connector.c:38`

### CP voltage PILOT_12V indicates connector unplugged

**Requirement:** `REQ-OCPP-041`

- **Given** CP voltage is PILOT_12V (12V, State A)
- **When** ocpp_is_connector_plugged is called
- **Then** Returns false because PILOT_12V means no vehicle connected

> Test: `test_connector_unplugged_at_12v` in `test_ocpp_connector.c:50`

### CP voltage PILOT_NOK indicates connector unplugged

**Requirement:** `REQ-OCPP-042`

- **Given** CP voltage is PILOT_NOK (0, fault condition)
- **When** ocpp_is_connector_plugged is called
- **Then** Returns false because PILOT_NOK is outside plugged range

> Test: `test_connector_unplugged_at_nok` in `test_ocpp_connector.c:62`

### CP voltage PILOT_DIODE indicates connector unplugged

**Requirement:** `REQ-OCPP-042`

- **Given** CP voltage is PILOT_DIODE (1, diode check)
- **When** ocpp_is_connector_plugged is called
- **Then** Returns false because PILOT_DIODE is below PILOT_3V

> Test: `test_connector_unplugged_at_diode` in `test_ocpp_connector.c:74`

### CP voltage PILOT_SHORT indicates connector unplugged

**Requirement:** `REQ-OCPP-042`

- **Given** CP voltage is PILOT_SHORT (255, short circuit)
- **When** ocpp_is_connector_plugged is called
- **Then** Returns false because PILOT_SHORT is above PILOT_9V

> Test: `test_connector_unplugged_at_short` in `test_ocpp_connector.c:86`

### CP voltage PILOT_3V indicates EV ready (State C/D)

**Requirement:** `REQ-OCPP-043`

- **Given** CP voltage is PILOT_3V
- **When** ocpp_is_ev_ready is called
- **Then** Returns true because PILOT_3V is within EV-ready range

> Test: `test_ev_ready_at_3v` in `test_ocpp_connector.c:100`

### CP voltage PILOT_6V indicates EV ready (State C)

**Requirement:** `REQ-OCPP-043`

- **Given** CP voltage is PILOT_6V
- **When** ocpp_is_ev_ready is called
- **Then** Returns true because PILOT_6V is within EV-ready range

> Test: `test_ev_ready_at_6v` in `test_ocpp_connector.c:112`

### CP voltage PILOT_9V indicates EV connected but not ready (State B)

**Requirement:** `REQ-OCPP-044`

- **Given** CP voltage is PILOT_9V
- **When** ocpp_is_ev_ready is called
- **Then** Returns false because State B means connected but not requesting charge

> Test: `test_ev_not_ready_at_9v` in `test_ocpp_connector.c:124`

### CP voltage PILOT_12V indicates no EV (State A)

**Requirement:** `REQ-OCPP-044`

- **Given** CP voltage is PILOT_12V
- **When** ocpp_is_ev_ready is called
- **Then** Returns false because no vehicle is connected

> Test: `test_ev_not_ready_at_12v` in `test_ocpp_connector.c:136`

### CP voltage PILOT_NOK indicates EV not ready

**Requirement:** `REQ-OCPP-044`

- **Given** CP voltage is PILOT_NOK (fault)
- **When** ocpp_is_ev_ready is called
- **Then** Returns false

> Test: `test_ev_not_ready_at_nok` in `test_ocpp_connector.c:148`

---

## OCPP IEC 61851 Status Mapping

### State A without active transaction maps to Available

**Requirement:** `REQ-OCPP-090`

- **Given** IEC 61851 state is A (no vehicle), no transaction active
- **When** ocpp_iec61851_to_status is called
- **Then** Returns "Available"

> Test: `test_iec_a_no_tx_available` in `test_ocpp_iec61851.c:1`

### State A with active transaction maps to Finishing

**Requirement:** `REQ-OCPP-090`

- **Given** IEC 61851 state is A (no vehicle), transaction still active (just unplugged)
- **When** ocpp_iec61851_to_status is called
- **Then** Returns "Finishing" because the transaction is ending

> Test: `test_iec_a_tx_active_finishing` in `test_ocpp_iec61851.c:28`

### State B without transaction maps to Preparing

**Requirement:** `REQ-OCPP-091`

- **Given** IEC 61851 state is B (vehicle connected), no transaction
- **When** ocpp_iec61851_to_status is called
- **Then** Returns "Preparing" because the vehicle is waiting for authorization

> Test: `test_iec_b_no_tx_preparing` in `test_ocpp_iec61851.c:43`

### State B with active transaction maps to SuspendedEV

**Requirement:** `REQ-OCPP-091`

- **Given** IEC 61851 state is B (connected but not drawing), transaction active
- **When** ocpp_iec61851_to_status is called
- **Then** Returns "SuspendedEV" because the EV has paused charging

> Test: `test_iec_b_tx_active_suspended_ev` in `test_ocpp_iec61851.c:56`

### State C with EVSE offering current maps to Charging

**Requirement:** `REQ-OCPP-092`

- **Given** IEC 61851 state is C (charging), EVSE ready (PWM > 0)
- **When** ocpp_iec61851_to_status is called
- **Then** Returns "Charging"

> Test: `test_iec_c_evse_ready_charging` in `test_ocpp_iec61851.c:71`

### State C with EVSE not offering current maps to SuspendedEVSE

**Requirement:** `REQ-OCPP-092`

- **Given** IEC 61851 state is C, EVSE not ready (current = 0, e.g. OCPP limit)
- **When** ocpp_iec61851_to_status is called
- **Then** Returns "SuspendedEVSE" because the EVSE has paused charging

> Test: `test_iec_c_evse_not_ready_suspended_evse` in `test_ocpp_iec61851.c:84`

### State D with EVSE ready maps to Charging

**Requirement:** `REQ-OCPP-093`

- **Given** IEC 61851 state is D (charging with ventilation), EVSE ready
- **When** ocpp_iec61851_to_status is called
- **Then** Returns "Charging" (same as State C for OCPP)

> Test: `test_iec_d_evse_ready_charging` in `test_ocpp_iec61851.c:99`

### State D with EVSE not ready maps to SuspendedEVSE

**Requirement:** `REQ-OCPP-093`

- **Given** IEC 61851 state is D, EVSE not ready
- **When** ocpp_iec61851_to_status is called
- **Then** Returns "SuspendedEVSE"

> Test: `test_iec_d_evse_not_ready_suspended_evse` in `test_ocpp_iec61851.c:112`

### State E maps to Faulted

**Requirement:** `REQ-OCPP-094`

- **Given** IEC 61851 state is E (error)
- **When** ocpp_iec61851_to_status is called
- **Then** Returns "Faulted"

> Test: `test_iec_e_faulted` in `test_ocpp_iec61851.c:127`

### State F maps to Faulted

**Requirement:** `REQ-OCPP-094`

- **Given** IEC 61851 state is F (not available)
- **When** ocpp_iec61851_to_status is called
- **Then** Returns "Faulted"

> Test: `test_iec_f_faulted` in `test_ocpp_iec61851.c:140`

### Unknown state maps to Faulted

**Requirement:** `REQ-OCPP-094`

- **Given** IEC 61851 state is an invalid character
- **When** ocpp_iec61851_to_status is called
- **Then** Returns "Faulted" as a safe default

> Test: `test_iec_unknown_faulted` in `test_ocpp_iec61851.c:153`

---

## OCPP Load Balancing Exclusivity

### OCPP+LoadBl=0 has no conflict, Smart Charging active

**Requirement:** `REQ-OCPP-030`

- **Given** OCPP is enabled, LoadBl=0 (standalone), and OCPP was initialized in standalone mode
- **When** ocpp_check_lb_exclusivity is called
- **Then** Returns OCPP_LB_OK because Smart Charging and LoadBl are compatible

> Test: `test_lb_standalone_no_conflict` in `test_ocpp_lb.c:1`

### OCPP disabled has no conflict regardless of LoadBl

**Requirement:** `REQ-OCPP-030`

- **Given** OCPP is disabled, LoadBl=1
- **When** ocpp_check_lb_exclusivity is called
- **Then** Returns OCPP_LB_OK because OCPP is not active

> Test: `test_lb_ocpp_disabled_no_conflict` in `test_ocpp_lb.c:27`

### OCPP+LoadBl=1 is a conflict, Smart Charging ineffective

**Requirement:** `REQ-OCPP-031`

- **Given** OCPP is enabled, LoadBl=1 (master), OCPP was initialized standalone
- **When** ocpp_check_lb_exclusivity is called
- **Then** Returns OCPP_LB_CONFLICT because the state machine ignores OCPP limits when LoadBl!=0

> Test: `test_lb_master_conflict` in `test_ocpp_lb.c:42`

### OCPP+LoadBl=2 (node) is a conflict

**Requirement:** `REQ-OCPP-031`

- **Given** OCPP is enabled, LoadBl=2 (node), OCPP was initialized standalone
- **When** ocpp_check_lb_exclusivity is called
- **Then** Returns OCPP_LB_CONFLICT

> Test: `test_lb_node_conflict` in `test_ocpp_lb.c:55`

### LoadBl changes from 0 to 1 while OCPP active is a conflict

**Requirement:** `REQ-OCPP-032`

- **Given** OCPP is enabled, LoadBl changed to 1 at runtime, was_standalone=true
- **When** ocpp_check_lb_exclusivity is called
- **Then** Returns OCPP_LB_CONFLICT because Smart Charging callback is still registered but limits are ignored

> Test: `test_lb_changed_0_to_1_conflict` in `test_ocpp_lb.c:68`

### LoadBl changes from 1 to 0 while OCPP active needs reinit

**Requirement:** `REQ-OCPP-033`

- **Given** OCPP is enabled, LoadBl=0 now, but was_standalone=false (was non-zero at init)
- **When** ocpp_check_lb_exclusivity is called
- **Then** Returns OCPP_LB_NEEDS_REINIT because Smart Charging callback was never registered

> Test: `test_lb_changed_1_to_0_needs_reinit` in `test_ocpp_lb.c:83`

### Runtime transition: standalone → master → back to standalone

**Requirement:** `REQ-OCPP-032`

- **Given** OCPP was initialized in standalone mode (was_standalone=true)
- **When** LoadBl changes 0→1 (conflict) then 1→0 (back to standalone)
- **Then** First check returns CONFLICT, second returns OK (was_standalone still true from init)

> Test: `test_lb_transition_standalone_master_standalone` in `test_ocpp_lb.c:98`

### Runtime transition: master init → standalone change

**Requirement:** `REQ-OCPP-033`

- **Given** OCPP was initialized in master mode (was_standalone=false)
- **When** LoadBl changes from 1 to 0
- **Then** Returns NEEDS_REINIT because Smart Charging was never registered

> Test: `test_lb_transition_master_init_to_standalone` in `test_ocpp_lb.c:119`

### High LoadBl values (nodes 3-8) all trigger conflict

**Requirement:** `REQ-OCPP-034`

- **Given** OCPP is enabled and was initialized standalone
- **When** LoadBl is set to values 3 through 8
- **Then** All return OCPP_LB_CONFLICT

> Test: `test_lb_all_node_values_conflict` in `test_ocpp_lb.c:139`

---

## OCPP RFID Formatting

### New reader 7-byte UUID formatted as 14-char hex string

**Requirement:** `REQ-OCPP-054`

- **Given** RFID bytes {0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67} (new reader)
- **When** ocpp_format_rfid_hex is called
- **Then** Output is "ABCDEF01234567" (7 bytes * 2 hex chars)

> Test: `test_rfid_new_reader_7byte` in `test_ocpp_rfid.c:1`

### UUID with leading zeros preserved in hex output

**Requirement:** `REQ-OCPP-052`

- **Given** RFID bytes {0x00, 0x00, 0x0A, 0x0B, 0x00, 0x00, 0x00} (new reader with leading zeros)
- **When** ocpp_format_rfid_hex is called
- **Then** Output is "0000 0A0B000000" with leading zeros preserved

> Test: `test_rfid_leading_zeros_preserved` in `test_ocpp_rfid.c:30`

### Old reader format uses RFID[1..6] offset

**Requirement:** `REQ-OCPP-053`

- **Given** RFID bytes {0x01, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF} (old reader flag at [0])
- **When** ocpp_format_rfid_hex is called
- **Then** Output is "AABBCCDDEEFF" (6 bytes from [1] to [6])

> Test: `test_rfid_old_reader_6byte` in `test_ocpp_rfid.c:47`

### Old reader with leading zero bytes preserved

**Requirement:** `REQ-OCPP-053`

- **Given** RFID bytes {0x01, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04} (old reader)
- **When** ocpp_format_rfid_hex is called
- **Then** Output is "000001020304" with leading zeros from [1] preserved

> Test: `test_rfid_old_reader_leading_zeros` in `test_ocpp_rfid.c:62`

### 4-byte UUID formatted as 8-char hex string

**Requirement:** `REQ-OCPP-050`

- **Given** RFID bytes {0xDE, 0xAD, 0xBE, 0xEF} (4-byte UID, new reader)
- **When** ocpp_format_rfid_hex is called with rfid_len=4
- **Then** Output is "DEADBEEF" (4 bytes * 2 hex chars)

> Test: `test_rfid_4byte_new_reader` in `test_ocpp_rfid.c:79`

### NULL RFID input produces empty string

**Requirement:** `REQ-OCPP-050`

- **Given** RFID pointer is NULL
- **When** ocpp_format_rfid_hex is called
- **Then** Output is empty string

> Test: `test_rfid_null_input` in `test_ocpp_rfid.c:96`

### Zero-length RFID produces empty string

**Requirement:** `REQ-OCPP-050`

- **Given** RFID bytes exist but length is 0
- **When** ocpp_format_rfid_hex is called
- **Then** Output is empty string

> Test: `test_rfid_zero_length` in `test_ocpp_rfid.c:111`

---

## OCPP Settings Validation

### Valid wss:// URL accepted

**Requirement:** `REQ-OCPP-060`

- **Given** URL is "wss://ocpp.example.com/smartevse"
- **When** ocpp_validate_backend_url is called
- **Then** Returns OCPP_VALIDATE_OK

> Test: `test_url_valid_wss` in `test_ocpp_settings.c:1`

### Valid ws:// URL accepted

**Requirement:** `REQ-OCPP-061`

- **Given** URL is "ws://192.168.1.100:8180/steve/websocket/CentralSystemService"
- **When** ocpp_validate_backend_url is called
- **Then** Returns OCPP_VALIDATE_OK

> Test: `test_url_valid_ws` in `test_ocpp_settings.c:27`

### URL without ws/wss scheme rejected

**Requirement:** `REQ-OCPP-062`

- **Given** URL is "http://ocpp.example.com"
- **When** ocpp_validate_backend_url is called
- **Then** Returns OCPP_VALIDATE_BAD_SCHEME

> Test: `test_url_http_rejected` in `test_ocpp_settings.c:40`

### URL with https scheme rejected

**Requirement:** `REQ-OCPP-062`

- **Given** URL is "https://ocpp.example.com"
- **When** ocpp_validate_backend_url is called
- **Then** Returns OCPP_VALIDATE_BAD_SCHEME

> Test: `test_url_https_rejected` in `test_ocpp_settings.c:53`

### Empty URL rejected

**Requirement:** `REQ-OCPP-063`

- **Given** URL is empty string
- **When** ocpp_validate_backend_url is called
- **Then** Returns OCPP_VALIDATE_EMPTY

> Test: `test_url_empty_rejected` in `test_ocpp_settings.c:66`

### NULL URL rejected

**Requirement:** `REQ-OCPP-063`

- **Given** URL pointer is NULL
- **When** ocpp_validate_backend_url is called
- **Then** Returns OCPP_VALIDATE_EMPTY

> Test: `test_url_null_rejected` in `test_ocpp_settings.c:79`

### Bare scheme without host rejected

**Requirement:** `REQ-OCPP-062`

- **Given** URL is "ws://" with no host
- **When** ocpp_validate_backend_url is called
- **Then** Returns OCPP_VALIDATE_BAD_SCHEME because there is no content after scheme

> Test: `test_url_bare_scheme_rejected` in `test_ocpp_settings.c:92`

### Bare wss scheme without host rejected

**Requirement:** `REQ-OCPP-062`

- **Given** URL is "wss://" with no host
- **When** ocpp_validate_backend_url is called
- **Then** Returns OCPP_VALIDATE_BAD_SCHEME

> Test: `test_url_bare_wss_scheme_rejected` in `test_ocpp_settings.c:105`

### Plain text without scheme rejected

**Requirement:** `REQ-OCPP-062`

- **Given** URL is "ocpp.example.com" (no ws:// prefix)
- **When** ocpp_validate_backend_url is called
- **Then** Returns OCPP_VALIDATE_BAD_SCHEME

> Test: `test_url_no_scheme_rejected` in `test_ocpp_settings.c:118`

### Valid ChargeBoxId accepted

**Requirement:** `REQ-OCPP-064`

- **Given** ChargeBoxId is "SmartEVSE-12345"
- **When** ocpp_validate_chargebox_id is called
- **Then** Returns OCPP_VALIDATE_OK

> Test: `test_cbid_valid` in `test_ocpp_settings.c:133`

### ChargeBoxId with special characters rejected

**Requirement:** `REQ-OCPP-064`

- **Given** ChargeBoxId contains '<' character
- **When** ocpp_validate_chargebox_id is called
- **Then** Returns OCPP_VALIDATE_BAD_CHARS

> Test: `test_cbid_special_chars_rejected` in `test_ocpp_settings.c:146`

### ChargeBoxId length > 20 rejected (OCPP 1.6 CiString20)

**Requirement:** `REQ-OCPP-065`

- **Given** ChargeBoxId is 21 characters long
- **When** ocpp_validate_chargebox_id is called
- **Then** Returns OCPP_VALIDATE_TOO_LONG

> Test: `test_cbid_too_long_rejected` in `test_ocpp_settings.c:159`

### ChargeBoxId exactly 20 characters is accepted

**Requirement:** `REQ-OCPP-065`

- **Given** ChargeBoxId is exactly 20 characters long
- **When** ocpp_validate_chargebox_id is called
- **Then** Returns OCPP_VALIDATE_OK

> Test: `test_cbid_exactly_20_accepted` in `test_ocpp_settings.c:172`

### Empty ChargeBoxId rejected

**Requirement:** `REQ-OCPP-064`

- **Given** ChargeBoxId is empty string
- **When** ocpp_validate_chargebox_id is called
- **Then** Returns OCPP_VALIDATE_EMPTY

> Test: `test_cbid_empty_rejected` in `test_ocpp_settings.c:185`

### ChargeBoxId with ampersand rejected

**Requirement:** `REQ-OCPP-064`

- **Given** ChargeBoxId contains '&' character
- **When** ocpp_validate_chargebox_id is called
- **Then** Returns OCPP_VALIDATE_BAD_CHARS

> Test: `test_cbid_ampersand_rejected` in `test_ocpp_settings.c:198`

### Auth key length > 40 rejected (OCPP 1.6 limit)

**Requirement:** `REQ-OCPP-066`

- **Given** Auth key is 41 characters long
- **When** ocpp_validate_auth_key is called
- **Then** Returns OCPP_VALIDATE_TOO_LONG

> Test: `test_auth_key_too_long` in `test_ocpp_settings.c:213`

### Auth key exactly 40 characters is accepted

**Requirement:** `REQ-OCPP-066`

- **Given** Auth key is exactly 40 characters long
- **When** ocpp_validate_auth_key is called
- **Then** Returns OCPP_VALIDATE_OK

> Test: `test_auth_key_exactly_40_accepted` in `test_ocpp_settings.c:226`

### Empty auth key is valid (no auth configured)

**Requirement:** `REQ-OCPP-066`

- **Given** Auth key is empty string
- **When** ocpp_validate_auth_key is called
- **Then** Returns OCPP_VALIDATE_OK because empty means no auth

> Test: `test_auth_key_empty_accepted` in `test_ocpp_settings.c:239`

---

## OCPP Telemetry

### Telemetry init zeros all counters

**Requirement:** `REQ-OCPP-080`

- **Given** A telemetry struct with arbitrary values
- **When** ocpp_telemetry_init is called
- **Then** All counters and flags are zero/false

> Test: `test_telemetry_init_zeros_all` in `test_ocpp_telemetry.c:1`

### Transaction start increments counter and sets active flag

**Requirement:** `REQ-OCPP-081`

- **Given** Telemetry is initialized
- **When** ocpp_telemetry_tx_started is called
- **Then** tx_start_count is 1 and tx_active is true

> Test: `test_telemetry_tx_start` in `test_ocpp_telemetry.c:44`

### Transaction stop increments counter and clears active flag

**Requirement:** `REQ-OCPP-081`

- **Given** A transaction has been started
- **When** ocpp_telemetry_tx_stopped is called
- **Then** tx_stop_count is 1 and tx_active is false

> Test: `test_telemetry_tx_stop` in `test_ocpp_telemetry.c:59`

### Multiple transactions accumulate counters

**Requirement:** `REQ-OCPP-081`

- **Given** Telemetry is initialized
- **When** 3 transactions are started and stopped
- **Then** tx_start_count and tx_stop_count are both 3

> Test: `test_telemetry_multiple_tx` in `test_ocpp_telemetry.c:75`

### Auth accept/reject/timeout counters increment independently

**Requirement:** `REQ-OCPP-082`

- **Given** Telemetry is initialized
- **When** 2 accepts, 1 reject, 1 timeout are recorded
- **Then** Each counter reflects the correct count

> Test: `test_telemetry_auth_counters` in `test_ocpp_telemetry.c:96`

### WebSocket connect/disconnect counters track reconnections

**Requirement:** `REQ-OCPP-083`

- **Given** Telemetry is initialized
- **When** 5 connects and 4 disconnects occur (currently connected)
- **Then** ws_connect_count=5, ws_disconnect_count=4

> Test: `test_telemetry_ws_reconnect_tracking` in `test_ocpp_telemetry.c:117`

### NULL pointer does not crash

**Requirement:** `REQ-OCPP-080`

- **Given** NULL telemetry pointer
- **When** Any telemetry function is called
- **Then** No crash occurs

> Test: `test_telemetry_null_safety` in `test_ocpp_telemetry.c:137`

---

## Operating Modes

### Normal mode sets IsetBalanced to MaxCurrent

**Requirement:** `REQ-MODE-001`

- **Given** EVSE is standalone in STATE_C in Normal mode
- **When** Balanced current is calculated
- **Then** IsetBalanced equals MaxCurrent * 10 (fixed current allocation)

> Test: `test_normal_mode_uses_max_current` in `test_operating_modes.c:1`

### Normal mode ignores mains meter readings

**Requirement:** `REQ-MODE-002`

- **Given** EVSE is standalone in STATE_C in Normal mode with high MainsMeterImeasured=300
- **When** Balanced current is calculated
- **Then** IsetBalanced remains at MaxCurrent * 10 regardless of mains load

> Test: `test_normal_mode_ignores_mains` in `test_operating_modes.c:45`

### Normal mode respects MaxCapacity as upper bound

**Requirement:** `REQ-MODE-003`

- **Given** EVSE is standalone in STATE_C in Normal mode with MaxCapacity=10A and MaxCurrent=16A
- **When** Balanced current is calculated
- **Then** ChargeCurrent is limited to 100 deciamps (MaxCapacity * 10) instead of MaxCurrent

> Test: `test_normal_mode_respects_max_capacity` in `test_operating_modes.c:62`

### Smart mode limits current based on MaxMains minus baseload

**Requirement:** `REQ-MODE-004`

- **Given** EVSE is standalone in STATE_C in Smart mode with MaxMains=25A and MainsMeterImeasured=200
- **When** Balanced current is calculated
- **Then** IsetBalanced does not exceed (MaxMains * 10) minus baseload

> Test: `test_smart_mode_respects_maxmains` in `test_operating_modes.c:81`

### Smart mode increases current conservatively (Idifference/4)

**Requirement:** `REQ-MODE-005`

- **Given** EVSE is standalone in STATE_C in Smart mode with low mains usage and measurements updated
- **When** Balanced current is calculated with headroom available
- **Then** IsetBalanced increases from its initial value but conservatively (not full step)

> Test: `test_smart_mode_slow_increase` in `test_operating_modes.c:101`

### Smart mode decreases current rapidly when over mains limit

**Requirement:** `REQ-MODE-006`

- **Given** EVSE is standalone in STATE_C in Smart mode with IsetBalanced=200 and mains way over MaxMains=10A
- **When** Balanced current is calculated with negative Idifference
- **Then** IsetBalanced decreases rapidly (full Idifference, not divided) below the initial 200

> Test: `test_smart_mode_fast_decrease` in `test_operating_modes.c:123`

### Solar mode requires surplus power to make current available

**Requirement:** `REQ-MODE-007`

- **Given** EVSE is in Solar mode with StartCurrent=6A and Isum=0 (no surplus)
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because there is no solar surplus for charging

> Test: `test_solar_current_available_requires_surplus` in `test_operating_modes.c:146`

### Solar mode allows charging when sufficient surplus is available

**Requirement:** `REQ-MODE-008`

- **Given** EVSE is in Solar mode with StartCurrent=6A and Isum=-80 (8A export surplus)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because export surplus exceeds StartCurrent threshold

> Test: `test_solar_current_available_with_surplus` in `test_operating_modes.c:165`

### Solar mode increases current in small steps when surplus is available

**Requirement:** `REQ-MODE-009`

- **Given** EVSE is standalone in STATE_C in Solar mode with 2A export surplus and past solar startup phase
- **When** Balanced current is calculated
- **Then** IsetBalanced increases from its initial 100 value in fine-grained solar increments

> Test: `test_solar_fine_grained_increase` in `test_operating_modes.c:185`

### Solar mode decreases current rapidly when importing from grid

**Requirement:** `REQ-MODE-010`

- **Given** EVSE is standalone in STATE_C in Solar mode with Isum=50 (5A import) and IsetBalanced=100
- **When** Balanced current is calculated with grid import detected
- **Then** IsetBalanced decreases below 100 to reduce grid import quickly

> Test: `test_solar_rapid_decrease_on_import` in `test_operating_modes.c:209`

### Solar mode ImportCurrent offset allows controlled grid import

**Requirement:** `REQ-MODE-011`

- **Given** EVSE is in Solar mode with ImportCurrent=3A allowance and Isum=20 (2A import within allowance)
- **When** Balanced current is calculated with import within the allowed offset
- **Then** IsetBalanced increases because IsumImport (20 - 30 = -10) indicates effective surplus

> Test: `test_solar_import_current_offset` in `test_operating_modes.c:233`

### EnableC2=NOT_PRESENT does not force single phase

**Requirement:** `REQ-MODE-012`

- **Given** EVSE has EnableC2 set to NOT_PRESENT (contactor 2 not installed)
- **When** evse_force_single_phase is called
- **Then** Returns 0 because the phase switching hardware is not present

> Test: `test_force_single_phase_not_present` in `test_operating_modes.c:259`

### EnableC2=ALWAYS_OFF forces single phase operation

**Requirement:** `REQ-MODE-013`

- **Given** EVSE has EnableC2 set to ALWAYS_OFF (contactor 2 always disabled)
- **When** evse_force_single_phase is called
- **Then** Returns 1 because the EVSE is configured to always operate in single phase

> Test: `test_force_single_phase_always_off` in `test_operating_modes.c:273`

### EnableC2=SOLAR_OFF forces single phase when in Solar mode

**Requirement:** `REQ-MODE-014`

- **Given** EVSE has EnableC2 set to SOLAR_OFF and Mode is MODE_SOLAR
- **When** evse_force_single_phase is called
- **Then** Returns 1 because SOLAR_OFF disables contactor 2 in solar mode

> Test: `test_force_single_phase_solar_off_in_solar_mode` in `test_operating_modes.c:287`

### EnableC2=SOLAR_OFF does not force single phase in Smart mode

**Requirement:** `REQ-MODE-015`

- **Given** EVSE has EnableC2 set to SOLAR_OFF and Mode is MODE_SMART
- **When** evse_force_single_phase is called
- **Then** Returns 0 because SOLAR_OFF only applies in Solar mode, not Smart mode

> Test: `test_force_single_phase_solar_off_in_smart_mode` in `test_operating_modes.c:302`

### EnableC2=AUTO forces single phase when charging on 1 phase

**Requirement:** `REQ-MODE-016`

- **Given** EVSE has EnableC2 set to AUTO and Nr_Of_Phases_Charging=1
- **When** evse_force_single_phase is called
- **Then** Returns 1 because AUTO mode follows the current phase count

> Test: `test_force_single_phase_auto_c2_1p` in `test_operating_modes.c:317`

### EnableC2=AUTO does not force single phase when charging on 3 phases

**Requirement:** `REQ-MODE-017`

- **Given** EVSE has EnableC2 set to AUTO and Nr_Of_Phases_Charging=3
- **When** evse_force_single_phase is called
- **Then** Returns 0 because AUTO mode allows 3-phase operation when already on 3 phases

> Test: `test_force_single_phase_auto_c2_3p` in `test_operating_modes.c:332`

### EnableC2=ALWAYS_ON does not force single phase

**Requirement:** `REQ-MODE-018`

- **Given** EVSE has EnableC2 set to ALWAYS_ON (contactor 2 always enabled for 3-phase)
- **When** evse_force_single_phase is called
- **Then** Returns 0 because the EVSE is configured to always operate in three phase

> Test: `test_force_single_phase_always_on` in `test_operating_modes.c:347`

### STATE_C entry with single phase disables contactor 2

**Requirement:** `REQ-MODE-019`

- **Given** EVSE has EnableC2 set to ALWAYS_OFF (force single phase)
- **When** EVSE transitions to STATE_C
- **Then** Contactor 1 is on, contactor 2 is off, and Nr_Of_Phases_Charging is 1

> Test: `test_state_C_contactor2_off_when_single_phase` in `test_operating_modes.c:361`

### STATE_C entry with three phase enables both contactors

**Requirement:** `REQ-MODE-020`

- **Given** EVSE has EnableC2 set to NOT_PRESENT (default 3-phase behavior)
- **When** EVSE transitions to STATE_C
- **Then** Both contactor 1 and contactor 2 are on and Nr_Of_Phases_Charging is 3

> Test: `test_state_C_contactor2_on_when_three_phase` in `test_operating_modes.c:378`

### Phase switch from 3P to 1P completes on STATE_C entry

**Requirement:** `REQ-MODE-021`

- **Given** EVSE has Switching_Phases_C2=GOING_TO_SWITCH_1P and EnableC2=AUTO
- **When** EVSE transitions to STATE_C
- **Then** Nr_Of_Phases_Charging is set to 1 and Switching_Phases_C2 is cleared to NO_SWITCH

> Test: `test_phase_switch_going_to_1p` in `test_operating_modes.c:395`

---

## P1 Meter Parsing

### JSON number extractor finds integer value

**Requirement:** `REQ-MTR-070`

- **Given** JSON string containing "active_current_l1_a":12
- **When** p1_json_find_number is called with key "active_current_l1_a"
- **Then** Returns 1 and value is 12.0

> Test: `test_json_find_integer` in `test_p1_parse.c:1`

### JSON number extractor finds negative decimal value

**Requirement:** `REQ-MTR-071`

- **Given** JSON string containing "active_power_l1_w":-2725.5
- **When** p1_json_find_number is called with key "active_power_l1_w"
- **Then** Returns 1 and value is approximately -2725.5

> Test: `test_json_find_negative_decimal` in `test_p1_parse.c:38`

### JSON number extractor returns 0 for missing key

**Requirement:** `REQ-MTR-072`

- **Given** JSON string without the requested key
- **When** p1_json_find_number is called with key "nonexistent_key"
- **Then** Returns 0

> Test: `test_json_find_missing_key` in `test_p1_parse.c:55`

### JSON extractor handles key that is a prefix of another key

**Requirement:** `REQ-MTR-073`

- **Given** JSON with "active_current_l1_a" and search for "active_current_l1"
- **When** p1_json_find_number is called
- **Then** Returns 0 because the key must match exactly (followed by closing quote)

> Test: `test_json_find_partial_key_no_match` in `test_p1_parse.c:71`

### JSON extractor handles whitespace around colon

**Requirement:** `REQ-MTR-074`

- **Given** JSON with spaces around the colon: "key" : 42
- **When** p1_json_find_number is called
- **Then** Returns 1 and value is 42

> Test: `test_json_find_whitespace_around_colon` in `test_p1_parse.c:87`

### JSON extractor NULL safety

**Requirement:** `REQ-MTR-075`

- **Given** NULL json, key, or out pointers
- **When** p1_json_find_number is called
- **Then** Returns 0 without crashing

> Test: `test_json_find_null_safety` in `test_p1_parse.c:104`

### 3-phase P1 response with positive currents and positive power

**Requirement:** `REQ-MTR-076`

- **Given** JSON with L1=10.5A/2400W, L2=8.3A/1900W, L3=12.1A/2800W
- **When** p1_parse_response is called
- **Then** phases=3, currents=[105, 83, 121] deci-amps (all positive)

> Test: `test_parse_3phase_positive` in `test_p1_parse.c:122`

### 1-phase P1 response (only L1 present)

**Requirement:** `REQ-MTR-077`

- **Given** JSON with only L1 current and power fields
- **When** p1_parse_response is called
- **Then** phases=1, current_da[0]=114 (11.43A * 10, rounded)

> Test: `test_parse_1phase` in `test_p1_parse.c:147`

### Negative power causes negative current (solar feed-in)

**Requirement:** `REQ-MTR-078`

- **Given** JSON with L1=-11.43A current and -2725W power (feeding in)
- **When** p1_parse_response is called
- **Then** current_da[0] = -114 (negative because power is negative)

> Test: `test_parse_feedin_negative_power` in `test_p1_parse.c:166`

### Mixed phases: L1 consuming, L2 feeding in

**Requirement:** `REQ-MTR-079`

- **Given** JSON with L1=5A/1150W (consuming) and L2=3A/-690W (feeding in)
- **When** p1_parse_response is called
- **Then** current_da[0]=50, current_da[1]=-30

> Test: `test_parse_mixed_direction` in `test_p1_parse.c:185`

### Missing all current keys returns invalid

**Requirement:** `REQ-MTR-080`

- **Given** JSON with only power fields, no current fields
- **When** p1_parse_response is called
- **Then** valid=0, phases=0

> Test: `test_parse_no_current_keys` in `test_p1_parse.c:207`

### Missing power keys defaults to positive current

**Requirement:** `REQ-MTR-081`

- **Given** JSON with current fields but no power fields
- **When** p1_parse_response is called
- **Then** currents are positive (power defaults to 0, which is >= 0)

> Test: `test_parse_missing_power_defaults_positive` in `test_p1_parse.c:225`

### NULL JSON returns invalid result

**Requirement:** `REQ-MTR-082`

- **Given** NULL json pointer
- **When** p1_parse_response is called
- **Then** valid=0, phases=0

> Test: `test_parse_null_json` in `test_p1_parse.c:247`

### Empty JSON string returns invalid result

**Requirement:** `REQ-MTR-083`

- **Given** Empty JSON string "{}"
- **When** p1_parse_response is called
- **Then** valid=0, phases=0

> Test: `test_parse_empty_json` in `test_p1_parse.c:261`

### Real-world Kaifa single-phase P1 response

**Requirement:** `REQ-MTR-084`

- **Given** Actual P1 meter JSON response from a Kaifa meter with solar feed-in
- **When** p1_parse_response is called
- **Then** Correctly extracts L1 current as -114 dA (11.43A feed-in)

> Test: `test_parse_real_world_kaifa` in `test_p1_parse.c:276`

### Zero current and zero power

**Requirement:** `REQ-MTR-085`

- **Given** JSON with all currents and powers at 0
- **When** p1_parse_response is called
- **Then** All current_da values are 0

> Test: `test_parse_zero_values` in `test_p1_parse.c:306`

### Power stores diagnostic values

**Requirement:** `REQ-MTR-086`

- **Given** JSON with L1=8.5A/1955W, L2=6.2A/1426W
- **When** p1_parse_response is called
- **Then** power_w contains [1955, 1426, 0] for diagnostics

> Test: `test_parse_power_diagnostics` in `test_p1_parse.c:331`

---

## Phase Switching

### AUTO + SOLAR: no switch needed when already at correct phase count

**Requirement:** `REQ-PHASE-003`

- **Given** The EVSE is in STATE_B with EnableC2=AUTO, MODE_SOLAR, and various phase counts
- **When** evse_check_switching_phases is called
- **Then** Switching_Phases_C2 is NO_SWITCH when already at the correct phase count

> Test: `test_check_auto_solar_forces_1p` in `test_phase_switching.c:1`

### AUTO + SOLAR already on 1 phase results in NO_SWITCH

**Requirement:** `REQ-PHASE-004`

- **Given** The EVSE is in STATE_B with EnableC2=AUTO, MODE_SOLAR, and 1 phase
- **When** evse_check_switching_phases is called
- **Then** Switching_Phases_C2 is NO_SWITCH (already single phase)

> Test: `test_check_auto_solar_already_1p` in `test_phase_switching.c:52`

### AUTO + SMART forces 3-phase when currently on 1 phase

**Requirement:** `REQ-PHASE-005`

- **Given** The EVSE is in STATE_B with EnableC2=AUTO, MODE_SMART, and 1 phase
- **When** evse_check_switching_phases is called
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_3P

> Test: `test_check_auto_smart_forces_3p` in `test_phase_switching.c:73`

### AUTO + SMART already on 3 phases results in NO_SWITCH

**Requirement:** `REQ-PHASE-006`

- **Given** The EVSE is in STATE_B with EnableC2=AUTO, MODE_SMART, and 3 phases
- **When** evse_check_switching_phases is called
- **Then** Switching_Phases_C2 is NO_SWITCH (already three phase)

> Test: `test_check_auto_smart_already_3p` in `test_phase_switching.c:94`

### ALWAYS_OFF in STATE_A sets Nr_Of_Phases_Charging directly to 1

**Requirement:** `REQ-PHASE-007`

- **Given** The EVSE is in STATE_A with EnableC2=ALWAYS_OFF and 3 phases configured
- **When** evse_check_switching_phases is called
- **Then** Nr_Of_Phases_Charging is set directly to 1 (no deferred switch needed)

> Test: `test_check_always_off_in_state_a` in `test_phase_switching.c:115`

### ALWAYS_OFF in STATE_B sets deferred switching flag to 1P

**Requirement:** `REQ-PHASE-008`

- **Given** The EVSE is in STATE_B with EnableC2=ALWAYS_OFF and 3 phases configured
- **When** evse_check_switching_phases is called
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_1P (deferred until STATE_C entry)

> Test: `test_check_always_off_in_state_b` in `test_phase_switching.c:136`

### SOLAR_OFF + SMART forces 3-phase charging

**Requirement:** `REQ-PHASE-009`

- **Given** The EVSE is in STATE_B with EnableC2=SOLAR_OFF, MODE_SMART, and 1 phase
- **When** evse_check_switching_phases is called
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_3P

> Test: `test_check_solar_off_smart_3p` in `test_phase_switching.c:156`

### SOLAR_OFF + SOLAR forces 1-phase charging

**Requirement:** `REQ-PHASE-010`

- **Given** The EVSE is in STATE_B with EnableC2=SOLAR_OFF, MODE_SOLAR, and 3 phases
- **When** evse_check_switching_phases is called
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_1P

> Test: `test_check_solar_off_solar_1p` in `test_phase_switching.c:178`

### STATE_C entry applies deferred 1P switch and opens contactor 2

**Requirement:** `REQ-PHASE-011`

- **Given** Switching_Phases_C2 is GOING_TO_SWITCH_1P with EnableC2=ALWAYS_OFF
- **When** The state is set to STATE_C
- **Then** Nr_Of_Phases_Charging is 1 and contactor2 is off (open)

> Test: `test_state_c_applies_1p_switch` in `test_phase_switching.c:199`

### STATE_C entry applies deferred 3P switch and closes contactor 2

**Requirement:** `REQ-PHASE-012`

- **Given** Switching_Phases_C2 is GOING_TO_SWITCH_3P with EnableC2=ALWAYS_ON
- **When** The state is set to STATE_C
- **Then** Nr_Of_Phases_Charging is 3 and contactor2 is on (closed)

> Test: `test_state_c_applies_3p_switch` in `test_phase_switching.c:220`

### STATE_C entry resets Switching_Phases_C2 to NO_SWITCH

**Requirement:** `REQ-PHASE-013`

- **Given** Switching_Phases_C2 is GOING_TO_SWITCH_1P
- **When** The state is set to STATE_C
- **Then** Switching_Phases_C2 is reset to NO_SWITCH

> Test: `test_state_c_resets_switching` in `test_phase_switching.c:241`

### Full 3P to 1P to 3P phase switching cycle in solar mode

**Requirement:** `REQ-PHASE-014`

- **Given** The EVSE is solar charging on 3 phases with EnableC2=AUTO
- **When** Solar shortage triggers 3P->1P switch, then surplus triggers 1P->3P switch
- **Then** The EVSE correctly switches from 3P to 1P and back to 3P with proper contactor and flag states

> Test: `test_full_3p_1p_3p_cycle` in `test_phase_switching.c:259`

### Severe solar shortage uses short PhaseSwitchTimer

**Requirement:** `REQ-PH-015`

- **Given** The EVSE is solar charging on 3P with severe shortage (IsumImport >= MinCurrent*10)
- **When** evse_calc_balanced_current is called
- **Then** PhaseSwitchTimer is set to PhaseSwitchSevereTime (30s default)

> Test: `test_severe_shortage_uses_short_timer` in `test_phase_switching.c:357`

### Mild solar shortage uses long PhaseSwitchTimer (StopTime-based)

**Requirement:** `REQ-PH-016`

- **Given** The EVSE is solar charging on 3P with mild shortage (0 < IsumImport < MinCurrent*10)
- **When** evse_calc_balanced_current is called
- **Then** PhaseSwitchTimer is set to StopTime*60 (600s default)

> Test: `test_mild_shortage_uses_long_timer` in `test_phase_switching.c:379`

### PhaseSwitchTimer reaching <=2 triggers 3P to 1P switch

**Requirement:** `REQ-PH-017`

- **Given** The EVSE is solar charging on 3P with PhaseSwitchTimer=2 and ongoing shortage
- **When** evse_calc_balanced_current is called
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_1P

> Test: `test_phase_switch_timer_triggers_1p` in `test_phase_switching.c:399`

### Switching from 3P to 1P starts the hold-down counter

**Requirement:** `REQ-PH-018`

- **Given** The EVSE is solar charging on 3P with PhaseSwitchTimer about to trigger
- **When** The 3P→1P switch is triggered (PhaseSwitchTimer<=2)
- **Then** PhaseSwitchHoldDown is set to PhaseSwitchHoldDownTime

> Test: `test_3p_to_1p_starts_holddown` in `test_phase_switching.c:418`

### Hold-down counter prevents premature 1P to 3P upgrade

**Requirement:** `REQ-PH-019`

- **Given** The EVSE is solar charging on 1P with sufficient surplus but PhaseSwitchHoldDown > 0
- **When** evse_calc_balanced_current is called
- **Then** PhaseSwitchTimer stays 0 and Switching_Phases_C2 stays NO_SWITCH (upgrade blocked)

> Test: `test_holddown_prevents_3p_upgrade` in `test_phase_switching.c:439`

### Hold-down expired allows 1P to 3P upgrade to proceed

**Requirement:** `REQ-PH-020`

- **Given** The EVSE is solar charging on 1P with sufficient surplus and PhaseSwitchHoldDown=0
- **When** evse_calc_balanced_current is called
- **Then** PhaseSwitchTimer starts countdown for 3P upgrade

> Test: `test_holddown_expired_allows_upgrade` in `test_phase_switching.c:464`

### PhaseSwitchTimer is independent of SolarStopTimer

**Requirement:** `REQ-PH-021`

- **Given** PhaseSwitchTimer and SolarStopTimer are at different values
- **When** evse_calc_balanced_current triggers a phase switch timer
- **Then** Only PhaseSwitchTimer changes, SolarStopTimer is unaffected

> Test: `test_phase_timer_independent_of_solar_stop` in `test_phase_switching.c:488`

### PhaseSwitchTimer counts down each second in tick_1s

**Requirement:** `REQ-PH-022`

- **Given** PhaseSwitchTimer=10 and PhaseSwitchHoldDown=5
- **When** evse_tick_1s is called
- **Then** PhaseSwitchTimer decrements to 9 and PhaseSwitchHoldDown decrements to 4

> Test: `test_phase_timer_countdown_in_tick_1s` in `test_phase_switching.c:509`

### Phase switching timer fields initialized correctly by evse_init

**Requirement:** `REQ-PH-023`

- **Given** A freshly initialized EVSE context
- **When** evse_init is called
- **Then** PhaseSwitchHoldDownTime and PhaseSwitchSevereTime have correct defaults

> Test: `test_phase_timer_defaults` in `test_phase_switching.c:529`

### Phase switch completion resets IntTimer for startup protection

**Requirement:** `REQ-PH-024`

- **Given** The EVSE was charging on 3P with IntTimer=500 and switches to 1P
- **When** STATE_C is entered with Switching_Phases_C2 = GOING_TO_SWITCH_1P
- **Then** Node[0].IntTimer is reset to 0 (new startup period begins)

> Test: `test_phase_switch_resets_inttimer` in `test_phase_switching.c:550`

### 3P upgrade also resets IntTimer

**Requirement:** `REQ-PH-025`

- **Given** The EVSE was charging on 1P with IntTimer=300 and switches to 3P
- **When** STATE_C is entered with Switching_Phases_C2 = GOING_TO_SWITCH_3P
- **Then** Node[0].IntTimer is reset to 0

> Test: `test_3p_upgrade_resets_inttimer` in `test_phase_switching.c:571`

### Normal STATE_C entry (no phase switch) does not reset IntTimer

**Requirement:** `REQ-PH-026`

- **Given** The EVSE enters STATE_C without a phase switch (Switching_Phases_C2 = NO_SWITCH)
- **When** evse_set_state is called with STATE_C
- **Then** Node[0].IntTimer is NOT reset (keeps previous value)

> Test: `test_no_switch_preserves_inttimer` in `test_phase_switching.c:592`

### SolarStopTimer suppressed during startup period after phase switch

**Requirement:** `REQ-PH-027`

- **Given** The EVSE just completed a phase switch (IntTimer=5, < SOLARSTARTTIME)
- **When** evse_calc_balanced_current detects a shortage in solar mode
- **Then** SolarStopTimer is NOT started (suppressed during startup settling)

> Test: `test_solar_stop_suppressed_during_startup` in `test_phase_switching.c:612`

### SolarStopTimer allowed after startup period

**Requirement:** `REQ-PH-028`

- **Given** The EVSE is past startup (IntTimer > SOLARSTARTTIME) with shortage
- **When** evse_calc_balanced_current detects a shortage in solar mode
- **Then** SolarStopTimer IS started (startup protection expired)

> Test: `test_solar_stop_allowed_after_startup` in `test_phase_switching.c:632`

---

## Power Availability

### Normal mode always reports current as available regardless of mains load

**Requirement:** `REQ-PWR-001`

- **Given** EVSE is standalone in Normal mode with very high MainsMeterImeasured=999
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because Normal mode does not check mains

> Test: `test_normal_mode_always_available` in `test_power_availability.c:1`

### Normal mode available even with high mains load

**Requirement:** `REQ-PWR-002`

- **Given** EVSE is standalone in Normal mode with MainsMeterImeasured=400
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because Normal mode ignores mains measurements

> Test: `test_normal_mode_available_with_high_load` in `test_power_availability.c:45`

### Smart mode allows current when mains load plus MinCurrent is under MaxMains

**Requirement:** `REQ-PWR-003`

- **Given** EVSE is standalone in Smart mode with MaxMains=25A and MainsMeterImeasured=100 (10A)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because baseload (10A) + MinCurrent (6A) = 16A < MaxMains (25A)

> Test: `test_smart_maxmains_allows_under_limit` in `test_power_availability.c:62`

### Smart mode blocks current when mains load plus MinCurrent exceeds MaxMains

**Requirement:** `REQ-PWR-004`

- **Given** EVSE is standalone in Smart mode with MaxMains=10A and MainsMeterImeasured=200 (20A baseload)
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because baseload (20A) + MinCurrent (6A) = 26A > MaxMains (10A)

> Test: `test_smart_maxmains_blocks_over_limit` in `test_power_availability.c:80`

### Smart mode allows current when circuit load is under MaxCircuit limit

**Requirement:** `REQ-PWR-005`

- **Given** EVSE is master (LoadBl=1) in Smart mode with MaxCircuit=20A and EVMeterImeasured=50
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because circuit load (5A) + MinCurrent (6A) is under MaxCircuit

> Test: `test_smart_maxcircuit_allows_under_limit` in `test_power_availability.c:101`

### Smart mode blocks current when circuit load exceeds MaxCircuit limit

**Requirement:** `REQ-PWR-006`

- **Given** EVSE is master (LoadBl=1) in Smart mode with MaxCircuit=8A and EVMeterImeasured=100 (10A)
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because circuit load (10A) already exceeds MaxCircuit (8A)

> Test: `test_smart_maxcircuit_blocks_over_limit` in `test_power_availability.c:121`

### MaxSumMains allows current when sum of phase currents is under limit

**Requirement:** `REQ-PWR-007`

- **Given** EVSE is in Smart mode with MaxSumMains=50 and Isum=100 (10A total)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because Isum plus MinCurrent is under MaxSumMains limit

> Test: `test_maxsummains_allows_under_limit` in `test_power_availability.c:143`

### MaxSumMains blocks current when sum of phase currents exceeds limit

**Requirement:** `REQ-PWR-008`

- **Given** EVSE is in Smart mode with MaxSumMains=10 and Isum=200 (way over limit)
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because total phase current sum exceeds MaxSumMains

> Test: `test_maxsummains_blocks_over_limit` in `test_power_availability.c:162`

### MaxSumMains=0 disables the sum-of-mains check entirely

**Requirement:** `REQ-PWR-009`

- **Given** EVSE is in Smart mode with MaxSumMains=0 (disabled) and Isum=9999 (extremely high)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because MaxSumMains=0 means the check is skipped

> Test: `test_maxsummains_zero_disables_check` in `test_power_availability.c:182`

### Solar mode blocks current when no surplus is available

**Requirement:** `REQ-PWR-010`

- **Given** EVSE is in Solar mode with StartCurrent=6A and Isum=0 (no export)
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because there is no solar surplus for charging

> Test: `test_solar_no_surplus_blocks` in `test_power_availability.c:203`

### Solar mode allows current when surplus exceeds StartCurrent

**Requirement:** `REQ-PWR-011`

- **Given** EVSE is in Solar mode with StartCurrent=6A and Isum=-80 (8A export surplus)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because 8A surplus exceeds 6A StartCurrent threshold

> Test: `test_solar_surplus_allows` in `test_power_availability.c:220`

### Solar mode blocks current when surplus is below StartCurrent threshold

**Requirement:** `REQ-PWR-012`

- **Given** EVSE is in Solar mode with StartCurrent=10A and Isum=-80 (only 8A surplus)
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because 8A surplus is below the 10A StartCurrent threshold

> Test: `test_solar_insufficient_surplus_blocks` in `test_power_availability.c:237`

### Solar mode with active EVSE checks fair share before allowing more

**Requirement:** `REQ-PWR-013`

- **Given** EVSE is in Solar mode with one active EVSE at MinCurrent and Isum=10 (1A import)
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because grid import indicates insufficient surplus for another EVSE

> Test: `test_solar_with_active_evse_checks_fair_share` in `test_power_availability.c:254`

### OCPP limit below MinCurrent blocks power availability

**Requirement:** `REQ-PWR-014`

- **Given** EVSE is standalone with OcppMode enabled and OcppCurrentLimit=4.0A (below MinCurrent=6A)
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because OCPP limit is below the minimum viable charge current

> Test: `test_ocpp_limit_blocks_when_below_min` in `test_power_availability.c:280`

### OCPP limit above MinCurrent allows power availability

**Requirement:** `REQ-PWR-015`

- **Given** EVSE is standalone with OcppMode enabled and OcppCurrentLimit=10.0A (above MinCurrent=6A)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because OCPP limit is above the minimum viable charge current

> Test: `test_ocpp_limit_allows_when_above_min` in `test_power_availability.c:297`

### OCPP negative limit (no limit set) allows power availability

**Requirement:** `REQ-PWR-016`

- **Given** EVSE is standalone with OcppMode enabled and OcppCurrentLimit=-1.0A (no limit)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because negative OCPP limit means no restriction

> Test: `test_ocpp_no_limit_allows` in `test_power_availability.c:314`

### OCPP availability check is skipped for non-standalone configurations

**Requirement:** `REQ-PWR-017`

- **Given** EVSE is master (LoadBl=1) with OcppMode enabled and OcppCurrentLimit=3.0A (below MinCurrent)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because OCPP check requires LoadBl=0 (standalone)

> Test: `test_ocpp_check_only_for_standalone` in `test_power_availability.c:330`

### PWM duty cycle conversion for 6A (minimum charge current)

**Requirement:** `REQ-PWR-018`

- **Given** A charge current of 60 deciamps (6A)
- **When** evse_current_to_duty is called
- **Then** Returns 102 as the PWM duty cycle value (60/0.6 * 1024/1000)

> Test: `test_current_to_duty_6A` in `test_power_availability.c:350`

### PWM duty cycle conversion for 16A (common residential limit)

**Requirement:** `REQ-PWR-019`

- **Given** A charge current of 160 deciamps (16A)
- **When** evse_current_to_duty is called
- **Then** Returns a duty cycle value between 100 and 600 (low-range formula)

> Test: `test_current_to_duty_16A` in `test_power_availability.c:364`

### PWM duty cycle conversion for 51A (upper boundary of low-range formula)

**Requirement:** `REQ-PWR-020`

- **Given** A charge current of 510 deciamps (51A)
- **When** evse_current_to_duty is called
- **Then** Returns a duty cycle value between 800 and 1000 (near top of low-range)

> Test: `test_current_to_duty_51A` in `test_power_availability.c:378`

### PWM duty cycle conversion for 60A (high-range formula)

**Requirement:** `REQ-PWR-021`

- **Given** A charge current of 600 deciamps (60A)
- **When** evse_current_to_duty is called
- **Then** Returns a duty cycle value between 850 and 1000 (high-range formula: current/2.5 + 640)

> Test: `test_current_to_duty_high_range` in `test_power_availability.c:392`

### PWM duty cycle conversion for 80A (near maximum charge current)

**Requirement:** `REQ-PWR-022`

- **Given** A charge current of 800 deciamps (80A)
- **When** evse_current_to_duty is called
- **Then** Returns a duty cycle value between 950 and 1024 (near the top of the PWM range)

> Test: `test_current_to_duty_80A` in `test_power_availability.c:406`

---

## Priority-Based Power Scheduling

### MODBUS_ADDR strategy produces ascending address order

**Requirement:** `REQ-LB-100`

- **Given** Master (LoadBl=1) with 4 EVSEs in STATE_C
- **When** evse_sort_priority() is called
- **Then** Priority[] = {0, 1, 2, 3}

> Test: `test_sort_modbus_addr` in `test_scheduling.c:1`

### FIRST_CONNECTED strategy orders by earliest connection time

**Requirement:** `REQ-LB-101`

- **Given** Master with 3 EVSEs in STATE_C
- **When** evse_sort_priority() is called
- **Then** Priority[] = {1, 2, 0, ...} (EVSE[1] first, EVSE[0] last)

> Test: `test_sort_first_connected` in `test_scheduling.c:70`

### LAST_CONNECTED strategy orders by most recent connection time

**Requirement:** `REQ-LB-102`

- **Given** Master with 3 EVSEs in STATE_C
- **When** evse_sort_priority() is called
- **Then** Priority[] = {0, 2, 1, ...} (EVSE[0] first, EVSE[1] last)

> Test: `test_sort_last_connected` in `test_scheduling.c:94`

### Disconnected EVSEs are sorted to end regardless of strategy

**Requirement:** `REQ-LB-103`

- **Given** Master with 4 EVSEs: [0]=STATE_C, [1]=STATE_A, [2]=STATE_C, [3]=STATE_A
- **When** evse_sort_priority() is called
- **Then** Priority[] = {0, 2, 1, 3} (active EVSEs first, then disconnected)

> Test: `test_sort_disconnected_to_end` in `test_scheduling.c:118`

### Insufficient power for 3 EVSEs: first 2 in priority get MinCurrent

**Requirement:** `REQ-LB-110`

- **Given** Master with 3 EVSEs in STATE_C, MinCurrent=6A, MaxCircuit=20A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** Balanced[0] >= 60 and Balanced[1] >= 60 and Balanced[2] == 0

> Test: `test_shortage_first_two_get_current` in `test_scheduling.c:146`

### Power for only 1 EVSE: highest priority gets it all

**Requirement:** `REQ-LB-111`

- **Given** Master with 3 EVSEs in STATE_C, MinCurrent=6A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** Balanced[0] == 80 and Balanced[1] == 0 and Balanced[2] == 0

> Test: `test_shortage_one_evse_gets_all` in `test_scheduling.c:177`

### Sufficient power: all EVSEs get current, no scheduling needed

**Requirement:** `REQ-LB-112`

- **Given** Master with 3 EVSEs in STATE_C, MinCurrent=6A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** All EVSEs get current, no LESS_6A errors, NoCurrent == 0

> Test: `test_sufficient_power_no_scheduling` in `test_scheduling.c:204`

### Surplus above MinCurrent distributed fairly among active EVSEs

**Requirement:** `REQ-LB-113`

- **Given** Master with 2 EVSEs in STATE_C, MinCurrent=6A, BalancedMax={320,320}
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** Balanced[0] == 100 and Balanced[1] == 100 (10A each = 6A + 4A surplus)

> Test: `test_surplus_distributed_fairly` in `test_scheduling.c:232`

### Standalone mode (LoadBl=0) does not use priority scheduling

**Requirement:** `REQ-LB-114`

- **Given** Single EVSE (LoadBl=0) in STATE_C, MinCurrent=6A, Mode=SMART
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** NoCurrent increments (original behavior preserved), no ScheduleState changes

> Test: `test_standalone_no_scheduling` in `test_scheduling.c:263`

### Solar mode: paused EVSEs get NO_SUN error instead of LESS_6A

**Requirement:** `REQ-LB-115`

- **Given** Master with 2 EVSEs in STATE_C, Mode=MODE_SOLAR
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** Balanced[1] == 0 and BalancedError[1] has NO_SUN set

> Test: `test_solar_paused_gets_no_sun` in `test_scheduling.c:300`

### Capped EVSE surplus redistributed to uncapped ones

**Requirement:** `REQ-LB-116`

- **Given** Master with 3 EVSEs in STATE_C, MinCurrent=6A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** Balanced[1] == 80 (capped) and Balanced[0] + Balanced[2] == 160

> Test: `test_capped_surplus_redistribution` in `test_scheduling.c:337`

### Power exactly equals MinCurrent for 1 EVSE

**Requirement:** `REQ-LB-117`

- **Given** Master with 3 EVSEs in STATE_C, MinCurrent=6A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** Exactly 1 EVSE has Balanced >= 60, exactly 2 have Balanced == 0

> Test: `test_exactly_one_mincurrent` in `test_scheduling.c:363`

### Zero available power pauses all EVSEs

**Requirement:** `REQ-LB-118`

- **Given** Master with 3 EVSEs in STATE_C, MinCurrent=6A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** All Balanced[] == 0, all paused, NoCurrent increments

> Test: `test_zero_power_pauses_all` in `test_scheduling.c:394`

### NoCurrent does NOT increment when priority scheduling pauses some EVSEs

**Requirement:** `REQ-LB-119`

- **Given** Master with 3 EVSEs in STATE_C, MinCurrent=6A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** NoCurrent == 0, EVSE[0] is charging

> Test: `test_no_current_not_incremented_on_deliberate_pause` in `test_scheduling.c:422`

### EVSE drawing <1A when IdleTimer expires gets paused

**Requirement:** `REQ-LB-120`

- **Given** Master with 2 EVSEs in STATE_C, EVSE[0] active, EVSE[1] paused
- **When** evse_schedule_tick_1s() is called
- **Then** EVSE[0] paused, EVSE[1] activated with IdleTimer[1] = 0

> Test: `test_idle_evse_paused_at_timeout` in `test_scheduling.c:450`

### EVSE not paused before IdleTimeout expires (anti-flap)

**Requirement:** `REQ-LB-121`

- **Given** EVSE[0] active with IdleTimer[0] = 30, IdleTimeout = 60
- **When** evse_schedule_tick_1s() is called
- **Then** EVSE[0] remains active

> Test: `test_antiflap_not_paused_early` in `test_scheduling.c:479`

### EVSE drawing power when IdleTimer expires stays active

**Requirement:** `REQ-LB-122`

- **Given** EVSE[0] active with IdleTimer[0] = 59, IdleTimeout = 60
- **When** evse_schedule_tick_1s() is called
- **Then** EVSE[0] stays active, RotationTimer starts if RotationInterval > 0

> Test: `test_charging_evse_stays_active` in `test_scheduling.c:505`

### Full idle cycle: all EVSEs tried, recircle to first

**Requirement:** `REQ-LB-123`

- **Given** 3 EVSEs in STATE_C, EVSE[2] active (last tried), EVSE[0] and [1] paused
- **When** evse_schedule_tick_1s() is called
- **Then** EVSE[2] paused, EVSE[0] reactivated (wraps around)

> Test: `test_idle_cycle_wraps_around` in `test_scheduling.c:534`

### RotationTimer expiry pauses current EVSE and activates next

**Requirement:** `REQ-LB-140`

- **Given** 3 EVSEs in STATE_C, EVSE[0] active, RotationInterval=30
- **When** evse_schedule_tick_1s() is called
- **Then** EVSE[0] paused, EVSE[1] activated, RotationTimer reset to 1800

> Test: `test_rotation_timer_expires` in `test_scheduling.c:567`

### RotationInterval=0 disables rotation entirely

**Requirement:** `REQ-LB-141`

- **Given** 2 EVSEs, EVSE[0] active, RotationInterval = 0
- **When** Checking ScheduleState
- **Then** EVSE[0] still active (never rotated)

> Test: `test_rotation_disabled` in `test_scheduling.c:598`

### Rotation wraps from last priority to first

**Requirement:** `REQ-LB-142`

- **Given** 3 EVSEs in priority order {0,1,2}, EVSE[2] active
- **When** evse_schedule_tick_1s() is called
- **Then** EVSE[2] paused, EVSE[0] activated

> Test: `test_rotation_wraps_to_first` in `test_scheduling.c:628`

### Rotation skips disconnected EVSEs (STATE_A)

**Requirement:** `REQ-LB-143`

- **Given** 3 EVSEs: [0] active, [1] STATE_A disconnected, [2] paused
- **When** evse_schedule_tick_1s() is called
- **Then** EVSE[0] paused, EVSE[2] activated (EVSE[1] skipped)

> Test: `test_rotation_skips_disconnected` in `test_scheduling.c:657`

### Newly activated EVSE gets idle check before rotation timer applies

**Requirement:** `REQ-LB-144`

- **Given** EVSE[1] just activated via rotation, IdleTimeout=60, RotationInterval=30
- **When** 60 seconds pass
- **Then** EVSE[1] paused due to idle (not waiting for rotation)

> Test: `test_idle_check_before_rotation` in `test_scheduling.c:687`

### Power increases: paused EVSE reactivated immediately

**Requirement:** `REQ-LB-150`

- **Given** 3 EVSEs, EVSE[0] active, EVSE[1] and [2] paused
- **When** evse_calc_balanced_current(ctx, 0) is called with new power
- **Then** EVSE[1] reactivated, IdleTimer reset

> Test: `test_power_increase_reactivates` in `test_scheduling.c:723`

### Reactivation follows priority order

**Requirement:** `REQ-LB-151`

- **Given** 3 EVSEs paused, PrioStrategy=PRIO_MODBUS_ADDR
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** EVSE[0] and EVSE[1] activated (not [0] and [2])

> Test: `test_reactivation_follows_priority` in `test_scheduling.c:758`

### Original bug: 2 EVSEs, power drops, only 1 stops (no oscillation)

**Requirement:** `REQ-LB-160`

- **Given** Master with 2 EVSEs in STATE_C, MinCurrent=6A, MaxCircuit=11A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** Exactly 1 EVSE continues, 1 paused, NoCurrent == 0

> Test: `test_regression_no_oscillation` in `test_scheduling.c:790`

### 6 EVSEs, power for 5: lowest priority paused

**Requirement:** `REQ-LB-161`

- **Given** Master with 6 EVSEs in STATE_C, MinCurrent=6A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** EVSEs [0]-[4] receive current, EVSE[5] paused

> Test: `test_six_evse_lowest_paused` in `test_scheduling.c:820`

### Node goes offline: removed from scheduling

**Requirement:** `REQ-LB-162`

- **Given** 3 EVSEs, EVSE[1] goes offline (STATE_A)
- **When** evse_schedule_tick_1s() runs
- **Then** EVSE[1] gets ScheduleState = SCHED_INACTIVE

> Test: `test_offline_node_removed` in `test_scheduling.c:845`

### New EVSE join during shortage doesn't displace active ones

**Requirement:** `REQ-LB-163`

- **Given** 2 EVSEs charging, power for 2 at MinCurrent
- **When** evse_calc_balanced_current(ctx, 1) is called
- **Then** EVSE[0] and EVSE[1] keep allocation, EVSE[2] gets 0

> Test: `test_new_evse_doesnt_displace` in `test_scheduling.c:872`

---

## Serial Message Parsing

### Valid three-phase Irms message with address 011

**Requirement:** `REQ-SERIAL-001`

- **Given** A serial buffer containing "Irms:011,312,123,124"
- **When** serial_parse_irms is called
- **Then** Address is 11 and three phase currents are parsed correctly

> Test: `test_irms_valid_three_phase` in `test_serial_parser.c:1`

### Irms message with negative current values

**Requirement:** `REQ-SERIAL-001`

- **Given** A serial buffer with negative Irms values (solar injection)
- **When** serial_parse_irms is called
- **Then** Negative values are parsed correctly

> Test: `test_irms_negative_values` in `test_serial_parser.c:40`

### Irms message with zero values

**Requirement:** `REQ-SERIAL-001`

- **Given** A serial buffer with all zero Irms values
- **When** serial_parse_irms is called
- **Then** All values parsed as zero

> Test: `test_irms_zero_values` in `test_serial_parser.c:57`

### Irms message embedded in larger buffer with extra text

**Requirement:** `REQ-SERIAL-001`

- **Given** A serial buffer with text before and after the Irms token
- **When** serial_parse_irms is called
- **Then** The Irms message is found and parsed correctly

> Test: `test_irms_embedded_in_buffer` in `test_serial_parser.c:74`

### Valid power measurement with address 010

**Requirement:** `REQ-SERIAL-002`

- **Given** A serial buffer containing "PowerMeasured:010,500"
- **When** serial_parse_power is called
- **Then** Address is 10 and power is 500

> Test: `test_power_valid` in `test_serial_parser.c:147`

### Power measurement with negative value (export)

**Requirement:** `REQ-SERIAL-002`

- **Given** A serial buffer with negative power value
- **When** serial_parse_power is called
- **Then** Negative power is parsed correctly

> Test: `test_power_negative` in `test_serial_parser.c:162`

### Valid 16-byte node status with state B and no errors

**Requirement:** `REQ-SERIAL-003`

- **Given** A 16-byte buffer with state=1 (B), error=0, mode=0 (Normal)
- **When** serial_parse_node_status is called
- **Then** All fields parsed correctly

> Test: `test_node_status_valid` in `test_serial_parser.c:220`

### Node status with error flags and solar timer

**Requirement:** `REQ-SERIAL-003`

- **Given** A buffer with RCM_TRIPPED error and large solar timer
- **When** serial_parse_node_status is called
- **Then** Error and solar timer are parsed correctly

> Test: `test_node_status_error_and_timer` in `test_serial_parser.c:248`

### Node status with mode Smart and max current boundary

**Requirement:** `REQ-SERIAL-003`

- **Given** A buffer with mode=1 (Smart) and max current 255 (max byte value)
- **When** serial_parse_node_status is called
- **Then** Max current is 255 * 10 = 2550

> Test: `test_node_status_max_current_boundary` in `test_serial_parser.c:276`

---

## Serial Input Validation

### Irms message with missing fields returns false

**Requirement:** `REQ-SERIAL-004`

- **Given** A serial buffer with only 2 of 4 expected Irms fields
- **When** serial_parse_irms is called
- **Then** Returns false

> Test: `test_irms_missing_fields` in `test_serial_parser.c:91`

### Irms token not found in buffer

**Requirement:** `REQ-SERIAL-004`

- **Given** A serial buffer without the Irms token
- **When** serial_parse_irms is called
- **Then** Returns false

> Test: `test_irms_token_not_found` in `test_serial_parser.c:104`

### NULL buffer passed to Irms parser

**Requirement:** `REQ-SERIAL-004`

- **Given** A NULL buffer pointer
- **When** serial_parse_irms is called
- **Then** Returns false without crashing

> Test: `test_irms_null_buffer` in `test_serial_parser.c:117`

### Empty buffer passed to Irms parser

**Requirement:** `REQ-SERIAL-004`

- **Given** An empty string buffer
- **When** serial_parse_irms is called
- **Then** Returns false

> Test: `test_irms_empty_buffer` in `test_serial_parser.c:130`

### Power message with missing field returns false

**Requirement:** `REQ-SERIAL-004`

- **Given** A serial buffer with only the address, no power value
- **When** serial_parse_power is called
- **Then** Returns false

> Test: `test_power_missing_field` in `test_serial_parser.c:177`

### Power token not found in buffer

**Requirement:** `REQ-SERIAL-004`

- **Given** A serial buffer without the PowerMeasured token
- **When** serial_parse_power is called
- **Then** Returns false

> Test: `test_power_token_not_found` in `test_serial_parser.c:190`

### NULL buffer passed to power parser

**Requirement:** `REQ-SERIAL-004`

- **Given** A NULL buffer pointer
- **When** serial_parse_power is called
- **Then** Returns false without crashing

> Test: `test_power_null_buffer` in `test_serial_parser.c:203`

### Node status buffer too short

**Requirement:** `REQ-SERIAL-004`

- **Given** A buffer shorter than 16 bytes
- **When** serial_parse_node_status is called
- **Then** Returns false

> Test: `test_node_status_buffer_too_short` in `test_serial_parser.c:297`

### NULL buffer passed to node status parser

**Requirement:** `REQ-SERIAL-004`

- **Given** A NULL buffer pointer
- **When** serial_parse_node_status is called
- **Then** Returns false without crashing

> Test: `test_node_status_null_buffer` in `test_serial_parser.c:311`

### NULL output passed to node status parser

**Requirement:** `REQ-SERIAL-004`

- **Given** A valid buffer but NULL output pointer
- **When** serial_parse_node_status is called
- **Then** Returns false without crashing

> Test: `test_node_status_null_output` in `test_serial_parser.c:324`

---

## Battery Current Calculation

### Fresh battery data in solar mode

**Requirement:** `REQ-CALC-001`

- **Given** Battery update 30s ago, solar mode, current = 1000
- **When** calc_battery_current is called
- **Then** Returns 1000 (battery current value)

> Test: `test_battery_current_fresh_solar_api` in `test_serial_parser.c:341`

### Stale battery data is ignored after 60 seconds

**Requirement:** `REQ-CALC-001`

- **Given** Battery update 61s ago
- **When** calc_battery_current is called
- **Then** Returns 0 (stale data ignored)

> Test: `test_battery_current_stale_data` in `test_serial_parser.c:354`

### Battery data exactly at 60 second boundary

**Requirement:** `REQ-CALC-001`

- **Given** Battery update exactly 60s ago
- **When** calc_battery_current is called
- **Then** Returns battery current (60s is not stale)

> Test: `test_battery_current_boundary_60s` in `test_serial_parser.c:367`

### Non-solar mode returns zero

**Requirement:** `REQ-CALC-001`

- **Given** Normal mode with fresh battery data
- **When** calc_battery_current is called
- **Then** Returns 0 (battery only used in solar mode)

> Test: `test_battery_current_normal_mode` in `test_serial_parser.c:380`

### Non-API meter in solar mode still returns battery current

**Requirement:** `REQ-CALC-001`

- **Given** Solar mode with non-API meter type
- **When** calc_battery_current is called
- **Then** Returns battery current (battery used with any meter in solar mode)

> Test: `test_battery_current_non_api_meter` in `test_serial_parser.c:393`

### Never-updated battery returns zero

**Requirement:** `REQ-CALC-001`

- **Given** time_since_update is 0 (never updated)
- **When** calc_battery_current is called
- **Then** Returns 0

> Test: `test_battery_current_never_updated` in `test_serial_parser.c:406`

### Negative battery current (discharging) in solar mode

**Requirement:** `REQ-CALC-001`

- **Given** Battery discharging with negative current value
- **When** calc_battery_current is called
- **Then** Returns the negative value

> Test: `test_battery_current_negative_discharge` in `test_serial_parser.c:419`

---

## Current Sum Calculation

### Three-phase system distributes battery current equally

**Requirement:** `REQ-CALC-002`

- **Given** Three-phase mains at 100,200,300 dA and battery current 300 dA
- **When** calc_isum is called with enable_c2 = NOT_PRESENT
- **Then** Battery current divided by 3 (100) subtracted from each phase, Isum = 300

> Test: `test_isum_three_phase_battery` in `test_serial_parser.c:436`

### Single-phase battery adjustment uses L1 only

**Requirement:** `REQ-CALC-003`

- **Given** EnableC2 ALWAYS_OFF, battery current 300 dA
- **When** calc_isum is called
- **Then** Full battery current subtracted from L1, L2 and L3 unchanged

> Test: `test_isum_single_phase_battery` in `test_serial_parser.c:457`

### Zero battery current leaves mains unchanged

**Requirement:** `REQ-CALC-002`

- **Given** Zero battery current
- **When** calc_isum is called
- **Then** Adjusted currents equal original mains currents

> Test: `test_isum_zero_battery` in `test_serial_parser.c:478`

### Negative mains currents (solar injection) with battery

**Requirement:** `REQ-CALC-002`

- **Given** Negative mains currents and positive battery current
- **When** calc_isum is called
- **Then** Battery adjustment makes values more negative

> Test: `test_isum_negative_mains` in `test_serial_parser.c:499`

### Battery current not evenly divisible by 3

**Requirement:** `REQ-CALC-002`

- **Given** Battery current of 100 (100/3 = 33 per phase, integer division)
- **When** calc_isum is called
- **Then** Each phase reduced by 33 (truncated integer division)

> Test: `test_isum_battery_rounding` in `test_serial_parser.c:520`

---

## Charge Session Logging

### Start and end a normal charge session

**Requirement:** `REQ-ERE-001`

- **Given** The session logger is initialized
- **When** A session is started then ended with energy readings
- **Then** energy_charged_wh equals end_energy - start_energy

> Test: `test_session_basic_lifecycle` in `test_session_log.c:1`

### Session IDs increment across sessions

**Requirement:** `REQ-ERE-002`

- **Given** The session logger is initialized
- **When** Two sessions are started and ended
- **Then** The second session has a higher session_id

> Test: `test_session_id_increments` in `test_session_log.c:51`

### OCPP transaction ID replaces session ID

**Requirement:** `REQ-ERE-003`

- **Given** An active charge session
- **When** session_set_ocpp_id is called with a transaction ID
- **Then** The session record has ocpp_active=1 and the OCPP transaction ID

> Test: `test_session_ocpp_id` in `test_session_log.c:74`

### session_end without prior session_start is ignored

**Requirement:** `REQ-ERE-004`

- **Given** The session logger is initialized with no active session
- **When** session_end is called
- **Then** No crash occurs and session_get_last returns NULL

> Test: `test_session_end_without_start` in `test_session_log.c:95`

### session_start while session active discards previous

**Requirement:** `REQ-ERE-005`

- **Given** An active charge session
- **When** session_start is called again
- **Then** The previous session is discarded and a new one begins

> Test: `test_session_start_while_active` in `test_session_log.c:111`

### session_get_last before any session returns NULL

**Requirement:** `REQ-ERE-006`

- **Given** The session logger is freshly initialized
- **When** session_get_last is called
- **Then** NULL is returned

> Test: `test_session_get_last_before_any` in `test_session_log.c:136`

### session_set_ocpp_id with no active session is ignored

**Requirement:** `REQ-ERE-007`

- **Given** The session logger is initialized with no active session
- **When** session_set_ocpp_id is called
- **Then** No crash occurs and no state changes

> Test: `test_session_set_ocpp_no_active` in `test_session_log.c:149`

### Solar mode session records mode correctly

**Requirement:** `REQ-ERE-008`

- **Given** The session logger is initialized
- **When** A session is started with MODE_SOLAR
- **Then** The completed record has mode=2

> Test: `test_session_solar_mode` in `test_session_log.c:164`

### Zero energy session is recorded correctly

**Requirement:** `REQ-ERE-015`

- **Given** A session where start and end energy are the same
- **When** The session ends
- **Then** energy_charged_wh is 0

> Test: `test_session_zero_energy` in `test_session_log.c:323`

### session_start with timestamp 0 is rejected

**Requirement:** `REQ-ERE-020`

- **Given** The session logger is initialized
- **When** session_start is called with timestamp 0 (NTP not synced)
- **Then** No session is started and session_is_active returns 0

> Test: `test_session_start_rejects_zero_timestamp` in `test_session_log.c:343`

### session_start with pre-2024 timestamp is rejected

**Requirement:** `REQ-ERE-021`

- **Given** The session logger is initialized
- **When** session_start is called with timestamp 1000 (pre-2024)
- **Then** No session is started and session_is_active returns 0

> Test: `test_session_start_rejects_pre2024_timestamp` in `test_session_log.c:357`

### session_start with valid 2024+ timestamp succeeds

**Requirement:** `REQ-ERE-022`

- **Given** The session logger is initialized
- **When** session_start is called with timestamp 1710000000 (March 2024)
- **Then** A session is started and session_is_active returns 1

> Test: `test_session_start_accepts_valid_timestamp` in `test_session_log.c:371`

### Session shorter than 60 seconds is discarded

**Requirement:** `REQ-ERE-025`

- **Given** The session logger is initialized
- **When** A session is started and ended after only 30 seconds
- **Then** The session is discarded, session_is_active returns 0, and session_get_last returns NULL

> Test: `test_session_short_duration_discarded` in `test_session_log.c:387`

### Session with duration exactly 60 seconds is kept

**Requirement:** `REQ-ERE-026`

- **Given** The session logger is initialized
- **When** A session is started and ended after exactly 60 seconds
- **Then** The session is stored and session_get_last returns a valid record with correct energy

> Test: `test_session_exact_min_duration_kept` in `test_session_log.c:405`

---

## Charge Session JSON Export

### JSON output contains all ERE-required fields

**Requirement:** `REQ-ERE-010`

- **Given** A completed charge session
- **When** session_to_json is called
- **Then** The JSON contains session_id, start, end, kwh, and energy fields

> Test: `test_session_json_basic` in `test_session_log.c:186`

### JSON with OCPP active includes transaction ID

**Requirement:** `REQ-ERE-011`

- **Given** A completed session with OCPP transaction ID set
- **When** session_to_json is called
- **Then** ocpp_tx_id contains the numeric transaction ID

> Test: `test_session_json_ocpp` in `test_session_log.c:219`

### Null record returns -1

**Requirement:** `REQ-ERE-012`

- **Given** A NULL session record pointer
- **When** session_to_json is called
- **Then** It returns -1

> Test: `test_session_json_null_record` in `test_session_log.c:240`

### Null buffer returns -1

**Requirement:** `REQ-ERE-012`

- **Given** A valid session record
- **When** session_to_json is called with NULL buffer
- **Then** It returns -1

> Test: `test_session_json_null_buffer` in `test_session_log.c:253`

### Zero-length buffer returns -1

**Requirement:** `REQ-ERE-012`

- **Given** A valid session record
- **When** session_to_json is called with bufsz=0
- **Then** It returns -1

> Test: `test_session_json_zero_buffer` in `test_session_log.c:270`

### Too-small buffer returns -1

**Requirement:** `REQ-ERE-013`

- **Given** A valid session record
- **When** session_to_json is called with a very small buffer
- **Then** It returns -1 (truncation detected)

> Test: `test_session_json_small_buffer` in `test_session_log.c:287`

### Smart mode string in JSON

**Requirement:** `REQ-ERE-014`

- **Given** A completed session in MODE_SMART
- **When** session_to_json is called
- **Then** mode field is "smart"

> Test: `test_session_json_smart_mode` in `test_session_log.c:304`

---

## Solar Balancing

### 3-phase solar shortage starts PhaseSwitchTimer

**Requirement:** `REQ-SOLAR-001`

- **Given** The EVSE is solar charging on 3 phases with EnableC2=AUTO and high mains load
- **When** evse_calc_balanced_current is called with large import (Isum=200)
- **Then** PhaseSwitchTimer is set to a value greater than 0

> Test: `test_solar_3p_shortage_starts_timer` in `test_solar_balancing.c:1`

### PhaseSwitchTimer reaching 2 or below triggers 3P to 1P phase switch

**Requirement:** `REQ-SOLAR-002`

- **Given** The EVSE is solar charging on 3 phases with EnableC2=AUTO and PhaseSwitchTimer=2
- **When** evse_calc_balanced_current is called with ongoing shortage
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_1P

> Test: `test_solar_3p_timer_triggers_1p_switch` in `test_solar_balancing.c:67`

### 1-phase solar surplus near MaxCurrent starts PhaseSwitchTimer for 3P upgrade

**Requirement:** `REQ-SOLAR-003`

- **Given** The EVSE is solar charging on 1 phase with IsetBalanced near MaxCurrent and good surplus
- **When** evse_calc_balanced_current is called with export (Isum=-100)
- **Then** PhaseSwitchTimer is set to 63 (countdown to 3P switch)

> Test: `test_solar_1p_surplus_starts_timer` in `test_solar_balancing.c:89`

### PhaseSwitchTimer reaching 3 or below on 1P triggers switch to 3P

**Requirement:** `REQ-SOLAR-004`

- **Given** The EVSE is solar charging on 1 phase with PhaseSwitchTimer=3 and large surplus
- **When** evse_calc_balanced_current is called
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_3P

> Test: `test_solar_1p_timer_triggers_3p_switch` in `test_solar_balancing.c:116`

### Insufficient surplus resets PhaseSwitchTimer to prevent false 3P upgrade

**Requirement:** `REQ-SOLAR-005`

- **Given** The EVSE is solar charging on 1 phase with IsetBalanced well below MaxCurrent
- **When** evse_calc_balanced_current is called with minimal surplus (Isum=-10)
- **Then** PhaseSwitchTimer is reset to 0

> Test: `test_solar_insufficient_surplus_resets_timer` in `test_solar_balancing.c:141`

### During solar startup period, EVSE is forced to MinCurrent

**Requirement:** `REQ-SOLAR-006`

- **Given** The EVSE is solar charging with IntTimer below SOLARSTARTTIME (in startup)
- **When** evse_calc_balanced_current is called
- **Then** Balanced[0] is set to MinCurrent*10 regardless of IsetBalanced

> Test: `test_solar_startup_forces_mincurrent` in `test_solar_balancing.c:166`

### Past startup period, EVSE uses calculated distribution value

**Requirement:** `REQ-SOLAR-007`

- **Given** The EVSE is solar charging with IntTimer past SOLARSTARTTIME
- **When** evse_calc_balanced_current is called
- **Then** Balanced[0] uses the calculated value (at least MinCurrent*10)

> Test: `test_solar_past_startup_uses_calculated` in `test_solar_balancing.c:184`

### Small solar export results in gradual current increase

**Requirement:** `REQ-SOLAR-008`

- **Given** The EVSE is solar charging with small export (Isum=-5)
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced increases by at least 1 (fine-grained increase)

> Test: `test_solar_fine_increase_small` in `test_solar_balancing.c:204`

### Large solar export results in larger current increase

**Requirement:** `REQ-SOLAR-009`

- **Given** The EVSE is solar charging with large export (Isum=-50)
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced increases by more than the small export case

> Test: `test_solar_fine_increase_large` in `test_solar_balancing.c:225`

### Moderate grid import decreases solar charging current

**Requirement:** `REQ-SOLAR-010`

- **Given** The EVSE is solar charging with IsetBalanced=150 and moderate import (Isum=15)
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced decreases below 150

> Test: `test_solar_fine_decrease_moderate` in `test_solar_balancing.c:246`

### Large grid import aggressively decreases solar charging current

**Requirement:** `REQ-SOLAR-011`

- **Given** The EVSE is solar charging with IsetBalanced=200 and large import (Isum=50)
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced decreases below 200

> Test: `test_solar_fine_decrease_aggressive` in `test_solar_balancing.c:266`

### Solar B-state with AUTO and small surplus determines 1-phase charging

**Requirement:** `REQ-SOLAR-012`

- **Given** The EVSE is in STATE_B with EnableC2=AUTO, 3 phases, and small surplus (Isum=-50)
- **When** evse_calc_balanced_current is called
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_1P

> Test: `test_solar_b_state_auto_determines_1p` in `test_solar_balancing.c:286`

### Solar B-state with AUTO and large surplus determines 3-phase charging

**Requirement:** `REQ-SOLAR-013`

- **Given** The EVSE is in STATE_B with EnableC2=AUTO, 1 phase, and large surplus (Isum=-500)
- **When** evse_calc_balanced_current is called
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_3P

> Test: `test_solar_b_state_auto_determines_3p` in `test_solar_balancing.c:307`

### Hard current shortage increments NoCurrent counter

**Requirement:** `REQ-SOLAR-014`

- **Given** The EVSE is in MODE_SMART with heavily overloaded mains and low MaxMains
- **When** evse_calc_balanced_current is called
- **Then** NoCurrent counter is incremented above 0

> Test: `test_hard_shortage_increments_nocurrent` in `test_solar_balancing.c:328`

### Soft shortage (Isum exceeds MaxSumMains) starts MaxSumMains timer

**Requirement:** `REQ-SOLAR-015`

- **Given** The EVSE is in MODE_SMART with Isum exceeding MaxSumMains and MaxSumMainsTime=5
- **When** evse_calc_balanced_current is called
- **Then** MaxSumMainsTimer is set to MaxSumMainsTime*60 (300 seconds)

> Test: `test_soft_shortage_starts_maxsummains_timer` in `test_solar_balancing.c:349`

### No shortage condition clears SolarStopTimer and decays NoCurrent

**Requirement:** `REQ-SOLAR-016`

- **Given** The EVSE is in MODE_SMART with low mains load and high MaxMains
- **When** evse_calc_balanced_current is called with no shortage detected
- **Then** SolarStopTimer is reset to 0 and NoCurrent decays by 1

> Test: `test_no_shortage_clears_timers` in `test_solar_balancing.c:374`

### IsetBalanced is capped at 800 (80A maximum)

**Requirement:** `REQ-SOLAR-017`

- **Given** The EVSE is in MODE_SMART with IsetBalanced=900 and large surplus
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced does not exceed 800

> Test: `test_isetbalanced_capped_at_800` in `test_solar_balancing.c:397`

### Normal mode forces 3-phase charging regardless of current phase count

**Requirement:** `REQ-SOLAR-018`

- **Given** A standalone EVSE in MODE_NORMAL currently on 1 phase
- **When** evse_calc_balanced_current is called
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_3P

> Test: `test_normal_mode_forces_3p` in `test_solar_balancing.c:417`

### phasesLastUpdateFlag=false prevents IsetBalanced regulation

**Requirement:** `REQ-SOLAR-019`

- **Given** The EVSE is in MODE_SMART with phasesLastUpdateFlag=false and large surplus
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced remains unchanged (regulation gated)

> Test: `test_phases_flag_gates_regulation` in `test_solar_balancing.c:444`

### Multi-EVSE solar startup: EVSE in startup gets MinCurrent, others get calculated

**Requirement:** `REQ-SOLAR-020`

- **Given** Two EVSEs as master, EVSE 0 in startup (IntTimer < SOLARSTARTTIME), EVSE 1 past startup
- **When** evse_calc_balanced_current is called
- **Then** EVSE 0 Balanced is set to MinCurrent*10 (startup forcing)

> Test: `test_multi_evse_solar_startup` in `test_solar_balancing.c:467`

### EMA smoothing dampens sudden IsetBalanced changes

**Requirement:** `REQ-SOL-021`

- **Given** The EVSE is in smart mode with IsetBalanced_ema=100 and EmaAlpha=50
- **When** evse_calc_balanced_current computes a new IsetBalanced of 200
- **Then** IsetBalanced_ema moves toward 200 but not all the way (between 100 and 200)

> Test: `test_ema_smoothing_dampens_change` in `test_solar_balancing.c:523`

### EMA with alpha=100 tracks raw IsetBalanced exactly (no smoothing)

**Requirement:** `REQ-SOL-022`

- **Given** The EVSE is in smart mode with EmaAlpha=100 and IsetBalanced_ema=50
- **When** evse_calc_balanced_current computes a new IsetBalanced with large surplus
- **Then** IsetBalanced_ema updates to a value different from the old 50

> Test: `test_ema_alpha_100_no_smoothing` in `test_solar_balancing.c:543`

### EMA with alpha=0 holds previous value (full dampening)

**Requirement:** `REQ-SOL-023`

- **Given** The EVSE is in smart mode with EmaAlpha=0 and IsetBalanced_ema=80
- **When** evse_calc_balanced_current computes a different IsetBalanced
- **Then** IsetBalanced_ema remains at 80

> Test: `test_ema_alpha_0_full_dampening` in `test_solar_balancing.c:563`

### EMA defaults are initialized correctly by evse_init

**Requirement:** `REQ-SOL-024`

- **Given** A freshly initialized EVSE context
- **When** evse_init is called
- **Then** EmaAlpha=100 (no smoothing), SmartDeadBand=10, RampRateDivisor=4, SolarFineDeadBand=5

> Test: `test_smoothing_defaults_initialized` in `test_solar_balancing.c:580`

### Smart mode dead band suppresses small adjustments

**Requirement:** `REQ-SOL-025`

- **Given** The EVSE is in smart mode with SmartDeadBand=10 and small Idifference (~5 dA)
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced does not change (within dead band)

> Test: `test_smart_deadband_suppresses_small_change` in `test_solar_balancing.c:600`

### Smart mode dead band allows large adjustments through

**Requirement:** `REQ-SOL-026`

- **Given** The EVSE is in smart mode with SmartDeadBand=10 and large Idifference
- **When** evse_calc_balanced_current is called with large surplus (Idifference >> 10)
- **Then** IsetBalanced increases (dead band does not suppress)

> Test: `test_smart_deadband_allows_large_change` in `test_solar_balancing.c:618`

### Smart mode dead band suppresses small negative Idifference

**Requirement:** `REQ-SOL-027`

- **Given** The EVSE is in smart mode with SmartDeadBand=10 and Idifference=-5
- **When** evse_calc_balanced_current is called with slight overload
- **Then** IsetBalanced does not decrease (within dead band)

> Test: `test_smart_deadband_suppresses_small_decrease` in `test_solar_balancing.c:635`

### Symmetric ramp applies same rate for increasing and decreasing

**Requirement:** `REQ-SOL-028`

- **Given** The EVSE is in smart mode with RampRateDivisor=4 and Idifference=40
- **When** evse_calc_balanced_current is called with positive Idifference
- **Then** IsetBalanced increases by Idifference/4 = 10

> Test: `test_symmetric_ramp_increase` in `test_solar_balancing.c:658`

### Symmetric ramp applies same divisor for decrease (was full-step)

**Requirement:** `REQ-SOL-029`

- **Given** The EVSE is in smart mode with RampRateDivisor=4 and Idifference=-40
- **When** evse_calc_balanced_current is called with negative Idifference
- **Then** IsetBalanced decreases by |Idifference|/4 = 10 (not full 40)

> Test: `test_symmetric_ramp_decrease` in `test_solar_balancing.c:681`

### Solar fine regulation dead band expanded to 5 dA

**Requirement:** `REQ-SOL-030`

- **Given** The EVSE is solar charging with IsumImport=4 (was outside old 3 dA band, now inside 5 dA)
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced does not decrease from fine regulation (4 dA within 5 dA dead band)

> Test: `test_solar_fine_deadband_expanded` in `test_solar_balancing.c:707`

### Solar fine regulation triggers decrease above expanded dead band

**Requirement:** `REQ-SOL-031`

- **Given** The EVSE is solar charging with IsumImport=15 (well above 5 dA dead band)
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced decreases (outside dead band)

> Test: `test_solar_fine_deadband_triggers_above` in `test_solar_balancing.c:733`

### NoCurrent below threshold does not trigger LESS_6A

**Requirement:** `REQ-SOL-032`

- **Given** The EVSE is in MODE_SMART with NoCurrent=5 and NoCurrentThreshold=10
- **When** evse_calc_balanced_current is called with hard shortage
- **Then** NoCurrent increments but LESS_6A is not set (below threshold)

> Test: `test_nocurrent_below_threshold_no_less6a` in `test_solar_balancing.c:769`

### NoCurrent reaching threshold triggers LESS_6A

**Requirement:** `REQ-SOL-033`

- **Given** The EVSE is in MODE_SMART with NoCurrent=9 and NoCurrentThreshold=10
- **When** evse_calc_balanced_current is called with hard shortage
- **Then** NoCurrent reaches 10 and LESS_6A is set

> Test: `test_nocurrent_at_threshold_triggers_less6a` in `test_solar_balancing.c:786`

### NoCurrent decays gradually when shortage resolves (not instant reset)

**Requirement:** `REQ-SOL-034`

- **Given** The EVSE is in MODE_SMART with NoCurrent=8 and no shortage
- **When** evse_calc_balanced_current is called with surplus
- **Then** NoCurrent decrements by 1 (not reset to 0)

> Test: `test_nocurrent_decays_gradually` in `test_solar_balancing.c:805`

### NoCurrent at 0 stays at 0 when no shortage

**Requirement:** `REQ-SOL-035`

- **Given** The EVSE is in MODE_SMART with NoCurrent=0 and no shortage
- **When** evse_calc_balanced_current is called
- **Then** NoCurrent stays at 0

> Test: `test_nocurrent_stays_zero` in `test_solar_balancing.c:825`

### Solar min run time prevents LESS_6A during initial charging

**Requirement:** `REQ-SOL-036`

- **Given** The EVSE is solar charging with IntTimer < SolarMinRunTime and hard shortage
- **When** NoCurrent exceeds threshold
- **Then** LESS_6A is NOT set (protected by min run time)

> Test: `test_solar_min_run_time_prevents_less6a` in `test_solar_balancing.c:846`

### Solar min run time expired allows LESS_6A

**Requirement:** `REQ-SOL-037`

- **Given** The EVSE is solar charging with IntTimer >= SolarMinRunTime and hard shortage
- **When** NoCurrent exceeds threshold
- **Then** LESS_6A is set (min run time has passed)

> Test: `test_solar_min_run_time_expired_allows_less6a` in `test_solar_balancing.c:864`

### Solar mode uses shorter charge delay when LESS_6A active

**Requirement:** `REQ-SOL-038`

- **Given** The EVSE is in MODE_SOLAR with LESS_6A error active and SolarChargeDelay=15
- **When** evse_tick_1s is called
- **Then** ChargeDelay is set to SolarChargeDelay (15) not CHARGEDELAY (60)

> Test: `test_solar_charge_delay_shorter` in `test_solar_balancing.c:884`

### Smart mode still uses full charge delay when LESS_6A active

**Requirement:** `REQ-SOL-039`

- **Given** The EVSE is in MODE_SMART with LESS_6A error active and no current available
- **When** evse_tick_1s is called
- **Then** ChargeDelay is set to CHARGEDELAY (60)

> Test: `test_smart_charge_delay_unchanged` in `test_solar_balancing.c:905`

### Cycling prevention defaults initialized correctly

**Requirement:** `REQ-SOL-040`

- **Given** A freshly initialized EVSE context
- **When** evse_init is called
- **Then** NoCurrentThreshold=10, SolarChargeDelay=15, SolarMinRunTime=60

> Test: `test_cycling_prevention_defaults` in `test_solar_balancing.c:927`

### Settling window suppresses smart regulation after current change

**Requirement:** `REQ-SOL-041`

- **Given** The EVSE is in smart mode with SettlingTimer > 0 (settling active)
- **When** evse_calc_balanced_current is called with large surplus
- **Then** IsetBalanced does not increase (regulation suppressed during settling)

> Test: `test_settling_window_suppresses_regulation` in `test_solar_balancing.c:947`

### Regulation proceeds normally when settling timer is 0

**Requirement:** `REQ-SOL-042`

- **Given** The EVSE is in smart mode with SettlingTimer=0 and large surplus
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced increases (regulation active)

> Test: `test_settling_expired_allows_regulation` in `test_solar_balancing.c:970`

### Balanced[0] change triggers settling timer

**Requirement:** `REQ-SOL-043`

- **Given** The EVSE is solar charging with LastBalanced=100 and SettlingWindow=5
- **When** evse_calc_balanced_current produces a different Balanced[0]
- **Then** SettlingTimer is set to SettlingWindow

> Test: `test_current_change_triggers_settling` in `test_solar_balancing.c:992`

### Ramp rate limits how much Balanced[0] can change per cycle

**Requirement:** `REQ-SOL-044`

- **Given** The EVSE is smart charging with MaxRampRate=30 and Balanced[0]=100
- **When** evse_calc_balanced_current produces a large increase
- **Then** Balanced[0] changes by at most MaxRampRate from LastBalanced

> Test: `test_ramp_rate_limits_increase` in `test_solar_balancing.c:1019`

### Ramp rate limits how much Balanced[0] can decrease per cycle

**Requirement:** `REQ-SOL-045`

- **Given** The EVSE is smart charging with MaxRampRate=30 and Balanced[0]=160
- **When** evse_calc_balanced_current produces a large decrease
- **Then** Balanced[0] decreases by at most MaxRampRate from LastBalanced

> Test: `test_ramp_rate_limits_decrease` in `test_solar_balancing.c:1044`

### SettlingTimer counts down each second

**Requirement:** `REQ-SOL-046`

- **Given** SettlingTimer=3
- **When** evse_tick_1s is called
- **Then** SettlingTimer decrements to 2

> Test: `test_settling_timer_countdown` in `test_solar_balancing.c:1069`

### Slow EV compatibility defaults initialized correctly

**Requirement:** `REQ-SOL-047`

- **Given** A freshly initialized EVSE context
- **When** evse_init is called
- **Then** SettlingWindow=5, MaxRampRate=30, SettlingTimer=0, LastBalanced=0

> Test: `test_slow_ev_defaults` in `test_solar_balancing.c:1087`

### MaxRampRate=0 disables ramp rate limiting

**Requirement:** `REQ-SOL-048`

- **Given** The EVSE is smart charging with MaxRampRate=0
- **When** evse_calc_balanced_current produces a large change
- **Then** Balanced[0] is not ramp-limited (can change freely)

> Test: `test_ramp_rate_zero_no_limit` in `test_solar_balancing.c:1106`

### Debug snapshot is populated after evse_calc_balanced_current

**Requirement:** `REQ-SOL-049`

- **Given** The EVSE is solar charging with known meter readings
- **When** evse_calc_balanced_current is called
- **Then** solar_debug snapshot contains matching values

> Test: `test_solar_debug_snapshot_populated` in `test_solar_balancing.c:1131`

---

## IEC 61851-1 State Transitions

### EVSE initialises to disconnected state

**Requirement:** `REQ-IEC61851-001`

- **Given** The EVSE is powered on
- **When** evse_init() is called
- **Then** The state machine starts in STATE_A (disconnected)

> Test: `test_init_state_is_A` in `test_state_transitions.c:1`

### Contactors are open after initialisation

**Requirement:** `REQ-IEC61851-002`

- **Given** The EVSE is powered on
- **When** evse_init() is called
- **Then** Both contactor1 and contactor2 are off (open)

> Test: `test_init_contactors_off` in `test_state_transitions.c:45`

### Pilot signal is connected after initialisation

**Requirement:** `REQ-IEC61851-003`

- **Given** The EVSE is powered on
- **When** evse_init() is called
- **Then** The pilot signal is connected (pilot_connected is true)

> Test: `test_init_pilot_connected` in `test_state_transitions.c:59`

### No error flags after initialisation

**Requirement:** `REQ-IEC61851-004`

- **Given** The EVSE is powered on
- **When** evse_init() is called
- **Then** ErrorFlags is NO_ERROR

> Test: `test_init_no_errors` in `test_state_transitions.c:72`

### STATE_A remains when no vehicle is connected

**Requirement:** `REQ-IEC61851-005`

- **Given** The EVSE is in STATE_A (disconnected) and ready to charge
- **When** A 12V pilot signal is received (no vehicle present)
- **Then** The state remains STATE_A

> Test: `test_A_stays_A_on_12V` in `test_state_transitions.c:85`

### Vehicle connection triggers STATE_A to STATE_B transition

**Requirement:** `REQ-IEC61851-006`

- **Given** The EVSE is in STATE_A, authorized, and ready to charge
- **When** A 9V pilot signal is received (vehicle connected)
- **Then** The state transitions to STATE_B (connected, not charging)

> Test: `test_A_to_B_on_9V_when_ready` in `test_state_transitions.c:99`

### Modem negotiation required before STATE_B when ModemStage is 0

**Requirement:** `REQ-IEC61851-007`

- **Given** The EVSE is in STATE_A with ModemStage=0 (unauthenticated modem)
- **When** A 9V pilot signal is received (vehicle connected)
- **Then** The state transitions to STATE_MODEM_REQUEST for ISO15118 negotiation

> Test: `test_A_to_modem_when_modem_stage_0` in `test_state_transitions.c:113`

### A→B goes directly to STATE_B when modem is disabled

**Requirement:** `REQ-IEC61851-007B`

- **Given** The EVSE is in STATE_A with ModemStage=0 but ModemEnabled=false
- **When** A 9V pilot signal is received (vehicle connected)
- **Then** The state transitions directly to STATE_B (modem flow skipped)

> Test: `test_A_to_B_skips_modem_when_disabled` in `test_state_transitions.c:132`

### Unauthorized EVSE blocks STATE_A to STATE_B transition

**Requirement:** `REQ-IEC61851-008`

- **Given** The EVSE is in STATE_A with AccessStatus OFF (not authorized)
- **When** A 9V pilot signal is received (vehicle connected)
- **Then** The state remains STATE_A (transition blocked)

> Test: `test_A_stays_A_when_access_off` in `test_state_transitions.c:151`

### Vehicle disconnect in STATE_B returns to STATE_A

**Requirement:** `REQ-IEC61851-011`

- **Given** The EVSE is in STATE_A with an active TEMP_HIGH error flag
- **Given** The EVSE is in STATE_A with ChargeDelay > 0
- **Given** The EVSE is in STATE_B (vehicle connected, not charging)
- **When** A 9V pilot signal is received (vehicle connected)
- **When** A 9V pilot signal is received (vehicle connected)
- **When** A 12V pilot signal is received (vehicle disconnected)
- **Then** The state transitions to STATE_B1 (connected, waiting due to error)
- **Then** The state transitions to STATE_B1 (connected, waiting for delay)
- **Then** The state transitions back to STATE_A (disconnected)

> Test: `test_B_to_A_on_disconnect` in `test_state_transitions.c:166`

### Vehicle charge request after diode check triggers STATE_B to STATE_C

**Requirement:** `REQ-IEC61851-012`

- **Given** The EVSE is in STATE_B with DiodeCheck passed and sufficient current
- **When** A 6V pilot signal is sustained for 500ms (vehicle requests charge)
- **Then** The state transitions to STATE_C (charging)

> Test: `test_B_to_C_on_6V_with_diode_check` in `test_state_transitions.c:213`

### Charge request without diode check does not transition to STATE_C

**Requirement:** `REQ-IEC61851-013`

- **Given** The EVSE is in STATE_B with DiodeCheck NOT passed
- **When** A 6V pilot signal is sustained for 500ms
- **Then** The state does NOT transition to STATE_C

> Test: `test_B_to_C_requires_diode_check` in `test_state_transitions.c:234`

### PILOT_DIODE signal sets DiodeCheck flag

**Requirement:** `REQ-IEC61851-014`

- **Given** The EVSE is in STATE_B with DiodeCheck=0
- **When** A PILOT_DIODE signal is received
- **Then** DiodeCheck is set to 1

> Test: `test_diode_check_sets_on_pilot_diode` in `test_state_transitions.c:255`

### Contactor 1 is closed when entering STATE_C

**Requirement:** `REQ-IEC61851-015`

- **Given** The EVSE transitions to STATE_C (charging)
- **When** evse_set_state is called with STATE_C
- **Then** contactor1_state is true (closed, power flowing)

> Test: `test_C_contactor1_on` in `test_state_transitions.c:271`

### Vehicle disconnect during charging returns to STATE_A with contactors open

**Requirement:** `REQ-IEC61851-016`

- **Given** The EVSE is in STATE_C (charging)
- **When** A 12V pilot signal is received (vehicle disconnected)
- **Then** The state transitions to STATE_A and contactor1 is opened

> Test: `test_C_to_A_on_disconnect` in `test_state_transitions.c:285`

### Vehicle disconnect from STATE_C1 returns to STATE_A

**Requirement:** `REQ-IEC61851-019`

- **Given** The EVSE is in STATE_C (charging)
- **Given** The EVSE is in STATE_C (charging)
- **Given** The EVSE is in STATE_C1 (charging suspended)
- **When** A 9V pilot signal is received (vehicle stops charge request)
- **When** PILOT_SHORT is sustained for more than 500ms (debounce period)
- **When** A 12V pilot signal is received (vehicle disconnected)
- **Then** The state transitions to STATE_B and DiodeCheck is reset to 0
- **Then** The state transitions to STATE_B
- **Then** The state transitions to STATE_A

> Test: `test_C1_to_A_on_disconnect` in `test_state_transitions.c:301`

### Vehicle disconnect from STATE_B1 returns to STATE_A

**Requirement:** `REQ-IEC61851-022`

- **Given** The EVSE is in STATE_C1 (charging suspended)
- **Given** The EVSE is in STATE_C1 with C1Timer set to 6 seconds
- **Given** The EVSE is in STATE_B1 (waiting) with pilot reconnected
- **When** A 9V pilot signal is received
- **When** The C1Timer counts down to zero via tick_1s
- **When** A 12V pilot signal is received (vehicle disconnected)
- **Then** The state transitions to STATE_B1 (waiting), not STATE_B
- **Then** The state transitions to STATE_B1 and both contactors are opened
- **Then** The state transitions to STATE_A

> Test: `test_B1_to_A_on_disconnect` in `test_state_transitions.c:352`

### Entering STATE_B1 sets a non-zero ChargeDelay

**Requirement:** `REQ-IEC61851-023`

- **Given** The EVSE is ready to charge with ChargeDelay=0
- **When** The state is set to STATE_B1
- **Then** ChargeDelay is set to a value greater than 0

> Test: `test_set_state_B1_sets_charge_delay` in `test_state_transitions.c:410`

### Entering STATE_A clears LESS_6A error and ChargeDelay

**Requirement:** `REQ-IEC61851-024`

- **Given** The EVSE has LESS_6A error flag set and ChargeDelay > 0
- **When** The state is set to STATE_A
- **Then** LESS_6A is cleared from ErrorFlags and ChargeDelay is set to 0

> Test: `test_set_state_A_clears_errors_and_delay` in `test_state_transitions.c:425`

### State transitions are recorded in the transition log

**Requirement:** `REQ-IEC61851-025`

- **Given** The EVSE is ready to charge
- **When** Two state transitions occur (STATE_B then STATE_C)
- **Then** transition_count is 2 and the log contains both states in order

> Test: `test_transition_log_records_states` in `test_state_transitions.c:442`

### Entering STATE_C1 sets PWM to off (+12V)

**Requirement:** `REQ-IEC61851-026`

- **Given** The EVSE is in STATE_C (charging)
- **When** The state is set to STATE_C1 (charging suspended)
- **Then** PWM duty is set to 1024 (off / +12V constant)

> Test: `test_set_state_C1_sets_pwm_off` in `test_state_transitions.c:459`

### Full charge cycle: A -> B -> C -> B -> A

**Requirement:** `REQ-IEC61851-027`

- **Given** The EVSE is in STATE_A, authorized and ready to charge
- **When** The vehicle connects (9V), requests charge (6V), stops (9V), disconnects (12V)
- **Then** The EVSE transitions A->B->C->B->A with correct contactor states

> Test: `test_full_charge_cycle` in `test_state_transitions.c:474`

### Vehicle disconnect during ACTSTART is NOT handled in tick_10ms

**Requirement:** `REQ-IEC61851-028`

- **Given** The EVSE is in STATE_ACTSTART (activation mode)
- **When** A 12V pilot signal is received (vehicle disconnected)
- **Then** The state stays ACTSTART (original behavior: no pilot check in ACTSTART,

> Test: `test_actstart_to_A_on_disconnect` in `test_state_transitions.c:511`

### ActivationMode=0 triggers STATE_ACTSTART on pilot detection in STATE_B

**Requirement:** `REQ-IEC61851-029`

- **Given** The EVSE is in STATE_B with ActivationMode=0
- **When** A 9V pilot signal is received
- **Then** The state transitions to STATE_ACTSTART with ActivationTimer set to 3

> Test: `test_activation_mode_triggers_actstart` in `test_state_transitions.c:528`

### ActivationMode=255 (always active) does not count down

**Requirement:** `REQ-IEC61851-031`

- **Given** The EVSE has ActivationMode set to 5
- **Given** The EVSE has ActivationMode set to 255
- **When** One second tick occurs
- **When** One second tick occurs
- **Then** ActivationMode decrements to 4
- **Then** ActivationMode remains at 255

> Test: `test_activation_mode_255_does_not_countdown` in `test_state_transitions.c:545`

### STATE_ACTSTART returns to STATE_B when ActivationTimer expires

**Requirement:** `REQ-IEC61851-032`

- **Given** The EVSE is in STATE_ACTSTART with ActivationTimer=0 (expired)
- **When** A non-12V pilot signal is received
- **Then** The state transitions to STATE_B and ActivationMode is set to 255

> Test: `test_actstart_returns_to_B_when_timer_expires` in `test_state_transitions.c:576`

### STATE_B re-entry sets Switching_Phases_C2 flag instead of direct phase change

**Requirement:** `REQ-PHASE-002`

- **Given** The EVSE is in STATE_COMM_B_OK (master approved B transition)
- **Given** The EVSE is in STATE_COMM_C_OK (master approved C transition)
- **Given** The EVSE is configured as a node (LoadBl=2)
- **Given** The EVSE is in STATE_A with EnableC2=ALWAYS_OFF and 3 phases configured
- **Given** The EVSE is already in STATE_B with EnableC2=ALWAYS_OFF and 3 phases configured
- **When** A 9V pilot signal is received
- **When** A 6V pilot signal is received
- **When** A 9V pilot signal is received (vehicle connected)
- **When** The state is set to STATE_B
- **When** The state is set to STATE_B again
- **Then** The state transitions to STATE_B and ActivationMode is set to 30
- **Then** The state transitions to STATE_C (charging)
- **Then** The state transitions to STATE_COMM_B (requesting master permission)
- **Then** Nr_Of_Phases_Charging is set directly to 1 (single phase)
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_1P (deferred switch)

> Test: `test_state_B_calls_check_switching_phases_from_B` in `test_state_transitions.c:593`

### STATE_B entry does NOT set pilot_connected when modem disabled

**Requirement:** `REQ-IEC61851-M3`

- **Given** ModemEnabled=false, EVSE transitions A→B
- **When** evse_set_state is called with STATE_B
- **Then** pilot_connected is NOT explicitly set by STATE_B entry

> Test: `test_state_b_no_pilot_reconnect_without_modem` in `test_state_transitions.c:688`

### STATE_B entry DOES set pilot_connected when modem enabled

**Requirement:** `REQ-IEC61851-M3B`

- **Given** ModemEnabled=true, EVSE transitions to STATE_B
- **When** evse_set_state is called with STATE_B
- **Then** pilot_connected is set to true

> Test: `test_state_b_pilot_reconnect_with_modem` in `test_state_transitions.c:710`

---

## 10ms Tick Processing

### Pilot disconnect guard ignores 9V signal while active

**Requirement:** `REQ-TICK10-001`

- **Given** EVSE is ready to charge with PilotDisconnected=true and PilotDisconnectTime > 0
- **When** A 9V pilot signal (vehicle connected) is received during a 10ms tick
- **Then** The EVSE remains in STATE_A because the disconnect guard suppresses the pilot reading

> Test: `test_pilot_disconnect_guards_reading` in `test_tick_10ms.c:1`

### Pilot disconnect flag clears when timer reaches zero

**Requirement:** `REQ-TICK10-002`

- **Given** EVSE is ready to charge with PilotDisconnected=true and PilotDisconnectTime=0
- **When** A 9V pilot signal is received during a 10ms tick
- **Then** PilotDisconnected is cleared to false and pilot_connected is set to true

> Test: `test_pilot_disconnect_clears_on_timer_zero` in `test_tick_10ms.c:47`

### RFID reader type 1 starts access lock timer

**Requirement:** `REQ-TICK10-003`

- **Given** EVSE is ready to charge with RFIDReader=1, AccessTimer=0, and AccessStatus=ON
- **When** A 12V pilot signal (no vehicle) is received during a 10ms tick
- **Then** AccessTimer is set to RFIDLOCKTIME to begin the RFID lock countdown

> Test: `test_rfid_reader_1_starts_access_timer` in `test_tick_10ms.c:66`

### MaxCapacity limits charge current when below MaxCurrent

**Requirement:** `REQ-TICK10-004`

- **Given** EVSE is ready to charge with MaxCapacity=8A which is less than MaxCurrent=13A
- **When** A 9V pilot signal triggers the A-to-B transition during a 10ms tick
- **Then** ChargeCurrent is set to 80 deciamps (MaxCapacity * 10)

> Test: `test_maxcapacity_limits_charge_current` in `test_tick_10ms.c:85`

### MaxCapacity at or above MaxCurrent falls back to MinCurrent

**Requirement:** `REQ-TICK10-005`

- **Given** EVSE is ready to charge with MaxCapacity=16A >= MaxCurrent=13A and MinCurrent=6A
- **When** A 9V pilot signal triggers the A-to-B transition during a 10ms tick
- **Then** ChargeCurrent is set to 60 deciamps (MinCurrent * 10) as the default starting point

> Test: `test_maxcapacity_default_uses_mincurrent` in `test_tick_10ms.c:101`

### LESS_6A error flag set when insufficient current available in Smart mode

**Requirement:** `REQ-TICK10-006`

- **Given** EVSE is in Smart mode standalone with very low MaxMains=2A and MainsMeterImeasured=200 (overloaded)
- **When** A 9V pilot signal (vehicle connected) is received during a 10ms tick
- **Then** The LESS_6A error flag is set because available current is below MinCurrent

> Test: `test_less_6a_when_no_current_available` in `test_tick_10ms.c:120`

### STATE_B with 6V pilot increments state timer for debounce

**Requirement:** `REQ-TICK10-007`

- **Given** EVSE is in STATE_B with DiodeCheck=1 and StateTimer=0
- **When** A 6V pilot signal (vehicle requesting charge) is received during a 10ms tick
- **Then** StateTimer increments to 1, counting toward the B-to-C debounce threshold

> Test: `test_b_6v_increments_state_timer` in `test_tick_10ms.c:144`

### STATE_B with 9V pilot resets the state timer

**Requirement:** `REQ-TICK10-008`

- **Given** EVSE is in STATE_B with StateTimer=30 (partially debounced)
- **When** A 9V pilot signal (vehicle connected but not requesting charge) is received
- **Then** StateTimer is reset to 0, canceling the B-to-C debounce countdown

> Test: `test_b_9v_resets_state_timer` in `test_tick_10ms.c:161`

### STATE_B to STATE_C transition requires 55 consecutive 6V ticks

**Requirement:** `REQ-TICK10-009`

- **Given** EVSE is in STATE_B with DiodeCheck=1 and ChargeCurrent at MaxCurrent
- **When** 50 consecutive 6V pilot ticks are received (below threshold) then 5 more (reaching 55)
- **Then** The EVSE does not transition at 50 ticks but transitions to STATE_C at 55 ticks

> Test: `test_b_to_c_debounce_threshold` in `test_tick_10ms.c:178`

### STATE_B to STATE_C transition is blocked when errors are present

**Requirement:** `REQ-TICK10-010`

- **Given** EVSE is in STATE_B with DiodeCheck=1 but ErrorFlags contains TEMP_HIGH
- **When** 55 consecutive 6V pilot ticks are received (past debounce threshold)
- **Then** The EVSE does not transition to STATE_C because an error condition is active

> Test: `test_b_to_c_requires_diode_and_no_errors` in `test_tick_10ms.c:201`

### STATE_C transitions to STATE_B after sustained pilot short debounce

**Requirement:** `REQ-TICK10-011`

- **Given** EVSE is in STATE_C (actively charging)
- **When** Fewer than 50 PILOT_SHORT ticks are received followed by enough to exceed 50 total
- **Then** The EVSE stays in STATE_C below 50 ticks but transitions to STATE_B after 50 consecutive short ticks

> Test: `test_c_short_debounce` in `test_tick_10ms.c:222`

### STATE_C stays in STATE_C on 6V pilot and resets state timer

**Requirement:** `REQ-TICK10-012`

- **Given** EVSE is in STATE_C with StateTimer=20
- **When** A 6V pilot signal is received during a 10ms tick
- **Then** The EVSE remains in STATE_C and StateTimer is reset to 0

> Test: `test_c_6v_no_transition` in `test_tick_10ms.c:243`

### COMM_B state does not trigger A-to-B transition logic on 9V pilot

**Requirement:** `REQ-TICK10-013`

- **Given** EVSE is a node (LoadBl=2) in STATE_COMM_B waiting for master confirmation
- **When** A 9V pilot signal is received during a 10ms tick
- **Then** The EVSE does not transition to STATE_B because COMM_B bypasses the A-to-B path

> Test: `test_comm_b_stays_on_9v` in `test_tick_10ms.c:262`

### Node transitions from STATE_B to STATE_COMM_C instead of STATE_C

**Requirement:** `REQ-TICK10-014`

- **Given** EVSE is a node (LoadBl=2) in STATE_B with DiodeCheck=1 and sufficient charge current
- **When** 55 consecutive 6V pilot ticks are received (past debounce threshold)
- **Then** The EVSE transitions to STATE_COMM_C (waiting for master to confirm charge start)

> Test: `test_node_b_to_comm_c` in `test_tick_10ms.c:286`

### A-to-B transition sets BalancedMax from MaxCapacity

**Requirement:** `REQ-TICK10-015`

- **Given** EVSE is ready to charge in standalone mode with MaxCapacity=10A
- **When** A 9V pilot signal triggers the A-to-B transition
- **Then** BalancedMax[0] is set to 100 deciamps (MaxCapacity * 10)

> Test: `test_a_to_b_sets_balanced_max` in `test_tick_10ms.c:309`

### A-to-B transition does NOT set extra PWM duty (F3 fidelity fix)

**Requirement:** `REQ-TICK10-016`

- **Given** EVSE is ready to charge in standalone mode
- **When** A 9V pilot signal triggers the A-to-B transition
- **Then** PWM duty remains 1024 (from STATE_A entry); module does not set PWM on A→B,

> Test: `test_a_to_b_no_extra_pwm` in `test_tick_10ms.c:324`

### A-to-B transition initializes ActivationMode to 30 and clears AccessTimer

**Requirement:** `REQ-TICK10-017`

- **Given** EVSE is ready to charge in standalone mode
- **When** A 9V pilot signal triggers the A-to-B transition
- **Then** ActivationMode is set to 30 (countdown for activation) and AccessTimer is cleared to 0

> Test: `test_a_to_b_sets_activation_mode_30` in `test_tick_10ms.c:343`

### STATE_B1 remains in B1 when errors are present on 9V pilot

**Requirement:** `REQ-TICK10-018`

- **Given** EVSE is in STATE_B1 (connected but waiting) with TEMP_HIGH error flag set
- **When** A 9V pilot signal is received during a 10ms tick
- **Then** The EVSE stays in STATE_B1 because errors prevent transition to charging states

> Test: `test_b1_with_errors_stays_b1_on_9v` in `test_tick_10ms.c:360`

### Modem states are NOT handled in tick_10ms (matches original Timer10ms)

**Requirement:** `REQ-TICK10-019`

- **Given** EVSE is in one of the modem states (REQUEST, WAIT, DONE, or DENIED)
- **When** A 12V pilot signal (no vehicle) is received during a 10ms tick
- **Then** The EVSE stays in its modem state (modem is managed solely by tick_1s)

> Test: `test_modem_states_to_a_on_12v` in `test_tick_10ms.c:381`

### ACTSTART transitions to STATE_B when activation timer expires

**Requirement:** `REQ-TICK10-020`

- **Given** EVSE is in STATE_ACTSTART with ActivationTimer=0 (timer expired)
- **When** A 9V pilot signal is received during a 10ms tick
- **Then** The EVSE transitions to STATE_B and ActivationMode is set to 255 (disabled)

> Test: `test_actstart_to_b_when_timer_zero` in `test_tick_10ms.c:403`

---

## 1-Second Tick Processing

### SolarStopTimer decrements by one each second

**Requirement:** `REQ-TICK1S-001`

- **Given** EVSE is in normal mode with SolarStopTimer=3
- **When** A 1-second tick occurs
- **Then** SolarStopTimer decrements to 2

> Test: `test_solar_stop_timer_countdown` in `test_tick_1s.c:1`

### SolarStopTimer expiry triggers STATE_C to STATE_C1 transition

**Requirement:** `REQ-TICK1S-002`

- **Given** EVSE is in Smart mode in STATE_C with high mains load and SolarStopTimer=1
- **When** A 1-second tick decrements SolarStopTimer to 0
- **Then** The EVSE transitions to STATE_C1 (charging suspended) and LESS_6A error flag is set

> Test: `test_solar_stop_timer_triggers_c1` in `test_tick_1s.c:39`

### SolarStopTimer expiry does not trigger C1 when not in STATE_C

**Requirement:** `REQ-TICK1S-003`

- **Given** EVSE is in Smart mode in STATE_B (not charging) with SolarStopTimer=1
- **When** A 1-second tick decrements SolarStopTimer to 0
- **Then** The EVSE does not transition to STATE_C1 but LESS_6A error flag is still set

> Test: `test_solar_stop_timer_not_in_c` in `test_tick_1s.c:62`

### Node charge timers increment when node is in STATE_C

**Requirement:** `REQ-TICK1S-004`

- **Given** Node 0 is in STATE_C with IntTimer=5 and Timer=100
- **When** A 1-second tick occurs
- **Then** IntTimer increments to 6 and Timer increments to 101

> Test: `test_node_charge_timer_increments` in `test_tick_1s.c:86`

### Node charge timer resets when node is not in STATE_C

**Requirement:** `REQ-TICK1S-005`

- **Given** Node 0 is in STATE_B (connected but not charging) with IntTimer=20
- **When** A 1-second tick occurs
- **Then** IntTimer is reset to 0

> Test: `test_node_charge_timer_resets` in `test_tick_1s.c:104`

### Multiple node charge timers update independently based on each node state

**Requirement:** `REQ-TICK1S-006`

- **Given** Nodes 0 and 2 are in STATE_C (charging) and node 1 is in STATE_B, all with IntTimer=10
- **When** A 1-second tick occurs
- **Then** Nodes 0 and 2 increment to 11 while node 1 resets to 0

> Test: `test_multi_node_timers` in `test_tick_1s.c:120`

### MainsMeter timeout sets CT_NOCOMM error on node

**Requirement:** `REQ-TICK1S-007`

- **Given** EVSE is a node (LoadBl=2) with MainsMeterTimeout=0
- **When** A 1-second tick occurs
- **Then** CT_NOCOMM error flag is set indicating mains meter communication lost

> Test: `test_mains_meter_timeout_node` in `test_tick_1s.c:144`

### MainsMeter timeout counter decrements on node each second

**Requirement:** `REQ-TICK1S-008`

- **Given** EVSE is a node (LoadBl=3) with MainsMeterTimeout=5
- **When** A 1-second tick occurs
- **Then** MainsMeterTimeout decrements to 4

> Test: `test_mains_meter_node_countdown` in `test_tick_1s.c:161`

### LESS_6A error forces STATE_C to STATE_C1 via power unavailable

**Requirement:** `REQ-TICK1S-009`

- **Given** EVSE is in Smart mode in STATE_C with LESS_6A error flag set and high mains load
- **When** A 1-second tick occurs
- **Then** The EVSE transitions to STATE_C1 (charging suspended due to insufficient power)

> Test: `test_less_6a_enforces_power_unavailable` in `test_tick_1s.c:179`

### LESS_6A error sets ChargeDelay to CHARGEDELAY (60 seconds)

**Requirement:** `REQ-TICK1S-010`

- **Given** EVSE is in Smart mode in STATE_B1 with LESS_6A error flag set and ChargeDelay=0
- **When** A 1-second tick occurs
- **Then** ChargeDelay is set to CHARGEDELAY (60 seconds) to prevent rapid retry

> Test: `test_less_6a_sets_charge_delay` in `test_tick_1s.c:201`

### MaxSumMains timer decrements each second

**Requirement:** `REQ-TICK1S-011`

- **Given** EVSE has MaxSumMainsTimer=5
- **When** A 1-second tick occurs
- **Then** MaxSumMainsTimer decrements to 4

> Test: `test_maxsummains_timer_countdown` in `test_tick_1s.c:226`

### MaxSumMains timer expiry triggers STATE_C to STATE_C1 transition

**Requirement:** `REQ-TICK1S-012`

- **Given** EVSE is in Smart mode in STATE_C with high mains load and MaxSumMainsTimer=1
- **When** A 1-second tick decrements MaxSumMainsTimer to 0
- **Then** The EVSE transitions to STATE_C1 and LESS_6A error flag is set

> Test: `test_maxsummains_timer_triggers_c1` in `test_tick_1s.c:241`

### AccessTimer is cleared when EVSE is not in STATE_A

**Requirement:** `REQ-TICK1S-013`

- **Given** EVSE is in STATE_B with AccessTimer=30
- **When** A 1-second tick occurs
- **Then** AccessTimer is cleared to 0 because it is only relevant in STATE_A

> Test: `test_access_timer_cleared_not_in_a` in `test_tick_1s.c:265`

### EV meter timeout counter decrements each second

**Requirement:** `REQ-TICK1S-014`

- **Given** EVMeterType=1 (meter installed) with EVMeterTimeout=5
- **When** A 1-second tick occurs
- **Then** EVMeterTimeout decrements to 4

> Test: `test_ev_meter_timeout_countdown` in `test_tick_1s.c:283`

### EV meter timeout reaching zero sets EV_NOCOMM error

**Requirement:** `REQ-TICK1S-015`

- **Given** EVMeterType=1 with EVMeterTimeout=0 and no existing errors in Smart mode
- **When** A 1-second tick occurs
- **Then** EV_NOCOMM error flag is set indicating EV meter communication lost

> Test: `test_ev_meter_timeout_triggers_error` in `test_tick_1s.c:299`

### Activation timer decrements each second

**Requirement:** `REQ-TICK1S-016`

- **Given** EVSE has ActivationTimer=3
- **When** A 1-second tick occurs
- **Then** ActivationTimer decrements to 2

> Test: `test_activation_timer_countdown` in `test_tick_1s.c:319`

### ActivationMode counter decrements each second

**Requirement:** `REQ-TICK1S-017`

- **Given** EVSE has ActivationMode=10
- **When** A 1-second tick occurs
- **Then** ActivationMode decrements to 9

> Test: `test_activation_mode_countdown` in `test_tick_1s.c:336`

### ChargeDelay is overridden by LESS_6A enforcement after decrementing to zero

**Requirement:** `REQ-TICK1S-018`

- **Given** EVSE is in Smart mode in STATE_B1 with ChargeDelay=1 and LESS_6A error flag set
- **When** A 1-second tick decrements ChargeDelay to 0 then LESS_6A enforcement re-sets it
- **Then** ChargeDelay is set back to CHARGEDELAY (60 seconds) by LESS_6A enforcement

> Test: `test_charge_delay_overridden_by_less_6a` in `test_tick_1s.c:353`

### LESS_6A resets ChargeDelay to CHARGEDELAY every tick, even when non-zero

**Requirement:** `REQ-TICK1S-F2A`

- **Given** EVSE is in Smart mode in STATE_B1 with LESS_6A set and ChargeDelay=30
- **When** A 1-second tick occurs
- **Then** ChargeDelay is reset to CHARGEDELAY (60), not decremented to 29

> Test: `test_less_6a_resets_charge_delay_every_tick` in `test_tick_1s.c:378`

### LESS_6A prevents ChargeDelay from ever reaching zero

**Requirement:** `REQ-TICK1S-F2B`

- **Given** EVSE is in Smart mode in STATE_B1 with LESS_6A set and ChargeDelay=1
- **When** A 1-second tick occurs (ChargeDelay decrements to 0, then LESS_6A resets it)
- **Then** ChargeDelay is CHARGEDELAY (60), not 0

> Test: `test_less_6a_charge_delay_never_reaches_zero` in `test_tick_1s.c:400`

---
