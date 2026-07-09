# RetroDesk

A retro desktop environment for the terminal. Think Windows 3.1 / 95 in your
console, written in C11 with strict layering, platform abstraction, and a
documented contract.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Architecture: C11 / curses](https://img.shields.io/badge/C-C11%20%2F%20curses-blue.svg)]()
[![Status: foundation hardened / v1.0 in progress](https://img.shields.io/badge/status-foundation%20hardened%20%2F%20v1.0%20in%20progress-orange.svg)]()

---

## What Is It

RetroDesk is a portable terminal desktop runtime in C11. It hosts multiple
apps inside a window manager, supports a real keyboard / mouse event flow, and
renders either through native curses or an ANSI fallback with frame diffing.
The runtime layers cleanly into `core`, `platform`, `render`, `wm`, `app`,
`apps`, and `ui`, so each subsystem has a single responsibility.

It is the C rewrite of [RetroTUI](https://github.com/roccolate/retrotui):
same ideas, smaller surface, multi-window from the start, and a real
portability policy across Linux, macOS, Windows, and DOS (DJGPP).

## Current Status

RetroDesk is a foundation-stage runtime, not a complete desktop suite yet.
The architectural baseline is in place: CMake-first build, explicit runtime
layers, app descriptors, window manager ownership, dual render backends,
capability-aware input behavior, themes, and a small widget library.

The built-in apps are intentionally minimal right now:

- File Manager is a keyboard-navigable stub.
- Notepad is a single-buffer placeholder editor.
- Terminal is a diagnostics placeholder, not a PTY / shell wrapper yet.

The next product milestones are documented in
[`docs/ROADMAP.md`](docs/ROADMAP.md): release hygiene / CI hardening, then
turning the built-in apps into useful workflows.

## Features

- **Window manager** — focus, z-order, close lifecycle, modals, title-bar drag
  where supported, and keyboard window movement.
- **Dual render backend** — native curses or ANSI fallback with frame diffing.
- **Platform abstraction** — ncurses/PDCurses input plus raw TTY + ANSI input
  where supported.
- **Themeable** — XP, Hacker, Amiga, and Win31 themes.
- **UI widget library** — text input, scrollable list, multi-line text buffer,
  button, dialog, menu bar, progress bar, tab, theme, and status bar modules.
- **Built-in app descriptors** — File Manager, Notepad, and Terminal are wired
  through the formal app runtime.
- **Keyboard and mouse policy** — keyboard is the baseline; mouse, drag,
  right-click, and resize behavior are capability-gated per backend/platform.
- **Contract tests** — unit tests, window-manager replay tests, renderer/input
  tests, desktop runtime tests, and optional `retrocore-spec` fixture tests.

## Build

### Requirements

- **CMake** 3.16 or newer.
- **Compiler** — GCC, Clang, or MSVC-compatible toolchain with C11 support.
- **Linux / macOS** — `ncurses` (`apt install libncurses-dev` or
  `brew install ncurses`).
- **Windows** — PDCurses via `PDCURSES_ROOT` or vcpkg (`pdcurses:x64-windows`).
- **DOS** — DJGPP profile via `Makefile.djgpp`.

### Build

```bash
git clone https://github.com/roccolate/retrodesk.git
cd retrodesk
cmake -S . -B build
cmake --build build
```

The repository `Makefile` is a thin CMake wrapper for ergonomics:

```bash
make                     # build via CMake
make dev                 # Debug build, Werror disabled
make strict              # strict warnings + Werror
make test                # build and run ctest
make check-build-sources # verify CMake and DJGPP source lists stay aligned
make smoke               # interactive PTY smoke test
make smoke-ci            # non-interactive fallback smoke profile
make smoke-linux-vc      # TERM=linux keyboard-first smoke test
```

## Usage

```bash
# Run with default theme (XP)
./build/retrodesk

# Change theme
./build/retrodesk --theme=hacker
./build/retrodesk --theme=amiga
./build/retrodesk --theme=win31

# Choose backend
./build/retrodesk --render-backend=curses   # native curses
./build/retrodesk --render-backend=ansi     # ANSI fallback
./build/retrodesk --input-backend=curses    # ncurses/PDCurses input
./build/retrodesk --input-backend=tty       # raw TTY + ANSI sequences

# Diagnostics
./build/retrodesk --bench-startup
./build/retrodesk --help
```

## Hotkeys

| Key | Action |
|---|---|
| `q` | Quit |
| `m` | Toggle launcher |
| `1` / `2` / `3` | Launch File Manager / Notepad / Terminal |
| `Tab` | Cycle focus |
| `HJKL` | Move active window |
| `w` | Close active window |
| `Esc` | Close focused app |

## Project Structure

```text
src/
├── main.c                       # Entry point
├── core/
│   ├── cli.c/.h                 # Command-line parsing
│   ├── desktop.c/.h             # Desktop lifecycle, launcher, frame loop
│   ├── event.h                  # RetroEvent / RetroKeyEvent / RetroMouseEvent
│   └── key_chord.c/.h           # Backend-neutral key vocabulary helpers
├── platform/
│   ├── platform.c/.h            # Platform abstraction facade
│   ├── platform_backend_internal.h
│   ├── platform_curses.c        # ncurses/PDCurses backend
│   ├── platform_tty_raw.c       # Raw TTY + ANSI input
│   └── tty_decoder.c/.h         # TTY escape sequence parser
├── render/
│   ├── render.c/.h              # DrawList + curses/ANSI dispatcher
│   └── ansi_renderer.c/.h       # ANSI renderer with frame diff
├── wm/
│   └── window_manager.c/.h      # Window manager
├── app/
│   └── app_runtime.c/.h         # App lifecycle + registry
├── ui/
│   ├── text_input.c/.h          # Single-line text input
│   ├── text_buffer.c/.h         # Multi-line text buffer
│   ├── scroll_list.c/.h         # Scrollable list widget
│   ├── button.c/.h              # Button widget
│   ├── dialog.c/.h              # Dialog system
│   ├── menu_bar.c/.h            # Menu bar widget
│   ├── progress_bar.c/.h        # Progress bar widget
│   ├── tab.c/.h                 # Tab widget
│   ├── theme.c/.h               # Theme system
│   └── statusbar.c/.h           # Status bar
└── apps/
    ├── apps.c/.h                # Built-in app registration
    ├── apps_internal.h
    ├── filemanager_app.c        # File Manager placeholder app
    ├── notepad_app.c            # Notepad placeholder app
    └── terminal_app.c           # Terminal diagnostics placeholder app
```

## Development

### Add a Widget

1. Create `src/ui/mywidget.{h,c}`.
2. Follow the patterns in `text_input.c` — lifecycle (`create` / `destroy`),
   state, events, render to `DrawList`.
3. Add the source to `CMakeLists.txt` (`RETRODESK_SOURCES`) and keep the
   DJGPP source list aligned when the module is DOS-compatible.
4. Write unit tests in `tests/mywidget_test.c` and register them under
   `BUILD_TESTING`.
5. Build with `-Werror` clean and pass `make test`.
6. Update the relevant docs when the public contract, source structure, build
   surface, hotkeys, tests, or platform behavior changes.

### Coding Standards

See [docs/CODE_STANDARDS.md](docs/CODE_STANDARDS.md). Highlights:

- C11, `-Wall -Wextra -Wpedantic -Werror` clean.
- Null-safe — every pointer parameter checked.
- Bounds-safe — every buffer write validated.
- Leak-free — every `malloc` has a paired free path.
- No use-after-free — null state before `free`.
- Functions under 60 lines, single responsibility.
- Tests required before commit.

## Testing

```bash
make test
make smoke-ci
```

Interactive terminal validation:

```bash
make smoke
make smoke-linux-vc
```

Tests run without requiring `initscr`; interactive smoke targets require a PTY.
When a sibling/shared `retrocore-spec` checkout is available, CMake also enables
the optional shared fixture test.

## Troubleshooting

### Build fails with "curses.h not found"

```bash
sudo apt install libncurses-dev   # Debian / Ubuntu
brew install ncurses              # macOS
make clean && make
```

### Unsupported terminal or weird rendering

```bash
./build/retrodesk --render-backend=ansi
echo $TERM
TERM=xterm ./build/retrodesk
```

### Mouse not working

```bash
./build/retrodesk --input-backend=tty
```

## Documentation

- [docs/INDEX.md](docs/INDEX.md) — recommended reading order.
- [docs/FOUNDATION_PRINCIPLES.md](docs/FOUNDATION_PRINCIPLES.md)
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- [docs/PORTABILITY.md](docs/PORTABILITY.md)
- [docs/BUILD_SYSTEM.md](docs/BUILD_SYSTEM.md)
- [docs/APP_RUNTIME.md](docs/APP_RUNTIME.md)
- [docs/RETROCORE_SPEC.md](docs/RETROCORE_SPEC.md)
- [docs/RETROTUI_GAP.md](docs/RETROTUI_GAP.md)
- [docs/TESTING.md](docs/TESTING.md)
- [docs/ROADMAP.md](docs/ROADMAP.md)
- [docs/RELEASE_0.1_CHECKLIST.md](docs/RELEASE_0.1_CHECKLIST.md)
- [docs/CODE_STANDARDS.md](docs/CODE_STANDARDS.md)
- [docs/TECH_DEBT_POLICY.md](docs/TECH_DEBT_POLICY.md)
- [docs/CORE_HARDENING_PLAN.md](docs/CORE_HARDENING_PLAN.md)
- [docs/AGENTS.md](docs/AGENTS.md)

## License

MIT — see [LICENSE](LICENSE).

## Acknowledgements

- [RetroTUI](https://github.com/roccolate/retrotui) — the Python original this
  project is the C rewrite of.
- The contributors who audited the runtime and built the UI widget library.
