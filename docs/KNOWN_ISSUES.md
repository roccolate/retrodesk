# Known Issues and Active Risks

> Snapshot: 2026-07-22  
> Applies to `main` at `31273f8c888b6074f416a938847e3124b88f9464`.

This register distinguishes defects and structural risks from ordinary future
features. An item stays here until it is fixed, explicitly accepted with a
removal trigger, or moved to a release-specific known-issues record.

## Severity Model

- **Release blocker** — prevents an honest release claim for the affected scope.
- **High** — correctness, lifecycle, portability, or maintainability risk that
  should be resolved before broad feature expansion.
- **Medium** — bounded limitation with a deterministic workaround.
- **Low** — polish or evidence gap that does not currently threaten data or
  ownership safety.

## Release Blockers

### KB-001 — No exact release-candidate evidence bundle

**Area:** release engineering  
**Status:** open  
**Impact:** no version tag should be called a validated release.

Development PR heads have repeatedly passed Linux, Windows, sanitizer, and DOS
matrices. The latest desktop slice was squash-merged, so its strongest workflow
evidence is attached to the pre-merge PR head rather than the final `main` commit.

**Required resolution:** choose one exact candidate commit and record clean-tree,
toolchain, Debug/Release, sanitizer, manifest, platform smoke, and manual app
acceptance evidence against that SHA.

### KB-002 — Manual terminal/platform matrix is incomplete

**Area:** input, rendering, portability  
**Status:** open

Headless tests cannot prove visual quality, terminal escape behavior, mouse
semantics, console restoration after unusual exits, or all PDCurses/DOSBox-X
interaction details.

**Required resolution:** record curses and ANSI/raw-TTY smoke across representative
Linux terminals and `TERM=linux`; record native Windows/PDCurses file workflows;
record reduced DOS interaction and exclusions; either validate macOS or leave it
unclaimed.

### KB-003 — Documentation and release claims must remain synchronized

**Area:** project governance  
**Status:** addressed by the current documentation PR, continuous thereafter

Several documents previously described POSIX-only storage and missing
minimize/maximize behavior after those features had already landed.

**Required resolution:** `PROJECT_STATUS.md` is the status source. Public behavior,
platform claims, release checklists, and roadmap completion must be updated in the
same PR as material state changes.

## High-Priority Engineering Risks

### KB-010 — Collection growth and `WindowId` exhaustion hardening is not in `main`

**Area:** core, Window Manager  
**Status:** open PR #27, stale base

Current dynamic tables use ordinary geometric growth. PR #27 proposes checked
capacity arithmetic, injected allocation-failure recovery tests, and deterministic
ID exhaustion at `INT_MAX`, but it is based on an older `main` and cannot be
counted as integrated.

**Risk:** theoretical size overflow and poorly specified behavior at extreme
identifier/capacity boundaries.

**Required resolution:** rebuild the hardening slice on current `main`, preserve
all current desktop features, rerun the complete matrix, then merge or document a
bounded deferral.

### KB-011 — Desktop integration is too concentrated

**Area:** `src/core/desktop.c`, UI/WM integration  
**Status:** open

Desktop currently coordinates app services, registry/lifecycle, shutdown,
Launcher, taskbar, window commands, diagnostics, status updates, and rendering.
Recent desktop polish avoided widening public APIs by using private header-only
bridges and macro remapping from `statusbar.h`.

**Risk:** hidden coupling, difficult local reasoning, macro-sensitive tests, and
higher regression probability when adding desktop surfaces.

**Required resolution:** extract explicit Desktop-owned controllers/services for
Launcher/taskbar/window commands. Remove translation-unit macro remapping once
real interfaces exist.

### KB-012 — Temporary bridge state is process-global within translation units

**Area:** move/resize  
**Status:** partially resolved; one remaining temporary bridge

Maximize state is owned by each `RetroWindow`; taskbar activation and Launcher
state are explicitly Desktop-owned. The remaining example is window-mode HUD
state in the operation-mode bridge.

**Risk:** multiple Desktop instances or future embedding could still observe
operation-mode state not owned by the correct Desktop object.

**Removal trigger:** Desktop decomposition provides explicit per-instance state
ownership in core/WM objects.

**Target:** before beta contract freeze.

### KB-013 — `app_launch` return value is ambiguous

**Area:** app runtime API  
**Status:** open

`app_launch()` may return `NULL` for both launch failure and the successful policy
case where a single-instance app was already running and was focused instead.

**Risk:** callers cannot distinguish failure from policy success without extra
state inspection.

**Required resolution:** return an explicit launch result and optional instance or
window output; cover launched, already-running, missing, rejected, and failed
cases.

### KB-014 — Diagnostics and capability ownership overlap

**Area:** core/platform  
**Status:** open

`PlatformFeatures` should be the authority for capabilities. Desktop diagnostics
still refreshes fields and may mutate derived capability state.

**Risk:** duplicated authority and future disagreement between launch gating,
user-visible diagnostics, and backend reality.

**Required resolution:** make diagnostics a read-only projection and keep
capability mutation in platform/initialization ownership.

### KB-015 — Built-in app registration failures are not a surfaced startup error

**Area:** app registry/startup  
**Status:** open

The built-in registration sequence is treated as expected-to-succeed setup.
Failure handling is less explicit than other startup ownership paths.

**Risk:** a duplicate or allocation failure could silently leave an incomplete app
catalog.

**Required resolution:** propagate registration failure through `desktop_create`,
rollback deterministically, and add a startup regression.

## Medium-Priority Functional Limits

### KB-020 — Taskbar has no overflow model

**Area:** taskbar  
**Status:** open; remaining `v0.5.0` work

The current fixed three-app catalog fits tested widths and switches between full
and compact labels. It has no deterministic overflow button/menu for a larger
catalog or unusually constrained width.

**Required resolution:** define priority rules that keep focused/running apps
reachable, prevent overlap with the clock, and expose hidden entries through a
keyboard- and mouse-accessible overflow surface.

### KB-021 — Redraw is broader than necessary

**Area:** performance/rendering  
**Status:** open

Desktop often marks the frame dirty after event dispatch, even when an event does
not change visible state. Taskbar/status updates also lack a complete no-op cache.

**Impact:** avoidable work and potential ANSI/curses flicker on slower terminals.

**Constraint:** optimization must preserve the single authoritative frame flush
and must not create independently rendered regions.

### KB-022 — App service scheduling is simple O(N)

**Area:** asynchronous app services  
**Status:** accepted current design

Every active service receives the same fixed 8 KiB budget each Desktop iteration,
and any service caps the platform wait to 16 ms.

**Impact:** sufficient for the current app count, but future PTY/network workloads
may require fairness, readiness hints, backpressure, and instrumentation.

**Removal trigger:** first real asynchronous app demonstrates measured need. Do
not optimize speculatively before that workload exists.

### KB-023 — Native responsive Notepad chrome is not integrated

**Area:** Notepad/UI  
**Status:** open draft PR #24 on stale history

File/Edit/View menus, `F11`, `Ctrl+N`, `Ctrl+O`, and responsive status chrome exist
only in the historical branch.

**Required resolution:** rebuild on the current desktop/WM integration and rerun
all platform matrices. Do not merge the stale stack directly.

### KB-024 — No real terminal emulator or PTY

**Area:** Diagnostics/Terminal  
**Status:** planned

The app named Diagnostics is a runtime information view. It does not spawn a
shell, parse terminal output, maintain a terminal screen buffer, or own a PTY.

The app-service contract is only the scheduling foundation. A terminal port still
needs pure screen-buffer and ANSI/CSI parser contracts before subprocess/PTY
integration.

### KB-025 — File Manager destructive and recursive operations are absent

**Area:** File Manager/storage  
**Status:** planned

Delete/trash, copy, move, recursive traversal, progress, and cancellation are not
implemented. This is an intentional safety boundary, not a hidden missing path.

Before implementation, define overwrite, recovery/trash, cancellation,
partial-failure, symlink/reparse-point, and cross-device policies.

### KB-026 — Notepad editing scope remains bounded

**Area:** Notepad/text  
**Status:** planned

Missing: Replace/Replace All, native system clipboard, mouse placement/drag
selection, wheel scrolling, Unicode normalization policy, syntax highlighting,
multiple encodings, tabs, and a large-file strategy.

These features must preserve UTF-8 boundary, display-cell, history, storage,
close, and portability contracts.

## Platform-Specific Limits

### KB-030 — Windows interactive maturity is below build/test maturity

The Win32 storage adapter and strict Windows tests are integrated. Native
PDCurses interaction and representative file workflows still need current manual
evidence before Windows is called equivalent to the Linux product profile.

### KB-031 — DOS replacement is transactional but not truly atomic

DOS/DJGPP uses same-directory 8.3 temporary files plus backup/restore because no
universal atomic replace primitive exists. A power/process failure at the wrong
moment has different guarantees from POSIX rename or Win32 replacement.

DOS paths/filenames remain ASCII/DOS-oriented while validated UTF-8 file content
is supported.

### KB-032 — macOS is unvalidated

The design expects the POSIX adapter and curses path to be usable, but no current
build/test/interactive evidence is attached. Keep macOS experimental and make no
product claim until evidence exists.

### KB-033 — Unicode terminal width is intentionally incomplete

The core validates UTF-8 and uses display-cell calculations, but terminal width
behavior for all scripts, combining sequences, emoji, variation selectors, and
ambiguous-width characters is not fully specified or manually validated.

Find performs limited case folding and no Unicode normalization. Accent
significance is intentional in the current contract.

## Resolved Items Removed from the Debt Register

The following are integrated and should not be reported as current missing work:

- native Win32 storage;
- native DJGPP storage and DOSBox-X storage smoke;
- Save/Discard/Cancel Notepad close behavior;
- transactional global shutdown;
- Desktop-owned clipboard service;
- adapter-neutral public storage metadata;
- sanitizer and startup-smoke gates;
- taskbar and Launcher redesign;
- theme-specific desktop surface tokens;
- taskbar minimize/restore;
- maximize/unmaximize;
- move/resize visual feedback.

## Maintenance Rule

Each issue must be updated when its status changes. A fix PR should name the
relevant `KB-xxx` item, add regression evidence, and either remove the item or
rewrite it to describe the remaining bounded risk.