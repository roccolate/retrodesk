# Technical Debt Policy

## Purpose

Technical debt is acceptable only when it is explicit, bounded, owned, and has a
removal trigger. The project must not normalize structural shortcuts simply
because they helped deliver one small validated slice.

The detailed operational issue register is
[KNOWN_ISSUES.md](KNOWN_ISSUES.md). This document defines the acceptance rules and
summarizes active structural debt.

## Non-Negotiable Architecture Rules

Debt may not violate:

- one Desktop event loop;
- one frame-flush owner;
- no backend-native types in public/domain headers;
- explicit resource ownership and rollback;
- capability/adaptor-based platform behavior;
- `retro_fs` ownership of file-app filesystem operations;
- normalized input routing;
- no nested modal/app loops;
- deterministic close/shutdown behavior;
- honest platform and release claims.

A change that violates these rules is a defect, not acceptable temporary debt.

## Acceptable Temporary Debt

Examples:

- small compatibility adapters while an interface is migrated;
- bounded header-only presentation contracts;
- platform-specific branches required by genuine OS semantics;
- deliberately simple algorithms proven sufficient for current scale;
- reduced platform guarantees that are documented and tested;
- temporary duplicated projection logic with a concrete extraction plan.

## Required Debt Record

Every accepted item must state:

- identifier;
- owner area;
- reason it exists;
- bounded impact;
- removal trigger;
- target milestone or decision point;
- tests/evidence preventing accidental expansion.

If the removal trigger is met, removal becomes mandatory or the debt record must
be formally re-approved with a new rationale.

## Unacceptable Debt

- hidden process-global Desktop ownership;
- an independent event/render loop inside an app or widget;
- multiple unrelated renderer flush paths;
- duplicate canonical production source lists without parity enforcement;
- app/UI calls to native filesystem APIs;
- silent partial operations or platform “maybe works” paths;
- tests that disappear under `NDEBUG`;
- stale PR behavior described as integrated product state;
- undocumented ownership transfer;
- unbounded service callbacks;
- data-destructive workflows without recovery/partial-failure policy;
- permanent macro remapping as a substitute for module interfaces.

## Review Checklist

For every change:

1. Which object owns new state?
2. Is ownership per Desktop/WM/app instance or accidentally process-global?
3. Can initialization fail after partial acquisition, and is rollback tested?
4. Does the change create another poll, loop, render, or flush owner?
5. Does platform-specific behavior stay behind an adapter/capability?
6. Does the public API distinguish policy success, rejection, and failure?
7. Are storage/version/close contracts preserved?
8. Are taskbar/Launcher hit regions derived from rendered geometry?
9. Does the DOS/Windows build still compile portable changes?
10. Are status, known-issue, roadmap, and architecture docs synchronized?

## Structural Metrics

Track over time:

- process-global/static owner-state count;
- translation-unit macro remapping count;
- `desktop.c` line count and responsibility count;
- public headers exposing native types;
- renderer flush call sites;
- event-loop/poll call sites;
- production source-list definitions and parity checks;
- duplicated app-id scans;
- full-frame redraws versus actual visible changes;
- service callbacks, work budget use, and redraw requests;
- stale open PR count and age.

Metrics are signals, not automatic refactoring requirements. They must be
interpreted against ownership and failure risk.

## Active Debt Register

### TD-001 — Desktop-private macro bridge layer

**Owner:** core/ui/wm  
**Reason:** delivered taskbar, Launcher, maximize, and operation-mode slices
without prematurely widening public APIs.  
**Impact:** macro-sensitive compilation and hidden coupling through
`statusbar.h`.  
**Removal trigger:** explicit Desktop-owned chrome/window-command interfaces
exist.  
**Target:** before beta contract freeze.  
**Protection:** Linux/Windows/DOS builds and WM/widget regressions.

### TD-002 — Bridge-local static state

**Owner:** core/ui/wm  
**Reason:** bounded implementation of Launcher selection, taskbar action state,
maximize geometry catalog, and operation-mode HUD state.  
**Impact:** multiple Desktop instances/future embedding may not have fully isolated
state; maximize catalog is bounded.  
**Removal trigger:** Desktop decomposition and WM-owned maximize state.  
**Target:** before beta contract freeze.  
**Protection:** current tests and single ordinary Desktop process assumption.

### TD-003 — `desktop.c` responsibility concentration

**Owner:** core  
**Reason:** incremental foundation accumulated orchestration in one file.  
**Impact:** app/service/shutdown/chrome/window/render changes interact in one
translation unit.  
**Removal trigger:** app, chrome, window-command, shutdown, diagnostics, and
service controllers have explicit contracts.  
**Target:** structural phase after `v0.5.0` behavior completion.

### TD-004 — Ambiguous `app_launch` result

**Owner:** app/core  
**Reason:** historical pointer-return API uses `NULL` for both failure and
already-running single-instance focus.  
**Impact:** callers cannot distinguish policy success from failure.  
**Removal trigger:** explicit launch-result API and regressions.  
**Target:** core hardening before broader app/plugin expansion.

### TD-005 — Diagnostics/capability authority overlap

**Owner:** core/platform  
**Reason:** diagnostics refresh grew alongside capability state.  
**Impact:** risk of duplicated or mutated authority.  
**Removal trigger:** diagnostics becomes a read-only projection.  
**Target:** next core hardening sequence.

### TD-006 — Broad redraw policy

**Owner:** core/render/ui  
**Reason:** correctness-first dirty marking after event dispatch.  
**Impact:** unnecessary work and possible flicker on slow terminal paths.  
**Removal trigger:** measured selective redraw and status/taskbar no-op cache.  
**Target:** remaining `v0.5.0`/structural phase.  
**Constraint:** one flush remains mandatory.

### TD-007 — Simple fixed-budget app-service scheduler

**Owner:** core/app  
**Reason:** no real async workload yet requires a more complex scheduler.  
**Impact:** O(N), equal 8 KiB budget, global 16 ms poll cap.  
**Removal trigger:** measured PTY/network/subprocess workload demonstrates
fairness/backpressure need.  
**Target:** terminal/async app milestone.  
**Decision:** accepted; do not optimize speculatively.

### TD-008 — Fixed taskbar catalog and no overflow

**Owner:** core/ui  
**Reason:** current product has three pinned apps.  
**Impact:** future larger catalog can become unreachable on narrow terminals.  
**Removal trigger:** catalog expansion or `v0.5.0` exit.  
**Target:** remaining `v0.5.0`.  
**Protection:** current 80/40/20-column tests.

### TD-009 — Collection/ID hardening lives only in stale PR #27

**Owner:** core/wm  
**Reason:** validated branch was not integrated before later `main` changes.  
**Impact:** checked growth and deterministic extreme ID behavior are absent from
current source.  
**Removal trigger:** rebuild and merge on current `main`, then close stale PR.  
**Target:** immediate P0/P1 sequence.

### TD-010 — Responsive Notepad chrome lives only in stale PR #24

**Owner:** apps/ui/core  
**Reason:** branch remains stacked on superseded hardening history.  
**Impact:** useful menus/responsive status cannot be maintained as product code;
documentation must exclude them.  
**Removal trigger:** rebuild or explicitly close/defer.  
**Target:** after #27 decision and Desktop contract reconciliation.

### TD-011 — Built-in registration failure propagation

**Owner:** app/core  
**Reason:** built-in setup assumes expected success more strongly than other init
paths.  
**Impact:** potential incomplete catalog after rare duplicate/allocation failure.  
**Removal trigger:** registration reports failure and Desktop rollback is tested.  
**Target:** next core hardening sequence.

## Resolved Debt

Removed from the active register because it is integrated:

- POSIX-only storage limitation: Win32 and DJGPP adapters now exist;
- missing dirty-close decision: Notepad has Save/Discard/Cancel;
- half-shutdown risk: global shutdown is transactional;
- per-Notepad clipboard ownership: clipboard is Desktop-owned;
- platform-native public storage metadata: token is adapter-neutral;
- missing sanitizer/startup smoke gates;
- diagnostic-looking taskbar/Launcher;
- missing minimize/restore;
- missing maximize/unmaximize;
- invisible keyboard move/resize mode.

Resolved items may still have platform/evidence limitations, which belong in
`KNOWN_ISSUES.md`, not as the old structural debt statement.

## Debt Review Cadence

Review this register:

- before opening a release candidate;
- after a milestone's major behavior lands;
- before adding a plugin ABI or large app catalog;
- when a temporary bridge gains another responsibility;
- when an open PR becomes stale relative to multiple merged features.

A release cannot proceed with an undocumented debt item that violates
[FOUNDATION_PRINCIPLES.md](FOUNDATION_PRINCIPLES.md).