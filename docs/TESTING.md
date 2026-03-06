# Testing Strategy

## Layered Strategy

- Unit tests for pure runtime logic (`core`, `wm`, app registry behavior).
- Integration tests for backend event translation and render contracts.
- Smoke tests per target platform profile.

## Mandatory Scenarios

- desktop create/run/shutdown lifecycle.
- init rollback on partial failure.
- focus and z-order transitions.
- drag on supported capabilities and fallback when unsupported.
- resize handling where available.
- capability-based app launch rejection.
- single-flush-per-tick enforcement.

## Platform Smoke Matrix

- Linux (Tier 1): keyboard baseline always; mouse as capability.
- Windows (Tier 1): keyboard + PDCurses mouse/resize where available.
- macOS (Tier 2): compile and keyboard baseline.
- DOS (Tier 2): compile and reduced feature baseline.

## Regression Rules

- Any interface-level change must update related docs.
- Any bug fix around input/focus/resize must add a reproducible scenario.
- CI strict job must remain warning-clean.

## Release Gate Reference

`v0.1.0` test sign-off follows
[RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md).
