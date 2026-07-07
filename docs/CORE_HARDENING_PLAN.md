# Core module hardening plan

Tracking document for the systematic cleanup of `src/core/` identified
during the senior-level review. Every phase is independently executable
and verifiable. Each commit must keep Debug and Release `ctest` at 100%.

## Context

`src/core/` (1033 LoC) was reviewed on 2026-07-06. The review surfaced
20 items: 2 real bugs, 10 smells, 8 minor nits. The bugs were fixed
in the same session (see git log: `bdec1d6`, `b316c49`). The remaining
items are queued here.

The repo's CI defaults to Release, which strips `assert()` via
`-DNDEBUG`. Several test/code drifts only surface in Debug builds. The
verification step at the end of each phase therefore runs both Debug
and Release `ctest`.

## Goals for the core module

1. Public API is unambiguous (no overloaded NULL returns, no stale
   comments, no duplicated fields).
2. `RetroKeyEvent` is sign-safe across platforms (`unsigned char ascii`).
3. `key_chord` covers the keys the target hardware can actually send
   (F1..F24).
4. Every bug in the review has a regression test that fails on the old
   code and passes on the new code.
5. Zero warnings with `-Wall -Wextra -Wconversion -Wsign-conversion -Werror`.
6. Zero leaks and zero undefined behavior under ASan + UBSan.

## Phases

### Phase 1 — Bug-shaped cleanup

**1.1** `app_launch`: missing `desc->destroy` on `desc->create` failure.

File: `src/core/desktop.c:424-428`. The path that fails inside
`desc->create` only frees the instance and closes the WM window; it
does not call `desc->destroy`. The OOM path at line 430-435 does.
Symmetry required.

```c
if (desc->create && !desc->create(instance, &instance->ctx)) {
    if (desc->destroy) desc->destroy(instance);   // <- add
    wm_close_window(desktop->wm, wid);
    free(instance);
    return NULL;
}
```

Test: extend `tests/desktop_runtime_test.c` with a descriptor whose
`create` callback fails. Hook `destroy` to bump a counter. Assert the
counter is 1.

**1.2** CLI: distinguish "showed usage" from "parse error".

Files: `src/core/cli.h`, `src/core/cli.c`, `src/main.c`.

Replace `bool retro_cli_parse(...)` with:

```c
typedef enum {
    RETRO_CLI_OK = 0,
    RETRO_CLI_SHOWED_USAGE,    // --help / -h
    RETRO_CLI_PARSE_ERROR,     // unknown arg or invalid backend combo
} RetroCliParseResult;

RetroCliParseResult retro_cli_parse(int argc, char **argv,
                                    RetroCliOptions *out, FILE *err);
```

Behavior:

* `--help` / `-h` writes usage to **stdout** and returns
  `RETRO_CLI_SHOWED_USAGE`.
* Unknown argument writes usage to **stderr** and returns
  `RETRO_CLI_PARSE_ERROR`.
* Invalid backend combination writes a specific message to **stderr**
  and returns `RETRO_CLI_PARSE_ERROR`.
* Otherwise returns `RETRO_CLI_OK`.

`main.c:13`:

```c
RetroCliParseResult pr = retro_cli_parse(argc, argv, &options, stderr);
if (pr == RETRO_CLI_PARSE_ERROR)  return EXIT_FAILURE;
if (pr == RETRO_CLI_SHOWED_USAGE) return EXIT_SUCCESS;
```

Tests: extend `tests/cli_parse_test.c` with three cases:

* `--help` returns `SHOWED_USAGE`; `stdout` contains "usage:".
* `--bogus-flag` returns `PARSE_ERROR`; `err` contains "usage:".
* `--input-backend=tty --render-backend=curses` returns `PARSE_ERROR`.

**1.3** `key_chord.h`: misleading comment + missing F-keys.

Line 62-63 says "generated via macro so additional rows can be added
without listing every constant" but F1..F12 are listed individually.
Fix the comment to say they are manually listed for grep-ability and
stable numeric values. Add F13..F24 in `0x110C..0x1117`. Update
`src/platform/platform_curses.c` to map `KEY_F13`..`KEY_F24` (standard
in ncurses and PDCurses) to the new constants.

Test: `tests/key_chord_test.c::test_function_keys` checks F13..F24
values.

### Phase 2 — API disambiguation

**2.1** `app_launch` overloaded `NULL` return.

Files: `src/core/desktop.h`, `src/core/desktop.c`, call sites in
`main.c`, `desktop.c`, `desktop_launcher_execute`.

Replace:

```c
RetroAppInstance *app_launch(Desktop *desktop, const char *app_id);
```

with:

```c
typedef enum {
    RETRO_APP_LAUNCHED,
    RETRO_APP_ALREADY_RUNNING,
    RETRO_APP_FAILED,
} RetroAppLaunchResult;

RetroAppLaunchResult app_launch(Desktop *desktop, const char *app_id,
                                RetroAppInstance **out_instance /* nullable */);
```

Semantics:

* `LAUNCHED`: a new instance was created and added to the running list.
  `*out_instance` receives the pointer if non-NULL.
* `ALREADY_RUNNING`: an instance of `app_id` was already running; the
  window was focused. `*out_instance` is left untouched.
* `FAILED`: nothing changed. Documented causes in the header:
  registry miss, capability mask unmet, WM window creation failed,
  `desc->create` failed, OOM when adding to the running list.

Update call sites:

* `desktop_create:361-362` (filemanager + notepad boot). On `FAILED`,
  log to stderr and continue — boot is best-effort.
* `desktop_launcher_execute:130,133,136`. Best-effort; return ignored.
* `desktop_handle_key_command:619,622,625` (hotkeys 1/2/3). Same.

Tests: `tests/desktop_runtime_test.c` covers the three branches by
launching the same app twice and by pointing at a missing app_id.

**2.2** `DesktopDiagnostics` vs `DesktopCapabilities` duplication.

Files: `src/core/desktop.h`, `src/core/desktop.c`, `src/main.c`,
`shell_draw`.

Currently `DesktopCapabilities` (alias of `PlatformFeatures`) and
`DesktopDiagnostics` overlap on five fields: `mouse_enabled`,
`drag_enabled`, `drag_degraded`, `resize_enabled`,
`linux_tty_keyboard_only`. Two structs in sync is fragile.

Shrink `DesktopDiagnostics` to fields not present in
`PlatformFeatures`:

```c
typedef struct DesktopDiagnostics {
    const char *backend_name;
    const char *render_backend_name;
    const char *theme_name;
    bool drag_degraded;
    bool linux_tty_keyboard_only;
} DesktopDiagnostics;
```

Expose:

```c
const PlatformFeatures *desktop_capabilities(const Desktop *desktop);
```

Update `main.c:50-59` (bench output) and `shell_draw`
(`desktop.c:262-275`) to query capability booleans via
`desktop_capabilities()`.

Test: bench mode prints all seven capability booleans; shell window
shows the same five before/after.

**2.3** `RetroKeyEvent.ascii`: sign-safe.

Files: `src/core/event.h`, all call sites that assign `ascii`.

Change:

```c
typedef struct RetroKeyEvent {
    int key_code;
    bool is_printable;
    unsigned char ascii;
} RetroKeyEvent;
```

Audited sites (each gets an explicit `(unsigned char)ch` cast under
`-Wconversion`):

* `src/platform/platform_tty_raw.c`
* `src/platform/platform_curses.c`
* `src/ui/text_input.c` (`key_char` helper)
* `tests/text_input_test.c` (`key_char` helper)

Document the change in `event.h`: "Byte values 0x80..0xFF are valid
in some locales; signed `char` would corrupt them on platforms where
`char` is signed."

Test: existing text_input tests continue to pass; add a unit test
that constructs a `RetroKeyEvent` with `ascii = 0xE9` (eñe in CP-1252)
and asserts it round-trips.

### Phase 3 — Internal refactors

**3.1** `desktop_find_app_index` helper.

`desktop_app_is_running` (line 66), `app_launch` (line 377), and
`desktop_app_window_id` (line 469) all run the same
`strcmp(descriptor->app_id, app_id)` loop. Extract:

```c
static ssize_t desktop_find_app_index(const Desktop *d, const char *app_id);
```

returns -1 on miss, otherwise the index. `ssize_t` from `<sys/types.h>`
is POSIX; if a portability constraint bites, use `size_t` with a
`DESKTOP_APP_NOT_FOUND = SIZE_MAX` sentinel.

After Phase 2.1, `app_launch` does at most one lookup, the other two
collapse to a single call each.

**3.2** `desktop_fill_diagnostics` should not mutate `capabilities`.

Currently (line 284-302) the function writes
`desktop->capabilities.drag_reliable = ...`. The function name advertises
a read-only operation; the side effect is a layering violation.

Move the line out, either into `desktop_create` or into a dedicated
`desktop_apply_runtime_capabilities` call. Rename the function to
`desktop_refresh_diagnostics`.

**3.3** Magic numbers in `desktop_launcher_open`.

File: `src/core/desktop.c:215-219`. Introduce:

```c
enum {
    LAUNCHER_DEFAULT_WIDTH  = 52,
    LAUNCHER_DEFAULT_HEIGHT = 12,
    LAUNCHER_MIN_WIDTH      = 20,
    LAUNCHER_MIN_HEIGHT     = 6,
};
```

**3.4** `LauncherAction::_COUNT`.

Add a sentinel so iteration does not depend on `sizeof` arithmetic:

```c
typedef enum LauncherAction {
    LAUNCHER_ACTION_FILEMANAGER = 0,
    LAUNCHER_ACTION_NOTEPAD,
    LAUNCHER_ACTION_TERMINAL,
    LAUNCHER_ACTION_CLOSE_ACTIVE,
    LAUNCHER_ACTION_CLOSE_MENU,
    LAUNCHER_ACTION_COUNT,
} LauncherAction;
```

Replace `sizeof(k_launcher_items) / sizeof(k_launcher_items[0])` at
lines 169 and 183 with `LAUNCHER_ACTION_COUNT`.

**3.5** Stale "now" comment.

File: `src/core/desktop.h:26`. Delete the word "now":

```c
/* DesktopCapabilities is a typedef alias for PlatformFeatures to avoid
   maintaining two identical structs. */
```

### Phase 4 — Tests + verification

**4.1 New / extended tests.**

| File | Cases |
|---|---|
| `tests/desktop_runtime_test.c` | 1.1 (create-fail cleanup), 2.1 (three launch branches), 3.1 (lookup helper) |
| `tests/cli_parse_test.c`        | 1.2 (SHOWED_USAGE → stdout, PARSE_ERROR → stderr, OK → no output) |
| `tests/key_chord_test.c`        | 1.3 (F13..F24 values), boundary coverage 0x80..0xFF (none of the three classifiers matches) |
| `tests/event_test.c` (new)      | 2.3 (ascii round-trip for 0xE9) |

**4.2 NULL-safety audit.**

Read the following and confirm `if (!p) return;` at the top:

* `renderer_destroy`
* `wm_destroy`
* `app_registry_destroy`
* `statusbar_destroy`
* `draw_list_destroy`
* `app_destroy`
* `app_launch` (post 2.1)

Patch any that lack the guard.

**4.3 Verification matrix.**

```bash
# 1. Debug, asserts enabled
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=ON
cmake --build build-debug
ctest --test-dir build-debug --output-on-failure

# 2. Release
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=ON
cmake --build build
ctest --test-dir build

# 3. Strict warnings + Werror
cmake -S . -B build-strict \
      -DENABLE_STRICT_WARNINGS=ON -DENABLE_WERROR=ON \
      -DCMAKE_BUILD_TYPE=Debug
cmake --build build-strict

# 4. ASan + UBSan
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan
ctest --test-dir build-asan
```

Target: 100% pass on (1) and (2), zero warnings on (3), zero
leaks/errors on (4).

### Phase 5 — Polish

**5.1** Hotkeys accept uppercase.

File: `src/core/desktop.c:591` and `:627`. Today `q` and `w` quit/close
only in lowercase, but `m` already accepts `m`/`M`. Inconsistency.

Decision: accept `q`/`Q` and `w`/`W`. The user's terminal with
Caps Lock on must not be unable to exit. Ctrl+C was considered
but requires signal handling which is out of scope for `core/`.

**5.2** Selective redraw in `desktop_run`.

File: `src/core/desktop.c:639-673`. Today every event sets
`needs_redraw`, which repaints the whole screen. For slow TTYs
(19200 baud serial) this is visible lag.

Add `uint64_t Desktop::last_frame_signature` — a cheap hash of
visible state (window positions, active window id, launcher state,
status text length). Compute before each `desktop_render_frame`;
skip if unchanged and `needs_redraw` came from a non-visual event.

**5.3** Statusbar cache.

File: `src/core/desktop.c:507-550`. Today the formatted status string
is rebuilt and pushed via `statusbar_set_text` every second even when
nothing changed. Cache the last formatted string and skip the
`statusbar_set_text` call if the new one is identical.

**5.4** Documentation.

* `docs/AGENTS.md`: section "ASCII vs extended ASCII (0x80..0xFF)"
  explaining that none of `is_chord` / `is_printable` / `is_control`
  classify this range.
* `src/core/event.h`: comment on `unsigned char ascii` with the
  sign-safety rationale.
* `src/core/desktop.h`: comment on why `DesktopDiagnostics` is a
  strict subset of `PlatformFeatures`.

## Execution order (suggested commit grouping)

```
Phase 1.3 → 4.1  commit: "core(key_chord): extend F-keys to F24, fix comment"
Phase 1.1 → 4.1  commit: "core(desktop): cleanup app on create failure"
Phase 1.2 → 4.1  commit: "core(cli): distinguish usage vs error exit"
Phase 2.3 → 4.1  commit: "core(event): sign-safe ASCII byte"
Phase 2.1 → 4.1  commit: "core(desktop): disambiguate app_launch return"
Phase 2.2 → 4.1  commit: "core(desktop): dedup diagnostics vs capabilities"
Phase 3.1-3.5    commit: "core(desktop): internal refactors"
Phase 5.1        commit: "core(desktop): accept uppercase quit/close hotkeys"
Phase 5.2-5.3    commit: "core(desktop): skip no-op redraws"
Phase 5.4        commit: "docs: core module notes"
```

Eleven commits, each independently verifiable. Total estimate: one
focused session per phase.

## Verification after Phase 5

All four configurations from §4.3 must report zero issues. The
review checklist at the top of this document should be fully checked:

* [ ] 2.1 — `app_launch` return type is unambiguous
* [ ] 2.2 — no duplicated capability/diagnostics fields
* [ ] 2.3 — `RetroKeyEvent.ascii` is `unsigned char`
* [ ] 3.1 — single lookup helper, three callers
* [ ] 3.2 — `desktop_refresh_diagnostics` is read-only
* [ ] 3.3 — launcher dimensions are named constants
* [ ] 3.4 — `LauncherAction::_COUNT` used in iterations
* [ ] 3.5 — "now" removed from comment
* [ ] 5.1 — uppercase Q/W hotkeys
* [ ] 5.2 — selective redraw
* [ ] 5.3 — statusbar cache
* [ ] 5.4 — docs in place

Quality gates:

* [ ] 0 warnings under `-Wall -Wextra -Wconversion -Wsign-conversion -Werror`
* [ ] 0 failures in Debug / Release
* [ ] 0 ASan / UBSan errors
* [ ] All review items covered by a regression test