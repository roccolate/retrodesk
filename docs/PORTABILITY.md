# Portability Policy

## Target vs Current Status

ADR-0002 defines the long-term target: Linux and Windows are Tier 1, macOS and
DOS are secondary profiles. Current product support remains narrower than the
build matrix.

| Profile | Current status |
| --- | --- |
| Linux/POSIX | Active product slice and primary runtime validation profile |
| Windows/PDCurses | Portable runtime builds and tests pass; native storage or explicit file-app gating is still required |
| macOS | Experimental compile/bring-up profile |
| DOS (DJGPP) | Experimental reduced profile with a synchronized specialized source list |

Documentation must not call a platform release-supported unless the exact
candidate commit has current build, test, and interactive evidence for the
claimed feature set.

## Backend Matrix

- Linux/macOS default: curses input and curses renderer.
- Linux/macOS experimental: raw TTY input with ANSI renderer.
- Windows: PDCurses build/test profile.
- DOS: DJGPP + PDCurses reduced profile.

The normalized event contract carries key code, printable text/codepoint, and
modifier bits. Unsupported terminal sequences degrade deterministically rather
than inventing modifiers.

## Terminal Control-Key Policy

The curses backend uses `cbreak()` rather than permanent `raw()` mode. On POSIX
terminals it selectively releases editor controls from flow control and signal
handling where supported, then restores the exact saved terminal mode during
shutdown or initialization rollback.

This allows Notepad shortcuts such as Save, Copy, Paste, Undo, and Redo to reach
the app without globally disabling all terminal signals.

## Storage Matrix

- Linux/POSIX: `src/storage/retro_fs_posix.c` provides the complete current File
  Manager and Notepad storage workflow.
- Windows/macOS/DOS: no equivalent product claim until a native/portable adapter
  passes the same tests or filesystem-backed apps are gated off explicitly.

Portable runtime compilation alone is not evidence of storage support.

## Unicode Scope

Current portable core behavior includes:

- validated UTF-8 decoding and encoding;
- cursor/selection boundaries on complete codepoints;
- terminal-cell-aware rendering and vertical navigation;
- modified navigation sequences where the backend reports them;
- UTF-8 clipboard and storage validation.

Remaining portability work includes broader terminal-width validation,
normalization policy, additional script/emoji testing, and backend-specific
manual smoke evidence.

## Required Capabilities

- keyboard input;
- basic window rendering;
- focus management;
- deterministic shutdown and cleanup;
- explicit app launch rejection when required capabilities are absent.

## Optional Capabilities

- mouse input and reliable drag;
- right-click and double-click;
- resize events;
- extended Unicode behavior;
- filesystem-backed apps;
- native system clipboard integration.

## Linux Virtual Console Policy (`TERM=linux`)

- Keyboard-first behavior is mandatory.
- Mouse is optional and only enabled when reliable.
- Drag/right-click remain disabled unless proven by the active backend.
- Launcher, taskbar alternatives, File Manager, Notepad, and close workflows
  must remain keyboard-operable.

## Degradation Policy

Unsupported features are disabled or rejected with deterministic fallback. No
partial “maybe works” product path is allowed.

## DOS Constraints

- reduced memory and feature profile;
- no POSIX TTY or filesystem assumptions;
- no requirement for advanced mouse semantics;
- no expectation of complete Tier 1 app parity;
- `Makefile.djgpp` must remain synchronized with the canonical source set where
  portable sources apply.
