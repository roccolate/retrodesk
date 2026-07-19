# Build System Policy

## Canonical Build

CMake is the canonical build system for modern targets. Production targets must
use explicit source lists, not wildcard `*.c` discovery.

## Makefiles

- `Makefile`: wrapper for CMake configure/build/clean/test ergonomics.
- `Makefile.djgpp`: specialized DOS build only.
- `make check-build-sources`: verifies DJGPP source-list parity with CMake.
- `make check-test-oracles`: rejects tests that rely on debug-only `assert`.
- `make smoke`: interactive startup smoke gate using `--bench-startup` (PTY
  required).
- `make smoke-ci`: non-interactive fallback smoke profile for CI/sandbox
  environments.
- `make smoke-linux-vc`: interactive Linux virtual-console policy check
  (`TERM=linux`, PTY required, expects `linux_tty_keyboard_only: true`).

## Dependency Policy

- Do not require a `vcpkg/` clone inside the repository tree as default
  behavior.
- Accept external toolchains via explicit configuration (`CMAKE_TOOLCHAIN_FILE`,
  `PDCURSES_ROOT`).
- Linux release evidence requires curses development headers/libraries
  (`libncurses-dev`, `ncurses-devel`, or equivalent).

## Build Profiles

- `dev`: warnings enabled, no forced Werror.
- `strict`: warnings enabled + Werror.
- `test`: Debug build plus CTest.
- `test-all`: Debug and Release tests plus CTest manifest comparison.
- `release`: optimized production build.
- `dos`: DJGPP profile via `Makefile.djgpp`.
- `smoke`: runtime startup sanity check in interactive terminal context.
- `smoke-ci`: smoke-compatible checks when interactive PTY is unavailable.
- `smoke-linux-vc`: Linux VC keyboard-first sanity check in interactive PTY
  context.

## Generated Files

Build outputs are ignored by Git. CMake build directories cache absolute source
and binary paths; if a checkout moves or a build directory was copied from
another path, remove it before using the result as evidence.

## CI Policy

- CI scripts must use canonical CMake target definitions.
- Linux CI should run strict build, clang-tidy/analyzer checks, Debug tests,
  Release tests, retrocore fixture tests, oracle guard, and source manifest
  checks.
- Windows CI is a required future gate for Tier 1 claims, but it is not a
  release claim until PDCurses plus storage adapter/gating passes on the
  candidate commit.
- Warnings are treated as errors in strict jobs.
