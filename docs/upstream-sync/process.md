# Upstream Integration Process

This document defines the workflow for analyzing and integrating upstream commits
from `dingo35/SmartEVSE-3.5` (and other upstreams) into the `basmeerman` fork.

## Why not cherry-pick?

The fork has restructured the codebase significantly:

- State machine extracted from `main.cpp` → `evse_state_machine.c` (pure C)
- ~70 scattered globals → `evse_ctx_t` context struct
- Direct hardware calls → HAL callbacks via `evse_bridge.cpp`
- New pure C modules: `mqtt_parser.c`, `http_api.c`, `ocpp_logic.c`, etc.
- Native test suite (50+ suites, 1,096+ tests)

Cherry-picks almost never apply cleanly. Each upstream commit must be **understood
and reimplemented** in the fork's architecture, with tests.

---

## Flow Overview

```
1. DISCOVER   → What's new upstream?
2. TRIAGE     → Relevant? Already done? Conflicting?
3. ANALYZE    → What does it change? Side effects? Fork mapping?
4. IMPLEMENT  → Reimplement in fork architecture (SbE workflow)
5. VERIFY     → Full quality pipeline (5-step verification)
6. MERGE      → PR + CI + merge
7. DOCUMENT   → Update sync state + upstream-differences.md
```

---

## Agent Roles

| Role | Responsibility | Skills needed |
|------|---------------|---------------|
| **Sync Scout** | Fetch upstream, list new commits, produce triage table | Git, GitHub API |
| **Commit Analyst** | Read each commit, classify, map to fork files, assess risk | Deep codebase knowledge |
| **Implementer** | Reimplement following SbE workflow (spec → test → code) | Specialist role per CLAUDE.md |
| **Quality Guardian** | Review, run full verification, approve merge | Testing, builds, standards |

Agents may combine roles. A single agent can Scout + Analyze. The Implementer
should use the appropriate Specialist Role from CLAUDE.md (State Machine, Solar,
Load Balancing, OCPP, etc.) based on the commit's domain.

---

## Step 1: DISCOVER

**Agent: Sync Scout**

```bash
# Fetch latest upstream
git fetch origin

# Find the last sync point
cat docs/upstream-sync/sync-state.md | grep "Last synced"

# List unintegrated commits
git log --oneline <last-sync-hash>..origin/master
```

**Output:** Update the triage table in `sync-state.md`.

---

## Step 2: TRIAGE

**Agent: Commit Analyst**

For each new commit, classify into one of:

| Classification | Action | Priority |
|---|---|---|
| **Already done** | Skip — note which fork PR covers it | — |
| **Docs / cosmetic** | Low priority, batch or skip | P4 |
| **Bug fix — safety-critical** | Reimplement with tests immediately | P1 |
| **Bug fix — non-safety** | Reimplement, normal priority | P2 |
| **New feature — wanted** | Evaluate scope, schedule | P2-P3 |
| **New feature — not wanted** | Skip, document why | — |
| **Conflicts with fork** | Analyze carefully, may need alternative approach | P2 |

**Decision criteria:**

1. Does this fix a bug our users hit? → High priority
2. Does this touch code we've extracted to pure C? → Needs careful reimplementation
3. Does this conflict with our improvements? → May need alternative approach
4. Is this a feature we want? → Evaluate scope vs benefit
5. Does upstream have tests? → If not, we add them (SbE requirement)

---

## Step 3: ANALYZE

**Agent: Commit Analyst**

For each commit to integrate, produce an analysis in `docs/upstream-sync/commits/`:

```markdown
## Upstream Commit: <hash> — <title>

### Summary
<1-2 sentence description of what it does and why>

### Upstream changes
- **Files:** <list>
- **Key diff:** <essential code changes, abbreviated>

### Fork mapping
| Upstream file:line | Fork file:line | Notes |
|---|---|---|
| main.cpp:1234 | evse_state_machine.c:567 | State machine logic |
| esp32.cpp:890 | esp32.cpp:890 | Glue code, same file |

### Risk assessment
- [ ] Safety-critical (state machine, contactors, current limiting)
- [ ] Touches extracted pure C modules
- [ ] Touches load balancing / multi-node
- [ ] Touches OCPP
- [ ] Requires new tests (no upstream test coverage)

### Side effects
<Interactions with fork-specific features: capacity tariff, multi-key validation,
solar stability improvements, etc.>

### Implementation approach
<How to reimplement in fork architecture. Which Specialist Role applies.>
```

---

## Step 4: IMPLEMENT

**Agent: Implementer** (assigned Specialist Role per CLAUDE.md)

Follow the project's SbE workflow strictly:

1. **Read** — CLAUDE.md, CONTRIBUTING.md, relevant docs
2. **Branch** — `upstream/<hash-short>-<description>`
3. **Spec** — Write Given/When/Then scenarios
4. **Test** — Create/extend test in `test/native/tests/`
5. **Code** — Implement until tests pass
6. **Verify** — Full 5-step verification:
   ```bash
   # 1. Native tests
   cd SmartEVSE-3/test/native && make clean test
   # 2. Sanitizers
   make clean test CFLAGS_EXTRA="-fsanitize=address,undefined -fno-omit-frame-pointer"
   # 3. Static analysis (cppcheck — full command from CLAUDE.md)
   # 4. ESP32 build
   pio run -e release -d SmartEVSE-3/
   # 5. CH32 build
   pio run -e ch32 -d SmartEVSE-3/
   ```

**Commit message format:**
```
upstream: <short description>

Integrates upstream commit <full-hash> ("<upstream title>").

<What was changed and why, in fork architecture terms>

Upstream-Commit: <full-hash>
Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
```

**Grouping:** Related upstream commits (e.g., two OCPP fixes in the same area) may
be implemented in a single PR. Note all upstream commits in the commit message.

---

## Step 5: VERIFY

**Agent: Quality Guardian**

- All 5 verification steps pass
- Regression tests for the affected area
- No flash/RAM budget violations
- `upstream-differences.md` updated if a difference narrowed or widened

---

## Step 6: MERGE

Standard PR flow → CI green → merge → delete branch.

---

## Step 7: DOCUMENT

Update `docs/upstream-sync/sync-state.md` with the new sync point and integration results.

If the commit narrows an upstream difference (fork adopted upstream's approach) or
widens one (fork took a different approach), update `docs/upstream-differences.md`.

---

## Multiple Upstreams

The same flow works for any upstream. Add remotes:

```bash
git remote add stevens https://github.com/<user>/SmartEVSE-3.5.git
git fetch stevens
```

The Sync Scout discovers from each remote independently. Triage deduplicates —
if two upstreams have the same fix, only integrate once and note both sources.

---

## Automation

| Task | Automatable? | Method |
|---|---|---|
| Discover new commits | Yes | `git fetch` + `git log` |
| Triage (already done?) | Partially | Match commit messages against fork PRs |
| Full analysis | No | Requires understanding intent |
| Implementation | Partially | Agent drafts, human reviews safety-critical |
| Verification | Yes | CI pipeline |
| Sync state tracking | Yes | Markdown file updates |

A scheduled agent (e.g., weekly) can run Step 1 and produce the triage table
for human review before proceeding.
