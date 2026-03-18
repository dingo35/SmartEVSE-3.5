# EVCC Integration

[EVCC](https://evcc.io/) is an open-source energy management system for electric
vehicle charging. SmartEVSE can be integrated with EVCC using the REST API via
a custom charger template.

## Requirements

- SmartEVSE firmware with EVCC API extensions (v3.7+ or this branch)
- EVCC 0.130+ (custom charger template support)
- SmartEVSE and EVCC on the same network

## EVCC Custom Charger Template

Add the following to your `evcc.yaml` configuration:

```yaml
chargers:
  - name: smartevse
    type: custom
    status:
      source: http
      uri: http://smartevse-xxxx.local/settings
      jq: .evse.iec61851_state
      # Returns "A" (disconnected), "B" (connected), "C" (charging),
      # "D" (ventilation), "E" (error), or "F" (not available)
    enabled:
      source: http
      uri: http://smartevse-xxxx.local/settings
      jq: .evse.charging_enabled
    enable:
      source: http
      uri: http://smartevse-xxxx.local/settings?mode={{if .enable}}1{{else}}0{{end}}
      method: POST
    maxcurrent:
      source: http
      uri: http://smartevse-xxxx.local/settings?override_current={{mul .maxcurrent 10}}
      method: POST
    phases1p3p:
      source: http
      uri: http://smartevse-xxxx.local/settings?phases={{.phases1p3p}}
      method: POST
```

Replace `smartevse-xxxx.local` with your SmartEVSE hostname or IP address.

## API Fields Used

| EVCC Interface | SmartEVSE Endpoint | JSON Path / Parameter |
|----------------|--------------------|-----------------------|
| `status` | GET /settings | `.evse.iec61851_state` |
| `enabled` | GET /settings | `.evse.charging_enabled` |
| `enable` | POST /settings | `mode=1` (on) / `mode=0` (off) |
| `maxcurrent` | POST /settings | `override_current=<amps*10>` |
| `phases1p3p` | POST /settings | `phases=1` or `phases=3` |

## IEC 61851 State Mapping

| Letter | Meaning | SmartEVSE States |
|--------|---------|-----------------|
| A | Disconnected | STATE_A (Ready to Charge) |
| B | Connected, not charging | STATE_B (Connected to EV), plus intermediate states |
| C | Charging | STATE_C (Charging) |
| D | Charging with ventilation | STATE_D |
| E | Error (protective earth) | Any state with hard error flags |
| F | Not available | Unknown or invalid state |

## Phase Switching

The `phases` parameter triggers safe 1P/3P switching via the C2 contactor.
Requirements:

- C2 contactor must be installed and configured (enable_C2 != "Not present")
- SmartEVSE must be Master or standalone (not a load balancing slave)

The state machine handles the full disconnect-switch-reconnect sequence
automatically. There is no need to manually set the mode to OFF first.

## Notes

- Current values in the SmartEVSE API are in deciamps (dA): 160 = 16.0A.
  EVCC passes amps, so the template multiplies by 10.
- The `override_current` parameter only works in NORMAL or SMART mode.
- Phase switching has a built-in cooldown via the ChargeDelay mechanism.
