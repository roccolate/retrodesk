# Storage Contract

> Current against `main` at the 2026-07-22 snapshot.

## Purpose

`retro_fs` is the only filesystem boundary used by built-in file applications.
File Manager and Notepad must not call POSIX, Win32, DJGPP, or standard-library
filesystem APIs directly from UI behavior.

The public contract is `src/storage/retro_fs.h`. Integrated adapters are:

- `src/storage/retro_fs_posix.c`;
- `src/storage/retro_fs_win32.c`;
- `src/storage/retro_fs_djgpp.c`.

## Common Public Contract

All adapters expose domain-owned types for:

- path lifecycle, parent, and join;
- portable file kind;
- fixed-width size metadata;
- adapter-neutral optimistic-concurrency version token;
- bounded directory listing;
- validated text read;
- validated version-aware text write;
- exclusive empty-file creation;
- directory creation;
- rename without intentional overwrite.

No public storage header exposes `struct stat`, `mode_t`, Win32 handles, UTF-16
native strings, DOS directory structures, or backend-native identity types.

Applications may retain and return a `RetroFsVersion`, but only the adapter may
interpret its identity fields.

## Text Contract

### Accepted

- valid UTF-8 sequences;
- LF line endings;
- consistently encoded CRLF line endings;
- printable Unicode text within configured size limits.

### Rejected

- malformed, truncated, overlong, surrogate, or out-of-range UTF-8;
- embedded NUL;
- ESC and unsupported C0/C1 controls;
- mixed LF and CRLF styles;
- content above configured limits;
- unsupported target kinds such as directories;
- links/reparse points where the active adapter's safe text policy rejects them.

Writes validate all text before creating a temporary file or modifying a target.
Reads validate before returning content to Notepad.

The contract does not perform Unicode normalization and does not convert legacy
encodings.

## Directory Contract

A listing:

- is bounded by a caller-provided maximum;
- fails instead of intentionally returning a partial oversized result;
- returns name, portable kind, and `uint64_t` size;
- orders directories before regular files, then other kinds;
- uses deterministic adapter-appropriate name ordering;
- leaves hidden-file visibility to File Manager policy.

Ordering details:

- POSIX/Win32 use deterministic adapter-defined ordering compatible with their
  tests;
- DJGPP uses ASCII case-insensitive DOS filename ordering;
- DJGPP constructs `..` explicitly when the runtime's enumeration omits it.

## Mutation Contract

### Create file

`retro_fs_create_file()` creates an empty regular file exclusively. An existing
destination returns `RETRO_FS_CONFLICT`; it is never truncated intentionally.

### Create directory

`retro_fs_create_directory()` creates one directory and reports destination
conflicts explicitly.

### Rename

`retro_fs_rename()` changes the source to a new destination supported by the
adapter. Existing destinations are rejected instead of replaced.

Delete/trash, general copy/move, recursive traversal, progress, and cancellation
are not in the current public product contract.

## Optimistic Concurrency

Notepad retains the version returned when it reads or successfully writes a file.
A later save may provide that version as the expected version.

The adapter must:

1. read current target identity/metadata;
2. compare it to the expected token when present;
3. return `RETRO_FS_CONFLICT` without modifying the target if stale;
4. write the new content only after validation and version acceptance;
5. return a new version after success.

The API permits `expected` and `written` pointers to alias. Adapters must not
clear or overwrite the output token before expected-version comparison finishes.
A Win32 regression discovered and fixed this exact boundary.

## POSIX Adapter

### Native behavior

- POSIX path representation and helpers;
- `stat`-derived kind, identity, size, and nanosecond modification data;
- bounded directory enumeration;
- exclusive create and mkdir;
- no-overwrite rename;
- same-directory temporary text write;
- file-content synchronization before replacement;
- rename-based replacement;
- parent-directory synchronization where supported;
- stale-version conflict detection.

### Guarantees and limits

A successful replacement follows the strongest current durability sequence used
by the project, but filesystem/mount behavior still determines ultimate crash and
hardware guarantees. Unsupported text target kinds are rejected.

## Win32 Adapter

### Native boundary

The public API accepts UTF-8. The adapter validates and converts paths to UTF-16
before calling Win32 APIs.

Supported path forms include normal drive paths, UNC paths, and extended-length
forms required for native long-path behavior.

### Native behavior

- Unicode filenames and validated UTF-8 file content;
- deterministic directory listing;
- volume serial/file identity/size/kind/modification data in version tokens;
- exclusive create and directory creation;
- no-overwrite rename;
- same-directory temporary write;
- replacement through `ReplaceFileW` or `MoveFileExW` according to target state;
- stale-version conflict detection;
- rejection of reparse points for text editing instead of silently following
  them.

### Guarantees and limits

Native Win32 storage is integrated and tested in Debug and Release. Interactive
PDCurses maturity is a separate portability/evidence question; it is not a
missing-storage issue.

## DJGPP/DOS Adapter

### Native behavior

- DOS/DJGPP path lifecycle and stat;
- directory listing with explicit `..` when needed;
- ASCII case-insensitive deterministic ordering;
- regular text read/write with validated UTF-8 content;
- exclusive create, mkdir, and no-overwrite rename;
- version token including a content fingerprint to strengthen coarse FAT time
  metadata;
- same-directory 8.3 temporary filenames;
- backup/restore replacement transaction;
- native `FSTEST.EXE` contract validation inside DOSBox-X.

### Explicit limits

- filenames and paths are ASCII/DOS-oriented;
- the adapter does not provide a universal true atomic-replace syscall;
- FAT timestamp granularity is coarse;
- case behavior differs from POSIX;
- a process/emulator can report delayed directory-removal state during test
  teardown, so teardown is best-effort after all public operations are asserted;
- reduced memory/profile expectations apply.

UTF-8 document content is supported even though filenames are restricted.

## macOS Status

The intended implementation is the POSIX adapter. No current macOS build,
storage-test, or interactive evidence is attached to this project snapshot, so
macOS remains experimental and unclaimed.

## Failure and Data-Safety Rules

- Validate before mutation.
- Never intentionally overwrite an existing create/rename destination.
- Never silently ignore a stale expected version.
- Clean temporary files on failure where possible.
- Preserve the original target when replacement cannot be committed.
- Report adapter limitations explicitly.
- Do not return success after only a partial operation.
- Do not implement destructive UI features before recovery/partial-failure policy
  exists.

## Required Tests

Common contract tests must cover:

- path validation and parent/join;
- empty, deterministic, and bounded directory listing;
- portable kind/size metadata;
- UTF-8 read/write roundtrip;
- malformed UTF-8, NUL, controls, mixed newline, and oversized rejection;
- exclusive file/directory creation;
- duplicate destination conflict;
- rename success, missing source, and no-overwrite behavior;
- versioned save and stale-token conflict;
- aliasing of expected/written version pointers;
- unsupported target/link behavior;
- File Manager and Notepad workflows consuming the contract.

Platform-specific tests additionally cover:

- Win32 Unicode filenames, extended path conversion, identity, and reparse-point
  policy;
- DJGPP case-insensitive ordering, parent entry, fingerprinted versioning,
  replacement transaction, native DOS execution, and package contents.

## Current Non-Goals

- arbitrary binary file API;
- generic metadata editing;
- permissions/ACL UI;
- symlink/reparse traversal policy for file apps;
- trash/recovery service;
- recursive copy/move engine;
- cross-device move policy;
- remote/network filesystems with custom consistency semantics.

See [PORTABILITY.md](PORTABILITY.md), [BUILTIN_APPS.md](BUILTIN_APPS.md), and
[KNOWN_ISSUES.md](KNOWN_ISSUES.md) for profile and product limits.