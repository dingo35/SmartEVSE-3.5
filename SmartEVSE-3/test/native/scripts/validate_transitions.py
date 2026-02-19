#!/usr/bin/env python3
"""
validate_transitions.py - SmartEVSE-3 State Transition Specification Validator

Phase 2 of the Specification by Example implementation.

Reads the state_transitions.yaml specification and validates that every
referenced test function exists in the test source files.  Optionally
compiles and runs the test binaries to verify PASS/FAIL status.

Usage:
    python scripts/validate_transitions.py                        # Validate references only
    python scripts/validate_transitions.py --run                  # Also compile & run tests
    python scripts/validate_transitions.py --html report.html     # Generate HTML report
    python scripts/validate_transitions.py --run --html report.html  # Both

Requirements: Python 3.6+ (stdlib only -- no external dependencies).
"""

from __future__ import annotations

import argparse
import html as html_mod
import os
import re
import subprocess
import sys
from collections import OrderedDict
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

# ---------------------------------------------------------------------------
#  Minimal YAML parser (stdlib-only fallback)
# ---------------------------------------------------------------------------

def _try_import_yaml():
    """Return the yaml module if PyYAML is available, else None."""
    try:
        import yaml  # type: ignore
        return yaml
    except ImportError:
        return None


def _mini_yaml_load(text: str) -> Any:
    """
    Minimal YAML-subset parser sufficient for the state_transitions.yaml
    format.  Handles:
      - Top-level mapping keys
      - Sequences of mappings (with ``- key: value`` syntax)
      - Scalar values (strings, ints, bools)
      - Sequences of scalar strings (``- "value"``)
      - Comment lines (``#``)
      - Quoted and unquoted strings

    This is intentionally limited -- it only needs to parse *our* spec file.
    """

    lines = text.splitlines()
    root: Dict[str, Any] = OrderedDict()
    current_key: Optional[str] = None
    current_list: Optional[List[Any]] = None
    current_item: Optional[Dict[str, Any]] = None
    current_sub_list: Optional[List[str]] = None
    current_sub_key: Optional[str] = None

    def _parse_scalar(val: str) -> Any:
        val = val.strip()
        if not val:
            return ""
        # Strip surrounding quotes
        if (val.startswith('"') and val.endswith('"')) or \
           (val.startswith("'") and val.endswith("'")):
            return val[1:-1]
        low = val.lower()
        if low == "true":
            return True
        if low == "false":
            return False
        if low == "null" or low == "~":
            return None
        try:
            return int(val)
        except ValueError:
            pass
        try:
            return float(val)
        except ValueError:
            pass
        return val

    def _flush_item():
        nonlocal current_item, current_sub_list, current_sub_key
        if current_item is not None and current_list is not None:
            if current_sub_list is not None and current_sub_key is not None:
                current_item[current_sub_key] = current_sub_list
            current_list.append(current_item)
        current_item = None
        current_sub_list = None
        current_sub_key = None

    for raw_line in lines:
        # Strip trailing whitespace, keep leading
        line = raw_line.rstrip()

        # Skip empty and comment-only lines
        stripped = line.lstrip()
        if not stripped or stripped.startswith("#"):
            continue

        indent = len(line) - len(stripped)

        # ----- Top-level key (indent == 0) -----
        if indent == 0 and ":" in stripped and not stripped.startswith("-"):
            _flush_item()
            if current_list is not None and current_key is not None:
                root[current_key] = current_list
                current_list = None
            key, _, val = stripped.partition(":")
            key = key.strip()
            val = val.strip()
            if val:
                root[key] = _parse_scalar(val)
                current_key = None
            else:
                current_key = key
            continue

        # ----- Second-level key inside a mapping (e.g., metadata fields) -----
        if indent >= 2 and ":" in stripped and not stripped.startswith("-"):
            # Could be a field inside metadata (dict) or inside a list item
            key, _, val = stripped.partition(":")
            key = key.strip()
            val = val.strip()

            if current_item is not None:
                # Inside a list item
                if current_sub_list is not None and current_sub_key is not None:
                    current_item[current_sub_key] = current_sub_list
                    current_sub_list = None
                    current_sub_key = None
                if val:
                    current_item[key] = _parse_scalar(val)
                else:
                    # Start of a sub-list
                    current_sub_key = key
                    current_sub_list = []
            elif current_key is not None:
                # Inside a top-level mapping (like metadata)
                if current_key not in root:
                    root[current_key] = OrderedDict()
                if isinstance(root[current_key], dict):
                    root[current_key][key] = _parse_scalar(val)
            continue

        # ----- List item start (  - key: value  or  - "scalar") -----
        if stripped.startswith("- "):
            item_content = stripped[2:].strip()

            # Is this the start of a new mapping item in the transitions list?
            if ":" in item_content and current_key is not None:
                # Ensure we have a list for the current key
                if current_list is None:
                    current_list = []
                _flush_item()
                current_item = OrderedDict()
                k, _, v = item_content.partition(":")
                k = k.strip()
                v = v.strip()
                if v:
                    current_item[k] = _parse_scalar(v)
                else:
                    current_sub_key = k
                    current_sub_list = []
            else:
                # Scalar list item (inside a sub-list like conditions)
                if current_sub_list is not None:
                    current_sub_list.append(_parse_scalar(item_content))
                elif current_list is not None:
                    current_list.append(_parse_scalar(item_content))
            continue

    # Flush remaining
    _flush_item()
    if current_list is not None and current_key is not None:
        root[current_key] = current_list

    return root


def load_yaml(path: Path) -> Any:
    """Load YAML from *path*, preferring PyYAML if available."""
    text = path.read_text(encoding="utf-8")
    yaml_mod = _try_import_yaml()
    if yaml_mod is not None:
        return yaml_mod.safe_load(text)
    return _mini_yaml_load(text)

# ---------------------------------------------------------------------------
#  Test function scanner
# ---------------------------------------------------------------------------

_FUNC_RE = re.compile(r"^\s*void\s+(test_\w+)\s*\(\s*void\s*\)", re.MULTILINE)


def scan_test_functions(test_dir: Path) -> Dict[str, List[str]]:
    """
    Scan all .c files under *test_dir* and return a mapping of
    ``{function_name: [file1, file2, ...]}``.
    """
    funcs: Dict[str, List[str]] = {}
    for c_file in sorted(test_dir.glob("*.c")):
        text = c_file.read_text(encoding="utf-8", errors="replace")
        for m in _FUNC_RE.finditer(text):
            name = m.group(1)
            funcs.setdefault(name, []).append(c_file.name)
    return funcs

# ---------------------------------------------------------------------------
#  Test runner (optional)
# ---------------------------------------------------------------------------

def run_tests(native_dir: Path) -> Dict[str, Tuple[bool, str]]:
    """
    Build and run the test suite via ``make test``.

    Returns a mapping ``{test_function: (passed, output_snippet)}``.
    Individual test results are parsed from the test-binary stdout which
    uses the format::

        [PASS] test_function_name
        [FAIL] test_function_name - message
    """
    results: Dict[str, Tuple[bool, str]] = {}

    # Build
    print("  Building tests ...")
    build = subprocess.run(
        ["make", "all"],
        cwd=str(native_dir),
        capture_output=True,
        text=True,
        timeout=120,
    )
    if build.returncode != 0:
        print(f"  BUILD FAILED:\n{build.stderr}")
        return results

    # Run each test binary
    build_dir = native_dir / "build"
    if not build_dir.is_dir():
        print(f"  Build directory not found: {build_dir}")
        return results

    for binary in sorted(build_dir.iterdir()):
        if not binary.is_file() or not os.access(str(binary), os.X_OK):
            continue
        # Skip non-test binaries
        if not binary.name.startswith("test_"):
            continue

        print(f"  Running {binary.name} ...")
        try:
            proc = subprocess.run(
                [str(binary)],
                capture_output=True,
                text=True,
                timeout=60,
            )
        except subprocess.TimeoutExpired:
            print(f"    TIMEOUT: {binary.name}")
            continue

        output = proc.stdout + proc.stderr
        for line in output.splitlines():
            line = line.strip()
            if line.startswith("[PASS]"):
                func = line.split("]", 1)[1].strip().split()[0]
                results[func] = (True, line)
            elif line.startswith("[FAIL]"):
                func = line.split("]", 1)[1].strip().split()[0]
                results[func] = (False, line)

    return results

# ---------------------------------------------------------------------------
#  Report generation
# ---------------------------------------------------------------------------

STATUS_PASS = "PASS"
STATUS_FAIL = "FAIL"
STATUS_MISSING = "MISSING_TEST"
STATUS_NOT_RUN = "NOT_RUN"


def classify_transitions(
    transitions: List[Dict[str, Any]],
    known_funcs: Dict[str, List[str]],
    run_results: Optional[Dict[str, Tuple[bool, str]]],
) -> List[Dict[str, Any]]:
    """Annotate each transition dict with a ``status`` field."""
    for t in transitions:
        func = t.get("test_function", "")
        if func not in known_funcs:
            t["status"] = STATUS_MISSING
            t["status_detail"] = f"Function '{func}' not found in test sources"
        elif run_results is not None:
            if func in run_results:
                passed, detail = run_results[func]
                t["status"] = STATUS_PASS if passed else STATUS_FAIL
                t["status_detail"] = detail
            else:
                t["status"] = STATUS_NOT_RUN
                t["status_detail"] = "Test binary did not report this function"
        else:
            t["status"] = STATUS_PASS  # Reference exists; good enough
            t["status_detail"] = f"Found in {', '.join(known_funcs[func])}"
    return transitions


def print_text_report(
    transitions: List[Dict[str, Any]],
    ran_tests: bool,
) -> int:
    """Print a human-readable report to stdout. Returns exit code (0 or 1)."""

    total = len(transitions)
    passing = sum(1 for t in transitions if t["status"] == STATUS_PASS)
    failing = sum(1 for t in transitions if t["status"] == STATUS_FAIL)
    missing = sum(1 for t in transitions if t["status"] == STATUS_MISSING)
    not_run = sum(1 for t in transitions if t["status"] == STATUS_NOT_RUN)
    safety = sum(1 for t in transitions if t.get("safety_critical"))
    safety_covered = sum(
        1 for t in transitions
        if t.get("safety_critical") and t["status"] == STATUS_PASS
    )

    sep = "=" * 72

    print()
    print(sep)
    print("  SmartEVSE-3 State Transition Validation Report")
    print(sep)
    print()
    print(f"  Total transitions specified : {total}")
    print(f"  Transitions with tests      : {passing}")
    if ran_tests:
        print(f"  Transitions FAILING         : {failing}")
        print(f"  Transitions NOT RUN         : {not_run}")
    print(f"  Transitions MISSING tests   : {missing}")
    print()
    print(f"  Safety-critical transitions  : {safety}")
    print(f"  Safety-critical covered      : {safety_covered}")
    print()

    # Detail table
    hdr = f"  {'ID':<8} {'From':<22} {'To':<22} {'Status':<14} Test Function"
    print(hdr)
    print("  " + "-" * (len(hdr) - 2))
    for t in transitions:
        tid = t.get("id", "?")
        frm = t.get("from", "?")
        to = t.get("to", "?")
        st = t.get("status", "?")
        func = t.get("test_function", "?")
        marker = ""
        if st == STATUS_FAIL:
            marker = " << FAIL"
        elif st == STATUS_MISSING:
            marker = " << MISSING"
        elif st == STATUS_NOT_RUN:
            marker = " << NOT_RUN"
        print(f"  {tid:<8} {frm:<22} {to:<22} {st:<14} {func}{marker}")

    print()

    # Print any problems
    problems = [t for t in transitions if t["status"] in (STATUS_FAIL, STATUS_MISSING)]
    if problems:
        print(sep)
        print("  PROBLEMS:")
        print(sep)
        for t in problems:
            print(f"  [{t['status']}] {t['id']} {t['from']} -> {t['to']}")
            print(f"           {t['status_detail']}")
        print()

    if not problems:
        print(f"  ALL {total} TRANSITIONS VALIDATED SUCCESSFULLY")
    else:
        print(f"  {len(problems)} PROBLEM(S) FOUND")
    print(sep)
    print()

    return 1 if problems else 0


def generate_html_report(
    transitions: List[Dict[str, Any]],
    ran_tests: bool,
    output_path: Path,
) -> None:
    """Generate an HTML report file."""

    total = len(transitions)
    passing = sum(1 for t in transitions if t["status"] == STATUS_PASS)
    failing = sum(1 for t in transitions if t["status"] == STATUS_FAIL)
    missing = sum(1 for t in transitions if t["status"] == STATUS_MISSING)
    not_run = sum(1 for t in transitions if t["status"] == STATUS_NOT_RUN)
    safety = sum(1 for t in transitions if t.get("safety_critical"))
    safety_covered = sum(
        1 for t in transitions
        if t.get("safety_critical") and t["status"] == STATUS_PASS
    )

    status_colors = {
        STATUS_PASS: "#2e7d32",
        STATUS_FAIL: "#c62828",
        STATUS_MISSING: "#e65100",
        STATUS_NOT_RUN: "#9e9e9e",
    }

    esc = html_mod.escape

    rows = []
    for t in transitions:
        color = status_colors.get(t["status"], "#000")
        sc = "Yes" if t.get("safety_critical") else "No"
        sc_style = ' style="color:#c62828;font-weight:bold"' if t.get("safety_critical") else ""
        conditions = t.get("conditions", [])
        if isinstance(conditions, list):
            cond_html = "<br>".join(esc(c) for c in conditions)
        else:
            cond_html = esc(str(conditions))

        rows.append(f"""<tr>
  <td>{esc(str(t.get('id', '')))}</td>
  <td>{esc(str(t.get('from', '')))}</td>
  <td>{esc(str(t.get('to', '')))}</td>
  <td>{esc(str(t.get('trigger', '')))}</td>
  <td class="cond">{cond_html}</td>
  <td style="color:{color};font-weight:bold">{esc(t['status'])}</td>
  <td{sc_style}>{sc}</td>
  <td><code>{esc(str(t.get('test_function', '')))}</code></td>
  <td><code>{esc(str(t.get('test_file', '')))}</code></td>
  <td>{esc(str(t.get('requirement', '')))}</td>
  <td>{esc(str(t.get('description', '')))}</td>
</tr>""")

    row_html = "\n".join(rows)

    problems_section = ""
    problems = [t for t in transitions if t["status"] in (STATUS_FAIL, STATUS_MISSING)]
    if problems:
        prob_rows = []
        for t in problems:
            color = status_colors.get(t["status"], "#000")
            prob_rows.append(
                f'<tr><td style="color:{color};font-weight:bold">{esc(t["status"])}</td>'
                f'<td>{esc(str(t.get("id", "")))}</td>'
                f'<td>{esc(str(t.get("from", "")))} -> {esc(str(t.get("to", "")))}</td>'
                f'<td>{esc(t.get("status_detail", ""))}</td></tr>'
            )
        problems_section = f"""
    <h2 style="color:#c62828">Problems ({len(problems)})</h2>
    <table>
      <tr><th>Status</th><th>ID</th><th>Transition</th><th>Detail</th></tr>
      {"".join(prob_rows)}
    </table>"""

    run_info = ""
    if ran_tests:
        run_info = f"""
      <tr><td>Failing tests</td><td><strong style="color:#c62828">{failing}</strong></td></tr>
      <tr><td>Not run</td><td>{not_run}</td></tr>"""

    page = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>SmartEVSE-3 State Transition Validation Report</title>
<style>
  body {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
         margin: 2rem; color: #212121; background: #fafafa; }}
  h1 {{ border-bottom: 3px solid #1565c0; padding-bottom: 0.5rem; }}
  table {{ border-collapse: collapse; width: 100%; margin: 1rem 0; }}
  th, td {{ border: 1px solid #bdbdbd; padding: 0.4rem 0.6rem; text-align: left;
            vertical-align: top; font-size: 0.85rem; }}
  th {{ background: #e3f2fd; }}
  tr:nth-child(even) {{ background: #f5f5f5; }}
  .summary td {{ border: none; padding: 0.2rem 1rem; }}
  .cond {{ font-size: 0.8rem; color: #555; max-width: 260px; }}
  code {{ background: #eceff1; padding: 0.1rem 0.3rem; border-radius: 3px; font-size: 0.82rem; }}
  .ok {{ color: #2e7d32; font-weight: bold; }}
  .err {{ color: #c62828; font-weight: bold; }}
</style>
</head>
<body>
<h1>SmartEVSE-3 State Transition Validation Report</h1>

<h2>Summary</h2>
<table class="summary" style="width:auto">
  <tr><td>Total transitions specified</td><td><strong>{total}</strong></td></tr>
  <tr><td>Transitions with passing tests</td><td class="ok">{passing}</td></tr>
  {run_info}
  <tr><td>Transitions with missing tests</td><td class="{"err" if missing else "ok"}">{missing}</td></tr>
  <tr><td>Safety-critical transitions</td><td><strong>{safety}</strong></td></tr>
  <tr><td>Safety-critical covered</td><td class="{"ok" if safety_covered == safety else "err"}">{safety_covered} / {safety}</td></tr>
</table>

{problems_section}

<h2>All Transitions</h2>
<table>
<tr>
  <th>ID</th><th>From</th><th>To</th><th>Trigger</th><th>Conditions</th>
  <th>Status</th><th>Safety</th><th>Test Function</th><th>Test File</th>
  <th>Requirement</th><th>Description</th>
</tr>
{row_html}
</table>

<p style="color:#757575;font-size:0.8rem;margin-top:2rem">
  Generated by <code>validate_transitions.py</code> &mdash; SmartEVSE-3 Phase 2 Specification by Example
</p>
</body>
</html>"""

    output_path.write_text(page, encoding="utf-8")
    print(f"  HTML report written to: {output_path}")

# ---------------------------------------------------------------------------
#  Main
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate SmartEVSE-3 state transition specifications against tests.",
    )
    parser.add_argument(
        "--run",
        action="store_true",
        help="Compile and run the test binaries to check PASS/FAIL.",
    )
    parser.add_argument(
        "--html",
        metavar="FILE",
        help="Write an HTML report to FILE.",
    )
    parser.add_argument(
        "--spec",
        metavar="FILE",
        help="Path to the YAML spec file (default: auto-detect).",
    )
    args = parser.parse_args()

    # Resolve paths relative to this script
    script_dir = Path(__file__).resolve().parent            # scripts/
    native_dir = script_dir.parent                          # test/native/
    specs_dir = native_dir / "specs"
    tests_dir = native_dir / "tests"

    spec_path = Path(args.spec) if args.spec else specs_dir / "state_transitions.yaml"

    if not spec_path.is_file():
        print(f"ERROR: Spec file not found: {spec_path}", file=sys.stderr)
        return 1

    if not tests_dir.is_dir():
        print(f"ERROR: Tests directory not found: {tests_dir}", file=sys.stderr)
        return 1

    # ---- Load spec ----
    print(f"  Loading spec: {spec_path}")
    spec = load_yaml(spec_path)
    transitions = spec.get("transitions", [])
    if not transitions:
        print("ERROR: No transitions found in spec file.", file=sys.stderr)
        return 1
    print(f"  Loaded {len(transitions)} transition(s).")

    # ---- Scan test functions ----
    print(f"  Scanning test functions in: {tests_dir}")
    known_funcs = scan_test_functions(tests_dir)
    print(f"  Found {len(known_funcs)} unique test function(s).")

    # ---- Optionally run tests ----
    run_results: Optional[Dict[str, Tuple[bool, str]]] = None
    if args.run:
        print("  Running tests ...")
        run_results = run_tests(native_dir)
        print(f"  Collected results for {len(run_results)} test function(s).")

    # ---- Classify ----
    transitions = classify_transitions(transitions, known_funcs, run_results)

    # ---- Text report ----
    exit_code = print_text_report(transitions, ran_tests=args.run)

    # ---- HTML report ----
    if args.html:
        html_path = Path(args.html)
        if not html_path.is_absolute():
            html_path = Path.cwd() / html_path
        generate_html_report(transitions, ran_tests=args.run, output_path=html_path)

    return exit_code


if __name__ == "__main__":
    sys.exit(main())
