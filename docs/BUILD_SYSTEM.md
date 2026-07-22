# Build System Policy

## Canonical Build

CMake is the canonical build description for modern targets. Production and test
targets use explicit source lists; wildcard `*.c` discovery is forbidden.

The root Makefile is an ergonomic wrapper around CMake. It must not become a
second independent production source-of-truth.

## Build Entry Points

### Root Makefile

- `make` — configure/build the default development tree.
- `make clean` — remove generated build trees known to the wrapper.
- `make strict` — strict warnings and Werror.
- `make test` — Debug test build and CTest.
- `make test-all` — Debug and Release tests plus manifest comparison.
- `make test-sanitize` — dedicated ASan/UBSan test tree where supported.
- `make check-test-oracles` — reject release-disabled assertion-only tests.
- `make check-build-sources` — verify canonical/DJGPP source synchronization.
- `make smoke` — interactive PTY startup smoke.
- `make smoke-ci` — non-interactive startup/diagnostic smoke.
- `make smoke-linux-vc` — interactive `TERM=linux` keyboard-first policy smoke.

### `Makefile.djgpp`

This is the specialized native DOS build definition. It exists because the DJGPP
profile cannot use the same host/library discovery path as modern CMake profiles.
It must remain synchronized with the portable production source set through the
source-manifest guard.

## Source-List Policy

- Add production sources to canonical CMake targets.
- Add portable sources to the DJGPP list when the reduced profile supports them.
- Exclude host-native source files from incompatible analyzers/toolchains through
  explicit CMake conditions or translation-unit isolation.
- Header-only desktop presentation/bridge contracts do not require source-list
  entries.
- Never hide a missing source behind wildcard discovery.

## Dependency Policy

Dependencies must be external, explicit, and reproducible.

- Do not require a repository-local `vcpkg/` clone by default.
- Accept `CMAKE_TOOLCHAIN_FILE`, `PDCURSES_ROOT`, and other explicit paths.
- Linux requires curses development headers/libraries.
- Windows CI pins/uses PDCurses through its configured toolchain.
- DOS CI pins DJGPP GCC, PDCurses source, and official CWSDPMI archives/revisions
  and verifies immutable checksums/revisions where applicable.
- Shared behavior fixtures use a pinned `retrocore-spec` revision in CI.

## Build Profiles

| Profile | Purpose |
| --- | --- |
| Development | warnings enabled, normal local iteration |
| Strict | warnings + Werror |
| Debug test | assertions/debug instrumentation plus complete CTest |
| Release test | optimized build plus complete matching CTest manifest |
| Sanitizer | Debug ASan/UBSan and leak checks where supported |
| Windows Debug/Release | PDCurses and native Win32 storage under `/W4 /WX` |
| DOS | DJGPP/PDCurses/CWSDPMI reduced native build |
| Smoke | interactive PTY runtime check |
| Smoke CI | non-interactive diagnostic/startup check |
| Linux VC | `TERM=linux` keyboard-first interactive policy check |

## Platform Source Selection

CMake selects the active storage adapter by target platform:

- POSIX adapter for supported Unix-like targets;
- Win32 adapter for native Windows;
- DJGPP adapter for DOS.

Tests use the same adapter selection as the product target. A platform-specific
storage contract executable is added where the host/profile requires it.

## DOS Build and Artifact Flow

The DOS workflow:

1. installs the pinned DJGPP cross toolchain;
2. verifies/installs official CWSDPMI;
3. checks out and builds pinned PDCurses for DOS;
4. checks CMake/DJGPP source parity;
5. builds `retrodesk.exe`;
6. builds native `FSTEST.EXE`;
7. executes the storage contract in DOSBox-X;
8. executes `retrodesk.exe --diagnose` in DOSBox-X;
9. requires DOS-written success evidence;
10. packages executable, storage test, CWSDPMI, README, and checksums;
11. retains failure logs/emulator state when a gate fails.

DOS is a reduced profile, not a second canonical architecture.

## Generated Files

Build directories contain absolute source/binary paths. A moved/copied checkout
must not reuse stale CMake caches as evidence.

Before validation:

```bash
make clean
# or remove the relevant build-* directories explicitly
```

Generated build outputs and downloaded toolchains/artifacts remain outside source
control unless a release process explicitly imports a small human-authored
manifest.

## Quality Policy

Strict jobs treat warnings as errors. Product and test targets should share the
project's quality function/options so a test-only source does not silently build
under weaker diagnostics.

Boundary tests may suppress a narrowly documented compiler warning only when the
test intentionally constructs the boundary and all other strict diagnostics
remain enabled.

## CI Policy

Current required development gates:

### Linux

- oracle guard;
- pinned fixture checkout;
- Debug/Release/sanitizer configure;
- static analysis;
- complete Debug and Release tests;
- sanitizer/leak tests;
- test-manifest comparison;
- DJGPP source parity;
- non-interactive startup smoke.

### Windows

- Debug and Release configure/build/tests;
- `/W4 /WX`;
- PDCurses;
- native Win32 storage contract.

### DOS

- pinned native build inputs;
- product and `FSTEST.EXE` build;
- DOSBox-X storage and application smoke;
- distribution artifact/checksums.

A green CI run is development evidence. Release evidence must be attached to the
exact final candidate commit and supplemented by required interactive acceptance.

## Canonicality Checks

The repository must reject:

- divergent source manifests;
- assertion-only tests disabled by `NDEBUG`;
- unpinned/replaced fixture revisions in evidence jobs;
- stale copied CMake trees presented as clean evidence;
- platform claims based only on cross-compilation;
- product source files that compile only because a test target omits strict flags.

## Current Known Build Debt

- macOS lacks current evidence.
- Interactive Windows and DOS maturity exceeds what automated build/tests alone
  can prove.
- Temporary header-only Desktop bridges increase macro-sensitive compilation
  paths; all modern, Windows, and DJGPP profiles must keep compiling them until
  Desktop decomposition removes that architecture.
- Release evidence metadata is not yet emitted as one concise machine-readable
  manifest.

See [TESTING.md](TESTING.md), [PORTABILITY.md](PORTABILITY.md), and
[KNOWN_ISSUES.md](KNOWN_ISSUES.md).