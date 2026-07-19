# Testing Strategy

## Layered Strategy

- Unit tests for pure runtime logic (`core`, `wm`, app registry, widgets,
  storage helpers).
- Integration tests for backend event translation, render contracts, app
  lifecycle, and desktop runtime behavior.
- Fixture tests for shared retrocore behavior when `RETROCORE_SPEC_DIR` is
  available or required.
- Smoke tests per claimed platform profile.

## Test Oracle Policy

Tests must use always-active oracles from `tests/test_harness.h`.

- Do not include `<assert.h>` in tests.
- Do not use `assert(...)`; Release builds may compile it out.
- Run `python3 scripts/check_test_oracles.py` before release evidence is
  collected.

## Mandatory Scenarios

- desktop create/run/shutdown lifecycle.
- init rollback on partial failure.
- focus and z-order transitions.
- drag on supported capabilities and fallback when unsupported.
- resize handling where available.
- capability-based app launch rejection.
- app close lifecycle releases window ownership cleanly.
- dirty app close rejection without nested loops.
- single-flush-per-tick enforcement.
- CLI/backend flag combination validation.
- POSIX storage list/read/write/conflict behavior while it is the active storage
  adapter.
- File Manager and Notepad open/save flows for the Linux preview.

## Platform Smoke Matrix

- Linux active profile: keyboard baseline always; mouse as capability.
- Windows planned Tier 1: PDCurses + storage adapter/gating required before any
  release claim.
- macOS experimental: compile and keyboard baseline only when validated.
- DOS experimental: compile/reduced feature baseline only when validated.
- Interactive smoke gate: `make smoke` (PTY required).
- Interactive Linux VC gate: `make smoke-linux-vc` (PTY required, expects
  `linux_tty_keyboard_only: true` under `TERM=linux`).
- Non-interactive fallback smoke: `make smoke-ci`.

## Local Commands

```bash
make check-test-oracles
make test
make test-all
make smoke-ci
```

`make test` configures with CMake and requires curses development headers.
Remove stale build directories before collecting evidence if they were created
from another absolute checkout path.

## Regression Rules

- Any interface-level change must update related docs.
- Any bug fix around input/focus/resize/storage must add a reproducible
  scenario.
- CI strict jobs must remain warning-clean.
- `wm` focus/z-order/drag/close behavior must be covered by event replay tests.
- tty raw decoder changes must be covered by parser unit tests.
- app-runtime changes must cover lifecycle, launch rejection, and close cleanup.

## Release Gate Reference

`v0.1.0` test sign-off follows
[RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md).
