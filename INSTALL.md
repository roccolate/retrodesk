# Installation & Quick Start

## Linux (Ubuntu/Debian/Fedora)

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y build-essential cmake libncurses-dev

# Fedora
sudo dnf install -y gcc cmake ncurses-devel

# Arch
sudo pacman -S base-devel cmake ncurses
```

### Build
```bash
git clone https://github.com/roccolate/retrodesk.git
cd retrodesk
make
```

### Run
```bash
./build/retrodesk
```

## Experimental profiles

Only the Linux build is currently release-gated. The following recipes are
developer bring-up paths, not claims of validated product support.

The current File Manager and Notepad preview uses the POSIX storage adapter in
`src/storage/retro_fs_posix.c`. Native Windows and DOS need a native storage
adapter or explicit feature gating before they can be treated as green release
profiles.

### macOS

### Prerequisites
```bash
# Using Homebrew
brew install cmake ncurses

# Or with MacPorts
sudo port install cmake ncurses
```

### Build
```bash
git clone https://github.com/roccolate/retrodesk.git
cd retrodesk
make
```

### Run
```bash
./build/retrodesk
```

### Windows (WSL2 or Native)

#### WSL2
```bash
# Inside WSL2 Ubuntu/Debian
sudo apt install -y build-essential cmake libncurses-dev
git clone https://github.com/roccolate/retrodesk.git
cd retrodesk
make
./build/retrodesk
```

#### Native Windows with PDCurses

Native Windows is a planned Tier 1 target, but it is not currently a release
claim. Use this recipe only for bring-up work; the storage layer must be
ported or gated before this profile can be considered complete.

```bash
# Clone PDCurses or download pre-built
git clone https://github.com/wmcbrine/PDCurses.git

# Build PDCurses (or use pre-built wincon version)
cd PDCurses/wincon
nmake -f Makefile.vc
cd ../..

# Clone RetroDesk and build
git clone https://github.com/roccolate/retrodesk.git
cd retrodesk
cmake -S . -B build -DPDCURSES_ROOT=../PDCurses
cmake --build build --config Release
build\retrodesk.exe
```

## First Run

```bash
# Start with default theme (XP)
./build/retrodesk

# Try different themes
./build/retrodesk --theme=hacker
./build/retrodesk --theme=amiga
./build/retrodesk --theme=win31

# Hotkeys:
# - F1: help, F2: launcher, F6: next window
# - F7: move/resize mode
# - Ctrl+W: close active window
# - Ctrl+Q: quit
# - Esc: close app
```

## Building from Source

### Full Build with Tests
```bash
make clean
make
make test
```

Tests are non-interactive and must pass without changing `TERM`.
`make test` requires curses development headers/libraries. If CMake reports
`Could NOT find Curses`, install `libncurses-dev`, `ncurses-devel`, or the
equivalent package for your platform and reconfigure from a clean build
directory.

### Full Local Gate
```bash
make clean
make strict
make test
make test-all
make smoke-ci
```

`make smoke` and `make smoke-linux-vc` require an interactive PTY. They are
manual gates, not reliable non-interactive sandbox checks.

### Development Build (no optimizations)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/retrodesk
```

### Strict Build (warnings = errors)
```bash
cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON -DENABLE_WERROR=ON
cmake --build build
```

## Troubleshooting

### "command not found: make"
You need build tools:
- **Ubuntu/Debian:** `sudo apt install build-essential`
- **Fedora:** `sudo dnf groupinstall "Development Tools"`
- **macOS:** Install Xcode Command Line Tools: `xcode-select --install`

### "CMake not found"
- **Ubuntu/Debian:** `sudo apt install cmake`
- **macOS:** `brew install cmake`
- **Fedora:** `sudo dnf install cmake`

### "libncurses not found"
- **Ubuntu/Debian:** `sudo apt install libncurses-dev`
- **macOS:** `brew install ncurses`
- **Fedora:** `sudo dnf install ncurses-devel`

If an existing `build/` directory was created from another absolute checkout
path, remove it with `make clean` before rebuilding. CMake caches absolute
source and binary directories.

### Build fails with errors
1. Clean rebuild: `make clean && make`
2. Check compiler: `gcc --version` (must be GCC 7+ or Clang 5+)
3. Check CMake: `cmake --version` (must be 3.16+)
4. Try verbose build: `make VERBOSE=1`
5. Check dependencies: `sudo apt install --reinstall libncurses-dev` (or equivalent for your OS)

### "Unsupported terminal"
```bash
# Try ANSI rendering backend
./build/retrodesk --render-backend=ansi

# Or set terminal type
TERM=xterm ./build/retrodesk
TERM=xterm-256color ./build/retrodesk
```

### Mouse not working
- Your terminal needs mouse support (most modern ones have it)
- Try raw input mode: `./build/retrodesk --input-backend=tty`
- Test: `echo -e '\e[?1000h'` should enable mouse (Esc+[+?+1000+h)

### Terminal gets corrupted
- Press Ctrl+L to refresh (redraw the screen)
- If stuck: `reset` or close the terminal and reopen
- Raw mode disabled: `stty sane` (if stuck in raw mode)

## Docker (Optional)

```dockerfile
FROM ubuntu:22.04

RUN apt update && apt install -y build-essential cmake libncurses-dev

WORKDIR /app
COPY . .

RUN make clean && make

CMD ["./build/retrodesk"]
```

Build and run:
```bash
docker build -t retrodesk .
docker run -it retrodesk
```

## Performance Tips

- **Slow rendering?** Try the ANSI backend: `--render-backend=ansi`
- **Input lag?** Increase input timeout: Currently 120ms, cannot change at runtime yet (edit main.c if needed)
- **High CPU?** Normal — event-driven, minimal idle CPU. If excessive, report a bug.

## Next Steps

1. **Explore the runtime:** Launch File Manager, open a text file, and try
   Notepad save/Save As on Linux
2. **Read the docs:** Check `docs/ROADMAP.md` for the v1.0 roadmap
3. **Contribute:** Find an open gate in `docs/RELEASE_0.1_CHECKLIST.md`
4. **Customize:** Modify themes in `src/ui/theme.c`

---

Need help? Open an issue on GitHub: https://github.com/roccolate/retrodesk/issues
