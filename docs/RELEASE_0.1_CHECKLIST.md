# Release 0.1 Checklist

## Intent

`v0.1.0` is a foundation milestone: stable runtime contracts, reproducible builds,
capability-driven behavior, and documented fallback policy.

This is **not** a feature-complete desktop release.

## Scope In

- Layered runtime (`core`, `platform`, `render`, `wm`, `app`, `apps`, `ui`) working as one system.
- Canonical CMake build and Makefile wrapper behavior aligned.
- Capability-aware input/drag/resize behavior.
- At least two apps launched through app runtime contracts.

## Scope Out

- Advanced app suite parity with RetroTUI.
- Plugin ecosystem.
- Full feature parity across Tier 1 and Tier 2 platforms.

## Exit Criteria (Definition of Done)

All items below must be satisfied before tagging `v0.1.0`.

### 1. Build and Tooling Gate

- [x] `make strict` passes on Linux.
- [x] `make test` passes on Linux.
- [ ] `make smoke` passes in an interactive terminal.
- [x] `make smoke-ci` passes in non-interactive CI/sandbox environments.
- [ ] Windows Tier 1 build succeeds with documented toolchain.
- [x] `Makefile` remains a CMake wrapper (no divergent source lists).

### 2. Runtime and Behavior Gate

- [x] Single-loop runtime confirmed (no nested modal loops).
- [x] Single frame-flush path confirmed.
- [x] Multi-window focus + z-order deterministic.
- [x] Drag behavior follows capability policy and keyboard fallback remains usable.
- [x] Shutdown path remains deterministic (no leaked ownership paths known by design).

### 3. App Runtime Gate

- [x] App registry rejects duplicate registration.
- [x] Capability-based app launch rejection is enforced.
- [x] At least two apps are launchable via runtime descriptors.
- [x] Closing app releases its window cleanly.

### 4. Portability Gate

- [ ] Linux profile validated (keyboard baseline always works).
- [ ] Windows profile validated (keyboard baseline; mouse/resize by capability).
- [x] macOS and DOS status documented as Tier 2 compile/reduced profile.
- [x] Linux `TERM=linux` keyboard-first policy remains documented and intact.

### 5. Documentation Gate

- [x] `docs/INDEX.md` links all active policies and this checklist.
- [x] `docs/ROADMAP.md` references `v0.1.0` release gate.
- [x] `docs/TESTING.md` reflects the release validation matrix.
- [x] README remains concise and points to docs index.

### 6. Debt Gate

- [x] No known violation of `FOUNDATION_PRINCIPLES.md`.
- [x] Any accepted temporary debt is explicitly tracked with owner + removal trigger.
- [x] No hidden global runtime owner reintroduced.

## Validation Commands (Linux)

```bash
make clean
make strict
make test
make smoke
make smoke-ci
make smoke-linux-vc
```

## Release Artifacts

- Git tag: `v0.1.0`
- Release notes summary:
  - architecture/runtime baseline,
  - platform profile status,
  - known limits for post-0.1 milestones.

## Validation Snapshot (2026-03-07)

- Verified in this environment:
  - `make strict`
  - `make test`
  - `make smoke-ci`
- Integration/runtime evidence:
  - `desktop_runtime_test` covers capability rejection, two app launches, clean close,
    and repeated create/run/shutdown cycles.
- `make smoke` and `make smoke-linux-vc` remain pending here because interactive PTY
  capture is unavailable.

## Post-Release Immediate Priorities

1. Expand WM/input regression tests.
2. Harden Windows build + smoke in CI.
3. Begin selective functional app porting from RetroTUI without breaking contracts.
