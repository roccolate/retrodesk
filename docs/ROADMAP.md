# Roadmap

## Phase 0: Documentation Foundation

- Establish docs and ADRs as architecture source of truth.
- Simplify `README.md` to index entrypoint.

Exit criteria: all mandatory docs exist and are internally consistent.

## Phase 1: Canonical Build

- CMake as source of truth.
- Make wrapper and DOS-specialized makefile alignment.
- Remove in-repo toolchain assumptions.

Exit criteria: Tier 1 modern builds use same target sources.

## Phase 2: Runtime Layering

- Introduce `core`, `wm`, `app`, `render`, `platform` contracts.
- Remove backend types from domain/public headers.

Exit criteria: normalized event/render interfaces are in place.

## Phase 3: State Ownership Cleanup

- Replace global runtime state with `Desktop` ownership.
- Ensure deterministic cleanup and rollback.

Exit criteria: no global owner state in runtime logic.

## Phase 4: Event Loop Unification

- Eliminate nested modal loops.
- Keep one input poll + one frame flush point.

Exit criteria: no widget/app autonomous blocking input loop.

## Phase 5: Multi-Window WM Hardening

- Focus, z-order, drag, and close lifecycle in `wm`.

Exit criteria: multi-window interaction behaves deterministically.

## Phase 6: App Runtime Formalization

- Registry, launch rules, capability checks, app lifecycle hooks.

Exit criteria: at least two apps run through formal runtime contracts.

## Phase 7: Portability Hardening

- Capability-driven fallback across all tiers.
- Linux virtual console keyboard-first behavior proven.

Exit criteria: documented behavior profile per platform.

## Phase 8: Feature Porting from RetroTUI

- Port selected subsystems using new contracts.

Exit criteria: feature growth without architectural regressions.

## v0.1.0 Release Gate

`v0.1.0` is approved only when
[RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md) is fully satisfied.
