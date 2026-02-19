"""
Shared fixtures and helpers for BDD step definitions.

This module builds the C test binaries once per session and provides
utility functions for running individual C test binaries and checking
the pass/fail status of specific test functions within them.

The C test framework emits lines like:
    test_function_name                                          [OK]
    test_function_name                                          [FAIL]

Binary exit code: 0 = all passed, 1 = at least one failure.
"""

import subprocess
import os
import re

import pytest

# ---------------------------------------------------------------------------
# Path constants
# ---------------------------------------------------------------------------
NATIVE_TEST_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = os.path.join(NATIVE_TEST_DIR, "build")


# ---------------------------------------------------------------------------
# Session-scoped build fixture
# ---------------------------------------------------------------------------
@pytest.fixture(scope="session", autouse=True)
def build_test_binaries():
    """Build all C test binaries before running any BDD test."""
    result = subprocess.run(
        ["make", "all"],
        cwd=NATIVE_TEST_DIR,
        capture_output=True,
        text=True,
        timeout=120,
    )
    if result.returncode != 0:
        pytest.fail(
            f"Failed to build test binaries:\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def run_native_test(binary_name: str) -> tuple[bool, str]:
    """Run a native C test binary and return (all_passed, combined_output)."""
    binary_path = os.path.join(BUILD_DIR, binary_name)
    if not os.path.isfile(binary_path):
        return False, f"Binary not found: {binary_path}"
    result = subprocess.run(
        [binary_path],
        capture_output=True,
        text=True,
        timeout=30,
    )
    output = result.stdout + result.stderr
    return result.returncode == 0, output


def check_specific_test(binary_name: str, test_function_name: str) -> bool:
    """Run a binary and return True if the named test function passed.

    Looks for a line containing *test_function_name* followed by ``[OK]``.
    Returns False if the test is marked ``[FAIL]`` or is not found at all.
    """
    success, output = run_native_test(binary_name)
    for line in output.splitlines():
        if test_function_name in line:
            return "[OK]" in line
    # Test function not found in output -- treat as failure.
    return False


def check_all_tests_in_suite(binary_name: str) -> bool:
    """Run a binary and return True only if every test in the suite passed."""
    success, _output = run_native_test(binary_name)
    return success


def get_test_results(binary_name: str) -> dict[str, bool]:
    """Run a binary and return {test_function_name: passed} for every test."""
    _success, output = run_native_test(binary_name)
    results: dict[str, bool] = {}
    for line in output.splitlines():
        # Lines look like:  "  test_function_name                       [OK]"
        match = re.match(r"\s+(test_\w+)\s+\[(OK|FAIL)\]", line)
        if match:
            results[match.group(1)] = match.group(2) == "OK"
    return results


# ---------------------------------------------------------------------------
# Shared step-definition fixtures
# ---------------------------------------------------------------------------
@pytest.fixture
def context():
    """A mutable dictionary shared across Given/When/Then steps in a scenario."""
    return {}
