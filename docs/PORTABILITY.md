# Portability Policy and Support Matrix

> Snapshot: 2026-07-22. Platform claims distinguish implementation, automated
> evidence, interactive evidence, and release support.

## Support Vocabulary

- **Implemented** — code exists in `main`.
- **Automated** — current CI/build/tests exercise the profile.
- **Interactively validated** — representative human-driven runtime workflows
  have been recorded for the exact candidate.
- **Release-supported** — the exact release candidate satisfies all applicable
  automated, interactive, documentation, and known-issue gates.

RetroDesk currently has implemented/automated profiles but no release-supported
stable version.

## Platform Matrix

| Profile | Runtime/build | Storage | Automated evidence | Interactive evidence | Claim |
| --- | --- | --- | --- | --- | --- |
| Linux + ncurses | Primary native curses path | POSIX adapter | Static analysis, strict Debug/Release, tests, sanitizers, smoke | Historical use; exact-candidate matrix incomplete | Primary pre-alpha product profile |
| Linux raw TTY + ANSI | Experimental normalized input + ANSI diff renderer | POSIX adapter | Decoder, renderer, integration, smoke-compatible tests | Broader terminal matrix incomplete | Experimental fallback |
| Windows + PDCurses | Native PDCurses input/render build | Win32 adapter | Debug/Release under `/W4 /WX`, native storage contract, app/runtime tests | Current native workflow record incomplete | Validated pre-alpha build/test profile |
| DOS + DJGPP/PDCurses | Native reduced executable | DJGPP adapter | Pinned build, `FSTEST.EXE`, DOSBox-X diagnostic, package artifact | Automated emulator smoke; broader manual UX incomplete | Validated reduced profile |
| macOS + curses | Expected POSIX/curses path | POSIX adapter design | No current evidence in this snapshot | None recorded | Experimental/unclaimed |

ADR-0002 describes the long-term tier target. This table describes current
reality.

## Backend Matrix

### Input

- Linux/macOS default: curses.
- Linux/macOS optional: raw TTY decoder.
- Windows: PDCurses.
- DOS: PDCurses built with the pinned DJGPP toolchain.

### Rendering

- Curses/PDCurses backend for the native terminal library path.
- ANSI renderer with frame diffing for compatible raw-TTY/fallback use.

Unsupported backend combinations fail during configuration/CLI normalization
rather than silently initializing a mismatched partial profile.

## Capability Contract

`PlatformFeatures` reports capabilities such as:

- keyboard;
- basic mouse;
- reliable drag;
- resize events;
- color;
- basic Unicode behavior;
- double click;
- right click;
- Linux virtual-console keyboard-only policy.

Apps and Desktop behavior use capabilities rather than scattered OS guesses.
Unsupported behavior is disabled or rejected explicitly.

## Terminal Input Policy

The normalized event contract carries:

- key code;
- portable printable byte/codepoint information;
- modifier bits;
- mouse position/button/movement/click state;
- resize data where supported.

Raw-TTY decoding covers tested CSI navigation/modifier sequences, including
fragmented input. Invalid sequences do not leak bytes into applications.

Curses remains in `cbreak()` mode. On POSIX, terminal controls used by Notepad are
selectively released from flow-control/signal ownership where supported. The
captured original terminal state is restored during normal shutdown and
initialization rollback.

## Linux Virtual Console (`TERM=linux`)

Mandatory behavior:

- keyboard-first operation;
- Launcher, taskbar, File Manager, Notepad, window focus, maximize, and
  move/resize remain accessible without mouse;
- mouse/drag/right-click are disabled unless the active backend proves reliable;
- no feature may depend exclusively on pointer semantics.

`make smoke-linux-vc` is the policy smoke command, but exact-candidate interactive
evidence is still required.

## Storage Matrix

### POSIX

Provides:

- native paths, stat/version, bounded listing;
- validated UTF-8 text read/write;
- exclusive create and mkdir;
- no-overwrite rename;
- temporary-file replacement and parent-directory sync where supported;
- stale-version conflict detection.

### Win32

Provides:

- UTF-8 public strings converted to UTF-16;
- drive, UNC, and extended-length path handling;
- deterministic directory listing and Unicode filenames/data;
- volume/file identity, size, kind, and modification information in version
  tokens;
- exclusive create, mkdir, and no-overwrite rename;
- same-directory temporary writes committed through `ReplaceFileW` or
  `MoveFileExW` as appropriate;
- rejection of reparse points for text operations.

Native storage is integrated. Remaining Windows uncertainty is primarily
interactive/PDCurses maturity, not missing filesystem code.

### DJGPP/DOS

Provides:

- native DOS path/stat/list/read/write/create/mkdir/rename;
- explicit parent `..` construction when enumeration omits it;
- deterministic ASCII case-insensitive ordering;
- validated UTF-8 file content;
- version tokens strengthened with a content fingerprint because FAT timestamps
  are coarse;
- same-directory 8.3 temporary files and backup/restore replacement;
- native contract validation in `FSTEST.EXE` inside DOSBox-X.

Limits:

- filename/path boundary is ASCII/DOS-oriented;
- no universal true atomic-replace syscall;
- case semantics differ from POSIX;
- reduced memory/runtime profile;
- raw POSIX TTY behavior is not available;
- advanced pointer semantics and Tier 1 parity are not claimed.

### macOS

The POSIX adapter is the intended path, but code design is not current evidence.
No macOS product claim is made until a current clean build/test/interactive record
exists.

## Unicode Scope

Implemented portable behavior:

- strict UTF-8 validation;
- codepoint-boundary cursor and selection operations;
- display-cell-aware editing/navigation/rendering for tested characters;
- validated UTF-8 clipboard and file content;
- Unicode Win32 filenames through UTF-16 conversion;
- DOS UTF-8 content with ASCII-oriented names.

Not claimed:

- Unicode normalization;
- universal grapheme-cluster editing;
- complete emoji/variation-selector/combining-sequence behavior;
- universal ambiguous-width policy;
- accent-insensitive Find;
- arbitrary legacy text encodings.

## Window/Desktop Portability

Keyboard contracts are authoritative. Mouse drag and title-bar double click are
optional capability paths.

- Minimize/restore is available through the taskbar and programmatic focus.
- Maximize is available through `F8`, with double click as an optional pointer
  equivalent.
- Move/resize is always keyboard-accessible through `F9` on an eligible window.
- The operation HUD adapts to wide, medium, and compact terminals.
- Taskbar labels compact at tested widths; a future larger-catalog overflow model
  remains pending.

## Degradation Policy

A profile may:

- hide/disable an unsupported feature;
- reject an app launch when a required capability is absent;
- use keyboard alternatives;
- use a documented reduced storage guarantee.

A profile may not:

- silently pretend an operation succeeded;
- expose a partially linked storage app;
- invent modifier/mouse semantics;
- bypass lifecycle/storage contracts for platform convenience;
- be called release-supported solely because it compiles.

## Evidence Required for a Platform Claim

For the exact release candidate:

- compiler/toolchain and dependency versions;
- clean configure/build logs;
- strict Debug and Release test manifests;
- platform-specific storage contract tests;
- startup/diagnostic smoke;
- terminal/console restoration checks;
- manual taskbar/Launcher/window/app workflow record;
- explicit known limitations;
- artifact checksum/provenance where distributed.

See [TESTING.md](TESTING.md) and
[RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md).