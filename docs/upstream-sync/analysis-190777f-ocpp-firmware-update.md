# Analysis: Upstream Commit 190777f — OCPP Firmware Update

## Question

Does upstream commit `190777f` ("Add OCPP firmware update functionality") need
adaptation to work with the fork's multi-key signature validation (PR #125)?

## TL;DR

**No adaptation needed for the validation path.** Multi-key compatibility is
transparent — the upstream commit calls `forceUpdate(fwUrl, /*validate=*/true)`,
which the fork already routes through `validate_sig()`, which PR #125 made into
a multi-key loop.

There ARE other concerns (memory budget, concurrency, `shouldReboot` reuse)
that should be addressed if/when we integrate. **Recommendation: P3 — adopt as
a separate PR with on-device verification and a memory-budget review.**

## What 190777f does

Adds an OCPP-triggered firmware update path. When the CSMS sends an
`UpdateFirmware.req` OCPP message, the charger downloads the URL provided by
the CSMS, validates the signature, and reboots into the new image.

Implementation (60 lines in `esp32.cpp`, 2 lines in `network_common.h`):

1. Includes `<MicroOcpp/Model/FirmwareManagement/FirmwareService.h>`.
2. In `ocppInit()`, after the existing OCPP setup, registers four MicroOcpp
   callbacks via `getFirmwareService()`:
   - `setOnDownload(location)` — spawned task: `forceUpdate(url, true)`
   - `setDownloadStatusInput()` — returns Downloaded / DownloadFailed / NotDownloaded
   - `setOnInstall(location)` — sets `shouldReboot = true`
   - `setInstallationStatusInput()` — returns Installed if `shouldReboot`, else NotInstalled
3. Uses a `static volatile int OcppFwStatus` to communicate between the
   download FreeRTOS task and the OCPP status callback.
4. Declares `forceUpdate()` and `downloadProgress` as externs in
   `network_common.h`.

## Compatibility audit against the fork

| Requirement | Upstream uses | Fork has? | Notes |
|---|---|---|---|
| `forceUpdate(const char*, bool)` | yes | **yes** at `firmware_manager.cpp:243` | extracted from upstream as part of fork's modularization |
| `validate=true` triggers signature check | yes | **yes** at `firmware_manager.cpp:380` (`validate_sig()`) | identical call site |
| Signature validation accepts upstream-signed FW | required | **yes** | trusted_keys[0] is the upstream public key |
| Signature validation accepts basmeerman-signed FW | not required | **bonus** | trusted_keys[1] is the fork's public key — fork-built firmware can be pushed via OCPP too |
| `downloadProgress` global | yes | **yes** at `network_common.cpp:342` | identical semantics |
| `shouldReboot` global | yes | **yes** at `network_common.cpp:323` | identical semantics |
| MicroOcpp `getFirmwareService()` API | yes | **yes** at `MicroOcpp.h:356` | library version supports it |
| MicroOcpp `setOnDownload` / `setOnInstall` / `setDownloadStatusInput` / `setInstallationStatusInput` | yes | **yes** at `FirmwareService.h:34` (class includes the setters) | library version supports it |

**Validation path walkthrough:**

```
CSMS → UpdateFirmware.req → MicroOcpp FirmwareService
  → onDownload(location) callback (190777f)
    → strdup(url) + xTaskCreate
      → forceUpdate(url, /*validate=*/true)        ← firmware_manager.cpp:243
        → HTTP GET, malloc(SIGNATURE_LENGTH), readBytes(signature)
        → Update.writeStream(*stream)
        → validate_sig(_target_partition, signature, updateSize)
          → Phase 1: hash partition (SPI flash reads)
          → Phase 2: for each trusted_keys[k] in trusted_keys[]:    ← PR #125
              parse → mbedtls_pk_verify(hash, signature)
              if ret==0: verified=true
          → return verified
        → if verified: esp_ota_set_boot_partition(_target_partition)
```

The multi-key loop is invisible to 190777f — any signature accepted by any
trusted key is accepted by `validate_sig()`. Upstream-signed and fork-signed
firmware both work via OCPP push.

## Concerns NOT related to multi-key

These are not blockers but should be addressed before merging an adapted
version of 190777f. None of them touch the signature validation.

### 1. Stack-size budget (CLAUDE.md HARD RULE)

> Do not add FreeRTOS task creation or stack allocation without checking the
> memory budget and documenting stack size rationale.

Upstream creates a 4096-byte task (`xTaskCreate("OcppFwUpdate", 4096, ...)`).
4096 bytes is plausible for an HTTPS download path that uses TLS, but the
fork's web-UI firmware update task (in `network_common.cpp` near the
`FirmwareUpdate` function) should be checked for the same stack size as a
sanity reference. **Action:** confirm stack size matches the existing OTA
update task and document the rationale in the integration commit.

### 2. `OcppFwStatus` race

```c
static volatile int OcppFwStatus; // 0=idle, 1=downloading, 2=downloaded ok, -1=download failed
```

Written from the download task, read from the OCPP loop task. `volatile int`
on a 32-bit aligned `int` is atomic on ESP32 (Xtensa) in practice, but the
fork's coding standards prefer explicit synchronization for cross-task state.
**Action:** either keep as-is and document the rationale, or wrap with the
existing `evse_bridge` spinlock for consistency.

### 3. `shouldReboot` is shared with other paths

The upstream code reads `shouldReboot` as the "installation done" signal:

```c
fwService->setInstallationStatusInput([] () -> MicroOcpp::InstallationStatus {
    if (shouldReboot) {
        return MicroOcpp::InstallationStatus::Installed;
    }
    return MicroOcpp::InstallationStatus::NotInstalled;
});
```

But `shouldReboot` is also set by web-UI update completion, settings save,
factory reset, etc. (see `network_common.cpp:1488, 1519, 1667, 1733`). If a
user triggers a web-UI update while an OCPP install is in flight, the OCPP
status callback could report `Installed` for the OCPP transaction even though
the OCPP install never actually ran. Edge case but worth noting.

**Action:** consider a dedicated `OcppFwInstalled` flag set only by the OCPP
install handler.

### 4. Concurrent update guard

Upstream guards against concurrent updates with:

```c
if (downloadProgress > 0) {
    _LOG_A("OCPP FW: rejected, another update is in progress\n");
    return false;
}
```

This races against the web-UI update path that also writes `downloadProgress`.
Acceptable for a basic guard but not airtight. **Action:** verify the web-UI
update does the same check before starting.

### 5. Pure C extraction (fork philosophy)

Per CLAUDE.md, the fork prefers extracting decision logic to pure C for unit
testing. The 60 lines in 190777f are mostly MicroOcpp callback registration
and FreeRTOS task creation — there isn't much pure decision logic to extract.
The "should we accept this update request" check is one boolean. Probably not
worth a pure C extraction. **Action:** add a small unit test for the
"reject if downloadProgress > 0" guard if a pure helper is introduced.

## Testability constraints

Unlike `ocpp_silence_decide()` and `ocpp_should_force_lock()`, this commit is
mostly side effects (HTTP, MicroOcpp callbacks, FreeRTOS tasks, partition I/O).
Native unit testing is limited to:

- The "concurrent update rejection" guard (1 test)
- The status mapping `OcppFwStatus → MicroOcpp::DownloadStatus` (3 tests)

End-to-end verification requires:

- A real or mock CSMS that supports the OCPP 1.6 `UpdateFirmware.req` message
  → could use the fork's existing OCPP compatibility test infrastructure
  (Plan 11 — `test/ocpp/`)
- A signed firmware image hosted somewhere reachable by the charger
- A second charger or rollback path in case the update fails

## Recommendation

**Adopt as P3, separate PR.**

- The validation path is fully compatible with the fork's multi-key design.
- The remaining concerns are quality polishes, not blockers.
- It's a useful feature for fleets managed via OCPP CSMS.
- It does not conflict with any other pending integration work.

**Pre-integration checklist:**

1. [ ] Confirm `xTaskCreate` stack size matches existing OTA update task
2. [ ] Decide on synchronization for `OcppFwStatus` (keep volatile or wrap)
3. [ ] Decide on `shouldReboot` reuse vs dedicated OCPP install flag
4. [ ] Add small unit tests for the guard and status mapping (if extracted)
5. [ ] Add an OCPP compatibility test that exercises `UpdateFirmware.req`
      end-to-end against the mock CSMS in `test/ocpp/`
6. [ ] On-device test: push fork-signed firmware via OCPP, observe accept
7. [ ] On-device test: push upstream-signed firmware via OCPP, observe accept
8. [ ] On-device test: push unsigned firmware via OCPP, observe reject
9. [ ] On-device test: push corrupted-signature firmware via OCPP, observe reject
10. [ ] Memory budget check after integration (`pio run -e release`)

## Decision

**Defer to a future PR.** No code changes needed in the current OCPP
resilience integration (PR #130). Mark as P3 in `sync-state.md` — needs
separate cycle with on-device verification.
