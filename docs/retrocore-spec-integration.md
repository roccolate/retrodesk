# Retrocore Spec Integration

RetroDesk consumes `retrocore-spec` as external contracts, fixtures, vocabulary,
and documentation reference only.

It is **not** a shared runtime dependency.

## Boundary

RetroDesk must remain native for:

- C app lifecycle,
- curses/PDCurses and ANSI render backends,
- TTY escape and mouse decoding,
- window manager implementation,
- terminal PTY behavior,
- platform capability detection.

Do not vendor, copy, link, or import runtime code from `retrocore-spec`.

## Expected Checkout Layout

The default local layout is:

```text
~/retrodesk
~/retrocore-spec
```

CMake defaults `RETROCORE_SPEC_DIR` to:

```text
../retrocore-spec
```

Override it explicitly when needed:

```bash
cmake -S . -B build -DRETROCORE_SPEC_DIR=/path/to/retrocore-spec
cmake --build build
ctest --test-dir build --output-on-failure
```

## Optional Test Enablement

RetroDesk enables the optional fixture test only when this file exists:

```text
$RETROCORE_SPEC_DIR/fixtures/events/open-files-and-focus.json
```

The test executable is:

```text
tests/retrocore_event_fixture_test.c
```

No network access is required. Fixtures are read from the external checkout at
test runtime/compile-time path configuration; they are not copied into this
repository.

## Supported Fixtures

| Fixture | Status | Notes |
| --- | --- | --- |
| `fixtures/events/open-files-and-focus.json` | Supported | Maps logical app `files` to RetroDesk app `filemanager`. Verifies launching/focusing does not duplicate the default File Manager instance. |
| `fixtures/events/window-drag-basic.json` | Supported | Replays pointer down/move/up through RetroDesk's desktop/WM event path. Verifies File Manager still exists, remains focused, is not duplicated, and verifies final WM geometry at the window-manager level. |
| `fixtures/events/focus-next-basic.json` | Supported | Maps logical app `notes` to RetroDesk app `notepad`, launches/focuses Notes, then replays `focus_next` and verifies focus moves back to Files while app/window counts remain stable. |
| `fixtures/events/close-focused-window.json` | Supported | Launches/focuses Files, replays `close_window` for the focused window, and verifies the File Manager app/window is removed. |

If newer optional fixtures are not present in a local `retrocore-spec` checkout,
the test prints a skip message for that fixture and continues.

## Supported Event Types

Currently supported by the tiny fixture runner:

- `launch_app`
- `pointer_down`
- `pointer_move`
- `pointer_up`
- `focus_next`
- `close_window`

Currently planned / TODO:

- `key`
- `menu_action`

Unsupported event types must not be ignored silently. The test should print a
clear TODO/skipped message or fail with a clear unsupported-event assertion.

## App ID Mapping

| Retrocore logical app | RetroDesk local app |
| --- | --- |
| `files` | `filemanager` |
| `notes` | `notepad` |

`File Manager` and `Notepad` are launched by default in the current RetroDesk
foundation desktop. Replaying `launch_app` for logical apps `files` and `notes`
should focus the existing local instances instead of creating duplicate
window/app instances.

## Coordinate Mapping

Retrocore pointer fixtures use logical coordinates. RetroDesk does not require
all consuming projects to share identical initial window geometry.

For `window-drag-basic.json`, the test preserves the fixture drag delta but
translates the starting pointer coordinate to the local File Manager title bar.
This proves RetroDesk can consume the fixture vocabulary and exercise real WM
pointer-drag handling without pretending that all projects share pixel-identical
layouts.

The desktop-level replay asserts logical outcomes: File Manager remains focused,
exists, and is not duplicated. A WM-level replay of the same fixture delta uses
`retro_window_get_geometry()` to assert final coordinates.

TODO: if future retrocore fixtures require app-level geometry assertions after a
full `Desktop` replay, add a small public desktop/window geometry helper instead
of reaching into private `Desktop` internals.

## Non-Goals

- No JSON dependency unless fixture complexity requires one later.
- No submodule requirement.
- No runtime dependency on `retrocore-spec`.
- No shared app lifecycle or renderer from `retrocore-spec`.
- No replacement of RetroDesk's native WM/input/render implementations.
