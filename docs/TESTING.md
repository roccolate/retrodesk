# Testing Strategy

## Layered Strategy

- Unit tests cover pure runtime logic, widgets, UTF-8 helpers, clipboard,
  storage, and parsers.
- Integration tests cover backend event translation, rendering, app lifecycle,
  and desktop behavior.
- Fixture tests cover shared retrocore behavior when `RETROCORE_SPEC_DIR` is
  available or required.
- PTY smoke tests cover terminal behavior that headless tests cannot prove.

## Test Oracle Policy

Tests use always-active oracles from `tests/test_harness.h`.

- Do not include `<assert.h>` or use `assert(...)`.
- Run `python3 scripts/check_test_oracles.py` before collecting release evidence.
- Prefer public behavior checks over private-state coupling.

## Required Runtime Coverage

- desktop create/run/shutdown and partial-init rollback;
- capability-based app rejection and create-failure cleanup;
- focus, z-order, drag, move, resize, modal routing, and close lifecycle;
- interactive taskbar rendering, hit testing, launch, focus, and instance cycling;
- one renderer flush per frame;
- CLI/backend option validation.

## Required Input Coverage

- curses and raw-TTY key normalization;
- ordinary and modified navigation sequences;
- fragmented and invalid escape-sequence handling;
- mouse decoding remaining unaffected by keyboard parser changes;
- editor control keys reaching applications;
- restoration of the original terminal mode.

## Required UTF-8 and Notepad Coverage

- accented-character insertion and roundtrip;
- malformed UTF-8 rejection without data loss;
- codepoint-safe cursor movement, Backspace, and Delete;
- display-cell-aware vertical navigation and rendering;
- multiline selection and replacement;
- shared clipboard behavior between Notepad instances;
- undoable cut/paste and bounded undo/redo history;
- dirty state relative to the last successful save;
- Find wraparound, case folding, accent significance, and byte-accurate ranges;
- Word Wrap visual navigation without changing logical text;
- Save/Discard/Cancel close handling.

## Required Storage and File Manager Coverage

- path, parent, and join behavior;
- bounded and empty directory listing;
- validated UTF-8 read/write roundtrip;
- invalid text and control-character rejection;
- exclusive file/directory creation and destination conflict;
- rename success and no-overwrite behavior;
- atomic save and stale-version conflict;
- File Manager viewport, hidden toggle, refresh, open, create, rename, and prompt
  cancellation.

## Main Product-Slice Tests

- `utf8_test`
- `clipboard_test`
- `text_buffer_test`
- `text_input_test`
- `storage_test`
- `tty_decoder_test`
- `statusbar_drawlist_test`
- `desktop_runtime_test`

Window Manager, renderer, CLI, app registry, lifecycle, and retrocore fixture tests
remain part of the complete matrix.

## CI Matrix

Current CI validates:

- Linux static analysis;
- Linux Debug build and tests;
- Linux Release build and tests;
- Linux AddressSanitizer, UndefinedBehaviorSanitizer, and leak checks;
- non-interactive startup smoke validation;
- Debug/Release test-manifest comparison;
- DJGPP/DOS source-manifest synchronization;
- Windows Debug build and tests with PDCurses;
- Windows Release build and tests with PDCurses.

A green portable Windows build is not a native Windows storage-support claim.

## Smoke Matrix

- `make smoke` — interactive PTY smoke test.
- `make smoke-linux-vc` — keyboard-first `TERM=linux` gate.
- `make smoke-ci` — non-interactive smoke path.
- Windows, macOS, and DOS interaction claims require profile-specific evidence.

## Local Commands

```bash
make check-test-oracles
make test
make test-all
make test-sanitize
make smoke-ci
```

Strict CMake validation:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_TESTS=ON \
  -DENABLE_STRICT_WARNINGS=ON \
  -DENABLE_WERROR=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Sanitizer Contract

`make test-sanitize` configures a dedicated Debug tree with AddressSanitizer and
UndefinedBehaviorSanitizer. On supported Linux toolchains, ASan also performs
leak detection. Sanitizer findings are fatal and run against the same CTest
manifest as the ordinary Debug/Release matrix. This gate does not replace the
interactive PTY smoke tests.

## Regression Rules

- Interface changes update related documentation.
- Input, focus, resize, storage, UTF-8, or close-flow fixes add regressions.
- Parser changes test complete, fragmented, and invalid input.
- App-runtime changes test launch rejection, rollback, close, and cleanup.
- File-app changes test both lower-level behavior and app event flows.
- Interactive terminal behavior is not release-verified solely by headless tests.

## Release Gate Reference

Release sign-off follows
[RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md) and must attach evidence to
the exact candidate commit.

## Capacity and Identifier Boundaries

`wm_event_replay_test` exercises checked capacity arithmetic, an injected Window
Manager growth failure, successful retry, and deterministic `WindowId`
exhaustion at `INT_MAX`. `desktop_runtime_test` injects failure at the 8-to-16
running-app table transition and verifies that app/window counts and existing
entries remain unchanged before a successful retry.

