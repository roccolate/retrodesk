# RetroDesk Documentation Index

## Vision

RetroDesk is a portable terminal desktop runtime in C11 that hosts multiple apps
with predictable ownership, input, rendering, storage, and lifecycle behavior.

`RetroTUI` is a feature reference and risk catalog. ArmoniOS contributed the
behavior model for the interactive taskbar. RetroDesk keeps its own layered C
architecture.

## Current State Summary

- The runtime uses explicit `core`, `wm`, `app`, `render`, `platform`, `ui`,
  `apps`, and `storage` layers.
- Build is CMake-first; Make wrappers exist for ergonomics and DOS source-list
  specialization.
- Linux/POSIX is the complete File Manager and Notepad product slice.
- The bottom status widget now operates as an interactive taskbar.
- File Manager supports navigation, scrolling, refresh, hidden files, text-file
  open, rename, new directory, and new file.
- Notepad supports UTF-8 editing, selection, shared clipboard, undo/redo, Find,
  soft Word Wrap, atomic save, Save As, and Save/Discard/Cancel close handling.
- Windows Debug/Release builds validate portable runtime code, but equivalent
  file-app support still requires native storage or explicit gating.
- Diagnostics remains runtime information only; it is not a shell or PTY.
- The project remains pre-alpha until release evidence and interactive smoke
  gates are rebuilt on an exact candidate commit.

## Design Principles

- Single main event loop.
- Single frame flush point.
- No `curses` types in domain/public headers.
- Explicit ownership and lifecycle for all resources.
- Capability-based behavior per backend/platform.
- UTF-8 byte offsets internally, normalized to codepoint boundaries and
  converted to display cells for rendering.
- No nested modal loops.

## Recommended Reading Order

1. [FOUNDATION_PRINCIPLES.md](FOUNDATION_PRINCIPLES.md)
2. [ARCHITECTURE.md](ARCHITECTURE.md)
3. [BUILTIN_APPS.md](BUILTIN_APPS.md)
4. [KEYBOARD_SHORTCUTS.md](KEYBOARD_SHORTCUTS.md)
5. [PORTABILITY.md](PORTABILITY.md)
6. [BUILD_SYSTEM.md](BUILD_SYSTEM.md)
7. [APP_RUNTIME.md](APP_RUNTIME.md)
8. [STORAGE.md](STORAGE.md)
9. [RETROCORE_SPEC.md](RETROCORE_SPEC.md)
10. [RETROTUI_GAP.md](RETROTUI_GAP.md)
11. [TESTING.md](TESTING.md)
12. [CODE_STANDARDS.md](CODE_STANDARDS.md)
13. [ROADMAP.md](ROADMAP.md)
14. [RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md)
15. [TECH_DEBT_POLICY.md](TECH_DEBT_POLICY.md)
16. [adr/ADR-0001-cmake-is-canonical.md](adr/ADR-0001-cmake-is-canonical.md)
17. [adr/ADR-0002-tiered-platform-support.md](adr/ADR-0002-tiered-platform-support.md)
18. [adr/ADR-0003-core-does-not-expose-curses.md](adr/ADR-0003-core-does-not-expose-curses.md)
19. [CORE_HARDENING_PLAN.md](CORE_HARDENING_PLAN.md)

## Platform Support Tiers

| Tier | Platforms | Status |
| --- | --- | --- |
| Active product slice | Linux/POSIX | Complete current File Manager/Notepad workflow and primary runtime validation |
| Planned Tier 1 | Windows | Portable runtime builds/tests pass; native storage or app gating still required |
| Experimental | macOS, DOS (DJGPP) | Compile/bring-up or reduced profiles with explicit limits |

## Documentation Policy

`README.md` is the user-facing overview. Technical contracts and platform caveats
live under `docs/`. Every behavior change that affects input, storage, app
lifecycle, rendering, or platform claims must update the relevant document and
add regression coverage.
