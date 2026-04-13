# Security review — 2026-04 — summary of changes

Scope of this document:

- What was reviewed, what was fixed, what was left for later
- Upstream incompatibilities introduced by the fixes (UI / web / LCD / REST / MQTT)
- Configuration-preservation analysis on upgrade (does any fix erase settings?)
- HTTP auth backward-compatibility strategy (the `AuthMode` setting)

## Review scope

Full scan of `SmartEVSE-3/src/`, `SmartEVSE-3/data/`, `SmartEVSE-3/platformio.ini`,
and `.github/workflows/*.yaml`. Attack surfaces modeled:

- HTTP (port 80 + 443) — all endpoints in `http_handlers.cpp` and `network_common.cpp`
- WebSocket `/ws/lcd`, `/ws/data`
- MQTT (subscribe + publish)
- OCPP WebSocket (outbound to CSMS)
- Modbus RTU (RS485, local-only)
- Firmware OTA (via HTTP and via MicroOcpp FirmwareService — the latter deferred)
- Physical (LCD buttons + RFID reader + serial flash port)

Threat models:
- **Primary:** attacker on home LAN (guest device, compromised IoT, ambient WiFi)
- **Secondary:** malicious OCPP CSMS / MQTT broker, DNS rebinding from malicious website
- **Tertiary:** physical access during service

## Findings at a glance

| Severity | Count | Fork closed | Fork opt-in (AuthMode=1) | Outstanding |
|---|---|---|---|---|
| Critical | 5 | 3 (C-1, C-2, C-5) | 1 (C-3) | 1 (C-4 shared TLS key) |
| High     | 7 | 2 (H-4, H-5) | 5 (H-1, H-2, H-3, H-6, H-7) | 0 |
| Medium   | 8 | 2 (M-3, M-4) | 2 (M-1, M-8) | 4 (M-2 compile-guard verify, M-5 secure boot, M-6 reconnect backoff, M-7 pairing PIN) |
| Low      | 4 | 1 (L-4) | 0 | 3 (L-1 cert refresh — blocked on C-4; L-2 CA pin — upstream; L-3 Mongoose strcpy — upstream) |
| Info     | 3 | 0 | 0 | 3 (hygiene) |

## What the fork shipped

Seven fork-only PRs, all merged, all with full CI green, all production-safe on upgrade:

| PR | Scope | Severity closed | LOC | Tests added |
|---|---|---|---|---|
| [#140](https://github.com/basmeerman/SmartEVSE-3.5/pull/140) | Reject unsigned firmware at `POST /update` | **C-1** Critical | ~30 | — |
| [#141](https://github.com/basmeerman/SmartEVSE-3.5/pull/141) | MQTT password redacted from boot log · strncpy NUL · 2 new CLAUDE.md rules | **C-5** + **H-5** | ~30 | — |
| [#142](https://github.com/basmeerman/SmartEVSE-3.5/pull/142) | OCPP URL validator rejects SSRF + embedded creds | **H-4** High | ~250 | 13 |
| [#143](https://github.com/basmeerman/SmartEVSE-3.5/pull/143) | Full-partition erase on sig-fail · zero hash before free | **M-3** + **M-4** | ~20 | — |
| [#144](https://github.com/basmeerman/SmartEVSE-3.5/pull/144) | OCPP auth_key never returned via `/settings` GET | **C-2** Critical | ~30 | — |
| [#145](https://github.com/basmeerman/SmartEVSE-3.5/pull/145) | Plan 16 design doc (auth layer architecture + BC strategy) | docs | 251 | — |
| [#146](https://github.com/basmeerman/SmartEVSE-3.5/pull/146) | Plan 16 Phase 1 — opt-in HTTP auth layer | **C-3** + **H-1** + **H-2** + **H-3** + **H-7** + **M-1** + **M-8** (*when enabled*) | ~600 | 14 |
| [#147](https://github.com/basmeerman/SmartEVSE-3.5/pull/147) | Upstream disclosure draft | docs | 255 | — |
| [#148](https://github.com/basmeerman/SmartEVSE-3.5/pull/148) | Signedness fix in HTTP body loop | **L-4** Low | ~8 | — |

**Total:** 9 PRs, ~1.5 kLOC, 27 new unit tests.

## Upstream compatibility matrix

Changes intentionally diverge from upstream `dingo35/SmartEVSE-3.5`. Relevance for users
switching between upstream and fork builds:

### User-visible behavior (fork-only; upstream retains the old behavior)

| Area | Divergence | Upstream behavior | Fork behavior | User impact when switching fork→upstream or upstream→fork |
|---|---|---|---|---|
| **Firmware upload** | C-1 | Accepts `firmware.bin` (unsigned) | Rejects `firmware.bin`; `firmware.signed.bin` only | User upgrading from upstream → fork must use the `.signed.bin` asset from the release workflow. Local dev builds can still flash via serial (`pio run -t upload`). |
| **OCPP URL** (Web / REST / MQTT) | H-4 | Accepts loopback, link-local, embedded creds | Rejects them with explicit error messages | Any user whose previously-working URL contained `127.x`, `localhost`, `169.254.x`, `[::1]`, `fe80::`, or `user:pass@host` pattern will get a rejection on save. In practice no legitimate deployment uses these; RFC1918 private ranges are still accepted so self-hosted SteVe keeps working. |
| **OCPP auth_key display** | C-2 | `GET /settings` returns plaintext key | Returns `auth_key_set: bool`; Web UI shows `"••••••••"` placeholder | Web UI displays "configured" rather than the actual password. REST clients that parsed `ocpp.auth_key` must change to `ocpp.auth_key_set`. |
| **HTTP auth** (new AuthMode setting) | Plan 16 | None — everything open on LAN | Opt-in `AuthMode` (0=Off legacy, 1=Required). **Default 0** on upgrade. | When left at default (0): no behavior change. When user opts into 1: all mutating endpoints + `GET /settings` require LCD PIN. |
| **Boot log** | C-5 | Prints full MQTT password hash | Prints 4-char prefix + `[redacted]` | Diagnostic-log consumers see truncated value. Debugging parity unchanged ("does the hash look right?" still answerable). |

### UI / web-dashboard incompatibilities

| UI piece | Divergence | Detail |
|---|---|---|
| `update2.html` | C-1 | Removed `firmware.bin` from the list of acceptable uploads; added explicit note about signature requirement. |
| `index.html` (OCPP panel) | C-2 | "Password" field now shows `••••••••` placeholder when backend has the key configured; actual plaintext never sent from backend. |
| `app.js` save flow | C-2 | `configureOcpp()` only POSTs `ocpp_auth_key` when the user actually typed a new value — prevents overwriting the real secret with bullets. |
| `/settings` JSON schema | C-2, Plan 16 | Added fields: `ocpp.auth_key_set` (replaces `ocpp.auth_key`), `settings.auth_mode`, `settings.auth_required`. Removed: `ocpp.auth_key`. |
| `/settings` REST error messages | H-4 | Added: `"URL must not point to the charger itself (loopback / 127.x / ::1)"`, `"URL must not point to link-local (169.254.x / fe80::)"`, `"URL must not contain embedded user:pass@ credentials"`. |
| HTTP error codes | Plan 16 | New 401 responses under AuthMode=1: `{"success":false,"error":"auth_required"}` / `"session_expired"`. New 403: `"csrf_origin_mismatch"`. Under AuthMode=0 (default) no new codes emitted. |

### LCD configuration

| Setting | Divergence | Detail |
|---|---|---|
| New menu items | `MENU_LEDMODE=51` (from upstream-adapt commit earlier), `MENU_AUTHMODE=52` (Plan 16). Upstream's corresponding menu slots may collide; fork uses the "> MENU_STATE" pattern established by MENU_PRIO through MENU_CAPLIMIT (46-49). | Upstream's `MENU_LEDMODE=43` shifts MENU_OFF/ON/EXIT — fork instead placed new items past MENU_STATE to avoid renumbering the 46-50 range that existing users' Modbus register map relies on. |
| LCD labels / options | Fork provides `lcdLabel` + option display for MENU_PRIO, MENU_ROTATION, MENU_IDLE_TIMEOUT, MENU_CAPLIMIT, MENU_LEDMODE via explicit switch cases in `glcd.cpp`. `MENU_AUTHMODE` label/option not yet added — Phase 2 follow-up. | Users setting AuthMode today must use Web UI, MQTT `/Set/AuthMode`, or REST `POST /settings?auth_mode=1`. LCD display shows blank for this item until Phase 2 lands. |

### REST / MQTT / OCPP ingestion (non-UI)

| Consumer | Impact |
|---|---|
| Home Assistant MQTT discovery | **No change.** MQTT path untouched. |
| Home Assistant REST integration | **No change under AuthMode=0 (default).** Under AuthMode=1 needs auth header (not implemented — use MQTT or keep AuthMode=0). |
| Custom dashboards / scripts polling HTTP | **No change under AuthMode=0.** Under AuthMode=1: break unless updated. |
| OCPP CSMS (outbound) | **No change.** |
| `smartevse.nl` app server | **No change** (MQTT-based). |
| EVCC | Some users poll `/settings`; same AuthMode caveat applies. |
| Modbus slaves (multi-node) | **No change.** Modbus stack untouched. |

## Configuration preservation on upgrade

**Audit question:** does any security fix cause a user's stored configuration
(WiFi, MQTT, OCPP, RFID tags, etc.) to be wiped on upgrade?

**Answer: no.** Verified per fix:

| Fix | NVS keys touched | Erases user config on upgrade? |
|---|---|---|
| C-1 unsigned-flash rejection | none | ✗ safe |
| C-2 OCPP auth_key redaction | none | ✗ safe |
| C-3 via Plan 16 (gated on AuthMode=1) | **adds** `AuthMode` (default 0) | ✗ safe — new key, existing keys unchanged |
| C-5 log redaction | none | ✗ safe |
| H-4 OCPP URL validator | none | ✗ safe. *Side effect:* a previously-saved URL containing loopback/link-local/embedded-creds stays in NVS as-is at boot; the next save-settings cycle will fail validation and the user sees a new error. No legitimate deployment should have such a URL. |
| H-5 strncpy NUL | none | ✗ safe |
| M-3 + M-4 firmware_manager | none | ✗ safe |
| Plan 16 auth layer | **adds** `AuthMode` (default 0) | ✗ safe |
| L-4 signedness | none | ✗ safe |

**Downgrade safety** (fork → upstream or older fork → newer fork): the added
`AuthMode` NVS key is ignored by builds that don't know about it. No data loss.

**New-install defaults** (fresh flash or post-`/erasesettings`):
`AuthMode=0`. User sees existing unauthenticated behavior and a nag banner in
the Web UI (once Phase 2 lands the banner; Phase 1 exposes `auth_required: false`
in `/settings` for UIs to detect).

## HTTP auth backward-compatibility strategy

User requested: "the http auth setting should be configurable / or something to
allow for backward compatibility."

Implemented as the `AuthMode` NVS setting, persisted across reboots, settable
via every input surface. Full design in
[`docs/security/plan-16-http-auth-layer.md`](plan-16-http-auth-layer.md).

### Key decisions

1. **Default AUTH_MODE_OFF on upgrade.** No existing installation breaks.
   User explicitly opts in to AUTH_MODE_REQUIRED when ready.
2. **Reuse existing LCD PIN.** No new secret to manage.
3. **30-minute idle session timeout.** Balances security with Web UI UX.
4. **CSRF via Origin header check.** Present + non-matching → 403. Absent
   (non-browser integration) → allowed. Doesn't break scripts.
5. **Gated endpoints:** all mutating + credential-exposing GET `/settings`.
   Explicit list in the design doc. WebSockets intentionally left to their
   existing gates.
6. **Pure C decision helper + 14 unit tests** — every path in the truth
   table is covered.

### Phase 2 scope (not yet shipped)

- Per-IP rate-limit on `/lcd-verify-password` (prevents 10k-PIN brute force)
- Nag banner in the Web UI when AuthMode=0 (current implementation exposes
  `auth_required: false` in `/settings`, allowing a UI banner to be wired
  client-side without further backend change)
- LCD menu label / option strings for `MENU_AUTHMODE`
- Guard preventing AuthMode=1 when `LCDPin==0` (avoids user lockout)
- Session-timeout duration configurable (default 30 min)

### Phase 3 scope (future)

- Per-client session tokens (current model is a per-server flag — fine for a
  single-user household, coarse for shared deployments)
- IPv6-aware rate-limit
- Optional HTTPS-only enforcement when AuthMode=1

## What remains open — prioritized

1. **C-4 shared TLS private key** — needs factory-process change (per-device
   keygen at first boot, cert emission with device serial in CN). Fork cannot
   fix unilaterally; coordinated upstream + fork rollout recommended.
2. **M-5 secure-boot / flash encryption** — platformio.ini + eFuse burn
   procedure. Device-lifecycle change. Factory-process.
3. **M-2 `/automated_testing` compile-guard** — verify release builds compile
   with `AUTOMATED_TESTING=0`. Quick CI assertion.
4. **M-6 OCPP/MQTT reconnect backoff** — small PR, standalone.
5. **M-7 pairing PIN via MQTT** — remove the publish. Small PR, standalone.
6. **Plan 16 Phase 2** — rate-limit + UI polish. Medium PR.

## Upstream coordination

Draft advisory landed at
[`docs/security/upstream-disclosure-draft.md`](upstream-disclosure-draft.md) (PR #147).
All findings mapped to CWEs; fork-side fix PRs linked; suggested 30-day SLA
documented. Routing decision (private GHSA, email, or public issue) is yours.

## Running-device recommendation

For a SmartEVSE currently running the fork's master on a **trusted home LAN**:
- Take no urgent action; the high-value fixes (C-1 unauthenticated RCE, C-2 OCPP key leak, C-5 log leak, H-4 OCPP SSRF, H-5 NUL bugs, M-3/M-4 firmware erase) are already deployed.

For a SmartEVSE on a **shared or untrusted LAN** (apartment, tenant, public WiFi):
1. Enable `AuthMode=1` (set a non-zero LCD PIN first, then `POST /settings?auth_mode=1` or via MQTT / LCD menu once Phase 2 lands).
2. Avoid exposing the charger's web UI beyond the LAN. No port-forward from WAN.
3. Consider an IoT-VLAN segregation if available.

For a SmartEVSE still on **upstream `dingo35` firmware**:
- Switch to the fork build to pick up the Critical/High fixes, OR
- Wait for upstream's response to the disclosure and its own patches.

## Session artifacts

All merged on `basmeerman/SmartEVSE-3.5` master:

- Security fixes: PRs #140, #141, #142, #143, #144, #146, #148
- Design + meta docs: PRs #145, #147, this summary (current PR)
- Files added: `src/http_auth.{h,c}`, `test/native/tests/test_http_auth.c`,
  `docs/security/plan-16-http-auth-layer.md`,
  `docs/security/upstream-disclosure-draft.md`,
  `docs/security/security-review-summary-2026-04.md` (this file).
- Files modified by the review: `http_handlers.cpp`, `network_common.cpp`,
  `network_common.h`, `esp32.cpp`, `esp32.h`, `main.cpp`, `main.h`,
  `firmware_manager.cpp`, `ocpp_logic.c`, `ocpp_logic.h`, `data/app.js`,
  `data/update2.html`, `Makefile`, `docs/operation.md`,
  `docs/upstream-differences.md`, `CLAUDE.md`.
