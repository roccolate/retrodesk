# Installation and Quick Start

## Linux (recommended product profile)

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

The complete current File Manager and Notepad storage workflow uses the POSIX
adapter in `src/storage/retro_fs_posix.c`.

## First Run

RetroDesk opens File Manager by default and displays an interactive taskbar on
the bottom row.

```text
[Apps] [Files*] [Notepad ] [Diag ]                          19:42:33
```

Essential desktop controls:

```text
F1 or F2  open Launcher
F6        focus next window
F9        enter move/resize mode
Ctrl+W    close active app window
Ctrl+Q    quit RetroDesk
```

In move/resize mode, use arrow keys, press `Tab` to switch between moving and
resizing, and press `Esc` to leave the mode.

See [docs/KEYBOARD_SHORTCUTS.md](docs/KEYBOARD_SHORTCUTS.md) for File Manager,
Notepad, Launcher, prompt, and taskbar controls.

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
```

Curses is the recommended interactive path. ANSI/raw-TTY remains useful for
bring-up and fallback testing, but terminal-specific smoke evidence is still
required before release claims.

## Built-In App Quick Tour

### File Manager

- Arrow keys or `J/K`: move selection.
- `Page Up/Page Down`, `Home/End`: viewport navigation.
- `Enter`: open directory or regular text file.
- `Backspace`: parent directory.
- `F2`: rename.
- `F5`: refresh.
- `F7`: new directory.
- `F8`: new empty file.
- `H`: toggle hidden files.

### Notepad

- `Shift` + arrows: select text.
- `Ctrl+A/C/X/V`: select all, copy, cut, paste.
- `Ctrl+Z/Y`: undo/redo.
- `Ctrl+F`: Find; `Enter` moves to the next result.
- `Ctrl+S`: save.
- `F3`: Save As.
- `F4`: toggle Word Wrap.
- `Ctrl+W`: close; dirty documents offer Save/Discard/Cancel.

See [docs/BUILTIN_APPS.md](docs/BUILTIN_APPS.md) for implemented workflows and
current exclusions.

## Development and Test Builds

### Standard tests

```bash
make clean
make
make test
```

### Full local gate

```bash
make clean
make strict
make test
make test-all
make smoke-ci
```

`make smoke` and `make smoke-linux-vc` require an interactive PTY and remain
manual gates.

### Direct CMake Debug build

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/retrodesk
```

### Strict build

```bash
cmake -S . -B build \
  -DENABLE_STRICT_WARNINGS=ON \
  -DENABLE_WERROR=ON \
  -DENABLE_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Tests are non-interactive and must pass without modifying `TERM`.

## Experimental Profiles

Linux/POSIX is the complete current product slice. Other recipes are bring-up or
portable-runtime validation paths unless exact product support is documented.

### WSL2

WSL2 follows the Linux instructions:

```bash
sudo apt install -y build-essential cmake libncurses-dev
git clone https://github.com/roccolate/retrodesk.git
cd retrodesk
make
./build/retrodesk
```

### Native Windows with PDCurses

Windows Debug and Release portable-runtime builds are exercised in CI, but a
native storage adapter or deterministic File Manager/Notepad gating is still
required before equivalent product support is claimed.

```bash
# Build PDCurses separately, then:
git clone https://github.com/roccolate/retrodesk.git
cd retrodesk
cmake -S . -B build -DPDCURSES_ROOT=../PDCurses
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -C Debug
cmake --build build --config Release
ctest --test-dir build --output-on-failure -C Release
```

### macOS

```bash
brew install cmake ncurses
git clone https://github.com/roccolate/retrodesk.git
cd retrodesk
make
```

macOS remains experimental until current build and interactive evidence is
recorded.

### DOS (DJGPP)

DOS uses a reduced specialized source list in `Makefile.djgpp`. It does not claim
POSIX storage, raw TTY, or complete Tier 1 app parity. Keep the source manifest
synchronized with:

```bash
python3 scripts/check_djgpp_sources.py
```

## Troubleshooting

### `curses.h` not found

```bash
# Ubuntu / Debian
sudo apt install libncurses-dev

# Fedora
sudo dnf install ncurses-devel

# macOS
brew install ncurses
```

Then remove the old build tree and reconfigure:

```bash
make clean
make
```

### CMake references another checkout path

CMake caches absolute paths. Remove `build/` or run `make clean` before building
from a moved/copied checkout.

### Unsupported terminal or odd rendering

```bash
./build/retrodesk --render-backend=ansi
echo "$TERM"
TERM=xterm ./build/retrodesk
```

### Mouse behavior is unavailable

Mouse and drag are capability-gated. RetroDesk remains keyboard-operable when
the backend cannot provide reliable pointer behavior.

Try the raw-TTY input path for backend diagnosis:

```bash
./build/retrodesk --render-backend=ansi --input-backend=tty
```

### Terminal state looks wrong after a crash

RetroDesk saves and restores the original terminal mode during normal shutdown
and initialization rollback. A process killed externally cannot always run its
cleanup path. Recover with:

```bash
stty sane
reset
```

## Docker (optional)

```dockerfile
FROM ubuntu:22.04

RUN apt update && apt install -y build-essential cmake libncurses-dev
WORKDIR /app
COPY . .
RUN make clean && make
CMD ["./build/retrodesk"]
```

Run Docker with an interactive TTY:

```bash
docker build -t retrodesk .
docker run --rm -it retrodesk
```

## Next Steps

1. Read [docs/KEYBOARD_SHORTCUTS.md](docs/KEYBOARD_SHORTCUTS.md).
2. Review [docs/BUILTIN_APPS.md](docs/BUILTIN_APPS.md).
3. Run the Linux manual smoke workflows.
4. Check [docs/ROADMAP.md](docs/ROADMAP.md) and
   [docs/RELEASE_0.1_CHECKLIST.md](docs/RELEASE_0.1_CHECKLIST.md).
