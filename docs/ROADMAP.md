# Roadmap

> Status (2026-07): RetroDesk is pre-alpha. No release tag exists. The current
> working tree contains an in-progress Linux/POSIX product preview for File
> Manager, Notepad, storage, stronger tests, and CI gates, but it is not a
> release candidate until the validation evidence in
> [RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md) is rebuilt on a clean
> candidate commit.

## Current State

- The foundation runtime is layered around `core`, `platform`, `render`, `wm`,
  `app`, `apps`, `ui`, and the new `storage` contract.
- Linux is the active development and validation profile. Native Windows is a
  planned Tier 1 target, but the current storage implementation is POSIX-only.
- File Manager can list directories, navigate with the keyboard, move to the
  parent directory, refresh, and open regular files through Notepad.
- Notepad can edit text, open a resource path, save existing files with version
  conflict detection, start Save As for untitled documents, and block close when
  dirty.
- Diagnostics is intentionally a runtime information app, not a terminal shell
  or PTY.
- Test oracles are always-active through `tests/test_harness.h`; `assert` is no
  longer accepted in tests.

## Release Gates

- **Gate 0 / foundation candidate:** clean Linux strict build, Debug/Release
  test parity, always-active oracle guard, storage/text safety tests,
  reproducible retrocore fixture guard, smoke evidence, and no stale build
  artifacts.
- **v0.2 alpha / current working target:** finish foundation release hygiene,
  resolve POSIX storage portability policy, make CI green on the claimed
  profiles, and tag `v0.1.0` only if the reopened checklist is fully satisfied.
- **v0.3 alpha / Linux product preview:** make File Manager and Notepad useful
  in a keyboard-only Linux curses session, including scrollable directory lists,
  robust dirty-document UX, and focused app behavior tests.
- **v0.4 alpha / portability recovery:** restore native Windows build/test with
  PDCurses and a native storage adapter or explicit feature gating; document
  macOS and DOS as compile/reduced profiles with honest limits.
- **v0.5 beta / WM and input polish:** stabilize focus, z-order, close,
  move/resize, modal routing, ANSI/raw input behavior, and keyboard-first
  fallback paths.
- **v1.0:** ship tested Linux and Windows artifacts with release evidence tied
  to the exact commit, no known critical defects, and no structural debt that
  violates [FOUNDATION_PRINCIPLES.md](FOUNDATION_PRINCIPLES.md).

Features outside v1 (PTY/shell, plugins, Settings, session persistence,
delete/rename/copy/move, undo/redo, clipboard, search, and full Unicode) must
not be documented as implemented or imminent product behavior.

## Milestones

### `v0.1.0`: Foundation Release

Goal: tag the historical foundation only if it is reproducible from a clean
candidate commit.

- Pass `make strict`, `make test`, `make test-all`, `make smoke-ci`, and the
  manual PTY smoke gates where applicable.
- Keep CMake as the canonical modern build and keep `Makefile.djgpp` source
  lists synchronized.
- Preserve the single event loop, single frame flush point, app lifecycle
  rollback, and capability-gated launch behavior.
- Record exact compiler, CMake, curses/PDCurses, retrocore-spec, Debug/Release,
  smoke, and sanitizer evidence.

Exit criteria: [RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md) is fully
checked with evidence attached to the candidate commit.

### `v0.2.0`: CI, Docs, and Portability Baseline

Goal: make the project state honest and repeatable before expanding features.

- Keep README, install docs, roadmap, release checklist, and support tiers in
  sync with the actual build matrix.
- Make Linux CI strict, analyzed, and fixture-backed.
- Decide whether non-Linux builds receive native storage adapters or are gated
  away from File Manager/Notepad until adapters exist.
- Keep generated build trees ignored and avoid vendored toolchains in the repo.

Exit criteria: every documented release-supported profile is actually built and
tested by the same canonical CMake target definition.

### `v0.3.0`: Useful Built-In Apps

Goal: turn the Linux preview into small but real workflows.

- File Manager uses the scroll-list widget for large directories and exposes
  predictable empty, unreadable, too-large, and unsupported-file states.
- File Manager opens text files through app runtime descriptors without
  duplicate/silent failure ambiguity.
- Notepad supports cursor movement, editing, open, save, Save As, conflict
  reporting, and non-blocking dirty-document handling.
- Diagnostics remains explicitly scoped unless a real shell/PTY milestone is
  accepted.
- App behavior tests cover lifecycle, open-with-path, save, conflict, dirty
  close, and keyboard-only navigation.

Exit criteria: File Manager and Notepad are practically useful on Linux without
breaking runtime contracts or portability policy.

### `v0.4.0`: Native Windows and Secondary Profiles

Goal: restore credible platform breadth after the POSIX storage slice.

- Add a native Windows storage adapter, a portable storage abstraction, or
  capability-based app gating that keeps Windows builds green.
- Validate Windows Debug/Release with PDCurses in CI.
- Document macOS and DOS as compile/reduced targets with exact exclusions.
- Keep Linux virtual console behavior keyboard-first and explicitly tested.

Exit criteria: Linux and Windows build/test claims are backed by current CI
evidence, and non-release profiles have precise documented limits.

### `v0.5.0`: Window Manager and Input Polish

Goal: make common multi-window workflows deterministic.

- Improve keyboard move/resize behavior and define modal/focus/close rules in
  tests.
- Harden curses and ANSI/raw input paths, including escape sequences and
  terminal capability degradation.
- Reduce redraw/flicker where possible while preserving one flush point.
- Expand event replay coverage for focus, drag, close, resize, and fallback
  behavior.

Exit criteria: users can operate the desktop by keyboard across supported
capability profiles, and pointer features degrade deterministically.

### `v0.6.0`: Configuration and Persistence

Goal: give users stable preferences without hidden global state.

- Add a configuration file format for theme/backend preferences.
- Persist a minimal desktop/session layout if it can be done without violating
  ownership rules.
- Document platform-specific defaults and override precedence.

Exit criteria: preferences survive restart and remain testable through explicit
runtime state.

### `v0.7.0`: Packaging and Distribution

Goal: make supported builds easy to install and smoke-test.

- Add installation and packaging evidence for supported platforms.
- Automate smoke checks where terminal constraints allow.
- Keep release artifacts reproducible from documented inputs.

Exit criteria: a user can build, install, run, and smoke-test RetroDesk from
documented steps without internal project knowledge.

### `v0.8.0`: Controlled Extension

Goal: grow app surface area without freezing an unstable plugin ABI.

- Add internal apps only through descriptors and capability checks.
- Reject unsupported capabilities explicitly.
- Defer external plugin compatibility until app/runtime contracts are mature.

Exit criteria: new app features do not require special-case runtime paths.

### `v0.9.0`: Beta Stabilization

Goal: freeze major contracts and remove release-blocking uncertainty.

- Freeze internal APIs for the v1.0 cycle.
- Publish a real compatibility matrix.
- Complete user-facing documentation and known-issues notes.
- Resolve or explicitly defer non-structural defects.

Exit criteria: remaining work is release polish, packaging, and documented bug
fixing, not architecture churn.

### `v1.0.0`: Stable Foundation

Goal: ship RetroDesk as a dependable terminal desktop baseline.

- Linux and Windows profiles are validated with current artifacts.
- Built-in apps cover basic file management, text editing, and diagnostics or
  explicitly scoped shell behavior.
- Window management is stable, keyboard-first, and test-backed.
- Input/render behavior is predictable by documented capability profile.
- Basic configuration and persistence are available.
- CI is reliable enough to guard future feature work.

Exit criteria: RetroDesk is usable as a small portable terminal desktop and is
safe to extend after v1.0.

## Prioritization Rules

- Finish release blockers before adding major features.
- Preserve the single event loop and single frame flush policy.
- Keep backend-native and OS-native details behind explicit adapters.
- Require tests for input, focus, render, app lifecycle, storage, and platform
  fallback changes.
- Prefer small app features that exercise runtime contracts over broad app
  parity with RetroTUI.
