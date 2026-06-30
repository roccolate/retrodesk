# RetroDesk

Reimplementación en C de [RetroTUI](https://github.com/roccolate/RetroTUI) — un entorno
de escritorio retro (estilo Windows 3.1) que corre completamente en terminales/consolas
de texto usando curses.

## Objetivo

Portar las funcionalidades de RetroTUI (Python/curses) a C puro con PDCurses,
manteniendo compatibilidad con Windows, Linux, macOS y DOS (DJGPP).

## Arquitectura real

```
src/
├── main.c                      # Entry point, platform detection, lifecycle
├── core/
│   ├── cli.c/h                 # CLI option parsing (--bench-startup, --theme, ...)
│   ├── desktop.c/h             # Desktop lifecycle, hotkeys, launcher, frame loop
│   └── event.h                 # RetroEvent / RetroKeyEvent / RetroMouseEvent / RetroResizeEvent
├── wm/
│   └── window_manager.c/h      # Window collection, z-order, focus, drag, modal
├── app/
│   └── app_runtime.c/h         # AppRegistry, RetroAppDescriptor, RetroAppInstance
├── render/
│   ├── render.c/h              # DrawList, RenderContext, curses/ANSI dispatch
│   └── ansi_renderer.c/h       # ANSI diff renderer (frame buffer + escape batching)
├── platform/
│   ├── platform.c/h            # Public PlatformBackend façade
│   ├── platform_backend_internal.h
│   ├── platform_curses.c       # ncurses/PDCurses input backend
│   ├── platform_tty_raw.c      # Raw TTY input backend (poll + decoder)
│   └── tty_decoder.c/h         # ANSI escape + SGR mouse sequence decoder
├── ui/
│   ├── theme.c/h               # Visual tokens (xp, hacker, amiga, win31)
│   └── statusbar.c/h           # Status bar widget
└── apps/
    ├── apps.c/h                # Built-in app registration
    ├── apps_internal.h
    ├── filemanager_app.c       # Stub
    ├── notepad_app.c           # Stub
    └── terminal_app.c          # Stub
```

Documentación de diseño en `docs/`. Reglas duras en `docs/FOUNDATION_PRINCIPLES.md`.

## Build

- **Sistema principal:** CMake (C11)
- **Dependencia:** PDCurses (Windows/DOS) o ncurses (Linux)
- **Windows:** vcpkg → `pdcurses:x64-windows`, luego CMake con toolchain vcpkg
- **Linux:** `apt install libncurses5-dev`, luego CMake estándar
- **DOS:** `Makefile.djgpp` con DJGPP GCC

```bash
# Windows (vcpkg)
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

# Linux
cmake -S . -B build && cmake --build build

# Tests
cd build && ctest --output-on-failure
```

## Convenciones de código

- **Estilo:** LLVM (`.clang-format`), indent 4 espacios, 100 columnas.
- **Warnings:** `-Wall -Wextra -Wpedantic -Werror` (GCC/Clang), `/W4 /WX` (MSVC).
- **Seguridad:** `snprintf()` siempre. Nunca `strcpy`/`strncpy`/`strcat`.
- **Recursos:** `create`/`destroy` explícito por módulo; `desktop_shutdown` en orden
  inverso de creación.
- **Redraw:** `wnoutrefresh()` por ventana + `doupdate()` una vez por iteración.
- **Prefijos de función por módulo:** `wm_*`, `renderer_*`, `app_*`, `draw_list_*`,
  `platform_*`, `retro_cli_*`, `retro_theme_*`, `retro_window_*`,
  `statusbar_*`, `tty_decoder_*`, `desktop_*`.
- **API:** structs opacos (`struct Foo *`) en headers públicos; definición en `.c`.
- **Headers públicos** no deben exponer `WINDOW *`, `MEVENT`, `getch`, `doupdate`
  ni otros tipos backend-native (ver `FOUNDATION_PRINCIPLES.md`).

## Patrones clave

- **Draw list contract:** `wm`/`app`/`ui` producen `DrawList*`. Solo `render`
  ejecuta llamadas backend (`werase`, `mvwaddch`, `fputs`, etc.).
- **Modal:** controlado por `WINDOW_FLAG_MODAL`. El WM bloquea eventos a ventanas
  no-modales cuando existe un modal abierto.
- **Event flow normalizado:** `platform_poll_event → RetroEvent → desktop → wm → app`.
  Sin loops anidados, sin bypasses directos al backend.
- **Dirty flag:** `desktop->needs_redraw` evita render redundante.
- **Capabilities:** apps declaran `required_capabilities`; el registry rechaza si el
  platform no las soporta (ver `app_launch`).
- **Hotkeys globales** (`q`, `m`, `1`/`2`/`3`, `w`, `Tab`, `HJKL`): `desktop_handle_key_command`
  retorna `true` si consumió el evento; en ese caso se suprime el dispatch al WM.

## Estado de RetroTUI original — features pendientes

El proyecto Python original aún tiene features que no están en RetroDesk:

- **Window management completo:** resize, maximize, minimize.
- **Shell embebido:** PTY dentro de ventanas (ConPTY / forkpty).
- **Apps completas:** File manager (dual-pane), editor de texto, calculadora.
- **Utilidades:** Process monitor, log viewer, control panel.
- **Juegos:** Minesweeper, Solitaire, Snake, Tetris.
- **Plugins:** RetroNet (browser de texto), character map, image viewer, WiFi.
- **Sistema de plugins:** Carga automática desde directorio configurable.

## CI/CD

GitHub Actions: build Linux (ubuntu + clang-tidy) y Windows (vcpkg + MSVC).