# Roadmap

> Status (2026-07): RetroDesk remains pre-alpha. The File Manager, taskbar, input,
> UTF-8 storage, Notepad parity stack, native Win32/DJGPP storage adapters, and
> budgeted app-service tick are integrated into `main`. Automated Linux, Windows,
> and DOS development matrices exist, but no stable release tag should be created
> until clean candidate evidence and manual terminal smoke results are attached to
> the exact release commit.

## Current State

### Runtime and desktop

- Explicit `core`, `platform`, `render`, `wm`, `app`, `apps`, `ui`, and `storage`
  layers are established.
- The desktop uses a single event loop and a single renderer flush point.
- Global shortcuts no longer steal printable editor text.
- The bottom status widget provides an interactive taskbar with Launcher access,
  app state, instance counts, clock, compact labels, and mouse hit testing.
- The taskbar and Launcher are functionally useful but visually provisional; a
  focused redesign is scheduled under `v0.5.0`.
- Optional app services receive bounded, non-blocking callbacks from the Desktop
  loop without introducing worker-owned UI state or nested event loops.
- Focus, drag, move/resize, modal routing, and lifecycle-aware close behavior are
  test-backed.

### File Manager

Implemented on the Linux/POSIX product slice:

- bounded directory listing and keyboard viewport;
- arrows, `J/K`, Page Up/Down, Home/End;
- parent navigation and refresh;
- hidden-file toggle;
- regular text-file open through Notepad;
- empty-directory and file-size display;
- rename without intentional overwrite;
- create directory and create empty file;
- retained selection after refresh and mutations.

### Notepad

Implemented on the Linux/POSIX product slice:

- validated UTF-8 open/save;
- codepoint-safe editing and display-cell-aware rendering;
- multiline selection and shared in-process clipboard;
- bounded undo/redo;
- Find with next-result wraparound;
- soft Word Wrap over visual rows;
- Save, Save As, atomic write, and version-conflict reporting;
- explicit Save/Discard/Cancel dirty close flow;
- multiple Notepad instances and taskbar cycling.

### Platform status

- Linux/POSIX remains the most complete and manually exercised file-app profile.
- Windows Debug/Release portable-runtime builds and tests pass with PDCurses, and
  the native Win32 storage adapter is integrated. Interactive Windows validation
  remains required before claiming equivalent product maturity.
- The native DJGPP storage adapter, DOS target, and automated DOS smoke path are
  integrated. DOS remains a deliberately reduced profile with exact exclusions.
- macOS remains experimental until current build, input, and smoke evidence is
  recorded.

## Release Gates

### Foundation gate

A release candidate must pass from a clean checkout:

- strict CMake configure and warning-clean build;
- Debug and Release tests with matching manifests;
- always-active test-oracle guard;
- retrocore fixture validation;
- DJGPP source-manifest synchronization;
- non-interactive smoke;
- interactive curses/TTY smoke for claimed terminal profiles;
- no stale generated artifacts or unrecorded toolchain assumptions.

### Product gate

The candidate must demonstrate:

- File Manager keyboard navigation and basic mutation workflow;
- Notepad UTF-8 editing, close, history, clipboard, Find, wrap, and save workflow;
- taskbar launch/focus/cycle behavior;
- terminal control-key delivery and state restoration;
- no known critical data-loss or lifecycle defects.

## Milestones

### `v0.1.0`: Reproducible Foundation

Goal: tag the historical foundation only when it is reproducible and the release
checklist is backed by exact evidence.

Remaining work:

- rebuild the checklist on a clean candidate;
- record compiler, CMake, curses/PDCurses, and fixture versions;
- attach Debug/Release, smoke, and sanitizer evidence where available;
- ensure documentation and source manifests match the candidate.

Exit criteria: [RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md) is complete for
the exact tag commit.

### `v0.2.0`: CI, Documentation, and Portability Baseline

Goal: make project claims honest and repeatable.

Current progress:

- Linux static analysis and Debug/Release tests exist;
- Windows Debug/Release portable-runtime tests exist;
- native Win32 storage is integrated and has platform-specific contract tests;
- native DJGPP storage, target build, source synchronization, and DOS smoke exist;
- Debug/Release manifest comparison exists;
- user, architecture, storage, testing, portability, app, and shortcut docs exist
  for the integrated product slice.

Remaining work:

- synchronize all documentation and product claims with the Win32/DJGPP adapters;
- add macOS evidence or keep the profile explicitly experimental;
- automate more release-evidence capture;
- establish a concise known-issues document for candidate releases.

### `v0.3.0`: Useful Built-In Apps

Goal: provide practical keyboard-first File Manager and Notepad workflows on
Linux.

Status: core functionality is implemented and automated regressions exist.

Remaining exit work:

- complete manual ncurses and raw-TTY smoke testing;
- verify representative terminals and `TERM=linux` behavior;
- audit error messages and narrow-terminal rendering;
- decide whether the current functionality is sufficient for an alpha product
  preview.

### `v0.4.0`: Native Windows and Secondary Profiles

Goal: establish credible platform breadth.

Current progress:

- native Win32 and DJGPP storage adapters are integrated;
- Windows Debug/Release builds and tests are automated;
- the native DOS target and DOSBox-X smoke path are automated.

Remaining work:

- validate Windows interactive PDCurses behavior and native file workflows;
- document exact Win32 storage guarantees and known limitations;
- document macOS build/input evidence or retain its experimental status;
- validate the reduced DOS profile and record its exact exclusions;
- ensure all profiles restore terminal/console state on failure and shutdown.

Exit criteria: every claimed platform feature is backed by current build, test,
and smoke evidence.

### `v0.5.0`: Window Manager, Launcher, and Desktop Polish

Goal: make multi-window workflows feel complete and give the desktop a coherent,
intentional visual identity instead of a merely functional status row.

Planned work:

- redesign the taskbar visual hierarchy so the Apps button, running applications,
  instance state, notifications/status area, and clock read as distinct regions;
- redesign the Launcher geometry, spacing, borders, selected-row treatment, and
  grouping so it feels like a real Start menu rather than a debug popup;
- define theme-specific taskbar and Launcher treatments for XP, Win31, Amiga, and
  Hacker themes while preserving the same interaction contract;
- improve focused, running, background, multiple-instance, disabled, pressed,
  hovered, and future minimized-window indicators;
- replace fragile compact labels with deterministic truncation and prioritization
  rules for narrow terminals;
- ensure taskbar hit regions always match the rendered buttons and never overlap
  the clock or adjacent application entries;
- add draw-list regressions for normal, crowded, and narrow-screen taskbar layouts;
- perform manual visual smoke checks across curses/ANSI backends and every theme;
- define minimize/restore and maximize contracts;
- extend taskbar behavior to minimized windows after the WM contract exists;
- improve keyboard move/resize feedback;
- add stronger focus/cycling and narrow-screen behavior tests;
- reduce redraw/flicker without violating the single-flush rule.

Exit criteria: the taskbar and Launcher remain fully keyboard-usable, preserve
correct mouse hit testing, degrade predictably on narrow terminals, and have an
approved visual treatment in every supported theme.

### `v0.6.0`: File and Editor Expansion

Goal: extend useful workflows without bypassing storage or editor contracts.

Candidate File Manager work:

- delete/trash with explicit confirmation and recovery policy;
- copy and move;
- recursive-operation progress and cancellation;
- previews, bookmarks, and optional dual-pane mode.

Candidate Notepad work:

- Replace and Replace All;
- mouse cursor placement and drag selection;
- native system clipboard integration;
- normalization policy and broader Unicode-width validation;
- tabs, syntax highlighting, multiple encodings, or large-file strategy only
  after their contracts are defined.

### `v0.7.0`: Configuration and Persistence

- theme/backend preference configuration;
- explicit override precedence;
- optional desktop/session layout persistence;
- no hidden global state or untestable configuration paths.

### `v0.8.0`: Packaging and Distribution

- reproducible install/package instructions;
- supported-platform artifacts;
- automated smoke where terminal constraints permit;
- release notes and known-issues evidence tied to exact commits.

### `v0.9.0`: Contract Freeze and Beta Stabilization

- freeze major internal contracts for the v1 cycle;
- publish a real compatibility matrix;
- remove release-blocking uncertainty;
- defer or fix remaining defects explicitly;
- complete user-facing documentation.

### `v1.0.0`: Stable Terminal Desktop Baseline

Target outcome:

- validated Linux and Windows product profiles;
- dependable File Manager, text editor, diagnostics, taskbar, and window
  management;
- predictable input/render behavior by capability profile;
- basic configuration and persistence;
- reproducible packaging and release evidence;
- no known critical data-loss, lifecycle, or structural defects.

## Prioritization Rules

- Finish release blockers before broad new features.
- Preserve the single event loop and single flush point.
- Keep backend-native and OS-native details behind adapters.
- Require tests for input, focus, render, lifecycle, storage, and fallback changes.
- Treat the taskbar and Launcher as first-class desktop UI, not diagnostic chrome.
- Prefer small behavior slices that exercise existing contracts.
- Do not claim platform or Unicode support beyond current evidence.
- Never trade data safety or deterministic cleanup for superficial parity.
