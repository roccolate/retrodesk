# RetroDesk

RetroDesk is a C11 terminal desktop runtime focused on portability, strict layering,
and long-term maintainability.

This repository now treats documentation as the source of architectural truth.
`RetroTUI` is used as functional reference only, not as design template.

## Start Here

Read [docs/INDEX.md](docs/INDEX.md) for:

- architecture and module boundaries,
- platform policy (Tier 1/Tier 2),
- build and CI rules,
- app runtime contract,
- roadmap and technical-debt policy.

Current runtime layout lives under `src/` with layered modules:
`core`, `platform`, `render`, `wm`, `app`, `apps`, and `ui`.

## Build

Canonical flow (CMake):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Windows users can either rely on a configured toolchain (for example vcpkg toolchain)
or pass `-DPDCURSES_ROOT=/path/to/pdcurses`.

The repository `Makefile` is a CMake wrapper. DOS is handled by `Makefile.djgpp`.
Use `make smoke` for interactive PTY smoke, `make smoke-linux-vc` for Linux
`TERM=linux` keyboard-first validation, and `make smoke-ci` for non-interactive
fallback checks.

Runtime flags:
- `--bench-startup`
- `--render-backend=curses|ansi`
- `--input-backend=curses|tty`
- `--theme=xp|hacker|amiga|win31`
