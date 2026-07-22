# Architecture

> Current against `main` at the 2026-07-22 project snapshot. Temporary bridge
> architecture is described honestly; proposed changes in open PRs are not
> treated as integrated.

## System Shape

RetroDesk is a hosted terminal desktop runtime. The Desktop owns the process-level
services and coordinates one event loop. Applications do not own platform input
loops and do not draw directly to curses, ANSI, PDCurses, or a native console.

```text
platform input
      |
      v
 normalized RetroEvent
      |
      v
 Desktop global routing --> taskbar / Launcher / window commands
      |
      v
 Window Manager routing --> focused or targeted hosted app
      |
      v
 app state changes / close / redraw request
      |
      v
 one dirty-frame render --> one renderer flush
```

## Layers

### `core`

Owns:

- CLI result handling;
- Desktop create/run/shutdown lifecycle;
- process-level services and runtime state;
- coordinated global commands and shutdown negotiation;
- Desktop-owned clipboard;
- UTF-8 helpers and normalized core event contracts;
- running-app table and app/window association;
- budgeted app-service scheduling.

`core/desktop.c` remains a decomposition target, but F9 move/resize state,
target synchronization, drag-theme projection and HUD behavior now live in the
private `core/desktop_window_mode` controller. Desktop retains composition,
command routing and the single render/flush sequence.

### `platform`

Owns:

- curses/PDCurses and raw-TTY input initialization;
- conversion from backend-native events into `RetroEvent`;
- capability reporting;
- terminal mode capture/restoration;
- backend-specific mouse, resize, and modifier behavior.

Platform-native types do not cross into public domain headers.

### `render`

Owns:

- the backend-neutral `DrawList` command model;
- render contexts and backend dispatch;
- curses/PDCurses drawing;
- ANSI frame construction and diffing;
- the only backend execution and frame-flush path.

Windows, apps, and widgets append commands. They never call backend drawing or
flush APIs directly.

### `wm`

Owns:

- window allocation and destruction;
- geometry, z-order, focus, active state, and hit testing;
- modal routing;
- capability-gated drag;
- keyboard movement/resizing primitives;
- minimized-window visibility/input/focus policy;
- draw lists associated with windows.

Maximize/restore state and exact restore geometry are owned by each
`RetroWindow`; ordinary Window Manager APIs implement maximize, restore, resize
refresh and rejection policy.

### `app`

Owns:

- descriptor registration and lookup;
- capability checks;
- lifecycle dispatch;
- close-result semantics;
- optional service callback dispatch;
- app-instance state helpers.

### `apps`

Contains hosted built-in applications:

- File Manager;
- Notepad;
- Diagnostics.

Apps may use high-level runtime and storage interfaces. They may not poll the
platform, start nested loops, call renderer flush, or bypass `retro_fs`.

### `ui`

Owns reusable presentation and interaction components:

- UTF-8 text input and text buffer;
- scroll list, buttons, dialogs, menu bar, progress bar, and tabs;
- status bar/taskbar;
- Launcher menu presentation;
- theme and desktop-surface tokens;
- move/resize HUD presentation.

### `storage`

Defines `retro_fs`, the platform-neutral filesystem/text boundary. Integrated
adapters are:

- `retro_fs_posix.c`;
- `retro_fs_win32.c`;
- `retro_fs_djgpp.c`.

Apps do not assign native meaning to the adapter-neutral version token.

## Event Loop

One Desktop loop performs this sequence:

1. Invoke each active app service once with a bounded work budget.
2. Reconcile app close requests and global-shutdown state.
3. Poll one normalized platform event with the current timeout policy.
4. Route global key/mouse commands that are unambiguous.
5. Route taskbar mouse actions before ordinary window hit testing.
6. Route remaining events through the Window Manager.
7. Reconcile app/window lifecycle again.
8. Refresh taskbar/status state.
9. Render only when the frame is marked dirty.

Printable text, ordinary `Tab`, and unclaimed function keys belong to the focused
application. Global routing must not steal editor input.

## App Service Policy

An app descriptor may expose `on_service` for asynchronous resources.

- Desktop invokes it once per ordinary loop iteration.
- The current work budget is 8192 bytes/units per callback.
- An active service caps event polling at 16 ms.
- A service returns idle or redraw intent.
- A service does not poll platform input, render, flush, block indefinitely, or
  create another owner thread/loop for UI state.

The policy is intentionally simple O(N). Fairness/readiness/backpressure changes
should be driven by the first measured PTY/network workload rather than by
speculation.

## Global Input Routing

Current global commands:

- `F1`/`F10`: Launcher;
- `F6`: focus cycle;
- `F8`: maximize/unmaximize;
- `F9`: keyboard move/resize mode;
- `Ctrl+W`: active app close request;
- `Ctrl+Q`: coordinated global shutdown.

`F2` remains application-local so File Manager Rename is not stolen by Desktop.

## Window State Contracts

### Focus and z-order

Only eligible visible windows participate in focus cycling. Bringing a window to
front does not bypass modal policy. Fixed desktop chrome is not treated as an
ordinary movable application surface.

### Minimize

A minimized window:

- remains allocated and retains its app instance and geometry;
- is excluded from rendering;
- is excluded from keyboard dispatch and mouse hit testing;
- cannot receive drag or participate in modal routing;
- is excluded from active-window selection and `F6` cycling;
- remains part of shutdown and close ownership.

Fixed and modal windows reject minimize. Explicit focus restores a minimized
window, preserving single-instance launch and deferred-close recovery behavior.

### Maximize

For an ordinary active window:

- maximization records its exact restore geometry;
- maximized geometry is `(0, 0)` with full terminal width and usable height above
  the taskbar;
- terminal resize refreshes maximized geometry;
- minimizing does not discard maximized state;
- taskbar restore reapplies current maximized geometry;
- movement and resizing are rejected until unmaximized;
- fixed/modal windows reject the operation.

Input mappings are `F8` and active title-bar double click.

### Keyboard move/resize mode

`F9` targets the current active ordinary window. The active frame uses the
existing operation/drag theme token and an overlay HUD displays MOVE/RESIZE,
geometry, and controls.

- arrows adjust position or size;
- `Tab` toggles MOVE/RESIZE;
- `Enter`, `F9`, or `Esc` finish;
- focus change cancels the mode;
- fixed/no-window and maximized cases show a short explanation rather than
  leaving a stuck mode.

## Render Flow

A dirty frame is rendered as:

1. `renderer_clear()`;
2. Window Manager renders each visible window using the selected theme;
3. app callbacks append content commands into their window draw lists;
4. Desktop renders taskbar and operation HUD into one overlay draw list;
5. renderer executes the overlay;
6. `renderer_flush()` runs exactly once.

The private window-mode controller may copy the immutable selected theme for one
frame and replace only the active-frame token with `window_frame_drag`. It does
not mutate the authoritative theme catalog.

## Theme Model

`RetroTheme` defines core window/body/app tokens. `RetroSurfaceTheme` defines
explicit desktop-surface states for:

- taskbar base;
- Apps normal/open;
- app idle/running/focused;
- clock;
- Launcher header/section/item/selected/separator/footer.

XP, Hacker, Amiga, and Win31 share the interaction contract but use distinct
surface identities. Unknown/custom combinations have deterministic fallback
styles.

## Taskbar Model

Desktop builds a snapshot containing:

- fixed catalog app id/label;
- instance count;
- focused state;
- Launcher-open state;
- clock text.

The StatusBar widget:

- chooses full or compact labels based on available width;
- appends the exact draw commands;
- records mouse regions from rendered widths;
- returns actions without directly owning app or window lifecycle.

Desktop translates taskbar actions:

- zero instances -> launch;
- running background -> focus/raise;
- focused app -> minimize active instance;
- minimized -> restore/focus/raise;
- multiple instances -> deterministic cycle.

Current limitation: the catalog is fixed and there is no overflow controller for
a future larger app set.

## Launcher Model

The Launcher is a modal fixed popup anchored bottom-left above the taskbar. It
uses a pure menu layout/presentation contract and explicit Desktop-owned
callbacks, state and lifetime.

It groups Applications and Desktop actions, supports responsive descriptions,
keyboard navigation/accelerators, and mouse row activation. It remains in the
single event loop and never owns a blocking menu loop.

## App Lifecycle

1. Resolve the descriptor.
2. Verify required capabilities.
3. Allocate an app instance and copy an optional resource path.
4. Create the app-owned window.
5. Invoke `create` to acquire private state.
6. On failure after `create` begins, invoke matching `destroy` rollback.
7. Associate app instance and window in Desktop's running-app table.
8. Route normalized events and DrawList rendering.
9. Route close through `RetroCloseResult`/`can_close` policy.
10. Destroy app state, close the window, and free runtime-owned resources.

Current API debt: `app_launch()` returns `NULL` both for failure and for the
single-instance policy case where an existing instance was focused.

## Close and Shutdown

`Ctrl+W` requests close of the active app. An app may allow, defer, or cancel.
Notepad uses deferred close to show Save/Discard/Cancel in app state without a
nested loop.

`Ctrl+Q` is transactional:

- apps are asked in sequence;
- clean authorizations do not destroy windows immediately;
- one deferred app receives focus;
- cancellation resets all earlier authorizations;
- successful completion commits all closes together.

## Text and Unicode Model

`TextBuffer` stores logical lines as UTF-8 byte arrays. Cursor and selection
columns are byte offsets for storage/API compatibility.

Invariants:

- public mutations normalize offsets to codepoint boundaries;
- Left/Right, Backspace, and Delete operate on complete codepoints;
- vertical movement preserves terminal display columns;
- rendering converts byte ranges to cells;
- selection may span lines;
- undo/redo restores text and cursor boundaries;
- soft wrap creates visual rows only;
- Find returns byte-accurate ranges used as normal selections.

The clipboard is a Desktop-owned, validated UTF-8 process service. Native system
clipboard integration is not part of the current contract.

## Storage Boundary

File Manager and Notepad call `retro_fs`. The adapters own:

- path representation and native conversion;
- stat and optimistic-concurrency identity;
- deterministic bounded listing;
- text validation;
- create/mkdir/rename semantics;
- replacement strategy and platform durability limits.

POSIX and Win32 have native replacement primitives with different APIs. DJGPP
uses a same-directory temporary plus backup/restore transaction because DOS has
no universal true atomic-replace syscall.

## Ownership Policy

- Desktop owns process-level runtime services and running-app associations.
- Window Manager owns windows and their draw lists.
- App runtime owns descriptor dispatch; Desktop owns instance allocation.
- Each app owns and destroys its private state.
- Clipboard data is owned by the Desktop clipboard service.
- Storage path objects have explicit init/destroy ownership.
- Themes are immutable catalogs borrowed by runtime consumers.

## Dependency Rules

- `core` consumes high-level interfaces from other layers and owns orchestration.
- `wm` depends on render/event contracts, not backend internals.
- `app` depends on normalized lifecycle/event/render contracts.
- `apps` depend on app runtime, reusable UI, core services, and `retro_fs`.
- `ui` may use core UTF-8/event contracts but not backend-native terminal types.
- `storage` adapters own OS filesystem details.
- `platform` and `render` own backend-native types and calls.
- No layer other than render executes a backend draw/flush operation.

## Desktop chrome ownership

Desktop chrome flow is explicit and instance-owned:

- maximize/restore state lives in each `RetroWindow`;
- F8 and title-bar gestures route through `core/desktop_chrome.c`;
- taskbar action metadata and target selection are consumed synchronously;
- Launcher rendering, selection, callbacks, geometry, and lifetime belong to each
  `Desktop`;
- F9 move/resize target, mode and one-frame blocked notice belong to each
  `Desktop`.

`statusbar.h` is again a pure widget contract. It includes no Desktop-private
adapter and performs no function-like macro remapping of Window Manager or
StatusBar operations. See [KNOWN_ISSUES.md](KNOWN_ISSUES.md), especially KB-011
and the resolved KB-012.

## Current Product Slice

The runtime and native storage implementations exist for POSIX, Win32, and
DJGPP, but maturity claims differ:

- Linux/ncurses is the primary development/product profile;
- Windows/PDCurses has strict automated runtime/storage coverage but incomplete
  current manual interaction evidence;
- DOS has a validated reduced native profile with explicit filesystem and runtime
  limits;
- raw-TTY/ANSI and macOS remain experimental evidence profiles.

See [PORTABILITY.md](PORTABILITY.md) for the authoritative claim matrix.

## Capacity and identifier invariants

Desktop and Window Manager dynamic tables use checked element-capacity
growth before byte-size multiplication or `realloc`. Failed growth leaves
the previous table, count, focus, z-order, and ownership unchanged.

Window IDs are positive and monotonic. `INT_MAX` is the final valid ID;
live IDs are never reused, and later creation returns
`WINDOW_ID_INVALID` without mutating existing windows.
