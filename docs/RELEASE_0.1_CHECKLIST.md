# Release 0.1 Checklist (Reopened)

> No `v0.1.0` tag exists. This checklist is not satisfied. Every result must be
> tied to one exact candidate commit; historical green PR heads are development
> evidence, not release sign-off.

## Candidate Record

Complete before validation:

- Candidate SHA: `________________________________________`
- Candidate branch: `_____________________________________`
- Clean-tree confirmation: `______________________________`
- Date/operator: `________________________________________`
- Pinned `retrocore-spec` revision: `_____________________`
- Intended platform claims: `____________________________`
- Explicit exclusions: `_________________________________`

Current documentation baseline before candidate selection:
`c36ceb92b269c4c485798eed2b8413ea6c096164`.

## Release Intent

`v0.1.0` is a reproducible **pre-alpha foundation/product preview**, not a stable
or feature-complete desktop. It may be tagged only after this checklist,
[PROJECT_STATUS.md](PROJECT_STATUS.md), and the release-specific known-issues
record agree with the unchanged exact candidate.

## Proposed Scope In

Only if the relevant gates pass:

- layered runtime and explicit ownership;
- one event loop and one flush;
- transactional app close/global shutdown;
- Desktop UTF-8 clipboard;
- bounded app service callback contract;
- themed taskbar and Start-menu-style Launcher;
- focus, z-order, drag policy, minimize/restore, maximize/unmaximize, and `F9`
  move/resize HUD;
- File Manager navigation, refresh, hidden files, text open, rename, mkdir, and
  new empty file;
- Notepad UTF-8 editing, selection, clipboard, undo/redo, Find, Word Wrap,
  versioned Save/Save As, recovery, and safe dirty close;
- native POSIX and Win32 storage;
- reduced native DJGPP/DOS storage/build/smoke profile;
- exact curses/PDCurses and explicitly claimed ANSI/raw-TTY behavior;
- exact evidence for every platform named in the release notes.

## Scope Out Unless Rebuilt and Added Explicitly

- stable plugin/extension ABI;
- shell, PTY, terminal emulator, or subprocess app;
- File Manager delete/trash/copy/move/recursion/progress/previews/bookmarks/dual
  pane;
- Notepad Replace/system clipboard/mouse editing/normalization/syntax/multiple
  encodings/large-file strategy;
- native responsive Notepad menu chrome from stale PR #24;
- collection/`WindowId` hardening from stale PR #27 unless rebuilt/merged;
- taskbar overflow for an arbitrary large catalog;
- Settings/configuration UI/session persistence;
- polished installers beyond explicitly produced artifacts;
- macOS support without current evidence;
- Linux/Windows/DOS feature parity;
- universal Unicode grapheme/normalization/width support.

## Gate 1 — Repository and Scope Integrity

- [ ] Candidate SHA exists on release branch.
- [ ] Working tree is clean.
- [ ] No temporary workflows/debug applicators/generated trees/artifacts are
      committed.
- [ ] #27 is rebuilt/merged or excluded as accepted risk.
- [ ] #24 is rebuilt/merged or excluded as unavailable feature.
- [ ] `PROJECT_STATUS.md` names the candidate baseline.
- [ ] No documentation claims PR-only behavior.
- [ ] `KNOWN_ISSUES.md` is reviewed and a release-specific record is prepared.
- [ ] Candidate remains unchanged after evidence collection.

## Gate 2 — Build, Tooling, and Provenance

- [ ] `make clean` completes.
- [ ] `make check-test-oracles` passes.
- [ ] `make check-build-sources` passes.
- [ ] `make strict` passes.
- [ ] `make test` passes.
- [ ] `make test-all` passes.
- [ ] Debug/Release CTest manifests match.
- [ ] `make test-sanitize` passes with supported ASan/UBSan/leak checks.
- [ ] `make smoke-ci` passes.
- [ ] Canonical CMake and DJGPP source parity are confirmed.
- [ ] Product code keeps full warning policy.

Record:

- [ ] compiler and linker versions;
- [ ] CMake version;
- [ ] curses/PDCurses versions/revisions;
- [ ] analyzer/sanitizer versions;
- [ ] Python version;
- [ ] fixture revision;
- [ ] Windows toolchain/vcpkg inputs;
- [ ] DJGPP/PDCurses/CWSDPMI revisions and checksums;
- [ ] workflow run IDs and artifact checksums.

## Gate 3 — Core Runtime and Ownership

- [ ] one event loop confirmed;
- [ ] one renderer flush owner confirmed;
- [ ] Desktop partial init rolls back every acquired object;
- [ ] built-in registration is complete or startup fails explicitly;
- [ ] descriptor create failure invokes matching destroy;
- [ ] app/window/resource ownership matches architecture docs;
- [ ] clipboard ownership/isolation passes;
- [ ] no app/widget directly polls or flushes;
- [ ] no nested modal/menu loop;
- [ ] app service callback is bounded and non-blocking;
- [ ] service budget/timeout/redraw tests pass;
- [ ] `Ctrl+Q` is transactionally cancellation-safe;
- [ ] shutdown/rollback idempotence promises pass.

If #27 hardening is included:

- [ ] Desktop and WM checked growth boundaries pass;
- [ ] injected allocation failure preserves state;
- [ ] retry succeeds;
- [ ] `WindowId` exhaustion at `INT_MAX` is deterministic.

Otherwise its absence appears in release known issues.

## Gate 4 — Window Manager and Desktop Chrome

### Focus/modal

- [ ] deterministic focus/z-order;
- [ ] `F6` skips minimized/ineligible windows;
- [ ] modal routes block background input;
- [ ] fixed desktop chrome is not ordinary movable app surface;
- [ ] mouse focus follows documented policy where claimed.

### Taskbar/Launcher

- [ ] Apps toggles Launcher by keyboard and mouse where claimed;
- [ ] closed app launches;
- [ ] running background app focuses/raises;
- [ ] focused app minimizes;
- [ ] minimized app restores/focuses/raises;
- [ ] multiple Notepad instances cycle;
- [ ] full/compact labels match hit regions;
- [ ] clock/separators are never buttons;
- [ ] Launcher arrows/W/S/J/K/Home/End/accelerators work;
- [ ] Launcher mouse behavior works where claimed;
- [ ] no global route steals printable app input.

### Minimize/maximize/operation mode

- [ ] minimized app retains state/geometry;
- [ ] minimized window gets no render/input/focus/modal/drag;
- [ ] fixed/modal windows reject minimize;
- [ ] `F8` maximize/exact restore works;
- [ ] double click works where claimed;
- [ ] maximize reserves taskbar row/reflows on resize;
- [ ] maximize survives minimize/restore;
- [ ] fixed/modal windows reject maximize;
- [ ] `F9` MOVE and `Tab` RESIZE work;
- [ ] arrows adjust geometry;
- [ ] `Enter`, `F9`, and `Esc` finish;
- [ ] focus change cancels operation;
- [ ] HUD is usable at wide/medium/compact width;
- [ ] fixed/maximized blocked guidance is deterministic.

### Theme acceptance

For XP, Hacker, Amiga, Win31:

- [ ] active/inactive/operation frames legible;
- [ ] Apps normal/open distinct;
- [ ] idle/running/focused taskbar states distinct;
- [ ] Launcher hierarchy/selection legible;
- [ ] curses acceptance recorded;
- [ ] ANSI acceptance recorded if claimed.

## Gate 5 — File Manager

- [ ] launches by default;
- [ ] start directory/fallback correct for adapter;
- [ ] arrows/J/K/Page/Home/End work;
- [ ] selection stays visible;
- [ ] parent/refresh work and retain selection as documented;
- [ ] hidden files default off and toggle;
- [ ] directory and validated text open;
- [ ] unsupported target handling deterministic;
- [ ] size/empty state safe;
- [ ] rename succeeds without overwrite;
- [ ] mkdir/new file succeed and select result;
- [ ] invalid/duplicate names preserve data;
- [ ] prompt cancellation safe;
- [ ] unreadable/oversized directory behavior bounded;
- [ ] platform filename/path limits documented.

## Gate 6 — Notepad

- [ ] validated UTF-8 open;
- [ ] codepoint-safe edit/navigation/deletion;
- [ ] display-cell cursor/selection rendering;
- [ ] multiline selection;
- [ ] Select All/Copy/Cut/Paste across instances;
- [ ] selection replacement is one undoable edit;
- [ ] undo/redo text/cursor/dirty behavior;
- [ ] 100-state/1 MiB limits;
- [ ] Find wraps without history mutation;
- [ ] documented case/accent behavior;
- [ ] Word Wrap visual-only;
- [ ] version-aware save/stale conflict;
- [ ] untitled Save/F3 Save As;
- [ ] failed/cancelled Save As preserves document;
- [ ] Save/Discard/Cancel dirty close;
- [ ] dirty global shutdown transaction;
- [ ] recovery behavior;
- [ ] no #24 shortcuts claimed unless merged.

## Gate 7 — Storage

Common:

- [ ] path lifecycle/parent/join;
- [ ] portable kind/size/version;
- [ ] bounded deterministic listing;
- [ ] valid UTF-8 LF/consistent CRLF roundtrip;
- [ ] malformed UTF-8/NUL/ESC/control/mixed newline/oversize rejection;
- [ ] exclusive file/mkdir;
- [ ] duplicate conflict;
- [ ] rename success/missing/no-overwrite;
- [ ] stale-version conflict;
- [ ] expected/written version aliasing;
- [ ] unsupported target/link policy;
- [ ] File Manager/Notepad consumers pass.

POSIX:

- [ ] temp write/sync/replacement path;
- [ ] parent sync behavior recorded;
- [ ] link/unsupported policy verified.

Win32:

- [ ] UTF-8/UTF-16 and Unicode names/data;
- [ ] drive/UNC/extended paths;
- [ ] deterministic listing and native version identity;
- [ ] `ReplaceFileW`/`MoveFileExW` behavior;
- [ ] reparse rejection;
- [ ] native storage tests Debug/Release.

DJGPP/DOS:

- [ ] ASCII/DOS name boundary documented;
- [ ] parent entry/case ordering;
- [ ] UTF-8 content;
- [ ] fingerprinted version;
- [ ] 8.3 temp backup/restore transaction;
- [ ] `FSTEST.EXE` passes in DOSBox-X;
- [ ] replacement is not called universally atomic.

## Gate 8 — Input and Restoration

- [ ] curses key normalization;
- [ ] raw-TTY claimed sequences;
- [ ] fragmented/invalid sequences do not leak;
- [ ] modified arrows where claimed;
- [ ] mouse decoder stability;
- [ ] editor controls reach apps without permanent raw mode;
- [ ] normal shutdown restores terminal state;
- [ ] init failure restores state;
- [ ] `make smoke` passes in representative PTY;
- [ ] `make smoke-linux-vc` passes;
- [ ] forced-kill recovery guidance remains documented.

## Gate 9 — Platform Acceptance

Linux/ncurses:

- [ ] complete desktop/theme/app workflow recorded;
- [ ] terminal emulator, `TERM`, locale recorded;
- [ ] `TERM=linux` keyboard-first workflow recorded.

Raw TTY/ANSI if claimed:

- [ ] terminal list/`TERM` recorded;
- [ ] modifiers/parser/resize accepted;
- [ ] flicker/diff accepted;
- [ ] full keyboard workflow recorded.

Windows if claimed:

- [ ] strict Debug/Release/native storage automated evidence;
- [ ] interactive taskbar/Launcher/window operations;
- [ ] Unicode file browse/edit/save;
- [ ] console restoration;
- [ ] exact PDCurses limitations.

DOS if distributed:

- [ ] pinned native build/package;
- [ ] storage/app DOSBox-X smoke;
- [ ] manual realistic-console workflow;
- [ ] filename/replacement/memory/input limits;
- [ ] checksums verified.

macOS only if claimed:

- [ ] current clean build/tests/storage;
- [ ] interactive curses workflow/restoration;
- [ ] known issues.

Otherwise macOS is explicitly omitted.

## Gate 10 — Documentation

- [ ] README and INSTALL match candidate;
- [ ] PROJECT_STATUS uses candidate SHA and separates unmerged work;
- [ ] KNOWN_ISSUES is current;
- [ ] INDEX links all active documents;
- [ ] ARCHITECTURE matches ownership/bridges;
- [ ] APP_RUNTIME matches lifecycle/service;
- [ ] BUILTIN_APPS/KEYBOARD match integrated behavior;
- [ ] STORAGE covers POSIX/Win32/DJGPP;
- [ ] PORTABILITY separates automated/interactive claims;
- [ ] TESTING/BUILD_SYSTEM match active workflows;
- [ ] RETROTUI_GAP removes completed gaps;
- [ ] CORE_HARDENING distinguishes stale/integrated work;
- [ ] TECH_DEBT has no resolved stale debt;
- [ ] ROADMAP marks integrated `v0.5.0` slices;
- [ ] release notes, compatibility, and release known issues prepared.

## Gate 11 — Debt and Risk

- [ ] no known foundation-principle violation;
- [ ] temporary bridge/static debt has owner/reason/trigger/target;
- [ ] stale PRs resolved or excluded;
- [ ] no critical data-loss/lifecycle issue;
- [ ] collection/ID risk merged or explicitly accepted;
- [ ] `app_launch` ambiguity accepted/fixed for scope;
- [ ] fixed catalog/no overflow acceptable for release scope;
- [ ] bridge architecture explicitly pre-alpha;
- [ ] platform limitations visible.

## Gate 12 — Artifacts

- [ ] tag points to unchanged candidate;
- [ ] release notes identify SHA/scope;
- [ ] compatibility matrix identifies claimed profiles;
- [ ] release known issues identify exclusions;
- [ ] artifacts are reproducible where published;
- [ ] DOS package contents/checksums correct;
- [ ] provenance recorded;
- [ ] no artifact from dirty/different tree.

## Required Evidence Bundle

- exact SHA and clean-tree log;
- dependency/toolchain versions;
- workflow IDs;
- Debug/Release manifests/results;
- static-analysis/sanitizer output;
- automated storage/startup/DOS logs;
- manual acceptance per profile;
- screenshots/logs for visual acceptance;
- artifact names/checksums;
- release notes/compatibility/known issues.

## Historical Development Evidence

Supporting, not sufficient:

- PR #36 CI `29900244919`;
- PR #36 DOS `29900244867`;
- tested head `f2e9e83ea2b6f2c55ad6ec9c4e9c414efe103985`;
- current integrated documentation baseline
  `c36ceb92b269c4c485798eed2b8413ea6c096164`.

## Tag Decision

- [ ] all in-scope gates complete;
- [ ] every unchecked item is explicitly out of scope in release notes;
- [ ] no contradictory documentation;
- [ ] candidate unchanged after evidence;
- [ ] maintainer approves pre-alpha `v0.1.0` tag.

Only then create tag and artifacts.