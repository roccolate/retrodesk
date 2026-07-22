# Testing and Evidence Strategy

> Snapshot: 2026-07-22. Automated development gates are strong; interactive
> evidence tied to an exact release candidate remains incomplete.

## Evidence Layers

RetroDesk uses several distinct forms of evidence. They are complementary, not
interchangeable.

1. **Unit tests** — pure helpers, widgets, parsers, UTF-8, storage behavior.
2. **Integration tests** — Desktop, Window Manager, app lifecycle, rendering,
   platform event translation, and app workflows.
3. **Shared fixture tests** — behavior from pinned `retrocore-spec` fixtures.
4. **Static analysis and strict compilation** — warnings/errors and analyzer
   findings across supported toolchains.
5. **Sanitizers** — AddressSanitizer, UndefinedBehaviorSanitizer, and leak checks
   where supported.
6. **Non-interactive smoke** — startup/diagnostic sanity without a PTY.
7. **Native/emulator smoke** — Windows build/test and native DJGPP execution in
   DOSBox-X.
8. **Manual interaction/visual acceptance** — terminal behavior that headless
   tests cannot prove.

A release claim requires the appropriate combination for the exact candidate SHA.

## Test Oracle Policy

Tests use always-active oracles from `tests/test_harness.h`.

- Do not use `<assert.h>` as the sole correctness oracle.
- `NDEBUG` must not disable the behavior checks used by release tests.
- Run `python3 scripts/check_test_oracles.py` before collecting evidence.
- Prefer public behavior and observable DrawList/event results over private
  implementation coupling.

## Runtime and Lifecycle Coverage

Required coverage includes:

- Desktop create/run/shutdown and partial-initialization rollback;
- app registration, capability rejection, and create-failure destroy rollback;
- app service budget and poll-timeout policy;
- ordinary and transactional global close flows;
- Desktop-owned clipboard isolation between Desktop instances;
- focus, z-order, modal routing, drag, movement, resize, and degradation;
- minimized-window exclusion from rendering/input/focus/modal/drag;
- taskbar minimize/restore behavior;
- maximize geometry, exact restore geometry, terminal resize, and minimize while
  maximized;
- `F8` and title-bar double-click routing;
- `F9` move/resize mode, finish keys, focus cancellation, blocked-window notices,
  and responsive HUD rendering;
- one renderer flush per dirty frame;
- CLI/backend option validation and unsupported profile rejection.

## Taskbar, Launcher, and Theme Coverage

The DrawList and bridge tests cover:

- legacy StatusBar compatibility;
- wide, medium, and compact taskbar layouts;
- exact rendered x positions and widths;
- exact mouse hit regions;
- Apps menu normal/open state;
- app idle/running/focused styles;
- instance counts and compact labels;
- clock visibility and separation;
- taskbar launch/focus/minimize/restore bridge behavior;
- Launcher geometry, section grouping, selection, keyboard wrap, Home/End,
  accelerators, and mouse hit testing;
- XP, Hacker, Amiga, and Win31 exact surface-token matching;
- operation HUD copy, geometry formatting, row placement, and two-command render
  contract.

Remaining required work:

- larger catalog/overflow behavior;
- focused/running priority under severe congestion;
- visual snapshots or manual acceptance across all themes and both render paths;
- stronger end-to-end focus/cycling coverage after overflow exists.

## Input Coverage

Required:

- curses/PDCurses normalized keys;
- raw-TTY ordinary and modified navigation;
- Shift/Alt/Ctrl combinations where encoded by the terminal;
- fragmented CSI sequences;
- invalid-sequence consumption without text leakage;
- mouse decoding unaffected by keyboard parser changes;
- editor control-key delivery;
- terminal mode restoration during shutdown and initialization rollback;
- Linux `TERM=linux` keyboard-first behavior.

Headless parser tests do not prove every terminal emulator's behavior. Manual PTY
smoke remains mandatory for claimed profiles.

## UTF-8 and Notepad Coverage

Required:

- valid accented-character insertion and roundtrip;
- malformed UTF-8 rejection without data loss;
- codepoint-safe cursor movement, Backspace, and Delete;
- display-cell-aware navigation and rendering;
- multiline selection and replacement;
- clipboard validation and exchange between instances;
- cut/paste as undoable edits;
- bounded undo/redo history and dirty baseline behavior;
- Find wraparound, limited case folding, accent significance, and byte ranges;
- soft Word Wrap navigation without logical-text mutation;
- Save/Save As, stale-version conflict, recovery, and failed-save behavior;
- Save/Discard/Cancel close;
- transactional global shutdown involving dirty Notepad instances.

The native responsive menu/status work in PR #24 is not part of the current
`main` test manifest until rebuilt and integrated.

## Storage and File Manager Coverage

Common adapter coverage:

- path init/destroy/parent/join;
- bounded and empty listing;
- deterministic ordering and portable kinds;
- validated UTF-8 text roundtrip;
- malformed text, controls, mixed newline, and size rejection;
- exclusive create/mkdir and duplicate conflict;
- rename success, missing source, and no-overwrite behavior;
- optimistic concurrency and stale-token conflict;
- aliasing expected/written version pointers;
- unsupported target/link policy;
- File Manager viewport, hidden files, refresh, open, create, rename, and prompt
  cancellation.

Platform-specific:

- POSIX native behavior;
- Win32 UTF-8/UTF-16 conversion, Unicode filenames, identity/version, atomic
  replacement, and reparse-point rejection;
- DJGPP DOS paths, case-insensitive ordering, explicit parent entry, fingerprinted
  version token, replacement transaction, and native execution.

## Main Test Areas

Representative executables include:

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

### Linux job

- checkout repository and pinned fixtures;
- install pinned/declared toolchain dependencies;
- guard test oracles;
- configure Debug, Release, and sanitizer trees;
- static analysis;
- complete Debug tests;
- complete Release tests;
- sanitizer tests and leak checks where supported;
- compare Debug/Release manifests;
- verify DJGPP source-list synchronization;
- non-interactive startup smoke.

### Windows jobs

Both Debug and Release:

- pinned PDCurses/vcpkg setup;
- strict `/W4 /WX` configure/build;
- complete tests;
- native Win32 storage contract.

Windows currently has native storage. Remaining platform uncertainty is
interactive product maturity, not a missing adapter.

### DOS job

- install/pin DJGPP toolchain;
- pin/build PDCurses;
- install official CWSDPMI runtime;
- build and strip `retrodesk.exe`;
- build native `FSTEST.EXE`;
- execute storage contract in DOSBox-X;
- execute RetroDesk diagnostic smoke in DOSBox-X;
- create/upload distribution artifact and checksums;
- retain failed logs/state as diagnostic artifacts when needed.

## Latest Integrated Desktop Checkpoint

The compact head of PR #36 passed:

- CI run `29900244919`;
- DOS run `29900244867`.

The tested head was `f2e9e83ea2b6f2c55ad6ec9c4e9c414efe103985`.
The squash merge produced main commit
`31273f8c888b6074f416a938847e3124b88f9464`.

This is valid development evidence for the integrated changes. It is not an exact
release-candidate evidence bundle because the final merge SHA was not the PR head.

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

Strict direct CMake example:

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

`make test-sanitize` creates a dedicated Debug build with AddressSanitizer and
UndefinedBehaviorSanitizer. Leak detection is enabled on supported Linux
configurations. Findings are fatal.

Sanitizers do not replace:

- Windows strict compilation;
- native storage tests;
- DOS native execution;
- manual terminal smoke;
- failure-injection tests for allocation/ID boundaries.

## Manual Acceptance Matrix

Before a release claim, record at minimum:

### Linux ncurses

- all themes;
- Launcher keyboard/mouse;
- taskbar full/compact behavior;
- focus, minimize, maximize, and `F9` HUD;
- File Manager open/create/rename;
- Notepad UTF-8 edit/find/wrap/save/close;
- terminal restoration.

### Linux raw TTY + ANSI

- supported terminal emulators;
- keyboard modifiers and fragmented escape behavior;
- resize and redraw/flicker;
- application workflows without curses assumptions.

### `TERM=linux`

- keyboard-only complete workflow;
- no reliance on unsupported mouse/drag;
- terminal restoration.

### Windows/PDCurses

- native file browsing/edit/save with Unicode names;
- taskbar/Launcher/window controls;
- console restoration and close behavior.

### DOS/DJGPP

- reduced-profile startup;
- storage/file workflows within DOS path limits;
- taskbar/Launcher/window behavior at realistic DOS console size;
- documented replacement and filename limits.

## Regression Rules

- Public behavior changes update status and user references.
- Input/focus/window/storage/lifecycle changes add lower-level and end-to-end tests.
- Parser changes cover complete, fragmented, and invalid input.
- App-service changes test budgets, timeout policy, cleanup, and redraw intent.
- File-app changes test both adapter and app event flows.
- Platform-specific adapters receive native tests, not only cross-platform mocks.
- A headless green test is never described as complete interactive proof.
- A PR-only green branch is never described as integrated `main` behavior.

## Release Evidence

Release sign-off follows
[RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md). The evidence record must
contain:

- exact candidate SHA and clean-tree confirmation;
- compiler, CMake, curses/PDCurses, analyzer, Python, fixture, and DOS toolchain
  versions;
- complete job/run identifiers;
- test manifests and counts;
- sanitizer and static-analysis output;
- platform smoke/manual acceptance;
- artifact checksums;
- exact known issues and excluded profiles.

## Capacity and identifier boundary coverage

`wm_event_replay_test` covers checked capacity arithmetic, injected Window
Manager growth failure, successful retry, and deterministic `WindowId`
exhaustion at `INT_MAX`. `desktop_runtime_test` injects failure at the
8-to-16 running-app table transition and verifies that app/window counts and
existing entries remain unchanged before a successful retry.
