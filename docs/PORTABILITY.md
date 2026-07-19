# Portability Policy

## Target vs Current Status

ADR-0002 defines the long-term target: Linux and Windows are Tier 1, macOS and
DOS are Tier 2. Current validation status is narrower:

| Profile | Current Status |
| --- | --- |
| Linux | Active development and validation profile |
| Windows | Planned Tier 1; blocked until storage has a native adapter or app gating |
| macOS | Experimental compile/bring-up profile |
| DOS (DJGPP) | Experimental reduced profile with specialized source list |

Documentation must not describe a platform as release-supported unless the
candidate commit has current build, test, and smoke evidence for that profile.

## Backend Matrix

- Linux/macOS default: `ncurses` input with curses renderer.
- Linux/macOS experimental: raw TTY input with ANSI renderer.
- Windows target: PDCurses backend.
- DOS target: DJGPP + PDCurses reduced profile.

## Storage Matrix

- Linux/POSIX: `src/storage/retro_fs_posix.c` provides the current File Manager
  and Notepad storage behavior.
- Windows/macOS/DOS: no release claim unless a native adapter exists or apps
  requiring storage are explicitly disabled by capability/build gating.

Storage interfaces must not silently break non-POSIX profiles. New file-app
features must either stay behind a supported adapter or degrade deterministically.

## Required Capabilities

- Keyboard input.
- Basic window rendering.
- Focus management.
- Deterministic shutdown/cleanup.
- Explicit app launch rejection when required capabilities are absent.

## Optional Capabilities

- Mouse input.
- Drag reliability.
- Right-click and double-click.
- Resize events.
- Extended unicode behavior.
- Filesystem-backed apps.

## Linux Virtual Console Policy (`TERM=linux`)

- Keyboard-first is mandatory.
- Mouse is optional and only enabled if backend support is reliable.
- Drag/right-click are disabled unless capability is explicitly proven.
- UI must remain fully operable by keyboard when mouse features are off.

## Degradation Policy

Unsupported features must be disabled or rejected with deterministic fallback
behavior. No partial "maybe works" feature path is allowed.

## DOS Constraints

- Reduced memory and feature profile.
- No assumption of advanced mouse semantics.
- No raw POSIX TTY or POSIX filesystem assumptions.
- No expectation of parity with Tier 1 feature set.
