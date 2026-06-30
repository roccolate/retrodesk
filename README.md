# RetroDesk 🐾

A retro desktop environment for the terminal. Think Windows 3.1/95 in your console.

## Features

- **Real Window Manager** — drag windows, z-order, focus, minimize/maximize, modals
- **Dual Render Backend** — native curses or ANSI fallback with frame diffing
- **Themeable** — XP, Hacker, Amiga, Win31 included
- **UI Widget Library** — text input, scrollable lists, buttons, dialogs, menu bars
- **Apps** — File Manager, Notepad, Terminal (PTY), Settings
- **Keyboard & Mouse** — full support on Linux/macOS/Windows (PDCurses)

## Build

```bash
git clone https://github.com/roccolate/retrodesk.git
cd retrodesk
make
./build/retrodesk
```

### Requirements

- **Linux/macOS:** ncurses (`apt install libncurses-dev` or `brew install ncurses`)
- **Windows:** PDCurses (see `CMakeLists.txt` for `PDCURSES_ROOT` setup)
- **Compiler:** GCC/Clang (C11, `-std=c11`)

## Usage

```bash
# Run with default theme (XP)
./build/retrodesk

# Change theme
./build/retrodesk --theme=hacker
./build/retrodesk --theme=amiga
./build/retrodesk --theme=win31

# Choose input backend
./build/retrodesk --input-backend=curses    # ncurses (default on ttys)
./build/retrodesk --input-backend=tty       # raw mode + ANSI

# Render backend
./build/retrodesk --render-backend=curses   # native curses
./build/retrodesk --render-backend=ansi     # ANSI fallback

# Benchmark mode (startup diagnostics)
./build/retrodesk --bench-startup

# Help
./build/retrodesk --help
```

## Hotkeys

| Key | Action |
|-----|--------|
| `q` | Quit |
| `m` | Toggle launcher |
| `1/2/3` | Launch File Manager / Notepad / Terminal |
| `Tab` | Cycle focus |
| `HJKL` | Move active window |
| `w` | Close active window |
| Esc | Close app (when focused) |

## Architecture

```
src/
├── main.c                 # Entry point
├── core/
│   ├── cli.c             # Command-line parsing
│   ├── desktop.c         # Desktop runtime + app launcher
│   └── event.h           # Event types
├── platform/
│   ├── platform.c        # Platform abstraction
│   ├── platform_curses.c # ncurses backend
│   ├── platform_tty_raw.c# Raw mode + ANSI input
│   └── tty_decoder.c     # TTY escape sequence parsing
├── render/
│   ├── render.c          # Curses + ANSI rendering
│   ├── ansi_renderer.c   # ANSI renderer with frame diff
│   └── ansi_*.c          # ANSI helpers
├── wm/
│   └── window_manager.c  # Window manager (drag, focus, z-order)
├── app/
│   └── app_runtime.c     # App lifecycle + registry
├── ui/
│   ├── text_input.c      # Single-line text input
│   ├── text_buffer.c     # Multi-line text buffer
│   ├── scroll_list.c     # Scrollable list widget
│   ├── button.c          # Button widget
│   ├── dialog.c          # Dialog system
│   ├── theme.c           # Theme system
│   └── statusbar.c       # Status bar
└── apps/
    ├── filemanager_app.c # File manager
    ├── notepad_app.c     # Notepad
    └── terminal_app.c    # Terminal
```

## Development

### Add a Widget

1. Create `src/ui/mywidget.{h,c}`
2. Follow patterns from `text_input.c` — lifecycle (create/destroy), state, events, render
3. Add to `CMakeLists.txt` in `RETRODESK_SOURCES`
4. Write unit tests in `tests/mywidget_test.c`
5. Add test to CMakeLists.txt
6. `make clean && make` — must pass `-Werror`
7. `make test` — all must pass
8. Commit and push

### Code Standards

- **C11, `-Wall -Wextra -Wpedantic -Werror`** — zero warnings
- **Null-safe:** Every pointer parameter checked
- **Bounds-safe:** All buffer writes validated
- **Leak-free:** Every malloc has a free path
- **No use-after-free:** Null state before free
- **Functions:** Under 60 lines, one responsibility
- **Tests:** Unit tests required before commit

## Testing

```bash
make test
```

All 14+ tests must pass. Tests run without a terminal (no initscr).

## Roadmap to v1.0

- ✅ Text Input widget
- ✅ Scrollable List widget
- ✅ Text Buffer widget
- ✅ Button widget
- 🚧 Dialog system
- [ ] Menu bar
- [ ] Progress bar
- [ ] Tab widget
- [ ] Window resize
- [ ] Minimize/maximize
- [ ] Unicode box drawing
- [ ] Real File Manager
- [ ] Real Notepad with file I/O
- [ ] Terminal with PTY
- [ ] Settings app
- [ ] Taskbar
- [ ] Desktop background
- [ ] Right-click context menu
- [ ] Config file (~/.retrodeskrc)
- [ ] v1.0 release

See `docs/ROADMAP.md` for the v1.0 roadmap and `docs/CODE_STANDARDS.md` for the coding standards.

## Contributing

1. Fork the repo
2. Create a feature branch: `git checkout -b feature/my-widget`
3. Make your changes (follow code standards above)
4. `make clean && make && make test` — must all pass
5. Commit with a clear message: `feat: add my widget with tests`
6. Push and submit a PR

## Troubleshooting

### Build fails with "curses.h not found"
```bash
# Ubuntu/Debian
sudo apt install libncurses-dev

# macOS
brew install ncurses

# Then rebuild
make clean && make
```

### "Unsupported terminal" or weird rendering
- Try the ANSI backend: `./build/retrodesk --render-backend=ansi`
- Check your `TERM` variable: `echo $TERM`
- Try `xterm` mode: `TERM=xterm ./build/retrodesk`

### Mouse not working
- Ensure your terminal supports mouse (most modern ones do)
- Try raw mode: `./build/retrodesk --input-backend=tty`

## Performance

- **ANSI renderer:** Frame diffing means only changed cells are sent to terminal
- **Input polling:** 120ms timeout between checks (configurable)
- **Memory:** ~2-5MB per app window
- **CPU:** Minimal, event-driven (no busy loops)

## License

MIT — see [LICENSE](LICENSE).

## Authors

- **Rocco** (@roccolate) — Lead developer, RetroTUI origin
- **Armin** — Code audit, UI widgets, automation setup

---

**Build it, run it, make it yours.** 🐾

For questions or issues: https://github.com/roccolate/retrodesk/issues
