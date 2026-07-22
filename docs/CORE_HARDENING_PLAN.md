# Core Hardening Status and Plan

> Current audit snapshot: 2026-07-22. This document distinguishes integrated
> hardening from work proposed in stale branches.

## Objective

Keep the Desktop/runtime core predictable under failure, explicit about
ownership, portable across active profiles, and small enough to reason about.
Every hardening slice must preserve the one-event-loop and one-flush contracts and
must pass Debug, Release, strict warning, sanitizer, Windows, and DOS gates where
the changed code is portable.

## Current Assessment

The core is structurally sound enough to continue; it does not require a rewrite.
The strongest foundations are:

- explicit Desktop lifecycle;
- normalized events;
- backend-neutral rendering;
- descriptor-based apps;
- rollback-aware create failure;
- transactional close/shutdown;
- Desktop-owned clipboard;
- adapter-neutral storage;
- budgeted app services;
- test-backed focus/window behavior.

The highest remaining risks are not missing architecture layers. They are:

- stale collection/identifier hardening not integrated;
- concentrated Desktop ownership and macro bridges;
- ambiguous APIs and duplicated authority;
- broader-than-needed redraw;
- incomplete failure propagation for built-in registration;
- future async workload policy not yet measured.

## Integrated Hardening

### App create-failure rollback

Once a descriptor's `create` callback begins, a failure invokes matching
`destroy`, closes the created window, releases the resource-path copy, and leaves
no running-app slot.

Coverage: test-only failing descriptor checks create/destroy counts and stable
app/window totals.

### CLI result disambiguation

CLI parsing distinguishes:

- valid configuration;
- help/usage shown;
- parse error.

`main()` exits successfully for help and fails for invalid arguments/backend
combinations.

### Portable function-key vocabulary

The currently claimed portable vocabulary is F1..F12. Higher function keys and
raw-TTY mappings are not claimed without consistent backend support/tests.

### Sign-safe input byte

`RetroKeyEvent.ascii` is unsigned so bytes `0x80..0xFF` are not corrupted by
platform `char` signedness. Portable printability/codepoint behavior remains
separate.

### Transactional global shutdown

A global close does not destroy early-authorized apps while a later dirty app is
still deciding. Cancellation restores the entire runtime to ordinary operation.

### Desktop-owned clipboard

Clipboard ownership is no longer hidden inside Notepad instances. Separate
Desktop objects have isolated clipboard services.

### App service boundary

Optional callbacks run inside the one Desktop loop with a bounded 8192-unit
budget and 16 ms active-service poll cap. They cannot render, flush, poll platform
input, or create a nested UI loop.

### Native storage boundaries

POSIX, Win32, and DJGPP adapters isolate OS-native filesystem details and keep
fixed-width public metadata. Platform-specific storage failures and stale-version
conflicts are tested.

### Window lifecycle expansion

Minimize/restore, maximize/unmaximize, and move/resize feedback preserve app
instance ownership, close/shutdown behavior, and single-flush rendering.

## Priority 0/1 — Rebuild Collection and ID Hardening

### Current state

Open PR #27 proposes this work but is based on older `main`. Its historically
green branch is not integrated and should not be merged without reconstruction.

### Required behavior

#### Checked capacity growth

Desktop running-app and Window Manager window arrays must:

- reject element-count overflow;
- check `SIZE_MAX / element_size` before byte multiplication;
- use deterministic geometric growth;
- preserve pointer, count, capacity, active window, and existing entries when
  growth/allocation fails;
- allow a later successful retry.

#### `WindowId` exhaustion

- IDs remain positive and monotonic.
- Live IDs are never reused.
- `INT_MAX` may be issued once as the last valid ID.
- Later window creation returns `WINDOW_ID_INVALID` without signed overflow.
- Failed creation does not consume an ID unless the contract explicitly decides
  otherwise; the rebuilt test must freeze the chosen behavior.

### Required tests

- pure checked-capacity arithmetic boundaries;
- injected WM 8->16 growth failure, state preservation, retry;
- injected Desktop running-app growth failure, rollback, retry;
- `WindowId` boundary at `INT_MAX`;
- full existing minimize/maximize/bridge tests on the rebuilt current branch;
- all Linux/Windows/DOS matrices.

### Completion condition

A fresh PR directly based on current `main` is merged. The old #27 is then closed
as superseded or updated to the replacement branch.

## Priority 1 — Desktop Decomposition

### Problem

`desktop.c` currently coordinates:

- service scheduling;
- registry and launch;
- app/window association;
- Launcher;
- taskbar;
- maximize/window operations;
- shutdown negotiation;
- diagnostics/status refresh;
- event routing;
- rendering.

Recent UI work uses private header-only bridges and macro remapping from
`statusbar.h` to avoid premature public API expansion. This was effective for
small validated slices but should not become permanent architecture.

### Target decomposition

Create explicit per-Desktop components/interfaces for:

1. **App controller** — lookup, launch result, running table, close cleanup.
2. **Desktop chrome controller** — taskbar snapshot/actions and Launcher state.
3. **Window command controller** — minimize/maximize/move/resize state and input.
4. **Shutdown coordinator** — transaction state machine.
5. **Status/diagnostics projection** — read-only view of platform/runtime state.
6. **Service scheduler** — current simple policy isolated behind one contract.

### Removal goals

- remove `wm_*`/`statusbar_*` macro remapping from UI headers;
- remove bridge-local static owner state;
- move maximize state into a WM/Desktop-owned structure;
- make multiple Desktop instances independent by construction;
- preserve ordinary widget tests without special link stubs.

### Constraints

- no new global owner;
- no second event loop;
- no independent render/flush owner;
- no backend types in core/controller headers;
- no behavior change without regression coverage.

## Priority 1 — API Disambiguation

### `app_launch`

Current shape conflates failure and “existing single-instance app focused.”

Target concept:

```c
typedef enum RetroAppLaunchResult {
    RETRO_APP_LAUNCHED = 0,
    RETRO_APP_ALREADY_RUNNING,
    RETRO_APP_REJECTED,
    RETRO_APP_FAILED,
} RetroAppLaunchResult;
```

The final API may expose an optional instance/window output, but callers must be
able to distinguish policy success from failure.

Coverage:

- launched new instance;
- focused existing single instance;
- multiple-instance launch;
- unknown descriptor;
- capability rejection;
- window/create/running-table failure.

### App lookup

Replace repeated `app_id` scans with one helper returning the running slot/index
set required by launch, taskbar snapshot, cleanup, and test access.

## Priority 1 — Capability and Diagnostics Authority

`PlatformFeatures` must remain the source of truth for backend capabilities.
Desktop diagnostics should be a read-only projection plus runtime-only values
such as selected backend name and current drag degradation.

Required work:

- stop diagnostic refresh from mutating authoritative capability state;
- distinguish configured, reported, and degraded runtime values;
- keep app launch gating tied to authoritative capability mask;
- preserve current user-visible diagnostics through tests.

## Priority 1 — Built-In Registration Failure

`apps_register_builtin()` should report failure. `desktop_create()` should treat an
incomplete catalog as an initialization failure and rollback like other owned
resources.

Required tests:

- duplicate or injected registry allocation failure;
- no partially usable Desktop returned;
- all previously created services destroyed;
- normal registration remains deterministic.

## Priority 2 — Selective Redraw

### Current issue

Desktop frequently marks the frame dirty after dispatch even when no visible state
changed. Taskbar/status updates may rebuild equivalent snapshots/text.

### Target

- callbacks/actions report visual change explicitly where practical;
- cache the last taskbar/status snapshot;
- skip no-op statusbar setters;
- avoid rendering on service idle/no-op events;
- preserve animation/service responsiveness;
- preserve one authoritative frame render and flush.

### Metrics

Instrument before/after:

- poll iterations;
- dirty frames;
- flush count;
- ANSI bytes/cells emitted;
- taskbar snapshot changes;
- service redraw requests.

Do not create separate taskbar/window flush paths.

## Priority 2 — Service Scheduler Readiness

The current O(N) equal-budget scheduler is acceptable for the current app count.
Before changing it, introduce a real workload and measure:

- service count;
- work consumed per tick;
- redraw rate;
- backlog/latency;
- poll wakeups;
- shutdown/close interaction.

Possible future contracts:

- app-declared pending work;
- adaptive finite budgets;
- fairness/round-robin cursor;
- backpressure;
- service-specific close/drain policy.

No worker-owned UI state or direct backend render is permitted.

## Priority 2 — Launcher/Taskbar Constants and Catalog

- replace remaining geometry/action magic values with named contracts;
- define catalog ownership independent of hardcoded three-app array;
- add count/sentinel helpers;
- implement congestion/overflow policy;
- ensure all actions remain reachable by keyboard;
- keep hit regions derived from exact render widths.

## Open PR Handling

### PR #27

Rebuild, validate, merge, then close/supersede the stale branch.

### PR #24

Do not merge directly. It depends on superseded hardening history and modifies
Desktop/app context in areas changed since. Rebuild after #27 decision and Desktop
contract reconciliation.

## Verification Matrix for Every Core Slice

```bash
make clean
make check-test-oracles
make strict
make test
make test-all
make test-sanitize
make smoke-ci
```

Also required through official workflows:

- Linux static analysis;
- Windows Debug and Release `/W4 /WX`;
- native Win32 storage tests when affected;
- DJGPP source guard and product build;
- native `FSTEST.EXE` and DOSBox-X diagnostic;
- manual PTY smoke for public input/render behavior changes.

## Completion Checklist

### Integrated

- [x] create-failure destroy rollback;
- [x] CLI result states;
- [x] F1..F12 portable vocabulary;
- [x] unsigned input byte;
- [x] transactional shutdown;
- [x] Desktop clipboard ownership;
- [x] app service boundary;
- [x] native POSIX/Win32/DJGPP storage isolation;
- [x] minimize/maximize/window-operation lifecycle preservation.

### Pending

- [ ] rebuild checked capacity growth and `WindowId` exhaustion;
- [ ] disambiguate `app_launch`;
- [ ] isolate app lookup/controller;
- [ ] make diagnostics a read-only capability projection;
- [ ] propagate built-in registration failure;
- [ ] decompose Desktop chrome/window command integration;
- [ ] remove macro/private global bridge ownership;
- [ ] selective redraw/status snapshot cache;
- [ ] measured service scheduling improvements if a real workload requires them;
- [ ] full strict/sanitizer/Windows/DOS validation on each resulting slice.

## Definition of Done

A hardening item is complete only when:

- source is merged into `main`;
- public/ownership behavior is explicit;
- old behavior fails a regression or boundary test where applicable;
- failure leaves state deterministic;
- all active build profiles pass;
- status, architecture, known-issue, and debt documents are updated;
- obsolete PRs/comments are closed or marked superseded.