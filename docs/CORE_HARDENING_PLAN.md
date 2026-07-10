# Core Module Hardening Plan

Tracking document for the systematic cleanup of `src/core/`. This document is
current against the source layout audited on 2026-07-09.

Every phase must be independently executable and verifiable. Each commit must
keep Debug and Release `ctest` at 100%.

## Current Source Status

The following items are still pending in the current source:

- `app_launch` returns `NULL` for multiple meanings: launch failure and
  "already running, focused existing window".
- Several `desktop.c` helpers still duplicate app lookup and capability /
  diagnostics state handling.
- Uppercase `Q`/`W` hotkeys are still tracked as a polish item unless source
  changes prove otherwise.
- Selective redraw and statusbar text caching are not implemented.

Completed hardening items:

- `app_launch` now calls the descriptor `destroy` callback when `create` fails
  after the app window has already been created. `tests/desktop_runtime_test.c`
  registers a test-only failing app descriptor and asserts create/destroy/count
  behavior.
- `retro_cli_parse` returns `RetroCliParseResult`, distinguishing valid parse,
  help/usage, and parse errors. `main()` exits successfully for `--help` / `-h`.
- Function-key vocabulary is now documented as the explicitly supported F1..F12
  range. F13..F24 are intentionally not claimed until backend mapping and tests
  prove consistent support.
- `RetroKeyEvent.ascii` is `unsigned char`, documented in `src/core/event.h`, and
  covered by `tests/key_chord_test.c` with an extended-byte preservation check.

Do not mark an item complete without the matching source change and a regression
test.

## Goals

1. Public API is unambiguous.
2. `RetroKeyEvent` is sign-safe across platforms.
3. Key vocabulary covers the function keys supported by target backends.
4. Every bug-shaped cleanup has a regression test that fails on the old code and
   passes on the new code.
5. Zero warnings under the project warning policy.
6. No leaks or undefined behavior under sanitizer builds where supported.

## Phase 1 — Bug-Shaped Cleanup

### 1.1 App create-failure cleanup — done

`app_launch` now performs descriptor cleanup when `create` fails after window
creation:

```c
if (desc->create && !desc->create(instance, &instance->ctx)) {
    if (desc->destroy) desc->destroy(instance);
    wm_close_window(desktop->wm, wid);
    free(instance);
    return NULL;
}
```

Test coverage: `tests/desktop_runtime_test.c` registers a test-only descriptor
whose `create` callback fails and whose `destroy` callback increments a counter.
The test asserts that:

- `create` is called once,
- `destroy` is called once,
- no app instance is recorded,
- no extra window remains,
- `desktop_app_window_id()` returns `WINDOW_ID_INVALID` for the failed app.

The test uses `RETRODESK_ENABLE_TEST_HOOKS` to expose
`desktop_register_app_for_test()` without adding the artificial descriptor to
normal runtime app registration.

### 1.2 CLI parse result states — done

`retro_cli_parse(...)` now returns:

```c
typedef enum RetroCliParseResult {
    RETRO_CLI_OK = 0,
    RETRO_CLI_SHOWED_USAGE,
    RETRO_CLI_PARSE_ERROR,
} RetroCliParseResult;
```

Behavior:

- `--help` / `-h` prints usage and returns `RETRO_CLI_SHOWED_USAGE`.
- Unknown arguments print usage/error details and return `RETRO_CLI_PARSE_ERROR`.
- Invalid backend combinations return `RETRO_CLI_PARSE_ERROR`.
- Valid configurations return `RETRO_CLI_OK`.
- `main()` returns `EXIT_SUCCESS` for help/usage and `EXIT_FAILURE` only for
  parse errors.

Test coverage: `tests/cli_parse_test.c` covers OK, help, short help, invalid
backend combinations, invalid theme, unknown flag, invalid argc, null argv, and
null output options.

### 1.3 Function-key vocabulary — done

`key_chord.h` now documents the current portable function-key vocabulary as
F1..F12 only. That matches the currently translated curses/PDCurses backend
surface.

F13..F24 are deliberately not added yet because the supported backends do not
currently expose a tested, consistent mapping for them. Raw TTY function-key
escape handling is also not claimed yet.

Test coverage: `tests/key_chord_test.c` verifies that F1..F12 are sequential,
classified as chords, non-printable, non-control, and live outside raw byte and
navigation ranges.

## Phase 2 — API Disambiguation

### 2.1 `app_launch` return value

Current return type conflates failure and already-running behavior:

```c
RetroAppInstance *app_launch(Desktop *desktop, const char *app_id);
```

Target shape:

```c
typedef enum RetroAppLaunchResult {
    RETRO_APP_LAUNCHED = 0,
    RETRO_APP_ALREADY_RUNNING,
    RETRO_APP_FAILED,
} RetroAppLaunchResult;

RetroAppLaunchResult app_launch(Desktop *desktop, const char *app_id,
                                RetroAppInstance **out_instance);
```

Tests: cover launched, already-running, missing app, and capability rejection.

### 2.2 Diagnostics vs capabilities

Keep `PlatformFeatures` as the source of capability booleans. `DesktopDiagnostics`
should contain only diagnostic fields that are not already represented by
`PlatformFeatures`.

Tests: bench mode and shell diagnostics must still expose the same user-visible
fields.

### 2.3 Sign-safe ASCII byte — done

Changed:

```c
char ascii;
```

to:

```c
unsigned char ascii;
```

`src/core/event.h` now documents that `0x80..0xFF` may be valid byte values in
some locales/codepages and must not be corrupted by signed `char` behavior.

Test coverage: `tests/key_chord_test.c` checks that byte `0xE9` is preserved as
an unsigned byte and is not misclassified as a RetroDesk chord, portable
printable character, or ASCII control value.

## Phase 3 — Internal Refactors

- Extract a single app lookup helper for repeated `app_id` scans.
- Ensure diagnostic refresh helpers do not mutate capability state.
- Replace launcher geometry magic numbers with named constants.
- Add a `LauncherAction` sentinel/count value instead of relying on repeated
  `sizeof(array) / sizeof(array[0])` patterns.
- Remove stale comments from public headers.

## Phase 4 — Polish

- Accept uppercase `Q`/`W` for quit/close hotkeys if lowercase equivalents are
  supported.
- Avoid unnecessary full-frame redraws for non-visual events.
- Cache statusbar text and skip no-op `statusbar_set_text` calls.

## Required Verification Matrix

```bash
# Debug, asserts enabled
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=ON
cmake --build build-debug
ctest --test-dir build-debug --output-on-failure

# Release
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure

# Strict warnings + Werror
cmake -S . -B build-strict \
      -DENABLE_STRICT_WARNINGS=ON -DENABLE_WERROR=ON \
      -DCMAKE_BUILD_TYPE=Debug
cmake --build build-strict

# Optional sanitizer pass on supported platforms
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

## Completion Checklist

- [x] 1.1 — `app_launch` calls `destroy` on `create` failure.
- [x] 1.2 — CLI parse result distinguishes OK, usage, and parse error.
- [x] 1.3 — function-key vocabulary/comment is corrected and tested.
- [ ] 2.1 — `app_launch` return type is unambiguous.
- [ ] 2.2 — diagnostics no longer duplicate capability ownership.
- [x] 2.3 — `RetroKeyEvent.ascii` is `unsigned char` and documented.
- [ ] 3.x — repeated app lookup and launcher internals are simplified.
- [ ] 4.x — uppercase hotkeys, selective redraw, and statusbar cache are done.
- [ ] Debug and Release tests pass.
- [ ] Strict warnings pass.
- [ ] Sanitizers report zero issues where supported.
