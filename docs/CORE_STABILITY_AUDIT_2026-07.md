# Core Stability Audit — July 2026

## Verdict

RetroDesk has a credible engineering core rather than a prototype assembled
around terminal effects. The architecture, ownership model, app lifecycle,
UTF-8 handling, storage validation, and automated test strategy are strong
enough for continued development and Linux dogfooding.

The contracts should not yet be frozen. The remaining work is hardening and
structural cleanup, not a rewrite.

Current qualitative assessment: **7.8/10**.

## Strong Foundations

- Explicit `core`, `platform`, `render`, `wm`, `app`, `apps`, `ui`, and
  `storage` layers.
- One main event loop and one renderer flush point.
- Normalized input events rather than backend-native input leaking into apps.
- App create-failure rollback through the matching descriptor destroy path.
- Transactional global shutdown with Allowed, Deferred, and Cancelled results.
- Non-blocking modal policy.
- UTF-8-safe text editing and bounded history.
- Validated text storage, bounded directory listing, atomic temporary writes,
  and stale-version detection.
- Strict warnings, `-Werror`, static analysis, Debug/Release tests,
  sanitizers, leak checks, smoke checks, Windows builds, and fixture pinning.

## Remaining Risks

### P0 — Dynamic collection overflow consistency

The app registry and draw-list collections already protect capacity arithmetic,
but the Desktop running-app table and Window Manager window table still grow by
unchecked multiplication.

Required changes:

- reject `count + 1` overflow;
- reject `capacity * 2` overflow;
- reject allocation-size multiplication overflow;
- preserve the original collection when growth fails;
- use one shared helper or one documented invariant across collections;
- add boundary regressions through test-only hooks where necessary.

### P0 — WindowId exhaustion

`WindowId` is a signed integer and `-1` is reserved as invalid. Incrementing the
next ID indefinitely must never reach signed-overflow undefined behavior or
issue a duplicate live ID.

Required changes:

- define IDs as monotonic, reusable, or generation-based;
- fail window creation deterministically when no valid ID is available;
- leave window count, focus, z-order, and ownership unchanged on failure;
- add an exhaustion regression using a narrow internal test hook.

### P0 — Exclusive private Notepad recovery

Emergency recovery is valuable, but a check-then-open sequence is not an atomic
creation contract.

Required changes:

- use exclusive-create semantics;
- use owner-only permissions on POSIX;
- never replace an existing recovery file;
- remove partial files after write or close failure;
- add collision, failure, and successful-recovery tests;
- move recovery policy behind a small service or adapter before it grows.

### P1 — Desktop coordination concentration

`desktop.c` currently owns or coordinates runtime creation, the running-app
table, Launcher, taskbar, global command routing, shutdown transactions,
cleanup, diagnostics, clock updates, and frame scheduling.

Recommended behavior-preserving extraction:

- `desktop_runtime.c` — create, run, frame scheduling, final ownership cleanup;
- `desktop_apps.c` — launch, running-app table, lookup, cleanup;
- `desktop_shutdown.c` — transactional close negotiation;
- `desktop_commands.c` — global input routing;
- `desktop_taskbar.c` — snapshot, hit testing, launch/focus/cycle.

Constraints:

- retain one event loop;
- retain one frame flush;
- introduce no nested modal loops;
- keep the complete `Desktop` structure private;
- make the first extraction PR behavior-neutral.

### P1 — Canonical production libraries in CMake

Tests currently recompose production sources manually. This can allow a test
target to compile a different source set or definition set from the product.

Recommended direction:

```cmake
add_library(retrodesk_runtime STATIC ...)
add_library(retrodesk_platform STATIC ...)
add_library(retrodesk_apps STATIC ...)

add_executable(retrodesk src/main.c)
target_link_libraries(retrodesk PRIVATE
    retrodesk_runtime retrodesk_platform retrodesk_apps)
```

Requirements:

- one canonical production source definition per platform profile;
- tests link production libraries plus deliberate stubs only;
- strict warnings and sanitizers apply to every production library;
- DJGPP manifest validation remains meaningful;
- Debug and Release test manifests remain identical.

### P1 — Storage concurrency contract

Linux `renameat2(..., RENAME_NOREPLACE)` provides a strong no-replace primitive.
Fallback paths that check a destination and then call `rename()` retain a TOCTOU
window. Existing optimistic version checks are useful but do not form a fully
linearizable compare-and-swap between hostile concurrent writers.

Required decision:

- document fallback conflict detection as best effort; or
- implement a stronger strategy for every claimed platform.

Required evidence:

- exact create, rename, and overwrite-save guarantees in `docs/STORAGE.md`;
- race tests where the platform can provide deterministic setup;
- no claim stronger than the active adapter can prove.

### P1 — POSIX signal contract

The current handler follows the important safety rule of only setting a
`sig_atomic_t` flag. The installation and restoration path should move from
`signal()` to `sigaction()`.

Required changes:

- define masks and restart behavior intentionally;
- restore previous actions exactly;
- roll back partial installation;
- test init-failure and restoration paths;
- keep terminal cleanup outside the signal handler.

## Implementation Order

1. Checked collection growth and WindowId exhaustion.
2. Exclusive private Notepad recovery.
3. `sigaction()` installation and restoration.
4. Storage concurrency contract and race coverage.
5. CMake internal-library migration.
6. Behavior-preserving Desktop decomposition.
7. Exact release-candidate evidence.

Each slice should be independently reviewable and must include tests for every
changed invariant.

## Validation Required Per Slice

```bash
make clean
make strict
make test-all
make test-sanitize
make smoke-ci
python3 scripts/check_test_oracles.py
python3 scripts/check_djgpp_sources.py
```

Interactive input, rendering, or terminal-state changes additionally require:

```bash
make smoke
make smoke-linux-vc
```

Windows-affecting changes require Debug and Release builds/tests with the pinned
PDCurses/vcpkg profile before merge.

## Core Freeze Criteria

The core can be treated as a stable development foundation when:

- all dynamic collections have checked growth;
- identifiers have defined exhaustion behavior;
- normal, failed-init, close, and forced-shutdown ownership is deterministic;
- recovery and storage guarantees are exact and test-backed;
- terminal and signal state is restored reliably;
- tests link canonical production units;
- Desktop coordination is split without hidden loops or owners;
- one immutable candidate passes the complete automated and interactive matrix;
- no known critical data-loss, lifecycle, undefined-behavior, or structural
  defect remains.
