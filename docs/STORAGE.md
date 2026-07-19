# Storage Contract

## Purpose

`storage` is the filesystem boundary used by built-in file apps. File Manager
and Notepad should use this contract instead of calling POSIX or platform APIs
directly.

## Current Implementation

- Public contract: `src/storage/retro_fs.h`.
- Active adapter: `src/storage/retro_fs_posix.c`.
- Build profile: Linux/POSIX preview only.

The adapter currently supports:

- path init/destroy/parent/join helpers,
- stat/version capture,
- bounded directory listing,
- ASCII-oriented text reads up to the implementation limit,
- atomic writes through a temporary file,
- conflict detection through captured file version metadata.

## Text Contract

The current text path is intentionally conservative:

- byte/ASCII oriented,
- rejects NUL, escape bytes, unsupported control characters, and bytes >= 127,
- rejects mixed LF and CRLF newline style,
- rejects files larger than the configured storage limit.

This is a preview constraint, not a Unicode policy for v1.

## Directory Contract

Directory listing sorts directories before regular files, then other entries.
The adapter rejects directories larger than the configured maximum before
delivering partial callback results.

## Atomic Write Contract

Saves must:

- reject unsupported targets such as directories and symlinks,
- compare expected file version when provided,
- write data to a temporary file in the target directory,
- fsync file contents before rename,
- rename into place,
- fsync the parent directory when supported.

## Portability Status

The storage layer is POSIX-only today. Native Windows, macOS, and DOS must not
be documented as supporting File Manager/Notepad storage workflows until one of
these is true:

- a native adapter exists and passes tests,
- a portable adapter exists and passes tests,
- file apps are explicitly gated off for that profile.

## Required Tests

Storage changes must cover:

- path validation and parent/join behavior,
- empty and bounded directory listing,
- read success and invalid-text rejection,
- too-large file rejection,
- atomic save success,
- stale-version conflict,
- unsupported target rejection,
- app-level open/save flows that consume the storage contract.
