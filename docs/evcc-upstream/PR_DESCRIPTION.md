# PR: Add SmartEVSE HTTP charger template

**Target repository:** evcc-io/evcc
**Target file:** `templates/definition/charger/smartevse-http.yaml`

---

## Title

feat: add Smart EVSE HTTP charger template

## Body

### Summary

Add an HTTP REST API based charger template for
[Smart EVSE](https://github.com/SmartEVSE) (Edgetech), complementing the
existing Modbus-based `smartevse` template.

This enables WiFi-only integration without RS485 wiring, which is simpler
for most users.

### Features

- IEC 61851 status reporting (A-F)
- Enable/disable charging (mode 0=OFF / 1=NORMAL)
- Current control via override_current (deciamps)
- 1-phase / 3-phase switching via C2 contactor
- EV meter data: power (W), per-phase currents, charged energy (kWh)

### Firmware Requirements

Requires SmartEVSE firmware with EVCC API extensions that add:
- `evse.iec61851_state` field in GET /settings (IEC 61851-1 state letter)
- `evse.charging_enabled` field in GET /settings (boolean)
- `phases` parameter in POST /settings (1 or 3)

These extensions are available in the SmartEVSE firmware from
[basmeerman/SmartEVSE-3.5](https://github.com/basmeerman/SmartEVSE-3.5)
on branch `work/plan-04`.

### Template Details

| EVCC Interface | Endpoint | Method |
|----------------|----------|--------|
| status | GET /settings → `.evse.iec61851_state` | GET |
| enabled | GET /settings → `.evse.charging_enabled` | GET |
| enable | POST /settings?mode=0/1 | POST |
| maxcurrent | POST /settings?override_current=N | POST |
| phases1p3p | POST /settings?phases=1/3 | POST |
| power | GET /settings → `.ev_meter.import_active_power` | GET |
| currents | GET /settings → `.ev_meter.currents.L1/L2/L3` | GET |
| energy | GET /settings → `.ev_meter.charged_wh` | GET |

### Notes

- SmartEVSE uses deciamps (1/10 A) for current values — the template
  multiplies maxcurrent by 10 and divides meter currents by 10.
- Energy is reported in Wh — scaled to kWh with `scale: 0.001`.
- The `enable` command sets mode to NORMAL (1) or OFF (0). Users in
  SOLAR or SMART mode should use EVCC's solar management instead.
- Phase switching requires C2 contactor hardware (enable_C2 != "Not present").

### Testing

Tested with SmartEVSE v3.x hardware running firmware with EVCC API extensions.

---

## Checklist before submitting

- [ ] Firmware with EVCC extensions is tested on real hardware
- [ ] Template YAML validates with `evcc charger` command
- [ ] All EVCC interfaces (status, enable, maxcurrent, phases1p3p) work end-to-end
- [ ] EV meter data (power, currents, energy) reads correctly
- [ ] Phase switching completes successfully in both directions (1P→3P, 3P→1P)
