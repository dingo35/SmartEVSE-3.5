# Contributing to SmartEVSE

Thank you for your interest in contributing to SmartEVSE! This document provides guidelines for contributing to the project.

## Getting Started

1. Fork the repository on GitHub
2. Clone your fork locally
3. Create a feature branch from `master`

## Development Setup

### Prerequisites

- [PlatformIO](https://platformio.org/) for firmware builds
- GCC (C11) for native tests
- Python 3.10+ for test tooling

### Building Firmware

```bash
# ESP32 v3
pio run -e release -d SmartEVSE-3/

# CH32
pio run -e ch32 -d SmartEVSE-3/
```

### Running Tests

```bash
cd SmartEVSE-3/test/native
make clean test
```

All 424 native tests (across 19 suites) must pass before submitting a PR.

## Making Changes

### Branch Naming

- `feature/short-description` for new features
- `fix/short-description` for bug fixes
- `docs/short-description` for documentation changes

### Commit Messages

Use clear, concise commit messages:

```
Add solar mode phase switching logic

Implements automatic 3P-to-1P switching when solar surplus drops
below threshold for 10 seconds.
```

- Use imperative mood ("Add", not "Added")
- First line under 72 characters
- Add a blank line and details if needed

### Code Style

- C source files: C11 standard
- Use existing code patterns as reference
- Keep functions focused and testable
- Add SbE annotations (`@feature`, `@req`, `@scenario`, `@given`, `@when`, `@then`) to new test functions

### Testing Requirements

- Add tests for new functionality
- Ensure all existing tests pass (`make clean test`)
- Safety-critical changes (state machine, contactors, current limiting) require thorough test coverage
- Add SbE annotations to all new test functions (see below)
- After adding tests, regenerate the specification to verify traceability:
  ```bash
  cd SmartEVSE-3/test/native
  python3 scripts/extract_traceability.py --markdown test-specification.md
  ```

### Specification by Example (SbE)

Every test function must include structured comment annotations that link the test
to a feature, requirement, and scenario. This enables automated traceability
reporting — the CI pipeline generates a
[test specification](SmartEVSE-3/test/native/test-specification.md) and an HTML
traceability matrix from these annotations on every build.

```c
/*
 * @feature State Machine
 * @req REQ-SM-001
 * @scenario Normal charging cycle
 * @given Vehicle connected in state B
 * @when Pilot duty cycle allows charging
 * @then State transitions to C and contactor closes
 */
void test_normal_charge_cycle(void) { ... }
```

The CI traceability job validates that all annotated tests have requirement IDs and
uploads the reports as build artifacts. See the
[test specification](SmartEVSE-3/test/native/test-specification.md) for the full
list of 410 scenarios across 31 features.

## Reporting Findings & Proposing Improvements

Whether you've found a bug or want to propose a functional improvement, the project
follows a **specification-first** workflow: describe the expected behavior in SbE
format, write the test, then make the code change. This ensures every change is
traceable and verifiable.

### Step 1: Describe the finding in SbE format

Before writing any code, describe what you observed (bug) or what you want to achieve
(improvement) using the Given/When/Then pattern. This forces clear thinking about
preconditions, triggers, and expected outcomes.

**Bug report example** — the EVSE doesn't stop charging when mains current exceeds
the limit:

```
Feature:  Error Handling & Safety
Req:      REQ-ERR-030
Scenario: Charging stops when mains sum exceeds maximum

Given  The EVSE is charging in Normal mode at 16A
And    MaxSumMains is configured to 25A
When   The mains meter reports L1=15A, L2=8A, L3=10A (sum=33A)
Then   The charging current is reduced or paused
And    An error condition is flagged
```

**Feature request example** — add a grace period before solar mode stops charging:

```
Feature:  Solar Balancing
Req:      REQ-SOL-025
Scenario: Solar stop timer provides grace period before stopping

Given  The EVSE is charging in Solar mode
And    The solar stop timer is set to 10 minutes
When   Grid import exceeds solar_max_import
Then   A countdown timer starts
And    Charging continues during the countdown
And    Charging stops only after the timer expires
```

Tips for writing good specifications:

- **Be specific with values** — use concrete numbers (16A, 25A, 10 minutes), not
  vague terms ("high current", "a while")
- **One behavior per scenario** — if you need "And" in your "When", consider
  splitting into two scenarios
- **Cover the negative case too** — if something should happen at a threshold,
  also specify what happens below that threshold
- **Use existing feature names** — check the
  [test specification](SmartEVSE-3/test/native/test-specification.md) for the list
  of established feature names and requirement ID prefixes

### Step 2: Choose a requirement ID

Requirement IDs follow the pattern `REQ-{AREA}-{NUMBER}`. Check existing IDs in the
[test specification](SmartEVSE-3/test/native/test-specification.md) and pick the
next available number for your area:

| Prefix | Area |
|--------|------|
| `REQ-SM-` | State machine transitions |
| `REQ-ERR-` | Error handling & safety |
| `REQ-LB-` | Load balancing |
| `REQ-SOL-` | Solar mode / solar balancing |
| `REQ-OCPP-` | OCPP integration |
| `REQ-MQTT-` | MQTT command parsing |
| `REQ-API-` | HTTP REST API |
| `REQ-AUTH-` | Authorization & access control |
| `REQ-MOD-` | Modem / ISO15118 |
| `REQ-PH-` | Phase switching |
| `REQ-MTR-` | Metering |
| `REQ-PWR-` | Power availability |
| `REQ-E2E-` | End-to-end charging flows |
| `REQ-DUAL-` | Dual-EVSE scenarios |
| `REQ-MULTI-` | Multi-node load balancing |

### Step 3: Write the test BEFORE changing code

Create or extend a test file in `SmartEVSE-3/test/native/tests/`. The test should
**fail** initially (or not compile if the feature doesn't exist yet) — this confirms
the test actually validates something.

```c
/*
 * @feature Error Handling & Safety
 * @req REQ-ERR-030
 * @scenario Charging stops when mains sum exceeds maximum
 * @given The EVSE is charging in Normal mode at 16A
 * @given MaxSumMains is configured to 25A
 * @when The mains meter reports L1=15A, L2=8A, L3=10A (sum=33A)
 * @then The charging current is reduced or paused
 */
void test_mains_overcurrent_triggers_reduction(void) {
    // Arrange
    evse_state_machine_ctx_t ctx;
    evse_state_machine_init(&ctx);
    ctx.mode = MODE_NORMAL;
    ctx.state = STATE_C;            // charging
    ctx.charge_current = 160;       // 16.0A
    ctx.max_sum_mains = 250;        // 25.0A

    // Act — simulate meter reading that exceeds limit
    ctx.mains_currents[0] = 150;    // L1 = 15.0A
    ctx.mains_currents[1] = 80;     // L2 = 8.0A
    ctx.mains_currents[2] = 100;    // L3 = 10.0A
    evse_state_machine_run(&ctx);

    // Assert
    assert(ctx.charge_current < 160 || ctx.state != STATE_C);
}
```

Run the test to confirm it fails or validates the fix:

```bash
cd SmartEVSE-3/test/native
make clean test
```

### Step 4: Make the code change

Now implement the fix or feature. The test you wrote in Step 3 tells you exactly
when you're done — it passes.

### Step 5: Regenerate the specification and verify

```bash
cd SmartEVSE-3/test/native
python3 scripts/extract_traceability.py --markdown test-specification.md --html traceability-report.html
```

Check that your new scenario appears in the test specification under the correct
feature, with the requirement ID you chose.

### Step 6: Submit the PR

Your PR should include:
1. The test file(s) with SbE annotations
2. The code change
3. The updated `test-specification.md` (CI will also regenerate this automatically
   on merge to master)

The CI pipeline will verify that all tests pass, generate fresh traceability reports,
and commit the updated test specification back to the repository.

### Workflow summary

```
Finding/Idea → SbE description → Write failing test → Fix code → Test passes → PR
```

This is the opposite of the traditional "fix first, test maybe later" approach. By
writing the specification and test first, you:

- Force yourself to clearly define the expected behavior
- Create a permanent, executable record of the requirement
- Enable automated traceability from requirement to test to code
- Make it easy for reviewers to understand what the change does and why

## Versioning

This project uses [semantic versioning](https://semver.org/): `vMAJOR.MINOR.PATCH[-prerelease]`

### Bump Rules

- **Patch** (`v3.11.0` → `v3.11.1`): bug fixes, documentation, test-only changes
- **Minor** (`v3.11.1` → `v3.12.0`): new features, non-breaking enhancements
- **Major** (`v3.12.0` → `v4.0.0`): breaking changes (config format, API, protocol)

### Development Builds

The VERSION in `platformio.ini` carries a `-dev` suffix (e.g. `v3.11.0-dev`) between
releases. Local builds display this version on the LCD and web interface.

### Release Flow

1. Update VERSION in `SmartEVSE-3/platformio.ini` — remove the `-dev` suffix
   (e.g. `v3.11.0-dev` → `v3.11.0`)
2. Commit: `Release v3.11.0`
3. Tag: `git tag v3.11.0`
4. Push: `git push origin master --tags`
5. CI validates the tag matches `platformio.ini` and builds the release
6. Bump VERSION for next development cycle (e.g. `v3.11.1-dev`), commit, push

## Submitting Changes

1. Push your branch to your fork
2. Open a Pull Request against `master`
3. Describe what your changes do and why
4. Reference any related issues
5. Ensure CI checks pass

## Reporting Bugs

Use the [bug report template](https://github.com/dingo35/SmartEVSE-3.5/issues/new?template=bug_report.md) and include:

- Your SmartEVSE hardware version (v3 or v4)
- Firmware version
- Configuration JSON (from the webserver "raw" button)
- Debug log capturing the issue

## AI-Assisted Development

This project supports contributions made with AI coding agents. Configuration files
are provided for both Claude Code and GitHub Copilot to ensure agents follow project
standards automatically.

### Setup

| Agent | Configuration file | How to use |
|-------|-------------------|------------|
| Claude Code | [`CLAUDE.md`](CLAUDE.md) | Auto-loaded when Claude Code opens the repo |
| GitHub Copilot | [`.github/copilot-instructions.md`](.github/copilot-instructions.md) | Auto-loaded by Copilot Chat and Copilot Workspace |

Both files encode the same rules: coding conventions, architectural principles,
test-first workflow, memory budgets, and safety constraints. The agent-specific
files format these rules in the way each tool consumes them best.

### Quality Guardian Pattern

When using multiple AI agents simultaneously (e.g., Claude Code's parallel Task
agents, or Copilot Workspace with multiple agents), designate one agent as the
**Quality Guardian**:

- The Quality Guardian **does not write implementation code**
- It reviews every change from other agents for compliance with:
  - Naming conventions (`snake_case` functions, `CamelCase` globals)
  - SbE annotations on all test functions
  - No `sprintf`, no heap allocation in critical sections
  - State machine changes paired with corresponding tests
  - `extern "C"` guards on headers shared between C and C++
- It runs the test suite and firmware builds after each agent completes
- It regenerates the test specification and verifies traceability
- It has veto authority — non-compliant code must be fixed before merging

### Single Agent Workflow

When working with a single AI agent, it must self-enforce the Quality Guardian
checklist before marking work complete:

1. All tests pass (`make clean test`)
2. Firmware compiles (`pio run -e release`, `pio run -e ch32`)
3. New tests have SbE annotations with valid requirement IDs
4. No coding standard violations in changed files
5. Test specification regenerated if tests were added

### What to Watch For

AI agents may:
- Generate `sprintf` instead of `snprintf` — always reject this
- Skip writing tests for "simple" changes — always require tests
- Add unnecessary abstractions or over-engineer — keep changes minimal
- Use incorrect naming conventions — check against `CODING_STANDARDS.md`
- Modify files outside their assigned scope — enforce file boundaries

## Security Vulnerabilities

Please report security vulnerabilities privately. See [SECURITY.md](SECURITY.md) for details.

## License

By contributing, you agree that your contributions will be licensed under the [MIT License](LICENSE).
