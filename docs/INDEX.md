# RetroDesk Documentation Index

## Vision

RetroDesk aims to be a portable terminal desktop runtime in C11 that can host multiple
apps with predictable behavior across platforms.

`RetroTUI` is treated as a feature reference and risk catalog, not an architecture guide.

## Current State Summary

- The runtime is being moved to explicit layers (`core`, `wm`, `app`, `render`, `platform`).
- Build is CMake-first; Make wrappers exist for ergonomics and DOS specialization.
- Linux virtual console support is keyboard-first by policy.

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
6. [RETROTUI_GAP.md](RETROTUI_GAP.md)
7. [TESTING.md](TESTING.md)
8. [ROADMAP.md](ROADMAP.md)
9. [RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md)
10. [TECH_DEBT_POLICY.md](TECH_DEBT_POLICY.md)
11. [adr/ADR-0001-cmake-is-canonical.md](adr/ADR-0001-cmake-is-canonical.md)
12. [adr/ADR-0002-tiered-platform-support.md](adr/ADR-0002-tiered-platform-support.md)
13. [adr/ADR-0003-core-does-not-expose-curses.md](adr/ADR-0003-core-does-not-expose-curses.md)

## Platform Support Tiers

| Tier | Platforms | Status |
| --- | --- | --- |
| Tier 1 | Windows, Linux | Primary stability and regression target |
| Tier 2 | macOS, DOS (DJGPP) | Maintained with explicit capability limits |

## README Policy

`README.md` is intentionally short. Detailed technical policy lives under `docs/`.
