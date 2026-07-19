# RetroDesk

Reimplementación en C de [RetroTUI](https://github.com/roccolate/RetroTUI) — un entorno
de escritorio retro (estilo Windows 3.1) que corre completamente en terminales/consolas
de texto usando curses.

## Objetivo

Portar funcionalidades seleccionadas de RetroTUI (Python/curses) a C11 con
capas explícitas y contratos testeables. Linux es el perfil activo; Windows es
Tier 1 planificado, pero el storage actual es POSIX-only y bloquea cualquier
reclamo nativo hasta que exista adapter o feature gating.

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
│   ├── text_input.c/h          # Input de una línea
│   ├── text_buffer.c/h         # Buffer multi-línea
│   ├── scroll_list.c/h         # Lista seleccionable
│   └── statusbar.c/h           # Status bar widget
├── storage/
│   ├── retro_fs.h              # Contrato de storage para apps de archivos
│   └── retro_fs_posix.c        # Adapter POSIX usado por la preview Linux
└── apps/
    ├── apps.c/h                # Built-in app registration
    ├── apps_internal.h
    ├── filemanager_app.c       # Preview Linux: directorios + open in Notepad
    ├── notepad_app.c           # Preview Linux: editor + save/Save As básico
    └── terminal_app.c          # Diagnostics; no shell/PTY
```

Documentación de diseño en `docs/`. Reglas duras en `docs/FOUNDATION_PRINCIPLES.md`.

## Build

- **Sistema principal:** CMake (C11)
- **Dependencia:** PDCurses (Windows/DOS) o ncurses (Linux)
- **Windows:** vcpkg/PDCurses para bring-up; no release claim hasta resolver storage
- **Linux:** `apt install libncurses-dev`, luego CMake estándar
- **DOS:** `Makefile.djgpp` con DJGPP GCC

```bash
# Windows (vcpkg)
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

# Linux
cmake -S . -B build && cmake --build build

# Tests
make test
make test-all
```

## Convenciones de código

- **Estilo:** LLVM (`.clang-format`), indent 4 espacios, 100 columnas.
- **Warnings:** `-Wall -Wextra -Wpedantic -Werror` (GCC/Clang), `/W4 /WX` (MSVC).
- **Seguridad:** `snprintf()` siempre. Nunca `strcpy`/`strncpy`/`strcat`.
- **Recursos:** `create`/`destroy` explícito por módulo; `desktop_shutdown` en orden
  inverso de creación.
- **Redraw:** `wm`/`app`/`ui` producen `DrawList`; `render` ejecuta backend y
  mantiene un único flush por tick.
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
- **Hotkeys globales:** usar chord/teclas no imprimibles (`F1`, `F2`, `F6`,
  `F7`, `Ctrl+W`, `Ctrl+Q`). Las letras imprimibles pertenecen a la app
  enfocada salvo modo ventana/launcher.
- **Storage:** File Manager/Notepad usan `storage/retro_fs.h`; no llamar POSIX
  directamente desde apps.

## Estado actual de apps

- File Manager: lista directorios en Linux/POSIX, navega con teclado, abre
  archivos regulares en Notepad. Falta scroll-list completo y operaciones
  destructivas.
- Notepad: edición multi-línea, open-with-path, Ctrl+S, F3 Save As, detección
  de conflicto y bloqueo de cierre si está dirty. Falta UX completa de discard
  y diálogo robusto.
- Diagnostics: muestra información de runtime; no es terminal.

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

GitHub Actions define jobs para Linux (ubuntu + clang-tidy) y Windows
(vcpkg/PDCurses). Tratar Windows como objetivo de CI, no como soporte validado,
hasta que el candidate commit pase con storage adapter o gating explícito.
