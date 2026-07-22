# Project Status

> Canonical engineering snapshot: 2026-07-22  
> Source-of-truth branch: `main`  
> Source-of-truth commit at the time of this snapshot:
> `c36ceb92b269c4c485798eed2b8413ea6c096164`

This document answers one question: **what is actually in RetroDesk today?**

A feature is listed as integrated only when it is present on `main`. Work in an
open pull request is listed separately, even when that branch has passed CI. A
platform is described as release-supported only when the exact release candidate
has current automated and interactive evidence for the claimed profile.

## Executive Summary

RetroDesk is a pre-alpha C11 terminal desktop runtime with a real multi-window
model, built-in file applications, native storage adapters for POSIX, Win32, and
DJGPP, curses/PDCurses and ANSI rendering paths, and automated Linux, Windows,
and DOS validation.

The current `main` supports practical File Manager and Notepad workflows, an
interactive themed taskbar and Launcher, taskbar minimize/restore,
maximize/unmaximize, and keyboard move/resize feedback. It is no longer merely a
window-manager prototype.

The project is **not yet a release candidate** because:

- release evidence has not been rebuilt and attached to one exact candidate SHA;
- broad manual curses, ANSI/raw-TTY, Windows/PDCurses, and reduced-DOS visual and
  interaction evidence is incomplete;
- two useful older PRs (#27 and #24) are not integrated and are based on stale
  history;
- taskbar congestion, selective redraw, Desktop decomposition, configuration,
  persistence, packaging, and several app expansions remain unfinished.

## Integrated Runtime and Ownership

- One Desktop event loop owns app services, platform polling, global routing,
  cleanup, status updates, and rendering.
- One renderer flush occurs at the end of each dirty frame.
- Desktop owns the app registry, clipboard, renderer, Window Manager, taskbar,
  overlay draw list, running-app table, and shutdown coordination.
- App descriptors own lifecycle behavior; app instances own private state.
- App create failure invokes matching descriptor destroy rollback.
- Global shutdown is transactional: dirty apps may Save, Discard, or Cancel
  without partially destroying earlier apps.
- Public/domain headers avoid curses/PDCurses and OS-native filesystem types.

## Integrated App Service Contract

- Descriptors may expose optional `on_service`.
- Desktop invokes active services once per ordinary loop iteration.
- Each callback receives an 8 KiB work budget.
- Services may report idle or request redraw.
- Services may not poll platform input, render, flush, create nested UI loops, or
  block indefinitely.
- Any active service caps platform polling at 16 ms.

This is the scheduling foundation for future PTY/network/subprocess apps. No real
terminal emulator or PTY app is integrated.

## Integrated Window and Desktop Behavior

- Window ownership, geometry, z-order, focus, modal routing, close, keyboard
  move/resize, and capability-gated mouse drag.
- Deterministic `F6` focus cycling.
- Minimized windows stay allocated but leave render, keyboard, mouse hit testing,
  drag, modal routing, and focus cycling.
- Clicking the focused taskbar app minimizes its active window.
- Clicking a minimized app restores, focuses, and raises it.
- `F8` or active title-bar double click toggles maximize/unmaximize.
- Maximization preserves exact restore geometry, occupies the work area above the
  taskbar, survives minimize/restore, and refreshes after terminal resize.
- Fixed and modal windows reject minimize/maximize.
- `F9` enters MOVE; `Tab` toggles RESIZE; arrows adjust geometry; `Enter`, `F9`,
  or `Esc` finish.
- Move/resize mode uses a themed frame and responsive live-geometry HUD.
- Focus change cancels an active keyboard window operation.

## Integrated Taskbar and Launcher

The old diagnostic status row has been replaced by a themed desktop surface:

- separate Apps, application, separator, and clock regions;
- deterministic full and compact labels;
- idle, running, focused, menu-open, and clock styles;
- multiple-instance counts;
- exact mouse hit regions matching rendered widths;
- XP, Hacker, Amiga, and Win31 surface tokens;
- bottom-left Start-menu-style Launcher above the taskbar;
- Applications and Desktop sections;
- arrows, `W/S/J/K`, Home/End, Enter/Space, Escape, `F10`, and direct
  accelerators;
- mouse row selection and activation.

Current limitation: the catalog remains fixed to Files, Notepad, and Diagnostics.
There is no overflow surface for a future larger catalog.

## Integrated File Manager

- bounded deterministic listing;
- directories before regular files;
- viewport scrolling and retained selection;
- arrows, `J/K`, Page Up/Down, Home/End;
- parent navigation, refresh, hidden-file toggle;
- open directories and regular validated text files;
- launch text files in a separate Notepad instance;
- file-size and empty-directory display;
- create empty files and directories;
- rename without intentional overwrite;
- deterministic invalid-name, duplicate-target, and cancellation behavior.

Not integrated: delete/trash, copy, move, recursive operations, progress/cancel,
previews, bookmarks, dual pane, or arbitrary binary viewers.

## Integrated Notepad

- validated UTF-8 open/save;
- codepoint-safe editing;
- display-cell-aware rendering/navigation;
- multiline Shift selection;
- Desktop-owned clipboard shared among instances;
- Select All, Copy, Cut, Paste, and selection replacement;
- bounded snapshot undo/redo: 100 states and 1 MiB per stack;
- dirty baseline relative to last successful save;
- Find with wraparound and byte-accurate ranges;
- limited ASCII/Latin case folding with accents significant;
- soft Word Wrap without logical-text mutation;
- Save/Save As, optimistic-concurrency conflict, and recovery behavior;
- Save/Discard/Cancel dirty close;
- multiple instances and taskbar cycling.

Not integrated on `main`: native File/Edit/View menu chrome, `F11`, `Ctrl+N`,
`Ctrl+O`, responsive editor status chrome, Replace/Replace All, native system
clipboard, mouse editing, wheel scrolling, normalization policy, syntax
highlighting, multiple encodings, or large-file strategy.

## Diagnostics

Diagnostics reports runtime, backend, capability, and geometry information. It is
not a shell, terminal emulator, PTY frontend, or subprocess manager.

## Storage Status

All file apps use `retro_fs`.

| Adapter | Integrated capabilities | Important limits |
| --- | --- | --- |
| POSIX | Path/stat/version, bounded listing, UTF-8 text, exclusive create, mkdir, no-overwrite rename, temporary-file replacement, stale-version conflict | Links/unsupported targets rejected for text; filesystem determines final crash-durability semantics |
| Win32 | UTF-8 public/UTF-16 native boundary, drive/UNC/extended paths, deterministic listing, Unicode filenames/data, native identity/version, create/mkdir/rename, Win32 replacement | Reparse points rejected for text; interactive PDCurses evidence incomplete |
| DJGPP/DOS | Native path/stat/list/read/write/create/mkdir/rename, case-insensitive ordering, explicit parent, content fingerprint, native DOSBox-X contract | ASCII/DOS-oriented names, coarse FAT times, backup/restore replacement rather than universal true atomic replace |
| macOS | Intended POSIX path | No current evidence; experimental/unclaimed |

The common text contract accepts valid UTF-8 and consistent LF or CRLF. It
rejects malformed UTF-8, NUL, ESC, unsupported controls, mixed newline styles,
oversized content, and unsupported targets.

## Platform Confidence Matrix

| Profile | Automated evidence | Storage | Interactive evidence | Current claim |
| --- | --- | --- | --- | --- |
| Linux + ncurses | Strict Debug/Release, static analysis, sanitizers, tests, smoke | POSIX | Exact-candidate full matrix incomplete | Primary pre-alpha product profile |
| Linux raw TTY + ANSI | Parser/render/integration tests | POSIX | Broader terminal matrix incomplete | Experimental fallback |
| Windows + PDCurses | Debug/Release `/W4 /WX`, native storage/app tests | Win32 | Current native workflow record incomplete | Validated pre-alpha build/test profile |
| DOS + DJGPP/PDCurses | Pinned build, `FSTEST.EXE`, DOSBox-X diagnostic, artifact | DJGPP | Automated emulator smoke; broader UX incomplete | Validated reduced profile |
| macOS + curses | None current | Intended POSIX | None | Experimental/unclaimed |

## Development Validation Checkpoint

The compact PR #36 head passed:

- Linux oracle guard, static analysis, Debug, Release, sanitizers, manifest
  comparison, source guard, and non-interactive smoke;
- Windows Debug and Release under strict warnings;
- pinned DJGPP build;
- native DOS storage contract and RetroDesk diagnostic in DOSBox-X;
- DOS distribution artifact generation.

Official development runs:

- CI `29900244919`;
- DOS `29900244867`;
- tested head `f2e9e83ea2b6f2c55ad6ec9c4e9c414efe103985`.

The repository history now identifies `c36ceb9...` as the current `main`
checkpoint for the same latest desktop slice. Because evidence is not attached to
that exact SHA, the release gate must be rerun on the final candidate.

## Open Pull Requests Not in `main`

### PR #27 — collection growth and `WindowId` exhaustion

Open and historically green, but based on older `main` and not safely mergeable
without rebuilding.

Proposed:

- checked capacity arithmetic;
- deterministic overflow/allocation failure;
- Desktop/WM state preservation after failed growth;
- positive monotonic `WindowId` values with deterministic exhaustion at
  `INT_MAX`.

Treat as P0/P1 proposed hardening, not integrated behavior.

### PR #24 — native responsive Notepad chrome

Open draft stacked on superseded history.

Proposed:

- File/Edit/View menus;
- `F11` and reliable Alt mnemonics;
- `Ctrl+N`/`Ctrl+O` separate windows;
- responsive viewport/status using hosted dimensions.

Rebuild after deciding #27 and reconciling current Desktop/window bridge changes.

## Milestone Position

- `v0.1.0`: no tag; checklist reopened.
- `v0.2.0`: most CI/documentation/portability foundations exist; exact candidate
  evidence remains.
- `v0.3.0`: useful File Manager and Notepad core workflows integrated.
- `v0.4.0`: Win32 and DJGPP native storage/build paths integrated; manual
  platform maturity incomplete.
- `v0.5.0`: taskbar, Launcher, theme surfaces, minimize/restore,
  maximize/unmaximize, and move/resize feedback integrated. Remaining:
  congestion/overflow, visual matrix, stronger narrow tests, redraw/flicker.

No milestone is shipped merely because implementation exists.

## Recommended Next Order

1. Merge this documentation baseline and keep it synchronized.
2. Rebuild PR #27 on current `main`; merge or explicitly defer.
3. Rebuild PR #24 on the resulting history.
4. Finish taskbar congestion/overflow and narrow behavior.
5. Record the complete theme/backend/platform manual smoke matrix.
6. Decompose `desktop.c` and retire translation-unit bridge macros/state.
7. Add selective redraw/status caching before app-catalog expansion.
8. Choose an exact release candidate and execute the reopened checklist.

## Source-of-Truth Rule

Use this document for status. Use:

- [ARCHITECTURE.md](ARCHITECTURE.md) for contracts;
- [KNOWN_ISSUES.md](KNOWN_ISSUES.md) for active risks;
- [ROADMAP.md](ROADMAP.md) for sequencing;
- [TESTING.md](TESTING.md) for evidence;
- [RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md) for sign-off.

Update this snapshot in the same PR whenever integrated project state changes.