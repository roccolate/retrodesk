# Core Module Hardening Plan

Tracking document for the systematic cleanup of `src/core/`. This document is
current against the source layout audited on 2026-07-09.

Every phase must be independently executable and verifiable. Each commit must
keep Debug and Release `ctest` at 100%.

## Current Source Status

The following items are still pending in the current source:

- `app_launch` returns `NULL` for multiple meanings: launch failure and
  "already running, focused existing window".
- `app_launch` does not call `desc->destroy` when `desc->create` fails after the
  window has already been created.
- `retro_cli_parse` still returns `bool`, so `--help` and parse failure are not
  represented as distinct result states.
- `RetroKeyEvent.ascii` is still `char`; it should become `unsigned char` before
  extended byte handling is considered safe across platforms.
- `key_chord.h` currently defines F1..F12 only. The function-key comment says
  the values are macro-generated even though they are manually listed.
- Several `desktop.c` helpers still duplicate app lookup and capability /
  diagnostics state handling.
- Uppercase `Q`/`W` hotkeys are still tracked as a polish item unless source
  changes prove otherwise.
- Selective redraw and statusbar text caching are not implemented.

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

### 1.1 App create-failure cleanup

Current shape:

```c
if (desc->create && !desc->create(instance, &instance->ctx)) {
    wm_close_window(desktop->wm, wid);
    free(instance);
    return NULL;
}
```

Required behavior:

```c
if (desc->create && !desc->create(instance, &instance->ctx)) {
    if (desc->destroy) desc->destroy(instance);
    wm_close_window(desktop->wm, wid);
    free(instance);
    return NULL;
}
```

Test: extend `tests/desktop_runtime_test.c` with a descriptor whose `create`
callback fails. Hook `destroy` to increment a counter. Assert the counter is 1.

### 1.2 CLI parse result states

Replace `bool retro_cli_parse(...)` with an enum result:

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

Tests: extend `tests/cli_parse_test.c` for each branch.

### 1.3 Function-key vocabulary

Update `key_chord.h` so the comment matches the implementation and add F13..F24
if the supported backends expose them consistently.

Tests: extend `tests/key_chord_test.c` for F-key ranges and classification.

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

### 2.3 Sign-safe ASCII byte

Change:

```c
char ascii;
```

to:

```c
unsigned char ascii;
```

Add comments in `src/core/event.h` explaining that `0x80..0xFF` may be valid
byte values in some locales/codepages and must not be corrupted by signed
`char` behavior.

Tests: add a round-trip test for a byte such as `0xE9`.

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

- [ ] 1.1 — `app_launch` calls `destroy` on `create` failure.
- [ ] 1.2 — CLI parse result distinguishes OK, usage, and parse error.
- [ ] 1.3 — function-key vocabulary/comment is corrected and tested.
- [ ] 2.1 — `app_launch` return type is unambiguous.
- [ ] 2.2 — diagnostics no longer duplicate capability ownership.
- [ ] 2.3 — `RetroKeyEvent.ascii` is `unsigned char` and documented.
- [ ] 3.x — repeated app lookup and launcher internals are simplified.
- [ ] 4.x — uppercase hotkeys, selective redraw, and statusbar cache are done.
- [ ] Debug and Release tests pass.
- [ ] Strict warnings pass.
- [ ] Sanitizers report zero issues where supported.
