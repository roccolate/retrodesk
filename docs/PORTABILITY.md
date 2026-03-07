# Portability Policy

## Support Tiers

- Tier 1: Windows, Linux.
- Tier 2: macOS, DOS (DJGPP).

## Backend Matrix

- Windows: PDCurses backend.
- Linux/macOS: `ncurses` input + curses renderer by default.
- Linux/macOS (experimental): `tty-raw` input + ANSI renderer.
- DOS: DJGPP + PDCurses profile (reduced capabilities).

## Required Capabilities

- Keyboard input.
- Basic window rendering.
- Focus management.
- Deterministic shutdown/cleanup.

## Optional Capabilities

- Mouse input.
- Drag reliability.
- Right-click and double-click.
- Resize events.
- Extended unicode behavior.

## Linux Virtual Console Policy (`TERM=linux`)

- Keyboard-first is mandatory.
- Mouse is optional and only enabled if backend support is reliable.
- Drag/right-click are disabled unless capability is explicitly proven.
- UI must remain fully operable by keyboard when mouse features are off.

## Degradation Policy

Unsupported features must be disabled with deterministic fallback behavior.
No partial "maybe works" feature path is allowed.

## DOS Constraints

- Reduced memory and feature profile.
- No assumption of advanced mouse semantics.
- No expectation of parity with Tier 1 feature set.
