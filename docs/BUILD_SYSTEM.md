# Build System Policy

## Canonical Build

CMake is the canonical build system for modern targets. The root
`CMakeLists.txt` owns the production source list, compiler options, curses /
PDCurses discovery, test registration, and optional `retrocore-spec` fixture
integration.

Current CMake baseline:

- `cmake_minimum_required(VERSION 3.16)`
- `project(RetroDesk C)`
- `CMAKE_C_STANDARD 11`
- `ENABLE_STRICT_WARNINGS` defaults to `ON`
- `ENABLE_WERROR` defaults to `ON`
- `ENABLE_TESTS` defaults to `ON`
- `PDCURSES_ROOT` may point at an external PDCurses install
- `RETROCORE_SPEC_DIR` defaults to `../retrocore-spec`

## Makefiles

- `Makefile`: wrapper for CMake configure/build/clean/test/smoke targets.
- `Makefile.djgpp`: specialized DOS build only.
- `make dev`: Debug build with strict warnings but `WERROR=OFF`.
- `make strict`: strict warnings + Werror.
- `make test`: configure/build and run `ctest`.
- `make check-build-sources`: verify `Makefile.djgpp` source list stays aligned
  with CMake's `RETRODESK_DJGPP_SOURCES`.
- `make smoke`: interactive startup smoke gate using `--bench-startup`
  (requires PTY).
- `make smoke-ci`: non-interactive fallback smoke profile for CI/sandbox
  environments.
- `make smoke-linux-vc`: interactive Linux virtual-console policy check
  (`TERM=linux`, requires PTY, expects `linux_tty_keyboard_only: true`).

## Dependency Policy

- Do not require a `vcpkg/` clone inside the repository tree as default
  behavior.
- Accept external toolchains via explicit configuration (`CMAKE_TOOLCHAIN_FILE`,
  `PDCURSES_ROOT`).
- Linux/macOS should use system or package-manager curses installs.
- Windows CI may clone vcpkg outside the source contract and install
  `pdcurses:x64-windows` for the job.
- DOS builds use the DJGPP profile and the explicitly maintained source list.

## Source Enumeration Policy

- CMake target sources must be explicit and versioned.
- Do not compile all `*.c` by wildcard for production targets.
- New source files must be added deliberately to the relevant source list:
  `RETRODESK_SOURCES`, `RETRODESK_DJGPP_SOURCES`, and/or test executables.
- When a DOS-compatible source is added, update `Makefile.djgpp` and run
  `make check-build-sources`.

## Build Profiles

- `dev`: warnings enabled, Werror disabled, Debug configuration.
- `strict`: warnings enabled + Werror.
- `release`: optimized production build via `CONFIG=Release` / CMake release
  configuration.
- `dos`: DJGPP profile via `Makefile.djgpp`.
- `smoke`: runtime startup sanity check in an interactive terminal context.
- `smoke-ci`: smoke-compatible checks when interactive PTY is unavailable.
- `smoke-linux-vc`: Linux VC keyboard-first sanity check in an interactive PTY
  context.

## CI Policy

- Tier 1 targets build and test on every push/PR where Actions is available.
- Current workflow matrix targets `ubuntu-latest` and `windows-latest`.
- Linux CI installs ncurses and runs `clang-tidy` across `src/*.c` with warnings
  treated as errors.
- Windows CI uses vcpkg + `pdcurses:x64-windows`, configures with the vcpkg
  toolchain, builds, and runs `ctest -C Release`.
- CI checks out `roccolate/retrocore-spec` into `retrocore-spec` so the optional
  shared fixture test can be enabled.
- CI scripts must use the canonical CMake target definition; no divergent build
  graph is allowed.
