# RetroDesk Documentation Index

## Vision

RetroDesk aims to be a portable terminal desktop runtime in C11 that can host
multiple apps with predictable behavior across platforms.

`RetroTUI` is treated as a feature reference and risk catalog, not an architecture guide.

## Current State Summary

- The runtime is being moved to explicit layers (`core`, `wm`, `app`,
  `render`, `platform`, `ui`, `apps`, `storage`).
- Build is CMake-first; Make wrappers exist for ergonomics and DOS specialization.
- Linux virtual console support is keyboard-first by policy.
- File Manager and Notepad cover a Linux/POSIX preview path; Diagnostics is
  read-only runtime information, not a shell.
- This documentation does not certify a stable release or native support outside
  validated Linux gates.

## Design Principles

- Single main event loop.
- Single frame flush point.
- No `curses` types in domain/public headers.
- Explicit ownership and lifecycle for all resources.
- Capability-based behavior per backend/platform.

## Recommended Reading Order

1. [FOUNDATION_PRINCIPLES.md](FOUNDATION_PRINCIPLES.md)
2. [ARCHITECTURE.md](ARCHITECTURE.md)
3. [PORTABILITY.md](PORTABILITY.md)
4. [BUILD_SYSTEM.md](BUILD_SYSTEM.md)
5. [APP_RUNTIME.md](APP_RUNTIME.md)
6. [STORAGE.md](STORAGE.md)
7. [RETROCORE_SPEC.md](RETROCORE_SPEC.md)
8. [RETROTUI_GAP.md](RETROTUI_GAP.md)
9. [TESTING.md](TESTING.md)
10. [CODE_STANDARDS.md](CODE_STANDARDS.md)
11. [RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md)
12. [TECH_DEBT_POLICY.md](TECH_DEBT_POLICY.md)
13. [adr/ADR-0001-cmake-is-canonical.md](adr/ADR-0001-cmake-is-canonical.md)
14. [adr/ADR-0002-tiered-platform-support.md](adr/ADR-0002-tiered-platform-support.md)
15. [adr/ADR-0003-core-does-not-expose-curses.md](adr/ADR-0003-core-does-not-expose-curses.md)
16. [CORE_HARDENING_PLAN.md](CORE_HARDENING_PLAN.md)

## Platform Support Tiers

| Tier | Platforms | Status |
| --- | --- | --- |
| Active | Linux | Current development and runtime validation target |
| Planned Tier 1 | Windows | Blocked on native storage support/gating and current CI evidence |
| Experimental | macOS, DOS (DJGPP) | Non-blocking compile/reduced profiles |

## README Policy

`README.md` is intentionally short. Detailed technical policy lives under `docs/`.
