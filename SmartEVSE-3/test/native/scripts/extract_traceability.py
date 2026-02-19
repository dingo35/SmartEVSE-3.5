#!/usr/bin/env python3
"""
extract_traceability.py - Extract SbE annotations from C test files.

Parses @feature, @req, @scenario, @given, @when, @then structured comment
blocks from C test source files and generates:
  1. An HTML traceability matrix
  2. A Markdown specification document

Usage:
    python scripts/extract_traceability.py [--html report.html] [--markdown spec.md]
    python scripts/extract_traceability.py --html report.html --markdown spec.md

No external dependencies - uses Python stdlib only.
"""

import argparse
import glob
import html
import os
import re
import sys
from collections import OrderedDict
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class TestAnnotation:
    feature: str = ""
    req: str = ""
    scenario: str = ""
    given: list = field(default_factory=list)
    when: list = field(default_factory=list)
    then: list = field(default_factory=list)
    function_name: str = ""
    file_name: str = ""
    line_number: int = 0


def parse_annotation_block(comment_text: str) -> dict:
    """Parse a structured comment block into annotation fields."""
    annotations = {
        "feature": "",
        "req": "",
        "scenario": "",
        "given": [],
        "when": [],
        "then": [],
    }

    for line in comment_text.split("\n"):
        line = line.strip().lstrip("* ").strip()
        if not line:
            continue

        match = re.match(r"@(\w+)\s+(.*)", line)
        if match:
            tag = match.group(1).lower()
            value = match.group(2).strip()
            if tag in ("given", "when", "then"):
                annotations[tag].append(value)
            elif tag in annotations:
                annotations[tag] = value

    return annotations


def extract_annotations(file_path: str) -> list:
    """Extract all test annotations from a C source file."""
    with open(file_path, "r") as f:
        content = f.read()

    file_name = os.path.basename(file_path)
    annotations = []

    # Find all block comments followed by a test function
    pattern = re.compile(
        r"/\*\s*\n(.*?)\*/\s*\n\s*void\s+(test_\w+)\s*\(",
        re.DOTALL,
    )

    for match in pattern.finditer(content):
        comment_text = match.group(1)
        function_name = match.group(2)

        # Only process comments that contain @feature or @scenario tags
        if "@feature" not in comment_text and "@scenario" not in comment_text:
            continue

        parsed = parse_annotation_block(comment_text)

        line_number = content[: match.start()].count("\n") + 1

        annotation = TestAnnotation(
            feature=parsed["feature"],
            req=parsed["req"],
            scenario=parsed["scenario"],
            given=parsed["given"],
            when=parsed["when"],
            then=parsed["then"],
            function_name=function_name,
            file_name=file_name,
            line_number=line_number,
        )
        annotations.append(annotation)

    return annotations


def group_by_feature(annotations: list) -> OrderedDict:
    """Group annotations by feature name."""
    features = OrderedDict()
    for ann in annotations:
        feature_name = ann.feature or "Uncategorized"
        if feature_name not in features:
            features[feature_name] = []
        features[feature_name].append(ann)
    return features


def generate_html(features: OrderedDict, output_path: str):
    """Generate an HTML traceability report."""
    total_tests = sum(len(tests) for tests in features.values())
    total_with_req = sum(
        1 for tests in features.values() for t in tests if t.req
    )

    h = html.escape

    lines = []
    lines.append("<!DOCTYPE html>")
    lines.append('<html lang="en">')
    lines.append("<head>")
    lines.append('<meta charset="UTF-8">')
    lines.append(
        '<meta name="viewport" content="width=device-width, initial-scale=1.0">'
    )
    lines.append("<title>SmartEVSE-3 Traceability Report</title>")
    lines.append("<style>")
    lines.append(
        """
body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    max-width: 1200px;
    margin: 0 auto;
    padding: 20px;
    background: #f5f5f5;
    color: #333;
}
h1 { color: #1a5276; border-bottom: 3px solid #2980b9; padding-bottom: 10px; }
h2 { color: #2c3e50; margin-top: 30px; }
.summary {
    background: #fff;
    border-radius: 8px;
    padding: 20px;
    margin: 20px 0;
    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    display: flex;
    gap: 30px;
}
.summary .stat {
    text-align: center;
}
.summary .stat .number {
    font-size: 2em;
    font-weight: bold;
    color: #2980b9;
}
.summary .stat .label {
    font-size: 0.9em;
    color: #666;
}
.feature-section {
    background: #fff;
    border-radius: 8px;
    padding: 20px;
    margin: 15px 0;
    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}
.feature-title {
    color: #2980b9;
    font-size: 1.3em;
    margin: 0 0 15px 0;
    padding-bottom: 8px;
    border-bottom: 2px solid #eee;
}
table {
    width: 100%;
    border-collapse: collapse;
    font-size: 0.9em;
}
th {
    background: #2c3e50;
    color: white;
    padding: 10px 12px;
    text-align: left;
}
td {
    padding: 8px 12px;
    border-bottom: 1px solid #eee;
    vertical-align: top;
}
tr:hover { background: #f8f9fa; }
.req-tag {
    background: #e8f4fd;
    color: #1a5276;
    padding: 2px 8px;
    border-radius: 12px;
    font-size: 0.85em;
    font-weight: 500;
    white-space: nowrap;
}
.no-req {
    color: #999;
    font-style: italic;
    font-size: 0.85em;
}
.scenario-steps {
    margin: 4px 0 0 0;
    padding: 0;
    list-style: none;
    font-size: 0.85em;
    color: #555;
}
.scenario-steps li { margin: 2px 0; }
.step-given { color: #27ae60; }
.step-when { color: #2980b9; }
.step-then { color: #8e44ad; }
.step-keyword { font-weight: bold; }
.file-ref {
    font-family: monospace;
    font-size: 0.85em;
    color: #666;
}
.coverage-bar {
    height: 8px;
    background: #ecf0f1;
    border-radius: 4px;
    overflow: hidden;
    margin-top: 5px;
}
.coverage-fill {
    height: 100%;
    background: #27ae60;
    border-radius: 4px;
}
footer {
    text-align: center;
    margin-top: 30px;
    padding: 15px;
    color: #999;
    font-size: 0.85em;
}
"""
    )
    lines.append("</style>")
    lines.append("</head>")
    lines.append("<body>")

    lines.append("<h1>SmartEVSE-3 Traceability Report</h1>")

    # Summary
    coverage_pct = (total_with_req / total_tests * 100) if total_tests else 0
    lines.append('<div class="summary">')
    lines.append(
        f'<div class="stat"><div class="number">{len(features)}</div>'
        f'<div class="label">Features</div></div>'
    )
    lines.append(
        f'<div class="stat"><div class="number">{total_tests}</div>'
        f'<div class="label">Test Scenarios</div></div>'
    )
    lines.append(
        f'<div class="stat"><div class="number">{total_with_req}</div>'
        f'<div class="label">With Requirements</div></div>'
    )
    lines.append(
        f'<div class="stat"><div class="number">{coverage_pct:.0f}%</div>'
        f'<div class="label">Requirement Coverage</div></div>'
    )
    lines.append("</div>")

    # Per-feature tables
    for feature_name, tests in features.items():
        feature_reqs = sum(1 for t in tests if t.req)
        feat_pct = (feature_reqs / len(tests) * 100) if tests else 0

        lines.append('<div class="feature-section">')
        lines.append(
            f'<div class="feature-title">{h(feature_name)} '
            f"({len(tests)} scenarios)</div>"
        )
        lines.append(f'<div class="coverage-bar">')
        lines.append(f'<div class="coverage-fill" style="width:{feat_pct}%"></div>')
        lines.append("</div>")
        lines.append("<table>")
        lines.append("<thead><tr>")
        lines.append(
            "<th>Requirement</th><th>Scenario</th>"
            "<th>Steps</th><th>Test Function</th><th>Source</th>"
        )
        lines.append("</tr></thead>")
        lines.append("<tbody>")

        for t in tests:
            req_cell = (
                f'<span class="req-tag">{h(t.req)}</span>'
                if t.req
                else '<span class="no-req">-</span>'
            )

            steps_html = '<ul class="scenario-steps">'
            for g in t.given:
                steps_html += (
                    f'<li class="step-given">'
                    f'<span class="step-keyword">Given</span> {h(g)}</li>'
                )
            for w in t.when:
                steps_html += (
                    f'<li class="step-when">'
                    f'<span class="step-keyword">When</span> {h(w)}</li>'
                )
            for th_ in t.then:
                steps_html += (
                    f'<li class="step-then">'
                    f'<span class="step-keyword">Then</span> {h(th_)}</li>'
                )
            steps_html += "</ul>"

            lines.append("<tr>")
            lines.append(f"<td>{req_cell}</td>")
            lines.append(f"<td>{h(t.scenario)}</td>")
            lines.append(f"<td>{steps_html}</td>")
            lines.append(
                f'<td><code>{h(t.function_name)}</code></td>'
            )
            lines.append(
                f'<td><span class="file-ref">'
                f"{h(t.file_name)}:{t.line_number}</span></td>"
            )
            lines.append("</tr>")

        lines.append("</tbody></table>")
        lines.append("</div>")

    lines.append(
        "<footer>Generated by extract_traceability.py | "
        "SmartEVSE-3 Specification by Example</footer>"
    )
    lines.append("</body></html>")

    with open(output_path, "w") as f:
        f.write("\n".join(lines))

    print(f"HTML report written to: {output_path}")


def generate_markdown(features: OrderedDict, output_path: str):
    """Generate a Markdown specification document."""
    total_tests = sum(len(tests) for tests in features.values())
    total_with_req = sum(
        1 for tests in features.values() for t in tests if t.req
    )

    lines = []
    lines.append("# SmartEVSE-3 Test Specification")
    lines.append("")
    lines.append(
        f"**{len(features)} features** | "
        f"**{total_tests} scenarios** | "
        f"**{total_with_req} with requirement IDs**"
    )
    lines.append("")
    lines.append("---")
    lines.append("")

    # Table of contents
    lines.append("## Table of Contents")
    lines.append("")
    for i, feature_name in enumerate(features, 1):
        anchor = feature_name.lower().replace(" ", "-").replace("/", "")
        lines.append(f"{i}. [{feature_name}](#{anchor})")
    lines.append("")

    # Per-feature sections
    for feature_name, tests in features.items():
        lines.append(f"## {feature_name}")
        lines.append("")

        for t in tests:
            lines.append(f"### {t.scenario}")
            lines.append("")
            if t.req:
                lines.append(f"**Requirement:** `{t.req}`")
                lines.append("")

            for g in t.given:
                lines.append(f"- **Given** {g}")
            for w in t.when:
                lines.append(f"- **When** {w}")
            for th_ in t.then:
                lines.append(f"- **Then** {th_}")

            lines.append("")
            lines.append(
                f"> Test: `{t.function_name}` in "
                f"`{t.file_name}:{t.line_number}`"
            )
            lines.append("")

        lines.append("---")
        lines.append("")

    with open(output_path, "w") as f:
        f.write("\n".join(lines))

    print(f"Markdown specification written to: {output_path}")


def generate_markdown_report(features: OrderedDict, output_path: str):
    """Generate a Markdown traceability report (matrix format)."""
    total_tests = sum(len(tests) for tests in features.values())
    total_with_req = sum(
        1 for tests in features.values() for t in tests if t.req
    )
    coverage_pct = (total_with_req / total_tests * 100) if total_tests else 0

    lines = []
    lines.append("# SmartEVSE-3 Traceability Report")
    lines.append("")
    lines.append(
        f"**{len(features)} features** | "
        f"**{total_tests} scenarios** | "
        f"**{total_with_req} with requirement IDs** | "
        f"**{coverage_pct:.0f}% coverage**"
    )
    lines.append("")
    lines.append("---")
    lines.append("")

    # Summary table
    lines.append("## Summary")
    lines.append("")
    lines.append("| Feature | Scenarios | With Req ID | Coverage |")
    lines.append("|---------|-----------|-------------|----------|")
    for feature_name, tests in features.items():
        n_req = sum(1 for t in tests if t.req)
        feat_pct = (n_req / len(tests) * 100) if tests else 0
        lines.append(
            f"| {feature_name} | {len(tests)} | {n_req} | {feat_pct:.0f}% |"
        )
    lines.append(
        f"| **TOTAL** | **{total_tests}** | **{total_with_req}** "
        f"| **{coverage_pct:.0f}%** |"
    )
    lines.append("")

    # Per-feature traceability matrices
    for feature_name, tests in features.items():
        lines.append(f"## {feature_name}")
        lines.append("")
        lines.append(
            "| Requirement | Scenario | Test Function | Source |"
        )
        lines.append("|-------------|----------|---------------|--------|")

        for t in tests:
            req = f"`{t.req}`" if t.req else "-"
            lines.append(
                f"| {req} | {t.scenario} "
                f"| `{t.function_name}` "
                f"| `{t.file_name}:{t.line_number}` |"
            )

        lines.append("")

        # Detailed steps (collapsible)
        lines.append("<details>")
        lines.append(f"<summary>Detailed steps ({len(tests)} scenarios)</summary>")
        lines.append("")
        for t in tests:
            lines.append(f"### {t.scenario}")
            if t.req:
                lines.append(f"**Requirement:** `{t.req}`")
            lines.append("")
            for g in t.given:
                lines.append(f"- **Given** {g}")
            for w in t.when:
                lines.append(f"- **When** {w}")
            for th_ in t.then:
                lines.append(f"- **Then** {th_}")
            lines.append("")
        lines.append("</details>")
        lines.append("")
        lines.append("---")
        lines.append("")

    lines.append(
        "*Generated by extract_traceability.py | "
        "SmartEVSE-3 Specification by Example*"
    )

    with open(output_path, "w") as f:
        f.write("\n".join(lines))

    print(f"Markdown traceability report written to: {output_path}")


def print_summary(features: OrderedDict):
    """Print a summary table to stdout."""
    total_tests = sum(len(tests) for tests in features.values())
    total_with_req = sum(
        1 for tests in features.values() for t in tests if t.req
    )

    print()
    print("=" * 60)
    print("  SmartEVSE-3 Traceability Summary")
    print("=" * 60)
    print()
    print(f"  {'Feature':<40} {'Scenarios':>9} {'With Req':>9}")
    print(f"  {'-'*40} {'-'*9} {'-'*9}")

    for feature_name, tests in features.items():
        n_req = sum(1 for t in tests if t.req)
        print(f"  {feature_name:<40} {len(tests):>9} {n_req:>9}")

    print(f"  {'-'*40} {'-'*9} {'-'*9}")
    print(f"  {'TOTAL':<40} {total_tests:>9} {total_with_req:>9}")
    print()

    if total_tests > 0:
        pct = total_with_req / total_tests * 100
        print(f"  Requirement traceability: {pct:.1f}%")
    print()


def main():
    parser = argparse.ArgumentParser(
        description="Extract SbE annotations from C test files"
    )
    parser.add_argument(
        "--html",
        metavar="FILE",
        help="Generate HTML traceability report",
    )
    parser.add_argument(
        "--markdown",
        metavar="FILE",
        help="Generate Markdown specification document",
    )
    parser.add_argument(
        "--markdown-report",
        metavar="FILE",
        help="Generate Markdown traceability report (matrix format)",
    )
    parser.add_argument(
        "test_dir",
        nargs="?",
        default=None,
        help="Directory containing test source files (default: auto-detect)",
    )
    args = parser.parse_args()

    # Find the test directory
    if args.test_dir:
        test_dir = args.test_dir
    else:
        # Auto-detect: look relative to script location
        script_dir = os.path.dirname(os.path.abspath(__file__))
        test_dir = os.path.join(script_dir, "..", "tests")
        if not os.path.isdir(test_dir):
            test_dir = os.path.join(os.getcwd(), "tests")

    test_dir = os.path.abspath(test_dir)

    if not os.path.isdir(test_dir):
        print(f"Error: Test directory not found: {test_dir}", file=sys.stderr)
        sys.exit(1)

    # Find all test files
    test_files = sorted(glob.glob(os.path.join(test_dir, "test_*.c")))

    if not test_files:
        print(f"Error: No test files found in {test_dir}", file=sys.stderr)
        sys.exit(1)

    print(f"Scanning {len(test_files)} test files in {test_dir}")

    # Extract annotations
    all_annotations = []
    for file_path in test_files:
        annotations = extract_annotations(file_path)
        all_annotations.extend(annotations)
        if annotations:
            print(f"  {os.path.basename(file_path)}: {len(annotations)} annotated tests")

    if not all_annotations:
        print("\nNo SbE annotations found. Add @feature/@scenario comment blocks to test functions.")
        sys.exit(0)

    # Group by feature
    features = group_by_feature(all_annotations)

    # Print summary
    print_summary(features)

    # Generate outputs
    if args.html:
        generate_html(features, args.html)

    if args.markdown:
        generate_markdown(features, args.markdown)

    if args.markdown_report:
        generate_markdown_report(features, args.markdown_report)

    if not args.html and not args.markdown and not args.markdown_report:
        print("Tip: Use --html or --markdown to generate reports.")


if __name__ == "__main__":
    main()
