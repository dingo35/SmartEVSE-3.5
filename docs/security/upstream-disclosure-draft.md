# Upstream Security Advisory — DRAFT

**Status:** DRAFT. Not filed anywhere. Owner (basmeerman) reviews and decides routing.
**Target project:** `dingo35/SmartEVSE-3.5`
**Reporter:** basmeerman (maintainer of the downstream fork)
**Findings from:** internal security review of the fork, April 2026, scope `SmartEVSE-3/src/` + `data/` + workflows.

## How to use this document

1. Review the findings below. Confirm each still applies to current upstream
   HEAD (a dingo35 commit hash is included where the audit was performed).
2. Decide routing:
    - **Private coordinated disclosure** (recommended for Critical/High) —
      email `dingo35` the content of this file, or open a
      [GitHub Security Advisory](https://docs.github.com/en/code-security/security-advisories/guidance-on-reporting-and-writing-information-about-vulnerabilities/privately-reporting-a-security-vulnerability)
      in your own fork and invite the upstream maintainer as a collaborator.
    - **Public issue** (acceptable for Medium/Low if you prefer) — file as a
      normal GitHub issue on `dingo35/SmartEVSE-3.5`.
3. Optionally reference this document's permalink once it lives in the fork's
   merged master, so upstream can pull the full list from one URL.

## One-line summaries

| ID | CWE | Severity | One-line description |
|---|---|---|---|
| C-1 | CWE-306 · CWE-494 | **Critical** | `POST /update?file=firmware.bin` accepts arbitrary unsigned firmware over unauthenticated HTTP — unauthenticated remote code execution on any LAN |
| C-2 | CWE-200 · CWE-522 | Critical | `GET /settings` (unauthenticated) returns the plaintext OCPP backend auth key to any LAN client |
| C-3 | CWE-306 · CWE-352 | Critical | `GET /erasesettings` (no auth, uses GET — CSRF-trivial) wipes all NVS preferences and reboots |
| C-4 | CWE-321 · CWE-798 | Critical | The HTTPS server private key (`data/key.pem`) is committed to the repo and packed into the firmware — every device ships the same TLS identity |
| C-5 | CWE-532 | Critical | `MQTTprivatePassword` (hash of EC private key, used against `mqtt.smartevse.nl`) is logged in plaintext on every boot via `_LOG_A` |
| H-1 | CWE-307 | High | `POST /lcd-verify-password` has no rate-limit and no lockout; a 4-digit numeric PIN is brute-forceable in seconds |
| H-2 | CWE-306 | High | `POST /rfid` (unauthenticated) invokes `CheckRFID()` as if a physical card was presented — attacker adds themself to the RFID whitelist |
| H-3 | CWE-306 | High | `POST /reboot` (unauthenticated) — trivial denial of service, interrupts paid charging sessions |
| H-4 | CWE-918 | High | `ocpp_validate_backend_url()` accepts loopback (`127.x`, `::1`, `0.0.0.0`), link-local (`169.254.x`, `fe80::`) and embedded-credentials (`wss://user:pass@host/`) — combined with unauthenticated `POST /settings?ocpp_update=1&ocpp_backend_url=...` an attacker redirects the charger's OCPP session to an attacker-controlled CSMS |
| H-5 | CWE-170 | High | `strncpy(RequiredEVCCID, src, sizeof(RequiredEVCCID))` in `esp32.cpp` at four sites leaves the buffer unterminated when source fills it; subsequent `Serial1.printf("%s", ...)` walks past the end |
| H-6 | CWE-770 | High | `POST /settings?mqtt_host=...` accepts arbitrary-length strings into Arduino `String` — multi-MB payload exhausts heap |
| H-7 | CWE-352 | High | No CSRF protection on any state-changing endpoint. Any website the user visits can fire `POST` requests at the charger's LAN IP |
| M-1 | CWE-200 | Medium | `GET /mqtt_ca_cert` (unauthenticated) returns the stored MQTT CA certificate |
| M-2 | CWE-489 | Medium | `/automated_testing` endpoint with "DANGEROUS IN PRODUCTION" warning in its own source — gated by compile-time flag `AUTOMATED_TESTING` but trivial to ship enabled |
| M-3 | CWE-226 | Medium | SHA-256 hash buffer in `validate_sig` is freed without zeroing — minor hygiene |
| M-4 | CWE-1188 | Medium | On signature-validation failure, `ESP.partitionEraseRange(..., ENCRYPTED_BLOCK_SIZE)` erases only 4 KB, leaving >4 KB of attacker bytes in flash |
| M-5 | CWE-311 | Medium | No secure-boot / flash-encryption in `platformio.ini` — NVS credentials stored in plaintext |
| M-6 | CWE-799 | Medium | No reconnect backoff on OCPP WebSocket or MQTT — failing backend triggers storm |
| M-7 | CWE-200 | Medium | Pairing PIN published via MQTT topic — if broker is on WAN or compromised, PIN is exposed |
| M-8 | CWE-200 · CWE-306 | Medium | `/diag/*` endpoints (diagnostic captures including RFID UIDs, meter readings, state timelines) are unauthenticated |

---

## Detailed findings

Each finding below follows the GitHub Security Advisory format.

### C-1 — Unauthenticated remote firmware flash

**CWE:** CWE-306 (Missing Authentication), CWE-494 (Download of Code Without
Integrity Check)

**Vulnerable code:** `SmartEVSE-3/src/network_common.cpp`, `/update` handler.

```c
} else if (mg_http_match_uri(hm, "/update")) {
    ...
    if (!memcmp(file, "firmware.bin", ...) ||
        !memcmp(file, "firmware.debug.bin", ...)) {
        if (!offset) Update.begin(...);
        Update.write((uint8_t*) hm->body.buf, hm->body.len);  // arbitrary bytes
        if (offset + hm->body.len >= size) {
            Update.end(true);                                  // commit without signature
            ESP.restart();
        }
    }
    ...
    if (!memcmp(file, "firmware.signed.bin", ...)) {          // SIGNED path OK
        ...
    }
}
```

**Impact:** Any device on the LAN (guest laptop, ambient IoT, compromised
phone, scripted attacker over a DHCP rogue) can flash arbitrary firmware
without credentials or signature. Complete device takeover — charger
behavior, RFID whitelist, OCPP impersonation, potential physical hazard
if attacker disables overcurrent guards.

**Reproducer (do NOT run against other people's chargers):**

```bash
curl -X POST 'http://<charger-ip>/update?file=firmware.bin&offset=0&size=<len>' \
     --data-binary @attacker-firmware.bin
```

**Recommended fix (applied in basmeerman fork PR #140):** remove the
`firmware.bin` / `firmware.debug.bin` acceptance branch entirely. Accept
only `*.signed.bin` (signature is validated by the existing
`firmware_manager.cpp::validate_sig()` path).

**Compatibility note:** release/nightly workflows already produce signed
images. Users who build unsigned firmware locally can flash via serial
(`pio run -t upload`), which requires physical access — a deliberately
different threat model.

---

### C-2 — OCPP auth_key returned in plaintext by unauthenticated `GET /settings`

**CWE:** CWE-200 (Information Exposure), CWE-522 (Insufficiently Protected
Credentials)

**Vulnerable code:** `SmartEVSE-3/src/network_common.cpp`, `handle_URI()`
`GET /settings` block (or equivalent; in basmeerman fork this lives in
`http_handlers.cpp:304`):

```c
doc["ocpp"]["auth_key"] = OcppWsClient->getAuthKey();
```

The `/settings` endpoint has no authentication layer.

**Impact:** Any LAN client can read the OCPP backend basic-auth key
(SP2 password for the CSMS) and impersonate the charger at the backend —
capture transactions, manipulate billing, cause Smart Charging profile
changes to affect other chargers if the backend trusts this identity.

**Recommended fix (applied in basmeerman fork PR #144):** return
`auth_key_set: bool` instead of the plaintext key (mirrors the
`mqtt.password_set` pattern already present). Update the Web UI to show a
`"••••••••"` placeholder when the key is configured and only POST a new
value when the user types one.

---

### C-3 — Unauthenticated factory reset via `GET /erasesettings`

**CWE:** CWE-306 (Missing Authentication), CWE-352 (CSRF — trivial because GET)

**Vulnerable code:** `SmartEVSE-3/src/network_common.cpp`:

```c
if (mg_match(hm->uri, mg_str("/erasesettings"), NULL)) {
    preferences.clear(); ...
    DeleteAllRFID();
    shouldReboot = true;
}
```

Uses a GET request, so any HTML `<img src=...>` / `<link href=...>` /
`<iframe src=...>` on a malicious website auto-fires this against every
charger on the user's LAN. No auth check, no AP-mode gate.

**Recommended fix (applied in basmeerman fork via Plan 16 Phase 1, PR #146):**
(1) change method to POST; (2) gate behind an opt-in auth layer (the fork's
`AuthMode` setting + LCD PIN).

---

### C-4 — Shared HTTPS server private key committed to repository

**CWE:** CWE-321 (Hardcoded Cryptographic Key), CWE-798 (Hardcoded Credentials)

**Vulnerable artifact:** `SmartEVSE-3/data/key.pem` — EC NIST P-256 private
key, issuer `C=NL, ST=Some-State, O=Stegen Electronics, OU=SmartEVSE`,
valid 2025-04-15 → 2035-04-13. Packed into the firmware binary at build.

**Impact:** Every SmartEVSE v3 shipping the public firmware uses the
**same** TLS private key. Anyone with access to the firmware binary (i.e.,
anyone) can impersonate any SmartEVSE-3.5 device's HTTPS endpoint. Any
user interacting with the charger over HTTPS is vulnerable to MITM from
any other device on the same LAN.

**Recommended fix:**
Option A (preferred): generate a unique EC keypair at first boot, store
private half in NVS, emit a self-signed cert with the device serial in CN.
Option B: drop the on-device HTTPS server entirely; rely on LAN-only
access + future reverse-proxy integration.

**Upstream coordination:** this change requires a factory-process decision
(where the per-device key material is stored) and should be discussed
before implementation.

---

### C-5 — MQTT private password logged on every boot

**CWE:** CWE-532 (Insertion of Sensitive Information into Log File)

**Vulnerable code:** `SmartEVSE-3/src/network_common.cpp` boot flow:

```c
_LOG_A("hwversion=%04x serialnr=%u mqtt_pwd=%s\n",
       hwversion, serialnr, MQTTprivatePassword.c_str());
```

`MQTTprivatePassword` is a hash of the device's EC private key, used as
the authentication secret against `mqtt.smartevse.nl`. `_LOG_A` is active
at info level, printed to Serial, and consumed by any enabled telnet /
cloud-forwarded log feed.

**Impact:** Anyone with a log snapshot can impersonate the device against
the SmartEVSE app server.

**Recommended fix (applied in basmeerman fork PR #141):** log a 4-char
prefix followed by `[redacted]`. Short enough to aid debugging ("does the
hash look right?"), opaque enough to not leak the secret.

---

### H-1 through H-7, M-1 through M-8 — see fork's security-review report

For brevity, the detailed text for each of H-1..M-8 mirrors the internal
security review. In the GitHub Security Advisory format they should each
become a separate CVE entry. The [upstream-differences.md](../upstream-differences.md)
"Security Hardening" table lists the fork's fixes with commit links that
serve as reference patches upstream can consider.

Quick fork-side fix map for upstream's reference:

| Upstream finding | Fork PR | Size |
|---|---|---|
| C-1 | [#140](https://github.com/basmeerman/SmartEVSE-3.5/pull/140) | ~25 lines (removal) |
| C-2 | [#144](https://github.com/basmeerman/SmartEVSE-3.5/pull/144) | ~10 lines |
| C-3 | [#146](https://github.com/basmeerman/SmartEVSE-3.5/pull/146) | part of Plan 16 auth layer |
| C-4 | TBD — needs factory-process coordination | larger |
| C-5 | [#141](https://github.com/basmeerman/SmartEVSE-3.5/pull/141) | 1 line |
| H-4 | [#142](https://github.com/basmeerman/SmartEVSE-3.5/pull/142) | ~100 lines incl. tests |
| H-5 | [#141](https://github.com/basmeerman/SmartEVSE-3.5/pull/141) | 4 lines |
| M-3 + M-4 | [#143](https://github.com/basmeerman/SmartEVSE-3.5/pull/143) | ~15 lines |
| C-3 + H-1 + H-2 + H-3 + H-7 + M-1 + M-8 | [#146](https://github.com/basmeerman/SmartEVSE-3.5/pull/146) | ~600 lines, opt-in auth layer |

The auth layer in PR #146 is opt-in (AuthMode setting, default OFF on upgrade)
specifically to preserve backward compatibility for existing Home Assistant /
MQTT / REST integrations. Upstream may prefer a different default or a
different mechanism entirely (e.g. session cookies, not LCD PIN) — the
fork's design and implementation are offered as one reference point.

---

## Coordination notes for basmeerman

- The fork has already **shipped** patches for C-1, C-2, C-5, H-4, H-5,
  M-3, M-4, and Plan 16 (opt-in auth) on its own master. They are not
  being held while upstream response is awaited — exposure of running
  chargers on user LANs outweighs the benefit of coordinated disclosure
  for a fork with a small install base.

- **Remaining fork-coordinated work:** C-4 (shared TLS private key) and
  M-5 (secure-boot / flash encryption) both need factory-process changes
  the fork cannot make unilaterally.

- If upstream declines or takes no action: fork can publish its security
  advisory independently citing this document as the root finding, and
  leave the advisory visible to guide users of unpatched upstream builds
  onto the fork.

- Suggested disclosure SLA: offer upstream 30 days to respond before
  publishing the fork's advisory publicly.
