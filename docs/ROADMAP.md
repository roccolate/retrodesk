# Roadmap

> Status (2026-07): RetroDesk remains pre-alpha. The File Manager, taskbar, input,
> UTF-8 storage, and Notepad parity stack is now integrated into `main`. The code
> has passed the automated Linux/Windows matrix used during development, but no
> stable release tag should be created until clean candidate evidence and manual
> terminal smoke results are attached to the exact release commit.

## Current State

### Runtime and desktop

- Explicit `core`, `platform`, `render`, `wm`, `app`, `apps`, `ui`, and `storage`
  layers are established.
- The desktop uses a single event loop and a single renderer flush point.
- Global shortcuts no longer steal printable editor text.
- The bottom status widget provides an interactive taskbar with Launcher access,
  app state, instance counts, clock, compact labels, and mouse hit testing.
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

- Linux/POSIX is the complete current file-app profile.
- Windows Debug/Release portable-runtime builds and tests pass with PDCurses.
- Native Windows storage or explicit file-app gating is still required before
  equivalent product support is claimed.
- macOS and DOS remain experimental/reduced profiles.

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
- Debug/Release manifest comparison exists;
- DOS source-manifest synchronization is guarded;
- user, architecture, storage, testing, portability, app, and shortcut docs are
  synchronized with the integrated product slice.

Remaining work:

- decide native Windows storage versus explicit app gating;
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

- add a native Windows storage adapter or explicit filesystem-app gating;
- validate Windows interactive PDCurses behavior;
- document macOS build/input evidence;
- keep DOS as a reduced profile with exact exclusions;
- ensure all profiles restore terminal/console state on failure and shutdown.

Exit criteria: every claimed platform feature is backed by current build, test,
and smoke evidence.

### `v0.5.0`: Window Manager and Desktop Polish

Goal: make multi-window workflows feel complete.

Planned work:

- define minimize/restore and maximize contracts;
- extend taskbar behavior to minimized windows after the WM contract exists;
- improve keyboard move/resize feedback;
- add stronger focus/cycling and narrow-screen behavior tests;
- reduce redraw/flicker without violating the single-flush rule.

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
- Prefer small behavior slices that exercise existing contracts.
- Do not claim platform or Unicode support beyond current evidence.
- Never trade data safety or deterministic cleanup for superficial parity.
