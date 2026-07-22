# Release 0.1 Checklist (Reopened)

> No `v0.1.0` tag exists. This checklist is not satisfied. All evidence must be
> rebuilt or confirmed against one exact candidate commit; historical green PR
> heads are supporting development evidence, not automatic release sign-off.

## Candidate Record

Fill before starting release validation:

- Candidate SHA: `____________________________`
- Candidate branch: `_________________________`
- Clean-tree confirmation: `___________________`
- Date/time and operator: `____________________`
- Pinned `retrocore-spec` revision: `__________`
- Intended platform claims: `_________________`
- Intended exclusions: `______________________`

## Release Intent

`v0.1.0` is a reproducible **pre-alpha foundation/product preview**, not a stable
or feature-complete desktop. It may be tagged only after this checklist,
[PROJECT_STATUS.md](PROJECT_STATUS.md), and a release-specific known-issues record
agree with the exact candidate.

## Proposed Scope In

The candidate may include the following only when each applicable gate passes:

- layered runtime (`core`, `platform`, `render`, `wm`, `app`, `apps`, `ui`,
  `storage`);
- one event loop and one frame flush;
- transactional app close and global shutdown;
- Desktop-owned UTF-8 clipboard;
- budgeted app service callbacks;
- themed taskbar and Start-menu-style Launcher;
- focus, z-order, capability-gated drag, minimize/restore,
  maximize/unmaximize, and keyboard move/resize HUD;
- File Manager navigation, refresh, hidden files, text open, rename, new directory,
  and new empty file;
- Notepad UTF-8 editing, selection, clipboard, undo/redo, Find, Word Wrap,
  versioned Save/Save As, recovery, and Save/Discard/Cancel close;
- native POSIX and Win32 storage;
- reduced native DJGPP/DOS storage/build/smoke profile;
- curses/PDCurses runtime and explicitly claimed ANSI/raw-TTY behavior;
- exact automated and manual evidence for the platforms listed in the candidate
  record.

## Scope Out Unless Explicitly Added to the Candidate

- stable plugin/extension ABI;
- real terminal emulator, shell, PTY, or subprocess app;
- File Manager delete/trash, copy, move, recursion, progress, previews,
  bookmarks, or dual-pane mode;
- Notepad Replace, system clipboard, mouse editing, normalization, syntax
  highlighting, multiple encodings, or large-file strategy;
- native responsive Notepad menu chrome from stale PR #24;
- collection/`WindowId` hardening from stale PR #27 unless rebuilt and merged;
- taskbar overflow for arbitrary large catalogs;
- Settings, configuration UI, session persistence;
- polished installers/package managers beyond artifacts explicitly produced;
- macOS support unless current evidence is added;
- complete feature parity across Linux, Windows, and DOS;
- universal Unicode grapheme/normalization/width support.

## Gate 1 — Repository and Scope Integrity

- [ ] Candidate SHA exists on the intended release branch.
- [ ] Working tree is clean.
- [ ] No temporary workflow, debug applicator, generated build tree, or test
      artifact is committed.
- [ ] Open PR #27 is rebuilt/merged or explicitly listed as excluded risk.
- [ ] Open PR #24 is rebuilt/merged or explicitly listed as excluded feature.
- [ ] `PROJECT_STATUS.md` identifies the candidate baseline correctly.
- [ ] Release scope does not claim PR-only behavior.
- [ ] `KNOWN_ISSUES.md` is reviewed and a release-specific subset is prepared.

## Gate 2 — Build and Tooling

Linux/common:

- [ ] `make clean` removes stale configured trees.
- [ ] `make check-test-oracles` passes.
- [ ] `make check-build-sources` passes.
- [ ] `make strict` passes.
- [ ] `make test` passes.
- [ ] `make test-all` passes.
- [ ] Debug and Release CTest manifests match.
- [ ] `make test-sanitize` passes with supported ASan/UBSan/leak checks.
- [ ] `make smoke-ci` passes.
- [ ] Canonical CMake source lists and DJGPP parity are confirmed.
- [ ] No warning suppression weakens product code without documented reason.

Toolchain record:

- [ ] compiler name/version recorded;
- [ ] CMake version recorded;
- [ ] curses/PDCurses version/revision recorded;
- [ ] analyzer/sanitizer runtime versions recorded;
- [ ] Python version recorded;
- [ ] `retrocore-spec` revision recorded;
- [ ] DJGPP, PDCurses DOS, and CWSDPMI pins/checksums recorded;
- [ ] Windows toolchain/vcpkg/PDCurses inputs recorded.

## Gate 3 — Core Runtime and Ownership

- [ ] One main event loop confirmed by source review.
- [ ] One renderer flush path confirmed by source review/test.
- [ ] Desktop create failure rolls back every acquired owner.
- [ ] Built-in app registration produces a complete deterministic catalog or
      fails startup explicitly.
- [ ] App create failure invokes matching destroy rollback.
- [ ] App/window/resource-path ownership is documented and test-backed.
- [ ] Desktop clipboard ownership and isolation are test-backed.
- [ ] No app/widget polls platform input or flushes rendering directly.
- [ ] No nested modal/menu event loop exists.
- [ ] Service callbacks are bounded and non-blocking.
- [ ] Service redraw/timeout behavior passes.
- [ ] `Ctrl+Q` shutdown is transactional and cancellation-safe.
- [ ] Shutdown/rollback is idempotent where promised.

If collection/ID hardening is in scope:

- [ ] checked Desktop capacity growth passes failure injection;
- [ ] checked WM capacity growth passes failure injection;
- [ ] existing state survives failed growth;
- [ ] retry succeeds;
- [ ] `WindowId` exhaustion at `INT_MAX` is deterministic.

If not integrated, the absence is explicitly accepted in the release known issues.

## Gate 4 — Window Manager and Desktop Chrome

### Focus and modal policy

- [ ] focus/z-order behavior is deterministic;
- [ ] `F6` skips minimized/ineligible windows;
- [ ] modal routing prevents background input;
- [ ] fixed desktop chrome is not treated as an ordinary app window;
- [ ] clicking/focusing follows the documented policy.

### Taskbar and Launcher

- [ ] Apps toggles Launcher by keyboard and mouse where claimed;
- [ ] closed app launches;
- [ ] running background app focuses/raises;
- [ ] focused app minimizes;
- [ ] minimized app restores/focuses/raises;
- [ ] multiple Notepad instances cycle deterministically;
- [ ] full/compact labels match exact hit regions;
- [ ] clock/separators never act as app buttons;
- [ ] Launcher arrows/W/S/J/K/Home/End/accelerators work;
- [ ] Launcher mouse selection works where capability is claimed;
- [ ] no taskbar/Launcher shortcut steals printable app input.

### Minimize/maximize/window mode

- [ ] minimized window retains app state and geometry;
- [ ] minimized window receives no render/input/focus/modal/drag routing;
- [ ] fixed/modal windows reject minimize;
- [ ] `F8` maximize and exact geometry restore work;
- [ ] title-bar double click works where claimed;
- [ ] maximize reserves taskbar row and reflows on resize;
- [ ] maximize survives minimize/restore;
- [ ] fixed/modal windows reject maximize;
- [ ] `F9` enters MOVE on eligible window;
- [ ] arrows adjust geometry;
- [ ] `Tab` toggles RESIZE;
- [ ] `Enter`, `F9`, and `Esc` finish;
- [ ] focus change cancels operation;
- [ ] HUD text/geometry is usable at wide/medium/compact widths;
- [ ] blocked fixed/maximized cases provide deterministic guidance.

### Theme acceptance

For XP, Hacker, Amiga, and Win31:

- [ ] active/inactive/operation frames are legible;
- [ ] Apps normal/open states are distinct;
- [ ] idle/running/focused taskbar states are distinct;
- [ ] Launcher header/section/item/selected/footer are legible;
- [ ] curses visual acceptance recorded;
- [ ] ANSI visual acceptance recorded for profiles claimed.

## Gate 5 — File Manager

- [ ] launches by default through app runtime;
- [ ] current adapter start directory/fallback is correct;
- [ ] arrows/J/K/Page Up/Down/Home/End work;
- [ ] selected item remains visible;
- [ ] parent navigation works;
- [ ] refresh retains selection where documented;
- [ ] hidden files default off and toggle with `H`;
- [ ] directories open;
- [ ] regular validated text launches a separate Notepad instance;
- [ ] binary/unsupported target handling is deterministic;
- [ ] file size and empty directory state display safely;
- [ ] rename succeeds and never intentionally overwrites;
- [ ] new directory succeeds and selects result;
- [ ] new empty file succeeds and selects result;
- [ ] invalid names and duplicate destinations preserve existing data;
- [ ] prompt cancellation is safe;
- [ ] unreadable/oversized directory behavior is bounded;
- [ ] platform filename/path limits are documented.

## Gate 6 — Notepad

- [ ] validated UTF-8 file open;
- [ ] codepoint-safe insertion/navigation/Backspace/Delete;
- [ ] display-cell cursor/selection rendering;
- [ ] multiline Shift selection;
- [ ] Select All/Copy/Cut/Paste between Notepad instances;
- [ ] selection replacement is one undoable edit;
- [ ] undo/redo restores text/cursor/dirty state;
- [ ] 100-state/1 MiB stack bounds are enforced;
- [ ] Find wraps and preserves history;
- [ ] limited case folding/accent significance matches docs;
- [ ] Word Wrap changes visual rows only;
- [ ] existing file saves with expected version;
- [ ] stale version reports conflict without overwrite;
- [ ] untitled `Ctrl+S` and `F3` enter Save As;
- [ ] failed/cancelled Save As preserves document;
- [ ] dirty `Ctrl+W` offers Save/Discard/Cancel;
- [ ] dirty `Ctrl+Q` participates transactionally;
- [ ] recovery path behavior is verified;
- [ ] current UI does not claim PR #24 menu shortcuts unless merged.

## Gate 7 — Storage Contract

Common:

- [ ] path init/destroy/parent/join;
- [ ] portable kind/size/version metadata;
- [ ] bounded deterministic listing;
- [ ] valid UTF-8 and LF/consistent CRLF roundtrip;
- [ ] malformed UTF-8, NUL, ESC, controls, mixed newline, and oversize rejection;
- [ ] exclusive file and directory creation;
- [ ] duplicate destination conflict;
- [ ] rename success/missing source/no-overwrite;
- [ ] stale-version conflict;
- [ ] expected/written version pointer aliasing;
- [ ] unsupported target/link policy;
- [ ] app-level consumers pass.

POSIX:

- [ ] temporary-file write and sync path verified;
- [ ] replacement and parent sync behavior recorded;
- [ ] unsupported target/symlink policy verified.

Win32:

- [ ] UTF-8/UTF-16 conversion and Unicode filenames/data;
- [ ] drive/UNC/extended path behavior;
- [ ] deterministic listing;
- [ ] native identity/version token;
- [ ] `ReplaceFileW`/`MoveFileExW` replacement path;
- [ ] reparse-point rejection;
- [ ] native storage test passes in Debug and Release.

DJGPP/DOS:

- [ ] ASCII/DOS filename boundary documented;
- [ ] explicit parent entry and case-insensitive ordering;
- [ ] UTF-8 content validation;
- [ ] fingerprinted version token;
- [ ] 8.3 temp and backup/restore transaction;
- [ ] `FSTEST.EXE` passes in DOSBox-X;
- [ ] replacement is not described as a universal atomic syscall.

## Gate 8 — Input and Terminal Restoration

- [ ] curses key normalization passes;
- [ ] raw-TTY tested sequences pass;
- [ ] fragmented/invalid sequences do not leak bytes;
- [ ] modified arrows work where claimed;
- [ ] mouse decoding remains correct;
- [ ] editor controls reach apps without permanent raw mode;
- [ ] normal shutdown restores exact captured terminal state;
- [ ] initialization failure restores state;
- [ ] `make smoke` passes in representative PTY;
- [ ] `make smoke-linux-vc` passes under `TERM=linux`;
- [ ] forced-kill recovery guidance is documented.

## Gate 9 — Platform Acceptance

### Linux/ncurses

- [ ] complete desktop/theme/app workflow manually recorded;
- [ ] representative terminal emulator(s) and locale recorded;
- [ ] `TERM=linux` keyboard-first workflow recorded.

### Linux raw TTY/ANSI, if claimed

- [ ] terminal list and `TERM` values recorded;
- [ ] modifier/parser/resize behavior recorded;
- [ ] flicker/diff rendering accepted;
- [ ] complete keyboard app workflow recorded.

### Windows/PDCurses, if claimed

- [ ] Debug/Release/native storage automated evidence;
- [ ] interactive taskbar/Launcher/window operations;
- [ ] Unicode File Manager/Notepad workflow;
- [ ] console restoration;
- [ ] exact PDCurses/console limitations documented.

### DOS reduced profile, if distributed

- [ ] pinned native build and package;
- [ ] storage/app DOSBox-X smoke;
- [ ] manual realistic-console desktop/app workflow;
- [ ] filename/replacement/memory/input exclusions documented;
- [ ] checksums verified.

### macOS, only if claimed

- [ ] current clean build/tests;
- [ ] POSIX storage tests;
- [ ] interactive curses workflow;
- [ ] terminal restoration;
- [ ] known issues.

Otherwise macOS is explicitly omitted.

## Gate 10 — Documentation

- [ ] `README.md` matches current main and candidate scope;
- [ ] `INSTALL.md` matches current build/platform paths;
- [ ] `PROJECT_STATUS.md` uses candidate SHA and distinguishes unmerged work;
- [ ] `KNOWN_ISSUES.md` is current;
- [ ] `INDEX.md` links every active policy/reference;
- [ ] `ARCHITECTURE.md` matches current ownership and temporary bridges;
- [ ] `APP_RUNTIME.md` matches lifecycle/service behavior;
- [ ] `BUILTIN_APPS.md` matches integrated app behavior;
- [ ] `KEYBOARD_SHORTCUTS.md` matches actual routing;
- [ ] `STORAGE.md` matches all three native adapters;
- [ ] `PORTABILITY.md` separates automated and interactive evidence;
- [ ] `TESTING.md` matches active CI/DOS/manual matrix;
- [ ] `BUILD_SYSTEM.md` matches canonical and pinned DOS flow;
- [ ] `RETROTUI_GAP.md` removes completed minimize/maximize/storage gaps;
- [ ] `CORE_HARDENING_PLAN.md` distinguishes integrated/stale work;
- [ ] `TECH_DEBT_POLICY.md` has no resolved stale debt;
- [ ] `ROADMAP.md` marks integrated `v0.5.0` behavior complete;
- [ ] release notes and release-specific known issues are prepared.

## Gate 11 — Debt and Risk

- [ ] no known violation of `FOUNDATION_PRINCIPLES.md`;
- [ ] every temporary bridge/static debt has owner/reason/trigger/target;
- [ ] stale open PRs are resolved or explicitly excluded;
- [ ] no critical data-loss/lifecycle issue is open;
- [ ] collection/ID boundary risk is merged or accepted explicitly;
- [ ] `app_launch` ambiguity is fixed or documented as non-blocking for scope;
- [ ] taskbar overflow absence is acceptable for the fixed release catalog;
- [ ] Desktop bridge architecture is accepted only for pre-alpha scope;
- [ ] platform limitations are visible to users.

## Gate 12 — Artifacts and Provenance

- [ ] source tag points to exact candidate;
- [ ] release notes identify SHA and scope;
- [ ] compatibility matrix identifies claimed profiles;
- [ ] known-issues record identifies exclusions;
- [ ] Linux/Windows artifacts are reproducible if published;
- [ ] DOS artifact includes expected executable/runtime/test/readme/checksums;
- [ ] artifact SHA-256 values are recorded;
- [ ] build/toolchain provenance is recorded;
- [ ] no artifact was produced from a dirty or different tree.

## Required Evidence Bundle

Attach or link:

- exact SHA and clean-tree log;
- toolchain/dependency versions;
- CI/DOS workflow run IDs;
- Debug/Release test manifests and results;
- static-analysis/sanitizer output;
- source/oracle guard output;
- automated startup/storage/DOS logs;
- manual acceptance checklist for each claimed profile;
- screenshots/logs where visual behavior is part of acceptance;
- artifact names and checksums;
- release notes, compatibility table, and known issues.

## Historical Development Evidence

Useful but not sufficient by itself:

- latest desktop PR head CI: `29900244919`;
- latest desktop PR head DOS: `29900244867`;
- tested head: `f2e9e83ea2b6f2c55ad6ec9c4e9c414efe103985`;
- squash merge on main: `31273f8c888b6074f416a938847e3124b88f9464`.

A release candidate must have evidence attached to its own exact SHA.

## Tag Decision

- [ ] All in-scope gates complete.
- [ ] Every unchecked item is explicitly out of scope and appears in release notes.
- [ ] No contradictory documentation remains.
- [ ] Candidate is unchanged after evidence collection.
- [ ] Maintainer approves `v0.1.0` pre-alpha tag.

Only then create the tag and artifacts.