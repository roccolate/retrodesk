# Retrocore Spec Alignment

RetroDesk consumes `retrocore-spec` as shared contract input for behavior that
should stay consistent across RetroTUI, RetroDesk, WiiOS, and ArmoniOS.

RetroDesk uses `retrocore-spec` as fixtures, vocabulary, and documentation
reference only. It must not import shared runtime code or become dependent on a
`retrocore-spec` library.

For the detailed integration boundary, see:

```text
docs/retrocore-spec-integration.md
```

## Current Integration

RetroDesk consumes event fixtures from:

```text
$RETROCORE_SPEC_DIR/fixtures/events/
```

Currently covered fixtures:

```text
open-files-and-focus.json
window-drag-basic.json
```

## Fixture: `open-files-and-focus.json`

The fixture says:

1. launch logical app `files`,
2. expect its window to be focused.

RetroDesk maps logical app `files` to local app id `filemanager`.

Because RetroDesk launches File Manager by default, replaying the fixture should
focus the existing File Manager window instead of creating a duplicate.

The test asserts:

- File Manager window exists.
- `launch_app files` maps to `app_launch(desktop, "filemanager")`.
- The existing File Manager instance is focused.
- App/window counts do not increase.

## Fixture: `window-drag-basic.json`

The fixture says:

1. launch/focus logical app `files`,
2. send `pointer_down`,
3. send `pointer_move`,
4. send `pointer_up`,
5. expect the logical `files` window to remain focused.

RetroDesk's fixture runner preserves the fixture drag delta but translates the
starting coordinate to the local File Manager title bar. This keeps RetroDesk's
layout native while still exercising real WM pointer-drag event handling.

The test currently asserts:

- File Manager window exists.
- File Manager remains focused after the pointer sequence.
- App/window counts do not increase.

TODO: add final window coordinate assertions when RetroDesk exposes a clean
public desktop/window geometry assertion helper for tests.

## Covered By

```text
tests/retrocore_event_fixture_test.c
```

CMake enables this test only when a sibling/shared checkout exists with at least:

```text
../retrocore-spec/fixtures/events/open-files-and-focus.json
```

Override with:

```bash
cmake -S . -B build -DRETROCORE_SPEC_DIR=/path/to/retrocore-spec
cmake --build build
ctest --test-dir build --output-on-failure
```

## Supported Events

Currently supported by the tiny fixture runner:

- `launch_app`
- `pointer_down`
- `pointer_move`
- `pointer_up`

Planned/TODO event types:

- `focus_next`
- `close_window`
- `key`
- `menu_action`

Unsupported fixture event types must not be silently ignored.

## Public Introspection Helpers

RetroDesk exposes small desktop introspection helpers so contract tests can
assert logical state without reaching into private structs:

- `desktop_active_window()`
- `desktop_app_window_id()`
- `desktop_app_count()`
- `desktop_window_count()`

## Boundary

RetroDesk should consume retrocore contracts as fixtures and vocabulary. It
should not depend on a shared runtime. Curses, ANSI rendering, TTY decoding, WM
implementation, terminal PTY behavior, and C app lifecycle stay RetroDesk-native.
