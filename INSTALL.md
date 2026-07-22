# Installation and Quick Start

> RetroDesk is pre-alpha. Linux is the recommended interactive development
> profile. Windows and DOS have native storage implementations and automated
> validation, but their release maturity still depends on exact-candidate manual
> evidence.

## Linux (Recommended)

### Prerequisites

```bash
# Ubuntu / Debian
sudo apt update
sudo apt install -y build-essential cmake libncurses-dev

# Fedora
sudo dnf install -y gcc cmake ncurses-devel

# Arch Linux
sudo pacman -S base-devel cmake ncurses
```

### Build and run

```bash
git clone https://github.com/roccolate/retrodesk.git
cd retrodesk
make
./build/retrodesk
```

Equivalent direct CMake:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/retrodesk
```

Linux uses the native POSIX storage adapter for File Manager and Notepad.

## First Run

RetroDesk opens File Manager by default and renders a themed taskbar at the
bottom. The exact colors and selection treatment depend on the active theme.

Typical wide layout:

```text
Apps | Files | Notepad | Diag                              19:42:33
```

Essential controls:

```text
F1/F10   open Launcher
F6       focus next eligible window
F8       maximize or restore active window
F9       enter keyboard move mode
Ctrl+W   request close of active app
Ctrl+Q   coordinated shutdown
```

In `F9` mode:

- arrows adjust position or size;
- `Tab` toggles MOVE/RESIZE;
- `Enter`, `F9`, or `Esc` finish;
- a HUD above the taskbar displays mode and geometry.

See [docs/KEYBOARD_SHORTCUTS.md](docs/KEYBOARD_SHORTCUTS.md).

## Themes and Backends

```bash
# Themes
./build/retrodesk --theme=xp
./build/retrodesk --theme=hacker
./build/retrodesk --theme=amiga
./build/retrodesk --theme=win31

# Render backends
./build/retrodesk --render-backend=curses
./build/retrodesk --render-backend=ansi

# Input backends
./build/retrodesk --input-backend=curses
./build/retrodesk --input-backend=tty

# Diagnostics
./build/retrodesk --diagnose
./build/retrodesk --bench-startup
./build/retrodesk --help
```

Curses is the recommended interactive path. Raw TTY + ANSI is an experimental
fallback/bring-up profile and requires terminal-specific acceptance before a
release claim.

## Desktop Quick Tour

### Launcher

- `F1` or `F10`: open.
- arrows or `W/S/J/K`: navigate.
- Home/End: first/last action.
- Enter/Space: activate.
- `F/N/D/X`: direct accelerators.
- Esc/Q/F10: close.

The Launcher is anchored above the bottom-left taskbar and remains in the one
Desktop event loop.

### Taskbar

- Click Apps: toggle Launcher.
- Click a closed app: launch.
- Click a running background app: focus/raise.
- Click the focused app: minimize.
- Click a minimized app: restore/focus/raise.
- Multiple Notepad instances cycle.

Labels compact automatically. A general overflow surface for a future larger app
catalog is not yet implemented.

### File Manager

- arrows or `J/K`: move selection;
- Page Up/Down, Home/End: viewport navigation;
- Enter: open directory or regular validated text file;
- Backspace: parent directory;
- `F2`: rename;
- `F5`: refresh;
- `F7`: new directory;
- `F8`: new empty file;
- `H`: toggle hidden files.

### Notepad

- Shift+arrows: select;
- `Ctrl+A/C/X/V`: select all/copy/cut/paste;
- `Ctrl+Z/Y`: undo/redo;
- `Ctrl+F`: Find; Enter selects next result;
- `Ctrl+S`: Save;
- `F3`: Save As;
- `F4`: Word Wrap;
- `Ctrl+W`: close with Save/Discard/Cancel when dirty.

Native File/Edit/View menu chrome, `F11`, `Ctrl+N`, and `Ctrl+O` are proposed in
open PR #24 and are not integrated on current `main`.

## Local Development Gates

```bash
make clean
make check-test-oracles
make strict
make test
make test-all
make test-sanitize
make smoke-ci
```

Interactive PTY gates:

```bash
make smoke
make smoke-linux-vc
```

`make smoke-linux-vc` validates the keyboard-first `TERM=linux` policy. Interactive
gates must be run in an actual PTY and recorded against the exact release
candidate.

## Windows with PDCurses

Native Win32 storage is integrated. Windows Debug and Release builds/tests run in
CI under `/W4 /WX`, including native storage contract tests.

A representative local configuration:

```powershell
git clone https://github.com/roccolate/retrodesk.git
cd retrodesk
cmake -S . -B build -DPDCURSES_ROOT=C:/path/to/PDCurses `
  -DENABLE_TESTS=ON -DENABLE_STRICT_WARNINGS=ON -DENABLE_WERROR=ON
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -C Debug
cmake --build build --config Release
ctest --test-dir build --output-on-failure -C Release
```

The Win32 adapter provides UTF-8 public paths converted to UTF-16, Unicode
filenames/data, deterministic listing, version conflicts, and native replacement
APIs. Text operations reject reparse points.

Before claiming Windows release parity, record native PDCurses interaction for:

- taskbar/Launcher;
- focus, minimize, maximize, and `F9` mode;
- Unicode File Manager/Notepad workflows;
- close/shutdown and console restoration.

## DOS with DJGPP/PDCurses

DOS is a validated **reduced** native profile. The repository workflow pins and
builds DJGPP, PDCurses, and CWSDPMI, builds `retrodesk.exe` and `FSTEST.EXE`, runs
storage and application diagnostics in DOSBox-X, and produces a distribution
artifact.

The specialized source list is in `Makefile.djgpp` and must remain synchronized:

```bash
python3 scripts/check_djgpp_sources.py
# or
make check-build-sources
```

Important DOS limits:

- ASCII/DOS-oriented filenames and paths;
- validated UTF-8 document content;
- case-insensitive filename behavior;
- backup/restore replacement rather than a universal atomic replace syscall;
- reduced memory/profile expectations;
- no raw POSIX TTY path;
- no Tier 1 feature-parity claim.

For ordinary development, use the official CI workflow rather than assuming a
host-local DJGPP/PDCurses environment matches the pinned profile.

## macOS

Expected development prerequisites:

```bash
brew install cmake ncurses
git clone https://github.com/roccolate/retrodesk.git
cd retrodesk
make
```

The intended storage path is the POSIX adapter, but no current macOS build/test or
interactive evidence is attached to the project snapshot. macOS remains
experimental and unclaimed.

## WSL2

WSL2 follows the Linux instructions:

```bash
sudo apt install -y build-essential cmake libncurses-dev
git clone https://github.com/roccolate/retrodesk.git
cd retrodesk
make
./build/retrodesk
```

Treat WSL terminal/mouse behavior as its own interactive environment when
collecting evidence.

## Troubleshooting

### `curses.h` not found

```bash
# Debian / Ubuntu
sudo apt install libncurses-dev

# Fedora
sudo dnf install ncurses-devel

# macOS
brew install ncurses
```

Then remove stale configuration:

```bash
make clean
make
```

### CMake references another checkout

CMake caches absolute paths. Delete the copied/moved build tree or run
`make clean`; never use a stale tree as release evidence.

### Unsupported terminal or odd rendering

```bash
./build/retrodesk --render-backend=ansi --input-backend=tty
echo "$TERM"
TERM=xterm ./build/retrodesk
```

The ANSI/raw-TTY path is diagnostic/experimental. Report the terminal emulator,
`TERM`, locale, backend combination, and exact commit when filing an issue.

### Mouse/drag unavailable

Mouse, drag, right-click, and double-click are capability-gated. All essential
Desktop and app workflows remain keyboard-operable. On Linux virtual console,
keyboard-first behavior is intentional.

### Terminal state is wrong after forced termination

Normal shutdown and initialization rollback restore the captured terminal mode.
An externally killed process may not execute cleanup:

```bash
stty sane
reset
```

## Docker (Optional Linux Build)

```dockerfile
FROM ubuntu:22.04
RUN apt update && apt install -y build-essential cmake libncurses-dev
WORKDIR /app
COPY . .
RUN make clean && make
CMD ["./build/retrodesk"]
```

Run with a PTY:

```bash
docker build -t retrodesk .
docker run --rm -it retrodesk
```

Docker build success does not replace native terminal acceptance.

## Next Reading

1. [docs/PROJECT_STATUS.md](docs/PROJECT_STATUS.md)
2. [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md)
3. [docs/KEYBOARD_SHORTCUTS.md](docs/KEYBOARD_SHORTCUTS.md)
4. [docs/BUILTIN_APPS.md](docs/BUILTIN_APPS.md)
5. [docs/PORTABILITY.md](docs/PORTABILITY.md)
6. [docs/TESTING.md](docs/TESTING.md)
7. [docs/RELEASE_0.1_CHECKLIST.md](docs/RELEASE_0.1_CHECKLIST.md)