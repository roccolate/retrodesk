# RetroDesk

A retro desktop environment for the terminal. Think Windows 3.1 / 95 in your
console, written in C11 with strict layering, platform abstraction, and a
documented contract.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Architecture: C11 / curses](https://img.shields.io/badge/C-C11%20%2F%20curses-blue.svg)]()
[![Status: pre-alpha](https://img.shields.io/badge/status-pre--alpha-orange.svg)]()

---

## What Is It

RetroDesk is an experimental terminal desktop runtime in C11. It hosts multiple
windows, supports curses and ANSI render paths, and now includes a useful
Linux/POSIX product slice for File Manager and Notepad. It is still pre-alpha:
release claims remain narrower than the code that compiles in CI.

It is the C rewrite of [RetroTUI](https://github.com/roccolate/retrotui).
RetroTUI is used as a behavior reference and risk catalog, not as the
architecture template.

## Current State

The runtime is layered around `core`, `platform`, `render`, `wm`, `app`, `apps`,
`ui`, and `storage`.

Implemented product workflows include:

- an interactive bottom taskbar with Launcher, app state, instance counts, and
  clock;
- keyboard-first multi-window focus and move/resize behavior;
- File Manager navigation, scrolling, hidden-file toggle, refresh, open,
  rename, new directory, and new file;
- UTF-8 Notepad editing, selection, shared clipboard, undo/redo, Find, soft Word
  Wrap, Save, Save As, and safe dirty-document close handling;
- validated UTF-8 reads and atomic POSIX writes with conflict detection;
- Diagnostics as a read-only runtime view, not a shell or PTY.

Linux/POSIX remains the complete file-app profile. Windows Debug and Release are
built and tested in CI for portable runtime behavior, but equivalent File
Manager/Notepad storage support still requires a native Windows adapter or
explicit product gating.

See [docs/ROADMAP.md](docs/ROADMAP.md) for the remaining work and
[docs/CODE_STANDARDS.md](docs/CODE_STANDARDS.md) for coding rules.

## Preview

![RetroDesk Linux/POSIX preview with File Manager](docs/assets/retrodesk-preview.png)

## Features

- **Window manager** — focus, z-order, modal routing, drag, move, resize, and
  lifecycle-aware close.
- **Dual render backend** — native curses or ANSI fallback with frame diffing.
- **Themeable** — XP, Hacker, Amiga, and Win31 themes.
- **Interactive taskbar** — Apps menu, pinned app entries, focused/running
  states, instance counts, clock, hit testing, and compact labels.
- **UI widget library** — UTF-8 text input, selection-aware multi-line buffer,
  scrollable list, button, dialog, menu bar, progress bar, tabs, and status bar.
- **File Manager** — keyboard viewport, directory traversal, text-file open,
  refresh, hidden files, rename, create directory, and create empty file.
- **Notepad** — UTF-8 editing, selection, clipboard, undo/redo, Find, Word Wrap,
  atomic save, Save As, and Save/Discard/Cancel close flow.
- **Storage contract** — bounded directory listing, validated UTF-8 text,
  exclusive creation, rename, atomic writes, and stale-version detection.
- **Portable input contract** — curses and raw-TTY event translation, modifier
  decoding, keyboard-first degradation, and restored terminal state.

## Build

### Verified development requirements

- **Linux** — `ncurses` development headers (`apt install libncurses-dev`).
- **Compiler** — GCC or Clang with C11 (`-std=c11`).

Other profiles are documented in [INSTALL.md](INSTALL.md) and
[docs/PORTABILITY.md](docs/PORTABILITY.md). They are not all equivalent product
profiles.

### Build commands

```bash
git clone https://github.com/roccolate/retrodesk.git
cd retrodesk
cmake -S . -B build
cmake --build build
```

The repository `Makefile` is a thin CMake wrapper:

```bash
make                # build via CMake
make test           # build and run ctest
make smoke          # interactive PTY smoke test
make smoke-linux-vc # TERM=linux keyboard-first smoke test
make smoke-ci       # non-interactive smoke path
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
./build/retrodesk --render-backend=curses
./build/retrodesk --render-backend=ansi
./build/retrodesk --input-backend=curses
./build/retrodesk --input-backend=tty

# Diagnostics
./build/retrodesk --bench-startup
./build/retrodesk --help
```

## Essential Hotkeys

| Key | Action |
| --- | --- |
| `F1` or `F10` | Open the Launcher |
| `F6` | Focus the next window |
| `F9` | Enter move/resize mode |
| `Ctrl+W` | Request close of the active app window |
| `Ctrl+Q` | Quit RetroDesk |

In move/resize mode, arrow keys apply the operation, `Tab` switches between move
and resize, and `Esc` exits the mode.

Application-specific shortcuts are documented in
[docs/KEYBOARD_SHORTCUTS.md](docs/KEYBOARD_SHORTCUTS.md).

## Taskbar

The bottom status widget acts as an interactive taskbar:

```text
[Apps] [Files*] [Notepad:2.] [Diag ]                         19:42:33
```

`*` marks the focused app, `.` marks a running background app, and `:N` shows
multiple instances. Clicking an entry launches, focuses, or cycles the app.

## Project Structure

```text
src/
├── main.c                 # Entry point
├── core/
│   ├── cli.c              # Command-line parsing
│   ├── clipboard.c        # Shared validated UTF-8 clipboard
│   ├── desktop.c          # Desktop runtime, Launcher, taskbar integration
│   ├── key_chord.c        # Portable key constants/chords
│   ├── utf8.c             # UTF-8 decoding, validation, and cell width
│   └── event.h            # Normalized key/mouse events and modifiers
├── platform/
│   ├── platform.c         # Platform abstraction
│   ├── platform_curses.c  # ncurses/PDCurses backend
│   ├── platform_tty_raw.c # Raw TTY + ANSI input
│   └── tty_decoder.c      # Escape sequence and modifier decoder
├── render/
│   ├── render.c           # Backend dispatcher
│   └── ansi_renderer.c    # ANSI renderer with frame diff
├── wm/
│   └── window_manager.c   # Window ownership, focus, z-order, move/resize
├── app/
│   └── app_runtime.c      # App lifecycle and registry
├── ui/
│   ├── text_input.c       # UTF-8 single-line input
│   ├── text_buffer.c      # UTF-8 multi-line editor core
│   ├── scroll_list.c      # Scrollable list widget
│   ├── statusbar.c        # Status bar and interactive taskbar view
│   └── theme.c            # Theme system
├── storage/
│   ├── retro_fs.h         # File-app storage contract
│   └── retro_fs_posix.c   # Active POSIX adapter
└── apps/
    ├── filemanager_app.c  # File Manager
    ├── notepad_app.c      # Notepad
    └── terminal_app.c     # Diagnostics, not a shell
```

## Development

### Add a widget

1. Create `src/ui/mywidget.{h,c}`.
2. Follow the lifecycle and draw-list patterns in existing widgets.
3. Add the source to `CMakeLists.txt` and, when applicable, the DJGPP manifest.
4. Add an always-active test under `tests/`.
5. Build warning-clean with strict flags and run the relevant test matrix.

### Coding standards

See [docs/CODE_STANDARDS.md](docs/CODE_STANDARDS.md). Highlights:

- C11 with `-Wall -Wextra -Wpedantic -Werror`.
- Null-safe and bounds-safe interfaces.
- Explicit ownership and paired cleanup.
- No backend-native types in public domain headers.
- No nested modal loops.
- Tests required for behavior changes.

## Testing

```bash
make check-test-oracles
make test
make test-all
make smoke-ci
```

Unit and integration tests run without `initscr()`. Interactive terminal behavior
still requires PTY smoke testing before a release claim.

## Current Limitations

- Complete filesystem-backed workflows are POSIX-only.
- File Manager does not yet provide delete/trash, copy, move, recursive
  operations, previews, bookmarks, or dual-pane mode.
- Notepad does not yet provide Replace, system clipboard integration, rich text,
  syntax highlighting, multiple encodings, or very-large-file editing.
- Find preserves accents and does not perform Unicode normalization.
- Diagnostics is not a terminal emulator.
- Minimize, maximize, Settings, plugins, and session persistence are absent.
- ANSI/raw input and full Unicode terminal-width behavior need broader manual
  terminal validation.

## Troubleshooting

### Build fails with `curses.h not found`

```bash
sudo apt install libncurses-dev   # Debian / Ubuntu
brew install ncurses             # macOS
make clean && make
```

### Unsupported terminal or odd rendering

```bash
./build/retrodesk --render-backend=ansi
echo $TERM
TERM=xterm ./build/retrodesk
```

### Mouse not working

```bash
./build/retrodesk --input-backend=tty
```

Mouse behavior is capability-gated. The Linux virtual console remains
keyboard-first when reliable pointer behavior is unavailable.

## Documentation

- [docs/INDEX.md](docs/INDEX.md) — recommended reading order.
- [docs/BUILTIN_APPS.md](docs/BUILTIN_APPS.md) — File Manager, Notepad, and
  Diagnostics behavior.
- [docs/KEYBOARD_SHORTCUTS.md](docs/KEYBOARD_SHORTCUTS.md) — complete input and
  taskbar reference.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- [docs/STORAGE.md](docs/STORAGE.md)
- [docs/TESTING.md](docs/TESTING.md)
- [docs/PORTABILITY.md](docs/PORTABILITY.md)
- [docs/ROADMAP.md](docs/ROADMAP.md)
- [docs/CODE_STANDARDS.md](docs/CODE_STANDARDS.md)

## License

MIT — see [LICENSE](LICENSE).

## Acknowledgements

- [RetroTUI](https://github.com/roccolate/retrotui) — behavior reference for the
  C rewrite.
- ArmoniOS — source of the taskbar behavior model reused under the project’s MIT
  licensing decision.
- The contributors who audited the runtime and built the widget/test foundation.
