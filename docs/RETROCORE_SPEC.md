# Retrocore Spec Alignment

RetroDesk uses `~/retrocore-spec` as shared contract input for behavior that should stay consistent across RetroTUI, RetroDesk, WiiOS, and ArmoniOS.

## Current Integration

RetroDesk consumes the shared event fixture:

```text
retrocore-spec/fixtures/events/open-files-and-focus.json
```

The fixture says:

1. launch logical app `files`
2. expect its window to be focused

RetroDesk maps logical app `files` to local app id `filemanager`.

Because RetroDesk launches File Manager by default, replaying the fixture should focus the existing File Manager window instead of creating a duplicate.

Covered by:

```text
tests/retrocore_event_fixture_test.c
```

CMake enables this test only when a sibling/shared checkout exists:

```text
../retrocore-spec/fixtures/events/open-files-and-focus.json
```

Override with:

```bash
cmake -S . -B build -DRETROCORE_SPEC_DIR=/path/to/retrocore-spec
```

## Public Introspection Helpers

RetroDesk exposes small desktop introspection helpers so contract tests can assert logical state without reaching into private structs:

- `desktop_active_window()`
- `desktop_app_window_id()`

## Boundary

RetroDesk should consume retrocore contracts as fixtures and vocabulary. It should not depend on a shared runtime. Curses, ANSI rendering, TTY decoding, and C app lifecycle stay RetroDesk-native.
