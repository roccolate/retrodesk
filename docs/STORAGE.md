# Storage Contract

## Purpose

`storage` is the filesystem boundary used by built-in file apps. File Manager
and Notepad must use this contract instead of calling POSIX or platform APIs from
UI code.

## Current Implementation

- Public contract: `src/storage/retro_fs.h`.
- Active adapter: `src/storage/retro_fs_posix.c`.
- Complete product profile: Linux/POSIX.

The adapter currently supports:

- path initialization, destruction, parent, and join helpers;
- stat/version capture;
- bounded directory listing;
- validated UTF-8 text reads;
- exclusive empty-file creation;
- directory creation;
- rename without intentional overwrite;
- atomic writes through a temporary file;
- stale-version conflict detection.

## Text Contract

The text path accepts validated UTF-8 rather than the former ASCII-only subset.

Accepted content:

- valid UTF-8 codepoint sequences;
- LF or consistently encoded CRLF line endings;
- printable Unicode text within configured size limits.

Rejected content:

- malformed, truncated, overlong, surrogate, or out-of-range UTF-8;
- embedded NUL;
- ESC and unsupported C0/C1 control characters;
- mixed LF and CRLF styles;
- files larger than the configured storage limit;
- unsupported targets such as directories and symlinks for text editing.

Text writes are validated before the atomic-write path begins. Reads are
validated before content is delivered to Notepad.

## Directory Contract

Directory listing:

- is bounded by the caller-provided maximum;
- sorts directories before regular files, then other entry types;
- includes entry name, mode, and size metadata;
- fails before delivering an intentionally partial oversized result;
- leaves hidden-file filtering to File Manager policy.

## Mutation Contract

### Create file

`retro_fs_create_file()` creates an empty regular file exclusively. An existing
destination produces `RETRO_FS_CONFLICT`; it is never truncated intentionally.

### Create directory

`retro_fs_create_directory()` creates one directory and reports destination
conflicts explicitly.

### Rename

`retro_fs_rename()` moves a file or directory to a new name within the adapter’s
supported path model. Existing destinations are rejected rather than replaced.

Delete/trash, recursive copy, and general move workflows are not part of the
current public product slice.

## Atomic Write Contract

A successful save must:

1. validate the outgoing UTF-8 text;
2. reject unsupported targets;
3. compare the expected version when provided;
4. write data to a temporary file in the target directory;
5. sync file contents before rename;
6. rename the temporary file into place;
7. sync the parent directory where supported;
8. return the new file version.

A stale expected version returns a conflict without silently overwriting changes
made outside RetroDesk.

## Portability Status

The storage implementation is POSIX-only today. Windows, macOS, and DOS must not
be documented as supporting equivalent File Manager/Notepad storage workflows
until one of these is true:

- a native adapter exists and passes the storage/app test suite;
- a portable adapter exists and passes the same suite;
- filesystem-backed apps are explicitly gated off for that profile.

Portable runtime compilation alone is not a storage-support claim.

## Required Tests

Storage changes must cover:

- path validation and parent/join behavior;
- empty and bounded directory listing;
- UTF-8 read/write roundtrip;
- malformed UTF-8 and control-character rejection;
- consistent newline policy;
- too-large file rejection;
- exclusive file and directory creation;
- duplicate destination conflict;
- rename success, missing source, and no-overwrite behavior;
- atomic save and stale-version conflict;
- unsupported target rejection;
- app-level File Manager and Notepad workflows that consume the contract.
