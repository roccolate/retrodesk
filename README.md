# RetroDesk

A retro desktop environment for the terminal. Think Windows 3.1 / 95 in your
console, written in C11 with strict layering, platform abstraction, and a
documented contract.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Architecture: C11 / curses](https://img.shields.io/badge/C-C11%20%2F%20curses-blue.svg)]()
[![Status: v1.0 in progress](https://img.shields.io/badge/status-v1.0%20in%20progress-orange.svg)]()

---

## What Is It

RetroDesk is a portable terminal desktop runtime in C11. It hosts multiple
apps inside a window manager, supports a real keyboard / mouse event flow,
and renders either through native curses or an ANSI fallback with frame
diffing. The runtime layers cleanly into `core`, `platform`, `render`, `wm`,
`app`, `apps`, and `ui`, so each subsystem has a single responsibility.

It is the C rewrite of [RetroTUI](https://github.com/roccolate/retrotui):
same ideas, smaller surface, multi-window from the start, and a real
portability policy across Linux, macOS, Windows, and DOS (DJGPP).

## Status

`v1.0` is in progress. The runtime is at a usable prototype: real window
manager, dual render backend, theme system, and a small UI widget library
(text input, scroll list, text buffer, button, dialog, status bar). Apps
are still mostly stubs (file manager, notepad, terminal).

See [docs/ROADMAP.md](docs/ROADMAP.md) for the v1.0 plan and
[docs/CODE_STANDARDS.md](docs/CODE_STANDARDS.md) for the coding rules.

## Features

- **Window manager** — drag, focus, z-order, modals, minimize / maximize.
- **Dual render backend** — native curses or ANSI fallback with frame diffing.
- **Themeable** — XP, Hacker, Amiga, Win31 included.
- **UI widget library** — text input, scrollable list, multi-line buffer,
  button, dialog, menu bar, status bar.
- **Apps** — File Manager, Notepad, Terminal (PTY), Settings.
- **Keyboard and mouse** — full support on Linux, macOS, Windows (PDCurses)
  and DOS (DJGPP).

## Build

### Requirements

- **Linux / macOS** — `ncurses` (`apt install libncurses-dev` or
  `brew install ncurses`).
- **Windows** — PDCurses; see `CMakeLists.txt` for the `PDCURSES_ROOT`
  setup, or use vcpkg.
- **Compiler** — GCC or Clang with C11 (`-std=c11`).

### Build

```bash
git clone https://github.com/roccolate/retrodesk.git
cd retrodesk
cmake -S . -B build
cmake --build build
```

The repository `Makefile` is a thin CMake wrapper for ergonomics:

```bash
make                # build via CMake
make test           # build and run ctest
make smoke          # interactive PTY smoke test
make smoke-linux-vc # TERM=linux keyboard-first smoke test
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
./build/retrodesk --render-backend=curses   # native curses (default on ttys)
./build/retrodesk --render-backend=ansi     # ANSI fallback
./build/retrodesk --input-backend=curses    # ncurses input (default)
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

```
src/
├── main.c                 # Entry point
├── core/
│   ├── cli.c              # Command-line parsing
│   ├── desktop.c          # Desktop runtime, launcher, frame loop
│   └── event.h            # RetroEvent / RetroKeyEvent / RetroMouseEvent
├── platform/
│   ├── platform.c         # Platform abstraction
│   ├── platform_curses.c  # ncurses backend
│   ├── platform_tty_raw.c # Raw TTY + ANSI input
│   └── tty_decoder.c      # TTY escape sequence parser
├── render/
│   ├── render.c           # curses + ANSI dispatcher
│   └── ansi_renderer.c    # ANSI renderer with frame diff
├── wm/
│   └── window_manager.c   # Window manager
├── app/
│   └── app_runtime.c      # App lifecycle + registry
├── ui/
│   ├── text_input.c       # Single-line text input
│   ├── text_buffer.c      # Multi-line text buffer
│   ├── scroll_list.c      # Scrollable list widget
│   ├── button.c           # Button widget
│   ├── dialog.c           # Dialog system
│   ├── theme.c            # Theme system
│   └── statusbar.c        # Status bar
└── apps/
    ├── filemanager_app.c  # File manager
    ├── notepad_app.c      # Notepad
    └── terminal_app.c     # Terminal
```

## Development

### Add a Widget

1. Create `src/ui/mywidget.{h,c}`.
2. Follow the patterns in `text_input.c` — lifecycle (`create` / `destroy`),
   state, events, render to `DrawList`.
3. Add the source to `CMakeLists.txt` (`RETRODESK_SOURCES`).
4. Write unit tests in `tests/mywidget_test.c` and register them under
   `BUILD_TESTING`.
5. Build with `-Werror` clean and pass `make test`.

### Coding standards

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
```

Tests run without a terminal (no `initscr`).

## Troubleshooting

### Build fails with "curses.h not found"

```bash
sudo apt install libncurses-dev   # Debian / Ubuntu
brew install ncurses             # macOS
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
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- [docs/FOUNDATION_PRINCIPLES.md](docs/FOUNDATION_PRINCIPLES.md)
- [docs/BUILD_SYSTEM.md](docs/BUILD_SYSTEM.md)
- [docs/APP_RUNTIME.md](docs/APP_RUNTIME.md)
- [docs/TESTING.md](docs/TESTING.md)
- [docs/PORTABILITY.md](docs/PORTABILITY.md)
- [docs/ROADMAP.md](docs/ROADMAP.md)
- [docs/CODE_STANDARDS.md](docs/CODE_STANDARDS.md)

## License

MIT — see [LICENSE](LICENSE).

## Acknowledgements

- [RetroTUI](https://github.com/roccolate/retrotui) — the Python original this
  project is the C rewrite of.
- The contributors who audited the runtime and built the UI widget library.