RetroDesk
========

Minimal C-based retro window manager skeleton.

Structure:

RetroDesk/
├── src/
│   ├── main.c              # entry point + platform detection
│   ├── desktop.c           # core window manager (ported from Python core)
│   ├── apps/               # FileManager.c, Notepad.c, Minesweeper.c, etc.
│   ├── ui/                 # common drawing, themes, mouse helpers
│   └── platform/           # platform-specific mouse/color init
├── pdcurses/               # vendored or git submodule
├── Makefile                # unified build (detects platform)
├── CMakeLists.txt          # optional modern build
├── README.md
├── LICENSE (MIT)
└── .github/workflows/      # CI for Linux + cross-compile DOS/Win


Build (quick):

Linux (ncurses):

RetroDesk
=========

Tiny terminal "desktop" demo using PDCurses.

Recommended build (Windows with vcpkg):

1) Install vcpkg and PDCurses

```powershell
cd F:\Proyectos\RetroDesk
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg.exe install pdcurses:x64-windows
```

2) Configure & build with CMake using the vcpkg toolchain

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=F:/Proyectos/RetroDesk/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

Alternative: use a local PDCurses build and pass `-DPDCURSES_ROOT`:

```powershell
cmake -S . -B build -DPDCURSES_ROOT="C:/path/to/PDCurses"
cmake --build build --config Release
```

Run the app (ensure `pdcurses.dll` is next to the exe or on PATH):

```powershell
build\Release\retrodesk.exe
```

Packaging example (portable ZIP):

```powershell
Compress-Archive -Path build\Release\retrodesk.exe, build\Release\pdcurses.dll -DestinationPath retrodesk-portable.zip
```

Notes
- The project prefers the vcpkg toolchain; if you use `PDCURSES_ROOT` ensure the include/lib paths are valid.
- CMake uses target-level include/link settings for safer, per-target configuration.

If you want me to produce a packaged ZIP, push to a remote, or continue refactoring modules, tell me which option.
│   │   ├── mouse.h

│   │   └── themes.c

