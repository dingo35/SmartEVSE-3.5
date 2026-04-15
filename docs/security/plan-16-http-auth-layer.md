# Plan 16 — HTTP Authentication Layer

**Status:** Design — awaiting implementation.
**Security findings closed by this plan:** C-3, H-1, H-2, H-3, H-7, M-1, M-2, M-8 (and hardens C-2 further with enforced auth on `GET /settings`).
**Scope:** Fork-only (basmeerman/SmartEVSE-3.5). Not a candidate for upstream PR — the fork-only auth model creates behavioral incompatibility with any unauthenticated integration.

## Problem statement

Every state-changing HTTP endpoint in the fork (inherited from upstream) is reachable without authentication from any device on the local network:

- `POST /update` (mitigated to signed-only in PR #140, but still no auth)
- `POST /reboot`, `POST /erasesettings`
- `POST /rfid` (adds an RFID card to the whitelist)
- `POST /settings?...` (changes OCPP URL, MQTT creds, current limits, modes)
- `POST /diag/*` (diagnostic capture control)
- `GET /settings` (returns sensitive config including MQTT host/user/prefix and OCPP URL/CB-Id)
- `GET /mqtt_ca_cert`, `GET /diag/download`, `GET /diag/file/*`

Combined with other ambient-LAN risks (DNS rebinding, malicious websites, compromised IoT devices, shared-tenant WiFi), this creates a large attack surface. The individual fixes that have shipped (C-1 signed-firmware-only, C-2 auth_key redacted, H-4 SSRF validator) close specific paths but don't address the underlying model: **the device trusts its LAN implicitly**.

## Goal

Provide a single, optional authentication gate that covers every mutating or credential-exposing HTTP endpoint, **without bricking existing installs** that rely on unauthenticated access (Home Assistant integrations, custom dashboards, scripts).

## Non-goals

- Per-user auth. One shared credential (LCD PIN) is sufficient for a household device.
- Replacing the existing LCD PIN mechanism. Reuse it.
- Cryptographic session tokens. Session-less server-side flag (`LCDPasswordOK`) matches the existing model; upgrade path to tokens is future work.
- OAuth / federated identity. Out of scope.
- Breaking `WebSocket /ws/lcd` which already gates on `LCDPasswordOK`.

## Backward compatibility — the central design choice

### Setting: `AuthMode`

| Value | Behavior |
|---|---|
| `0` (Off) | All endpoints remain unauthenticated. Existing behavior preserved. **Default on upgrade** — will not break any installation. A prominent banner appears in the Web UI warning that authentication is disabled. |
| `1` (Required) | All mutating endpoints + sensitive-data GETs require the LCD PIN. Rate-limited, CSRF-protected. |

Stored in NVS alongside `LCDPin`. Settable from:
- LCD menu (new `MENU_AUTH` item — numeric slot TBD, following the existing "> MENU_STATE" convention so we don't cascade-renumber)
- Web UI (new row in Security panel)
- MQTT (`/Set/AuthMode`)
- REST (`POST /settings?auth_mode=0|1`)

### Default chosen: `AuthMode = 0`

Reasoning:
1. **An upgrade must not brick the device for any user.** Home Assistant, MQTT automations, scripts calling `/settings` all exist today; all would fail silently under AuthMode=1 until the user notices and creates a PIN.
2. Users who have never set `LCDPin` have no way to authenticate — forcing AuthMode=1 would lock them out of the Web UI too.
3. A visible nag banner inside the Web UI is the right amount of pressure: "Anyone on your network can control this charger. Enable authentication in Security →".
4. Power users / security-conscious users explicitly opt in.

### Upgrade analysis: will any security change erase config?

Independent of AuthMode, the security review fixes already shipped must not drop user configuration on upgrade. Checked each:

| Fix | NVS keys touched | Verdict |
|---|---|---|
| C-1 unsigned-flash rejection (PR #140) | none | ✓ safe |
| C-2 OCPP auth_key redaction (PR #144) | none | ✓ safe |
| C-5 log redaction (PR #141) | none | ✓ safe |
| H-4 OCPP URL validator (PR #142) | none | ✓ safe. **Side effect:** a previously-accepted URL containing `127.x`, `localhost`, `169.254.x`, or `user@host` will now be rejected **when the user tries to save it**. The URL in NVS is loaded as-is at boot; the next save-settings cycle will fail validation and the user will see the new error message. In practice no legitimate user has such a URL stored, but note the case. |
| H-5 strncpy NUL (PR #141) | none | ✓ safe |
| M-3/M-4 firmware_manager (PR #143) | none | ✓ safe |
| Plan 16 (this doc) | **adds** `AuthMode` key (default 0) | ✓ safe. Existing keys unchanged. |

No security fix erases user config. Good.

### Ecosystem compatibility

| Consumer | Effect under AuthMode=0 | Effect under AuthMode=1 |
|---|---|---|
| Web UI (same device) | no change | after the user sets PIN, works as today |
| Home Assistant MQTT integration | no change | no change (MQTT path is separate; not covered by HTTP auth) |
| Home Assistant REST integration | no change | **breaks** — needs `X-Auth-PIN` header or the AuthMode stays off |
| Custom dashboards via HTTP | no change | **breaks** — same |
| LCD config | no change | no change |
| OCPP CSMS (outbound) | no change | no change |
| EVCC | depends — many users poll `/settings` via HTTP | same |
| `smartevse.nl` app server | MQTT-based, no HTTP | no change |

**Recommendation:** document that users with REST integrations should either (a) update their integration to send `X-Auth-PIN: <pin>`, or (b) keep AuthMode=0 and restrict LAN access via firewall / separate IoT VLAN.

## Authentication mechanism

### Server-side gate

Reuse the existing `LCDPasswordOK` flag (currently controls `/lcd` GET and the `/ws/lcd` WebSocket button-command handler). Extend to:

- Every mutating endpoint reads `LCDPasswordOK` **only when `AuthMode == 1`**.
- When AuthMode==0, endpoints behave exactly as today.

Endpoints gated:
```
GET  /settings               (AuthMode=1 only — legacy clients still read under AuthMode=0)
POST /settings               (any form)
POST /color_off, /color_normal, /color_smart, /color_solar, /color_custom
POST /currents, /ev_meter, /cablelock, /rfid, /ev_state
POST /autoupdate, /update, /reboot, /erasesettings
POST /diag/start, /diag/stop, /diag/dump
GET  /diag/status, /diag/download, /diag/files, /diag/file/*
DEL  /diag/file/*
GET  /mqtt_ca_cert
POST /automated_testing     (verify #if AUTOMATED_TESTING is 0 in release regardless)
```

NOT gated (intentional):
- `GET /` and the static UI files (HTML/CSS/JS/images)
- `POST /lcd-verify-password` (the auth endpoint itself)
- `POST /save` (WiFi-setup captive-portal — gated by AP-mode check)
- `GET /ev_state`, `GET /session/last` — public read-only status (like a meter reading)
- `WebSocket /ws/lcd` — already gated
- `WebSocket /ws/data` — left open for dashboards (read-only stream; sensitive fields redacted by same policy as /settings)

### Client identification

The `LCDPasswordOK` flag is per-server, not per-client. That means once ONE client authenticates, every client behaves as authenticated. This matches the current upstream model. Upgrade to per-client sessions (HMAC cookie or token) is deferred to a future plan.

**Pragmatic mitigations:**
- `LCDPasswordOK` times out after N minutes of inactivity (reset to false).
- Timeout resets on every authenticated request (keepalive).
- Suggested value: 30 minutes.

### Rate limiting on `/lcd-verify-password`

Per-IP small ring buffer (8 entries, LRU eviction). Extractable as pure C for unit-testability.

```c
enum { RL_OK, RL_DELAY, RL_LOCKED };

typedef struct {
    uint32_t ip;             // source IP (IPv4 only for now)
    uint32_t last_fail_ms;
    uint8_t  fail_count;
} rl_entry_t;

typedef struct { rl_entry_t entries[8]; } rl_state_t;

rl_result_t rl_check   (rl_state_t *st, uint32_t now_ms, uint32_t ip);
void        rl_record_fail(rl_state_t *st, uint32_t now_ms, uint32_t ip);
void        rl_record_ok  (rl_state_t *st, uint32_t ip);
```

Policy:
- First 3 failures: free
- 4th: 5 s delay
- 5th: 60 s delay
- 6th+: 15 min lockout (unlocks on success from same IP, or after 1 hour total)

**IPv6:** not supported by the current network stack integration. If/when IPv6 becomes addressable, extend `ip` to 16 bytes.

### CSRF — Origin header check

Browsers always send `Origin` on POSTs from scripts/fetch. Non-browsers (curl, scripts, HA) typically don't.

Policy when AuthMode=1:
- Mutating request has `Origin` header → it must match the charger's own hostname/IP. Else 403.
- No `Origin` header → allowed (non-browser client has deliberate intent).

This blocks malicious-website CSRF attacks without breaking integrations.

### UI changes

1. Add a **Security** section to the dashboard (inside the existing Settings panel):
   - Toggle: "Require authentication for configuration changes"
   - PIN-change field (reuses existing `/lcd-verify-password` → `/settings?lcd_pin=...`)
   - Session timeout selector (default 30 min)
2. Nag banner when AuthMode=0:
   - Red/yellow banner at the top of the dashboard
   - "Authentication is disabled. Anyone on your network can control this charger. [Enable now]"
   - Dismissable for the session; re-appears on next page load
3. When AuthMode=1 and LCDPasswordOK=false: overlay prompting for PIN before showing the dashboard (reuse the existing `/lcd-verify-password` flow)

## Implementation phases

### Phase 1 — AuthMode + middleware + mutating endpoints (this plan's primary PR)

- Add `AuthMode` NVS setting + menu + web UI + MQTT + REST
- New pure C helper `http_auth_request_allowed(auth_mode, password_ok, origin, allowed_origin) → bool` + unit tests
- Apply `require_auth(hm, c)` to every mutating endpoint
- Apply to `GET /settings` (sensitive config) only when AuthMode=1
- LCDPasswordOK timeout
- UI: Security section + nag banner (when off)
- Docs update

**Size:** ~400 LOC + ~150 LOC tests. Substantial PR, but coherent.

### Phase 2 — Rate-limit + CSRF (follow-up PR)

- Pure C rate-limit ring buffer + tests
- Integrate in `/lcd-verify-password` handler
- Origin header check helper + apply to mutating endpoints

**Size:** ~200 LOC + ~100 LOC tests.

### Phase 3 — Deferred / future

- Per-client session tokens (HMAC-signed cookie)
- IPv6-aware rate limiting
- Mandatory-auth on-device setup flow for new devices
- `smartevse.cli` bash/python client wrapper that handles PIN injection for scripted use

## Testing strategy

1. **Pure C unit tests** for `http_auth_request_allowed()` and the rate-limit ring. Exhaust the truth table. Add to existing `test/native/tests/`.
2. **BDD/integration tests**: extend existing OCPP/Modbus compat harness with auth coverage. Optional.
3. **On-device smoke test matrix** (manual):
   - [ ] Upgrade existing device → AuthMode=0 (default), no user-visible change
   - [ ] Turn AuthMode=1 from LCD → Web UI prompts for PIN; existing REST scripts fail with 401; MQTT unaffected
   - [ ] Turn AuthMode=1 from LCD with LCDPin=0000 (unset) → UI blocks the toggle and shows "Set a PIN first"
   - [ ] Rate-limit triggers on 4th wrong PIN
   - [ ] CSRF: malicious origin blocked; legitimate Origin (the device's own IP) allowed
   - [ ] Session timeout after 30 min idle

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| User locks themselves out of Web UI by turning AuthMode=1 without a PIN | Menu / UI guard: cannot enable AuthMode unless LCDPin != 0. LCD override: a physical button-press sequence at the device clears AuthMode back to 0 (documented factory-reset-lite). |
| Integrations silently break on upgrade | AuthMode=0 default; nag banner in UI; release notes |
| LCD PIN (10k entropy) brute-forceable over slow network | Rate limit in Phase 2 makes 10k attempts take ~4 years |
| Per-server LCDPasswordOK means lateral movement after one auth | Documented as known limitation; Phase 3 adds per-client sessions |
| User loses PIN, can't reset device via unauthenticated `/erasesettings` | LCD menu can set a new PIN at any time (physical access required). If physical access is also lost, full firmware reflash recovers. |
| Home Assistant users who relied on unauthenticated REST | Keep AuthMode=0 or update HA config to send `X-Auth-PIN` header |

## Open questions for the user

1. **Session timeout default** — 30 min reasonable? Alternatives: 5 min, 1 hour, "until browser closes".
2. **`GET /settings` in AuthMode=0** — should we still redact MQTT username / OCPP backend URL (as "information disclosure" hardening), or only touch these under AuthMode=1? My default is: leave MQTT/OCPP visible under AuthMode=0 to preserve HA compat, redact under AuthMode=1.
3. **Nag banner dismissal** — session-only re-prompt, or permanently-dismissable with a "don't show again" checkbox?
4. **`GET /debug`** — add to the gated list, or leave public (it's read-only)?

## Acceptance criteria

Phase 1 complete when:
- [ ] `AuthMode` setting persists across reboots
- [ ] AuthMode=0 gives identical behavior to current master on every gated endpoint
- [ ] AuthMode=1 returns HTTP 401 on gated endpoints without PIN; 200 with correct PIN via `/lcd-verify-password`
- [ ] LCDPasswordOK resets after 30 min idle
- [ ] Nag banner appears when AuthMode=0
- [ ] Unit tests cover the `http_auth_request_allowed()` truth table
- [ ] 5-step verification green
- [ ] On-device smoke test matrix all pass
- [ ] Docs: configuration.md has a Security section; upstream-differences.md notes the divergence

---

**Next action:** user to review this design + answer the 4 open questions. Once answered, Phase 1 implementation PR follows.
