# RetroDesk Roadmap

## Product Target

RetroDesk aims to become a stable, portable terminal desktop runtime in C11:

- multi-window,
- keyboard-first,
- useful with built-in apps,
- portable across documented terminal/platform profiles,
- structured around contracts that can grow without architectural regressions.

RetroDesk is a runtime/desktop shell project. It may later inform other projects
such as ArmoniOS/WiiOS-style environments, but this repository should stay
focused on the portable terminal desktop runtime.

## Current Reality Snapshot

RetroDesk is currently a foundation-stage project with the main runtime shape in
place:

- CMake-first build with Make wrapper.
- Explicit layers: `core`, `wm`, `app`, `apps`, `render`, `platform`, `ui`.
- Window manager with focus, z-order, modal policy, close lifecycle, and
  capability-gated drag behavior.
- App runtime with descriptors, capability checks, app-owned windows, and
  built-in app registration.
- Dual render direction: curses/PDCurses plus ANSI diff rendering.
- Platform capability policy: keyboard baseline first; mouse/drag/resize only
  when explicitly supported.
- Theme and widget foundation: XP/Hacker/Amiga/Win31 themes plus text input,
  text buffer, scroll list, button, dialog, menu bar, progress bar, tab, and
  status bar.
- Built-in apps are wired through the runtime but still intentionally minimal:
  File Manager, Notepad, and Terminal are placeholder workflows.
- `retrocore-spec` fixture integration exists when the shared checkout is
  available.

## Roadmap Model

This roadmap has four layers:

1. **Release gates** — conditions that must be true before version tags.
2. **Hardening tracks** — focused cleanup areas that protect architecture.
3. **Version milestones** — user-visible progress from foundation to `v1.0.0`.
4. **Non-goals** — work intentionally deferred to avoid architecture drift.

Release gates take priority over feature growth. New features should only land
when they preserve the layering, ownership, event-loop, render, and capability
contracts defined in the active docs.

## Status Legend

| Status | Meaning |
| --- | --- |
| Done | Implemented or documented enough to serve as current baseline. |
| Active | Immediate next work; should be preferred before broader feature work. |
| Planned | Accepted milestone work, not next unless it unlocks active work. |
| Deferred | Explicitly out of scope until a later contract is stable. |

## Active References

Use these docs together with this roadmap:

- `docs/RELEASE_0.1_CHECKLIST.md` — foundation release gate.
- `docs/CORE_HARDENING_PLAN.md` — immediate core/API cleanup queue.
- `docs/ARCHITECTURE.md` — layer ownership and dependency rules.
- `docs/BUILD_SYSTEM.md` — CMake/Make/CI source of truth.
- `docs/APP_RUNTIME.md` — app descriptor and lifecycle contract.
- `docs/PORTABILITY.md` — platform tiers and capability fallback policy.
- `docs/RETROTUI_GAP.md` — feature gaps versus RetroTUI.
- `docs/TESTING.md` — current test suite and validation matrix.

## Milestone Overview

| Milestone | Status | Theme | Outcome |
| --- | --- | --- | --- |
| `v0.1.0` | Active gate | Foundation release | Stable contracts, documented limits, reproducible baseline. |
| `v0.1.x` | Active hardening | Core/API cleanup | Remove ambiguity and bug-shaped debt before feature growth. |
| `v0.2.0` | Planned | Release, CI, Windows | Linux/Windows Tier 1 validation and repeatable release flow. |
| `v0.3.0` | Planned | Useful apps | File Manager and Notepad become practical. |
| `v0.4.0` | Planned | Window manager polish | Predictable multi-window behavior. |
| `v0.5.0` | Planned | Config and persistence | Preferences and minimal session state survive restart. |
| `v0.6.0` | Planned | Render/input hardening | Less flicker, stronger ANSI/curses and TTY behavior. |
| `v0.7.0` | Planned | Packaging | Easier install/build/run experience. |
| `v0.8.0` | Planned | Controlled extension | More internal/optional apps without unstable plugin ABI. |
| `v0.9.0` | Planned | Beta stabilization | API freeze, compatibility matrix, bug bash. |
| `v1.0.0` | Planned | Stable baseline | Dependable portable terminal desktop. |

---

# `v0.1.0` — Foundation Release Gate

## Goal

Tag the first foundation release only when the runtime contracts, build surface,
portability policy, and documentation baseline are internally consistent.

## Scope In

- Layered runtime works as one system.
- CMake build and Make wrapper are aligned.
- App runtime can launch at least two apps through descriptors.
- WM focus/z-order/close/drag behavior is deterministic enough for foundation.
- Platform behavior is capability-driven and documented.
- Linux validation is complete.
- Windows build path is documented and validated enough to be Tier 1 target.

## Scope Out

- Complete file manager.
- Complete text editor.
- Real PTY terminal.
- Plugin ABI.
- Full feature parity with RetroTUI.
- OS-level shell integration beyond terminal runtime behavior.

## Exit Criteria

- `docs/RELEASE_0.1_CHECKLIST.md` is fully satisfied.
- `README.md` and docs describe the actual source/build/app state.
- `make strict`, `make test`, `make smoke-ci` pass in the expected Linux
  environment.
- Interactive smoke checks pass where a PTY is available.
- Any accepted limitations are documented as limitations, not hidden TODOs.

---

# `v0.1.x` — Immediate Core Hardening Track

## Goal

Clean up the small but important core/API issues before growing user-facing
features. This track prevents later features from building on ambiguous
contracts.

Source of truth: `docs/CORE_HARDENING_PLAN.md`.

## Work Packages

### 1. App launch correctness

- Call `desc->destroy` when `desc->create` fails after instance allocation.
- Add regression test for failed create cleanup.
- Disambiguate `app_launch` return value so callers can distinguish:
  - launched,
  - already running and focused,
  - failed.

### 2. CLI parse result cleanup

- Replace boolean parse result with explicit enum result.
- Make `--help`, valid parse, and parse error distinct.
- Add/extend CLI parser tests.

### 3. Key/event safety

- Make `RetroKeyEvent.ascii` sign-safe with `unsigned char`.
- Document extended byte behavior.
- Fix function-key vocabulary/comment mismatch.
- Add F13..F24 only if backend behavior is verified and tested.

### 4. Desktop internals cleanup

- Deduplicate app lookup logic.
- Keep diagnostics and capabilities ownership unambiguous.
- Replace launcher magic numbers with named constants.
- Add launcher enum count/sentinel where useful.

### 5. Small UX/performance polish

- Accept uppercase quit/close hotkeys if lowercase equivalents exist.
- Avoid obvious no-op redraws.
- Cache statusbar text where safe.

## Exit Criteria

- `CORE_HARDENING_PLAN.md` checklist is complete or explicitly split into later
  tracked work.
- Debug and Release `ctest` pass.
- Strict warning build passes.
- Sanitizer pass is clean where supported.
- No new public API ambiguity is introduced.

---

# `v0.2.0` — Release, CI, and Windows

## Goal

Make the foundation release repeatable and prove Tier 1 portability.

## Work Packages

### 1. Release process

- Tag `v0.1.0` once the release checklist is satisfied.
- Add release notes template.
- Document how to cut future tags.
- Keep release artifacts and generated outputs out of source history unless
  intentionally published.

### 2. CI hardening

- Keep Linux CI building and running `ctest` through canonical CMake.
- Keep Windows CI building and running `ctest -C Release` with vcpkg/PDCurses.
- Keep optional `retrocore-spec` fixtures available in CI.
- Make CI failures actionable by documenting expected local repro commands.

### 3. Windows Tier 1 validation

- Validate native Windows build path with PDCurses/vcpkg.
- Confirm keyboard baseline.
- Confirm mouse/resize capabilities only where supported.
- Document any limitations in `PORTABILITY.md`.

## Exit Criteria

- Linux and Windows CI build and test the same canonical CMake target surface.
- Release process is reproducible from a clean checkout.
- Windows Tier 1 behavior is documented by capability, not assumed by platform.

---

# `v0.3.0` — Useful Built-In Apps

## Goal

Replace placeholder apps with small but real workflows while preserving runtime
contracts.

## File Manager

- List current directory.
- Support keyboard navigation.
- Enter directories.
- Open text files through the app runtime.
- Show clear errors for unsupported file types or permission failures.
- Keep file operations minimal and safe.

## Notepad

- Cursor movement.
- Insert/delete text.
- Open text files.
- Save text files.
- Handle dirty state and close confirmation through runtime/WM contracts.
- Keep buffer limits explicit until a larger text model is designed.

## Terminal

Choose one direction and document it clearly:

1. Real shell wrapper, with platform-specific PTY/ConPTY constraints; or
2. Diagnostics console, intentionally scoped and not pretending to be a shell.

## Exit Criteria

- At least two built-in apps are practically useful.
- App features do not bypass descriptors, capability checks, normalized events,
  or DrawList rendering.
- Each built-in app has focused lifecycle and behavior tests.

---

# `v0.4.0` — Window Manager Polish

## Goal

Make multi-window interaction feel deterministic and usable across capability
profiles.

## Work Packages

- Improve keyboard move behavior.
- Define resize behavior and limits before implementing broad resize features.
- Clarify focus, z-order, modal, close, and app-owned window rules.
- Expand event replay coverage for focus, drag, close, fallback, and keyboard-only
  paths.
- Ensure keyboard-first operation remains complete when mouse support is absent.
- Decide whether minimize/maximize belongs in this milestone or later.

## Exit Criteria

- Common window workflows are predictable across Tier 1 profiles.
- Mouse features degrade cleanly without breaking keyboard operation.
- WM behavior has event replay coverage.

---

# `v0.5.0` — Persistence and Configuration

## Goal

Give users stable preferences and basic session continuity.

## Work Packages

- Add configuration file format.
- Make default theme configurable.
- Make backend preferences configurable.
- Persist minimal desktop/session layout.
- Document platform-specific default behavior and override rules.
- Avoid hidden global state while adding persistence.

## Exit Criteria

- Preferences survive restart.
- Minimal session state survives restart where supported.
- Config failures are safe and recoverable.
- Persistence does not introduce hidden runtime owners.

---

# `v0.6.0` — Render and Input Hardening

## Goal

Reduce terminal-specific rough edges and rendering artifacts.

## Work Packages

- Improve ANSI renderer behavior.
- Improve curses/PDCurses renderer behavior.
- Reduce unnecessary redraw/flicker.
- Add broader DrawList and diff-rendering tests.
- Harden TTY/raw input parsing for common terminal escape sequences.
- Keep Linux virtual console and keyboard fallback behavior explicit.
- Document known terminal profiles.

## Exit Criteria

- Render/input behavior is stable across documented Tier 1 and Tier 2 profiles.
- Flicker/no-op redraw issues are reduced or explicitly documented.
- Parser changes are covered by tests.

---

# `v0.7.0` — Packaging and Distribution

## Goal

Make RetroDesk easy to build, install, and smoke-test.

## Work Packages

- Produce release artifacts for supported platforms where practical.
- Add clear Linux, Windows, macOS, and DOS build/run instructions.
- Keep build outputs reproducible from documented inputs.
- Automate smoke checks as much as terminal constraints allow.
- Decide whether packaged builds include themes/docs/examples.

## Exit Criteria

A user can build and run RetroDesk from documented steps without knowing the
internal architecture.

---

# `v0.8.0` — Controlled Extension

## Goal

Grow app surface area without committing to an unstable external plugin ABI.

## Work Packages

- Add more internal apps or optional built-in modules.
- Tighten app runtime contracts before wider extension points.
- Keep unsupported capabilities rejected through explicit app descriptors.
- Define what an eventual plugin ABI would need, but do not promise it yet.
- Prefer apps that exercise existing contracts over apps that require special
  runtime exceptions.

## Exit Criteria

New app features can be added without special-case runtime paths.

---

# `v0.9.0` — Beta Stabilization

## Goal

Freeze the main contracts and remove release-blocking uncertainty before
`v1.0.0`.

## Work Packages

- Freeze major internal APIs for the `v1.0.0` cycle.
- Run a bug bash across Tier 1 platforms.
- Complete user-facing documentation.
- Publish a compatibility matrix.
- Resolve or explicitly defer non-structural known issues.
- Audit docs for stale claims before release candidate tagging.

## Exit Criteria

Remaining work is polish, packaging, and documented bug fixing, not architecture
churn.

---

# `v1.0.0` — Stable Portable Terminal Desktop

## Goal

Ship RetroDesk as a dependable terminal desktop baseline.

## Required Capabilities

- Linux and Windows Tier 1 profiles are validated.
- Built-in apps cover basic file management, editing, and diagnostics or shell
  workflows.
- Window management is stable and keyboard-first.
- Input and render behavior are predictable by documented capability profile.
- Basic configuration and persistence are available.
- Tests and CI are reliable enough to guard future feature work.
- No known structural debt violates the foundation principles.

## Exit Criteria

RetroDesk is usable as a small portable terminal desktop and safe to extend after
`v1.0.0`.

---

# Deferred / Non-Goals Before `v1.0.0`

The following should not drive the roadmap before the foundation is stable:

- External plugin ABI stability.
- Full RetroTUI feature parity.
- Complete OS/distribution identity.
- Graphical framebuffer backend outside terminal/console scope.
- Broad game/app suite expansion.
- Backend-native shortcuts inside apps/widgets.
- Any feature that requires a second event loop or second frame flush path.

---

# Prioritization Rules

1. Fix release blockers before adding major features.
2. Finish core/API ambiguity before building more app behavior on top.
3. Preserve the single event loop and single frame flush policy.
4. Keep backend-native details out of public domain headers.
5. Require tests for input, focus, render, app lifecycle, widgets, and platform
   fallback changes.
6. Prefer small app features that exercise runtime contracts over broad app
   parity with RetroTUI.
7. Keep documentation truthful: placeholder apps must be called placeholders;
   planned features must be called planned.
