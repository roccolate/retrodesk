# Build System Policy

## Canonical Build

CMake is the canonical build system for modern targets.

## Makefiles

- `Makefile`: wrapper for CMake configure/build/clean.
- `Makefile.djgpp`: specialized DOS build only.
- `make smoke`: interactive startup smoke gate using `--bench-startup`.

## Dependency Policy

- Do not require a `vcpkg/` clone inside repository tree as default behavior.
- Accept external toolchains via explicit configuration (`CMAKE_TOOLCHAIN_FILE`, `PDCURSES_ROOT`).

## Source Enumeration Policy

- CMake target sources must be explicit and versioned.
- Do not compile all `*.c` by wildcard for production targets.

## Build Profiles

- `dev`: warnings enabled, no forced Werror.
- `strict`: warnings enabled + Werror.
- `release`: optimized production build.
- `dos`: DJGPP profile via `Makefile.djgpp`.
- `smoke`: runtime startup sanity check in terminal context.

## CI Policy

- Tier 1 targets build on every PR.
- Warnings are treated as errors in strict jobs.
- CI scripts must use canonical CMake target definition.
