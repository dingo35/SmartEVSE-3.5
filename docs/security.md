# Security

This firmware runs on your LAN and controls a 32 A circuit. The
security defaults and features here are what the operator controls.

For defaults-only summary see §2. For the full model see §3 onwards.

---

## 1. Threat model

Realistic threats:

| Threat | Mitigation |
|---|---|
| Malicious website triggers charge commands via CSRF | Origin check (§5) |
| Guest on your WiFi brute-forces the LCD PIN | Rate-limited PIN verify (§6) |
| Guest on your WiFi flashes hostile firmware | Signed firmware + LCD-PIN gate (§7) |
| Guest on your WiFi reads your Wi-Fi / MQTT / OCPP secrets | Redacted GET endpoints (§4) |
| Attacker with LAN + PIN steals session tokens | 30-min session timeout (§6) |
| Attacker with physical USB access | Out of scope — USB flash bypasses all |

Not in scope:

- Attackers with physical access to the device. A malicious actor at
  your meter cabinet can replace the whole charger.
- Attackers on the WAN side if the Web UI is port-forwarded. Don't do
  that. The Web UI is designed for LAN use.
- Denial-of-service on the local network.

---

## 2. Defaults

Out of the box, on a fresh install:

| Setting | Default | Secure? |
|---|---|---|
| AuthMode | 0 (Off — no auth required on Web UI) | Fine on a trusted home LAN. Enable for shared LAN. |
| LCDPin | 0 (unset) | Must set a PIN (non-zero) to use AuthMode=1 |
| Firmware upload | Signed only | Yes — unsigned rejected |
| OCPP auth key | Not returned in GET /settings | Yes — redacted |
| MQTT password | Not returned in GET /settings | Yes — redacted |
| Origin / CSRF check | Active when AuthMode=1 | Yes |

If you're on a shared LAN (apartment Wi-Fi, office, rented space),
enable AuthMode=1. On your own home LAN where you trust every
connected device, AuthMode=0 is fine.

---

## 3. AuthMode — the master switch

`AuthMode` is a persisted setting with two values:

| Value | Behaviour |
|---|---|
| `0` (Off, default) | Web UI and REST API are open to anyone on the LAN. No authentication required. Backward-compatible with pre-auth integrations (existing HA setups, curl scripts). |
| `1` (Required) | Every mutating endpoint (`POST /settings`, `POST /mode`, `POST /update`, etc.) requires a valid session. Session obtained via `POST /lcd-verify-password`. |

### Enabling AuthMode

1. Set a non-zero LCDPin first. Web UI → settings → LCD PIN, or via
   the LCD menu.
2. Web UI → Security → AuthMode → **Required**.
3. Save. The Web UI will redirect to a PIN prompt.

### Disabling AuthMode

1. LCD menu → settings → AuthMode = Off.
2. Or via the Web UI while currently authenticated.

> [!IMPORTANT]
> Never enable AuthMode=1 without first setting a non-zero LCDPin. The
> Web UI prevents this, but if you bypass the UI (via MQTT / REST) you
> can lock yourself out. Recovery requires physical LCD access.

### Effect on existing integrations

- **Home Assistant via MQTT**: unaffected. MQTT is a separate channel
  with its own auth.
- **HA via REST**: breaks until HA sends `X-Auth-PIN` header. If you
  rely on HA calling REST, stay on AuthMode=0 or add the header.
- **OCPP backend**: unaffected. OCPP is a separate outbound WebSocket.
- **Browser**: prompts for PIN once per 30-minute session.

---

## 4. Secret redaction in GET /settings

Three secrets are configured through the Web UI but never returned in
the settings JSON:

| Secret | What you get instead | Upload semantics |
|---|---|---|
| MQTT password | `mqtt.password_set: true/false` | Empty or `••••••••` placeholder = keep existing. Any other value = replace |
| OCPP auth_key | `ocpp.auth_key_set: true/false` | Same — empty or bullets keep existing |
| WiFi password | not exposed at all | Set via LCD WiFi menu only |

This means if a malicious script on your LAN calls `GET /settings`, it
learns only **that** a MQTT password is set, not what it is.

### Why the bullets

The Web UI displays `••••••••` in password fields when a secret is
configured. If the user saves without retyping, the frontend and
backend both skip the field (don't overwrite). Prevents the common
"save settings → lose password" bug.

---

## 5. CSRF / Origin check

Only active when AuthMode=1. When the Web UI / API receives a browser
request with an `Origin:` header, the header must match the device's
own hostname or IP. Cross-origin POSTs from malicious websites are
rejected with 403.

Non-browser clients (curl, Python requests, HA REST command) usually
don't send `Origin:` — they're unaffected.

For fully technical details see the header-matching logic in
`src/http_auth.c::http_auth_origin_matches`.

---

## 6. PIN rate limiter

`/lcd-verify-password` has an escalating backoff to defeat brute-force
of the 4-digit PIN.

| Consecutive failures | Cooldown |
|---|---|
| 1–2 | None |
| 3 | 10 s |
| 4 | 60 s |
| 5 | 5 min |
| 6+ | 30 min (cap) |

During cooldown, the endpoint returns **429 Too Many Requests** with a
`Retry-After: <seconds>` header and does **not** attempt the PIN
compare (avoids timing side-channel). A successful PIN clears the
counter. An idle period > 10 minutes also clears if no cooldown is
active.

At steady state, an attacker gets ~50 attempts/day. Full 10,000-PIN
space takes ~200 days — defeats scripted brute-force.

---

## 7. Firmware signature verification

The OTA upload endpoint (`/update`) only accepts firmware images signed
with one of the trusted public keys baked into the running firmware.
Unsigned `firmware.bin` / `firmware.debug.bin` is rejected on release
builds.

### Multi-key setup

Two trusted keys are embedded:

| Key | Purpose |
|---|---|
| Key 0 | Community / reference builds |
| Key 1 | Additional release channel |

A firmware image is accepted if it's signed by **any** of the trusted
keys. This lets release channels rotate without re-flashing every
device to swap keys.

### Debug-build unsigned uploads

Debug builds (compiled with `DBG=1`) offer an additional path for local
iteration: `firmware.bin` and `firmware.debug.bin` can be uploaded
unsigned **when** the LCD PIN is set (non-zero) **and** has been
verified in the current Web-UI session. Physical-presence auth stands
in for the signature.

Release builds never accept unsigned uploads. This is a hard
compile-time distinction, not a runtime toggle.

### Why this matters

Without this, a LAN attacker could flash hostile firmware via plain
HTTP with no signature and no authentication — unauthenticated RCE.
The signature + LCD-PIN gate closes that hole.

### Flashing a new firmware version

Download `firmware.signed.bin` from the
[releases page](https://github.com/basmeerman/SmartEVSE-3.5/releases)
and upload via the Web UI. The signature is validated on-device before
the boot partition is swapped. Invalid signatures are rejected and the
target partition is erased.

---

## 8. Session timeout

After `POST /lcd-verify-password` succeeds, the authenticated session
(`LCDPasswordOK`) stays active for 30 minutes of idle time. Every
successful authenticated request resets the clock. 30 minutes without
any authenticated activity → the session expires and the next request
returns 401.

30 minutes is hardcoded; future releases may make it configurable.

The session is a server-side flag, not a signed cookie. This means:

- A single authenticated session covers **all** clients on the LAN —
  once someone verifies the PIN, anyone else on the LAN has 30 minutes
  to act. Matches the "trusted LAN" assumption.
- Future work (Phase 3 of the auth plan) adds per-client HMAC cookies
  if the threat model demands per-client isolation.

---

## 9. MQTT / OCPP — channel-specific notes

### MQTT

- Password in transit: plaintext by default. Enable TLS on the broker
  if secrets matter.
- Topic prefix is not a secret — anyone who can read the broker can
  read your device's messages regardless of prefix.
- Change-Only publishing (enabled by default in recent releases)
  reduces information exposure: less traffic, fewer periodic secret
  dumps. Note: no secret is ever in an MQTT publish either way; this
  is about volume, not disclosure.

### OCPP

- TLS WebSocket (`wss://`) recommended. Plain `ws://` works but sends
  auth_key in the clear.
- The `auth_key` field is the Basic Auth password for the WebSocket
  handshake. Treat it as secret. It's never logged to telnet or
  returned in `/settings`.
- RemoteStart / RemoteStop from the backend are unconditionally
  obeyed when OCPP is enabled — ensure your OCPP backend's own access
  control is tight.

---

## 10. Logging — what's safe to share

### Telnet debug log

Full runtime log. **Do** share when filing bug reports. **Check for
secrets before posting publicly** — the log does not normally include
passwords, auth keys, or WiFi credentials (these are explicitly
redacted in firmware), but your MQTT topic structure, IP addresses,
and behaviour patterns are visible.

### /settings JSON

Safe to share. Passwords (MQTT, WiFi, OCPP auth_key) are all redacted
to `_set: bool` booleans. You can post this verbatim.

### Boot log

Safe. The firmware explicitly redacts the generated MQTT password's
hash to "first 4 chars + ellipsis" (security fix C-5). Full password
never appears.

---

## 11. What to do if you suspect compromise

1. **Physical access.** Power off the charger at the main breaker.
2. **Settings wipe.** Factory reset via LCD menu → Settings → Reset.
   Clears NVS including WiFi, LCDPin, MQTT password, OCPP auth_key.
3. **Firmware reflash via USB.** Bypasses OTA, guarantees known
   firmware. See [building_flashing.md](building_flashing.md).
4. **Rotate external secrets.** MQTT broker password, OCPP auth_key,
   WiFi password (if the WiFi was compromised, the whole network
   needs attention, not just this device).
5. **Check the main breaker history.** Any charging sessions you
   didn't initiate?

---

## 12. Reporting security issues

Please use GitHub's private vulnerability reporting on the repository
rather than a public issue. Private report → maintainer gets notified →
fix developed → coordinated disclosure.

For architectural context on security reviews see
[docs/security/](security/) (internal working documents).
