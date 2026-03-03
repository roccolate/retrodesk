# RetroDesk

Reimplementación en C de [RetroTUI](https://github.com/roccolate/RetroTUI) — un entorno de escritorio retro (estilo Windows 3.1) que corre completamente en terminales/consolas de texto usando curses.

## Objetivo

Portar las funcionalidades de RetroTUI (Python/curses) a C puro con PDCurses, manteniendo compatibilidad con Windows, Linux, macOS y DOS (DJGPP).

## Arquitectura

```
src/
├── main.c              # Entry point, platform detection, curses lifecycle
├── desktop.c/h         # Core window manager: event loop, input, redraw
├── apps/               # Aplicaciones stub (FileManager, Notepad, Terminal)
├── ui/                 # Componentes UI reutilizables
│   ├── window.c/h      # Widget ventana (RetroWindow) con draw callbacks
│   ├── dialog.c/h      # Popups modales
│   ├── menu.c/h        # Menú vertical con selección
│   ├── mouse.c/h       # Drag & drop de ventanas (DragState)
│   ├── statusbar.c/h   # Barra de estado inferior (reloj + info)
│   ├── themes.c/h      # Pares de color (init_pair)
│   └── draw.h          # Utilidades de dibujo (stubs)
└── platform/
    └── platform.c      # Abstracción de plataforma unificada
```

## Build

- **Sistema principal:** CMake (C11)
- **Dependencia:** PDCurses (Windows/DOS) o ncurses (Linux)
- **Windows:** vcpkg → `pdcurses:x64-windows`, luego CMake con toolchain vcpkg
- **Linux:** `apt install libncurses5-dev`, luego CMake estándar
- **DOS:** Makefile.djgpp con DJGPP GCC

```bash
# Windows (vcpkg)
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

# Linux
cmake -S . -B build && cmake --build build
```

## Convenciones de código

- **Estilo:** LLVM (via .clang-format), indent 4 espacios, 100 columnas
- **Warnings:** `-Wall -Wextra -Wpedantic -Werror` (GCC/Clang), `/W4 /WX` (MSVC)
- **Seguridad:** `snprintf()` siempre (nunca strcpy/strncpy)
- **Recursos:** Allocate/destroy explícito, `atexit()` para cleanup
- **Redraw:** `wnoutrefresh()` batched + `doupdate()` una vez por iteración
- **Prefijos:** `window_*`, `dialog_*`, `menu_*`, `statusbar_*`, `mouse_*`, `theme_*`

## Patrones clave

- **Draw callbacks:** `RetroWindow` tiene `draw_cb(rw, win, ctx)` para contenido custom
- **Dirty flag:** `g_need_redraw` y `rw->dirty` para evitar redraws innecesarios
- **Popups:** `ui_popup_create()` con flags `UI_POPUP_CENTER | UI_POPUP_MODAL`
- **Event loop:** `getch()` con timeout configurable (default 200ms)
- **Plataforma:** Preprocessor guards (`_WIN32`, `__DJGPP__`) en platform.c y main.c

## RetroTUI original — funcionalidades pendientes de portar

El proyecto original en Python tiene muchas más features que aún no están en RetroDesk:

- **Window management completo:** resize, maximize, minimize, z-order
- **Shell embebido:** PTY dentro de ventanas (pywinpty en Windows)
- **Apps completas:** File manager (dual-pane), editor de texto, calculadora, clipboard viewer
- **Utilidades de sistema:** Process monitor, resource viewer, log viewer, control panel
- **Juegos:** Minesweeper, Solitaire, Snake, Tetris
- **Plugins:** Browser de texto (RetroNet), character map, reloj/calendario, image viewer, WiFi manager
- **Temas:** Windows 3.1, DOS, Windows 95, Hacker, Amiga
- **Sistema de plugins:** Carga automática desde directorio configurable

## CI/CD

GitHub Actions: build Linux (ubuntu + clang-tidy) y Windows (vcpkg + MSVC).
