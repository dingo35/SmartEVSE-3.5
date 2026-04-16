# Contributor guide

You want to add a feature, fix a bug, or port a change from another
branch. This guide is the architectural + workflow on-ramp; the
reference material lives in
[CONTRIBUTING.md](../CONTRIBUTING.md),
[CODING_STANDARDS.md](../CODING_STANDARDS.md),
[quality.md](quality.md), and [CLAUDE.md](../CLAUDE.md).

---

## 1. Repository layout

```
SmartEVSE-3/
  src/                       # Firmware source
    evse_state_machine.c     # Core charge-state machine — pure C, no platform
    evse_ctx.h               # State context struct
    evse_bridge.cpp          # Sync firmware globals <-> context (spinlock)
    mqtt_parser.c            # Pure-C MQTT command parsing
    http_api.c               # Pure-C HTTP request validation
    http_auth.c              # Pure-C auth decision (AuthMode + CSRF + timeout)
    pin_rate_limit.c         # Pure-C PIN-verify rate limiter
    ocpp_logic.c             # Pure-C OCPP URL + idTag validation
    firmware_manager.cpp     # OTA update logic
    esp32.cpp                # Firmware glue / callbacks / hardware I/O
    network_common.cpp       # WiFi + MQTT client + HTTP server (Mongoose)
    ...

  test/native/               # Native host tests (runs with plain gcc)
    tests/                   # One test_*.c per module
    include/test_framework.h # Minimal Unity-style framework
    Makefile                 # Builds each suite independently

  data/                      # Web UI source (app.js, index.html, ...)
  platformio.ini             # ESP32 / CH32 build environments

docs/                         # Reference + guides (you are here)
```

### Architectural principles

1. **Pure-C modules for testability.** State machine, parsers,
   validators compile natively with `gcc` — no Arduino, ESP-IDF, or
   FreeRTOS headers. Every non-trivial logic change is implemented here
   and tested natively before the firmware glue ever sees it.

2. **Thin glue layer.** `esp32.cpp` and `network_common.cpp` are
   dispatchers. They convert Arduino types to C types and call pure
   modules. Keep them boring.

3. **Snapshot structs.** Pure functions operate on struct snapshots
   (`evse_ctx_t`, `http_settings_request_t`, `mqtt_command_t`). Never
   on live globals. The bridge layer does the synchronisation.

4. **Bridge pattern.** `evse_bridge.cpp` syncs globals to/from
   `evse_ctx_t` inside spinlock-protected critical sections. Never
   access `evse_ctx_t` fields from outside the bridge without explicit
   synchronisation.

5. **Platform guards only in glue.** `#ifdef SMARTEVSE_VERSION` (30 for
   v3, 40 for v4 ESP32-S3) belongs in the bridge and glue layers, never
   in the state machine or parser modules.

Full writeup: [quality.md](quality.md).

---

## 2. Hard rules

Read before touching code.

| Rule | Why |
|---|---|
| **Never modify `evse_state_machine.c` without adding or updating tests.** | This module controls contactors, CP pilot, current limiting. Untested changes here can damage vehicles or trip breakers. |
| **Never use `sprintf`.** Always `snprintf` with explicit buffer size. | Buffer overrun prevention. |
| **Always NUL-terminate after `strncpy`.** | `strncpy(dst, src, n)` does not terminate when src fills the buffer. Enforced rule after security finding H-5. |
| **Never log secrets.** | No MQTT passwords, OCPP auth keys, PINs in `_LOG_A` / `_LOG_I` / `Serial.print`. Short fingerprints (first 4 chars + ellipsis) are acceptable. |
| **Never allocate heap in ISRs or critical sections.** | No `malloc`, no Arduino `String`, no blocking calls inside `portENTER_CRITICAL` / `portEXIT_CRITICAL`. |
| **Never bypass safety checks.** | Don't remove or weaken overcurrent, CP diode, or state-machine guards. |

Full list in [CLAUDE.md](../CLAUDE.md) — "Critical Rules" section.

---

## 3. The test-first workflow

Every non-trivial change follows this sequence. Skipping steps is how
regressions land.

### 1. Write the specification

Given / When / Then, before any code. Captured as a block comment on
each test function, using requirement IDs from
[test-specification.md](../SmartEVSE-3/test/native/test-specification.md).

```c
/*
 * @feature PIN rate limit
 * @req REQ-AUTH-021
 * @scenario Third failure arms a 10-second cooldown
 * @given Two failures already recorded
 * @when A third failure is recorded and check is called immediately
 * @then check returns DENY_COOLDOWN and retry_after ~= 10 s
 */
void test_pin_rl_third_failure_10s_cooldown(void) { ... }
```

### 2. Write the test

In `test/native/tests/test_<module>.c`. Should fail at first.

### 3. Implement

Just enough to make the test pass. Don't add features beyond the test.

### 4. Verify

```bash
cd SmartEVSE-3/test/native
make clean test
```

All suites must pass. Not "the one I changed". All.

### 5. Sanitizers

```bash
make clean test CFLAGS_EXTRA="-fsanitize=address,undefined -fno-omit-frame-pointer"
```

Catches buffer overruns, use-after-free, undefined behavior.

### 6. Static analysis

```bash
cppcheck --enable=warning,style,performance \
  --error-exitcode=1 --suppress=missingIncludeSystem \
  --inline-suppr \
  -I SmartEVSE-3/src -I SmartEVSE-3/test/native/include \
  SmartEVSE-3/src/evse_state_machine.c \
  ...                                   # full list in CLAUDE.md
```

Must be clean.

### 7. Firmware builds

```bash
pio run -e release -d SmartEVSE-3/      # ESP32 v3
pio run -e debug   -d SmartEVSE-3/      # ESP32 v3 debug
pio run -e v4      -d SmartEVSE-3/      # ESP32-S3 v4
pio run -e ch32    -d SmartEVSE-3/      # CH32 co-processor
```

All environments must build. Type errors, missing symbols, Arduino API
misuse — these only surface in the platform builds.

### 8. Push + PR

Branch name `fix/<topic>` or `feat/<topic>`. PR description: problem,
approach, tests added, verification commands run. Keep PRs focused —
one change per PR.

---

## 4. Specialist roles

Changes are easier to review when the author has context on the area.
`CLAUDE.md` enumerates specialist roles with file ownership and domain
knowledge. Quick map:

| Area | Owns | Req prefix |
|---|---|---|
| State machine | `evse_state_machine.c`, `evse_ctx.h`, `evse_bridge.cpp` | REQ-SM-, REQ-ERR- |
| Load balancing | Multi-node logic in `main.cpp` (extraction candidate) | REQ-LB-, REQ-MULTI-, REQ-PWR- |
| Solar / smart mode | Mode logic in `main.cpp` + state machine | REQ-SOL-, REQ-PH- |
| OCPP | `ocpp_logic.c` + OCPP library integration | REQ-OCPP- |
| Network / MQTT / HTTP | `network_common.cpp`, `mqtt_parser.c`, `http_api.c` | REQ-MQTT-, REQ-API- |
| Modbus / metering | `modbus.cpp`, meter code | REQ-MTR- |
| Web UI | `data/` → `packed_fs.c` | REQ-API- |
| Test infrastructure | `test/native/` | — |

If a PR crosses roles, call out which ones in the description. Reviewers
pick up faster.

---

## 5. Testing philosophy — SbE

Specification-by-Example. Every test function has a block comment with
`@feature`, `@req`, `@scenario`, `@given`, `@when`, `@then`. A
traceability script walks these and produces
[test-specification.md](../SmartEVSE-3/test/native/test-specification.md)
plus an HTML matrix. CI enforces 100% requirement coverage.

Regenerate after adding tests:

```bash
cd SmartEVSE-3/test/native
python3 scripts/extract_traceability.py \
  --markdown test-specification.md \
  --html traceability-report.html
```

CI runs this automatically on merge. Locally, regenerate before asking
for review so the matrix is current.

---

## 6. AI-agent contributors

This codebase was refactored using multi-agent AI-assisted development.
That continues to be a supported contribution mode. If you're running
Claude Code or similar:

- Read [CLAUDE.md](../CLAUDE.md) in full before starting. It contains
  the hard rules, file-ownership boundaries, and deviation protocol.
- Assign a specialist role per agent based on the file scope of the
  task.
- The Quality Guardian pattern (one agent reviewing, others
  implementing) is documented in CLAUDE.md — use it on multi-agent
  workflows.
- AI-authored commit messages should include the
  `Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>`
  trailer. Reviewers need to see it clearly labelled.

Human contributors: nothing changes for you. Write code, write tests,
open PRs. The agent workflow is additive, not mandatory.

---

## 7. Memory budget

| Target | Flash budget | RAM budget | Current |
|---|---|---|---|
| ESP32 v3 | 95% (1,680 KB) | 90% (288 KB) | ~86% / ~24% |
| ESP32 v4 (S3) | 95% | 90% | ~84% |
| CH32 | 95% (62 KB) | 90% (18 KB) | ~59% / ~19% |

Before merging, verify your PR doesn't push past the budget. The
release build log prints flash and RAM usage at the end. CI enforces.

For features that would push past budget, consider moving them behind
a compile-time flag or splitting across builds.

---

## 8. Common contribution patterns

### Fixing a bug reported in a GitHub issue

1. Reproduce locally. If you can't, ask for a telnet log in the issue.
2. Find the failing scenario. Write a test that exercises it.
3. The test should fail on `main` and pass with your fix.
4. Link the issue in the PR description.

### Porting a change from another branch

1. Identify the source commit. `git log` on the source branch.
2. Read the change. Understand what it does before copying.
3. Recreate manually. **Do not** cherry-pick without review — the
   state-machine area has architectural differences that make direct
   cherry-picks compile but break tests.
4. Add a short note to `docs/upstream-sync/` describing the port.

### Adding a feature

1. Write a short design note. Publish in `EVSE-team-planning/` or as
   an issue with the `design` label.
2. Get agreement on the design before coding.
3. Implement test-first per §3.
4. Large features: increment-per-PR. Don't merge giant branches.

### Touching the web UI

Files in `data/` are packed into `packed_fs.c` at build time by the
pre-script `SmartEVSE-3/packfs.py`. Running `pio run` regenerates it.
**Commit both the source change and the regenerated `packed_fs.c`** —
CI checks they're in sync.

### Touching native-test infrastructure

Changes to `test/native/Makefile` or `test_framework.h` affect every
test. Propose these changes as standalone PRs, not bundled with feature
work.

---

## 9. What not to do

- **Don't refactor Arduino `String` to `char[]`** in non-safety code.
  It works. Churn risks payload truncation for no benefit.
- **Don't add `#include` dependencies** from pure-C modules to
  Arduino / ESP-IDF headers. The native-test build will break.
- **Don't add FreeRTOS task creation** without documenting stack size
  rationale and checking the memory budget.
- **Don't remove test assertions.** Tests are the safety net.
- **Don't scope-creep PRs.** If you spotted an unrelated issue while
  doing a fix, open a separate PR.
- **Don't commit generated files that aren't required.** `packed_fs.c`
  is required (CI checks). Traceability artefacts are auto-generated by
  CI, don't commit them locally.

---

## 10. Where next

- [CLAUDE.md](../CLAUDE.md) — full agent / human contribution rules.
- [CONTRIBUTING.md](../CONTRIBUTING.md) — submission process, branch style, PR review.
- [CODING_STANDARDS.md](../CODING_STANDARDS.md) — naming, formatting, safety idioms.
- [quality.md](quality.md) — architecture, test pipeline, CI/CD.
- [test-specification.md](../SmartEVSE-3/test/native/test-specification.md) — current SbE spec + requirement coverage.
