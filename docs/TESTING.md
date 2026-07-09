# Testing Strategy

## Layered Strategy

- Unit tests for pure runtime logic (`core`, `wm`, app registry behavior).
- Widget tests for reusable UI components.
- Integration tests for backend event translation and render contracts.
- Desktop runtime tests for app launch/close and lifecycle behavior.
- Optional shared contract tests against `retrocore-spec` fixtures.
- Smoke tests per target platform profile.

## Current Test Suite

Registered by `CMakeLists.txt` when `ENABLE_TESTS=ON` and `BUILD_TESTING` is
true:

- `cli_parse_test`
- `key_chord_test`
- `app_registry_test`
- `platform_features_test`
- `wm_event_replay_test`
- `theme_catalog_test`
- `draw_list_contract_test`
- `statusbar_drawlist_test`
- `ansi_renderer_diff_test`
- `tty_decoder_test`
- `text_input_test`
- `scroll_list_test`
- `text_buffer_test`
- `button_test`
- `dialog_test`
- `menu_bar_test`
- `progress_bar_test`
- `tab_test`
- `desktop_runtime_test`

Optional test:

- `retrocore_event_fixture_test` is enabled only when
  `${RETROCORE_SPEC_DIR}/fixtures/events/open-files-and-focus.json` exists.

## Mandatory Scenarios

- Desktop create/run/shutdown lifecycle.
- Init rollback on partial failure.
- Focus and z-order transitions.
- Drag on supported capabilities and fallback when unsupported.
- Resize handling where available.
- Capability-based app launch rejection.
- App close lifecycle releases window ownership cleanly.
- Single-flush-per-tick enforcement.
- CLI/backend flag combination validation.
- Backend-neutral key vocabulary behavior.
- Widget event/render behavior without requiring `initscr`.
- Shared fixture compatibility when `retrocore-spec` is available.

## Platform Smoke Matrix

- Linux (Tier 1): keyboard baseline always; mouse as capability.
- Windows (Tier 1): keyboard + PDCurses mouse/resize where available.
- macOS (Tier 2): compile and keyboard baseline.
- DOS (Tier 2): compile and reduced feature baseline.
- Interactive smoke gate: `make smoke` (PTY required).
- Interactive Linux VC gate: `make smoke-linux-vc` (PTY required, expects
  `linux_tty_keyboard_only: true` under `TERM=linux`).
- Non-interactive fallback smoke: `make smoke-ci`.

## Local Validation Commands

```bash
make clean
make strict
make test
make check-build-sources
make smoke-ci
```

Interactive terminal validation:

```bash
make smoke
make smoke-linux-vc
```

## Regression Rules

- Any interface-level change must update related docs.
- Any source layout/build target change must update `README.md`,
  `docs/INDEX.md`, and `docs/BUILD_SYSTEM.md` when user-facing or
  architecture-facing behavior changes.
- Any bug fix around input/focus/resize must add a reproducible scenario.
- CI strict job must remain warning-clean.
- `wm` focus/z-order/drag/close behavior must be covered by event replay tests.
- TTY raw decoder changes must be covered by parser unit tests.
- Widget behavior changes must add or update widget-level tests.

## Release Gate Reference

`v0.1.0` test sign-off follows
[RELEASE_0.1_CHECKLIST.md](RELEASE_0.1_CHECKLIST.md).
