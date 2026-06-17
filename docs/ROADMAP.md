# Roadmap

## Product Target

RetroDesk aims to become a stable, portable terminal desktop runtime in C:
multi-window, keyboard-first, useful with built-in apps, and structured around
internal contracts that can grow without architectural regressions.

## Roadmap Model

This roadmap has two layers:

- Foundation phases describe the architecture baseline established for `v0.1.0`.
- Version milestones describe the path from the foundation release to `v1.0.0`.

Release gates take priority over feature growth. New features should only land
when they preserve the layering, ownership, event-loop, render, and capability
contracts defined in the active docs.

## Foundation Baseline (`v0.1.0`)

`v0.1.0` is the foundation milestone. It is approved only when
[RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md) is fully satisfied.

### Phase 0: Documentation Foundation

- Establish docs and ADRs as architecture source of truth.
- Simplify `README.md` to index entrypoint.

Exit criteria: all mandatory docs exist and are internally consistent.

### Phase 1: Canonical Build

- CMake as source of truth.
- Make wrapper and DOS-specialized makefile alignment.
- Remove in-repo toolchain assumptions.

Exit criteria: Tier 1 modern builds use same target sources.

### Phase 2: Runtime Layering

- Introduce `core`, `wm`, `app`, `render`, `platform` contracts.
- Remove backend types from domain/public headers.

Exit criteria: normalized event/render interfaces are in place.

### Phase 3: State Ownership Cleanup

- Replace global runtime state with `Desktop` ownership.
- Ensure deterministic cleanup and rollback.

Exit criteria: no global owner state in runtime logic.

### Phase 4: Event Loop Unification

- Eliminate nested modal loops.
- Keep one input poll + one frame flush point.

Exit criteria: no widget/app autonomous blocking input loop.

### Phase 5: Multi-Window WM Hardening

- Focus, z-order, drag, and close lifecycle in `wm`.

Exit criteria: multi-window interaction behaves deterministically.

### Phase 6: App Runtime Formalization

- Registry, launch rules, capability checks, app lifecycle hooks.

Exit criteria: at least two apps run through formal runtime contracts.

### Phase 7: Portability Hardening

- Capability-driven fallback across all tiers.
- Linux virtual console keyboard-first behavior proven.

Exit criteria: documented behavior profile per platform.

### Phase 8: Feature Porting Readiness

- Port selected subsystems from RetroTUI only through new contracts.
- Treat RetroTUI as behavior reference and risk catalog, not architecture source.

Exit criteria: feature growth can begin without architectural regressions.

## Version Milestones

### `v0.2.0`: Release, CI, and Windows

Goal: make the foundation release repeatable and prove Tier 1 portability.

- Close Windows Tier 1 native validation with CMake and PDCurses.
- Run `ctest` in Linux and Windows CI.
- Keep release hygiene clean: ignored generated files, no vendored toolchains by
  default, and no credentials in remotes or docs.
- Tag `v0.1.0` once its release checklist is fully satisfied.
- Document the release process for future tags.

Exit criteria: Linux and Windows CI build and test the same canonical CMake
targets, and the release process is reproducible from a clean checkout.

### `v0.3.0`: Useful Built-In Apps

Goal: replace placeholder apps with small but real workflows.

- File Manager lists directories and supports keyboard navigation.
- File Manager can open text files through the app runtime.
- Notepad supports cursor movement, editing, open, and save.
- Terminal is either a real shell wrapper or explicitly scoped as a diagnostics
  console.
- Add focused lifecycle and behavior tests for each built-in app.

Exit criteria: at least two built-in apps are practically useful without breaking
runtime contracts.

### `v0.4.0`: Window Manager Polish

Goal: make multi-window interaction feel deterministic and usable.

- Improve keyboard move and resize behavior.
- Define clear rules for focus, z-order, modal windows, close requests, and app
  owned windows.
- Expand event replay coverage for focus, drag, close, and fallback paths.
- Ensure keyboard-first operation remains complete when mouse support is absent.

Exit criteria: common window workflows are predictable across capability profiles.

### `v0.5.0`: Persistence and Configuration

Goal: give users stable preferences and basic session continuity.

- Add a configuration file format.
- Make default theme and backend preferences configurable.
- Persist a minimal desktop/session layout.
- Document platform-specific default behavior and override rules.

Exit criteria: user preferences survive restart without adding hidden global
runtime state.

### `v0.6.0`: Render and Input Hardening

Goal: reduce terminal-specific rough edges and rendering artifacts.

- Improve ANSI and curses renderer behavior.
- Reduce unnecessary redraw/flicker where possible.
- Add broader draw-list and diff-rendering tests.
- Harden tty/raw input parsing for common terminal escape sequences.
- Keep Linux virtual console and keyboard fallback behavior explicit.

Exit criteria: render/input behavior is stable across the supported terminal
profiles documented for Tier 1 and Tier 2.

### `v0.7.0`: Packaging and Distribution

Goal: make RetroDesk easy to build, install, and smoke-test.

- Produce release artifacts for supported platforms where practical.
- Add clear Linux, Windows, macOS, and DOS build/run instructions.
- Keep build outputs reproducible from documented inputs.
- Automate smoke checks as much as terminal constraints allow.

Exit criteria: a user can build and run RetroDesk from documented steps without
knowing the internal architecture.

### `v0.8.0`: Controlled Extension

Goal: grow app surface area without committing to an unstable plugin ABI.

- Add more internal apps or optional built-in app modules.
- Tighten app runtime contracts before exposing wider extension points.
- Keep unsupported capabilities rejected through explicit app descriptors.
- Defer external plugin compatibility until the app ABI is mature.

Exit criteria: new app features can be added without special-case runtime paths.

### `v0.9.0`: Beta Stabilization

Goal: freeze the main contracts and remove release-blocking uncertainty.

- Freeze major internal APIs for the `v1.0.0` cycle.
- Run a bug bash across Tier 1 platforms.
- Complete user-facing documentation.
- Publish a real compatibility matrix.
- Resolve or explicitly defer non-structural known issues.

Exit criteria: remaining work is polish, packaging, and documented bug fixing,
not architecture churn.

### `v1.0.0`: Stable Foundation

Goal: ship RetroDesk as a dependable terminal desktop baseline.

- Linux and Windows Tier 1 profiles are validated.
- Built-in apps cover basic file management, editing, and diagnostics or shell
  workflows.
- Window management is stable and keyboard-first.
- Input and render behavior are predictable by documented capability profile.
- Basic configuration and persistence are available.
- Tests and CI are reliable enough to guard future feature work.
- No known structural debt violates the foundation principles.

Exit criteria: RetroDesk is usable as a small portable terminal desktop and is
safe to extend after `v1.0.0`.

## Prioritization Rules

- Finish release blockers before adding major features.
- Preserve the single event loop and single frame flush policy.
- Keep backend-native details out of public domain headers.
- Require tests for input, focus, render, app lifecycle, and platform fallback
  changes.
- Prefer small app features that exercise the runtime contracts over broad app
  parity with RetroTUI.
