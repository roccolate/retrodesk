# Roadmap

> Snapshot: 2026-07-22. RetroDesk remains pre-alpha. This roadmap describes
> sequencing, not shipped releases. See [PROJECT_STATUS.md](PROJECT_STATUS.md) for
> what is actually in `main` and [KNOWN_ISSUES.md](KNOWN_ISSUES.md) for current
> blockers and risks.

## Roadmap Rules

- A milestone is not shipped merely because most implementation work exists.
- A feature is complete only when it is in `main`, documented, regression-tested,
  and validated for every profile claimed by the milestone.
- PR-only work is not counted as integrated.
- Release evidence must be tied to the exact candidate commit.
- Data safety, lifecycle correctness, one event loop, and one flush take priority
  over feature parity.

## Current Baseline

### Integrated foundations

- Layered C11 runtime: `core`, `platform`, `render`, `wm`, `app`, `apps`, `ui`,
  `storage`.
- Single Desktop event loop and single renderer flush.
- Transactional global shutdown and lifecycle-aware close.
- Desktop-owned UTF-8 clipboard.
- Adapter-neutral storage metadata and native POSIX, Win32, and DJGPP adapters.
- Strict Linux and Windows builds/tests, sanitizer gate, non-interactive smoke,
  pinned DJGPP build, native DOS storage contract, DOSBox-X smoke, and DOS
  artifact generation.
- Budgeted app-service callback contract for future asynchronous apps.

### Integrated product workflows

- File Manager navigation, viewport, hidden files, refresh, text open, rename,
  directory creation, and empty-file creation.
- Notepad UTF-8 editing, display-cell rendering, selection, clipboard,
  undo/redo, Find, Word Wrap, Save/Save As, version conflict handling, recovery,
  and safe dirty close.
- Themed taskbar and Start-menu-style Launcher.
- Minimize/restore, maximize/unmaximize, and keyboard move/resize HUD.

### Work validated elsewhere but not integrated

- PR #27: checked collection growth and deterministic `WindowId` exhaustion;
  stale base, rebuild required.
- PR #24: native responsive Notepad File/Edit/View menus; stale stacked base,
  rebuild required.

## Immediate Execution Order

### Phase A — Restore a clean integration baseline

1. Rebuild PR #27 on current `main`.
2. Re-run Linux, Windows, sanitizer, source-manifest, DJGPP, and DOSBox-X gates.
3. Merge the hardening slice or explicitly defer it with a bounded risk record.
4. Rebuild PR #24 on the resulting `main`; reconcile current desktop bridge and
   window-geometry behavior before validating it.

Reason: continuing to stack UI and app work while old correctness branches remain
open makes the repository state harder to reason about.

### Phase B — Finish `v0.5.0` desktop polish

Remaining work:

- define deterministic taskbar congestion and overflow behavior;
- prioritize focused and running apps when width is insufficient;
- keep every hidden entry keyboard- and mouse-accessible;
- ensure overflow and clock regions cannot overlap;
- add crowded-catalog and extremely narrow-screen regressions;
- exercise focus, cycling, minimize, maximize, Launcher, and window-mode behavior
  across all four themes;
- complete manual visual smoke on curses and ANSI paths;
- reduce unnecessary redraw/flicker without adding another flush owner;
- decide whether hover/pressed/disabled states are meaningful for every backend
  and add them only with deterministic event semantics.

Already completed for `v0.5.0`:

- taskbar visual hierarchy and compact layouts;
- Start-menu-style Launcher geometry/grouping/navigation;
- theme-specific XP, Hacker, Amiga, and Win31 desktop surfaces;
- taskbar minimize/restore;
- maximize/unmaximize with geometry restoration;
- move/resize themed frame and responsive HUD;
- responsive DrawList regressions for current fixed catalog.

Exit criteria:

- every taskbar app remains reachable at supported minimum widths;
- taskbar, Launcher, minimize/maximize, and window operation behavior is
  regression-backed;
- mouse hit regions match exact rendered surfaces;
- every theme has approved manual curses and ANSI evidence;
- no avoidable full-frame redraw/flicker regression is known.

### Phase C — Reduce structural integration debt

- Split Launcher/taskbar/window-command ownership out of `desktop.c`.
- Replace translation-unit macro remapping with explicit per-instance interfaces.
- Move maximize and window-mode state into appropriate Desktop/WM-owned objects.
- Remove bridge-local global/static owner state.
- Disambiguate `app_launch` results.
- Separate diagnostics projection from platform capability ownership.
- Surface built-in app registration failure during Desktop initialization.
- Add selective redraw and no-op status/taskbar update suppression.

This phase should precede a large app catalog or plugin system.

## Milestones

## `v0.1.0` — Reproducible Pre-Alpha Foundation

**Status:** not shipped; checklist reopened.

Goal: produce the first exact candidate with honest evidence, not freeze the
project at its historical feature count.

Implementation available:

- layered runtime and built-in apps;
- multi-window desktop and current taskbar/Launcher behavior;
- native storage adapters;
- automated multi-platform development gates.

Remaining release work:

- choose one exact candidate SHA;
- perform clean strict Debug/Release and sanitizer validation;
- record immutable toolchain/fixture revisions;
- run manual Linux curses, raw-TTY/ANSI, and `TERM=linux` smoke;
- record Windows/PDCurses and reduced-DOS acceptance for the scope claimed;
- complete the documentation, debt, compatibility, and known-issues gates;
- attach evidence to the candidate and only then create the tag.

Exit criteria: every item in
[RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md) is resolved or explicitly
scoped out for the exact commit.

## `v0.2.0` — CI, Documentation, and Portability Baseline

**Implementation status:** largely present; release status: unshipped.

Completed:

- canonical CMake build and strict warning policy;
- always-active test oracle guard;
- Linux static analysis, Debug/Release, sanitizers, and smoke;
- Windows Debug/Release under PDCurses;
- native Win32 storage tests;
- pinned DJGPP/PDCurses/CWSDPMI build path;
- native DOS storage contract and DOSBox-X smoke;
- Debug/Release manifest and DJGPP source parity checks;
- architecture, app, storage, input, portability, testing, and status docs.

Remaining:

- keep all documents synchronized through the candidate;
- automate a compact evidence manifest containing exact versions and SHAs;
- add a release-specific compatibility and known-issues record;
- either validate macOS or explicitly omit it from the release claim;
- retire stale historical PRs after replacement branches are integrated.

## `v0.3.0` — Useful Built-In Apps

**Implementation status:** core workflows integrated; release status: unshipped.

Completed:

- practical File Manager navigation and basic non-destructive mutations;
- practical UTF-8 Notepad editing, history, clipboard, Find, wrap, save, and
  dirty-close workflows;
- app lifecycle and storage regressions.

Remaining exit work:

- complete manual ncurses/raw-TTY app acceptance;
- audit very narrow windows and error messaging;
- decide the minimum alpha-preview app scope;
- rebuild and decide the responsive Notepad chrome from PR #24.

## `v0.4.0` — Native Windows and Secondary Profiles

**Implementation status:** native adapters and automated profiles integrated;
release status: unshipped.

Completed:

- native Win32 storage and platform-specific tests;
- native DJGPP storage and DOS contract test;
- strict Windows Debug/Release CI;
- pinned DOS build, DOSBox-X diagnostic smoke, and distribution artifact.

Remaining:

- manual native Windows/PDCurses file-app and desktop acceptance;
- documented Win32 console/PDCurses limitations for an exact candidate;
- broader reduced-DOS manual behavior and failure-recovery evidence;
- macOS evidence or explicit exclusion;
- terminal/console restoration evidence for each claimed profile.

Exit criteria: every platform feature in the support table has build, test, and
appropriate interactive evidence on the release candidate.

## `v0.5.0` — Window Manager, Launcher, and Desktop Polish

**Implementation status:** major behavior integrated; remaining work listed in
Phase B.

Goal: make multi-window workflows coherent and intentional rather than a debug
shell around apps.

Integrated:

- redesigned taskbar and Launcher;
- four theme surface identities;
- minimize/restore and maximize/unmaximize;
- keyboard move/resize feedback;
- responsive current-catalog layouts and hit testing.

Remaining:

- overflow/congestion policy;
- stronger crowded/narrow focus and cycling tests;
- complete visual smoke matrix;
- redraw/flicker reduction;
- final acceptance of pressed/hover/disabled states where backend semantics allow.

## `v0.6.0` — File and Editor Expansion

Goal: expand workflows without weakening storage, history, Unicode, or close
contracts.

File Manager candidates:

- delete/trash with confirmation and recovery policy;
- copy/move with no-overwrite and cross-device semantics;
- recursive operations with progress, cancellation, and partial-failure reports;
- previews, bookmarks, and optional dual-pane mode.

Notepad candidates:

- Replace and Replace All;
- mouse cursor placement, drag selection, and wheel scrolling;
- native system clipboard behind a portable service;
- Unicode normalization policy and broader width testing;
- tabs, syntax highlighting, multiple encodings, and large-file strategy only
  after explicit contracts exist.

Required first step: rebuild or replace PR #24 rather than layering new editor UI
on its stale base.

## `v0.7.0` — Configuration and Persistence

- typed configuration model;
- explicit CLI/config/default precedence;
- theme/backend preference persistence;
- optional session/window layout persistence;
- safe schema/version migration;
- no hidden globals or backend-specific policy in app code.

## `v0.8.0` — Packaging and Distribution

- reproducible install/package instructions;
- Linux and Windows artifacts for validated profiles;
- maintained reduced DOS package;
- automated checksums and provenance/evidence manifest;
- release notes and exact known-issues record;
- installation/uninstallation behavior that does not depend on a source checkout.

## `v0.9.0` — Contract Freeze and Beta Stabilization

- remove temporary bridge architecture;
- freeze major public/domain contracts for the v1 cycle;
- publish a measured compatibility matrix;
- complete stress, failure-injection, and long-running service tests;
- remove release-blocking uncertainty or explicitly defer it;
- finish user-facing documentation.

## `v1.0.0` — Stable Terminal Desktop Baseline

Target outcome:

- validated Linux and Windows product profiles;
- dependable window management, taskbar, Launcher, File Manager, Notepad, and
  Diagnostics;
- predictable capability-based input/render behavior;
- basic configuration and optional persistence;
- reproducible packaging and exact release evidence;
- no known critical data-loss, lifecycle, ownership, or structural defects.

## Prioritization Policy

- Resolve stale correctness and integration work before broad new features.
- Treat `PROJECT_STATUS.md` as the integrated-state source.
- Preserve one event loop and one flush point.
- Keep platform/OS behavior behind capabilities and adapters.
- Require lower-level and end-to-end tests for behavior changes.
- Treat taskbar and Launcher as first-class desktop UI.
- Never claim Unicode or platform support beyond evidence.
- Prefer small, independently validated slices.
- Never trade data safety or deterministic cleanup for superficial parity.