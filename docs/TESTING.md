# Testing and Evidence Strategy

> Snapshot: 2026-07-22. Automated development gates are strong; interactive
> evidence tied to an exact release candidate remains incomplete.

## Evidence Layers

RetroDesk uses complementary evidence types:

1. unit tests for pure helpers, widgets, UTF-8, storage, and parsers;
2. integration tests for Desktop, Window Manager, app lifecycle, rendering,
   platform translation, and app workflows;
3. pinned shared fixture tests from `retrocore-spec`;
4. strict compilation and static analysis;
5. ASan/UBSan/leak validation where supported;
6. non-interactive startup smoke;
7. native/emulator Windows and DOS validation;
8. manual interaction and visual acceptance.

A release claim requires the appropriate combination for the exact candidate SHA.

## Test Oracle Policy

Tests use always-active oracles from `tests/test_harness.h`.

- Do not use `<assert.h>` as the sole correctness oracle.
- `NDEBUG` must not disable release behavior checks.
- Run `python3 scripts/check_test_oracles.py` before evidence collection.
- Prefer public behavior, event results, geometry, and DrawList output over private
  implementation coupling.

## Runtime and Lifecycle Coverage

Required coverage includes:

- Desktop create/run/shutdown and partial-init rollback;
- app registration, capability rejection, and create-failure destroy rollback;
- app service budget and active-service timeout policy;
- ordinary close and transactional global shutdown;
- clipboard validation and isolation among Desktop instances;
- focus, z-order, modal routing, drag, movement, resize, and degradation;
- minimized-window exclusion from render/input/focus/modal/drag;
- taskbar minimize/restore;
- maximize geometry, exact restore, resize refresh, and minimize while maximized;
- `F8` and title-bar double-click routing;
- `F9` mode, finish keys, focus cancellation, blocked notices, and HUD rendering;
- one renderer flush per dirty frame;
- CLI/backend validation and unsupported profile rejection.

## Taskbar, Launcher, and Theme Coverage

Current tests cover:

- legacy StatusBar compatibility;
- wide, medium, and compact taskbar layouts;
- exact x positions, widths, and mouse regions;
- Apps normal/open state;
- app idle/running/focused style;
- instance counts and compact labels;
- clock visibility and separation;
- launch/focus/minimize/restore taskbar bridge behavior;
- Launcher geometry, groups, keyboard wrap, Home/End, accelerators, and mouse hits;
- XP, Hacker, Amiga, and Win31 exact surface tokens;
- operation HUD copy, geometry, row, and two-command DrawList contract.

Remaining:

- larger-catalog overflow and priority behavior;
- severe congestion/narrow focus tests;
- visual/manual acceptance across every theme and render path;
- stronger end-to-end cycling after overflow exists.

## Input Coverage

Required:

- curses/PDCurses normalized keys;
- raw-TTY ordinary and modified navigation;
- fragmented CSI input;
- invalid-sequence consumption without text leakage;
- mouse decoder stability;
- editor control-key delivery;
- terminal-state restoration on shutdown/init rollback;
- `TERM=linux` keyboard-first policy.

Parser tests do not prove every terminal emulator. Manual PTY smoke remains
mandatory for profiles claimed in a release.

## UTF-8 and Notepad Coverage

- accented UTF-8 insertion/roundtrip;
- malformed UTF-8 rejection without data loss;
- codepoint-safe movement/Backspace/Delete;
- display-cell-aware navigation/rendering;
- multiline selection and replacement;
- clipboard exchange among Notepad instances;
- cut/paste as undoable edits;
- bounded history and dirty baseline;
- Find wraparound, limited case folding, accent significance, and byte ranges;
- Word Wrap navigation without logical-text mutation;
- Save/Save As, stale-version conflict, recovery, and failed-save behavior;
- Save/Discard/Cancel close;
- transactional shutdown involving dirty instances.

PR #24 menu/status behavior is not part of current `main` tests until rebuilt and
integrated.

## Storage and File Manager Coverage

Common adapter coverage:

- path lifecycle/parent/join;
- bounded empty/deterministic listing;
- portable kind/size/version metadata;
- UTF-8 text roundtrip;
- malformed text, controls, newline, and size rejection;
- exclusive create/mkdir and duplicate conflict;
- rename success/missing source/no-overwrite;
- expected-version stale conflict;
- expected/written version pointer aliasing;
- unsupported target/link policy;
- File Manager viewport, hidden, refresh, open, create, rename, cancellation.

Platform-specific:

- POSIX native behavior and replacement path;
- Win32 UTF-8/UTF-16, Unicode names, identity/version, native replacement, and
  reparse rejection;
- DJGPP paths, case ordering, explicit parent, content fingerprint, replacement
  transaction, and native DOS execution.

## Main Test Areas

Representative executables:

- `cli_parse_test`;
- `key_chord_test`;
- `utf8_test`;
- `clipboard_test`;
- `app_registry_test`;
- `platform_features_test`;
- `wm_event_replay_test`;
- `theme_catalog_test`;
- `draw_list_contract_test`;
- `statusbar_drawlist_test`;
- `ansi_renderer_diff_test`;
- `tty_decoder_test`;
- reusable widget tests;
- `text_buffer_test`;
- platform-specific `storage_test`;
- `desktop_runtime_test`;
- `retrocore_event_fixture_test` when pinned fixtures are available;
- native DOS `FSTEST.EXE` in the DOS workflow.

Debug and Release test manifests must match.

## Automated CI Matrix

### Linux

- checkout repository and pinned fixtures;
- oracle guard;
- Debug/Release/sanitizer configure;
- static analysis;
- complete Debug tests;
- complete Release tests;
- sanitizer/leak tests;
- manifest comparison;
- DJGPP source parity;
- non-interactive startup smoke.

### Windows

Debug and Release:

- pinned/configured PDCurses toolchain;
- strict `/W4 /WX` configure/build;
- complete tests;
- native Win32 storage contract.

Windows has native storage. Remaining uncertainty is interactive product maturity,
not a missing adapter.

### DOS

- pinned DJGPP/PDCurses/CWSDPMI inputs;
- product build;
- native `FSTEST.EXE` build;
- storage contract in DOSBox-X;
- RetroDesk diagnostic in DOSBox-X;
- distribution artifact and checksums;
- failure artifacts/logs when needed.

## Latest Development Checkpoint

The compact PR #36 head passed:

- CI `29900244919`;
- DOS `29900244867`;
- tested head `f2e9e83ea2b6f2c55ad6ec9c4e9c414efe103985`.

The current repository baseline for the integrated latest desktop slice is
`c36ceb92b269c4c485798eed2b8413ea6c096164`.

The green PR runs remain strong development evidence, but they are not attached
to the current exact `main` SHA. A release candidate must rerun/attach evidence to
its own final commit and remain unchanged afterward.

## Local Commands

```bash
make clean
make check-test-oracles
make strict
make test
make test-all
make test-sanitize
make smoke-ci
```

Interactive:

```bash
make smoke
make smoke-linux-vc
```

Strict direct CMake:

```bash
cmake -S . -B build-strict \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_TESTS=ON \
  -DENABLE_STRICT_WARNINGS=ON \
  -DENABLE_WERROR=ON
cmake --build build-strict --parallel
ctest --test-dir build-strict --output-on-failure
```

## Sanitizer Contract

`make test-sanitize` creates a dedicated Debug ASan/UBSan build. Leak detection is
enabled where supported. Findings are fatal.

Sanitizers do not replace Windows strict builds, native storage tests, DOS native
execution, manual terminal smoke, or allocation/identifier failure injection.

## Manual Acceptance Matrix

### Linux ncurses

- all themes;
- Launcher keyboard/mouse;
- taskbar full/compact/minimize/restore;
- focus, maximize, and `F9` HUD;
- File Manager open/create/rename;
- Notepad UTF-8/find/wrap/save/close;
- terminal restoration.

### Linux raw TTY + ANSI

- representative terminal emulators;
- modifiers/fragmented escape input;
- resize and redraw/flicker;
- complete keyboard app workflows.

### `TERM=linux`

- keyboard-only complete workflow;
- no dependency on unsupported pointer behavior;
- terminal restoration.

### Windows/PDCurses

- Unicode native file browse/edit/save;
- taskbar/Launcher/window controls;
- close/shutdown and console restoration.

### DOS/DJGPP

- reduced startup;
- file workflows within DOS path limits;
- desktop behavior at realistic console size;
- replacement/filename limitations recorded.

## Regression Rules

- Public behavior changes update status and user references.
- Input/focus/window/storage/lifecycle changes add lower-level and end-to-end
  tests.
- Parser changes cover complete, fragmented, and invalid input.
- App-service changes test budget, timeout, cleanup, and redraw.
- File-app changes test adapter and app flows.
- Platform adapters receive native tests.
- Headless green is never complete interactive proof.
- PR-only green is never integrated `main` behavior.

## Release Evidence

The release bundle must contain:

- exact candidate SHA and clean-tree confirmation;
- compiler, CMake, curses/PDCurses, analyzer, fixture, Python, Windows, and DOS
  toolchain versions;
- workflow/run identifiers;
- test manifests/results;
- static-analysis/sanitizer output;
- platform smoke/manual acceptance;
- artifact checksums/provenance;
- exact compatibility and known-issue record.

See [RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md).