# RetroDesk Documentation Index

## Start Here

RetroDesk documentation separates current facts from future plans:

1. [PROJECT_STATUS.md](PROJECT_STATUS.md) — canonical snapshot of what is in
   `main`, what is only in open PRs, current evidence, and next priorities.
2. [KNOWN_ISSUES.md](KNOWN_ISSUES.md) — release blockers, active engineering
   risks, platform limits, and accepted temporary debt.
3. [README.md](../README.md) — user-facing overview, build, usage, and limits.
4. [ROADMAP.md](ROADMAP.md) — milestone sequencing and unfinished work.

When documents disagree about project state, `PROJECT_STATUS.md` wins until the
contradiction is corrected.

## Vision

RetroDesk is a portable terminal desktop runtime in C11 that hosts multiple apps
with predictable ownership, input, rendering, storage, and lifecycle behavior.
It preserves one Desktop event loop and one frame-flush point while supporting
curses/PDCurses, ANSI/raw-TTY bring-up, native platform storage adapters, and
keyboard-first degradation.

RetroTUI is a feature reference and risk catalog. ArmoniOS contributed the
behavior model for the interactive taskbar. RetroDesk retains its own layered C
architecture and test contracts.

## Current State Summary

Integrated on `main`:

- explicit `core`, `wm`, `app`, `render`, `platform`, `ui`, `apps`, and `storage`
  layers;
- one event loop, one frame flush, transactional shutdown, and Desktop-owned
  clipboard;
- optional bounded app services for future asynchronous workloads;
- themed taskbar and Start-menu-style Launcher;
- taskbar minimize/restore;
- `F8` maximize/unmaximize and title-bar double click;
- `F9` move/resize HUD with live geometry;
- File Manager navigation, text open, rename, mkdir, and new file;
- UTF-8 Notepad selection, clipboard, undo/redo, Find, wrap, safe close, and
  versioned save;
- native POSIX, Win32, and DJGPP storage adapters;
- Linux, Windows, sanitizer, and native DOS/DOSBox-X development gates.

Not integrated:

- PR #27 collection-growth and `WindowId` boundary hardening;
- PR #24 responsive Notepad File/Edit/View menu chrome;
- taskbar overflow, Desktop decomposition, selective redraw, configuration,
  persistence, packaging, PTY terminal, destructive File Manager workflows, and
  broader Notepad features.

The project remains pre-alpha. Automated development matrices are strong, but no
release candidate has a complete evidence bundle tied to its exact commit.

## Recommended Technical Reading Order

1. [FOUNDATION_PRINCIPLES.md](FOUNDATION_PRINCIPLES.md)
2. [PROJECT_STATUS.md](PROJECT_STATUS.md)
3. [KNOWN_ISSUES.md](KNOWN_ISSUES.md)
4. [ARCHITECTURE.md](ARCHITECTURE.md)
5. [APP_RUNTIME.md](APP_RUNTIME.md)
6. [BUILTIN_APPS.md](BUILTIN_APPS.md)
7. [KEYBOARD_SHORTCUTS.md](KEYBOARD_SHORTCUTS.md)
8. [STORAGE.md](STORAGE.md)
9. [PORTABILITY.md](PORTABILITY.md)
10. [BUILD_SYSTEM.md](BUILD_SYSTEM.md)
11. [TESTING.md](TESTING.md)
12. [RETROCORE_SPEC.md](RETROCORE_SPEC.md)
13. [RETROTUI_GAP.md](RETROTUI_GAP.md)
14. [CORE_HARDENING_PLAN.md](CORE_HARDENING_PLAN.md)
15. [TECH_DEBT_POLICY.md](TECH_DEBT_POLICY.md)
16. [CODE_STANDARDS.md](CODE_STANDARDS.md)
17. [ROADMAP.md](ROADMAP.md)
18. [RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md)

Architecture decisions:

- [ADR-0001: CMake is canonical](adr/ADR-0001-cmake-is-canonical.md)
- [ADR-0002: tiered platform support](adr/ADR-0002-tiered-platform-support.md)
- [ADR-0003: core does not expose curses](adr/ADR-0003-core-does-not-expose-curses.md)

## Document Responsibilities

| Document | Authority |
| --- | --- |
| `PROJECT_STATUS.md` | Current integrated state, evidence, open PR distinction, immediate ordering |
| `KNOWN_ISSUES.md` | Active defects, risks, limitations, and accepted debt |
| `ARCHITECTURE.md` | Runtime contracts, ownership, dependencies, and event/render flow |
| `BUILTIN_APPS.md` | Current app workflows and exclusions |
| `KEYBOARD_SHORTCUTS.md` | Input routing implemented on `main` |
| `STORAGE.md` | Cross-platform filesystem and text guarantees |
| `PORTABILITY.md` | Honest platform/backend support claims |
| `TESTING.md` | Automated and manual evidence requirements |
| `ROADMAP.md` | Future sequence and milestone exit criteria |
| `RELEASE_0.1_CHECKLIST.md` | Exact release sign-off record |
| `CORE_HARDENING_PLAN.md` | Core-specific hardening backlog and completion status |
| `TECH_DEBT_POLICY.md` | Rules for accepting and retiring structural debt |

## Platform Support Summary

| Profile | Status |
| --- | --- |
| Linux/ncurses | Primary development/product profile; pre-alpha |
| Linux raw TTY/ANSI | Experimental fallback; broader manual evidence pending |
| Windows/PDCurses | Strict build/tests and native Win32 storage integrated; interactive maturity pending |
| DOS/DJGPP/PDCurses | Validated reduced native profile with storage contract and DOSBox-X smoke |
| macOS/curses | Experimental and currently unvalidated |

The exact matrix and caveats are maintained in
[PORTABILITY.md](PORTABILITY.md).

## Documentation Policy

- Describe a feature as integrated only when it is present in `main`.
- Identify PR-only behavior explicitly, including whether the branch is stale.
- Do not convert automated build evidence into an interactive product claim.
- Tie release claims to one exact candidate SHA.
- Update public behavior, status, roadmap, and known-issue documents in the same
  PR when project state materially changes.
- Preserve historical ADRs unless the decision itself is superseded by a new ADR.
- Never remove a known risk merely because a proposed fix exists in an unmerged
  branch.