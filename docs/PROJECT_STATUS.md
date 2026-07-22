# Project Status

> Canonical engineering snapshot: 2026-07-22  
> Source-of-truth branch: `main`  
> Source-of-truth commit at the time of this snapshot:
> `31273f8c888b6074f416a938847e3124b88f9464`

This document answers one question: **what is actually in RetroDesk today?**

A feature is listed as integrated only when it is present on `main`. Work in an
open pull request is listed separately, even when that branch has passed CI. A
platform is described as release-supported only when the exact release candidate
has current automated and interactive evidence for the claimed profile.

## Executive Summary

RetroDesk is a pre-alpha, C11 terminal desktop runtime with a real multi-window
model, built-in file applications, native storage adapters for POSIX, Win32, and
DJGPP, two rendering paths, and automated Linux, Windows, and DOS validation.
It is no longer merely a window-manager prototype: the current `main` supports
practical File Manager and Notepad workflows, an interactive themed taskbar and
Launcher, minimize/restore, maximize/unmaximize, and keyboard move/resize
feedback.

The project is **not yet a release candidate**. The main remaining reasons are:

- release evidence has not been rebuilt and attached to one exact candidate
  commit;
- broad manual curses, ANSI/raw-TTY, Windows/PDCurses, and reduced-DOS visual and
  interaction smoke evidence is incomplete;
- two useful older PRs (#27 and #24) are not integrated and are based on stale
  history;
- taskbar congestion, selective redraw, Desktop decomposition, configuration,
  persistence, packaging, and several app expansions remain unfinished.

## What Is Integrated on `main`

### Runtime and ownership

- One Desktop event loop owns service dispatch, platform polling, event routing,
  cleanup, status updates, and rendering.
- One renderer flush occurs at the end of a dirty frame.
- Desktop owns the app registry, clipboard service, renderer, Window Manager,
  taskbar, overlay draw list, running-app table, and shutdown coordination.
- App descriptors own lifecycle behavior; app instances own private state.
- Global shutdown is transactional: dirty apps may Save, Discard, or Cancel
  without destroying earlier apps prematurely.
- App creation failure invokes the descriptor's matching destroy path.
- Public domain headers avoid curses/PDCurses and OS-native filesystem types.

### App service contract

- Descriptors may expose an optional `on_service` callback.
- Desktop invokes each active service once per ordinary loop iteration.
- Each callback receives an 8 KiB work budget.
- A service may report idle or request a redraw; it may not poll platform input,
  render directly, flush, create another event loop, or block indefinitely.
- While any service is active, platform polling is capped at 16 ms.

This contract is ready for future PTY, network, or subprocess-backed apps, but no
real terminal emulator or PTY app is integrated yet.

### Window Manager and desktop interaction

Integrated behavior includes:

- window ownership, z-order, focus, modal routing, close, keyboard movement,
  keyboard resizing, and capability-gated mouse drag;
- deterministic focus cycling with `F6`;
- minimized windows remain allocated but are excluded from rendering, keyboard,
  mouse hit testing, drag, modal routing, and focus cycling;
- clicking the focused taskbar app minimizes its active window;
- clicking a minimized app restores, focuses, and raises it;
- `F8` or a title-bar double click toggles maximize/unmaximize;
- maximization preserves exact restore geometry, occupies the work area above the
  taskbar, survives minimize/restore, and refreshes after terminal resize;
- fixed and modal windows reject minimize/maximize;
- `F9` enters keyboard move mode, `Tab` toggles resize, arrows adjust geometry,
  and `Enter`, `F9`, or `Esc` finish;
- move/resize mode shows a themed frame and a responsive HUD with live geometry;
- changing focus cancels an active keyboard window operation.

### Taskbar and Launcher

The old diagnostic-looking status row has been replaced by a desktop surface:

- separate Apps, application, separator, and clock regions;
- full and compact application labels selected deterministically by available
  width;
- distinct idle, running, focused, menu-open, and clock styles;
- multiple-instance counts;
- exact mouse hit regions matching rendered buttons;
- theme-specific surface tokens for XP, Hacker, Amiga, and Win31;
- bottom-left Start-menu-style Launcher above the taskbar;
- Launcher sections for Applications and Desktop actions;
- keyboard navigation with arrows, `W/S/J/K`, Home/End, Enter/Space, Escape,
  `F10`, and direct accelerators;
- mouse row selection and activation.

Current limitation: the catalog is still a fixed Files/Notepad/Diagnostics set and
there is no overflow menu when a future, larger catalog cannot fit.

### File Manager

Integrated workflows:

- bounded, deterministic directory listing;
- directories before regular files;
- viewport scrolling and retained selection;
- arrows, `J/K`, Page Up/Down, Home/End;
- parent navigation, refresh, hidden-file toggle;
- open directories and regular validated text files;
- launch text files in a separate Notepad instance;
- show file size and empty-directory state;
- create empty files and directories;
- rename without intentional overwrite;
- deterministic handling of invalid names, duplicate targets, and cancellation.

Not integrated: delete/trash, copy, move, recursive operations, progress/cancel,
previews, bookmarks, dual-pane mode, or arbitrary binary viewers.

### Notepad

Integrated workflows:

- validated UTF-8 open and save;
- codepoint-safe cursor movement, insertion, Backspace, and Delete;
- display-cell-aware rendering and vertical navigation;
- multiline selection with Shift+arrows;
- Desktop-owned in-process clipboard shared among Notepad instances;
- Select All, Copy, Cut, Paste, and selection replacement;
- bounded snapshot undo/redo: 100 states and 1 MiB per stack;
- dirty state relative to the last successful save;
- Find with wraparound and byte-accurate selection ranges;
- ASCII/Latin case folding while preserving accent significance;
- soft Word Wrap that does not rewrite logical lines or saved bytes;
- Save, Save As, optimistic-concurrency conflicts, and recovery behavior;
- Save/Discard/Cancel dirty close flow;
- multiple instances and taskbar cycling.

Not integrated on `main`: native File/Edit/View menu chrome, `F11` menus,
`Ctrl+N`, `Ctrl+O`, responsive editor status chrome, Replace/Replace All, native
system clipboard, mouse cursor placement, drag selection, wheel scrolling,
normalization policy, syntax highlighting, multiple encodings, or a large-file
strategy.

### Diagnostics

Diagnostics displays runtime, backend, capability, and geometry information. It
is intentionally not a shell, terminal emulator, or PTY frontend.

## Storage Status

All file applications use the platform-neutral `retro_fs` contract.

| Adapter | Integrated capabilities | Important limits |
| --- | --- | --- |
| POSIX | Path helpers, stat/version, bounded listing, UTF-8 text read/write, exclusive create, mkdir, no-overwrite rename, temporary-file atomic replace, stale-version conflict | Symlinks and unsupported targets are rejected for text editing; terminal profile still requires manual evidence |
| Win32 | UTF-8 public API with UTF-16 native boundary, drive/UNC and extended paths, deterministic listing, Unicode filenames/data, identity/version tokens, create/mkdir/rename, atomic replace through Win32 APIs | Reparse points are rejected for text operations; interactive PDCurses product evidence is not yet complete |
| DJGPP/DOS | Native path/stat/list/read/write/create/mkdir/rename, deterministic case-insensitive ordering, explicit parent entry, content fingerprint in version token, DOSBox-X contract test | Filename/path boundary is ASCII/DOS-oriented; FAT timestamps are coarse; replacement uses same-directory 8.3 temp plus backup/restore, not a true OS atomic-replace syscall |
| macOS | Expected to use POSIX adapter | Current build and interactive evidence has not been recorded; profile remains experimental |

The common text contract accepts valid UTF-8 and consistent LF or CRLF. It
rejects malformed UTF-8, embedded NUL, ESC, unsupported controls, mixed newline
styles, oversized content, and unsupported target types.

## Platform Confidence Matrix

| Profile | Build/test evidence | Storage | Interactive evidence | Current claim |
| --- | --- | --- | --- | --- |
| Linux + ncurses | Strict Debug/Release, static analysis, sanitizers, tests, non-interactive smoke | Native POSIX | Some historical use; full candidate matrix still required | Primary development/product profile, pre-alpha |
| Linux raw TTY + ANSI | Parser/render tests and non-interactive paths | Native POSIX | Broader representative-terminal smoke incomplete | Experimental fallback |
| Windows + PDCurses | Debug and Release build/tests under `/W4 /WX` | Native Win32 | Current interactive native-file workflow evidence incomplete | Validated build/test profile, pre-alpha |
| DOS + DJGPP/PDCurses | Pinned cross-build, native `FSTEST.EXE`, application diagnostic in DOSBox-X, artifact generation | Native DJGPP | Automated DOSBox-X smoke; broader manual UX evidence incomplete | Validated reduced profile, not Tier 1 |
| macOS + curses | No current evidence attached to this snapshot | POSIX design path | Not recorded | Experimental/unclaimed |

## Automated Validation at the Current UI Checkpoint

The final PR head for the latest integrated desktop slice passed:

- Linux test-oracle guard;
- Linux configure and static analysis;
- Linux Debug build and complete tests;
- Linux Release build and complete tests;
- AddressSanitizer and UndefinedBehaviorSanitizer tests with leak checks where
  supported;
- Debug/Release test-manifest comparison;
- DJGPP source-manifest guard;
- non-interactive startup smoke;
- Windows Debug and Release build/tests under strict warnings;
- pinned DJGPP build;
- native DOS storage contract in DOSBox-X;
- application diagnostic smoke in DOSBox-X;
- DOS distribution artifact generation.

Official runs for the compact PR #36 head:

- CI: `29900244919`;
- DOS: `29900244867`.

These runs validate the PR head `f2e9e83...`; the squash merge produced
`31273f8...`. They are strong development evidence, but a release must rerun and
attach evidence to the exact candidate commit.

## Open Pull Requests Not in `main`

### PR #27 — collection growth and `WindowId` exhaustion

Status: open, historically green, now based on an older `main` and not mergeable
without rebuilding.

Proposed behavior:

- checked element-capacity growth before byte-size multiplication;
- deterministic failure on overflow or allocation failure;
- preservation of existing Desktop/WM state after failed growth;
- positive monotonic `WindowId` values with deterministic exhaustion at
  `INT_MAX`.

This remains important P0/P1 hardening, but none of it should be described as
integrated until a fresh branch is rebuilt on current `main` and revalidated.

### PR #24 — native responsive Notepad chrome

Status: open draft, stacked on a superseded historical hardening branch.

Proposed behavior:

- shared native File/Edit/View menu widget;
- `F11` application menu activation and reliable Alt mnemonics;
- `Ctrl+N` and `Ctrl+O` opening separate Notepad windows;
- responsive viewport and status row using hosted window dimensions.

This functionality is not in `main`. The PR must be rebuilt after deciding how to
resolve #27 and after reconciling the current desktop/window bridge changes.

## Current Milestone Position

- `v0.1.0`: no tag; release checklist remains reopened.
- `v0.2.0`: most CI, documentation, portability, and storage foundations exist,
  but candidate evidence and documentation consistency are still being closed.
- `v0.3.0`: useful File Manager and Notepad core workflows are integrated.
- `v0.4.0`: native Win32 and DJGPP storage/build paths are integrated; manual
  platform maturity evidence is incomplete.
- `v0.5.0`: taskbar, Launcher, theme surfaces, minimize/restore,
  maximize/unmaximize, and move/resize feedback are integrated. Remaining exit
  work is taskbar congestion, visual/manual smoke, stronger focus/narrow-screen
  tests, and redraw/flicker reduction.

No semantic version milestone should be treated as shipped merely because much
of its implementation exists.

## Recommended Next Order

1. Keep this documentation snapshot synchronized and use it as the status source.
2. Rebuild PR #27 on current `main`; validate and merge or explicitly defer it.
3. Rebuild PR #24 on the resulting `main`; do not merge the stale stack directly.
4. Finish taskbar congestion/overflow and narrow-terminal behavior.
5. Perform and record the complete theme/backend/platform manual smoke matrix.
6. Reduce `desktop.c` concentration and retire translation-unit macro bridges.
7. Address selective redraw/status caching before expanding the app catalog.
8. Select an exact release candidate and execute the reopened release checklist.

## Source-of-Truth Rule

Use this document for project status. Use:

- [ARCHITECTURE.md](ARCHITECTURE.md) for contracts and dependencies;
- [KNOWN_ISSUES.md](KNOWN_ISSUES.md) for active risks and limits;
- [ROADMAP.md](ROADMAP.md) for future sequencing;
- [TESTING.md](TESTING.md) for evidence requirements;
- [RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md) for release sign-off.

When code changes, update the relevant contract document and this status snapshot
in the same PR when the public project state changes.