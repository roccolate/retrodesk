# Release 0.1 Checklist (reopened)

> No `v0.1.0` tag exists and this checklist is not satisfied. Every check below
> requires evidence tied to the exact candidate commit. The integrated `main`
> branch contains the current File Manager, taskbar, UTF-8 storage, Notepad,
> input, test, and CI work, but it is not a release candidate until these gates
> are rebuilt from a clean tree and interactive smoke evidence is recorded.

## Intent

`v0.1.0` remains an unshipped foundation/product-preview milestone. It may be
tagged only when the evidence below is complete for the exact tag commit.

This is not a stable or feature-complete desktop release.

## Scope In

- Layered runtime (`core`, `platform`, `render`, `wm`, `app`, `apps`, `ui`, and
  `storage`) operating as one system.
- Canonical CMake build with Make wrappers and synchronized DJGPP source list.
- Capability-aware input, drag, resize, app launch, and storage behavior.
- Interactive taskbar with Launcher, app state, instance counts, and clock.
- File Manager keyboard viewport, navigation, refresh, hidden files, regular
  text-file open, rename, new directory, and new file.
- Notepad UTF-8 editing, selection, shared clipboard, bounded undo/redo, Find,
  Word Wrap, Save, Save As, version conflict, and safe dirty close.
- Linux/POSIX validated UTF-8 storage with atomic writes.
- Diagnostics explicitly scoped as runtime information, not a shell.

## Scope Out

- Plugin ecosystem and stable external extension ABI.
- PTY/shell terminal.
- File Manager delete/trash, copy, move, recursive operations, previews,
  bookmarks, and dual-pane workflows.
- Notepad Replace, native system clipboard, rich text, syntax highlighting,
  multiple encodings, and very-large-file editing.
- Minimize/maximize, Settings, session persistence, and packaging polish.
- Full feature parity across planned platform tiers.

## Exit Criteria

All items below must be satisfied before tagging `v0.1.0`.

### 1. Build and Tooling Gate

- [ ] `make clean` removes stale CMake cache paths.
- [ ] `make strict` passes on Linux with curses development headers installed.
- [ ] `make test` passes on Linux.
- [ ] `make test-all` passes and Debug/Release CTest manifests match.
- [ ] `make smoke` passes in an interactive terminal.
- [ ] `make smoke-ci` passes in non-interactive CI/sandbox environments.
- [ ] `make smoke-linux-vc` passes in an interactive PTY with `TERM=linux`.
- [ ] `python3 scripts/check_test_oracles.py` reports always-active test oracles.
- [ ] `python3 scripts/check_djgpp_sources.py` reports CMake and DJGPP source
      manifests in sync.
- [ ] `Makefile` remains a thin CMake wrapper with no divergent production list.

### 2. Runtime and Desktop Gate

- [ ] Single main event loop confirmed.
- [ ] Single frame-flush path confirmed.
- [ ] Multi-window focus and z-order are deterministic.
- [ ] Drag behavior follows capability policy and keyboard fallback remains usable.
- [ ] `F9` move/resize mode, `Tab` toggle, arrow operations, and `Esc` exit work.
- [ ] Taskbar app launch, focus, and multi-instance cycling work.
- [ ] Launcher and global shortcut routing do not steal printable app text.
- [ ] Shutdown restores terminal state and leaves no known ownership leak.

### 3. App Runtime Gate

- [ ] App registry rejects duplicate registration.
- [ ] Capability-based launch rejection is enforced.
- [ ] App create failure invokes matching rollback cleanup.
- [ ] Closing an app releases its window and state cleanly.
- [ ] `can_close` can defer close without nested loops.
- [ ] Multiple Notepad instances launch and cycle predictably.

### 4. File Manager Gate

- [ ] File Manager launches by default through the app runtime.
- [ ] Arrow, `J/K`, Page Up/Down, Home/End, parent, and refresh work.
- [ ] Selection remains visible and is retained where promised.
- [ ] Hidden files default off and toggle with `H`.
- [ ] Directories open and regular text files launch Notepad.
- [ ] Rename succeeds without intentional overwrite.
- [ ] New directory and new empty file succeed and select the new entry.
- [ ] Invalid names, duplicate targets, and prompt cancellation are deterministic.
- [ ] Empty, unreadable, and oversized directory states are handled safely.

### 5. Notepad Gate

- [ ] UTF-8 insertion, cursor movement, Backspace, and Delete preserve codepoints.
- [ ] Selection and cursor rendering use terminal display cells.
- [ ] Select All, Copy, Cut, and Paste work between Notepad instances.
- [ ] Undo/redo restores text, cursor, dirty state, and respects memory limits.
- [ ] Find selects repeated results and wraps without mutating history.
- [ ] Word Wrap changes visual rows without changing saved logical text.
- [ ] Existing files save atomically with stale-version conflict reporting.
- [ ] Untitled Save and `F3` enter Save As.
- [ ] Dirty close offers Save, Discard, and Cancel.
- [ ] Failed save does not silently close or lose the document.

### 6. Storage Gate

- [ ] POSIX adapter rejects unsupported file types.
- [ ] Reads and writes accept valid UTF-8 and reject malformed UTF-8, NUL, ESC,
      unsupported controls, mixed newline styles, and oversized content.
- [ ] Directory listing is bounded and does not intentionally deliver partial
      oversized results.
- [ ] Exclusive file/directory creation reports destination conflicts.
- [ ] Rename rejects existing destinations rather than replacing them silently.
- [ ] Atomic save uses a temporary file and detects stale versions.
- [ ] Non-POSIX builds have a native adapter or deterministic file-app gating.

### 7. Input and Portability Gate

- [ ] Curses and raw-TTY key normalization are validated.
- [ ] Modified arrows are decoded where the terminal reports them.
- [ ] Fragmented/invalid escape sequences do not leak bytes into apps.
- [ ] Editor controls reach Notepad without permanent `raw()` mode.
- [ ] Original terminal mode is restored on shutdown and initialization failure.
- [ ] Linux keyboard baseline works under normal terminal and `TERM=linux` policy.
- [ ] Windows claim is honest: portable PDCurses builds/tests plus native storage
      or explicit product gating.
- [ ] macOS and DOS are documented with exact experimental/reduced exclusions.

### 8. Documentation Gate

- [ ] `README.md` matches the implemented app/taskbar/input behavior and limits.
- [ ] `INSTALL.md` distinguishes recommended and bring-up profiles.
- [ ] `docs/INDEX.md` links all active policies and user references.
- [ ] `docs/BUILTIN_APPS.md` describes current workflows and exclusions.
- [ ] `docs/KEYBOARD_SHORTCUTS.md` matches actual routing.
- [ ] `docs/STORAGE.md` matches the validated UTF-8 and mutation contracts.
- [ ] `docs/ROADMAP.md` separates implemented work from remaining milestones.
- [ ] `docs/TESTING.md` matches the active CI and smoke matrix.
- [ ] `docs/PORTABILITY.md` separates build evidence from product support.

### 9. Debt Gate

- [ ] No known violation of `FOUNDATION_PRINCIPLES.md`.
- [ ] Accepted temporary debt has owner, reason, removal trigger, and milestone.
- [ ] No hidden runtime owner or nested modal event loop has been introduced.
- [ ] Known hardening items are completed or explicitly deferred.

## Validation Commands (Linux)

```bash
make clean
make strict
make test
make test-all
make smoke
make smoke-ci
make smoke-linux-vc
```

If an old `build/` directory points at another absolute checkout, remove it
before collecting evidence.

## Validation Commands (Windows)

A Windows product claim requires Debug/Release evidence with PDCurses and a
storage adapter or deterministic storage-app gating.

```bash
cmake -S . -B build-windows \
  -DPDCURSES_ROOT=/path/to/PDCurses \
  -DENABLE_WERROR=ON \
  -DENABLE_STRICT_WARNINGS=ON
cmake --build build-windows --config Debug
cmake --build build-windows --config Release
ctest --test-dir build-windows --output-on-failure -C Debug
ctest --test-dir build-windows --output-on-failure -C Release
```

## Release Artifacts

- Git tag: `v0.1.0`.
- Release notes covering architecture, Linux product slice, taskbar, File
  Manager, Notepad, platform status, and known limits.
- Compatibility/known-issues record tied to the exact commit.

## Evidence Required Per Candidate

- commit SHA and clean-tree confirmation;
- compiler, CMake, curses/PDCurses, Python, and analyzer versions;
- Debug and Release test counts and manifests;
- static-analysis and sanitizer output where available;
- smoke logs for every claimed runtime profile;
- immutable `retrocore-spec` revision when fixture tests are required;
- manual acceptance record for each claimed app and platform workflow.

## Post-Release Immediate Priorities

1. Resolve native Windows storage portability or explicit app gating.
2. Add File Manager delete/trash/copy/move only after safety contracts exist.
3. Add Notepad Replace and native clipboard only through portable interfaces.
4. Define minimize/restore and maximize before extending taskbar behavior.
5. Complete packaging, configuration, and remaining core hardening work.
