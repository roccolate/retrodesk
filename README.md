# RetroDesk

A retro desktop environment for the terminal. Think Windows 3.1 / 95 in your
console, written in C11 with strict layering, platform abstraction, and a
documented contract.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Architecture: C11 / curses](https://img.shields.io/badge/C-C11%20%2F%20curses-blue.svg)]()
[![Status: pre-alpha](https://img.shields.io/badge/status-pre--alpha-orange.svg)]()

---

## What Is It

RetroDesk is an experimental terminal desktop runtime in C11. It already hosts
multiple windows, has curses and ANSI render paths, and includes an early Linux
preview of File Manager and Notepad workflows. It is not a stable release. The
code is being hardened around explicit `core`, `platform`, `render`, `wm`,
`app`, `apps`, `ui`, and `storage` layers.

It is the C rewrite of [RetroTUI](https://github.com/roccolate/retrotui), used
as a behavior reference rather than an implementation template.

## Status

The repository is pre-alpha. The window manager, render abstraction, themes,
and UI widgets are functional foundations. File Manager and Notepad now cover a
small Linux/POSIX preview path: browsing directories, opening regular text
files, editing text, and bounded atomic saves. The diagnostics app is currently
a read-only runtime view: it does not provide a shell or PTY.

Linux is the active development and validation profile. Native Windows is a
planned Tier 1 target, but the current storage adapter is POSIX-only; Windows,
macOS, and DOS paths are not release claims until their native gates are
exercised and recorded.

See [docs/ROADMAP.md](docs/ROADMAP.md) for the v1.0 plan and
[docs/CODE_STANDARDS.md](docs/CODE_STANDARDS.md) for the coding rules.

## Preview

![RetroDesk Linux/POSIX preview with File Manager](docs/assets/retrodesk-preview.png)

## Features

- **Window manager** — drag, focus, z-order, modal routing, move, and close.
- **Dual render backend** — native curses or ANSI fallback with frame diffing.
- **Themeable** — XP, Hacker, Amiga, Win31 included.
- **UI widget library** — text input, scrollable list, multi-line buffer,
  button, dialog, menu bar, status bar.
- **Built-in app preview** — File Manager, Notepad, and Diagnostics.
- **Storage preview** — POSIX directory listing, ASCII text read, and atomic
  save with conflict detection.
- **Input** — keyboard baseline and capability-gated mouse behavior; terminal
  support varies by backend and remains under test.

## Build

### Verified development requirements

- **Linux** — `ncurses` (`apt install libncurses-dev`).
- **Compiler** — GCC or Clang with C11 (`-std=c11`).

Other build profiles are documented in [INSTALL.md](INSTALL.md), but are not
release-supported yet.

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
| `F1` | Help |
| `F2` | Launcher |
| `F6` | Next window |
| `F7` | Move/resize mode |
| `Ctrl+W` | Close active window |
| `Ctrl+Q` | Quit |
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
├── storage/
│   ├── retro_fs.h         # Storage contract for file apps
│   └── retro_fs_posix.c   # POSIX adapter used by Linux preview builds
└── apps/
    ├── filemanager_app.c  # File manager
    ├── notepad_app.c      # Notepad
    └── terminal_app.c     # Diagnostics app (not a shell)
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

## Current limitations

- File Manager and Notepad cover a Linux/POSIX preview path only.
- File Manager has basic directory navigation and text-file open; destructive
  filesystem operations and full scrollable-list polish are absent.
- Notepad supports edit/open/save/Save As basics; discard dialogs and broader
  document UX are still incomplete.
- Destructive filesystem operations (delete, rename, copy, move) are outside v1.
- Diagnostics is not a shell.
- Minimize, maximize, Settings, plugins, and session persistence are absent.
- The visible text contract is byte/ASCII oriented; Unicode is not supported.

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
- [docs/STORAGE.md](docs/STORAGE.md)
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
