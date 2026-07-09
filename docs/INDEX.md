# RetroDesk Documentation Index

## Vision

RetroDesk aims to be a portable terminal desktop runtime in C11 that can host
multiple apps with predictable behavior across platforms.

`RetroTUI` is treated as a feature reference and risk catalog, not an
architecture guide.

## Current State Summary

- The runtime uses explicit layers: `core`, `wm`, `app`, `apps`, `render`,
  `platform`, and `ui`.
- Build is CMake-first; Make wrappers exist for ergonomics and DOS
  specialization.
- The window manager, app runtime, render abstraction, theme catalog, and
  current widget set are implemented as foundation components.
- Built-in apps are currently placeholder workflows wired through the app
  runtime, not finished user-facing productivity apps.
- Linux virtual console support is keyboard-first by policy.
- Windows and Linux are Tier 1 validation targets; macOS and DOS/DJGPP are
  Tier 2 capability-limited targets.

## Design Principles

- Single main event loop.
- Single frame flush point.
- No `curses`/PDCurses backend types in domain/public headers.
- Explicit ownership and lifecycle for all resources.
- Capability-based behavior per backend/platform.
- Tests and docs change with public contracts, build targets, platform policy,
  or source layout.

## Recommended Reading Order

1. [FOUNDATION_PRINCIPLES.md](FOUNDATION_PRINCIPLES.md)
2. [ARCHITECTURE.md](ARCHITECTURE.md)
3. [PORTABILITY.md](PORTABILITY.md)
4. [BUILD_SYSTEM.md](BUILD_SYSTEM.md)
5. [APP_RUNTIME.md](APP_RUNTIME.md)
6. [RETROCORE_SPEC.md](RETROCORE_SPEC.md)
7. [retrocore-spec-integration.md](retrocore-spec-integration.md)
8. [RETROTUI_GAP.md](RETROTUI_GAP.md)
9. [TESTING.md](TESTING.md)
10. [ROADMAP.md](ROADMAP.md)
11. [RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md)
12. [CODE_STANDARDS.md](CODE_STANDARDS.md)
13. [TECH_DEBT_POLICY.md](TECH_DEBT_POLICY.md)
14. [CORE_HARDENING_PLAN.md](CORE_HARDENING_PLAN.md)
15. [AGENTS.md](AGENTS.md)
16. [adr/ADR-0001-cmake-is-canonical.md](adr/ADR-0001-cmake-is-canonical.md)
17. [adr/ADR-0002-tiered-platform-support.md](adr/ADR-0002-tiered-platform-support.md)
18. [adr/ADR-0003-core-does-not-expose-curses.md](adr/ADR-0003-core-does-not-expose-curses.md)

## Platform Support Tiers

| Tier | Platforms | Status |
| --- | --- | --- |
| Tier 1 | Windows, Linux | Primary stability and regression target |
| Tier 2 | macOS, DOS (DJGPP) | Maintained with explicit capability limits |

## README Policy

`README.md` is the user-facing entrypoint: quick overview, build/use commands,
current project structure, and links into the deeper technical docs. Detailed
policy and release gates live under `docs/`.
