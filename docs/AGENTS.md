# RetroDesk

Reimplementación en C de [RetroTUI](https://github.com/roccolate/RetroTUI): un entorno
de escritorio retro estilo Windows 3.1 / 95 que corre completamente en
terminales/consolas de texto usando curses, PDCurses o salida ANSI según el
perfil de plataforma.

## Objetivo

Portar la esencia de RetroTUI (Python/curses) a C11 con capas estrictas,
contratos documentados y compatibilidad planificada con Windows, Linux, macOS y
DOS (DJGPP).

RetroTUI es referencia de comportamiento y catálogo de riesgos; no es plantilla
de arquitectura.

## Arquitectura real

```text
src/
├── main.c                       # Entry point
├── core/
│   ├── cli.c/.h                 # CLI option parsing (--bench-startup, --theme, ...)
│   ├── desktop.c/.h             # Desktop lifecycle, launcher, hotkeys, frame loop
│   ├── event.h                  # RetroEvent / RetroKeyEvent / RetroMouseEvent / RetroResizeEvent
│   └── key_chord.c/.h           # Vocabulario de teclas backend-neutral
├── wm/
│   └── window_manager.c/.h      # Window collection, z-order, focus, drag, modal policy
├── app/
│   └── app_runtime.c/.h         # AppRegistry, RetroAppDescriptor, RetroAppInstance
├── apps/
│   ├── apps.c/.h                # Built-in app registration
│   ├── apps_internal.h
│   ├── filemanager_app.c        # Placeholder app
│   ├── notepad_app.c            # Placeholder app
│   └── terminal_app.c           # Diagnostics placeholder app
├── render/
│   ├── render.c/.h              # DrawList, RenderContext, curses/ANSI dispatch
│   └── ansi_renderer.c/.h       # ANSI diff renderer (frame buffer + escape batching)
├── platform/
│   ├── platform.c/.h            # Public PlatformBackend façade
│   ├── platform_backend_internal.h
│   ├── platform_curses.c        # ncurses/PDCurses input backend
│   ├── platform_tty_raw.c       # Raw TTY input backend (poll + decoder)
│   └── tty_decoder.c/.h         # ANSI escape + SGR mouse sequence decoder
└── ui/
    ├── theme.c/.h               # Visual tokens (xp, hacker, amiga, win31)
    ├── statusbar.c/.h           # Status bar widget
    ├── text_input.c/.h          # Single-line text input
    ├── text_buffer.c/.h         # Multi-line text buffer
    ├── scroll_list.c/.h         # Scrollable list widget
    ├── button.c/.h              # Button widget
    ├── dialog.c/.h              # Dialog system
    ├── menu_bar.c/.h            # Menu bar widget
    ├── progress_bar.c/.h        # Progress bar widget
    └── tab.c/.h                 # Tab widget
```

Documentación de diseño en `docs/`. Reglas duras en
`docs/FOUNDATION_PRINCIPLES.md`.

## Build

- **Sistema principal:** CMake (C11), `cmake_minimum_required(VERSION 3.16)`.
- **Linux/macOS:** `ncurses` (`sudo apt install libncurses-dev` o
  `brew install ncurses`).
- **Windows:** PDCurses por `PDCURSES_ROOT` o vcpkg con `pdcurses:x64-windows`.
- **DOS:** `Makefile.djgpp` con DJGPP GCC y source list explícita.
- **Fixtures compartidos:** `RETROCORE_SPEC_DIR` apunta por defecto a
  `../retrocore-spec`.

```bash
# Build normal
cmake -S . -B build
cmake --build build

# Wrapper del repo
make
make dev
make strict
make test
make check-build-sources
make smoke-ci

# Smoke interactivo con PTY
make smoke
make smoke-linux-vc
```

## Convenciones de código

- **Estándar:** C11.
- **Warnings:** `-Wall -Wextra -Wpedantic -Werror` (GCC/Clang), `/W4 /WX`
  (MSVC cuando aplique).
- **Seguridad:** `snprintf()` siempre. Nunca `strcpy`/`strncpy`/`strcat`.
- **Recursos:** `create`/`destroy` explícito por módulo; shutdown en orden
  inverso de creación cuando aplique.
- **Render:** `wm`/`app`/`ui` producen `DrawList`; solo `render` ejecuta llamadas
  backend y hace flush.
- **Prefijos de función por módulo:** `wm_*`, `renderer_*`, `app_*`,
  `draw_list_*`, `platform_*`, `retro_cli_*`, `retro_theme_*`,
  `retro_window_*`, `statusbar_*`, `tty_decoder_*`, `desktop_*`.
- **Headers públicos:** no deben exponer `WINDOW *`, `MEVENT`, `getch`,
  `doupdate` ni otros tipos backend-native.
- **Tests:** cada cambio de contrato, widget, input, render, focus, app runtime o
  build debe actualizar/agregar pruebas.
- **Docs:** si cambia estructura pública, comandos, hotkeys, plataforma o
  comportamiento visible, actualizar README y docs relacionadas.

## Patrones clave

- **Draw list contract:** `wm`/`app`/`ui` producen `DrawList*`; solo `render`
  ejecuta llamadas backend (`werase`, `mvwaddch`, `fputs`, etc.).
- **Modal:** controlado por `WINDOW_FLAG_MODAL`. El WM bloquea eventos a ventanas
  no-modales cuando existe un modal abierto.
- **Event flow normalizado:** `platform_poll_event → RetroEvent → desktop → wm → app`.
  Sin loops anidados, sin bypasses directos al backend.
- **Dirty flag:** `desktop->needs_redraw` marca cuándo el runtime necesita pintar
  un nuevo frame.
- **Capabilities:** apps declaran `required_capabilities`; `app_launch` rechaza si
  la plataforma no soporta lo requerido.
- **Hotkeys globales:** `q`, `m`, `1`/`2`/`3`, `w`, `Tab`, `HJKL`. Si
  `desktop_handle_key_command` consume el evento, se suprime el dispatch al WM.

## ASCII vs extended ASCII (0x80..0xFF)

`src/core/key_chord.h` reserva `0x80..0xFF` para bytes extendidos dependientes de
locale/codepage. No se deben clasificar como chords portables ni como ASCII
printable/control clásico.

Regla para agentes:

- `retro_key_is_chord()` debe cubrir solo chords backend-neutral no ASCII
  (`0x1000..`) como flechas, navegación y function keys.
- `retro_key_is_printable()` debe cubrir ASCII printable clásico `0x20..0x7E`.
- `retro_key_is_control()` debe cubrir `0x00..0x1F` y `0x7F`.
- No meter lógica de locale/codepage dentro de widgets sin una política explícita
  de plataforma/input.

## Estado de RetroTUI original — features pendientes

El proyecto Python original aún tiene features que no están completas en
RetroDesk:

- File manager real: directorios, navegación útil, abrir archivos.
- Editor de texto real: cursor, edición, open/save, buffers más grandes.
- Terminal real: PTY dentro de ventanas (ConPTY / forkpty) o consola diagnóstica
  formalmente limitada.
- Utilidades: process monitor, log viewer, control panel/settings.
- Juegos: Minesweeper, Solitaire, Snake, Tetris.
- Plugins: RetroNet, character map, image viewer, WiFi, sistema de carga.

## CI/CD

GitHub Actions usa matriz Linux/Windows:

- Linux: instala ncurses + clang-tidy, configura CMake, corre análisis estático,
  build y `ctest`.
- Windows: clona vcpkg, instala `pdcurses:x64-windows`, configura con toolchain
  vcpkg, build y `ctest -C Release`.
- El workflow también clona `roccolate/retrocore-spec` para habilitar fixtures
  compartidos cuando estén disponibles.
