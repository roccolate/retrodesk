# Release 0.1 Checklist (reopened)

> No `v0.1.0` tag exists and this checklist is not satisfied. Every check below
> requires evidence tied to the candidate commit. The current working tree adds
> Linux/POSIX File Manager, Notepad, storage, test-oracle, and CI work, but it is
> not a release candidate until the gates below are rebuilt from a clean tree.

## Intent

`v0.1.0` remains an unshipped historical foundation milestone. It may only be
tagged if the evidence gates below are rebuilt on the exact candidate commit.

This is **not** a feature-complete desktop release.

## Scope In

- Layered runtime (`core`, `platform`, `render`, `wm`, `app`, `apps`, `ui`,
  `storage`) working as one system on the release-supported profile.
- Canonical CMake build and Makefile wrapper behavior aligned.
- Capability-aware input, drag, resize, app-launch, and storage behavior.
- File Manager, Notepad, and Diagnostics launched through app runtime
  descriptors.
- Linux/POSIX storage preview for directory listing, text read, atomic save, and
  conflict detection.

## Scope Out

- Advanced app suite parity with RetroTUI.
- Plugin ecosystem.
- PTY/shell terminal.
- Destructive filesystem operations: delete, rename, copy, move.
- Full feature parity across planned Tier 1 and Tier 2 platforms.

## Exit Criteria

All items below must be satisfied before tagging `v0.1.0`.

### 1. Build and Tooling Gate

- [ ] `make clean` removes stale CMake cache paths.
- [ ] `make strict` passes on Linux with curses development headers installed.
- [ ] `make test` passes on Linux.
- [ ] `make test-all` passes and Debug/Release CTest manifests match.
- [ ] `make smoke` passes in an interactive terminal.
- [ ] `make smoke-ci` passes in non-interactive CI/sandbox environments.
- [ ] `make smoke-linux-vc` passes in an interactive PTY with `TERM=linux`.
- [ ] `python3 scripts/check_test_oracles.py` reports all test files using
  always-active `TEST_CHECK`/`TEST_REQUIRE` oracles.
- [ ] `python3 scripts/check_djgpp_sources.py` reports CMake and DJGPP source
  manifests in sync.
- [ ] `Makefile` remains a thin CMake wrapper with no divergent production
  source list.

### 2. Runtime and Behavior Gate

- [ ] Single-loop runtime confirmed.
- [ ] Single frame-flush path confirmed.
- [ ] Multi-window focus and z-order are deterministic.
- [ ] Drag behavior follows capability policy and keyboard fallback remains
  usable.
- [ ] Shutdown path remains deterministic with no known leaked ownership path.
- [ ] Dirty-document close rejection keeps the app open without nested loops.

### 3. App Runtime and Product Preview Gate

- [ ] App registry rejects duplicate registration.
- [ ] Capability-based app launch rejection is enforced.
- [ ] File Manager launches by default through the app runtime.
- [ ] File Manager lists the current directory and supports keyboard navigation,
  parent navigation, refresh, and regular-file open through Notepad.
- [ ] Notepad opens a resource path, edits text, saves existing files, supports
  Save As for untitled files, and reports write conflicts.
- [ ] Diagnostics remains explicitly scoped as read-only runtime information.
- [ ] Closing an app releases its window cleanly.

### 4. Storage Gate

- [ ] POSIX storage adapter rejects unsupported file types.
- [ ] Text read is bounded and rejects non-ASCII/control content outside the
  documented text contract.
- [ ] Atomic save writes through a temporary file and detects stale versions.
- [ ] Directory listing handles empty, unreadable, and too-large directories
  without partial callback delivery.
- [ ] Non-POSIX builds either have a native adapter or explicitly gate apps that
  require storage.

### 5. Portability Gate

- [ ] Linux profile validated: keyboard baseline always works.
- [ ] Windows status is honest: either native PDCurses Debug/Release passes with
  storage support, or Windows is documented as planned/not release-supported for
  this tag.
- [ ] macOS and DOS status is documented as compile/reduced or unsupported for
  this tag.
- [ ] Linux `TERM=linux` keyboard-first policy remains documented and intact.

### 6. Documentation Gate

- [ ] `README.md` states pre-alpha status, Linux active profile, app preview
  limits, and Diagnostics-not-shell scope.
- [ ] `INSTALL.md` documents Linux as the recommended path and marks native
  Windows/macOS/DOS recipes as bring-up unless validated.
- [ ] `docs/INDEX.md` links all active policies and this checklist.
- [ ] `docs/ROADMAP.md` references this release gate and does not claim
  unsupported features.
- [ ] `docs/TESTING.md` reflects the release validation matrix.
- [ ] `docs/PORTABILITY.md` separates target tiers from current validation
  status.

### 7. Debt Gate

- [ ] No known violation of `FOUNDATION_PRINCIPLES.md`.
- [ ] Any accepted temporary debt is explicitly tracked with owner, reason,
  removal trigger, and target milestone.
- [ ] No hidden global runtime owner reintroduced.
- [ ] Known structural cleanup items in `CORE_HARDENING_PLAN.md` are either
  completed or explicitly deferred outside `v0.1.0`.

## Validation Commands (Linux)

```bash
make clean
make strict
make test
make test-all
make smoke
make smoke-ci
make smoke-linux-vc
```

If CMake cannot find curses, install the platform curses development package
first. If an old `build/` directory points at another absolute checkout path,
remove it before collecting evidence.

## Validation Commands (Windows bring-up)

Native Windows is planned, not assumed green. A candidate that claims Windows
support must include equivalent Debug/Release evidence using PDCurses and a
storage implementation or storage-app gating that keeps the build green.

Example PDCurses root flow:

```bash
cmake -S . -B build-windows \
  -DPDCURSES_ROOT=/path/to/PDCurses \
  -DENABLE_WERROR=ON -DENABLE_STRICT_WARNINGS=ON
cmake --build build-windows --config Debug
cmake --build build-windows --config Release
ctest --test-dir build-windows --output-on-failure -C Debug
ctest --test-dir build-windows --output-on-failure -C Release
```

Example vcpkg flow:

```bash
cmake -S . -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build-windows --config Debug
cmake --build build-windows --config Release
ctest --test-dir build-windows --output-on-failure -C Debug
ctest --test-dir build-windows --output-on-failure -C Release
```

## Release Artifacts

- Git tag: `v0.1.0`
- Release notes summary:
  - architecture/runtime baseline,
  - Linux product-preview scope,
  - platform profile status,
  - known limits for post-0.1 milestones.

## Evidence Required Per Candidate

- Commit SHA and clean-tree confirmation.
- Compiler, CMake, curses/PDCurses, Python, and analyzer versions.
- Debug and Release test/check counts with active non-`assert` oracles.
- ASan/UBSan/LSan output where available.
- Smoke logs for every claimed runtime profile.
- Immutable `retrocore-spec` revision when fixture tests are required.
- Manual acceptance record for each claimed platform and workflow.

## Post-Release Immediate Priorities

1. Resolve native Windows storage portability.
2. Expand File Manager and Notepad behavior tests.
3. Complete Core hardening items that were explicitly deferred from `v0.1.0`.
