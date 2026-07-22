# RetroTUI Gap Map

> Snapshot: 2026-07-22. RetroTUI is a behavior reference and risk catalog, not
> the architecture template for RetroDesk.

A behavior is considered ported only when it fits RetroDesk ownership,
portability, storage, input, rendering, lifecycle, and testing contracts and is
integrated on `main`.

## Current Parity Snapshot

| Subsystem | RetroTUI reference | RetroDesk current status |
| --- | --- | --- |
| Desktop runtime | Broad feature set accumulated through rapid evolution | Explicit layered C11 runtime, one event loop, one flush, transactional shutdown |
| Window management | Practical multi-window desktop behavior | Dedicated WM with focus, z-order, modal routing, drag, move/resize, minimize/restore, maximize/unmaximize, and operation HUD |
| Taskbar/panel | Launcher and running-state panel | Themed taskbar with responsive controls, instance counts, minimize/restore, clock, and exact hit testing |
| Launcher | App/action menu | Bottom-left Start-menu-style modal surface with grouping, accelerators, keyboard, and mouse |
| App model | Broad app set and Python object patterns | Descriptor/runtime model with capabilities, rollback, close policy, multiple instances, and optional services |
| Input | Practical but historically terminal-dependent | Normalized events, modifier parser, keyboard-first degradation, terminal-state restoration |
| Rendering | curses-heavy direct integration | DrawList abstraction with curses/PDCurses and ANSI diff paths, one flush owner |
| Storage | Direct Python filesystem use | Portable `retro_fs` contract with native POSIX, Win32, and DJGPP adapters |
| File Manager | Mature browsing and operations | Navigation, viewport, hidden files, refresh, text open, rename, mkdir, and new file |
| Notepad | Practical text editor | UTF-8 editor, selection, clipboard, history, Find, wrap, versioned save, recovery, dirty close |
| Terminal | Shell/terminal behavior in broader reference | Only service scheduling foundation exists; Diagnostics is not a PTY/terminal |
| Plugins/advanced apps | Present in reference project | Deferred until core contracts and Desktop ownership mature |

## Completed Behavior Ports

### Desktop/runtime

- interactive taskbar model;
- Start-menu-style Launcher;
- focus and multi-instance cycling;
- taskbar minimize/restore;
- maximize/unmaximize with geometry restoration;
- keyboard move/resize with visible feedback;
- coordinated close/shutdown without nested loops;
- budgeted app-service scheduling boundary.

### File Manager

- keyboard viewport;
- retained selection;
- parent navigation/refresh;
- hidden-file policy;
- regular-text open in Notepad;
- rename without intentional overwrite;
- new directory and empty file.

### Notepad

- UTF-8-safe TextBuffer core;
- display-cell-aware cursor/render behavior;
- multiline selection;
- Desktop-owned clipboard;
- bounded undo/redo;
- Find with wraparound;
- soft Word Wrap;
- Save/Save As with optimistic concurrency;
- Save/Discard/Cancel close;
- transactional global shutdown participation.

### Portability and safety

- native POSIX, Win32, and DJGPP storage boundaries;
- strict Windows build/test profile;
- native DOS contract and DOSBox-X smoke;
- input degradation and terminal restoration;
- backend-neutral DrawLists and explicit theme tokens.

## Validated Work Not Yet Ported to `main`

### PR #27 — core collection boundaries

Proposes checked capacity growth, allocation-failure state preservation, and
deterministic `WindowId` exhaustion. The branch is stale relative to current
`main`; behavior is not considered ported until rebuilt and merged.

### PR #24 — native responsive Notepad chrome

Proposes File/Edit/View menus, `F11`, `Ctrl+N`, `Ctrl+O`, and responsive status
layout. The branch is stacked on superseded history; behavior is not part of
current parity.

## Remaining High-Value Gaps

### Desktop and runtime

- taskbar overflow and priority policy for a larger app catalog;
- explicit Desktop-owned Launcher/taskbar/window controllers replacing bridge
  macros and translation-unit global state;
- selective redraw and reduced terminal flicker;
- typed configuration and session persistence;
- packaging/release evidence tooling;
- stable extension/plugin ABI only after ownership contracts freeze.

### File Manager

- delete/trash with recovery policy;
- copy and move with overwrite/cross-device semantics;
- recursive operations with progress, cancellation, and partial-failure reports;
- previews and bookmarks;
- optional dual-pane workflow;
- broader file-kind handling.

### Notepad

- rebuild native responsive menu chrome;
- Replace and Replace All;
- native system clipboard service;
- mouse cursor placement, drag selection, and wheel scrolling;
- normalization policy and broader Unicode width/grapheme testing;
- syntax highlighting, tabs, multiple encodings;
- large-file architecture.

### Terminal/subprocess

The service tick is only the scheduler boundary. A real terminal port still
requires:

1. backend-neutral terminal screen buffer;
2. incremental ANSI/CSI parser with invalid/fragmented-input tests;
3. scrollback and selection contracts;
4. PTY/subprocess adapter per platform;
5. bounded service integration and backpressure;
6. rendering through DrawList only;
7. shutdown/child cleanup policy.

Do not relabel Diagnostics as Terminal before those contracts exist.

### Platform maturity

- exact-candidate manual Linux curses/raw-TTY/`TERM=linux` evidence;
- native Windows/PDCurses workflow acceptance;
- broader reduced-DOS interaction evidence;
- macOS build/test/interactive evidence or explicit omission;
- complete release artifacts and compatibility record.

## Intentional Non-Parity

RetroDesk should not reproduce reference behavior that violates its contracts.
Intentional differences include:

- no direct native filesystem access from apps;
- no nested modal/menu loops;
- no direct curses rendering in apps/widgets;
- no hidden global Desktop owner;
- no silent stale-file overwrite;
- no unsupported platform feature presented as “best effort”;
- no byte-oriented editing that can split UTF-8;
- no plugin ABI before lifecycle/ownership freeze.

## Port Strategy

1. Specify behavior and failure policy first.
2. Place OS/backend details behind adapters or capabilities.
3. Preserve keyboard-only operation.
4. Implement a pure lower-level contract before UI integration where possible.
5. Add lower-level and end-to-end regressions.
6. Validate Linux, Windows, and DOS build paths when portable code is touched.
7. Record PR-only behavior separately until merged.
8. Update status, gap, roadmap, and known-issue docs together.

## Risk Patterns to Avoid

- stacked stale PRs treated as product state;
- Desktop integration expanded through more macro bridges indefinitely;
- platform build success mistaken for interactive product support;
- direct POSIX assumptions in app/UI code;
- unbounded service callbacks;
- terminal parser bytes leaking into application text;
- data-destructive file features without recovery/cancellation policy;
- Unicode claims broader than tested codepoint/display-cell behavior;
- parity work that bypasses app close, storage version, or cleanup contracts.

See [PROJECT_STATUS.md](PROJECT_STATUS.md), [KNOWN_ISSUES.md](KNOWN_ISSUES.md),
and [ROADMAP.md](ROADMAP.md) for current priorities.