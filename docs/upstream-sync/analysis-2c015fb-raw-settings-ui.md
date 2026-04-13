# Analysis: Upstream Commit 2c015fb — Raw Settings UI

## Question

Adopt upstream `2c015fb` ("Improved Raw Settings view with formatted JSON and
Download button (#353)") now, or defer?

## TL;DR

**Defer to a future Plan 07 (Web UI Modernization) PR.** The change is a
nice-to-have UX polish (in-page JSON viewer with syntax highlighting + download)
that conflicts heavily with the fork's diverged dashboard and its own planned
web UI redesign. The fork already has a working "Raw Data" link that opens the
raw `/settings` JSON in the browser — no user-visible regression from deferring.

## What 2c015fb does

Adds an in-page raw settings viewer. The existing "Raw Data" link (currently
href=`/settings`) is replaced with a link that loads the same dashboard with
`?raw=1` in the URL. When that flag is present, the dashboard switches to a
viewer mode that:

- Fetches `/settings` (same endpoint as today)
- Syntax-highlights the JSON in-browser (custom JS tokenizer)
- Provides a **Refresh** button
- Provides a **Download** button (saves the JSON as a file)
- Renders with sticky action buttons and full-screen scroll

All of this lives inline in `data/index.html` — 179 lines of new JavaScript and
markup. Plus:

- `data/styling.css`: 58 new lines of viewer-specific styles
- `packfs.py`: 55 lines of pack-time changes (probably adds the new asset /
  gzips CSS)
- `platformio.ini`: 12 lines (pack-dependency updates)
- `.gitignore`: 1 line

## Fork state

The fork's web UI has diverged significantly from upstream:

| Fork divergence | Mentioned in |
|---|---|
| Offline-first web UI (no CDN) | `docs/upstream-differences.md` "Web & Connectivity" |
| WebSocket data channel replaces HTTP polling | same |
| Dashboard card redesign | same |
| Dark mode | same |
| Load balancing node overview | same |
| Diagnostic telemetry viewer | same |
| LCD widget modernization | same |

File-level diffs:

| File | Upstream | Fork |
|---|---|---|
| Dashboard HTML | `data/index.html` (same path) | `data/index.html` — 500 lines, heavily customized |
| Stylesheet | `data/styling.css` (NEW in upstream) | `data/style.css` (existing, different name) |
| Pack script | `packfs.py` at repo root | `SmartEVSE-3/packfs.py` (fork has its own) |

The fork's existing "Raw Data" UX:

```html
<a class="btn btn-sm" href="/settings" data-ajax="false" title="Raw JSON settings">Raw Data</a>
```

Clicking this opens the raw JSON in a new tab — browser renders it as-is. No
syntax highlighting, no download button, but functional.

## Why deferring is the right call

1. **High merge conflict risk.** 179 lines of new inline JS slotted into a
   500-line file whose surrounding structure has been substantially rewritten
   by the fork. Line-by-line merge would fight both sides' intent.

2. **Stylesheet rename.** Upstream adds `styling.css`; fork uses `style.css`
   with its own ruleset. Merging requires renaming or duplicating.

3. **Pack script is custom.** `SmartEVSE-3/packfs.py` in the fork doesn't match
   the path or structure upstream edits. Cannot apply patch mechanically.

4. **Fork has its own Web UI plan.** `CLAUDE.md` lists **Plan 07 Web UI
   Modernization** as P4 ("largest scope, least safety-critical, back of
   backlog"). An incoming JSON viewer improvement is exactly the kind of thing
   Plan 07 should consider — possibly redesigning the whole settings surface,
   not patching the current dashboard.

5. **No safety or functional regression from deferring.** Users can still view
   settings via the existing Raw Data link. The download capability is a
   convenience, not a required feature.

6. **Context budget.** Each P3 adaptation of this size eats significant time
   and reviewer attention. Better to focus reviewer attention on the P1/P2
   safety fixes that have already landed.

## Pre-integration checklist (if revived later)

1. [ ] Decide whether to keep it as an in-page mode (`?raw=1`) or a standalone
       `/raw.html` page. Upstream chose in-page; `/raw.html` would avoid
       touching the main dashboard.
2. [ ] Reconcile `styling.css` vs fork's `style.css`. Options:
       - Add a new `raw.css` served only on the viewer page
       - Fold upstream's 58 lines into fork's `style.css` under a scoped class
3. [ ] Port `packfs.py` changes to the fork's layout.
4. [ ] Decide whether the dashboard gets the viewer, or the diagnostic panel
       (Plan 06) absorbs it.
5. [ ] On-device test: render large `/settings` JSON without JS timeout on ESP32's
       WebSocket path.
6. [ ] On-device test: download produces a valid `.json` file in common browsers.
7. [ ] Consider whether the same viewer should serve the diag telemetry feed.

## Decision

**Defer.** Track as input for Plan 07. No code changes needed now. The
existing Raw Data link covers the baseline need.
