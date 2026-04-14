#!/usr/bin/env python3
"""
lint_ui_visibility.py — assert show/hide symmetry for hidden-by-default UI elements.

Background (issue #129): the EVCC-inspired dashboard redesign added
`style="display:none"` to several elements (e.g. ocpp_config_outer, show_rfid)
without adding the matching showById() / showEl() / showAll() in app.js.
Result: those elements stayed permanently invisible to users.

This script enforces a simple invariant for every element in
SmartEVSE-3/data/index.html that defaults to display:none:

    - Either it is referenced by a `showById('id')`, a
      `$qa('[id=id]').forEach(showEl)`, or a `showAll('.class')` call
      (where class is on the same element), OR
    - It is in the EXEMPT_IDS allowlist below (truly static / never-shown).

It also flags asymmetric cases:
    - hide present but show missing → ❌ FAIL (the issue #129 bug)
    - show present but hide missing → ⚠ warning (often OK for non-reversible
      reveals like an error banner that stays up once shown)

Run from repo root:
    python3 SmartEVSE-3/test/native/scripts/lint_ui_visibility.py

Wire into native test target: see Makefile target `lint-ui-visibility`.

Exit codes:
    0  no asymmetric (only-hide) bugs found
    1  one or more elements have a hide call but never a show call
"""

from __future__ import annotations
import re
import sys
from pathlib import Path

REPO_ROOT  = Path(__file__).resolve().parents[4]
HTML_PATH  = REPO_ROOT / "SmartEVSE-3" / "data" / "index.html"
JS_PATH    = REPO_ROOT / "SmartEVSE-3" / "data" / "app.js"

# IDs of elements that are intentionally permanently hidden (e.g. waiting on
# a feature implementation, or controlled by hand-off to other JS files).
# Keep this list short; prefer fixing the JS over adding entries here.
EXEMPT_IDS = {
    # mqtt header chip — placeholder for "MQTT connected" indicator that has
    # never been wired up. Tracked separately for Plan 07; not a regression.
    "mqtt",
}


def collect_hidden_elements(html: str) -> list[tuple[str, str]]:
    """Return (id, class_attr) for every opening tag whose style attribute
    contains display:none, regardless of attribute ordering."""
    items: list[tuple[str, str]] = []
    seen_ids: set[str] = set()

    # Match every opening tag, parse its attribute soup, then keep tags whose
    # style="...display:none..." and that have an id="..."
    for tag in re.finditer(r'<[a-zA-Z][^>]*?>', html):
        body = tag.group(0)
        style = re.search(r'style="([^"]*)"', body)
        if not style or 'display:none' not in style.group(1).replace(' ', ''):
            continue
        ident = re.search(r'id="([^"]+)"', body)
        if not ident:
            continue
        if ident.group(1) in seen_ids:
            continue
        klass = re.search(r'class="([^"]*)"', body)
        items.append((ident.group(1), klass.group(1) if klass else ""))
        seen_ids.add(ident.group(1))
    return sorted(items)


def count_show_paths(js: str, ident: str, klass: str) -> int:
    """Count any code path that can make the element visible.
    Includes `style.display = ...` direct assignments — those typically toggle
    in both directions (e.g. `... ? '' : 'none'`)."""
    patterns = [
        rf"showById\(['\"]{re.escape(ident)}['\"]\)",
        rf"\$qa\([^)]*\[id={re.escape(ident)}\][^)]*\)\.forEach\(showEl\)",
        rf"\$id\(['\"]{re.escape(ident)}['\"]\)\.style\.display",
    ]
    for c in klass.split():
        patterns.append(rf"showAll\(['\"]\.{re.escape(c)}['\"]\)")
    return sum(len(re.findall(p, js)) for p in patterns)


def count_hide_paths(js: str, ident: str, klass: str) -> int:
    """Count any code path that hides the element.
    Includes `style.display = ...` direct assignments — those typically toggle
    in both directions (e.g. `... ? '' : 'none'`), so they count as both
    show and hide capability."""
    patterns = [
        rf"hideById\(['\"]{re.escape(ident)}['\"]\)",
        rf"\$qa\([^)]*\[id={re.escape(ident)}\][^)]*\)\.forEach\(hideEl\)",
        rf"\$id\(['\"]{re.escape(ident)}['\"]\)\.style\.display",
    ]
    for c in klass.split():
        patterns.append(rf"hideAll\(['\"]\.{re.escape(c)}['\"]\)")
    return sum(len(re.findall(p, js)) for p in patterns)


def is_referenced_by_js(js: str, ident: str) -> bool:
    """True if app.js mentions the id at all (handles indirect control like
    `var x = $id('ID'); x.style.display = ...` which the show/hide regexes
    miss)."""
    return bool(re.search(rf"\$id\(['\"]{re.escape(ident)}['\"]\)", js)) or \
           bool(re.search(rf"\[id={re.escape(ident)}\]", js))


def main() -> int:
    html = HTML_PATH.read_text()
    js = JS_PATH.read_text()

    items = collect_hidden_elements(html)
    failures: list[str] = []
    warnings: list[str] = []

    print(
        f"Scanning {len(items)} hidden-by-default element(s) in "
        f"{HTML_PATH.relative_to(REPO_ROOT)}\n"
    )
    print(f"  {'id':<32}  show  hide  status")
    print(f"  {'-' * 32}  ----  ----  --------------------")

    for ident, klass in items:
        if ident in EXEMPT_IDS:
            print(f"  {ident:<32}    -     -    EXEMPT (allowlisted)")
            continue
        s = count_show_paths(js, ident, klass)
        h = count_hide_paths(js, ident, klass)

        referenced = is_referenced_by_js(js, ident)
        if s == 0 and h == 0:
            if referenced:
                # JS touches the element but not via show/hide helpers — likely
                # indirect control (var x = $id('id'); x.style.display = ...).
                # Flag as warning for manual review, not a hard failure.
                warnings.append(
                    f"{ident}: referenced by JS but no explicit show/hide call "
                    f"detected. Likely indirect control (local var). Verify."
                )
                status = "⚠  indirect control"
            else:
                failures.append(
                    f"{ident}: never shown or hidden by JS (orphan). "
                    f"Either add a show path, or add to EXEMPT_IDS with rationale."
                )
                status = "❌ ORPHAN"
        elif s == 0 and h > 0:
            failures.append(
                f"{ident}: hidden by JS ({h}x) but never shown — "
                f"the HTML default 'display:none' makes it permanently invisible."
            )
            status = "❌ ASYMMETRIC (hide-only)"
        elif s > 0 and h == 0:
            warnings.append(
                f"{ident}: only ever shown ({s}x), never hidden. "
                f"OK if intentional (e.g. error banner). Verify."
            )
            status = "⚠  show-only"
        else:
            status = "OK"
        print(f"  {ident:<32}  {s:>4}  {h:>4}  {status}")

    print()
    if warnings:
        print("Warnings (review, not fatal):")
        for w in warnings:
            print(f"  ⚠  {w}")
        print()
    if failures:
        print("FAILURES — fix or allowlist these:")
        for f in failures:
            print(f"  ❌ {f}")
        print()
        return 1
    print("All hidden-by-default elements have at least one show path. ✓")
    return 0


if __name__ == "__main__":
    sys.exit(main())
