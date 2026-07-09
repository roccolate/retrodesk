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
- [x] `make smoke` passes in an interactive terminal.
- [x] `make smoke-ci` passes in non-interactive CI/sandbox environments.
- [x] Windows Tier 1 build succeeds with documented toolchain (mingw-w64 cross-build validated locally; vcpkg CI path hardened to define `HAVE_PDCURSES` + `PDC_NCMOUSE` automatically).
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

- [x] Linux profile validated (keyboard baseline always works).
- [x] Windows profile validated (mingw-w64 cross-build produces `retrodesk.exe` plus all 19 test `.exe` binaries; `HAVE_PDCURSES` + `PDC_NCMOUSE` are defined; CI smoke execution still pending a Windows runner with billing restored).
- [x] macOS and DOS status documented as Tier 2 compile/reduced profile.
- [x] Linux `TERM=linux` keyboard-first policy remains documented and intact.

### 5. Documentation Gate

- [x] `docs/INDEX.md` links all active policies and this checklist.
- [x] `docs/ROADMAP.md` references `v0.1.0` release gate.
- [x] `docs/TESTING.md` reflects the release validation matrix.
- [x] README is the user-facing entrypoint and points to the deeper docs index.
- [x] Source layout, build targets, test suite, app status, and platform policy are reflected in the active docs.

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

## Validation Commands (Windows cross-build)

```bash
cmake -S . -B build-mingw \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
  -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
  -DPDCURSES_ROOT=/path/to/PDCurses \
  -DENABLE_WERROR=ON -DENABLE_STRICT_WARNINGS=ON
cmake --build build-mingw
```

Or via vcpkg toolchain (CI path):

```bash
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build
ctest --test-dir build --output-on-failure
```

## Release Artifacts

- Git tag: `v0.1.0`
- Release notes summary:
  - architecture/runtime baseline,
  - platform profile status,
  - known limits for post-0.1 milestones.

## Validation Snapshot (2026-07-06)

- Verified in WSL/Linux:
  - `make strict`
  - `make test`
  - `make smoke`
  - `make smoke-ci`
  - `make smoke-linux-vc`
- Verified Windows Tier 1 cross-build locally:
  - mingw-w64 (gcc 13.2.0) cross-compiles `retrodesk.exe` plus all 19 test
    `.exe` binaries cleanly with `-Wall -Wextra -Werror`.
  - PDCurses wincon built from source (`pdcurses.a`) linked successfully.
  - CMake `HAVE_PDCURSES` + `PDC_NCMOUSE` are auto-defined both via the
    explicit `PDCURSES_ROOT` path and the vcpkg toolchain path (where
    `CURSES_LIBRARIES` resolves to `pdcurses.lib`).
- Integration/runtime evidence:
  - `desktop_runtime_test` covers capability rejection, two app launches, clean close,
    and repeated create/run/shutdown cycles.
- Linux virtual-console policy evidence:
  - `make smoke-linux-vc` verifies `TERM=linux` reports
    `linux_tty_keyboard_only: true`.
- Remaining open item:
  - End-to-end CI validation on `windows-latest` runner is pending; the
    GitHub account billing must be restored for Actions to start jobs.
    The workflow `.github/workflows/ci.yml` is configured to clone vcpkg,
    install `pdcurses:x64-windows`, configure with the vcpkg toolchain, and
    run `ctest -C Release`.

## Documentation Refresh Snapshot (2026-07-09)

The active docs were refreshed against the current CMake/source layout:

- `README.md`
- `docs/INDEX.md`
- `docs/ARCHITECTURE.md`
- `docs/BUILD_SYSTEM.md`
- `docs/TESTING.md`
- `docs/RETROTUI_GAP.md`
- `docs/AGENTS.md`

## Post-Release Immediate Priorities

1. Expand WM/input regression tests.
2. Harden Windows build + smoke in CI (requires billing resolution).
3. Begin selective functional app porting from RetroTUI without breaking contracts.
