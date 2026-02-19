# SmartEVSE-3 Traceability Report

**38 features** | **525 scenarios** | **525 with requirement IDs** | **100% coverage**

---

## Summary

| Feature | Scenarios | With Req ID | Coverage |
|---------|-----------|-------------|----------|
| Authorization & Access Control | 17 | 17 | 100% |
| Dual-EVSE Load Balancing | 23 | 23 | 100% |
| End-to-End Charging | 12 | 12 | 100% |
| Error Handling & Safety | 29 | 29 | 100% |
| Fidelity: DisconnectTimeCounter | 3 | 3 | 100% |
| Fidelity: PilotDisconnectTime | 2 | 2 | 100% |
| Fidelity: Fall-through behavior | 5 | 5 | 100% |
| Fidelity: ACTSTART no pilot check | 2 | 2 | 100% |
| Fidelity: Modem states not in tick_10ms | 1 | 1 | 100% |
| Fidelity: Handler ordering | 1 | 1 | 100% |
| Fidelity: Config field | 2 | 2 | 100% |
| HTTP API Color Parsing | 3 | 3 | 100% |
| HTTP API Input Validation | 24 | 24 | 100% |
| HTTP API Validation | 23 | 23 | 100% |
| HTTP API Settings Validation | 7 | 7 | 100% |
| LED Status Indication | 15 | 15 | 100% |
| LED Color Configuration | 4 | 4 | 100% |
| Load Balancing | 15 | 15 | 100% |
| Meter Timeout & Recovery | 12 | 12 | 100% |
| Modem / ISO15118 Negotiation | 29 | 29 | 100% |
| MQTT Command Parsing | 27 | 27 | 100% |
| MQTT Input Validation | 29 | 29 | 100% |
| MQTT Meter Parsing | 6 | 6 | 100% |
| MQTT Color Parsing | 4 | 4 | 100% |
| Multi-Node Load Balancing | 14 | 14 | 100% |
| OCPP Current Limiting | 12 | 12 | 100% |
| Operating Modes | 21 | 21 | 100% |
| Phase Switching | 12 | 12 | 100% |
| Power Availability | 22 | 22 | 100% |
| Priority-Based Power Scheduling | 29 | 29 | 100% |
| Serial Message Parsing | 9 | 9 | 100% |
| Serial Input Validation | 10 | 10 | 100% |
| Battery Current Calculation | 7 | 7 | 100% |
| Current Sum Calculation | 5 | 5 | 100% |
| Solar Balancing | 20 | 20 | 100% |
| IEC 61851-1 State Transitions | 29 | 29 | 100% |
| 10ms Tick Processing | 20 | 20 | 100% |
| 1-Second Tick Processing | 20 | 20 | 100% |
| **TOTAL** | **525** | **525** | **100%** |

## Authorization & Access Control

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-AUTH-001` | Setting access to ON stores the authorization status | `test_set_access_on` | `test_authorization.c:1` |
| `REQ-AUTH-002` | Setting access to OFF stores the authorization status | `test_set_access_off` | `test_authorization.c:40` |
| `REQ-AUTH-003` | Revoking access during charging suspends the session (C -> C1) | `test_set_access_off_from_C_goes_C1` | `test_authorization.c:54` |
| `REQ-AUTH-004` | Pausing access during charging suspends the session (C -> C1) | `test_set_access_pause_from_C_goes_C1` | `test_authorization.c:73` |
| `REQ-AUTH-005` | Revoking access in STATE_B moves to waiting state (B -> B1) | `test_set_access_off_from_B_goes_B1` | `test_authorization.c:91` |
| `REQ-AUTH-006` | Revoking access during modem request aborts to B1 | `test_set_access_off_from_modem_request_goes_B1` | `test_authorization.c:108` |
| `REQ-AUTH-007` | Revoking access during modem wait aborts to B1 | `test_set_access_off_from_modem_wait_goes_B1` | `test_authorization.c:125` |
| `REQ-AUTH-008` | Revoking access in STATE_A has no state transition side effect | `test_set_access_off_from_A_stays_A` | `test_authorization.c:142` |
| `REQ-AUTH-009` | Granting access in STATE_B1 does not auto-recover to STATE_B | `test_set_access_on_from_B1_does_not_auto_recover` | `test_authorization.c:157` |
| `REQ-AUTH-010` | OCPP current limit below MinCurrent blocks current availability | `test_ocpp_blocks_current_availability` | `test_authorization.c:175` |
| `REQ-AUTH-011` | OCPP current limit above MinCurrent allows current availability | `test_ocpp_allows_when_limit_sufficient` | `test_authorization.c:192` |
| `REQ-AUTH-012` | Negative OCPP current limit is ignored (not set) | `test_ocpp_negative_limit_ignored` | `test_authorization.c:210` |
| `REQ-AUTH-014` | Access timer counts down each second while in STATE_A | `test_access_timer_counts_down_in_state_A` | `test_authorization.c:229` |
| `REQ-AUTH-015` | Access timer expiry revokes authorization | `test_access_timer_expires_turns_off` | `test_authorization.c:266` |
| `REQ-AUTH-016` | Access timer is cleared when EVSE is not in STATE_A | `test_access_timer_cleared_when_not_in_A` | `test_authorization.c:284` |
| `REQ-AUTH-017` | No STATE_A to STATE_B transition without authorization | `test_no_A_to_B_without_access` | `test_authorization.c:303` |
| `REQ-AUTH-018` | No STATE_B to STATE_C transition after access revoked mid-session | `test_no_B_to_C_without_access` | `test_authorization.c:318` |

<details>
<summary>Detailed steps (17 scenarios)</summary>

### Setting access to ON stores the authorization status
**Requirement:** `REQ-AUTH-001`

- **Given** The EVSE is initialised in basic configuration
- **When** evse_set_access is called with ON
- **Then** AccessStatus is set to ON

### Setting access to OFF stores the authorization status
**Requirement:** `REQ-AUTH-002`

- **Given** The EVSE is initialised in basic configuration
- **When** evse_set_access is called with OFF
- **Then** AccessStatus is set to OFF

### Revoking access during charging suspends the session (C -> C1)
**Requirement:** `REQ-AUTH-003`

- **Given** The EVSE is in STATE_C (charging) with AccessStatus ON
- **When** evse_set_access is called with OFF
- **Then** The state transitions to STATE_C1 and AccessStatus is OFF

### Pausing access during charging suspends the session (C -> C1)
**Requirement:** `REQ-AUTH-004`

- **Given** The EVSE is in STATE_C (charging) with AccessStatus ON
- **When** evse_set_access is called with PAUSE
- **Then** The state transitions to STATE_C1 and AccessStatus is PAUSE

### Revoking access in STATE_B moves to waiting state (B -> B1)
**Requirement:** `REQ-AUTH-005`

- **Given** The EVSE is in STATE_B (connected) with AccessStatus ON
- **When** evse_set_access is called with OFF
- **Then** The state transitions to STATE_B1 (waiting)

### Revoking access during modem request aborts to B1
**Requirement:** `REQ-AUTH-006`

- **Given** The EVSE is in STATE_MODEM_REQUEST with AccessStatus ON
- **When** evse_set_access is called with OFF
- **Then** The state transitions to STATE_B1 (waiting)

### Revoking access during modem wait aborts to B1
**Requirement:** `REQ-AUTH-007`

- **Given** The EVSE is in STATE_MODEM_WAIT with AccessStatus ON
- **When** evse_set_access is called with OFF
- **Then** The state transitions to STATE_B1 (waiting)

### Revoking access in STATE_A has no state transition side effect
**Requirement:** `REQ-AUTH-008`

- **Given** The EVSE is in STATE_A (disconnected)
- **When** evse_set_access is called with OFF
- **Then** The state remains STATE_A

### Granting access in STATE_B1 does not auto-recover to STATE_B
**Requirement:** `REQ-AUTH-009`

- **Given** The EVSE is in STATE_B1 (waiting)
- **When** evse_set_access is called with ON
- **Then** The state remains STATE_B1 (no automatic recovery)

### OCPP current limit below MinCurrent blocks current availability
**Requirement:** `REQ-AUTH-010`

- **Given** OCPP mode is enabled with OcppCurrentLimit=3A and MinCurrent=6A
- **When** evse_is_current_available is called
- **Then** The function returns 0 (current not available)

### OCPP current limit above MinCurrent allows current availability
**Requirement:** `REQ-AUTH-011`

- **Given** OCPP mode is enabled with OcppCurrentLimit=10A and MinCurrent=6A
- **When** evse_is_current_available is called
- **Then** The function returns 1 (current available)

### Negative OCPP current limit is ignored (not set)
**Requirement:** `REQ-AUTH-012`

- **Given** OCPP mode is enabled with OcppCurrentLimit=-1 (unset)
- **When** evse_is_current_available is called
- **Then** The function returns 1 (limit not applied)

### Access timer counts down each second while in STATE_A
**Requirement:** `REQ-AUTH-014`

- **Given** The EVSE has AccessStatus ON, RFIDReader=2, and AccessTimer=0
- **Given** The EVSE is in STATE_A with AccessTimer=5
- **When** A 12V pilot signal is received (EV disconnects)
- **When** One second tick occurs
- **Then** AccessTimer is set to RFIDLOCKTIME
- **Then** AccessTimer decrements to 4

### Access timer expiry revokes authorization
**Requirement:** `REQ-AUTH-015`

- **Given** The EVSE is in STATE_A with AccessTimer=1 and AccessStatus ON
- **When** One second tick occurs (timer reaches 0)
- **Then** AccessStatus is set to OFF (authorization revoked)

### Access timer is cleared when EVSE is not in STATE_A
**Requirement:** `REQ-AUTH-016`

- **Given** The EVSE is in STATE_B with AccessTimer=30
- **When** One second tick occurs
- **Then** AccessTimer is reset to 0

### No STATE_A to STATE_B transition without authorization
**Requirement:** `REQ-AUTH-017`

- **Given** The EVSE is in STATE_A with AccessStatus OFF
- **When** A 9V pilot signal is received (vehicle connected)
- **Then** The state remains STATE_A (transition blocked)

### No STATE_B to STATE_C transition after access revoked mid-session
**Requirement:** `REQ-AUTH-018`

- **Given** The EVSE is in STATE_B with DiodeCheck passed but AccessStatus revoked to OFF
- **When** A 6V pilot signal is sustained for 500ms
- **Then** The state does NOT transition to STATE_C

</details>

---

## Dual-EVSE Load Balancing

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-DUAL-S1A` | Both EVSEs get equal share of MaxCircuit in Normal mode | `test_s1_both_start_equal_split` | `test_dual_evse.c:1` |
| `REQ-DUAL-S1B` | IsetBalanced equals MaxCircuit * 10 in Normal mode | `test_s1_isetbalanced_equals_max_circuit` | `test_dual_evse.c:67` |
| `REQ-DUAL-S1C` | EV meter baseload reduces available current | `test_s1_ev_meter_baseload` | `test_dual_evse.c:85` |
| `REQ-DUAL-S2A` | Master reduces current when slave joins | `test_s2_slave_joins_master_reduces` | `test_dual_evse.c:112` |
| `REQ-DUAL-S2B` | Slave node sends COMM_B on car connect | `test_s2_slave_sends_comm_b` | `test_dual_evse.c:145` |
| `REQ-DUAL-S3A` | MaxCircuit reduction redistributes current equally | `test_s3_maxcircuit_reduction` | `test_dual_evse.c:170` |
| `REQ-DUAL-S3B` | MaxCircuit at exactly 2 * MinCurrent gives each MinCurrent | `test_s3_maxcircuit_to_mincurrent` | `test_dual_evse.c:191` |
| `REQ-DUAL-S4A` | Slave disconnects, master gets capped at BalancedMax | `test_s4_slave_disconnects` | `test_dual_evse.c:215` |
| `REQ-DUAL-S4B` | Master with higher MaxCurrent gets more after slave disconnect | `test_s4_master_absorbs_full_capacity` | `test_dual_evse.c:240` |
| `REQ-DUAL-S5A` | Smart mode: new EVSE joining recalculates from mains headroom | `test_s5_smart_mode_new_join` | `test_dual_evse.c:273` |
| `REQ-DUAL-S5B` | Smart mode: surplus increases IsetBalanced gradually | `test_s5_smart_surplus_increases` | `test_dual_evse.c:297` |
| `REQ-DUAL-S5C` | Smart mode: overload decreases IsetBalanced immediately | `test_s5_smart_overload_decreases` | `test_dual_evse.c:321` |
| `REQ-DUAL-S6A` | Solar mode: both EVSEs in startup get MinCurrent | `test_s6_solar_both_in_startup` | `test_dual_evse.c:348` |
| `REQ-DUAL-S6B` | Solar mode: insufficient solar starts SolarStopTimer | `test_s6_solar_insufficient_starts_timer` | `test_dual_evse.c:376` |
| `REQ-DUAL-S7A` | Zero available power pauses all EVSEs via priority scheduling | `test_s7_mincurrent_violation` | `test_dual_evse.c:412` |
| `REQ-DUAL-S7B` | Exactly 2 * MinCurrent — no shortage | `test_s7_barely_enough` | `test_dual_evse.c:440` |
| `REQ-DUAL-S8A` | Slave error → master absorbs capacity | `test_s8_slave_error_master_absorbs` | `test_dual_evse.c:465` |
| `REQ-DUAL-S8B` | Slave recovers, current redistributed | `test_s8_slave_recovers` | `test_dual_evse.c:488` |
| `REQ-DUAL-S9A` | MaxSumMains overridden Idifference limits IsetBalanced | `test_s9_maxsummains_limits` | `test_dual_evse.c:520` |
| `REQ-DUAL-S9B` | MaxSumMains timer expiry stops charging | `test_s9_maxsummains_timer_expiry` | `test_dual_evse.c:546` |
| `REQ-DUAL-S10A` | Normal mode forces 3P when currently on 1P | `test_s10_normal_forces_3p` | `test_dual_evse.c:578` |
| `REQ-DUAL-S10B` | STATE_C entry applies 1P switch | `test_s10_state_c_applies_1p` | `test_dual_evse.c:597` |
| `REQ-DUAL-S10C` | Smart mode with AUTO forces back to 3P | `test_s10_smart_auto_forces_3p` | `test_dual_evse.c:617` |

<details>
<summary>Detailed steps (23 scenarios)</summary>

### Both EVSEs get equal share of MaxCircuit in Normal mode
**Requirement:** `REQ-DUAL-S1A`

- **Given** Two EVSEs both in STATE_C, Normal mode, MaxCircuit=32A
- **When** evse_calc_balanced_current(mod=1) is called
- **Then** Each EVSE gets 160 (16A), equal split of 320 IsetBalanced

### IsetBalanced equals MaxCircuit * 10 in Normal mode
**Requirement:** `REQ-DUAL-S1B`

- **Given** Two EVSEs in Normal mode
- **When** evse_calc_balanced_current(mod=1)
- **Then** IsetBalanced = MaxCircuit * 10 = 320

### EV meter baseload reduces available current
**Requirement:** `REQ-DUAL-S1C`

- **Given** Two EVSEs, EV meter reads 200 (other loads on circuit)
- **When** evse_calc_balanced_current(mod=1)
- **Then** IsetBalanced reduced by baseload

### Master reduces current when slave joins
**Requirement:** `REQ-DUAL-S2A`

- **Given** Master alone at 160, slave not active
- **When** Slave enters STATE_C and calc runs with mod=1
- **Then** Both get equal share (160 each with MaxCircuit=32)

### Slave node sends COMM_B on car connect
**Requirement:** `REQ-DUAL-S2B`

- **Given** Slave EVSE (LoadBl=2) in STATE_A with 9V pilot
- **When** tick_10ms with PILOT_9V
- **Then** Transitions to STATE_COMM_B (requests master permission)

### MaxCircuit reduction redistributes current equally
**Requirement:** `REQ-DUAL-S3A`

- **Given** Both at 160 with MaxCircuit=32
- **When** MaxCircuit reduced to 20
- **Then** Each gets 100 (200 / 2)

### MaxCircuit at exactly 2 * MinCurrent gives each MinCurrent
**Requirement:** `REQ-DUAL-S3B`

- **Given** Both in STATE_C, MaxCircuit=12
- **When** evse_calc_balanced_current
- **Then** Each gets exactly 60 (MinCurrent * 10)

### Slave disconnects, master gets capped at BalancedMax
**Requirement:** `REQ-DUAL-S4A`

- **Given** Both at 160, slave disconnects (STATE_A)
- **When** Recalculation runs
- **Then** Master gets BalancedMax[0]=160, slave gets 0

### Master with higher MaxCurrent gets more after slave disconnect
**Requirement:** `REQ-DUAL-S4B`

- **Given** Master MaxCurrent=32, MaxCircuit=40, slave disconnects
- **When** Recalculation with mod=0
- **Then** Master gets up to 320 (BalancedMax[0]=ChargeCurrent=320)

### Smart mode: new EVSE joining recalculates from mains headroom
**Requirement:** `REQ-DUAL-S5A`

- **Given** Master in MODE_SMART, both in STATE_C, MaxMains=32A
- **When** evse_calc_balanced_current(mod=1)
- **Then** IsetBalanced based on (MaxMains*10) - Baseload

### Smart mode: surplus increases IsetBalanced gradually
**Requirement:** `REQ-DUAL-S5B`

- **Given** MODE_SMART, low mains load, IsetBalanced=200
- **When** evse_calc_balanced_current(mod=0)
- **Then** IsetBalanced increases by Idifference/4

### Smart mode: overload decreases IsetBalanced immediately
**Requirement:** `REQ-DUAL-S5C`

- **Given** MODE_SMART, high mains load
- **When** evse_calc_balanced_current(mod=0)
- **Then** IsetBalanced decreases by full Idifference

### Solar mode: both EVSEs in startup get MinCurrent
**Requirement:** `REQ-DUAL-S6A`

- **Given** MODE_SOLAR, both in STATE_C with IntTimer < SOLARSTARTTIME
- **When** evse_calc_balanced_current
- **Then** Both receive exactly MinCurrent * 10 = 60

### Solar mode: insufficient solar starts SolarStopTimer
**Requirement:** `REQ-DUAL-S6B`

- **Given** MODE_SOLAR, high grid import, past startup
- **When** evse_calc_balanced_current
- **Then** SolarStopTimer is set

### Zero available power pauses all EVSEs via priority scheduling
**Requirement:** `REQ-DUAL-S7A`

- **Given** Smart mode, MaxMains=5A, heavy mains load (IsetBalanced drops to 0)
- **When** evse_calc_balanced_current
- **Then** Both EVSEs paused (Balanced=0), NoCurrent increments (true hard shortage)

### Exactly 2 * MinCurrent — no shortage
**Requirement:** `REQ-DUAL-S7B`

- **Given** Normal mode, MaxCircuit=12
- **When** evse_calc_balanced_current
- **Then** Each gets 60, NoCurrent stays 0

### Slave error → master absorbs capacity
**Requirement:** `REQ-DUAL-S8A`

- **Given** Both at 160 each, slave enters B1
- **When** Recalculation
- **Then** Master gets 160 (capped by BalancedMax)

### Slave recovers, current redistributed
**Requirement:** `REQ-DUAL-S8B`

- **Given** Master alone at 160, slave re-enters STATE_C
- **When** evse_calc_balanced_current(mod=1)
- **Then** Both get equal share

### MaxSumMains overridden Idifference limits IsetBalanced
**Requirement:** `REQ-DUAL-S9A`

- **Given** MODE_SMART, MaxSumMains=30, Isum close to limit
- **When** evse_calc_balanced_current(mod=0)
- **Then** IsetBalanced constrained by MaxSumMains

### MaxSumMains timer expiry stops charging
**Requirement:** `REQ-DUAL-S9B`

- **Given** STATE_C with MaxSumMainsTimer=1
- **When** evse_tick_1s (timer expires)
- **Then** C → C1, LESS_6A set

### Normal mode forces 3P when currently on 1P
**Requirement:** `REQ-DUAL-S10A`

- **Given** Normal mode, Nr_Of_Phases_Charging=1
- **When** evse_calc_balanced_current
- **Then** Switching_Phases_C2 = GOING_TO_SWITCH_3P

### STATE_C entry applies 1P switch
**Requirement:** `REQ-DUAL-S10B`

- **Given** Switching_Phases_C2=GOING_TO_SWITCH_1P, EnableC2=ALWAYS_OFF
- **When** evse_set_state(STATE_C)
- **Then** Nr_Of_Phases_Charging=1, contactor2 off

### Smart mode with AUTO forces back to 3P
**Requirement:** `REQ-DUAL-S10C`

- **Given** MODE_SMART, EnableC2=AUTO, Nr_Of_Phases_Charging=1
- **When** evse_check_switching_phases
- **Then** Switching_Phases_C2 = GOING_TO_SWITCH_3P

</details>

---

## End-to-End Charging

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-E2E-001` | Complete standalone charge cycle with DiodeCheck | `test_e2e_standalone_happy_path` | `test_e2e_charging.c:1` |
| `REQ-E2E-002` | Slave EVSE full charge cycle with master handshake | `test_e2e_slave_happy_path` | `test_e2e_charging.c:99` |
| `REQ-E2E-003` | OCPP grants access mid-session, car starts charging | `test_e2e_ocpp_authorization_flow` | `test_e2e_charging.c:152` |
| `REQ-E2E-004` | OCPP denied — car connects but stays in STATE_A | `test_e2e_ocpp_denied_stays_in_a` | `test_e2e_charging.c:200` |
| `REQ-E2E-005` | Two full charge cycles — verify no stale state leaks | `test_e2e_reconnect_after_disconnect` | `test_e2e_charging.c:224` |
| `REQ-E2E-006` | Temperature spike during STATE_C, recovery after cooldown | `test_e2e_temp_error_during_charge` | `test_e2e_charging.c:279` |
| `REQ-E2E-007` | Meter communication lost during STATE_C | `test_e2e_ct_nocomm_during_charge` | `test_e2e_charging.c:318` |
| `REQ-E2E-008` | TEMP_HIGH + CT_NOCOMM simultaneously during charge | `test_e2e_multiple_errors_during_charge` | `test_e2e_charging.c:355` |
| `REQ-E2E-009` | 6V pilot without DiodeCheck does not transition to STATE_C | `test_e2e_no_charge_without_diode` | `test_e2e_charging.c:393` |
| `REQ-E2E-010` | ChargeDelay > 0 blocks A→B transition, sends to B1 | `test_e2e_charge_delay_blocks_charging` | `test_e2e_charging.c:420` |
| `REQ-E2E-011` | StateTimer is properly reset between charge sessions | `test_e2e_state_timer_reset_on_c_to_b` | `test_e2e_charging.c:446` |
| `REQ-E2E-012` | Power unavailable during charge suspends charging | `test_e2e_power_unavailable_c_to_c1_to_b1` | `test_e2e_charging.c:488` |

<details>
<summary>Detailed steps (12 scenarios)</summary>

### Complete standalone charge cycle with DiodeCheck
**Requirement:** `REQ-E2E-001`

- **Given** Standalone EVSE, authorized, Normal mode
- **When** Car connects (9V) → DiodeCheck (DIODE) → requests charge (6V) → stops (9V) → disconnects (12V)
- **Then** Full cycle: A → B → (DiodeCheck) → C → B → A with correct contactor states

### Slave EVSE full charge cycle with master handshake
**Requirement:** `REQ-E2E-002`

- **Given** Slave EVSE (LoadBl=2), authorized
- **When** Car connects → COMM_B → master approves → B → DiodeCheck → 6V → COMM_C → master approves → C → disconnect
- **Then** Full slave cycle: A → COMM_B → COMM_B_OK → B → COMM_C → COMM_C_OK → C → B → A

### OCPP grants access mid-session, car starts charging
**Requirement:** `REQ-E2E-003`

- **Given** Standalone EVSE with OcppMode=true, AccessStatus=OFF
- **When** Car connects, OCPP grants access, car charges, OCPP revokes
- **Then** A (blocked) → A (OCPP grants) → B → C → C1 (revoked) → B1

### OCPP denied — car connects but stays in STATE_A
**Requirement:** `REQ-E2E-004`

- **Given** OCPP mode with AccessStatus=OFF
- **When** Car repeatedly presents 9V pilot
- **Then** State stays A for 100+ ticks

### Two full charge cycles — verify no stale state leaks
**Requirement:** `REQ-E2E-005`

- **Given** Standalone EVSE
- **When** First cycle: connect → charge → disconnect. Second cycle: connect → charge → disconnect.
- **Then** Both cycles complete successfully, all state is clean between them

### Temperature spike during STATE_C, recovery after cooldown
**Requirement:** `REQ-E2E-006`

- **Given** EVSE charging in STATE_C
- **When** Temperature exceeds maxTemp, then cools below hysteresis
- **Then** C → C1 → B1 (TEMP_HIGH), cooldown clears error

### Meter communication lost during STATE_C
**Requirement:** `REQ-E2E-007`

- **Given** EVSE charging in Smart mode with MainsMeter
- **When** Meter timeout reaches 0
- **Then** CT_NOCOMM set, power unavailable, C → C1

### TEMP_HIGH + CT_NOCOMM simultaneously during charge
**Requirement:** `REQ-E2E-008`

- **Given** EVSE charging in Smart mode
- **When** Temperature spikes AND meter fails at same time
- **Then** Both errors set, both must clear for full recovery

### 6V pilot without DiodeCheck does not transition to STATE_C
**Requirement:** `REQ-E2E-009`

- **Given** EVSE in STATE_B, DiodeCheck=0
- **When** 55 ticks of 6V pilot
- **Then** State stays STATE_B (DiodeCheck blocks B→C)

### ChargeDelay > 0 blocks A→B transition, sends to B1
**Requirement:** `REQ-E2E-010`

- **Given** Standalone EVSE with ChargeDelay=10
- **When** Car connects (9V)
- **Then** Goes to B1 (not B), must wait for delay to expire

### StateTimer is properly reset between charge sessions
**Requirement:** `REQ-E2E-011`

- **Given** EVSE in STATE_C with accumulated StateTimer
- **When** Car stops (9V → B), then requests charge again (6V)
- **Then** Debounce starts from 0, requiring full 500ms

### Power unavailable during charge suspends charging
**Requirement:** `REQ-E2E-012`

- **Given** EVSE charging in STATE_C
- **When** evse_set_power_unavailable is called
- **Then** C → C1 (PWM off) → B1 (contactors open after C1Timer)

</details>

---

## Error Handling & Safety

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-ERR-001` | Setting an error flag stores it in ErrorFlags | `test_set_error_flags` | `test_error_handling.c:1` |
| `REQ-ERR-002` | Multiple error flags can be set simultaneously | `test_set_multiple_error_flags` | `test_error_handling.c:46` |
| `REQ-ERR-003` | Clearing an error flag removes only the specified flag | `test_clear_error_flags` | `test_error_handling.c:62` |
| `REQ-ERR-004` | Clearing one flag preserves all other active flags | `test_clear_preserves_other_flags` | `test_error_handling.c:78` |
| `REQ-ERR-005` | ChargeDelay decrements each second | `test_charge_delay_counts_down` | `test_error_handling.c:97` |
| `REQ-ERR-006` | ChargeDelay does not underflow past zero | `test_charge_delay_stops_at_zero` | `test_error_handling.c:112` |
| `REQ-ERR-008` | Temperature exceeding maxTemp triggers TEMP_HIGH error | `test_temp_high_triggers_error` | `test_error_handling.c:129` |
| `REQ-ERR-009` | Overtemperature shuts down active charging session | `test_temp_high_shuts_down_charging` | `test_error_handling.c:165` |
| `REQ-ERR-010` | Temperature recovery requires 10-degree hysteresis below maxTemp | `test_temp_recovery_with_hysteresis` | `test_error_handling.c:182` |
| `REQ-ERR-011` | Temperature recovery boundary: exactly at threshold does not clear | `test_temp_recovery_boundary` | `test_error_handling.c:206` |
| `REQ-ERR-012` | Mains meter communication timeout sets CT_NOCOMM error | `test_mains_meter_timeout_sets_ct_nocomm` | `test_error_handling.c:232` |
| `REQ-ERR-013` | Mains meter timeout counter decrements each second | `test_mains_meter_timeout_counts_down` | `test_error_handling.c:250` |
| `REQ-ERR-014` | Normal mode ignores mains meter timeout (no CT_NOCOMM) | `test_mains_meter_normal_mode_ignores_timeout` | `test_error_handling.c:268` |
| `REQ-ERR-015` | No mains meter configured resets timeout to COMM_TIMEOUT | `test_no_mains_meter_resets_timeout` | `test_error_handling.c:287` |
| `REQ-ERR-016` | EV meter communication timeout sets EV_NOCOMM error | `test_ev_meter_timeout_sets_ev_nocomm` | `test_error_handling.c:306` |
| `REQ-ERR-017` | No EV meter configured resets timeout to COMM_EVTIMEOUT | `test_no_ev_meter_resets_timeout` | `test_error_handling.c:323` |
| `REQ-ERR-018` | CT_NOCOMM error clears when mains meter communication resumes | `test_ct_nocomm_recovers_on_communication` | `test_error_handling.c:341` |
| `REQ-ERR-019` | EV_NOCOMM error clears when EV meter communication resumes | `test_ev_nocomm_recovers_on_communication` | `test_error_handling.c:357` |
| `REQ-ERR-020` | LESS_6A error auto-recovers when sufficient current becomes available | `test_less_6a_recovers_when_current_available` | `test_error_handling.c:375` |
| `REQ-ERR-021` | LESS_6A error persists when current is still unavailable | `test_less_6a_stays_when_current_unavailable` | `test_error_handling.c:394` |
| `REQ-ERR-022` | Node EVSEs (LoadBl >= 2) do not auto-recover LESS_6A | `test_less_6a_no_recovery_for_nodes` | `test_error_handling.c:414` |
| `REQ-ERR-023` | Power unavailable during charging suspends to STATE_C1 | `test_power_unavailable_from_C_goes_C1` | `test_error_handling.c:434` |
| `REQ-ERR-024` | Power unavailable in STATE_B moves to waiting state B1 | `test_power_unavailable_from_B_goes_B1` | `test_error_handling.c:448` |
| `REQ-ERR-025` | Power unavailable in STATE_A has no effect | `test_power_unavailable_from_A_stays_A` | `test_error_handling.c:464` |
| `REQ-ERR-026` | Power unavailable in STATE_B1 remains in B1 (already waiting) | `test_power_unavailable_from_B1_stays_B1` | `test_error_handling.c:479` |
| `REQ-ERR-027` | Power unavailable in STATE_C1 remains in C1 (already suspended) | `test_power_unavailable_from_C1_stays_C1` | `test_error_handling.c:494` |
| `REQ-ERR-028` | Entering STATE_B1 disconnects pilot when authorized | `test_pilot_disconnect_on_B1_entry` | `test_error_handling.c:511` |
| `REQ-ERR-029` | Pilot reconnects after PilotDisconnectTime expires | `test_pilot_reconnect_after_timer` | `test_error_handling.c:528` |
| `REQ-ERR-030` | MaxSumMains timer expiry stops charging with LESS_6A error | `test_maxsummains_timer_stops_charging` | `test_error_handling.c:560` |

<details>
<summary>Detailed steps (29 scenarios)</summary>

### Setting an error flag stores it in ErrorFlags
**Requirement:** `REQ-ERR-001`

- **Given** The EVSE is initialised with no errors
- **When** evse_set_error_flags is called with TEMP_HIGH
- **Then** TEMP_HIGH bit is set in ErrorFlags

### Multiple error flags can be set simultaneously
**Requirement:** `REQ-ERR-002`

- **Given** The EVSE is initialised with no errors
- **When** evse_set_error_flags is called with TEMP_HIGH then CT_NOCOMM
- **Then** Both TEMP_HIGH and CT_NOCOMM bits are set in ErrorFlags

### Clearing an error flag removes only the specified flag
**Requirement:** `REQ-ERR-003`

- **Given** The EVSE has TEMP_HIGH and CT_NOCOMM error flags set
- **When** evse_clear_error_flags is called with TEMP_HIGH
- **Then** TEMP_HIGH is cleared but CT_NOCOMM remains set

### Clearing one flag preserves all other active flags
**Requirement:** `REQ-ERR-004`

- **Given** The EVSE has TEMP_HIGH, LESS_6A, and CT_NOCOMM error flags set
- **When** evse_clear_error_flags is called with LESS_6A
- **Then** TEMP_HIGH and CT_NOCOMM remain set, LESS_6A is cleared

### ChargeDelay decrements each second
**Requirement:** `REQ-ERR-005`

- **Given** The EVSE has ChargeDelay set to 10
- **When** One second tick occurs
- **Then** ChargeDelay decrements to 9

### ChargeDelay does not underflow past zero
**Requirement:** `REQ-ERR-006`

- **Given** The EVSE has ChargeDelay set to 1
- **When** Two second ticks occur
- **Then** ChargeDelay reaches 0 and stays at 0

### Temperature exceeding maxTemp triggers TEMP_HIGH error
**Requirement:** `REQ-ERR-008`

- **Given** The EVSE is in STATE_A with ChargeDelay=5 and AccessStatus ON
- **Given** The EVSE is charging with TempEVSE=70 and maxTemp=65
- **When** A 9V pilot signal is received (vehicle connected)
- **When** One second tick occurs
- **Then** The state transitions to STATE_B1 instead of STATE_B
- **Then** TEMP_HIGH error flag is set in ErrorFlags

### Overtemperature shuts down active charging session
**Requirement:** `REQ-ERR-009`

- **Given** The EVSE is in STATE_C (charging) with TempEVSE=70 and maxTemp=65
- **When** One second tick occurs triggering temperature protection
- **Then** The state transitions out of STATE_C (charging suspended)

### Temperature recovery requires 10-degree hysteresis below maxTemp
**Requirement:** `REQ-ERR-010`

- **Given** The EVSE has TEMP_HIGH error with maxTemp=65
- **When** Temperature drops to 60 (within hysteresis) then to 50 (below threshold)
- **Then** TEMP_HIGH persists at 60 but clears at 50 (below maxTemp-10)

### Temperature recovery boundary: exactly at threshold does not clear
**Requirement:** `REQ-ERR-011`

- **Given** The EVSE has TEMP_HIGH error with maxTemp=65
- **When** Temperature is exactly 55 (maxTemp-10) then drops to 54
- **Then** TEMP_HIGH persists at 55 but clears at 54 (strictly below threshold)

### Mains meter communication timeout sets CT_NOCOMM error
**Requirement:** `REQ-ERR-012`

- **Given** The EVSE is in MODE_SMART with MainsMeterTimeout=0 (timed out)
- **When** One second tick occurs
- **Then** CT_NOCOMM error flag is set

### Mains meter timeout counter decrements each second
**Requirement:** `REQ-ERR-013`

- **Given** The EVSE is in MODE_SMART with MainsMeterTimeout=5
- **When** One second tick occurs
- **Then** MainsMeterTimeout decrements to 4

### Normal mode ignores mains meter timeout (no CT_NOCOMM)
**Requirement:** `REQ-ERR-014`

- **Given** The EVSE is in MODE_NORMAL with MainsMeterTimeout=0
- **When** One second tick occurs
- **Then** CT_NOCOMM error flag is NOT set

### No mains meter configured resets timeout to COMM_TIMEOUT
**Requirement:** `REQ-ERR-015`

- **Given** The EVSE has MainsMeterType=0 (no meter) with MainsMeterTimeout=3
- **When** One second tick occurs
- **Then** MainsMeterTimeout is reset to COMM_TIMEOUT

### EV meter communication timeout sets EV_NOCOMM error
**Requirement:** `REQ-ERR-016`

- **Given** The EVSE has EVMeterType=1 with EVMeterTimeout=0 (timed out)
- **When** One second tick occurs
- **Then** EV_NOCOMM error flag is set

### No EV meter configured resets timeout to COMM_EVTIMEOUT
**Requirement:** `REQ-ERR-017`

- **Given** The EVSE has EVMeterType=0 (no meter) with EVMeterTimeout=3
- **When** One second tick occurs
- **Then** EVMeterTimeout is reset to COMM_EVTIMEOUT

### CT_NOCOMM error clears when mains meter communication resumes
**Requirement:** `REQ-ERR-018`

- **Given** The EVSE has CT_NOCOMM error with MainsMeterTimeout=5 (restored)
- **When** One second tick occurs
- **Then** CT_NOCOMM error flag is cleared

### EV_NOCOMM error clears when EV meter communication resumes
**Requirement:** `REQ-ERR-019`

- **Given** The EVSE has EV_NOCOMM error with EVMeterTimeout=5 (restored)
- **When** One second tick occurs
- **Then** EV_NOCOMM error flag is cleared

### LESS_6A error auto-recovers when sufficient current becomes available
**Requirement:** `REQ-ERR-020`

- **Given** The EVSE is in MODE_NORMAL standalone with LESS_6A error and AccessStatus ON
- **When** One second tick occurs (normal mode always has current available)
- **Then** LESS_6A error flag is cleared

### LESS_6A error persists when current is still unavailable
**Requirement:** `REQ-ERR-021`

- **Given** The EVSE is in MODE_SMART with LESS_6A error and mains heavily loaded
- **When** One second tick occurs
- **Then** LESS_6A error flag remains set

### Node EVSEs (LoadBl >= 2) do not auto-recover LESS_6A
**Requirement:** `REQ-ERR-022`

- **Given** The EVSE is configured as a node (LoadBl=3) with LESS_6A error
- **When** One second tick occurs
- **Then** LESS_6A error flag remains set (nodes rely on master for recovery)

### Power unavailable during charging suspends to STATE_C1
**Requirement:** `REQ-ERR-023`

- **Given** The EVSE is in STATE_C (charging)
- **When** evse_set_power_unavailable is called
- **Then** The state transitions to STATE_C1 (charging suspended)

### Power unavailable in STATE_B moves to waiting state B1
**Requirement:** `REQ-ERR-024`

- **Given** The EVSE is in STATE_B (connected)
- **When** evse_set_power_unavailable is called
- **Then** The state transitions to STATE_B1 (waiting)

### Power unavailable in STATE_A has no effect
**Requirement:** `REQ-ERR-025`

- **Given** The EVSE is in STATE_A (disconnected)
- **When** evse_set_power_unavailable is called
- **Then** The state remains STATE_A

### Power unavailable in STATE_B1 remains in B1 (already waiting)
**Requirement:** `REQ-ERR-026`

- **Given** The EVSE is in STATE_B1 (waiting)
- **When** evse_set_power_unavailable is called
- **Then** The state remains STATE_B1

### Power unavailable in STATE_C1 remains in C1 (already suspended)
**Requirement:** `REQ-ERR-027`

- **Given** The EVSE is in STATE_C1 (charging suspended)
- **When** evse_set_power_unavailable is called
- **Then** The state remains STATE_C1

### Entering STATE_B1 disconnects pilot when authorized
**Requirement:** `REQ-ERR-028`

- **Given** The EVSE has AccessStatus ON and PilotDisconnected is false
- **When** The state is set to STATE_B1
- **Then** PilotDisconnected is true and pilot_connected is false

### Pilot reconnects after PilotDisconnectTime expires
**Requirement:** `REQ-ERR-029`

- **Given** The EVSE has PilotDisconnectTime=2 with pilot disconnected
- **When** Two second ticks occur
- **Then** PilotDisconnected is cleared and pilot_connected is restored

### MaxSumMains timer expiry stops charging with LESS_6A error
**Requirement:** `REQ-ERR-030`

- **Given** The EVSE is charging with MaxSumMainsTimer=1 and mains heavily loaded
- **When** One second tick occurs (timer expires)
- **Then** The state transitions to STATE_C1 and LESS_6A error flag is set

</details>

---

## Fidelity: DisconnectTimeCounter

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-FID-D1A` | Module tick_1s does not increment DisconnectTimeCounter | `test_fid_disconnect_counter_not_in_module` | `test_fidelity.c:1` |
| `REQ-FID-D1B` | Module correctly sets counter via set_state for STATE_A | `test_fid_disconnect_counter_starts_on_state_a` | `test_fidelity.c:59` |
| `REQ-FID-D1C` | Module disables counter on MODEM_REQUEST entry | `test_fid_disconnect_counter_disabled_on_modem_request` | `test_fidelity.c:75` |

<details>
<summary>Detailed steps (3 scenarios)</summary>

### Module tick_1s does not increment DisconnectTimeCounter
**Requirement:** `REQ-FID-D1A`

- **Given** ModemEnabled=true, DisconnectTimeCounter=0
- **When** tick_1s is called
- **Then** Counter stays 0 (firmware wrapper handles increment + pilot check)

### Module correctly sets counter via set_state for STATE_A
**Requirement:** `REQ-FID-D1B`

- **Given** ModemEnabled=true, DisconnectTimeCounter=-1
- **When** evse_set_state to STATE_A
- **Then** Counter is set to 0 (start counting)

### Module disables counter on MODEM_REQUEST entry
**Requirement:** `REQ-FID-D1C`

- **Given** DisconnectTimeCounter=5
- **When** evse_set_state to STATE_MODEM_REQUEST
- **Then** Counter is set to -1 (disabled)

</details>

---

## Fidelity: PilotDisconnectTime

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-FID-D2A` | tick_1s only decrements timer, does not reconnect | `test_fid_pilot_disconnect_no_reconnect_in_tick_1s` | `test_fidelity.c:95` |
| `REQ-FID-D2B` | tick_10ms reconnects when PilotDisconnectTime reaches 0 | `test_fid_pilot_disconnect_reconnects_in_tick_10ms` | `test_fidelity.c:116` |

<details>
<summary>Detailed steps (2 scenarios)</summary>

### tick_1s only decrements timer, does not reconnect
**Requirement:** `REQ-FID-D2A`

- **Given** PilotDisconnectTime=1, PilotDisconnected=true
- **When** tick_1s is called (timer reaches 0)
- **Then** PilotDisconnected is still true (reconnect happens in tick_10ms)

### tick_10ms reconnects when PilotDisconnectTime reaches 0
**Requirement:** `REQ-FID-D2B`

- **Given** PilotDisconnected=true, PilotDisconnectTime=0, State=B1
- **When** tick_10ms is called with PILOT_9V
- **Then** PilotDisconnected=false, pilot_connected=true

</details>

---

## Fidelity: Fall-through behavior

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-FID-D3A` | COMM_B_OK transitions to STATE_B and B handler runs same tick | `test_fid_comm_b_ok_falls_through_to_b_handler` | `test_fidelity.c:141` |
| `REQ-FID-D3B` | COMM_B_OK → B → 12V disconnect in same tick | `test_fid_comm_b_ok_to_b_then_disconnect` | `test_fidelity.c:161` |
| `REQ-FID-D4A` | A→B transition allows B handler to run and detect PILOT_DIODE | `test_fid_a_to_b_falls_through_to_b_handler` | `test_fidelity.c:183` |
| `REQ-FID-D5A` | COMM_C_OK transitions to STATE_C and C handler runs same tick | `test_fid_comm_c_ok_falls_through_to_c_handler` | `test_fidelity.c:208` |
| `REQ-FID-D5B` | COMM_C_OK → C → immediate 12V disconnect in same tick | `test_fid_comm_c_ok_to_c_then_disconnect` | `test_fidelity.c:230` |

<details>
<summary>Detailed steps (5 scenarios)</summary>

### COMM_B_OK transitions to STATE_B and B handler runs same tick
**Requirement:** `REQ-FID-D3A`

- **Given** State=COMM_B_OK, DiodeCheck=0
- **When** tick_10ms with PILOT_DIODE
- **Then** State=STATE_B, and DiodeCheck=1 (B handler fired in same tick)

### COMM_B_OK → B → 12V disconnect in same tick
**Requirement:** `REQ-FID-D3B`

- **Given** State=COMM_B_OK
- **When** tick_10ms with PILOT_12V
- **Then** State=STATE_A (B handler detects disconnect in same tick)

### A→B transition allows B handler to run and detect PILOT_DIODE
**Requirement:** `REQ-FID-D4A`

- **Given** State=STATE_A, PILOT_DIODE would trigger A→B on 9V but we pass 9V
- **When** tick_10ms with PILOT_9V in STATE_A with access
- **Then** State=STATE_B, ActivationMode=30, and StateTimer reset to 0

### COMM_C_OK transitions to STATE_C and C handler runs same tick
**Requirement:** `REQ-FID-D5A`

- **Given** State=COMM_C_OK
- **When** tick_10ms with PILOT_6V
- **Then** State=STATE_C, StateTimer=0 (C handler's else resets StateTimer)

### COMM_C_OK → C → immediate 12V disconnect in same tick
**Requirement:** `REQ-FID-D5B`

- **Given** State=COMM_C_OK
- **When** tick_10ms with PILOT_12V
- **Then** State=STATE_A (C handler detects disconnect immediately)

</details>

---

## Fidelity: ACTSTART no pilot check

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-FID-D6A` | ACTSTART ignores PILOT_12V (original has no pilot check here) | `test_fid_actstart_no_pilot_12v_check` | `test_fidelity.c:252` |
| `REQ-FID-D6B` | ACTSTART timer expiry leads to STATE_B (B handler is before ACTSTART | `test_fid_actstart_timer_then_disconnect` | `test_fidelity.c:270` |

<details>
<summary>Detailed steps (2 scenarios)</summary>

### ACTSTART ignores PILOT_12V (original has no pilot check here)
**Requirement:** `REQ-FID-D6A`

- **Given** State=ACTSTART with timer running (3 seconds)
- **When** tick_10ms with PILOT_12V
- **Then** State stays ACTSTART (timer must expire → B → detects 12V → A)

### ACTSTART timer expiry leads to STATE_B (B handler is before ACTSTART
**Requirement:** `REQ-FID-D6B`

- **Given** State=ACTSTART, ActivationTimer=0
- **When** tick_10ms with PILOT_12V, then another tick_10ms with PILOT_12V
- **Then** First tick: ACTSTART → B. Second tick: B → A (12V detected)

</details>

---

## Fidelity: Modem states not in tick_10ms

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-FID-D7A` | Modem states are invisible to tick_10ms | `test_fid_modem_states_invisible_to_tick_10ms` | `test_fidelity.c:299` |

<details>
<summary>Detailed steps (1 scenarios)</summary>

### Modem states are invisible to tick_10ms
**Requirement:** `REQ-FID-D7A`

- **Given** EVSE in each modem state
- **When** tick_10ms with any pilot value
- **Then** State does not change (modem managed entirely by tick_1s)

</details>

---

## Fidelity: Handler ordering

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-FID-D8A` | B→ACTSTART falls through to ACTSTART handler in same tick | `test_fid_b_to_actstart_falls_through` | `test_fidelity.c:327` |

<details>
<summary>Detailed steps (1 scenarios)</summary>

### B→ACTSTART falls through to ACTSTART handler in same tick
**Requirement:** `REQ-FID-D8A`

- **Given** State=STATE_B, ActivationMode=0 (expired)
- **When** tick_10ms with PILOT_9V
- **Then** State=STATE_ACTSTART, ActivationTimer=3

</details>

---

## Fidelity: Config field

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-FID-CFG-A` | Socket mode (Config=0) caps ChargeCurrent by MaxCapacity | `test_fid_config_socket_caps_by_maxcapacity` | `test_fidelity.c:351` |
| `REQ-FID-CFG-B` | Fixed cable mode (Config=1) does NOT cap by MaxCapacity | `test_fid_config_fixed_cable_no_maxcapacity_cap` | `test_fidelity.c:372` |

<details>
<summary>Detailed steps (2 scenarios)</summary>

### Socket mode (Config=0) caps ChargeCurrent by MaxCapacity
**Requirement:** `REQ-FID-CFG-A`

- **Given** Config=0, MaxCurrent=25, MaxCapacity=16, STATE_C
- **When** calc_balanced_current is called
- **Then** ChargeCurrent=160 (capped by MaxCapacity)

### Fixed cable mode (Config=1) does NOT cap by MaxCapacity
**Requirement:** `REQ-FID-CFG-B`

- **Given** Config=1, MaxCurrent=25, MaxCapacity=16, STATE_C
- **When** calc_balanced_current is called
- **Then** ChargeCurrent=250 (MaxCurrent * 10, not capped)

</details>

---

## HTTP API Color Parsing

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-API-001` | Valid RGB color values are accepted | `test_color_valid` | `test_http_api.c:1` |
| `REQ-API-001` | Zero RGB values are accepted | `test_color_zero` | `test_http_api.c:30` |
| `REQ-API-001` | Maximum RGB values are accepted | `test_color_max` | `test_http_api.c:41` |

<details>
<summary>Detailed steps (3 scenarios)</summary>

### Valid RGB color values are accepted
**Requirement:** `REQ-API-001`

- **Given** Integer values for R, G, B
- **When** All values are in 0..255
- **Then** http_api_parse_color returns true with correct output

### Zero RGB values are accepted
**Requirement:** `REQ-API-001`


### Maximum RGB values are accepted
**Requirement:** `REQ-API-001`


</details>

---

## HTTP API Input Validation

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-API-002` | RGB value above 255 is rejected | `test_color_out_of_range` | `test_http_api.c:52` |
| `REQ-API-002` | Negative RGB value is rejected | `test_color_negative` | `test_http_api.c:62` |
| `REQ-API-004` | Override current below minimum is rejected | `test_override_current_below_min` | `test_http_api.c:111` |
| `REQ-API-004` | Override current above maximum is rejected | `test_override_current_above_max` | `test_http_api.c:120` |
| `REQ-API-004` | Override current on slave is rejected | `test_override_current_slave` | `test_http_api.c:129` |
| `REQ-API-005` | Current min below 6A is rejected | `test_current_min_too_low` | `test_http_api.c:158` |
| `REQ-API-005` | Current min above 16A is rejected | `test_current_min_too_high` | `test_http_api.c:167` |
| `REQ-API-005` | Current min on slave is rejected | `test_current_min_slave` | `test_http_api.c:176` |
| `REQ-API-006` | Max sum mains between 1-9 is rejected | `test_max_sum_mains_gap` | `test_http_api.c:214` |
| `REQ-API-006` | Max sum mains above 600 is rejected | `test_max_sum_mains_too_high` | `test_http_api.c:223` |
| `REQ-API-006` | Max sum mains on slave is rejected | `test_max_sum_mains_slave` | `test_http_api.c:232` |
| `REQ-API-007` | Stop timer above 60 is rejected | `test_stop_timer_too_high` | `test_http_api.c:261` |
| `REQ-API-007` | Negative stop timer is rejected | `test_stop_timer_negative` | `test_http_api.c:270` |
| `REQ-API-008` | Solar start current above 48 is rejected | `test_solar_start_too_high` | `test_http_api.c:299` |
| `REQ-API-009` | Solar max import above 48 is rejected | `test_solar_import_too_high` | `test_http_api.c:319` |
| `REQ-API-011` | PrioStrategy value 3 is rejected | `test_prio_strategy_too_high` | `test_http_api.c:360` |
| `REQ-API-011` | PrioStrategy negative value is rejected | `test_prio_strategy_negative` | `test_http_api.c:369` |
| `REQ-API-011` | PrioStrategy on slave is rejected | `test_prio_strategy_slave` | `test_http_api.c:378` |
| `REQ-API-012` | RotationInterval in gap (1-29) is rejected | `test_rotation_interval_gap` | `test_http_api.c:422` |
| `REQ-API-012` | RotationInterval above maximum is rejected | `test_rotation_interval_too_high` | `test_http_api.c:431` |
| `REQ-API-012` | RotationInterval on slave is rejected | `test_rotation_interval_slave` | `test_http_api.c:440` |
| `REQ-API-013` | IdleTimeout below minimum (29) is rejected | `test_idle_timeout_too_low` | `test_http_api.c:481` |
| `REQ-API-013` | IdleTimeout above maximum (301) is rejected | `test_idle_timeout_too_high` | `test_http_api.c:490` |
| `REQ-API-013` | IdleTimeout on slave is rejected | `test_idle_timeout_slave` | `test_http_api.c:499` |

<details>
<summary>Detailed steps (24 scenarios)</summary>

### RGB value above 255 is rejected
**Requirement:** `REQ-API-002`


### Negative RGB value is rejected
**Requirement:** `REQ-API-002`


### Override current below minimum is rejected
**Requirement:** `REQ-API-004`


### Override current above maximum is rejected
**Requirement:** `REQ-API-004`


### Override current on slave is rejected
**Requirement:** `REQ-API-004`


### Current min below 6A is rejected
**Requirement:** `REQ-API-005`


### Current min above 16A is rejected
**Requirement:** `REQ-API-005`


### Current min on slave is rejected
**Requirement:** `REQ-API-005`


### Max sum mains between 1-9 is rejected
**Requirement:** `REQ-API-006`


### Max sum mains above 600 is rejected
**Requirement:** `REQ-API-006`


### Max sum mains on slave is rejected
**Requirement:** `REQ-API-006`


### Stop timer above 60 is rejected
**Requirement:** `REQ-API-007`


### Negative stop timer is rejected
**Requirement:** `REQ-API-007`


### Solar start current above 48 is rejected
**Requirement:** `REQ-API-008`


### Solar max import above 48 is rejected
**Requirement:** `REQ-API-009`


### PrioStrategy value 3 is rejected
**Requirement:** `REQ-API-011`


### PrioStrategy negative value is rejected
**Requirement:** `REQ-API-011`


### PrioStrategy on slave is rejected
**Requirement:** `REQ-API-011`

- **Given** A slave EVSE (load_bl=2)
- **When** prio_strategy is 0 (valid value)
- **Then** Validation fails because slaves cannot set scheduling

### RotationInterval in gap (1-29) is rejected
**Requirement:** `REQ-API-012`


### RotationInterval above maximum is rejected
**Requirement:** `REQ-API-012`


### RotationInterval on slave is rejected
**Requirement:** `REQ-API-012`


### IdleTimeout below minimum (29) is rejected
**Requirement:** `REQ-API-013`


### IdleTimeout above maximum (301) is rejected
**Requirement:** `REQ-API-013`


### IdleTimeout on slave is rejected
**Requirement:** `REQ-API-013`


</details>

---

## HTTP API Validation

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-API-003` | Override current zero is always valid (disables override) | `test_override_current_zero` | `test_http_api.c:74` |
| `REQ-API-003` | Override current within range is valid | `test_override_current_valid` | `test_http_api.c:83` |
| `REQ-API-003` | Override current at minimum boundary is valid | `test_override_current_at_min` | `test_http_api.c:93` |
| `REQ-API-003` | Override current at maximum boundary is valid | `test_override_current_at_max` | `test_http_api.c:102` |
| `REQ-API-005` | Current min at boundary (6A) is valid | `test_current_min_valid` | `test_http_api.c:140` |
| `REQ-API-005` | Current min at 16A is valid | `test_current_min_max` | `test_http_api.c:149` |
| `REQ-API-006` | Max sum mains zero disables limit | `test_max_sum_mains_zero` | `test_http_api.c:187` |
| `REQ-API-006` | Max sum mains at minimum (10A) is valid | `test_max_sum_mains_min` | `test_http_api.c:196` |
| `REQ-API-006` | Max sum mains at maximum (600A) is valid | `test_max_sum_mains_max` | `test_http_api.c:205` |
| `REQ-API-007` | Stop timer at zero is valid | `test_stop_timer_zero` | `test_http_api.c:243` |
| `REQ-API-007` | Stop timer at max (60) is valid | `test_stop_timer_max` | `test_http_api.c:252` |
| `REQ-API-008` | Solar start current at 0 is valid | `test_solar_start_zero` | `test_http_api.c:281` |
| `REQ-API-008` | Solar start current at 48 is valid | `test_solar_start_max` | `test_http_api.c:290` |
| `REQ-API-009` | Solar max import at 0 is valid | `test_solar_import_zero` | `test_http_api.c:310` |
| `REQ-API-011` | PrioStrategy MODBUS_ADDR (0) is valid on master | `test_prio_strategy_valid_0` | `test_http_api.c:330` |
| `REQ-API-011` | PrioStrategy FIRST_CONNECTED (1) is valid | `test_prio_strategy_valid_1` | `test_http_api.c:342` |
| `REQ-API-011` | PrioStrategy LAST_CONNECTED (2) is valid | `test_prio_strategy_valid_2` | `test_http_api.c:351` |
| `REQ-API-012` | RotationInterval 0 (disabled) is valid | `test_rotation_interval_zero` | `test_http_api.c:392` |
| `REQ-API-012` | RotationInterval at minimum (30) is valid | `test_rotation_interval_min` | `test_http_api.c:404` |
| `REQ-API-012` | RotationInterval at maximum (1440) is valid | `test_rotation_interval_max` | `test_http_api.c:413` |
| `REQ-API-013` | IdleTimeout at minimum (30) is valid | `test_idle_timeout_min` | `test_http_api.c:451` |
| `REQ-API-013` | IdleTimeout at default (60) is valid | `test_idle_timeout_default` | `test_http_api.c:463` |
| `REQ-API-013` | IdleTimeout at maximum (300) is valid | `test_idle_timeout_max` | `test_http_api.c:472` |

<details>
<summary>Detailed steps (23 scenarios)</summary>

### Override current zero is always valid (disables override)
**Requirement:** `REQ-API-003`


### Override current within range is valid
**Requirement:** `REQ-API-003`


### Override current at minimum boundary is valid
**Requirement:** `REQ-API-003`


### Override current at maximum boundary is valid
**Requirement:** `REQ-API-003`


### Current min at boundary (6A) is valid
**Requirement:** `REQ-API-005`


### Current min at 16A is valid
**Requirement:** `REQ-API-005`


### Max sum mains zero disables limit
**Requirement:** `REQ-API-006`


### Max sum mains at minimum (10A) is valid
**Requirement:** `REQ-API-006`


### Max sum mains at maximum (600A) is valid
**Requirement:** `REQ-API-006`


### Stop timer at zero is valid
**Requirement:** `REQ-API-007`


### Stop timer at max (60) is valid
**Requirement:** `REQ-API-007`


### Solar start current at 0 is valid
**Requirement:** `REQ-API-008`


### Solar start current at 48 is valid
**Requirement:** `REQ-API-008`


### Solar max import at 0 is valid
**Requirement:** `REQ-API-009`


### PrioStrategy MODBUS_ADDR (0) is valid on master
**Requirement:** `REQ-API-011`

- **Given** A master EVSE (load_bl=0)
- **When** prio_strategy is 0
- **Then** Validation passes

### PrioStrategy FIRST_CONNECTED (1) is valid
**Requirement:** `REQ-API-011`


### PrioStrategy LAST_CONNECTED (2) is valid
**Requirement:** `REQ-API-011`


### RotationInterval 0 (disabled) is valid
**Requirement:** `REQ-API-012`

- **Given** A master EVSE (load_bl=0)
- **When** rotation_interval is 0
- **Then** Validation passes

### RotationInterval at minimum (30) is valid
**Requirement:** `REQ-API-012`


### RotationInterval at maximum (1440) is valid
**Requirement:** `REQ-API-012`


### IdleTimeout at minimum (30) is valid
**Requirement:** `REQ-API-013`

- **Given** A master EVSE (load_bl=0)
- **When** idle_timeout is 30
- **Then** Validation passes

### IdleTimeout at default (60) is valid
**Requirement:** `REQ-API-013`


### IdleTimeout at maximum (300) is valid
**Requirement:** `REQ-API-013`


</details>

---

## HTTP API Settings Validation

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-API-010` | Valid settings request passes validation | `test_validate_settings_valid` | `test_http_api.c:510` |
| `REQ-API-010` | Invalid current_min in combined request | `test_validate_settings_invalid_min` | `test_http_api.c:531` |
| `REQ-API-010` | Multiple invalid fields | `test_validate_settings_multiple_errors` | `test_http_api.c:548` |
| `REQ-API-010` | Empty request passes validation | `test_validate_settings_empty` | `test_http_api.c:566` |
| `REQ-API-010` | Slave restrictions applied | `test_validate_settings_slave_restrictions` | `test_http_api.c:580` |
| `REQ-API-014` | Valid scheduling settings in combined request | `test_validate_settings_scheduling_valid` | `test_http_api.c:596` |
| `REQ-API-014` | Invalid scheduling settings on slave | `test_validate_settings_scheduling_slave` | `test_http_api.c:619` |

<details>
<summary>Detailed steps (7 scenarios)</summary>

### Valid settings request passes validation
**Requirement:** `REQ-API-010`

- **Given** A settings request with valid current_min and override_current
- **When** Validated against current limits
- **Then** No errors are returned

### Invalid current_min in combined request
**Requirement:** `REQ-API-010`


### Multiple invalid fields
**Requirement:** `REQ-API-010`


### Empty request passes validation
**Requirement:** `REQ-API-010`


### Slave restrictions applied
**Requirement:** `REQ-API-010`


### Valid scheduling settings in combined request
**Requirement:** `REQ-API-014`

- **Given** A settings request with valid scheduling fields
- **When** Validated on master (load_bl=1)
- **Then** No errors are returned

### Invalid scheduling settings on slave
**Requirement:** `REQ-API-014`

- **Given** A settings request with scheduling fields
- **When** Validated on slave (load_bl=2)
- **Then** All three scheduling fields produce errors

</details>

---

## LED Status Indication

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-LED-001` | RCM tripped error produces red flashing pattern on ESP32 | `test_error_rcm_tripped_esp32` | `test_led_color.c:1` |
| `REQ-LED-001` | CT_NOCOMM error shows red flashing | `test_error_ct_nocomm` | `test_led_color.c:82` |
| `REQ-LED-001` | TEMP_HIGH error shows red flashing | `test_error_temp_high` | `test_led_color.c:105` |
| `REQ-LED-001` | CH32 RCM mismatch with no test counter shows error | `test_error_ch32_rcm_mismatch` | `test_led_color.c:126` |
| `REQ-LED-001` | CH32 RCM test in progress does not show error flash | `test_no_error_ch32_rcm_test_active` | `test_led_color.c:149` |
| `REQ-LED-002` | Access OFF shows off color | `test_access_off_default` | `test_led_color.c:181` |
| `REQ-LED-002` | MODEM_DENIED state shows off color | `test_modem_denied_shows_off` | `test_led_color.c:224` |
| `REQ-LED-003` | Solar mode with charge delay shows slow blink | `test_waiting_solar_blink` | `test_led_color.c:247` |
| `REQ-LED-003` | Smart mode waiting shows smart color blink | `test_waiting_smart_color` | `test_led_color.c:271` |
| `REQ-LED-005` | State A shows dimmed LED | `test_state_a_dimmed` | `test_led_color.c:320` |
| `REQ-LED-005` | State B shows full brightness LED | `test_state_b_full_brightness` | `test_led_color.c:340` |
| `REQ-LED-005` | State B1 shows full brightness (same as B) | `test_state_b1_full_brightness` | `test_led_color.c:359` |
| `REQ-LED-005` | State B sets led_count to 128 for smooth C transition | `test_state_b_sets_count_128` | `test_led_color.c:378` |
| `REQ-LED-006` | State C shows breathing animation | `test_state_c_breathing` | `test_led_color.c:395` |
| `REQ-LED-006` | State C Solar mode has slower breathing | `test_state_c_solar_slower` | `test_led_color.c:418` |

<details>
<summary>Detailed steps (15 scenarios)</summary>

### RCM tripped error produces red flashing pattern on ESP32
**Requirement:** `REQ-LED-001`

- **Given** ErrorFlags has RCM_TRIPPED set on ESP32 platform
- **When** led_compute_color is called repeatedly
- **Then** LED alternates between red and off

### CT_NOCOMM error shows red flashing
**Requirement:** `REQ-LED-001`

- **Given** ErrorFlags has CT_NOCOMM set
- **When** led_compute_color is called multiple times
- **Then** LED flashes red

### TEMP_HIGH error shows red flashing
**Requirement:** `REQ-LED-001`

- **Given** ErrorFlags has TEMP_HIGH set
- **When** led_compute_color is called
- **Then** LED flashes red

### CH32 RCM mismatch with no test counter shows error
**Requirement:** `REQ-LED-001`

- **Given** CH32 platform with RCM_TRIPPED set but not RCM_TEST, counter=0
- **When** led_compute_color is called
- **Then** LED shows red (error condition)

### CH32 RCM test in progress does not show error flash
**Requirement:** `REQ-LED-001`

- **Given** CH32 platform with RCM_TRIPPED set, counter > 0 (test running)
- **When** led_compute_color is called
- **Then** LED does NOT show rapid red error flash (enters waiting blink instead)

### Access OFF shows off color
**Requirement:** `REQ-LED-002`

- **Given** Access status is OFF, no custom button
- **When** led_compute_color is called
- **Then** LED shows ColorOff values

### MODEM_DENIED state shows off color
**Requirement:** `REQ-LED-002`

- **Given** State is STATE_MODEM_DENIED, access ON
- **When** led_compute_color is called
- **Then** LED shows ColorOff values (same as access OFF)

### Solar mode with charge delay shows slow blink
**Requirement:** `REQ-LED-003`

- **Given** Solar mode, ChargeDelay > 0, no errors
- **When** led_compute_color is called repeatedly
- **Then** LED blinks with solar color (orange)

### Smart mode waiting shows smart color blink
**Requirement:** `REQ-LED-003`

- **Given** Smart mode, ChargeDelay > 0
- **When** led_compute_color is called when LED is on
- **Then** Color matches ColorSmart

### State A shows dimmed LED
**Requirement:** `REQ-LED-005`

- **Given** State A, Normal mode, no errors
- **When** led_compute_color is called
- **Then** LED shows dimmed green (STATE_A_LED_BRIGHTNESS)

### State B shows full brightness LED
**Requirement:** `REQ-LED-005`

- **Given** State B, Normal mode, no errors
- **When** led_compute_color is called
- **Then** LED shows full brightness green

### State B1 shows full brightness (same as B)
**Requirement:** `REQ-LED-005`

- **Given** State B1, Normal mode
- **When** led_compute_color is called
- **Then** LED shows full brightness green

### State B sets led_count to 128 for smooth C transition
**Requirement:** `REQ-LED-005`

- **Given** State B entered
- **When** led_compute_color is called
- **Then** led_count is set to 128

### State C shows breathing animation
**Requirement:** `REQ-LED-006`

- **Given** State C, Normal mode
- **When** led_compute_color is called multiple times
- **Then** LED brightness varies (breathing effect)

### State C Solar mode has slower breathing
**Requirement:** `REQ-LED-006`

- **Given** State C, Solar mode
- **When** led_compute_color is called
- **Then** led_count increments by 1 per call (vs 2 for other modes)

</details>

---

## LED Color Configuration

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-LED-004` | Custom button active when access OFF shows custom color | `test_access_off_custom_button` | `test_led_color.c:204` |
| `REQ-LED-004` | Custom button waiting shows custom color | `test_waiting_custom_button` | `test_led_color.c:294` |
| `REQ-LED-004` | Solar mode State A shows solar orange (dimmed) | `test_state_a_solar_color` | `test_led_color.c:445` |
| `REQ-LED-004` | Custom button overrides mode color in State B | `test_state_b_custom_override` | `test_led_color.c:466` |

<details>
<summary>Detailed steps (4 scenarios)</summary>

### Custom button active when access OFF shows custom color
**Requirement:** `REQ-LED-004`

- **Given** Access OFF and CustomButton is true
- **When** led_compute_color is called
- **Then** LED shows ColorCustom values

### Custom button waiting shows custom color
**Requirement:** `REQ-LED-004`

- **Given** Custom button active, ChargeDelay > 0
- **When** led_compute_color is called when LED is on
- **Then** Color matches ColorCustom

### Solar mode State A shows solar orange (dimmed)
**Requirement:** `REQ-LED-004`

- **Given** State A, Solar mode
- **When** led_compute_color is called
- **Then** LED shows orange tint at STATE_A brightness

### Custom button overrides mode color in State B
**Requirement:** `REQ-LED-004`

- **Given** State B, Normal mode, CustomButton true
- **When** led_compute_color is called
- **Then** LED shows custom blue at full brightness

</details>

---

## Load Balancing

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-LB-001` | Single standalone EVSE receives full MaxCurrent allocation | `test_single_evse_gets_full_current` | `test_load_balancing.c:1` |
| `REQ-LB-002` | Two EVSEs receive equal current distribution | `test_two_evse_equal_distribution` | `test_load_balancing.c:66` |
| `REQ-LB-003` | Two EVSEs respect MaxCircuit total capacity limit | `test_two_evse_respects_max_circuit` | `test_load_balancing.c:82` |
| `REQ-LB-004` | Individual EVSE BalancedMax caps its current allocation | `test_balanced_max_caps_individual` | `test_load_balancing.c:101` |
| `REQ-LB-006` | Each active EVSE receives at least MinCurrent | `test_minimum_current_enforced` | `test_load_balancing.c:121` |
| `REQ-LB-007` | New EVSE joining (mod=1) triggers full recalculation | `test_mod1_new_evse_recalculates` | `test_load_balancing.c:167` |
| `REQ-LB-008` | OCPP current limit reduces ChargeCurrent below MaxCurrent | `test_ocpp_limit_reduces_charge_current` | `test_load_balancing.c:189` |
| `REQ-LB-009` | OCPP current limit below MinCurrent zeros out ChargeCurrent | `test_ocpp_limit_below_min_zeros_current` | `test_load_balancing.c:215` |
| `REQ-LB-010` | OverrideCurrent takes precedence over calculated ChargeCurrent | `test_override_current_takes_precedence` | `test_load_balancing.c:242` |
| `REQ-LB-011` | Current shortage increments NoCurrent counter | `test_shortage_increments_nocurrent` | `test_load_balancing.c:269` |
| `REQ-LB-012` | No current shortage clears NoCurrent counter | `test_no_shortage_clears_nocurrent` | `test_load_balancing.c:293` |
| `REQ-LB-013` | Open grid relay caps IsetBalanced at GridRelayMaxSumMains per phase | `test_grid_relay_limits_current` | `test_load_balancing.c:317` |
| `REQ-LB-014` | Node EVSE requests COMM_C instead of transitioning directly to STATE_C | `test_node_requests_comm_c` | `test_load_balancing.c:353` |
| `REQ-LB-F1A` | Socket mode (Config=0) caps ChargeCurrent by MaxCapacity | `test_config_socket_caps_by_maxcapacity` | `test_load_balancing.c:381` |
| `REQ-LB-F1B` | Fixed Cable mode (Config=1) does NOT cap by MaxCapacity | `test_config_fixed_cable_no_maxcapacity_cap` | `test_load_balancing.c:408` |

<details>
<summary>Detailed steps (15 scenarios)</summary>

### Single standalone EVSE receives full MaxCurrent allocation
**Requirement:** `REQ-LB-001`

- **Given** A standalone EVSE (LoadBl=0) in STATE_C with MaxCurrent=16A
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced is set to 160 (16A in tenths)

### Two EVSEs receive equal current distribution
**Requirement:** `REQ-LB-002`

- **Given** Two EVSEs are charging as master (LoadBl=1) with equal BalancedMax
- **When** evse_calc_balanced_current is called
- **Then** Both EVSEs receive equal Balanced current allocations

### Two EVSEs respect MaxCircuit total capacity limit
**Requirement:** `REQ-LB-003`

- **Given** Two EVSEs are charging with MaxCircuit=16A total
- **When** evse_calc_balanced_current is called
- **Then** Each EVSE receives at most half the circuit capacity

### Individual EVSE BalancedMax caps its current allocation
**Requirement:** `REQ-LB-004`

- **Given** Two EVSEs are charging with EVSE 1 having BalancedMax=60 (6A)
- **When** evse_calc_balanced_current is called
- **Then** EVSE 1 Balanced is capped at its BalancedMax of 60

### Each active EVSE receives at least MinCurrent
**Requirement:** `REQ-LB-006`

- **Given** Two EVSEs are both in STATE_A (disconnected) as master
- **Given** Two EVSEs are charging with MinCurrent=6A and limited total capacity
- **When** evse_calc_balanced_current is called
- **When** evse_calc_balanced_current is called
- **Then** NoCurrent and SolarStopTimer are reset to 0
- **Then** Each charging EVSE with non-zero allocation gets at least MinCurrent*10

### New EVSE joining (mod=1) triggers full recalculation
**Requirement:** `REQ-LB-007`

- **Given** Two EVSEs in MODE_SMART with existing current distribution
- **When** evse_calc_balanced_current is called with mod=1 (new EVSE joining)
- **Then** IsetBalanced is recalculated from scratch (different from previous value)

### OCPP current limit reduces ChargeCurrent below MaxCurrent
**Requirement:** `REQ-LB-008`

- **Given** A standalone EVSE with OcppCurrentLimit=10A and MaxCurrent=16A
- **When** evse_calc_balanced_current is called
- **Then** ChargeCurrent is capped at 100 (10A in tenths) or below

### OCPP current limit below MinCurrent zeros out ChargeCurrent
**Requirement:** `REQ-LB-009`

- **Given** A standalone EVSE with OcppCurrentLimit=3A and MinCurrent=6A
- **When** evse_calc_balanced_current is called
- **Then** ChargeCurrent is set to 0 (below minimum, cannot charge)

### OverrideCurrent takes precedence over calculated ChargeCurrent
**Requirement:** `REQ-LB-010`

- **Given** A standalone EVSE with OverrideCurrent=80 (8A) and MaxCurrent=16A
- **When** evse_calc_balanced_current is called
- **Then** ChargeCurrent is set to 80 (override value)

### Current shortage increments NoCurrent counter
**Requirement:** `REQ-LB-011`

- **Given** Two EVSEs in MODE_SMART with mains heavily loaded and low MaxMains
- **When** evse_calc_balanced_current is called with insufficient capacity
- **Then** NoCurrent counter is incremented above 0

### No current shortage clears NoCurrent counter
**Requirement:** `REQ-LB-012`

- **Given** Two EVSEs in MODE_SMART with low mains load and high MaxMains
- **When** evse_calc_balanced_current is called with sufficient capacity
- **Then** NoCurrent counter is cleared to 0

### Open grid relay caps IsetBalanced at GridRelayMaxSumMains per phase
**Requirement:** `REQ-LB-013`

- **Given** A standalone EVSE in MODE_SMART with GridRelayOpen=true and 3 phases
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced is capped at GridRelayMaxSumMains*10/3

### Node EVSE requests COMM_C instead of transitioning directly to STATE_C
**Requirement:** `REQ-LB-014`

- **Given** The EVSE is configured as a node (LoadBl=2) in STATE_B with DiodeCheck passed
- **When** A 6V pilot signal is sustained for 500ms (vehicle requests charge)
- **Then** The state transitions to STATE_COMM_C (requesting master permission to charge)

### Socket mode (Config=0) caps ChargeCurrent by MaxCapacity
**Requirement:** `REQ-LB-F1A`

- **Given** Config=0 (Socket), MaxCurrent=25, MaxCapacity=16, STATE_C
- **When** evse_calc_balanced_current is called
- **Then** ChargeCurrent is capped at 160 (MaxCapacity * 10)

### Fixed Cable mode (Config=1) does NOT cap by MaxCapacity
**Requirement:** `REQ-LB-F1B`

- **Given** Config=1 (Fixed Cable), MaxCurrent=25, MaxCapacity=16, STATE_C
- **When** evse_calc_balanced_current is called
- **Then** ChargeCurrent is 250 (MaxCurrent * 10), not capped by MaxCapacity

</details>

---

## Meter Timeout & Recovery

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-METER-001` | CT_NOCOMM error is set on timeout and cleared when communication restores | `test_ct_nocomm_set_then_restored` | `test_meter_recovery.c:1` |
| `REQ-METER-002` | EV_NOCOMM error is set on timeout and cleared when communication restores | `test_ev_nocomm_set_then_restored` | `test_meter_recovery.c:51` |
| `REQ-METER-003` | Both CT_NOCOMM and EV_NOCOMM can be set simultaneously and recover independently | `test_both_ct_and_ev_nocomm_simultaneously` | `test_meter_recovery.c:77` |
| `REQ-METER-004` | Mains meter timeout during STATE_C triggers transition to STATE_C1 | `test_mains_timeout_during_state_c` | `test_meter_recovery.c:118` |
| `REQ-METER-005` | EV meter timeout during STATE_C triggers transition to STATE_C1 | `test_ev_timeout_during_state_c` | `test_meter_recovery.c:145` |
| `REQ-METER-006` | MainsMeter timeout on node sets CT_NOCOMM regardless of operating mode | `test_mains_timeout_on_node` | `test_meter_recovery.c:175` |
| `REQ-METER-007` | MainsMeter timeout on standalone in Normal mode does not set CT_NOCOMM | `test_mains_timeout_master_normal_mode_ignored` | `test_meter_recovery.c:196` |
| `REQ-METER-008` | No EV meter installed continuously resets EVMeterTimeout to COMM_EVTIMEOUT | `test_no_ev_meter_resets_timeout_continuously` | `test_meter_recovery.c:221` |
| `REQ-METER-009` | No mains meter type and standalone resets MainsMeterTimeout to COMM_TIMEOUT | `test_no_mains_meter_resets_timeout_continuously` | `test_meter_recovery.c:248` |
| `REQ-METER-010` | Temperature recovery requires strictly below hysteresis boundary | `test_temp_recovery_exactly_at_boundary` | `test_meter_recovery.c:269` |
| `REQ-METER-011` | Temperature recovery clears TEMP_HIGH when one degree below hysteresis boundary | `test_temp_recovery_one_below_boundary` | `test_meter_recovery.c:288` |
| `REQ-METER-012` | Mains meter countdown sequence from 3 to 0 then CT_NOCOMM on next tick | `test_mains_meter_countdown_to_error` | `test_meter_recovery.c:309` |

<details>
<summary>Detailed steps (12 scenarios)</summary>

### CT_NOCOMM error is set on timeout and cleared when communication restores
**Requirement:** `REQ-METER-001`

- **Given** EVSE is in Smart mode standalone with MainsMeterType=1 and MainsMeterTimeout=0
- **When** A 1-second tick sets CT_NOCOMM, then MainsMeterTimeout is restored to 5 and another tick occurs
- **Then** CT_NOCOMM is set after the first tick and cleared after the second tick

### EV_NOCOMM error is set on timeout and cleared when communication restores
**Requirement:** `REQ-METER-002`

- **Given** EVSE is in Smart mode with EVMeterType=1 and EVMeterTimeout=0
- **When** A 1-second tick sets EV_NOCOMM, then EVMeterTimeout is restored to 10 and another tick occurs
- **Then** EV_NOCOMM is set after the first tick and cleared after the second tick

### Both CT_NOCOMM and EV_NOCOMM can be set simultaneously and recover independently
**Requirement:** `REQ-METER-003`

- **Given** EVSE is in Smart mode with both MainsMeterTimeout=0 and EVMeterTimeout=0
- **When** Both timeouts expire, then mains meter is restored first, then EV meter is restored
- **Then** Each NOCOMM flag is set and cleared independently as its respective meter recovers

### Mains meter timeout during STATE_C triggers transition to STATE_C1
**Requirement:** `REQ-METER-004`

- **Given** EVSE is in Smart mode standalone in STATE_C with high mains load and MainsMeterTimeout=0
- **When** A 1-second tick occurs
- **Then** CT_NOCOMM is set and EVSE transitions from STATE_C to STATE_C1 (charging suspended)

### EV meter timeout during STATE_C triggers transition to STATE_C1
**Requirement:** `REQ-METER-005`

- **Given** EVSE is in Smart mode standalone in STATE_C with EVMeterType=1 and EVMeterTimeout=0
- **When** A 1-second tick occurs
- **Then** EV_NOCOMM is set and EVSE transitions from STATE_C to STATE_C1 (charging suspended)

### MainsMeter timeout on node sets CT_NOCOMM regardless of operating mode
**Requirement:** `REQ-METER-006`

- **Given** EVSE is a node (LoadBl=3) in Normal mode with MainsMeterTimeout=0
- **When** A 1-second tick occurs
- **Then** CT_NOCOMM is set because nodes do not have the MODE_NORMAL guard for timeout checks

### MainsMeter timeout on standalone in Normal mode does not set CT_NOCOMM
**Requirement:** `REQ-METER-007`

- **Given** EVSE is standalone (LoadBl=0) in MODE_NORMAL with MainsMeterType=1 and MainsMeterTimeout=0
- **When** A 1-second tick occurs
- **Then** CT_NOCOMM is not set because the MODE_NORMAL guard skips the timeout check for master/standalone

### No EV meter installed continuously resets EVMeterTimeout to COMM_EVTIMEOUT
**Requirement:** `REQ-METER-008`

- **Given** EVSE has EVMeterType=0 (no EV meter installed) with EVMeterTimeout artificially lowered
- **When** 1-second ticks occur even with EVMeterTimeout set to 0
- **Then** EVMeterTimeout is always reset to COMM_EVTIMEOUT and EV_NOCOMM is never set

### No mains meter type and standalone resets MainsMeterTimeout to COMM_TIMEOUT
**Requirement:** `REQ-METER-009`

- **Given** EVSE has MainsMeterType=0 and LoadBl=0 with MainsMeterTimeout artificially lowered to 3
- **When** A 1-second tick occurs
- **Then** MainsMeterTimeout is reset to COMM_TIMEOUT because no mains meter is configured

### Temperature recovery requires strictly below hysteresis boundary
**Requirement:** `REQ-METER-010`

- **Given** EVSE has TEMP_HIGH error with maxTemp=65 and TempEVSE=55 (exactly at maxTemp-10)
- **When** A 1-second tick occurs
- **Then** TEMP_HIGH error remains set because recovery requires TempEVSE < (maxTemp - 10), not <=

### Temperature recovery clears TEMP_HIGH when one degree below hysteresis boundary
**Requirement:** `REQ-METER-011`

- **Given** EVSE has TEMP_HIGH error with maxTemp=65 and TempEVSE=54 (one below maxTemp-10)
- **When** A 1-second tick occurs
- **Then** TEMP_HIGH error is cleared because 54 < 55 (maxTemp - 10) satisfies the recovery condition

### Mains meter countdown sequence from 3 to 0 then CT_NOCOMM on next tick
**Requirement:** `REQ-METER-012`

- **Given** EVSE is in Smart mode standalone with MainsMeterType=1 and MainsMeterTimeout=3
- **When** Four consecutive 1-second ticks occur decrementing the timeout
- **Then** CT_NOCOMM remains clear during countdown (3 to 0) and is set on the tick after reaching 0

</details>

---

## Modem / ISO15118 Negotiation

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-MODEM-001` | Entering MODEM_REQUEST disconnects the pilot signal | `test_modem_request_disconnects_pilot` | `test_modem_states.c:1` |
| `REQ-MODEM-002` | Entering MODEM_REQUEST sets PWM to off (+12V) | `test_modem_request_pwm_off` | `test_modem_states.c:38` |
| `REQ-MODEM-003` | Entering MODEM_REQUEST ensures contactors are open | `test_modem_request_contactors_off` | `test_modem_states.c:52` |
| `REQ-MODEM-004` | MODEM_REQUEST transitions to MODEM_WAIT after timer expires | `test_modem_request_to_wait_on_timer` | `test_modem_states.c:67` |
| `REQ-MODEM-005` | Modem states are NOT handled in tick_10ms (original behavior) | `test_modem_request_to_A_on_disconnect` | `test_modem_states.c:83` |
| `REQ-MODEM-006` | Entering MODEM_WAIT sets 5% PWM duty cycle for ISO15118 signalling | `test_modem_wait_5pct_duty` | `test_modem_states.c:100` |
| `REQ-MODEM-007` | Entering MODEM_WAIT reconnects the pilot signal | `test_modem_wait_pilot_connected` | `test_modem_states.c:114` |
| `REQ-MODEM-008` | Entering MODEM_WAIT sets ToModemDoneStateTimer to 60 seconds | `test_modem_wait_timer_set` | `test_modem_states.c:128` |
| `REQ-MODEM-009` | MODEM_WAIT transitions to MODEM_DONE after 60-second timeout | `test_modem_wait_to_done_after_timeout` | `test_modem_states.c:142` |
| `REQ-MODEM-010` | MODEM_WAIT is NOT handled in tick_10ms (original behavior) | `test_modem_wait_to_A_on_disconnect` | `test_modem_states.c:160` |
| `REQ-MODEM-011` | Entering MODEM_DONE disconnects the pilot signal | `test_modem_done_disconnects_pilot` | `test_modem_states.c:177` |
| `REQ-MODEM-012` | Entering MODEM_DONE sets LeaveModemDoneStateTimer to 5 seconds | `test_modem_done_timer_set` | `test_modem_states.c:191` |
| `REQ-MODEM-013` | MODEM_DONE transitions to STATE_B after 5-second timer with ModemStage=1 | `test_modem_done_to_B_after_timer` | `test_modem_states.c:205` |
| `REQ-MODEM-014` | MODEM_DONE is NOT handled in tick_10ms (original behavior) | `test_modem_done_to_A_on_disconnect` | `test_modem_states.c:223` |
| `REQ-MODEM-015` | MODEM_DENIED transitions to STATE_A after timer expires | `test_modem_denied_to_A_after_timer` | `test_modem_states.c:240` |
| `REQ-MODEM-016` | MODEM_DENIED is NOT handled in tick_10ms (original behavior) | `test_modem_denied_to_A_on_disconnect` | `test_modem_states.c:258` |
| `REQ-MODEM-M1A` | MODEM_WAIT timer=1 does not transition immediately on decrement | `test_modem_wait_timer_1_no_immediate_transition` | `test_modem_states.c:276` |
| `REQ-MODEM-M1B` | MODEM_DONE timer=1 does not transition immediately on decrement | `test_modem_done_timer_1_no_immediate_transition` | `test_modem_states.c:299` |
| `REQ-MODEM-M1C` | MODEM_DENIED timer=1 does not transition immediately on decrement | `test_modem_denied_timer_1_no_immediate_transition` | `test_modem_states.c:320` |
| `REQ-MODEM-M2A` | DisconnectTimeCounter starts on STATE_A entry when modem enabled | `test_disconnect_counter_starts_on_state_a` | `test_modem_states.c:343` |
| `REQ-MODEM-M2B` | DisconnectTimeCounter disabled on MODEM_REQUEST entry | `test_disconnect_counter_disabled_on_modem_request` | `test_modem_states.c:358` |
| `REQ-MODEM-M2C` | DisconnectTimeCounter disabled on MODEM_DONE entry | `test_disconnect_counter_disabled_on_modem_done` | `test_modem_states.c:373` |
| `REQ-MODEM-M2D` | DisconnectTimeCounter is NOT incremented in tick_1s (handled by firmware wrapper) | `test_disconnect_counter_increments_in_tick_1s` | `test_modem_states.c:388` |
| `REQ-MODEM-M2E` | DisconnectTimeCounter does not increment when disabled (-1) | `test_disconnect_counter_stays_disabled` | `test_modem_states.c:403` |
| `REQ-MODEM-EVCCID-001` | Empty RequiredEVCCID allows any vehicle | `test_evccid_empty_required_allows_any` | `test_modem_states.c:420` |
| `REQ-MODEM-EVCCID-002` | Matching EVCCID passes validation | `test_evccid_matching_passes` | `test_modem_states.c:441` |
| `REQ-MODEM-EVCCID-003` | Mismatched EVCCID triggers MODEM_DENIED | `test_evccid_mismatch_denied` | `test_modem_states.c:462` |
| `REQ-MODEM-EVCCID-004` | Full flow: EVCCID mismatch → DENIED → timeout → STATE_A | `test_evccid_mismatch_full_flow_to_a` | `test_modem_states.c:485` |
| `REQ-MODEM-017` | Full modem negotiation flow: REQUEST -> WAIT -> DONE -> STATE_B | `test_full_modem_flow` | `test_modem_states.c:515` |

<details>
<summary>Detailed steps (29 scenarios)</summary>

### Entering MODEM_REQUEST disconnects the pilot signal
**Requirement:** `REQ-MODEM-001`

- **Given** The EVSE is initialised with basic configuration
- **When** The state is set to STATE_MODEM_REQUEST
- **Then** pilot_connected is false

### Entering MODEM_REQUEST sets PWM to off (+12V)
**Requirement:** `REQ-MODEM-002`

- **Given** The EVSE is initialised with basic configuration
- **When** The state is set to STATE_MODEM_REQUEST
- **Then** PWM duty is set to 1024 (off / +12V constant)

### Entering MODEM_REQUEST ensures contactors are open
**Requirement:** `REQ-MODEM-003`

- **Given** The EVSE is initialised with basic configuration
- **When** The state is set to STATE_MODEM_REQUEST
- **Then** Both contactor1 and contactor2 are off (open)

### MODEM_REQUEST transitions to MODEM_WAIT after timer expires
**Requirement:** `REQ-MODEM-004`

- **Given** The EVSE is in STATE_MODEM_REQUEST with ToModemWaitStateTimer=0
- **When** One second tick occurs
- **Then** The state transitions to STATE_MODEM_WAIT

### Modem states are NOT handled in tick_10ms (original behavior)
**Requirement:** `REQ-MODEM-005`

- **Given** The EVSE is in STATE_MODEM_REQUEST
- **When** A 12V pilot signal is received during tick_10ms
- **Then** The state stays MODEM_REQUEST (modem is managed only by tick_1s timers)

### Entering MODEM_WAIT sets 5% PWM duty cycle for ISO15118 signalling
**Requirement:** `REQ-MODEM-006`

- **Given** The EVSE is initialised with basic configuration
- **When** The state is set to STATE_MODEM_WAIT
- **Then** PWM duty is set to 51 (5% duty cycle)

### Entering MODEM_WAIT reconnects the pilot signal
**Requirement:** `REQ-MODEM-007`

- **Given** The EVSE is initialised with basic configuration
- **When** The state is set to STATE_MODEM_WAIT
- **Then** pilot_connected is true

### Entering MODEM_WAIT sets ToModemDoneStateTimer to 60 seconds
**Requirement:** `REQ-MODEM-008`

- **Given** The EVSE is initialised with basic configuration
- **When** The state is set to STATE_MODEM_WAIT
- **Then** ToModemDoneStateTimer is set to 60

### MODEM_WAIT transitions to MODEM_DONE after 60-second timeout
**Requirement:** `REQ-MODEM-009`

- **Given** The EVSE is in STATE_MODEM_WAIT with 60-second timer
- **When** 61 second ticks occur (60 to decrement to 0, 1 more to fire transition)
- **Then** The state transitions to STATE_MODEM_DONE

### MODEM_WAIT is NOT handled in tick_10ms (original behavior)
**Requirement:** `REQ-MODEM-010`

- **Given** The EVSE is in STATE_MODEM_WAIT
- **When** A 12V pilot signal is received during tick_10ms
- **Then** The state stays MODEM_WAIT (modem is managed only by tick_1s timers)

### Entering MODEM_DONE disconnects the pilot signal
**Requirement:** `REQ-MODEM-011`

- **Given** The EVSE is initialised with basic configuration
- **When** The state is set to STATE_MODEM_DONE
- **Then** pilot_connected is false

### Entering MODEM_DONE sets LeaveModemDoneStateTimer to 5 seconds
**Requirement:** `REQ-MODEM-012`

- **Given** The EVSE is initialised with basic configuration
- **When** The state is set to STATE_MODEM_DONE
- **Then** LeaveModemDoneStateTimer is set to 5

### MODEM_DONE transitions to STATE_B after 5-second timer with ModemStage=1
**Requirement:** `REQ-MODEM-013`

- **Given** The EVSE is in STATE_MODEM_DONE with 5-second timer
- **When** 6 second ticks occur (5 to decrement to 0, 1 more to fire transition)
- **Then** The state transitions to STATE_B and ModemStage is set to 1

### MODEM_DONE is NOT handled in tick_10ms (original behavior)
**Requirement:** `REQ-MODEM-014`

- **Given** The EVSE is in STATE_MODEM_DONE
- **When** A 12V pilot signal is received during tick_10ms
- **Then** The state stays MODEM_DONE (modem is managed only by tick_1s timers)

### MODEM_DENIED transitions to STATE_A after timer expires
**Requirement:** `REQ-MODEM-015`

- **Given** The EVSE is in STATE_MODEM_DENIED with LeaveModemDeniedStateTimer=3
- **When** 4 second ticks occur (3 to decrement to 0, 1 more to fire transition)
- **Then** The state transitions to STATE_A

### MODEM_DENIED is NOT handled in tick_10ms (original behavior)
**Requirement:** `REQ-MODEM-016`

- **Given** The EVSE is in STATE_MODEM_DENIED with timer still running
- **When** A 12V pilot signal is received during tick_10ms
- **Then** The state stays MODEM_DENIED (modem is managed only by tick_1s timers)

### MODEM_WAIT timer=1 does not transition immediately on decrement
**Requirement:** `REQ-MODEM-M1A`

- **Given** EVSE is in STATE_MODEM_WAIT with ToModemDoneStateTimer=1
- **When** One second tick occurs (timer decrements to 0)
- **When** Another second tick occurs (timer is now 0, else branch fires)
- **Then** The EVSE stays in STATE_MODEM_WAIT (does not yet transition to MODEM_DONE)
- **Then** The EVSE transitions to STATE_MODEM_DONE

### MODEM_DONE timer=1 does not transition immediately on decrement
**Requirement:** `REQ-MODEM-M1B`

- **Given** EVSE is in STATE_MODEM_DONE with LeaveModemDoneStateTimer=1
- **When** One second tick occurs (timer decrements to 0)
- **When** Another second tick occurs
- **Then** The EVSE stays in STATE_MODEM_DONE
- **Then** The EVSE transitions to STATE_B (EVCCID accepted)

### MODEM_DENIED timer=1 does not transition immediately on decrement
**Requirement:** `REQ-MODEM-M1C`

- **Given** EVSE is in STATE_MODEM_DENIED with LeaveModemDeniedStateTimer=1
- **When** One second tick occurs (timer decrements to 0)
- **When** Another second tick occurs
- **Then** The EVSE stays in STATE_MODEM_DENIED
- **Then** The EVSE transitions to STATE_A

### DisconnectTimeCounter starts on STATE_A entry when modem enabled
**Requirement:** `REQ-MODEM-M2A`

- **Given** ModemEnabled=true, DisconnectTimeCounter=-1 (disabled)
- **When** State is set to STATE_A
- **Then** DisconnectTimeCounter is set to 0 (started)

### DisconnectTimeCounter disabled on MODEM_REQUEST entry
**Requirement:** `REQ-MODEM-M2B`

- **Given** DisconnectTimeCounter=5 (running)
- **When** State is set to STATE_MODEM_REQUEST
- **Then** DisconnectTimeCounter is set to -1 (disabled)

### DisconnectTimeCounter disabled on MODEM_DONE entry
**Requirement:** `REQ-MODEM-M2C`

- **Given** DisconnectTimeCounter=5 (running)
- **When** State is set to STATE_MODEM_DONE
- **Then** DisconnectTimeCounter is set to -1 (disabled)

### DisconnectTimeCounter is NOT incremented in tick_1s (handled by firmware wrapper)
**Requirement:** `REQ-MODEM-M2D`

- **Given** ModemEnabled=true, DisconnectTimeCounter=0
- **When** tick_1s occurs
- **Then** DisconnectTimeCounter stays 0 (firmware wrapper handles increment + pilot check)

### DisconnectTimeCounter does not increment when disabled (-1)
**Requirement:** `REQ-MODEM-M2E`

- **Given** ModemEnabled=true, DisconnectTimeCounter=-1
- **When** tick_1s occurs
- **Then** DisconnectTimeCounter remains -1

### Empty RequiredEVCCID allows any vehicle
**Requirement:** `REQ-MODEM-EVCCID-001`

- **Given** MODEM_DONE, LeaveModemDoneStateTimer expired, RequiredEVCCID=""
- **When** tick_1s processes MODEM_DONE timer expiry
- **Then** Transitions to STATE_B with ModemStage=1

### Matching EVCCID passes validation
**Requirement:** `REQ-MODEM-EVCCID-002`

- **Given** MODEM_DONE, timer expired, RequiredEVCCID matches EVCCID
- **When** tick_1s processes timer expiry
- **Then** Transitions to STATE_B with ModemStage=1

### Mismatched EVCCID triggers MODEM_DENIED
**Requirement:** `REQ-MODEM-EVCCID-003`

- **Given** MODEM_DONE, timer expired, RequiredEVCCID != EVCCID
- **When** tick_1s processes timer expiry
- **Then** Transitions to MODEM_DENIED, ModemStage=0, LeaveModemDeniedStateTimer=60

### Full flow: EVCCID mismatch → DENIED → timeout → STATE_A
**Requirement:** `REQ-MODEM-EVCCID-004`

- **Given** Modem flow reaches MODEM_DONE with wrong EVCCID
- **When** Timer expires and EVCCID doesn't match, then DENIED timer expires
- **Then** MODEM_DENIED → STATE_A after 60+1 seconds

### Full modem negotiation flow: REQUEST -> WAIT -> DONE -> STATE_B
**Requirement:** `REQ-MODEM-017`

- **Given** The EVSE is initialised with basic configuration
- **When** The modem negotiation proceeds through all stages with timers expiring
- **Then** The EVSE transitions REQUEST->WAIT->DONE->B with ModemStage=1 and correct PWM/pilot at each stage

</details>

---

## MQTT Command Parsing

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-MQTT-001` | Set mode to Normal via MQTT | `test_mode_normal` | `test_mqtt_parser.c:1` |
| `REQ-MQTT-001` | Set mode to Solar via MQTT | `test_mode_solar` | `test_mqtt_parser.c:32` |
| `REQ-MQTT-001` | Set mode to Smart via MQTT | `test_mode_smart` | `test_mqtt_parser.c:43` |
| `REQ-MQTT-001` | Set mode to Off via MQTT | `test_mode_off` | `test_mqtt_parser.c:54` |
| `REQ-MQTT-001` | Set mode to Pause via MQTT | `test_mode_pause` | `test_mqtt_parser.c:65` |
| `REQ-MQTT-003` | CustomButton set to On | `test_custom_button_on` | `test_mqtt_parser.c:90` |
| `REQ-MQTT-003` | CustomButton set to Off | `test_custom_button_off` | `test_mqtt_parser.c:101` |
| `REQ-MQTT-006` | CP PWM override normal mode (-1) | `test_cp_pwm_normal` | `test_mqtt_parser.c:192` |
| `REQ-MQTT-006` | CP PWM override disconnect (0) | `test_cp_pwm_disconnect` | `test_mqtt_parser.c:203` |
| `REQ-MQTT-006` | CP PWM override max value (1024) | `test_cp_pwm_max` | `test_mqtt_parser.c:213` |
| `REQ-MQTT-009` | Home battery current set | `test_home_battery_current` | `test_mqtt_parser.c:370` |
| `REQ-MQTT-009` | Home battery current negative (discharging) | `test_home_battery_current_negative` | `test_mqtt_parser.c:381` |
| `REQ-MQTT-011` | Cable lock enabled | `test_cable_lock_enable` | `test_mqtt_parser.c:473` |
| `REQ-MQTT-011` | Cable lock disabled | `test_cable_lock_disable` | `test_mqtt_parser.c:484` |
| `REQ-MQTT-011` | Cable lock any non-"1" disables | `test_cable_lock_any_other` | `test_mqtt_parser.c:494` |
| `REQ-MQTT-012` | EnableC2 numeric value | `test_enable_c2_numeric` | `test_mqtt_parser.c:506` |
| `REQ-MQTT-012` | EnableC2 string value | `test_enable_c2_string` | `test_mqtt_parser.c:517` |
| `REQ-MQTT-013` | RequiredEVCCID set | `test_required_evccid` | `test_mqtt_parser.c:547` |
| `REQ-MQTT-015` | PrioStrategy set to MODBUS_ADDR (0) via MQTT | `test_prio_strategy_modbus_addr` | `test_mqtt_parser.c:570` |
| `REQ-MQTT-015` | PrioStrategy set to FIRST_CONNECTED (1) | `test_prio_strategy_first_connected` | `test_mqtt_parser.c:584` |
| `REQ-MQTT-015` | PrioStrategy set to LAST_CONNECTED (2) | `test_prio_strategy_last_connected` | `test_mqtt_parser.c:594` |
| `REQ-MQTT-016` | RotationInterval set to 0 (disabled) via MQTT | `test_rotation_interval_zero` | `test_mqtt_parser.c:627` |
| `REQ-MQTT-016` | RotationInterval set to minimum (30 minutes) | `test_rotation_interval_min` | `test_mqtt_parser.c:641` |
| `REQ-MQTT-016` | RotationInterval set to maximum (1440 minutes = 24h) | `test_rotation_interval_max` | `test_mqtt_parser.c:651` |
| `REQ-MQTT-017` | IdleTimeout set to minimum (30 seconds) via MQTT | `test_idle_timeout_min` | `test_mqtt_parser.c:684` |
| `REQ-MQTT-017` | IdleTimeout set to default (60 seconds) | `test_idle_timeout_default` | `test_mqtt_parser.c:698` |
| `REQ-MQTT-017` | IdleTimeout set to maximum (300 seconds) | `test_idle_timeout_max` | `test_mqtt_parser.c:708` |

<details>
<summary>Detailed steps (27 scenarios)</summary>

### Set mode to Normal via MQTT
**Requirement:** `REQ-MQTT-001`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/Mode with payload "Normal"
- **Then** Command type is MQTT_CMD_MODE with mode MQTT_MODE_NORMAL

### Set mode to Solar via MQTT
**Requirement:** `REQ-MQTT-001`


### Set mode to Smart via MQTT
**Requirement:** `REQ-MQTT-001`


### Set mode to Off via MQTT
**Requirement:** `REQ-MQTT-001`


### Set mode to Pause via MQTT
**Requirement:** `REQ-MQTT-001`


### CustomButton set to On
**Requirement:** `REQ-MQTT-003`


### CustomButton set to Off
**Requirement:** `REQ-MQTT-003`


### CP PWM override normal mode (-1)
**Requirement:** `REQ-MQTT-006`


### CP PWM override disconnect (0)
**Requirement:** `REQ-MQTT-006`


### CP PWM override max value (1024)
**Requirement:** `REQ-MQTT-006`


### Home battery current set
**Requirement:** `REQ-MQTT-009`


### Home battery current negative (discharging)
**Requirement:** `REQ-MQTT-009`


### Cable lock enabled
**Requirement:** `REQ-MQTT-011`


### Cable lock disabled
**Requirement:** `REQ-MQTT-011`


### Cable lock any non-"1" disables
**Requirement:** `REQ-MQTT-011`


### EnableC2 numeric value
**Requirement:** `REQ-MQTT-012`


### EnableC2 string value
**Requirement:** `REQ-MQTT-012`


### RequiredEVCCID set
**Requirement:** `REQ-MQTT-013`


### PrioStrategy set to MODBUS_ADDR (0) via MQTT
**Requirement:** `REQ-MQTT-015`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/PrioStrategy with payload "0"
- **Then** Command type is MQTT_CMD_PRIO_STRATEGY with value 0

### PrioStrategy set to FIRST_CONNECTED (1)
**Requirement:** `REQ-MQTT-015`


### PrioStrategy set to LAST_CONNECTED (2)
**Requirement:** `REQ-MQTT-015`


### RotationInterval set to 0 (disabled) via MQTT
**Requirement:** `REQ-MQTT-016`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/RotationInterval with payload "0"
- **Then** Command type is MQTT_CMD_ROTATION_INTERVAL with value 0

### RotationInterval set to minimum (30 minutes)
**Requirement:** `REQ-MQTT-016`


### RotationInterval set to maximum (1440 minutes = 24h)
**Requirement:** `REQ-MQTT-016`


### IdleTimeout set to minimum (30 seconds) via MQTT
**Requirement:** `REQ-MQTT-017`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/IdleTimeout with payload "30"
- **Then** Command type is MQTT_CMD_IDLE_TIMEOUT with value 30

### IdleTimeout set to default (60 seconds)
**Requirement:** `REQ-MQTT-017`


### IdleTimeout set to maximum (300 seconds)
**Requirement:** `REQ-MQTT-017`


</details>

---

## MQTT Input Validation

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-MQTT-002` | Invalid mode string is rejected | `test_mode_invalid` | `test_mqtt_parser.c:76` |
| `REQ-MQTT-004` | Current override with valid value | `test_current_override_valid` | `test_mqtt_parser.c:114` |
| `REQ-MQTT-004` | Current override zero resets override | `test_current_override_zero` | `test_mqtt_parser.c:128` |
| `REQ-MQTT-004` | Current override with max value | `test_current_override_max` | `test_mqtt_parser.c:139` |
| `REQ-MQTT-005` | Max sum mains valid value | `test_max_sum_mains_valid` | `test_mqtt_parser.c:151` |
| `REQ-MQTT-005` | Max sum mains zero disables | `test_max_sum_mains_zero` | `test_mqtt_parser.c:162` |
| `REQ-MQTT-005` | Max sum mains below minimum rejected | `test_max_sum_mains_below_min` | `test_mqtt_parser.c:172` |
| `REQ-MQTT-005` | Max sum mains above maximum rejected | `test_max_sum_mains_above_max` | `test_mqtt_parser.c:181` |
| `REQ-MQTT-006` | CP PWM override out of range rejected | `test_cp_pwm_out_of_range` | `test_mqtt_parser.c:223` |
| `REQ-MQTT-006` | CP PWM override below -1 rejected | `test_cp_pwm_below_neg1` | `test_mqtt_parser.c:232` |
| `REQ-MQTT-007` | Mains meter out of range rejected (>=2000) | `test_mains_meter_out_of_range` | `test_mqtt_parser.c:272` |
| `REQ-MQTT-007` | Mains meter out of range rejected (<=-2000) | `test_mains_meter_out_of_range_neg` | `test_mqtt_parser.c:282` |
| `REQ-MQTT-007` | Mains meter missing fields rejected | `test_mains_meter_missing_fields` | `test_mqtt_parser.c:292` |
| `REQ-MQTT-008` | EV meter partial data rejected | `test_ev_meter_partial` | `test_mqtt_parser.c:345` |
| `REQ-MQTT-010` | RGB color out of range rejected | `test_rgb_out_of_range` | `test_mqtt_parser.c:406` |
| `REQ-MQTT-010` | RGB color negative rejected | `test_rgb_negative` | `test_mqtt_parser.c:416` |
| `REQ-MQTT-010` | RGB color missing component rejected | `test_rgb_missing` | `test_mqtt_parser.c:426` |
| `REQ-MQTT-012` | EnableC2 out of range rejected | `test_enable_c2_out_of_range` | `test_mqtt_parser.c:527` |
| `REQ-MQTT-012` | EnableC2 invalid string rejected | `test_enable_c2_invalid_string` | `test_mqtt_parser.c:536` |
| `REQ-MQTT-013` | RequiredEVCCID too long rejected | `test_required_evccid_too_long` | `test_mqtt_parser.c:558` |
| `REQ-MQTT-015` | PrioStrategy value 3 is rejected (out of range) | `test_prio_strategy_out_of_range` | `test_mqtt_parser.c:604` |
| `REQ-MQTT-015` | PrioStrategy negative value is rejected | `test_prio_strategy_negative` | `test_mqtt_parser.c:616` |
| `REQ-MQTT-016` | RotationInterval in gap (1-29) is rejected | `test_rotation_interval_gap` | `test_mqtt_parser.c:661` |
| `REQ-MQTT-016` | RotationInterval above maximum is rejected | `test_rotation_interval_too_high` | `test_mqtt_parser.c:673` |
| `REQ-MQTT-017` | IdleTimeout below minimum (29) is rejected | `test_idle_timeout_too_low` | `test_mqtt_parser.c:718` |
| `REQ-MQTT-017` | IdleTimeout above maximum (301) is rejected | `test_idle_timeout_too_high` | `test_mqtt_parser.c:730` |
| `REQ-MQTT-017` | IdleTimeout zero is rejected (minimum is 30) | `test_idle_timeout_zero` | `test_mqtt_parser.c:739` |
| `REQ-MQTT-014` | Unrecognized topic returns false | `test_unrecognized_topic` | `test_mqtt_parser.c:750` |
| `REQ-MQTT-014` | Wrong prefix returns false | `test_wrong_prefix` | `test_mqtt_parser.c:759` |

<details>
<summary>Detailed steps (29 scenarios)</summary>

### Invalid mode string is rejected
**Requirement:** `REQ-MQTT-002`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/Mode with payload "Invalid"
- **Then** The parser returns false

### Current override with valid value
**Requirement:** `REQ-MQTT-004`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/CurrentOverride with payload "100"
- **Then** Command has current_override = 100

### Current override zero resets override
**Requirement:** `REQ-MQTT-004`


### Current override with max value
**Requirement:** `REQ-MQTT-004`


### Max sum mains valid value
**Requirement:** `REQ-MQTT-005`


### Max sum mains zero disables
**Requirement:** `REQ-MQTT-005`


### Max sum mains below minimum rejected
**Requirement:** `REQ-MQTT-005`


### Max sum mains above maximum rejected
**Requirement:** `REQ-MQTT-005`


### CP PWM override out of range rejected
**Requirement:** `REQ-MQTT-006`


### CP PWM override below -1 rejected
**Requirement:** `REQ-MQTT-006`


### Mains meter out of range rejected (>=2000)
**Requirement:** `REQ-MQTT-007`


### Mains meter out of range rejected (<=-2000)
**Requirement:** `REQ-MQTT-007`


### Mains meter missing fields rejected
**Requirement:** `REQ-MQTT-007`


### EV meter partial data rejected
**Requirement:** `REQ-MQTT-008`


### RGB color out of range rejected
**Requirement:** `REQ-MQTT-010`


### RGB color negative rejected
**Requirement:** `REQ-MQTT-010`


### RGB color missing component rejected
**Requirement:** `REQ-MQTT-010`


### EnableC2 out of range rejected
**Requirement:** `REQ-MQTT-012`


### EnableC2 invalid string rejected
**Requirement:** `REQ-MQTT-012`


### RequiredEVCCID too long rejected
**Requirement:** `REQ-MQTT-013`


### PrioStrategy value 3 is rejected (out of range)
**Requirement:** `REQ-MQTT-015`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/PrioStrategy with payload "3"
- **Then** The parser returns false

### PrioStrategy negative value is rejected
**Requirement:** `REQ-MQTT-015`


### RotationInterval in gap (1-29) is rejected
**Requirement:** `REQ-MQTT-016`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/RotationInterval with payload "15"
- **Then** The parser returns false

### RotationInterval above maximum is rejected
**Requirement:** `REQ-MQTT-016`


### IdleTimeout below minimum (29) is rejected
**Requirement:** `REQ-MQTT-017`

- **Given** A valid MQTT prefix
- **When** Topic is prefix/Set/IdleTimeout with payload "29"
- **Then** The parser returns false

### IdleTimeout above maximum (301) is rejected
**Requirement:** `REQ-MQTT-017`


### IdleTimeout zero is rejected (minimum is 30)
**Requirement:** `REQ-MQTT-017`


### Unrecognized topic returns false
**Requirement:** `REQ-MQTT-014`


### Wrong prefix returns false
**Requirement:** `REQ-MQTT-014`


</details>

---

## MQTT Meter Parsing

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-MQTT-007` | Mains meter format L1:L2:L3 is parsed correctly | `test_mains_meter_valid` | `test_mqtt_parser.c:243` |
| `REQ-MQTT-007` | Mains meter with negative values | `test_mains_meter_negative` | `test_mqtt_parser.c:259` |
| `REQ-MQTT-007` | Mains meter via full command parse | `test_mains_meter_command` | `test_mqtt_parser.c:302` |
| `REQ-MQTT-008` | EV meter format L1:L2:L3:W:WH is parsed correctly | `test_ev_meter_valid` | `test_mqtt_parser.c:317` |
| `REQ-MQTT-008` | EV meter with unknown values (-1) | `test_ev_meter_unknown_values` | `test_mqtt_parser.c:332` |
| `REQ-MQTT-008` | EV meter via full command parse | `test_ev_meter_command` | `test_mqtt_parser.c:355` |

<details>
<summary>Detailed steps (6 scenarios)</summary>

### Mains meter format L1:L2:L3 is parsed correctly
**Requirement:** `REQ-MQTT-007`

- **Given** Valid three-phase mains meter data
- **When** Payload is "100:200:300"
- **Then** L1=100, L2=200, L3=300

### Mains meter with negative values
**Requirement:** `REQ-MQTT-007`


### Mains meter via full command parse
**Requirement:** `REQ-MQTT-007`


### EV meter format L1:L2:L3:W:WH is parsed correctly
**Requirement:** `REQ-MQTT-008`


### EV meter with unknown values (-1)
**Requirement:** `REQ-MQTT-008`


### EV meter via full command parse
**Requirement:** `REQ-MQTT-008`


</details>

---

## MQTT Color Parsing

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-MQTT-010` | Valid RGB color parsed | `test_rgb_valid` | `test_mqtt_parser.c:393` |
| `REQ-MQTT-010` | ColorOff topic parsed correctly | `test_color_off_command` | `test_mqtt_parser.c:436` |
| `REQ-MQTT-010` | ColorSolar topic parsed correctly | `test_color_solar_command` | `test_mqtt_parser.c:450` |
| `REQ-MQTT-010` | ColorCustom topic parsed correctly | `test_color_custom_command` | `test_mqtt_parser.c:461` |

<details>
<summary>Detailed steps (4 scenarios)</summary>

### Valid RGB color parsed
**Requirement:** `REQ-MQTT-010`


### ColorOff topic parsed correctly
**Requirement:** `REQ-MQTT-010`


### ColorSolar topic parsed correctly
**Requirement:** `REQ-MQTT-010`


### ColorCustom topic parsed correctly
**Requirement:** `REQ-MQTT-010`


</details>

---

## Multi-Node Load Balancing

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-MULTI-001` | Four EVSEs receive equal current distribution | `test_four_evse_fair_distribution` | `test_multi_node.c:1` |
| `REQ-MULTI-002` | Master EVSE BalancedMax is derived from ChargeCurrent | `test_four_evse_master_max_from_chargecurrent` | `test_multi_node.c:74` |
| `REQ-MULTI-003` | One EVSE with low max capacity, remainder shared by others | `test_one_evse_low_max_others_share` | `test_multi_node.c:102` |
| `REQ-MULTI-004` | Node going offline causes redistribution to remaining active nodes | `test_node_goes_offline_redistributes` | `test_multi_node.c:130` |
| `REQ-MULTI-005` | Priority scheduling allocates available power to highest-priority EVSE during shortage | `test_all_nodes_mincurrent_during_shortage` | `test_multi_node.c:159` |
| `REQ-MULTI-006` | MaxCircuit limits total current distribution across all nodes | `test_maxcircuit_limits_total_distribution` | `test_multi_node.c:189` |
| `REQ-MULTI-007` | MaxCircuit accounts for EV meter baseload in distribution | `test_maxcircuit_with_ev_meter_baseload` | `test_multi_node.c:217` |
| `REQ-MULTI-008` | Six EVSEs in large cluster receive fair distribution | `test_six_evse_fair_distribution` | `test_multi_node.c:243` |
| `REQ-MULTI-009` | NoCurrent counter increments during hard power shortage | `test_nocurrent_increments_on_hard_shortage` | `test_multi_node.c:269` |
| `REQ-MULTI-010` | NoCurrent counter clears when sufficient power is available | `test_nocurrent_zero_when_sufficient` | `test_multi_node.c:294` |
| `REQ-MULTI-011` | Node in STATE_B does not participate in current distribution | `test_state_b_node_gets_no_current` | `test_multi_node.c:319` |
| `REQ-MULTI-012` | IsetBalanced is capped at the sum of all active node maximums | `test_isetbalanced_capped_at_active_max` | `test_multi_node.c:344` |
| `REQ-MULTI-013` | Three EVSEs with all different BalancedMax values | `test_three_evse_all_different_max` | `test_multi_node.c:369` |
| `REQ-MULTI-014` | Tight circuit with unequal max: surplus from small EVSE redistributed | `test_unequal_max_tight_circuit` | `test_multi_node.c:400` |

<details>
<summary>Detailed steps (14 scenarios)</summary>

### Four EVSEs receive equal current distribution
**Requirement:** `REQ-MULTI-001`

- **Given** Master with 4 EVSEs all in STATE_C, MaxCircuit=64A, no EV meter baseload
- **When** Balanced current is calculated
- **Then** Each EVSE receives 160 deciamps (640 / 4 = 160) with all values equal

### Master EVSE BalancedMax is derived from ChargeCurrent
**Requirement:** `REQ-MULTI-002`

- **Given** Master with 4 EVSEs all in STATE_C, master ChargeCurrent limited to 200 deciamps (20A)
- **When** Balanced current is calculated
- **Then** Master EVSE (Balanced[0]) does not exceed its ChargeCurrent limit of 200 deciamps

### One EVSE with low max capacity, remainder shared by others
**Requirement:** `REQ-MULTI-003`

- **Given** Master with 3 EVSEs in STATE_C, node 1 limited to BalancedMax=60 deciamps (6A)
- **When** Balanced current is calculated
- **Then** Node 1 receives at most 60 deciamps while nodes 0 and 2 share the remainder equally

### Node going offline causes redistribution to remaining active nodes
**Requirement:** `REQ-MULTI-004`

- **Given** Master with 3 EVSEs in STATE_C receiving equal distribution
- **When** Node 2 goes offline (BalancedState changes to STATE_A) and current is recalculated
- **Then** Remaining active nodes each receive more current than before and offline node gets 0

### Priority scheduling allocates available power to highest-priority EVSE during shortage
**Requirement:** `REQ-MULTI-005`

- **Given** Master with 4 EVSEs in STATE_C in Smart mode, available power ~105 dA (enough for 1 EVSE, not 4)
- **When** Balanced current is calculated with insufficient power for all nodes
- **Then** Highest-priority EVSE gets all available power, others are paused

### MaxCircuit limits total current distribution across all nodes
**Requirement:** `REQ-MULTI-006`

- **Given** Master with 4 EVSEs in STATE_C in Normal mode with MaxCircuit=24A
- **When** Balanced current is calculated
- **Then** Total distributed current across all nodes does not exceed 240 deciamps (MaxCircuit * 10)

### MaxCircuit accounts for EV meter baseload in distribution
**Requirement:** `REQ-MULTI-007`

- **Given** Master with 2 EVSEs in STATE_C, MaxCircuit=20A, EV meter measuring 250 deciamps total
- **When** Balanced current is calculated with EV meter baseload subtracted
- **Then** Total distributed current does not exceed (MaxCircuit * 10) minus baseload

### Six EVSEs in large cluster receive fair distribution
**Requirement:** `REQ-MULTI-008`

- **Given** Master with 6 EVSEs all in STATE_C, MaxCircuit=64A
- **When** Balanced current is calculated
- **Then** All 6 EVSEs receive equal current within 1 deciamp tolerance (integer division rounding)

### NoCurrent counter increments during hard power shortage
**Requirement:** `REQ-MULTI-009`

- **Given** Master with 4 EVSEs in Smart mode, 50A mains measured against 25A limit, IsetBalanced too low
- **When** Balanced current is calculated and total MinCurrent demand exceeds available power
- **Then** NoCurrent counter increments above 0 indicating sustained shortage

### NoCurrent counter clears when sufficient power is available
**Requirement:** `REQ-MULTI-010`

- **Given** Master with 2 EVSEs in Smart mode, low mains load, IsetBalanced=400, NoCurrent previously at 5
- **When** Balanced current is calculated with plenty of available power
- **Then** NoCurrent counter is cleared to 0

### Node in STATE_B does not participate in current distribution
**Requirement:** `REQ-MULTI-011`

- **Given** Master with 3 EVSEs, nodes 0 and 2 in STATE_C, node 1 in STATE_B (waiting)
- **When** Balanced current is calculated
- **Then** Only active STATE_C nodes (0 and 2) receive distributed current, each getting 320 deciamps

### IsetBalanced is capped at the sum of all active node maximums
**Requirement:** `REQ-MULTI-012`

- **Given** Master with 2 EVSEs in STATE_C, node 1 limited to BalancedMax=80 deciamps (8A)
- **When** Balanced current is calculated with IsetBalanced exceeding ActiveMax (320+80=400)
- **Then** IsetBalanced is capped to 400, node 0 gets 320 and node 1 gets 80

### Three EVSEs with all different BalancedMax values
**Requirement:** `REQ-MULTI-013`

- **Given** Master with 3 EVSEs: max 320, 160, 80 deciamps, MaxCircuit=64A
- **When** Balanced current is calculated
- **Then** Each EVSE capped at its max, remainder redistributed to uncapped EVSEs

### Tight circuit with unequal max: surplus from small EVSE redistributed
**Requirement:** `REQ-MULTI-014`

- **Given** 2 EVSEs: EVSE[0] max 320, EVSE[1] max 60 (MinCurrent), MaxCircuit=25A
- **When** Balanced current is calculated
- **Then** EVSE[1] gets 60, EVSE[0] gets remainder (250-60=190)

</details>

---

## OCPP Current Limiting

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-OCPP-001` | OCPP current limit exactly at MinCurrent boundary is accepted | `test_ocpp_limit_equal_to_mincurrent` | `test_ocpp.c:1` |
| `REQ-OCPP-002` | OCPP current limit exactly at MaxCurrent does not reduce current | `test_ocpp_limit_equal_to_maxcurrent` | `test_ocpp.c:60` |
| `REQ-OCPP-003` | OCPP current limit above MaxCurrent does not increase current beyond MaxCurrent | `test_ocpp_limit_above_maxcurrent_no_increase` | `test_ocpp.c:78` |
| `REQ-OCPP-004` | OCPP current limit is ignored when LoadBl is set to master | `test_ocpp_ignored_when_loadbl_master` | `test_ocpp.c:96` |
| `REQ-OCPP-005` | OCPP current limit is ignored when LoadBl is set to node | `test_ocpp_ignored_when_loadbl_node` | `test_ocpp.c:115` |
| `REQ-OCPP-006` | OverrideCurrent takes precedence over OCPP limit | `test_override_current_overrides_ocpp` | `test_ocpp.c:134` |
| `REQ-OCPP-007` | OverrideCurrent restores charging even when OCPP would zero the current | `test_override_current_overrides_ocpp_zero` | `test_ocpp.c:151` |
| `REQ-OCPP-008` | OCPP current limit of 0.0A zeros the charge current | `test_ocpp_limit_zero_zeros_current` | `test_ocpp.c:170` |
| `REQ-OCPP-009` | Negative OCPP current limit is treated as no limit set | `test_ocpp_negative_limit_no_restriction` | `test_ocpp.c:188` |
| `REQ-OCPP-010` | OCPP limit of 0.0A blocks current availability check | `test_ocpp_blocks_current_available_at_zero` | `test_ocpp.c:206` |
| `REQ-OCPP-011` | OCPP limit at MinCurrent allows current availability | `test_ocpp_allows_current_available_at_mincurrent` | `test_ocpp.c:223` |
| `REQ-OCPP-012` | Negative OCPP limit does not block current availability | `test_ocpp_negative_limit_allows_current_available` | `test_ocpp.c:241` |

<details>
<summary>Detailed steps (12 scenarios)</summary>

### OCPP current limit exactly at MinCurrent boundary is accepted
**Requirement:** `REQ-OCPP-001`

- **Given** EVSE is standalone in STATE_C with OcppMode enabled and MinCurrent=6A
- **When** OcppCurrentLimit is set to 6.0A (exactly MinCurrent) and balanced current is calculated
- **Then** ChargeCurrent is set to 60 deciamps (6A) because the limit equals MinCurrent

### OCPP current limit exactly at MaxCurrent does not reduce current
**Requirement:** `REQ-OCPP-002`

- **Given** EVSE is standalone in STATE_C with OcppMode enabled and MaxCurrent=16A
- **When** OcppCurrentLimit is set to 16.0A (exactly MaxCurrent) and balanced current is calculated
- **Then** ChargeCurrent remains at 160 deciamps (16A) because OCPP limit does not cap below MaxCurrent

### OCPP current limit above MaxCurrent does not increase current beyond MaxCurrent
**Requirement:** `REQ-OCPP-003`

- **Given** EVSE is standalone in STATE_C with OcppMode enabled and MaxCurrent=16A
- **When** OcppCurrentLimit is set to 32.0A (well above MaxCurrent) and balanced current is calculated
- **Then** ChargeCurrent stays at 160 deciamps (MaxCurrent) because OCPP cannot raise current above hardware limit

### OCPP current limit is ignored when LoadBl is set to master
**Requirement:** `REQ-OCPP-004`

- **Given** EVSE is a master (LoadBl=1) in STATE_C with OcppMode enabled
- **When** OcppCurrentLimit is set to 3.0A (below MinCurrent) and balanced current is calculated
- **Then** ChargeCurrent remains at 160 deciamps because OCPP limit requires standalone mode (LoadBl=0)

### OCPP current limit is ignored when LoadBl is set to node
**Requirement:** `REQ-OCPP-005`

- **Given** EVSE is a node (LoadBl=2) in STATE_C with OcppMode enabled
- **When** OcppCurrentLimit is set to 3.0A (below MinCurrent) and balanced current is calculated
- **Then** ChargeCurrent remains at 160 deciamps because OCPP limit requires standalone mode (LoadBl=0)

### OverrideCurrent takes precedence over OCPP limit
**Requirement:** `REQ-OCPP-006`

- **Given** EVSE is standalone in STATE_C with OcppCurrentLimit=10.0A and OverrideCurrent=80 deciamps
- **When** Balanced current is calculated
- **Then** ChargeCurrent is set to 80 deciamps because OverrideCurrent is applied after OCPP capping

### OverrideCurrent restores charging even when OCPP would zero the current
**Requirement:** `REQ-OCPP-007`

- **Given** EVSE is standalone in STATE_C with OcppCurrentLimit=3.0A (below MinCurrent) and OverrideCurrent=120
- **When** Balanced current is calculated
- **Then** ChargeCurrent is set to 120 deciamps because OverrideCurrent overrides the OCPP-zeroed value

### OCPP current limit of 0.0A zeros the charge current
**Requirement:** `REQ-OCPP-008`

- **Given** EVSE is standalone in STATE_C with OcppMode enabled
- **When** OcppCurrentLimit is set to 0.0A and balanced current is calculated
- **Then** ChargeCurrent is zeroed because 0.0A is below MinCurrent (6A)

### Negative OCPP current limit is treated as no limit set
**Requirement:** `REQ-OCPP-009`

- **Given** EVSE is standalone in STATE_C with OcppMode enabled
- **When** OcppCurrentLimit is set to -1.0A (default init value meaning no limit)
- **Then** ChargeCurrent remains at 160 deciamps because the OCPP capping block is skipped

### OCPP limit of 0.0A blocks current availability check
**Requirement:** `REQ-OCPP-010`

- **Given** EVSE is standalone with OcppMode enabled and OcppCurrentLimit=0.0A
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because OCPP limit is below MinCurrent

### OCPP limit at MinCurrent allows current availability
**Requirement:** `REQ-OCPP-011`

- **Given** EVSE is standalone with OcppMode enabled and OcppCurrentLimit=6.0A (MinCurrent)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because 6.0A is not less than MinCurrent

### Negative OCPP limit does not block current availability
**Requirement:** `REQ-OCPP-012`

- **Given** EVSE is standalone with OcppMode enabled and OcppCurrentLimit=-1.0A (no limit)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because negative limit skips the OCPP availability check

</details>

---

## Operating Modes

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-MODE-001` | Normal mode sets IsetBalanced to MaxCurrent | `test_normal_mode_uses_max_current` | `test_operating_modes.c:1` |
| `REQ-MODE-002` | Normal mode ignores mains meter readings | `test_normal_mode_ignores_mains` | `test_operating_modes.c:45` |
| `REQ-MODE-003` | Normal mode respects MaxCapacity as upper bound | `test_normal_mode_respects_max_capacity` | `test_operating_modes.c:62` |
| `REQ-MODE-004` | Smart mode limits current based on MaxMains minus baseload | `test_smart_mode_respects_maxmains` | `test_operating_modes.c:81` |
| `REQ-MODE-005` | Smart mode increases current conservatively (Idifference/4) | `test_smart_mode_slow_increase` | `test_operating_modes.c:101` |
| `REQ-MODE-006` | Smart mode decreases current rapidly when over mains limit | `test_smart_mode_fast_decrease` | `test_operating_modes.c:123` |
| `REQ-MODE-007` | Solar mode requires surplus power to make current available | `test_solar_current_available_requires_surplus` | `test_operating_modes.c:146` |
| `REQ-MODE-008` | Solar mode allows charging when sufficient surplus is available | `test_solar_current_available_with_surplus` | `test_operating_modes.c:165` |
| `REQ-MODE-009` | Solar mode increases current in small steps when surplus is available | `test_solar_fine_grained_increase` | `test_operating_modes.c:185` |
| `REQ-MODE-010` | Solar mode decreases current rapidly when importing from grid | `test_solar_rapid_decrease_on_import` | `test_operating_modes.c:209` |
| `REQ-MODE-011` | Solar mode ImportCurrent offset allows controlled grid import | `test_solar_import_current_offset` | `test_operating_modes.c:233` |
| `REQ-MODE-012` | EnableC2=NOT_PRESENT does not force single phase | `test_force_single_phase_not_present` | `test_operating_modes.c:259` |
| `REQ-MODE-013` | EnableC2=ALWAYS_OFF forces single phase operation | `test_force_single_phase_always_off` | `test_operating_modes.c:273` |
| `REQ-MODE-014` | EnableC2=SOLAR_OFF forces single phase when in Solar mode | `test_force_single_phase_solar_off_in_solar_mode` | `test_operating_modes.c:287` |
| `REQ-MODE-015` | EnableC2=SOLAR_OFF does not force single phase in Smart mode | `test_force_single_phase_solar_off_in_smart_mode` | `test_operating_modes.c:302` |
| `REQ-MODE-016` | EnableC2=AUTO forces single phase when charging on 1 phase | `test_force_single_phase_auto_c2_1p` | `test_operating_modes.c:317` |
| `REQ-MODE-017` | EnableC2=AUTO does not force single phase when charging on 3 phases | `test_force_single_phase_auto_c2_3p` | `test_operating_modes.c:332` |
| `REQ-MODE-018` | EnableC2=ALWAYS_ON does not force single phase | `test_force_single_phase_always_on` | `test_operating_modes.c:347` |
| `REQ-MODE-019` | STATE_C entry with single phase disables contactor 2 | `test_state_C_contactor2_off_when_single_phase` | `test_operating_modes.c:361` |
| `REQ-MODE-020` | STATE_C entry with three phase enables both contactors | `test_state_C_contactor2_on_when_three_phase` | `test_operating_modes.c:378` |
| `REQ-MODE-021` | Phase switch from 3P to 1P completes on STATE_C entry | `test_phase_switch_going_to_1p` | `test_operating_modes.c:395` |

<details>
<summary>Detailed steps (21 scenarios)</summary>

### Normal mode sets IsetBalanced to MaxCurrent
**Requirement:** `REQ-MODE-001`

- **Given** EVSE is standalone in STATE_C in Normal mode
- **When** Balanced current is calculated
- **Then** IsetBalanced equals MaxCurrent * 10 (fixed current allocation)

### Normal mode ignores mains meter readings
**Requirement:** `REQ-MODE-002`

- **Given** EVSE is standalone in STATE_C in Normal mode with high MainsMeterImeasured=300
- **When** Balanced current is calculated
- **Then** IsetBalanced remains at MaxCurrent * 10 regardless of mains load

### Normal mode respects MaxCapacity as upper bound
**Requirement:** `REQ-MODE-003`

- **Given** EVSE is standalone in STATE_C in Normal mode with MaxCapacity=10A and MaxCurrent=16A
- **When** Balanced current is calculated
- **Then** ChargeCurrent is limited to 100 deciamps (MaxCapacity * 10) instead of MaxCurrent

### Smart mode limits current based on MaxMains minus baseload
**Requirement:** `REQ-MODE-004`

- **Given** EVSE is standalone in STATE_C in Smart mode with MaxMains=25A and MainsMeterImeasured=200
- **When** Balanced current is calculated
- **Then** IsetBalanced does not exceed (MaxMains * 10) minus baseload

### Smart mode increases current conservatively (Idifference/4)
**Requirement:** `REQ-MODE-005`

- **Given** EVSE is standalone in STATE_C in Smart mode with low mains usage and measurements updated
- **When** Balanced current is calculated with headroom available
- **Then** IsetBalanced increases from its initial value but conservatively (not full step)

### Smart mode decreases current rapidly when over mains limit
**Requirement:** `REQ-MODE-006`

- **Given** EVSE is standalone in STATE_C in Smart mode with IsetBalanced=200 and mains way over MaxMains=10A
- **When** Balanced current is calculated with negative Idifference
- **Then** IsetBalanced decreases rapidly (full Idifference, not divided) below the initial 200

### Solar mode requires surplus power to make current available
**Requirement:** `REQ-MODE-007`

- **Given** EVSE is in Solar mode with StartCurrent=6A and Isum=0 (no surplus)
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because there is no solar surplus for charging

### Solar mode allows charging when sufficient surplus is available
**Requirement:** `REQ-MODE-008`

- **Given** EVSE is in Solar mode with StartCurrent=6A and Isum=-80 (8A export surplus)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because export surplus exceeds StartCurrent threshold

### Solar mode increases current in small steps when surplus is available
**Requirement:** `REQ-MODE-009`

- **Given** EVSE is standalone in STATE_C in Solar mode with 2A export surplus and past solar startup phase
- **When** Balanced current is calculated
- **Then** IsetBalanced increases from its initial 100 value in fine-grained solar increments

### Solar mode decreases current rapidly when importing from grid
**Requirement:** `REQ-MODE-010`

- **Given** EVSE is standalone in STATE_C in Solar mode with Isum=50 (5A import) and IsetBalanced=100
- **When** Balanced current is calculated with grid import detected
- **Then** IsetBalanced decreases below 100 to reduce grid import quickly

### Solar mode ImportCurrent offset allows controlled grid import
**Requirement:** `REQ-MODE-011`

- **Given** EVSE is in Solar mode with ImportCurrent=3A allowance and Isum=20 (2A import within allowance)
- **When** Balanced current is calculated with import within the allowed offset
- **Then** IsetBalanced increases because IsumImport (20 - 30 = -10) indicates effective surplus

### EnableC2=NOT_PRESENT does not force single phase
**Requirement:** `REQ-MODE-012`

- **Given** EVSE has EnableC2 set to NOT_PRESENT (contactor 2 not installed)
- **When** evse_force_single_phase is called
- **Then** Returns 0 because the phase switching hardware is not present

### EnableC2=ALWAYS_OFF forces single phase operation
**Requirement:** `REQ-MODE-013`

- **Given** EVSE has EnableC2 set to ALWAYS_OFF (contactor 2 always disabled)
- **When** evse_force_single_phase is called
- **Then** Returns 1 because the EVSE is configured to always operate in single phase

### EnableC2=SOLAR_OFF forces single phase when in Solar mode
**Requirement:** `REQ-MODE-014`

- **Given** EVSE has EnableC2 set to SOLAR_OFF and Mode is MODE_SOLAR
- **When** evse_force_single_phase is called
- **Then** Returns 1 because SOLAR_OFF disables contactor 2 in solar mode

### EnableC2=SOLAR_OFF does not force single phase in Smart mode
**Requirement:** `REQ-MODE-015`

- **Given** EVSE has EnableC2 set to SOLAR_OFF and Mode is MODE_SMART
- **When** evse_force_single_phase is called
- **Then** Returns 0 because SOLAR_OFF only applies in Solar mode, not Smart mode

### EnableC2=AUTO forces single phase when charging on 1 phase
**Requirement:** `REQ-MODE-016`

- **Given** EVSE has EnableC2 set to AUTO and Nr_Of_Phases_Charging=1
- **When** evse_force_single_phase is called
- **Then** Returns 1 because AUTO mode follows the current phase count

### EnableC2=AUTO does not force single phase when charging on 3 phases
**Requirement:** `REQ-MODE-017`

- **Given** EVSE has EnableC2 set to AUTO and Nr_Of_Phases_Charging=3
- **When** evse_force_single_phase is called
- **Then** Returns 0 because AUTO mode allows 3-phase operation when already on 3 phases

### EnableC2=ALWAYS_ON does not force single phase
**Requirement:** `REQ-MODE-018`

- **Given** EVSE has EnableC2 set to ALWAYS_ON (contactor 2 always enabled for 3-phase)
- **When** evse_force_single_phase is called
- **Then** Returns 0 because the EVSE is configured to always operate in three phase

### STATE_C entry with single phase disables contactor 2
**Requirement:** `REQ-MODE-019`

- **Given** EVSE has EnableC2 set to ALWAYS_OFF (force single phase)
- **When** EVSE transitions to STATE_C
- **Then** Contactor 1 is on, contactor 2 is off, and Nr_Of_Phases_Charging is 1

### STATE_C entry with three phase enables both contactors
**Requirement:** `REQ-MODE-020`

- **Given** EVSE has EnableC2 set to NOT_PRESENT (default 3-phase behavior)
- **When** EVSE transitions to STATE_C
- **Then** Both contactor 1 and contactor 2 are on and Nr_Of_Phases_Charging is 3

### Phase switch from 3P to 1P completes on STATE_C entry
**Requirement:** `REQ-MODE-021`

- **Given** EVSE has Switching_Phases_C2=GOING_TO_SWITCH_1P and EnableC2=AUTO
- **When** EVSE transitions to STATE_C
- **Then** Nr_Of_Phases_Charging is set to 1 and Switching_Phases_C2 is cleared to NO_SWITCH

</details>

---

## Phase Switching

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-PHASE-003` | AUTO + SOLAR: no switch needed when already at correct phase count | `test_check_auto_solar_forces_1p` | `test_phase_switching.c:1` |
| `REQ-PHASE-004` | AUTO + SOLAR already on 1 phase results in NO_SWITCH | `test_check_auto_solar_already_1p` | `test_phase_switching.c:52` |
| `REQ-PHASE-005` | AUTO + SMART forces 3-phase when currently on 1 phase | `test_check_auto_smart_forces_3p` | `test_phase_switching.c:73` |
| `REQ-PHASE-006` | AUTO + SMART already on 3 phases results in NO_SWITCH | `test_check_auto_smart_already_3p` | `test_phase_switching.c:94` |
| `REQ-PHASE-007` | ALWAYS_OFF in STATE_A sets Nr_Of_Phases_Charging directly to 1 | `test_check_always_off_in_state_a` | `test_phase_switching.c:115` |
| `REQ-PHASE-008` | ALWAYS_OFF in STATE_B sets deferred switching flag to 1P | `test_check_always_off_in_state_b` | `test_phase_switching.c:136` |
| `REQ-PHASE-009` | SOLAR_OFF + SMART forces 3-phase charging | `test_check_solar_off_smart_3p` | `test_phase_switching.c:156` |
| `REQ-PHASE-010` | SOLAR_OFF + SOLAR forces 1-phase charging | `test_check_solar_off_solar_1p` | `test_phase_switching.c:178` |
| `REQ-PHASE-011` | STATE_C entry applies deferred 1P switch and opens contactor 2 | `test_state_c_applies_1p_switch` | `test_phase_switching.c:199` |
| `REQ-PHASE-012` | STATE_C entry applies deferred 3P switch and closes contactor 2 | `test_state_c_applies_3p_switch` | `test_phase_switching.c:220` |
| `REQ-PHASE-013` | STATE_C entry resets Switching_Phases_C2 to NO_SWITCH | `test_state_c_resets_switching` | `test_phase_switching.c:241` |
| `REQ-PHASE-014` | Full 3P to 1P to 3P phase switching cycle in solar mode | `test_full_3p_1p_3p_cycle` | `test_phase_switching.c:259` |

<details>
<summary>Detailed steps (12 scenarios)</summary>

### AUTO + SOLAR: no switch needed when already at correct phase count
**Requirement:** `REQ-PHASE-003`

- **Given** The EVSE is in STATE_B with EnableC2=AUTO, MODE_SOLAR, and various phase counts
- **When** evse_check_switching_phases is called
- **Then** Switching_Phases_C2 is NO_SWITCH when already at the correct phase count

### AUTO + SOLAR already on 1 phase results in NO_SWITCH
**Requirement:** `REQ-PHASE-004`

- **Given** The EVSE is in STATE_B with EnableC2=AUTO, MODE_SOLAR, and 1 phase
- **When** evse_check_switching_phases is called
- **Then** Switching_Phases_C2 is NO_SWITCH (already single phase)

### AUTO + SMART forces 3-phase when currently on 1 phase
**Requirement:** `REQ-PHASE-005`

- **Given** The EVSE is in STATE_B with EnableC2=AUTO, MODE_SMART, and 1 phase
- **When** evse_check_switching_phases is called
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_3P

### AUTO + SMART already on 3 phases results in NO_SWITCH
**Requirement:** `REQ-PHASE-006`

- **Given** The EVSE is in STATE_B with EnableC2=AUTO, MODE_SMART, and 3 phases
- **When** evse_check_switching_phases is called
- **Then** Switching_Phases_C2 is NO_SWITCH (already three phase)

### ALWAYS_OFF in STATE_A sets Nr_Of_Phases_Charging directly to 1
**Requirement:** `REQ-PHASE-007`

- **Given** The EVSE is in STATE_A with EnableC2=ALWAYS_OFF and 3 phases configured
- **When** evse_check_switching_phases is called
- **Then** Nr_Of_Phases_Charging is set directly to 1 (no deferred switch needed)

### ALWAYS_OFF in STATE_B sets deferred switching flag to 1P
**Requirement:** `REQ-PHASE-008`

- **Given** The EVSE is in STATE_B with EnableC2=ALWAYS_OFF and 3 phases configured
- **When** evse_check_switching_phases is called
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_1P (deferred until STATE_C entry)

### SOLAR_OFF + SMART forces 3-phase charging
**Requirement:** `REQ-PHASE-009`

- **Given** The EVSE is in STATE_B with EnableC2=SOLAR_OFF, MODE_SMART, and 1 phase
- **When** evse_check_switching_phases is called
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_3P

### SOLAR_OFF + SOLAR forces 1-phase charging
**Requirement:** `REQ-PHASE-010`

- **Given** The EVSE is in STATE_B with EnableC2=SOLAR_OFF, MODE_SOLAR, and 3 phases
- **When** evse_check_switching_phases is called
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_1P

### STATE_C entry applies deferred 1P switch and opens contactor 2
**Requirement:** `REQ-PHASE-011`

- **Given** Switching_Phases_C2 is GOING_TO_SWITCH_1P with EnableC2=ALWAYS_OFF
- **When** The state is set to STATE_C
- **Then** Nr_Of_Phases_Charging is 1 and contactor2 is off (open)

### STATE_C entry applies deferred 3P switch and closes contactor 2
**Requirement:** `REQ-PHASE-012`

- **Given** Switching_Phases_C2 is GOING_TO_SWITCH_3P with EnableC2=ALWAYS_ON
- **When** The state is set to STATE_C
- **Then** Nr_Of_Phases_Charging is 3 and contactor2 is on (closed)

### STATE_C entry resets Switching_Phases_C2 to NO_SWITCH
**Requirement:** `REQ-PHASE-013`

- **Given** Switching_Phases_C2 is GOING_TO_SWITCH_1P
- **When** The state is set to STATE_C
- **Then** Switching_Phases_C2 is reset to NO_SWITCH

### Full 3P to 1P to 3P phase switching cycle in solar mode
**Requirement:** `REQ-PHASE-014`

- **Given** The EVSE is solar charging on 3 phases with EnableC2=AUTO
- **When** Solar shortage triggers 3P->1P switch, then surplus triggers 1P->3P switch
- **Then** The EVSE correctly switches from 3P to 1P and back to 3P with proper contactor and flag states

</details>

---

## Power Availability

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-PWR-001` | Normal mode always reports current as available regardless of mains load | `test_normal_mode_always_available` | `test_power_availability.c:1` |
| `REQ-PWR-002` | Normal mode available even with high mains load | `test_normal_mode_available_with_high_load` | `test_power_availability.c:45` |
| `REQ-PWR-003` | Smart mode allows current when mains load plus MinCurrent is under MaxMains | `test_smart_maxmains_allows_under_limit` | `test_power_availability.c:62` |
| `REQ-PWR-004` | Smart mode blocks current when mains load plus MinCurrent exceeds MaxMains | `test_smart_maxmains_blocks_over_limit` | `test_power_availability.c:80` |
| `REQ-PWR-005` | Smart mode allows current when circuit load is under MaxCircuit limit | `test_smart_maxcircuit_allows_under_limit` | `test_power_availability.c:101` |
| `REQ-PWR-006` | Smart mode blocks current when circuit load exceeds MaxCircuit limit | `test_smart_maxcircuit_blocks_over_limit` | `test_power_availability.c:121` |
| `REQ-PWR-007` | MaxSumMains allows current when sum of phase currents is under limit | `test_maxsummains_allows_under_limit` | `test_power_availability.c:143` |
| `REQ-PWR-008` | MaxSumMains blocks current when sum of phase currents exceeds limit | `test_maxsummains_blocks_over_limit` | `test_power_availability.c:162` |
| `REQ-PWR-009` | MaxSumMains=0 disables the sum-of-mains check entirely | `test_maxsummains_zero_disables_check` | `test_power_availability.c:182` |
| `REQ-PWR-010` | Solar mode blocks current when no surplus is available | `test_solar_no_surplus_blocks` | `test_power_availability.c:203` |
| `REQ-PWR-011` | Solar mode allows current when surplus exceeds StartCurrent | `test_solar_surplus_allows` | `test_power_availability.c:220` |
| `REQ-PWR-012` | Solar mode blocks current when surplus is below StartCurrent threshold | `test_solar_insufficient_surplus_blocks` | `test_power_availability.c:237` |
| `REQ-PWR-013` | Solar mode with active EVSE checks fair share before allowing more | `test_solar_with_active_evse_checks_fair_share` | `test_power_availability.c:254` |
| `REQ-PWR-014` | OCPP limit below MinCurrent blocks power availability | `test_ocpp_limit_blocks_when_below_min` | `test_power_availability.c:280` |
| `REQ-PWR-015` | OCPP limit above MinCurrent allows power availability | `test_ocpp_limit_allows_when_above_min` | `test_power_availability.c:297` |
| `REQ-PWR-016` | OCPP negative limit (no limit set) allows power availability | `test_ocpp_no_limit_allows` | `test_power_availability.c:314` |
| `REQ-PWR-017` | OCPP availability check is skipped for non-standalone configurations | `test_ocpp_check_only_for_standalone` | `test_power_availability.c:330` |
| `REQ-PWR-018` | PWM duty cycle conversion for 6A (minimum charge current) | `test_current_to_duty_6A` | `test_power_availability.c:350` |
| `REQ-PWR-019` | PWM duty cycle conversion for 16A (common residential limit) | `test_current_to_duty_16A` | `test_power_availability.c:364` |
| `REQ-PWR-020` | PWM duty cycle conversion for 51A (upper boundary of low-range formula) | `test_current_to_duty_51A` | `test_power_availability.c:378` |
| `REQ-PWR-021` | PWM duty cycle conversion for 60A (high-range formula) | `test_current_to_duty_high_range` | `test_power_availability.c:392` |
| `REQ-PWR-022` | PWM duty cycle conversion for 80A (near maximum charge current) | `test_current_to_duty_80A` | `test_power_availability.c:406` |

<details>
<summary>Detailed steps (22 scenarios)</summary>

### Normal mode always reports current as available regardless of mains load
**Requirement:** `REQ-PWR-001`

- **Given** EVSE is standalone in Normal mode with very high MainsMeterImeasured=999
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because Normal mode does not check mains

### Normal mode available even with high mains load
**Requirement:** `REQ-PWR-002`

- **Given** EVSE is standalone in Normal mode with MainsMeterImeasured=400
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because Normal mode ignores mains measurements

### Smart mode allows current when mains load plus MinCurrent is under MaxMains
**Requirement:** `REQ-PWR-003`

- **Given** EVSE is standalone in Smart mode with MaxMains=25A and MainsMeterImeasured=100 (10A)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because baseload (10A) + MinCurrent (6A) = 16A < MaxMains (25A)

### Smart mode blocks current when mains load plus MinCurrent exceeds MaxMains
**Requirement:** `REQ-PWR-004`

- **Given** EVSE is standalone in Smart mode with MaxMains=10A and MainsMeterImeasured=200 (20A baseload)
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because baseload (20A) + MinCurrent (6A) = 26A > MaxMains (10A)

### Smart mode allows current when circuit load is under MaxCircuit limit
**Requirement:** `REQ-PWR-005`

- **Given** EVSE is master (LoadBl=1) in Smart mode with MaxCircuit=20A and EVMeterImeasured=50
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because circuit load (5A) + MinCurrent (6A) is under MaxCircuit

### Smart mode blocks current when circuit load exceeds MaxCircuit limit
**Requirement:** `REQ-PWR-006`

- **Given** EVSE is master (LoadBl=1) in Smart mode with MaxCircuit=8A and EVMeterImeasured=100 (10A)
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because circuit load (10A) already exceeds MaxCircuit (8A)

### MaxSumMains allows current when sum of phase currents is under limit
**Requirement:** `REQ-PWR-007`

- **Given** EVSE is in Smart mode with MaxSumMains=50 and Isum=100 (10A total)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because Isum plus MinCurrent is under MaxSumMains limit

### MaxSumMains blocks current when sum of phase currents exceeds limit
**Requirement:** `REQ-PWR-008`

- **Given** EVSE is in Smart mode with MaxSumMains=10 and Isum=200 (way over limit)
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because total phase current sum exceeds MaxSumMains

### MaxSumMains=0 disables the sum-of-mains check entirely
**Requirement:** `REQ-PWR-009`

- **Given** EVSE is in Smart mode with MaxSumMains=0 (disabled) and Isum=9999 (extremely high)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because MaxSumMains=0 means the check is skipped

### Solar mode blocks current when no surplus is available
**Requirement:** `REQ-PWR-010`

- **Given** EVSE is in Solar mode with StartCurrent=6A and Isum=0 (no export)
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because there is no solar surplus for charging

### Solar mode allows current when surplus exceeds StartCurrent
**Requirement:** `REQ-PWR-011`

- **Given** EVSE is in Solar mode with StartCurrent=6A and Isum=-80 (8A export surplus)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because 8A surplus exceeds 6A StartCurrent threshold

### Solar mode blocks current when surplus is below StartCurrent threshold
**Requirement:** `REQ-PWR-012`

- **Given** EVSE is in Solar mode with StartCurrent=10A and Isum=-80 (only 8A surplus)
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because 8A surplus is below the 10A StartCurrent threshold

### Solar mode with active EVSE checks fair share before allowing more
**Requirement:** `REQ-PWR-013`

- **Given** EVSE is in Solar mode with one active EVSE at MinCurrent and Isum=10 (1A import)
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because grid import indicates insufficient surplus for another EVSE

### OCPP limit below MinCurrent blocks power availability
**Requirement:** `REQ-PWR-014`

- **Given** EVSE is standalone with OcppMode enabled and OcppCurrentLimit=4.0A (below MinCurrent=6A)
- **When** evse_is_current_available is called
- **Then** Returns 0 (unavailable) because OCPP limit is below the minimum viable charge current

### OCPP limit above MinCurrent allows power availability
**Requirement:** `REQ-PWR-015`

- **Given** EVSE is standalone with OcppMode enabled and OcppCurrentLimit=10.0A (above MinCurrent=6A)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because OCPP limit is above the minimum viable charge current

### OCPP negative limit (no limit set) allows power availability
**Requirement:** `REQ-PWR-016`

- **Given** EVSE is standalone with OcppMode enabled and OcppCurrentLimit=-1.0A (no limit)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because negative OCPP limit means no restriction

### OCPP availability check is skipped for non-standalone configurations
**Requirement:** `REQ-PWR-017`

- **Given** EVSE is master (LoadBl=1) with OcppMode enabled and OcppCurrentLimit=3.0A (below MinCurrent)
- **When** evse_is_current_available is called
- **Then** Returns 1 (available) because OCPP check requires LoadBl=0 (standalone)

### PWM duty cycle conversion for 6A (minimum charge current)
**Requirement:** `REQ-PWR-018`

- **Given** A charge current of 60 deciamps (6A)
- **When** evse_current_to_duty is called
- **Then** Returns 102 as the PWM duty cycle value (60/0.6 * 1024/1000)

### PWM duty cycle conversion for 16A (common residential limit)
**Requirement:** `REQ-PWR-019`

- **Given** A charge current of 160 deciamps (16A)
- **When** evse_current_to_duty is called
- **Then** Returns a duty cycle value between 100 and 600 (low-range formula)

### PWM duty cycle conversion for 51A (upper boundary of low-range formula)
**Requirement:** `REQ-PWR-020`

- **Given** A charge current of 510 deciamps (51A)
- **When** evse_current_to_duty is called
- **Then** Returns a duty cycle value between 800 and 1000 (near top of low-range)

### PWM duty cycle conversion for 60A (high-range formula)
**Requirement:** `REQ-PWR-021`

- **Given** A charge current of 600 deciamps (60A)
- **When** evse_current_to_duty is called
- **Then** Returns a duty cycle value between 850 and 1000 (high-range formula: current/2.5 + 640)

### PWM duty cycle conversion for 80A (near maximum charge current)
**Requirement:** `REQ-PWR-022`

- **Given** A charge current of 800 deciamps (80A)
- **When** evse_current_to_duty is called
- **Then** Returns a duty cycle value between 950 and 1024 (near the top of the PWM range)

</details>

---

## Priority-Based Power Scheduling

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-LB-100` | MODBUS_ADDR strategy produces ascending address order | `test_sort_modbus_addr` | `test_scheduling.c:1` |
| `REQ-LB-101` | FIRST_CONNECTED strategy orders by earliest connection time | `test_sort_first_connected` | `test_scheduling.c:70` |
| `REQ-LB-102` | LAST_CONNECTED strategy orders by most recent connection time | `test_sort_last_connected` | `test_scheduling.c:94` |
| `REQ-LB-103` | Disconnected EVSEs are sorted to end regardless of strategy | `test_sort_disconnected_to_end` | `test_scheduling.c:118` |
| `REQ-LB-110` | Insufficient power for 3 EVSEs: first 2 in priority get MinCurrent | `test_shortage_first_two_get_current` | `test_scheduling.c:146` |
| `REQ-LB-111` | Power for only 1 EVSE: highest priority gets it all | `test_shortage_one_evse_gets_all` | `test_scheduling.c:177` |
| `REQ-LB-112` | Sufficient power: all EVSEs get current, no scheduling needed | `test_sufficient_power_no_scheduling` | `test_scheduling.c:204` |
| `REQ-LB-113` | Surplus above MinCurrent distributed fairly among active EVSEs | `test_surplus_distributed_fairly` | `test_scheduling.c:232` |
| `REQ-LB-114` | Standalone mode (LoadBl=0) does not use priority scheduling | `test_standalone_no_scheduling` | `test_scheduling.c:263` |
| `REQ-LB-115` | Solar mode: paused EVSEs get NO_SUN error instead of LESS_6A | `test_solar_paused_gets_no_sun` | `test_scheduling.c:300` |
| `REQ-LB-116` | Capped EVSE surplus redistributed to uncapped ones | `test_capped_surplus_redistribution` | `test_scheduling.c:337` |
| `REQ-LB-117` | Power exactly equals MinCurrent for 1 EVSE | `test_exactly_one_mincurrent` | `test_scheduling.c:363` |
| `REQ-LB-118` | Zero available power pauses all EVSEs | `test_zero_power_pauses_all` | `test_scheduling.c:394` |
| `REQ-LB-119` | NoCurrent does NOT increment when priority scheduling pauses some EVSEs | `test_no_current_not_incremented_on_deliberate_pause` | `test_scheduling.c:422` |
| `REQ-LB-120` | EVSE drawing <1A when IdleTimer expires gets paused | `test_idle_evse_paused_at_timeout` | `test_scheduling.c:450` |
| `REQ-LB-121` | EVSE not paused before IdleTimeout expires (anti-flap) | `test_antiflap_not_paused_early` | `test_scheduling.c:479` |
| `REQ-LB-122` | EVSE drawing power when IdleTimer expires stays active | `test_charging_evse_stays_active` | `test_scheduling.c:505` |
| `REQ-LB-123` | Full idle cycle: all EVSEs tried, recircle to first | `test_idle_cycle_wraps_around` | `test_scheduling.c:534` |
| `REQ-LB-140` | RotationTimer expiry pauses current EVSE and activates next | `test_rotation_timer_expires` | `test_scheduling.c:567` |
| `REQ-LB-141` | RotationInterval=0 disables rotation entirely | `test_rotation_disabled` | `test_scheduling.c:598` |
| `REQ-LB-142` | Rotation wraps from last priority to first | `test_rotation_wraps_to_first` | `test_scheduling.c:628` |
| `REQ-LB-143` | Rotation skips disconnected EVSEs (STATE_A) | `test_rotation_skips_disconnected` | `test_scheduling.c:657` |
| `REQ-LB-144` | Newly activated EVSE gets idle check before rotation timer applies | `test_idle_check_before_rotation` | `test_scheduling.c:687` |
| `REQ-LB-150` | Power increases: paused EVSE reactivated immediately | `test_power_increase_reactivates` | `test_scheduling.c:723` |
| `REQ-LB-151` | Reactivation follows priority order | `test_reactivation_follows_priority` | `test_scheduling.c:758` |
| `REQ-LB-160` | Original bug: 2 EVSEs, power drops, only 1 stops (no oscillation) | `test_regression_no_oscillation` | `test_scheduling.c:790` |
| `REQ-LB-161` | 6 EVSEs, power for 5: lowest priority paused | `test_six_evse_lowest_paused` | `test_scheduling.c:820` |
| `REQ-LB-162` | Node goes offline: removed from scheduling | `test_offline_node_removed` | `test_scheduling.c:845` |
| `REQ-LB-163` | New EVSE join during shortage doesn't displace active ones | `test_new_evse_doesnt_displace` | `test_scheduling.c:872` |

<details>
<summary>Detailed steps (29 scenarios)</summary>

### MODBUS_ADDR strategy produces ascending address order
**Requirement:** `REQ-LB-100`

- **Given** Master (LoadBl=1) with 4 EVSEs in STATE_C
- **When** evse_sort_priority() is called
- **Then** Priority[] = {0, 1, 2, 3}

### FIRST_CONNECTED strategy orders by earliest connection time
**Requirement:** `REQ-LB-101`

- **Given** Master with 3 EVSEs in STATE_C
- **When** evse_sort_priority() is called
- **Then** Priority[] = {1, 2, 0, ...} (EVSE[1] first, EVSE[0] last)

### LAST_CONNECTED strategy orders by most recent connection time
**Requirement:** `REQ-LB-102`

- **Given** Master with 3 EVSEs in STATE_C
- **When** evse_sort_priority() is called
- **Then** Priority[] = {0, 2, 1, ...} (EVSE[0] first, EVSE[1] last)

### Disconnected EVSEs are sorted to end regardless of strategy
**Requirement:** `REQ-LB-103`

- **Given** Master with 4 EVSEs: [0]=STATE_C, [1]=STATE_A, [2]=STATE_C, [3]=STATE_A
- **When** evse_sort_priority() is called
- **Then** Priority[] = {0, 2, 1, 3} (active EVSEs first, then disconnected)

### Insufficient power for 3 EVSEs: first 2 in priority get MinCurrent
**Requirement:** `REQ-LB-110`

- **Given** Master with 3 EVSEs in STATE_C, MinCurrent=6A, MaxCircuit=20A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** Balanced[0] >= 60 and Balanced[1] >= 60 and Balanced[2] == 0

### Power for only 1 EVSE: highest priority gets it all
**Requirement:** `REQ-LB-111`

- **Given** Master with 3 EVSEs in STATE_C, MinCurrent=6A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** Balanced[0] == 80 and Balanced[1] == 0 and Balanced[2] == 0

### Sufficient power: all EVSEs get current, no scheduling needed
**Requirement:** `REQ-LB-112`

- **Given** Master with 3 EVSEs in STATE_C, MinCurrent=6A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** All EVSEs get current, no LESS_6A errors, NoCurrent == 0

### Surplus above MinCurrent distributed fairly among active EVSEs
**Requirement:** `REQ-LB-113`

- **Given** Master with 2 EVSEs in STATE_C, MinCurrent=6A, BalancedMax={320,320}
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** Balanced[0] == 100 and Balanced[1] == 100 (10A each = 6A + 4A surplus)

### Standalone mode (LoadBl=0) does not use priority scheduling
**Requirement:** `REQ-LB-114`

- **Given** Single EVSE (LoadBl=0) in STATE_C, MinCurrent=6A, Mode=SMART
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** NoCurrent increments (original behavior preserved), no ScheduleState changes

### Solar mode: paused EVSEs get NO_SUN error instead of LESS_6A
**Requirement:** `REQ-LB-115`

- **Given** Master with 2 EVSEs in STATE_C, Mode=MODE_SOLAR
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** Balanced[1] == 0 and BalancedError[1] has NO_SUN set

### Capped EVSE surplus redistributed to uncapped ones
**Requirement:** `REQ-LB-116`

- **Given** Master with 3 EVSEs in STATE_C, MinCurrent=6A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** Balanced[1] == 80 (capped) and Balanced[0] + Balanced[2] == 160

### Power exactly equals MinCurrent for 1 EVSE
**Requirement:** `REQ-LB-117`

- **Given** Master with 3 EVSEs in STATE_C, MinCurrent=6A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** Exactly 1 EVSE has Balanced >= 60, exactly 2 have Balanced == 0

### Zero available power pauses all EVSEs
**Requirement:** `REQ-LB-118`

- **Given** Master with 3 EVSEs in STATE_C, MinCurrent=6A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** All Balanced[] == 0, all paused, NoCurrent increments

### NoCurrent does NOT increment when priority scheduling pauses some EVSEs
**Requirement:** `REQ-LB-119`

- **Given** Master with 3 EVSEs in STATE_C, MinCurrent=6A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** NoCurrent == 0, EVSE[0] is charging

### EVSE drawing <1A when IdleTimer expires gets paused
**Requirement:** `REQ-LB-120`

- **Given** Master with 2 EVSEs in STATE_C, EVSE[0] active, EVSE[1] paused
- **When** evse_schedule_tick_1s() is called
- **Then** EVSE[0] paused, EVSE[1] activated with IdleTimer[1] = 0

### EVSE not paused before IdleTimeout expires (anti-flap)
**Requirement:** `REQ-LB-121`

- **Given** EVSE[0] active with IdleTimer[0] = 30, IdleTimeout = 60
- **When** evse_schedule_tick_1s() is called
- **Then** EVSE[0] remains active

### EVSE drawing power when IdleTimer expires stays active
**Requirement:** `REQ-LB-122`

- **Given** EVSE[0] active with IdleTimer[0] = 59, IdleTimeout = 60
- **When** evse_schedule_tick_1s() is called
- **Then** EVSE[0] stays active, RotationTimer starts if RotationInterval > 0

### Full idle cycle: all EVSEs tried, recircle to first
**Requirement:** `REQ-LB-123`

- **Given** 3 EVSEs in STATE_C, EVSE[2] active (last tried), EVSE[0] and [1] paused
- **When** evse_schedule_tick_1s() is called
- **Then** EVSE[2] paused, EVSE[0] reactivated (wraps around)

### RotationTimer expiry pauses current EVSE and activates next
**Requirement:** `REQ-LB-140`

- **Given** 3 EVSEs in STATE_C, EVSE[0] active, RotationInterval=30
- **When** evse_schedule_tick_1s() is called
- **Then** EVSE[0] paused, EVSE[1] activated, RotationTimer reset to 1800

### RotationInterval=0 disables rotation entirely
**Requirement:** `REQ-LB-141`

- **Given** 2 EVSEs, EVSE[0] active, RotationInterval = 0
- **When** Checking ScheduleState
- **Then** EVSE[0] still active (never rotated)

### Rotation wraps from last priority to first
**Requirement:** `REQ-LB-142`

- **Given** 3 EVSEs in priority order {0,1,2}, EVSE[2] active
- **When** evse_schedule_tick_1s() is called
- **Then** EVSE[2] paused, EVSE[0] activated

### Rotation skips disconnected EVSEs (STATE_A)
**Requirement:** `REQ-LB-143`

- **Given** 3 EVSEs: [0] active, [1] STATE_A disconnected, [2] paused
- **When** evse_schedule_tick_1s() is called
- **Then** EVSE[0] paused, EVSE[2] activated (EVSE[1] skipped)

### Newly activated EVSE gets idle check before rotation timer applies
**Requirement:** `REQ-LB-144`

- **Given** EVSE[1] just activated via rotation, IdleTimeout=60, RotationInterval=30
- **When** 60 seconds pass
- **Then** EVSE[1] paused due to idle (not waiting for rotation)

### Power increases: paused EVSE reactivated immediately
**Requirement:** `REQ-LB-150`

- **Given** 3 EVSEs, EVSE[0] active, EVSE[1] and [2] paused
- **When** evse_calc_balanced_current(ctx, 0) is called with new power
- **Then** EVSE[1] reactivated, IdleTimer reset

### Reactivation follows priority order
**Requirement:** `REQ-LB-151`

- **Given** 3 EVSEs paused, PrioStrategy=PRIO_MODBUS_ADDR
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** EVSE[0] and EVSE[1] activated (not [0] and [2])

### Original bug: 2 EVSEs, power drops, only 1 stops (no oscillation)
**Requirement:** `REQ-LB-160`

- **Given** Master with 2 EVSEs in STATE_C, MinCurrent=6A, MaxCircuit=11A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** Exactly 1 EVSE continues, 1 paused, NoCurrent == 0

### 6 EVSEs, power for 5: lowest priority paused
**Requirement:** `REQ-LB-161`

- **Given** Master with 6 EVSEs in STATE_C, MinCurrent=6A
- **When** evse_calc_balanced_current(ctx, 0) is called
- **Then** EVSEs [0]-[4] receive current, EVSE[5] paused

### Node goes offline: removed from scheduling
**Requirement:** `REQ-LB-162`

- **Given** 3 EVSEs, EVSE[1] goes offline (STATE_A)
- **When** evse_schedule_tick_1s() runs
- **Then** EVSE[1] gets ScheduleState = SCHED_INACTIVE

### New EVSE join during shortage doesn't displace active ones
**Requirement:** `REQ-LB-163`

- **Given** 2 EVSEs charging, power for 2 at MinCurrent
- **When** evse_calc_balanced_current(ctx, 1) is called
- **Then** EVSE[0] and EVSE[1] keep allocation, EVSE[2] gets 0

</details>

---

## Serial Message Parsing

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-SERIAL-001` | Valid three-phase Irms message with address 011 | `test_irms_valid_three_phase` | `test_serial_parser.c:1` |
| `REQ-SERIAL-001` | Irms message with negative current values | `test_irms_negative_values` | `test_serial_parser.c:40` |
| `REQ-SERIAL-001` | Irms message with zero values | `test_irms_zero_values` | `test_serial_parser.c:57` |
| `REQ-SERIAL-001` | Irms message embedded in larger buffer with extra text | `test_irms_embedded_in_buffer` | `test_serial_parser.c:74` |
| `REQ-SERIAL-002` | Valid power measurement with address 010 | `test_power_valid` | `test_serial_parser.c:147` |
| `REQ-SERIAL-002` | Power measurement with negative value (export) | `test_power_negative` | `test_serial_parser.c:162` |
| `REQ-SERIAL-003` | Valid 16-byte node status with state B and no errors | `test_node_status_valid` | `test_serial_parser.c:220` |
| `REQ-SERIAL-003` | Node status with error flags and solar timer | `test_node_status_error_and_timer` | `test_serial_parser.c:248` |
| `REQ-SERIAL-003` | Node status with mode Smart and max current boundary | `test_node_status_max_current_boundary` | `test_serial_parser.c:276` |

<details>
<summary>Detailed steps (9 scenarios)</summary>

### Valid three-phase Irms message with address 011
**Requirement:** `REQ-SERIAL-001`

- **Given** A serial buffer containing "Irms:011,312,123,124"
- **When** serial_parse_irms is called
- **Then** Address is 11 and three phase currents are parsed correctly

### Irms message with negative current values
**Requirement:** `REQ-SERIAL-001`

- **Given** A serial buffer with negative Irms values (solar injection)
- **When** serial_parse_irms is called
- **Then** Negative values are parsed correctly

### Irms message with zero values
**Requirement:** `REQ-SERIAL-001`

- **Given** A serial buffer with all zero Irms values
- **When** serial_parse_irms is called
- **Then** All values parsed as zero

### Irms message embedded in larger buffer with extra text
**Requirement:** `REQ-SERIAL-001`

- **Given** A serial buffer with text before and after the Irms token
- **When** serial_parse_irms is called
- **Then** The Irms message is found and parsed correctly

### Valid power measurement with address 010
**Requirement:** `REQ-SERIAL-002`

- **Given** A serial buffer containing "PowerMeasured:010,500"
- **When** serial_parse_power is called
- **Then** Address is 10 and power is 500

### Power measurement with negative value (export)
**Requirement:** `REQ-SERIAL-002`

- **Given** A serial buffer with negative power value
- **When** serial_parse_power is called
- **Then** Negative power is parsed correctly

### Valid 16-byte node status with state B and no errors
**Requirement:** `REQ-SERIAL-003`

- **Given** A 16-byte buffer with state=1 (B), error=0, mode=0 (Normal)
- **When** serial_parse_node_status is called
- **Then** All fields parsed correctly

### Node status with error flags and solar timer
**Requirement:** `REQ-SERIAL-003`

- **Given** A buffer with RCM_TRIPPED error and large solar timer
- **When** serial_parse_node_status is called
- **Then** Error and solar timer are parsed correctly

### Node status with mode Smart and max current boundary
**Requirement:** `REQ-SERIAL-003`

- **Given** A buffer with mode=1 (Smart) and max current 255 (max byte value)
- **When** serial_parse_node_status is called
- **Then** Max current is 255 * 10 = 2550

</details>

---

## Serial Input Validation

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-SERIAL-004` | Irms message with missing fields returns false | `test_irms_missing_fields` | `test_serial_parser.c:91` |
| `REQ-SERIAL-004` | Irms token not found in buffer | `test_irms_token_not_found` | `test_serial_parser.c:104` |
| `REQ-SERIAL-004` | NULL buffer passed to Irms parser | `test_irms_null_buffer` | `test_serial_parser.c:117` |
| `REQ-SERIAL-004` | Empty buffer passed to Irms parser | `test_irms_empty_buffer` | `test_serial_parser.c:130` |
| `REQ-SERIAL-004` | Power message with missing field returns false | `test_power_missing_field` | `test_serial_parser.c:177` |
| `REQ-SERIAL-004` | Power token not found in buffer | `test_power_token_not_found` | `test_serial_parser.c:190` |
| `REQ-SERIAL-004` | NULL buffer passed to power parser | `test_power_null_buffer` | `test_serial_parser.c:203` |
| `REQ-SERIAL-004` | Node status buffer too short | `test_node_status_buffer_too_short` | `test_serial_parser.c:297` |
| `REQ-SERIAL-004` | NULL buffer passed to node status parser | `test_node_status_null_buffer` | `test_serial_parser.c:311` |
| `REQ-SERIAL-004` | NULL output passed to node status parser | `test_node_status_null_output` | `test_serial_parser.c:324` |

<details>
<summary>Detailed steps (10 scenarios)</summary>

### Irms message with missing fields returns false
**Requirement:** `REQ-SERIAL-004`

- **Given** A serial buffer with only 2 of 4 expected Irms fields
- **When** serial_parse_irms is called
- **Then** Returns false

### Irms token not found in buffer
**Requirement:** `REQ-SERIAL-004`

- **Given** A serial buffer without the Irms token
- **When** serial_parse_irms is called
- **Then** Returns false

### NULL buffer passed to Irms parser
**Requirement:** `REQ-SERIAL-004`

- **Given** A NULL buffer pointer
- **When** serial_parse_irms is called
- **Then** Returns false without crashing

### Empty buffer passed to Irms parser
**Requirement:** `REQ-SERIAL-004`

- **Given** An empty string buffer
- **When** serial_parse_irms is called
- **Then** Returns false

### Power message with missing field returns false
**Requirement:** `REQ-SERIAL-004`

- **Given** A serial buffer with only the address, no power value
- **When** serial_parse_power is called
- **Then** Returns false

### Power token not found in buffer
**Requirement:** `REQ-SERIAL-004`

- **Given** A serial buffer without the PowerMeasured token
- **When** serial_parse_power is called
- **Then** Returns false

### NULL buffer passed to power parser
**Requirement:** `REQ-SERIAL-004`

- **Given** A NULL buffer pointer
- **When** serial_parse_power is called
- **Then** Returns false without crashing

### Node status buffer too short
**Requirement:** `REQ-SERIAL-004`

- **Given** A buffer shorter than 16 bytes
- **When** serial_parse_node_status is called
- **Then** Returns false

### NULL buffer passed to node status parser
**Requirement:** `REQ-SERIAL-004`

- **Given** A NULL buffer pointer
- **When** serial_parse_node_status is called
- **Then** Returns false without crashing

### NULL output passed to node status parser
**Requirement:** `REQ-SERIAL-004`

- **Given** A valid buffer but NULL output pointer
- **When** serial_parse_node_status is called
- **Then** Returns false without crashing

</details>

---

## Battery Current Calculation

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-CALC-001` | Fresh battery data in solar mode | `test_battery_current_fresh_solar_api` | `test_serial_parser.c:341` |
| `REQ-CALC-001` | Stale battery data is ignored after 60 seconds | `test_battery_current_stale_data` | `test_serial_parser.c:354` |
| `REQ-CALC-001` | Battery data exactly at 60 second boundary | `test_battery_current_boundary_60s` | `test_serial_parser.c:367` |
| `REQ-CALC-001` | Non-solar mode returns zero | `test_battery_current_normal_mode` | `test_serial_parser.c:380` |
| `REQ-CALC-001` | Non-API meter in solar mode still returns battery current | `test_battery_current_non_api_meter` | `test_serial_parser.c:393` |
| `REQ-CALC-001` | Never-updated battery returns zero | `test_battery_current_never_updated` | `test_serial_parser.c:406` |
| `REQ-CALC-001` | Negative battery current (discharging) in solar mode | `test_battery_current_negative_discharge` | `test_serial_parser.c:419` |

<details>
<summary>Detailed steps (7 scenarios)</summary>

### Fresh battery data in solar mode
**Requirement:** `REQ-CALC-001`

- **Given** Battery update 30s ago, solar mode, current = 1000
- **When** calc_battery_current is called
- **Then** Returns 1000 (battery current value)

### Stale battery data is ignored after 60 seconds
**Requirement:** `REQ-CALC-001`

- **Given** Battery update 61s ago
- **When** calc_battery_current is called
- **Then** Returns 0 (stale data ignored)

### Battery data exactly at 60 second boundary
**Requirement:** `REQ-CALC-001`

- **Given** Battery update exactly 60s ago
- **When** calc_battery_current is called
- **Then** Returns battery current (60s is not stale)

### Non-solar mode returns zero
**Requirement:** `REQ-CALC-001`

- **Given** Normal mode with fresh battery data
- **When** calc_battery_current is called
- **Then** Returns 0 (battery only used in solar mode)

### Non-API meter in solar mode still returns battery current
**Requirement:** `REQ-CALC-001`

- **Given** Solar mode with non-API meter type
- **When** calc_battery_current is called
- **Then** Returns battery current (battery used with any meter in solar mode)

### Never-updated battery returns zero
**Requirement:** `REQ-CALC-001`

- **Given** time_since_update is 0 (never updated)
- **When** calc_battery_current is called
- **Then** Returns 0

### Negative battery current (discharging) in solar mode
**Requirement:** `REQ-CALC-001`

- **Given** Battery discharging with negative current value
- **When** calc_battery_current is called
- **Then** Returns the negative value

</details>

---

## Current Sum Calculation

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-CALC-002` | Three-phase system distributes battery current equally | `test_isum_three_phase_battery` | `test_serial_parser.c:436` |
| `REQ-CALC-003` | Single-phase battery adjustment uses L1 only | `test_isum_single_phase_battery` | `test_serial_parser.c:457` |
| `REQ-CALC-002` | Zero battery current leaves mains unchanged | `test_isum_zero_battery` | `test_serial_parser.c:478` |
| `REQ-CALC-002` | Negative mains currents (solar injection) with battery | `test_isum_negative_mains` | `test_serial_parser.c:499` |
| `REQ-CALC-002` | Battery current not evenly divisible by 3 | `test_isum_battery_rounding` | `test_serial_parser.c:520` |

<details>
<summary>Detailed steps (5 scenarios)</summary>

### Three-phase system distributes battery current equally
**Requirement:** `REQ-CALC-002`

- **Given** Three-phase mains at 100,200,300 dA and battery current 300 dA
- **When** calc_isum is called with enable_c2 = NOT_PRESENT
- **Then** Battery current divided by 3 (100) subtracted from each phase, Isum = 300

### Single-phase battery adjustment uses L1 only
**Requirement:** `REQ-CALC-003`

- **Given** EnableC2 ALWAYS_OFF, battery current 300 dA
- **When** calc_isum is called
- **Then** Full battery current subtracted from L1, L2 and L3 unchanged

### Zero battery current leaves mains unchanged
**Requirement:** `REQ-CALC-002`

- **Given** Zero battery current
- **When** calc_isum is called
- **Then** Adjusted currents equal original mains currents

### Negative mains currents (solar injection) with battery
**Requirement:** `REQ-CALC-002`

- **Given** Negative mains currents and positive battery current
- **When** calc_isum is called
- **Then** Battery adjustment makes values more negative

### Battery current not evenly divisible by 3
**Requirement:** `REQ-CALC-002`

- **Given** Battery current of 100 (100/3 = 33 per phase, integer division)
- **When** calc_isum is called
- **Then** Each phase reduced by 33 (truncated integer division)

</details>

---

## Solar Balancing

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-SOLAR-001` | 3-phase solar shortage starts SolarStopTimer | `test_solar_3p_shortage_starts_timer` | `test_solar_balancing.c:1` |
| `REQ-SOLAR-002` | SolarStopTimer reaching 2 or below triggers 3P to 1P phase switch | `test_solar_3p_timer_triggers_1p_switch` | `test_solar_balancing.c:66` |
| `REQ-SOLAR-003` | 1-phase solar surplus near MaxCurrent starts timer for 3P upgrade | `test_solar_1p_surplus_starts_timer` | `test_solar_balancing.c:88` |
| `REQ-SOLAR-004` | SolarStopTimer reaching 3 or below on 1P triggers switch to 3P | `test_solar_1p_timer_triggers_3p_switch` | `test_solar_balancing.c:113` |
| `REQ-SOLAR-005` | Insufficient surplus resets SolarStopTimer to prevent false 3P upgrade | `test_solar_insufficient_surplus_resets_timer` | `test_solar_balancing.c:136` |
| `REQ-SOLAR-006` | During solar startup period, EVSE is forced to MinCurrent | `test_solar_startup_forces_mincurrent` | `test_solar_balancing.c:159` |
| `REQ-SOLAR-007` | Past startup period, EVSE uses calculated distribution value | `test_solar_past_startup_uses_calculated` | `test_solar_balancing.c:177` |
| `REQ-SOLAR-008` | Small solar export results in gradual current increase | `test_solar_fine_increase_small` | `test_solar_balancing.c:197` |
| `REQ-SOLAR-009` | Large solar export results in larger current increase | `test_solar_fine_increase_large` | `test_solar_balancing.c:218` |
| `REQ-SOLAR-010` | Moderate grid import decreases solar charging current | `test_solar_fine_decrease_moderate` | `test_solar_balancing.c:239` |
| `REQ-SOLAR-011` | Large grid import aggressively decreases solar charging current | `test_solar_fine_decrease_aggressive` | `test_solar_balancing.c:259` |
| `REQ-SOLAR-012` | Solar B-state with AUTO and small surplus determines 1-phase charging | `test_solar_b_state_auto_determines_1p` | `test_solar_balancing.c:279` |
| `REQ-SOLAR-013` | Solar B-state with AUTO and large surplus determines 3-phase charging | `test_solar_b_state_auto_determines_3p` | `test_solar_balancing.c:300` |
| `REQ-SOLAR-014` | Hard current shortage increments NoCurrent counter | `test_hard_shortage_increments_nocurrent` | `test_solar_balancing.c:321` |
| `REQ-SOLAR-015` | Soft shortage (Isum exceeds MaxSumMains) starts MaxSumMains timer | `test_soft_shortage_starts_maxsummains_timer` | `test_solar_balancing.c:342` |
| `REQ-SOLAR-016` | No shortage condition clears SolarStopTimer and NoCurrent | `test_no_shortage_clears_timers` | `test_solar_balancing.c:367` |
| `REQ-SOLAR-017` | IsetBalanced is capped at 800 (80A maximum) | `test_isetbalanced_capped_at_800` | `test_solar_balancing.c:390` |
| `REQ-SOLAR-018` | Normal mode forces 3-phase charging regardless of current phase count | `test_normal_mode_forces_3p` | `test_solar_balancing.c:410` |
| `REQ-SOLAR-019` | phasesLastUpdateFlag=false prevents IsetBalanced regulation | `test_phases_flag_gates_regulation` | `test_solar_balancing.c:437` |
| `REQ-SOLAR-020` | Multi-EVSE solar startup: EVSE in startup gets MinCurrent, others get calculated | `test_multi_evse_solar_startup` | `test_solar_balancing.c:460` |

<details>
<summary>Detailed steps (20 scenarios)</summary>

### 3-phase solar shortage starts SolarStopTimer
**Requirement:** `REQ-SOLAR-001`

- **Given** The EVSE is solar charging on 3 phases with EnableC2=AUTO and high mains load
- **When** evse_calc_balanced_current is called with large import (Isum=200)
- **Then** SolarStopTimer is set to a value greater than 0

### SolarStopTimer reaching 2 or below triggers 3P to 1P phase switch
**Requirement:** `REQ-SOLAR-002`

- **Given** The EVSE is solar charging on 3 phases with EnableC2=AUTO and SolarStopTimer=2
- **When** evse_calc_balanced_current is called with ongoing shortage
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_1P

### 1-phase solar surplus near MaxCurrent starts timer for 3P upgrade
**Requirement:** `REQ-SOLAR-003`

- **Given** The EVSE is solar charging on 1 phase with IsetBalanced near MaxCurrent and good surplus
- **When** evse_calc_balanced_current is called with export (Isum=-100)
- **Then** SolarStopTimer is set to 63 (countdown to 3P switch)

### SolarStopTimer reaching 3 or below on 1P triggers switch to 3P
**Requirement:** `REQ-SOLAR-004`

- **Given** The EVSE is solar charging on 1 phase with SolarStopTimer=3 and large surplus
- **When** evse_calc_balanced_current is called
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_3P

### Insufficient surplus resets SolarStopTimer to prevent false 3P upgrade
**Requirement:** `REQ-SOLAR-005`

- **Given** The EVSE is solar charging on 1 phase with IsetBalanced well below MaxCurrent
- **When** evse_calc_balanced_current is called with minimal surplus (Isum=-10)
- **Then** SolarStopTimer is reset to 0

### During solar startup period, EVSE is forced to MinCurrent
**Requirement:** `REQ-SOLAR-006`

- **Given** The EVSE is solar charging with IntTimer below SOLARSTARTTIME (in startup)
- **When** evse_calc_balanced_current is called
- **Then** Balanced[0] is set to MinCurrent*10 regardless of IsetBalanced

### Past startup period, EVSE uses calculated distribution value
**Requirement:** `REQ-SOLAR-007`

- **Given** The EVSE is solar charging with IntTimer past SOLARSTARTTIME
- **When** evse_calc_balanced_current is called
- **Then** Balanced[0] uses the calculated value (at least MinCurrent*10)

### Small solar export results in gradual current increase
**Requirement:** `REQ-SOLAR-008`

- **Given** The EVSE is solar charging with small export (Isum=-5)
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced increases by at least 1 (fine-grained increase)

### Large solar export results in larger current increase
**Requirement:** `REQ-SOLAR-009`

- **Given** The EVSE is solar charging with large export (Isum=-50)
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced increases by more than the small export case

### Moderate grid import decreases solar charging current
**Requirement:** `REQ-SOLAR-010`

- **Given** The EVSE is solar charging with IsetBalanced=150 and moderate import (Isum=15)
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced decreases below 150

### Large grid import aggressively decreases solar charging current
**Requirement:** `REQ-SOLAR-011`

- **Given** The EVSE is solar charging with IsetBalanced=200 and large import (Isum=50)
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced decreases below 200

### Solar B-state with AUTO and small surplus determines 1-phase charging
**Requirement:** `REQ-SOLAR-012`

- **Given** The EVSE is in STATE_B with EnableC2=AUTO, 3 phases, and small surplus (Isum=-50)
- **When** evse_calc_balanced_current is called
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_1P

### Solar B-state with AUTO and large surplus determines 3-phase charging
**Requirement:** `REQ-SOLAR-013`

- **Given** The EVSE is in STATE_B with EnableC2=AUTO, 1 phase, and large surplus (Isum=-500)
- **When** evse_calc_balanced_current is called
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_3P

### Hard current shortage increments NoCurrent counter
**Requirement:** `REQ-SOLAR-014`

- **Given** The EVSE is in MODE_SMART with heavily overloaded mains and low MaxMains
- **When** evse_calc_balanced_current is called
- **Then** NoCurrent counter is incremented above 0

### Soft shortage (Isum exceeds MaxSumMains) starts MaxSumMains timer
**Requirement:** `REQ-SOLAR-015`

- **Given** The EVSE is in MODE_SMART with Isum exceeding MaxSumMains and MaxSumMainsTime=5
- **When** evse_calc_balanced_current is called
- **Then** MaxSumMainsTimer is set to MaxSumMainsTime*60 (300 seconds)

### No shortage condition clears SolarStopTimer and NoCurrent
**Requirement:** `REQ-SOLAR-016`

- **Given** The EVSE is in MODE_SMART with low mains load and high MaxMains
- **When** evse_calc_balanced_current is called with no shortage detected
- **Then** SolarStopTimer and NoCurrent are both reset to 0

### IsetBalanced is capped at 800 (80A maximum)
**Requirement:** `REQ-SOLAR-017`

- **Given** The EVSE is in MODE_SMART with IsetBalanced=900 and large surplus
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced does not exceed 800

### Normal mode forces 3-phase charging regardless of current phase count
**Requirement:** `REQ-SOLAR-018`

- **Given** A standalone EVSE in MODE_NORMAL currently on 1 phase
- **When** evse_calc_balanced_current is called
- **Then** Switching_Phases_C2 is set to GOING_TO_SWITCH_3P

### phasesLastUpdateFlag=false prevents IsetBalanced regulation
**Requirement:** `REQ-SOLAR-019`

- **Given** The EVSE is in MODE_SMART with phasesLastUpdateFlag=false and large surplus
- **When** evse_calc_balanced_current is called
- **Then** IsetBalanced remains unchanged (regulation gated)

### Multi-EVSE solar startup: EVSE in startup gets MinCurrent, others get calculated
**Requirement:** `REQ-SOLAR-020`

- **Given** Two EVSEs as master, EVSE 0 in startup (IntTimer < SOLARSTARTTIME), EVSE 1 past startup
- **When** evse_calc_balanced_current is called
- **Then** EVSE 0 Balanced is set to MinCurrent*10 (startup forcing)

</details>

---

## IEC 61851-1 State Transitions

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-IEC61851-001` | EVSE initialises to disconnected state | `test_init_state_is_A` | `test_state_transitions.c:1` |
| `REQ-IEC61851-002` | Contactors are open after initialisation | `test_init_contactors_off` | `test_state_transitions.c:45` |
| `REQ-IEC61851-003` | Pilot signal is connected after initialisation | `test_init_pilot_connected` | `test_state_transitions.c:59` |
| `REQ-IEC61851-004` | No error flags after initialisation | `test_init_no_errors` | `test_state_transitions.c:72` |
| `REQ-IEC61851-005` | STATE_A remains when no vehicle is connected | `test_A_stays_A_on_12V` | `test_state_transitions.c:85` |
| `REQ-IEC61851-006` | Vehicle connection triggers STATE_A to STATE_B transition | `test_A_to_B_on_9V_when_ready` | `test_state_transitions.c:99` |
| `REQ-IEC61851-007` | Modem negotiation required before STATE_B when ModemStage is 0 | `test_A_to_modem_when_modem_stage_0` | `test_state_transitions.c:113` |
| `REQ-IEC61851-007B` | A→B goes directly to STATE_B when modem is disabled | `test_A_to_B_skips_modem_when_disabled` | `test_state_transitions.c:132` |
| `REQ-IEC61851-008` | Unauthorized EVSE blocks STATE_A to STATE_B transition | `test_A_stays_A_when_access_off` | `test_state_transitions.c:151` |
| `REQ-IEC61851-011` | Vehicle disconnect in STATE_B returns to STATE_A | `test_B_to_A_on_disconnect` | `test_state_transitions.c:166` |
| `REQ-IEC61851-012` | Vehicle charge request after diode check triggers STATE_B to STATE_C | `test_B_to_C_on_6V_with_diode_check` | `test_state_transitions.c:213` |
| `REQ-IEC61851-013` | Charge request without diode check does not transition to STATE_C | `test_B_to_C_requires_diode_check` | `test_state_transitions.c:234` |
| `REQ-IEC61851-014` | PILOT_DIODE signal sets DiodeCheck flag | `test_diode_check_sets_on_pilot_diode` | `test_state_transitions.c:255` |
| `REQ-IEC61851-015` | Contactor 1 is closed when entering STATE_C | `test_C_contactor1_on` | `test_state_transitions.c:271` |
| `REQ-IEC61851-016` | Vehicle disconnect during charging returns to STATE_A with contactors open | `test_C_to_A_on_disconnect` | `test_state_transitions.c:285` |
| `REQ-IEC61851-019` | Vehicle disconnect from STATE_C1 returns to STATE_A | `test_C1_to_A_on_disconnect` | `test_state_transitions.c:301` |
| `REQ-IEC61851-022` | Vehicle disconnect from STATE_B1 returns to STATE_A | `test_B1_to_A_on_disconnect` | `test_state_transitions.c:352` |
| `REQ-IEC61851-023` | Entering STATE_B1 sets a non-zero ChargeDelay | `test_set_state_B1_sets_charge_delay` | `test_state_transitions.c:410` |
| `REQ-IEC61851-024` | Entering STATE_A clears LESS_6A error and ChargeDelay | `test_set_state_A_clears_errors_and_delay` | `test_state_transitions.c:425` |
| `REQ-IEC61851-025` | State transitions are recorded in the transition log | `test_transition_log_records_states` | `test_state_transitions.c:442` |
| `REQ-IEC61851-026` | Entering STATE_C1 sets PWM to off (+12V) | `test_set_state_C1_sets_pwm_off` | `test_state_transitions.c:459` |
| `REQ-IEC61851-027` | Full charge cycle: A -> B -> C -> B -> A | `test_full_charge_cycle` | `test_state_transitions.c:474` |
| `REQ-IEC61851-028` | Vehicle disconnect during ACTSTART is NOT handled in tick_10ms | `test_actstart_to_A_on_disconnect` | `test_state_transitions.c:511` |
| `REQ-IEC61851-029` | ActivationMode=0 triggers STATE_ACTSTART on pilot detection in STATE_B | `test_activation_mode_triggers_actstart` | `test_state_transitions.c:528` |
| `REQ-IEC61851-031` | ActivationMode=255 (always active) does not count down | `test_activation_mode_255_does_not_countdown` | `test_state_transitions.c:545` |
| `REQ-IEC61851-032` | STATE_ACTSTART returns to STATE_B when ActivationTimer expires | `test_actstart_returns_to_B_when_timer_expires` | `test_state_transitions.c:576` |
| `REQ-PHASE-002` | STATE_B re-entry sets Switching_Phases_C2 flag instead of direct phase change | `test_state_B_calls_check_switching_phases_from_B` | `test_state_transitions.c:593` |
| `REQ-IEC61851-M3` | STATE_B entry does NOT set pilot_connected when modem disabled | `test_state_b_no_pilot_reconnect_without_modem` | `test_state_transitions.c:688` |
| `REQ-IEC61851-M3B` | STATE_B entry DOES set pilot_connected when modem enabled | `test_state_b_pilot_reconnect_with_modem` | `test_state_transitions.c:710` |

<details>
<summary>Detailed steps (29 scenarios)</summary>

### EVSE initialises to disconnected state
**Requirement:** `REQ-IEC61851-001`

- **Given** The EVSE is powered on
- **When** evse_init() is called
- **Then** The state machine starts in STATE_A (disconnected)

### Contactors are open after initialisation
**Requirement:** `REQ-IEC61851-002`

- **Given** The EVSE is powered on
- **When** evse_init() is called
- **Then** Both contactor1 and contactor2 are off (open)

### Pilot signal is connected after initialisation
**Requirement:** `REQ-IEC61851-003`

- **Given** The EVSE is powered on
- **When** evse_init() is called
- **Then** The pilot signal is connected (pilot_connected is true)

### No error flags after initialisation
**Requirement:** `REQ-IEC61851-004`

- **Given** The EVSE is powered on
- **When** evse_init() is called
- **Then** ErrorFlags is NO_ERROR

### STATE_A remains when no vehicle is connected
**Requirement:** `REQ-IEC61851-005`

- **Given** The EVSE is in STATE_A (disconnected) and ready to charge
- **When** A 12V pilot signal is received (no vehicle present)
- **Then** The state remains STATE_A

### Vehicle connection triggers STATE_A to STATE_B transition
**Requirement:** `REQ-IEC61851-006`

- **Given** The EVSE is in STATE_A, authorized, and ready to charge
- **When** A 9V pilot signal is received (vehicle connected)
- **Then** The state transitions to STATE_B (connected, not charging)

### Modem negotiation required before STATE_B when ModemStage is 0
**Requirement:** `REQ-IEC61851-007`

- **Given** The EVSE is in STATE_A with ModemStage=0 (unauthenticated modem)
- **When** A 9V pilot signal is received (vehicle connected)
- **Then** The state transitions to STATE_MODEM_REQUEST for ISO15118 negotiation

### A→B goes directly to STATE_B when modem is disabled
**Requirement:** `REQ-IEC61851-007B`

- **Given** The EVSE is in STATE_A with ModemStage=0 but ModemEnabled=false
- **When** A 9V pilot signal is received (vehicle connected)
- **Then** The state transitions directly to STATE_B (modem flow skipped)

### Unauthorized EVSE blocks STATE_A to STATE_B transition
**Requirement:** `REQ-IEC61851-008`

- **Given** The EVSE is in STATE_A with AccessStatus OFF (not authorized)
- **When** A 9V pilot signal is received (vehicle connected)
- **Then** The state remains STATE_A (transition blocked)

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

### Vehicle charge request after diode check triggers STATE_B to STATE_C
**Requirement:** `REQ-IEC61851-012`

- **Given** The EVSE is in STATE_B with DiodeCheck passed and sufficient current
- **When** A 6V pilot signal is sustained for 500ms (vehicle requests charge)
- **Then** The state transitions to STATE_C (charging)

### Charge request without diode check does not transition to STATE_C
**Requirement:** `REQ-IEC61851-013`

- **Given** The EVSE is in STATE_B with DiodeCheck NOT passed
- **When** A 6V pilot signal is sustained for 500ms
- **Then** The state does NOT transition to STATE_C

### PILOT_DIODE signal sets DiodeCheck flag
**Requirement:** `REQ-IEC61851-014`

- **Given** The EVSE is in STATE_B with DiodeCheck=0
- **When** A PILOT_DIODE signal is received
- **Then** DiodeCheck is set to 1

### Contactor 1 is closed when entering STATE_C
**Requirement:** `REQ-IEC61851-015`

- **Given** The EVSE transitions to STATE_C (charging)
- **When** evse_set_state is called with STATE_C
- **Then** contactor1_state is true (closed, power flowing)

### Vehicle disconnect during charging returns to STATE_A with contactors open
**Requirement:** `REQ-IEC61851-016`

- **Given** The EVSE is in STATE_C (charging)
- **When** A 12V pilot signal is received (vehicle disconnected)
- **Then** The state transitions to STATE_A and contactor1 is opened

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

### Entering STATE_B1 sets a non-zero ChargeDelay
**Requirement:** `REQ-IEC61851-023`

- **Given** The EVSE is ready to charge with ChargeDelay=0
- **When** The state is set to STATE_B1
- **Then** ChargeDelay is set to a value greater than 0

### Entering STATE_A clears LESS_6A error and ChargeDelay
**Requirement:** `REQ-IEC61851-024`

- **Given** The EVSE has LESS_6A error flag set and ChargeDelay > 0
- **When** The state is set to STATE_A
- **Then** LESS_6A is cleared from ErrorFlags and ChargeDelay is set to 0

### State transitions are recorded in the transition log
**Requirement:** `REQ-IEC61851-025`

- **Given** The EVSE is ready to charge
- **When** Two state transitions occur (STATE_B then STATE_C)
- **Then** transition_count is 2 and the log contains both states in order

### Entering STATE_C1 sets PWM to off (+12V)
**Requirement:** `REQ-IEC61851-026`

- **Given** The EVSE is in STATE_C (charging)
- **When** The state is set to STATE_C1 (charging suspended)
- **Then** PWM duty is set to 1024 (off / +12V constant)

### Full charge cycle: A -> B -> C -> B -> A
**Requirement:** `REQ-IEC61851-027`

- **Given** The EVSE is in STATE_A, authorized and ready to charge
- **When** The vehicle connects (9V), requests charge (6V), stops (9V), disconnects (12V)
- **Then** The EVSE transitions A->B->C->B->A with correct contactor states

### Vehicle disconnect during ACTSTART is NOT handled in tick_10ms
**Requirement:** `REQ-IEC61851-028`

- **Given** The EVSE is in STATE_ACTSTART (activation mode)
- **When** A 12V pilot signal is received (vehicle disconnected)
- **Then** The state stays ACTSTART (original behavior: no pilot check in ACTSTART,

### ActivationMode=0 triggers STATE_ACTSTART on pilot detection in STATE_B
**Requirement:** `REQ-IEC61851-029`

- **Given** The EVSE is in STATE_B with ActivationMode=0
- **When** A 9V pilot signal is received
- **Then** The state transitions to STATE_ACTSTART with ActivationTimer set to 3

### ActivationMode=255 (always active) does not count down
**Requirement:** `REQ-IEC61851-031`

- **Given** The EVSE has ActivationMode set to 5
- **Given** The EVSE has ActivationMode set to 255
- **When** One second tick occurs
- **When** One second tick occurs
- **Then** ActivationMode decrements to 4
- **Then** ActivationMode remains at 255

### STATE_ACTSTART returns to STATE_B when ActivationTimer expires
**Requirement:** `REQ-IEC61851-032`

- **Given** The EVSE is in STATE_ACTSTART with ActivationTimer=0 (expired)
- **When** A non-12V pilot signal is received
- **Then** The state transitions to STATE_B and ActivationMode is set to 255

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

### STATE_B entry does NOT set pilot_connected when modem disabled
**Requirement:** `REQ-IEC61851-M3`

- **Given** ModemEnabled=false, EVSE transitions A→B
- **When** evse_set_state is called with STATE_B
- **Then** pilot_connected is NOT explicitly set by STATE_B entry

### STATE_B entry DOES set pilot_connected when modem enabled
**Requirement:** `REQ-IEC61851-M3B`

- **Given** ModemEnabled=true, EVSE transitions to STATE_B
- **When** evse_set_state is called with STATE_B
- **Then** pilot_connected is set to true

</details>

---

## 10ms Tick Processing

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-TICK10-001` | Pilot disconnect guard ignores 9V signal while active | `test_pilot_disconnect_guards_reading` | `test_tick_10ms.c:1` |
| `REQ-TICK10-002` | Pilot disconnect flag clears when timer reaches zero | `test_pilot_disconnect_clears_on_timer_zero` | `test_tick_10ms.c:47` |
| `REQ-TICK10-003` | RFID reader type 1 starts access lock timer | `test_rfid_reader_1_starts_access_timer` | `test_tick_10ms.c:66` |
| `REQ-TICK10-004` | MaxCapacity limits charge current when below MaxCurrent | `test_maxcapacity_limits_charge_current` | `test_tick_10ms.c:85` |
| `REQ-TICK10-005` | MaxCapacity at or above MaxCurrent falls back to MinCurrent | `test_maxcapacity_default_uses_mincurrent` | `test_tick_10ms.c:101` |
| `REQ-TICK10-006` | LESS_6A error flag set when insufficient current available in Smart mode | `test_less_6a_when_no_current_available` | `test_tick_10ms.c:120` |
| `REQ-TICK10-007` | STATE_B with 6V pilot increments state timer for debounce | `test_b_6v_increments_state_timer` | `test_tick_10ms.c:144` |
| `REQ-TICK10-008` | STATE_B with 9V pilot resets the state timer | `test_b_9v_resets_state_timer` | `test_tick_10ms.c:161` |
| `REQ-TICK10-009` | STATE_B to STATE_C transition requires 55 consecutive 6V ticks | `test_b_to_c_debounce_threshold` | `test_tick_10ms.c:178` |
| `REQ-TICK10-010` | STATE_B to STATE_C transition is blocked when errors are present | `test_b_to_c_requires_diode_and_no_errors` | `test_tick_10ms.c:201` |
| `REQ-TICK10-011` | STATE_C transitions to STATE_B after sustained pilot short debounce | `test_c_short_debounce` | `test_tick_10ms.c:222` |
| `REQ-TICK10-012` | STATE_C stays in STATE_C on 6V pilot and resets state timer | `test_c_6v_no_transition` | `test_tick_10ms.c:243` |
| `REQ-TICK10-013` | COMM_B state does not trigger A-to-B transition logic on 9V pilot | `test_comm_b_stays_on_9v` | `test_tick_10ms.c:262` |
| `REQ-TICK10-014` | Node transitions from STATE_B to STATE_COMM_C instead of STATE_C | `test_node_b_to_comm_c` | `test_tick_10ms.c:286` |
| `REQ-TICK10-015` | A-to-B transition sets BalancedMax from MaxCapacity | `test_a_to_b_sets_balanced_max` | `test_tick_10ms.c:309` |
| `REQ-TICK10-016` | A-to-B transition does NOT set extra PWM duty (F3 fidelity fix) | `test_a_to_b_no_extra_pwm` | `test_tick_10ms.c:324` |
| `REQ-TICK10-017` | A-to-B transition initializes ActivationMode to 30 and clears AccessTimer | `test_a_to_b_sets_activation_mode_30` | `test_tick_10ms.c:343` |
| `REQ-TICK10-018` | STATE_B1 remains in B1 when errors are present on 9V pilot | `test_b1_with_errors_stays_b1_on_9v` | `test_tick_10ms.c:360` |
| `REQ-TICK10-019` | Modem states are NOT handled in tick_10ms (matches original Timer10ms) | `test_modem_states_to_a_on_12v` | `test_tick_10ms.c:381` |
| `REQ-TICK10-020` | ACTSTART transitions to STATE_B when activation timer expires | `test_actstart_to_b_when_timer_zero` | `test_tick_10ms.c:403` |

<details>
<summary>Detailed steps (20 scenarios)</summary>

### Pilot disconnect guard ignores 9V signal while active
**Requirement:** `REQ-TICK10-001`

- **Given** EVSE is ready to charge with PilotDisconnected=true and PilotDisconnectTime > 0
- **When** A 9V pilot signal (vehicle connected) is received during a 10ms tick
- **Then** The EVSE remains in STATE_A because the disconnect guard suppresses the pilot reading

### Pilot disconnect flag clears when timer reaches zero
**Requirement:** `REQ-TICK10-002`

- **Given** EVSE is ready to charge with PilotDisconnected=true and PilotDisconnectTime=0
- **When** A 9V pilot signal is received during a 10ms tick
- **Then** PilotDisconnected is cleared to false and pilot_connected is set to true

### RFID reader type 1 starts access lock timer
**Requirement:** `REQ-TICK10-003`

- **Given** EVSE is ready to charge with RFIDReader=1, AccessTimer=0, and AccessStatus=ON
- **When** A 12V pilot signal (no vehicle) is received during a 10ms tick
- **Then** AccessTimer is set to RFIDLOCKTIME to begin the RFID lock countdown

### MaxCapacity limits charge current when below MaxCurrent
**Requirement:** `REQ-TICK10-004`

- **Given** EVSE is ready to charge with MaxCapacity=8A which is less than MaxCurrent=13A
- **When** A 9V pilot signal triggers the A-to-B transition during a 10ms tick
- **Then** ChargeCurrent is set to 80 deciamps (MaxCapacity * 10)

### MaxCapacity at or above MaxCurrent falls back to MinCurrent
**Requirement:** `REQ-TICK10-005`

- **Given** EVSE is ready to charge with MaxCapacity=16A >= MaxCurrent=13A and MinCurrent=6A
- **When** A 9V pilot signal triggers the A-to-B transition during a 10ms tick
- **Then** ChargeCurrent is set to 60 deciamps (MinCurrent * 10) as the default starting point

### LESS_6A error flag set when insufficient current available in Smart mode
**Requirement:** `REQ-TICK10-006`

- **Given** EVSE is in Smart mode standalone with very low MaxMains=2A and MainsMeterImeasured=200 (overloaded)
- **When** A 9V pilot signal (vehicle connected) is received during a 10ms tick
- **Then** The LESS_6A error flag is set because available current is below MinCurrent

### STATE_B with 6V pilot increments state timer for debounce
**Requirement:** `REQ-TICK10-007`

- **Given** EVSE is in STATE_B with DiodeCheck=1 and StateTimer=0
- **When** A 6V pilot signal (vehicle requesting charge) is received during a 10ms tick
- **Then** StateTimer increments to 1, counting toward the B-to-C debounce threshold

### STATE_B with 9V pilot resets the state timer
**Requirement:** `REQ-TICK10-008`

- **Given** EVSE is in STATE_B with StateTimer=30 (partially debounced)
- **When** A 9V pilot signal (vehicle connected but not requesting charge) is received
- **Then** StateTimer is reset to 0, canceling the B-to-C debounce countdown

### STATE_B to STATE_C transition requires 55 consecutive 6V ticks
**Requirement:** `REQ-TICK10-009`

- **Given** EVSE is in STATE_B with DiodeCheck=1 and ChargeCurrent at MaxCurrent
- **When** 50 consecutive 6V pilot ticks are received (below threshold) then 5 more (reaching 55)
- **Then** The EVSE does not transition at 50 ticks but transitions to STATE_C at 55 ticks

### STATE_B to STATE_C transition is blocked when errors are present
**Requirement:** `REQ-TICK10-010`

- **Given** EVSE is in STATE_B with DiodeCheck=1 but ErrorFlags contains TEMP_HIGH
- **When** 55 consecutive 6V pilot ticks are received (past debounce threshold)
- **Then** The EVSE does not transition to STATE_C because an error condition is active

### STATE_C transitions to STATE_B after sustained pilot short debounce
**Requirement:** `REQ-TICK10-011`

- **Given** EVSE is in STATE_C (actively charging)
- **When** Fewer than 50 PILOT_SHORT ticks are received followed by enough to exceed 50 total
- **Then** The EVSE stays in STATE_C below 50 ticks but transitions to STATE_B after 50 consecutive short ticks

### STATE_C stays in STATE_C on 6V pilot and resets state timer
**Requirement:** `REQ-TICK10-012`

- **Given** EVSE is in STATE_C with StateTimer=20
- **When** A 6V pilot signal is received during a 10ms tick
- **Then** The EVSE remains in STATE_C and StateTimer is reset to 0

### COMM_B state does not trigger A-to-B transition logic on 9V pilot
**Requirement:** `REQ-TICK10-013`

- **Given** EVSE is a node (LoadBl=2) in STATE_COMM_B waiting for master confirmation
- **When** A 9V pilot signal is received during a 10ms tick
- **Then** The EVSE does not transition to STATE_B because COMM_B bypasses the A-to-B path

### Node transitions from STATE_B to STATE_COMM_C instead of STATE_C
**Requirement:** `REQ-TICK10-014`

- **Given** EVSE is a node (LoadBl=2) in STATE_B with DiodeCheck=1 and sufficient charge current
- **When** 55 consecutive 6V pilot ticks are received (past debounce threshold)
- **Then** The EVSE transitions to STATE_COMM_C (waiting for master to confirm charge start)

### A-to-B transition sets BalancedMax from MaxCapacity
**Requirement:** `REQ-TICK10-015`

- **Given** EVSE is ready to charge in standalone mode with MaxCapacity=10A
- **When** A 9V pilot signal triggers the A-to-B transition
- **Then** BalancedMax[0] is set to 100 deciamps (MaxCapacity * 10)

### A-to-B transition does NOT set extra PWM duty (F3 fidelity fix)
**Requirement:** `REQ-TICK10-016`

- **Given** EVSE is ready to charge in standalone mode
- **When** A 9V pilot signal triggers the A-to-B transition
- **Then** PWM duty remains 1024 (from STATE_A entry); module does not set PWM on A→B,

### A-to-B transition initializes ActivationMode to 30 and clears AccessTimer
**Requirement:** `REQ-TICK10-017`

- **Given** EVSE is ready to charge in standalone mode
- **When** A 9V pilot signal triggers the A-to-B transition
- **Then** ActivationMode is set to 30 (countdown for activation) and AccessTimer is cleared to 0

### STATE_B1 remains in B1 when errors are present on 9V pilot
**Requirement:** `REQ-TICK10-018`

- **Given** EVSE is in STATE_B1 (connected but waiting) with TEMP_HIGH error flag set
- **When** A 9V pilot signal is received during a 10ms tick
- **Then** The EVSE stays in STATE_B1 because errors prevent transition to charging states

### Modem states are NOT handled in tick_10ms (matches original Timer10ms)
**Requirement:** `REQ-TICK10-019`

- **Given** EVSE is in one of the modem states (REQUEST, WAIT, DONE, or DENIED)
- **When** A 12V pilot signal (no vehicle) is received during a 10ms tick
- **Then** The EVSE stays in its modem state (modem is managed solely by tick_1s)

### ACTSTART transitions to STATE_B when activation timer expires
**Requirement:** `REQ-TICK10-020`

- **Given** EVSE is in STATE_ACTSTART with ActivationTimer=0 (timer expired)
- **When** A 9V pilot signal is received during a 10ms tick
- **Then** The EVSE transitions to STATE_B and ActivationMode is set to 255 (disabled)

</details>

---

## 1-Second Tick Processing

| Requirement | Scenario | Test Function | Source |
|-------------|----------|---------------|--------|
| `REQ-TICK1S-001` | SolarStopTimer decrements by one each second | `test_solar_stop_timer_countdown` | `test_tick_1s.c:1` |
| `REQ-TICK1S-002` | SolarStopTimer expiry triggers STATE_C to STATE_C1 transition | `test_solar_stop_timer_triggers_c1` | `test_tick_1s.c:39` |
| `REQ-TICK1S-003` | SolarStopTimer expiry does not trigger C1 when not in STATE_C | `test_solar_stop_timer_not_in_c` | `test_tick_1s.c:62` |
| `REQ-TICK1S-004` | Node charge timers increment when node is in STATE_C | `test_node_charge_timer_increments` | `test_tick_1s.c:86` |
| `REQ-TICK1S-005` | Node charge timer resets when node is not in STATE_C | `test_node_charge_timer_resets` | `test_tick_1s.c:104` |
| `REQ-TICK1S-006` | Multiple node charge timers update independently based on each node state | `test_multi_node_timers` | `test_tick_1s.c:120` |
| `REQ-TICK1S-007` | MainsMeter timeout sets CT_NOCOMM error on node | `test_mains_meter_timeout_node` | `test_tick_1s.c:144` |
| `REQ-TICK1S-008` | MainsMeter timeout counter decrements on node each second | `test_mains_meter_node_countdown` | `test_tick_1s.c:161` |
| `REQ-TICK1S-009` | LESS_6A error forces STATE_C to STATE_C1 via power unavailable | `test_less_6a_enforces_power_unavailable` | `test_tick_1s.c:179` |
| `REQ-TICK1S-010` | LESS_6A error sets ChargeDelay to CHARGEDELAY (60 seconds) | `test_less_6a_sets_charge_delay` | `test_tick_1s.c:201` |
| `REQ-TICK1S-011` | MaxSumMains timer decrements each second | `test_maxsummains_timer_countdown` | `test_tick_1s.c:226` |
| `REQ-TICK1S-012` | MaxSumMains timer expiry triggers STATE_C to STATE_C1 transition | `test_maxsummains_timer_triggers_c1` | `test_tick_1s.c:241` |
| `REQ-TICK1S-013` | AccessTimer is cleared when EVSE is not in STATE_A | `test_access_timer_cleared_not_in_a` | `test_tick_1s.c:265` |
| `REQ-TICK1S-014` | EV meter timeout counter decrements each second | `test_ev_meter_timeout_countdown` | `test_tick_1s.c:283` |
| `REQ-TICK1S-015` | EV meter timeout reaching zero sets EV_NOCOMM error | `test_ev_meter_timeout_triggers_error` | `test_tick_1s.c:299` |
| `REQ-TICK1S-016` | Activation timer decrements each second | `test_activation_timer_countdown` | `test_tick_1s.c:319` |
| `REQ-TICK1S-017` | ActivationMode counter decrements each second | `test_activation_mode_countdown` | `test_tick_1s.c:336` |
| `REQ-TICK1S-018` | ChargeDelay is overridden by LESS_6A enforcement after decrementing to zero | `test_charge_delay_overridden_by_less_6a` | `test_tick_1s.c:353` |
| `REQ-TICK1S-F2A` | LESS_6A resets ChargeDelay to CHARGEDELAY every tick, even when non-zero | `test_less_6a_resets_charge_delay_every_tick` | `test_tick_1s.c:378` |
| `REQ-TICK1S-F2B` | LESS_6A prevents ChargeDelay from ever reaching zero | `test_less_6a_charge_delay_never_reaches_zero` | `test_tick_1s.c:400` |

<details>
<summary>Detailed steps (20 scenarios)</summary>

### SolarStopTimer decrements by one each second
**Requirement:** `REQ-TICK1S-001`

- **Given** EVSE is in normal mode with SolarStopTimer=3
- **When** A 1-second tick occurs
- **Then** SolarStopTimer decrements to 2

### SolarStopTimer expiry triggers STATE_C to STATE_C1 transition
**Requirement:** `REQ-TICK1S-002`

- **Given** EVSE is in Smart mode in STATE_C with high mains load and SolarStopTimer=1
- **When** A 1-second tick decrements SolarStopTimer to 0
- **Then** The EVSE transitions to STATE_C1 (charging suspended) and LESS_6A error flag is set

### SolarStopTimer expiry does not trigger C1 when not in STATE_C
**Requirement:** `REQ-TICK1S-003`

- **Given** EVSE is in Smart mode in STATE_B (not charging) with SolarStopTimer=1
- **When** A 1-second tick decrements SolarStopTimer to 0
- **Then** The EVSE does not transition to STATE_C1 but LESS_6A error flag is still set

### Node charge timers increment when node is in STATE_C
**Requirement:** `REQ-TICK1S-004`

- **Given** Node 0 is in STATE_C with IntTimer=5 and Timer=100
- **When** A 1-second tick occurs
- **Then** IntTimer increments to 6 and Timer increments to 101

### Node charge timer resets when node is not in STATE_C
**Requirement:** `REQ-TICK1S-005`

- **Given** Node 0 is in STATE_B (connected but not charging) with IntTimer=20
- **When** A 1-second tick occurs
- **Then** IntTimer is reset to 0

### Multiple node charge timers update independently based on each node state
**Requirement:** `REQ-TICK1S-006`

- **Given** Nodes 0 and 2 are in STATE_C (charging) and node 1 is in STATE_B, all with IntTimer=10
- **When** A 1-second tick occurs
- **Then** Nodes 0 and 2 increment to 11 while node 1 resets to 0

### MainsMeter timeout sets CT_NOCOMM error on node
**Requirement:** `REQ-TICK1S-007`

- **Given** EVSE is a node (LoadBl=2) with MainsMeterTimeout=0
- **When** A 1-second tick occurs
- **Then** CT_NOCOMM error flag is set indicating mains meter communication lost

### MainsMeter timeout counter decrements on node each second
**Requirement:** `REQ-TICK1S-008`

- **Given** EVSE is a node (LoadBl=3) with MainsMeterTimeout=5
- **When** A 1-second tick occurs
- **Then** MainsMeterTimeout decrements to 4

### LESS_6A error forces STATE_C to STATE_C1 via power unavailable
**Requirement:** `REQ-TICK1S-009`

- **Given** EVSE is in Smart mode in STATE_C with LESS_6A error flag set and high mains load
- **When** A 1-second tick occurs
- **Then** The EVSE transitions to STATE_C1 (charging suspended due to insufficient power)

### LESS_6A error sets ChargeDelay to CHARGEDELAY (60 seconds)
**Requirement:** `REQ-TICK1S-010`

- **Given** EVSE is in Smart mode in STATE_B1 with LESS_6A error flag set and ChargeDelay=0
- **When** A 1-second tick occurs
- **Then** ChargeDelay is set to CHARGEDELAY (60 seconds) to prevent rapid retry

### MaxSumMains timer decrements each second
**Requirement:** `REQ-TICK1S-011`

- **Given** EVSE has MaxSumMainsTimer=5
- **When** A 1-second tick occurs
- **Then** MaxSumMainsTimer decrements to 4

### MaxSumMains timer expiry triggers STATE_C to STATE_C1 transition
**Requirement:** `REQ-TICK1S-012`

- **Given** EVSE is in Smart mode in STATE_C with high mains load and MaxSumMainsTimer=1
- **When** A 1-second tick decrements MaxSumMainsTimer to 0
- **Then** The EVSE transitions to STATE_C1 and LESS_6A error flag is set

### AccessTimer is cleared when EVSE is not in STATE_A
**Requirement:** `REQ-TICK1S-013`

- **Given** EVSE is in STATE_B with AccessTimer=30
- **When** A 1-second tick occurs
- **Then** AccessTimer is cleared to 0 because it is only relevant in STATE_A

### EV meter timeout counter decrements each second
**Requirement:** `REQ-TICK1S-014`

- **Given** EVMeterType=1 (meter installed) with EVMeterTimeout=5
- **When** A 1-second tick occurs
- **Then** EVMeterTimeout decrements to 4

### EV meter timeout reaching zero sets EV_NOCOMM error
**Requirement:** `REQ-TICK1S-015`

- **Given** EVMeterType=1 with EVMeterTimeout=0 and no existing errors in Smart mode
- **When** A 1-second tick occurs
- **Then** EV_NOCOMM error flag is set indicating EV meter communication lost

### Activation timer decrements each second
**Requirement:** `REQ-TICK1S-016`

- **Given** EVSE has ActivationTimer=3
- **When** A 1-second tick occurs
- **Then** ActivationTimer decrements to 2

### ActivationMode counter decrements each second
**Requirement:** `REQ-TICK1S-017`

- **Given** EVSE has ActivationMode=10
- **When** A 1-second tick occurs
- **Then** ActivationMode decrements to 9

### ChargeDelay is overridden by LESS_6A enforcement after decrementing to zero
**Requirement:** `REQ-TICK1S-018`

- **Given** EVSE is in Smart mode in STATE_B1 with ChargeDelay=1 and LESS_6A error flag set
- **When** A 1-second tick decrements ChargeDelay to 0 then LESS_6A enforcement re-sets it
- **Then** ChargeDelay is set back to CHARGEDELAY (60 seconds) by LESS_6A enforcement

### LESS_6A resets ChargeDelay to CHARGEDELAY every tick, even when non-zero
**Requirement:** `REQ-TICK1S-F2A`

- **Given** EVSE is in Smart mode in STATE_B1 with LESS_6A set and ChargeDelay=30
- **When** A 1-second tick occurs
- **Then** ChargeDelay is reset to CHARGEDELAY (60), not decremented to 29

### LESS_6A prevents ChargeDelay from ever reaching zero
**Requirement:** `REQ-TICK1S-F2B`

- **Given** EVSE is in Smart mode in STATE_B1 with LESS_6A set and ChargeDelay=1
- **When** A 1-second tick occurs (ChargeDelay decrements to 0, then LESS_6A resets it)
- **Then** ChargeDelay is CHARGEDELAY (60), not 0

</details>

---

*Generated by extract_traceability.py | SmartEVSE-3 Specification by Example*