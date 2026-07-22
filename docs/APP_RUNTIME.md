# App Runtime Contract

> Describes the runtime integrated on `main`. Proposed responsive Notepad chrome
> in PR #24 is not part of this contract snapshot.

## Descriptor Model

Each hosted app is represented by a `RetroAppDescriptor` containing:

- stable `app_id`;
- display name;
- required capability mask;
- default window geometry and window flags;
- optional multiple-instance policy;
- lifecycle callbacks such as `create`, event, render, close policy, and
  `destroy`;
- optional bounded `on_service` callback for asynchronous resources.

Descriptors are registered before launch. Duplicate IDs are rejected.

## App Context

The runtime supplies borrowed high-level context including:

- owning Desktop;
- Desktop clipboard service;
- app window ID;
- platform capability view;
- selected theme;
- optional runtime-owned resource path.

Apps do not receive backend-native window, terminal, console, or filesystem
handles through the public context.

The responsive hosted-window dimensions proposed in PR #24 are not integrated on
current `main` as a general app-context contract.

## Launch Lifecycle

1. Resolve the descriptor by `app_id`.
2. Verify its required capabilities.
3. Apply single/multiple-instance policy.
4. Allocate a runtime-owned app instance.
5. Copy an optional resource path for the lifetime of the instance.
6. Create the Window Manager-owned app window.
7. Invoke descriptor `create` to acquire private state.
8. If create reports failure, invoke matching `destroy` rollback.
9. Record the app/window association in Desktop's running-app table.
10. Mark the frame dirty.

Current API debt: `app_launch()` may return `NULL` for true failure and for the
successful single-instance policy case where an existing app was focused. This
must eventually become an explicit launch result.

## Ownership

- Desktop owns app-instance allocation and running-app associations.
- Window Manager owns app windows and window DrawLists.
- Runtime owns the copied resource path.
- Each app owns only its private `state`.
- Descriptor `destroy` releases all private state, including partial create state.
- Desktop owns the clipboard and platform/runtime services.
- Apps borrow capabilities/themes and do not mutate their authority.

No silent ownership transfer is permitted.

## Event Contract

Apps receive normalized `RetroEvent` values after Desktop global routing and
Window Manager focus/modal routing.

Apps may:

- mutate private state;
- request redraw through high-level runtime behavior;
- request their own close;
- launch another descriptor through Desktop APIs where the workflow requires it;
- use `retro_fs` and reusable UI contracts.

Apps may not:

- call `getch`, `wgetch`, platform polling, or native message loops;
- own a nested event loop for prompts or menus;
- call renderer backend primitives or flush;
- directly destroy their Window Manager window;
- call native filesystem APIs for file-app workflows;
- assume a capability that was not reported.

## Render Contract

Descriptor render callbacks append commands to the supplied `DrawList`.

- Drawing is backend-neutral.
- The callback does not clear or flush the screen.
- Theme tokens are borrowed from the selected theme.
- Cursor/selection geometry uses the shared UTF-8/display-cell contracts.
- Desktop/Window Manager decides whether the window is visible, active,
  minimized, or maximized.

## App Service Contract

A descriptor may expose `on_service` when work can arrive without a user event.
Desktop invokes it once per main-loop iteration.

Current policy:

- work budget: 8192 bytes/units per call;
- results: idle and/or redraw request;
- platform poll cap while any service is active: 16 ms;
- callback executes inside the Desktop loop;
- no independent UI owner thread or event loop.

A service callback must:

- return promptly;
- consume only bounded work;
- never poll platform input;
- never render or flush;
- never block waiting for a child/network peer indefinitely;
- leave close/lifecycle state consistent if it requests redraw or shutdown work.

This is the foundation for a future PTY/network/subprocess app. Diagnostics does
not currently implement a PTY or terminal emulator.

## Close Contract

`Ctrl+W` becomes an app close request for the active ordinary app window.

An app can:

- allow immediate close;
- defer while collecting an in-loop decision;
- cancel/preserve state.

Notepad defers close to render Save/Discard/Cancel state. There is no nested modal
loop. A successful save or explicit discard authorizes close; cancellation leaves
the same app/window alive.

## Global Shutdown Contract

`Ctrl+Q` begins coordinated negotiation:

1. Desktop asks each app to authorize close.
2. Clean authorization is remembered but not committed immediately.
3. A deferred dirty app keeps focus and resolves its workflow.
4. Cancellation resets all prior app requests.
5. Successful completion closes every app and exits together.

This prevents half-shutdown state when one dirty app cancels.

## Multiple Instances and Taskbar

Descriptors may allow multiple instances. Notepad does; File Manager and
Diagnostics follow their descriptor policies.

Taskbar behavior is implemented outside the widget's lifecycle ownership:

- zero instances -> Desktop launches the descriptor;
- running background instance -> focus/raise;
- focused app -> minimize active instance;
- minimized instance -> restore/focus/raise;
- multiple instances -> deterministic cycling.

The taskbar returns actions. Desktop and Window Manager execute app/window
operations.

## Capability Rejection

Launch checks the descriptor's required mask against platform capabilities.
Unsupported apps fail deterministically without creating partial state or a
window.

This is the required pattern for optional filesystem, pointer, color, Unicode,
or future PTY capabilities. Apps must not initialize and then silently offer a
broken partial workflow.

## Built-In Descriptors

### `filemanager`

Uses `retro_fs` for bounded listing, navigation, regular-text open, rename,
mkdir, and empty-file creation.

### `notepad`

Uses TextBuffer, Desktop clipboard, and `retro_fs` for UTF-8 editing, selection,
history, Find, Word Wrap, versioned Save/Save As, recovery, and dirty close.

### `diagnostics`

Read-only runtime/backend/capability view. It has no shell, PTY, terminal parser,
or subprocess ownership.

## Failure Rules

- A failed capability check creates no instance/window.
- Failed instance/resource allocation releases prior allocations.
- Failed window creation releases instance/resource allocation.
- Once descriptor create begins, failure invokes descriptor destroy.
- Failed running-table insertion destroys app state and closes its window.
- App-requested close is reconciled through Desktop; it is not a raw free.
- Shutdown remains idempotent where practical.

Checked running-table capacity growth and injected growth-failure recovery are
proposed in stale PR #27, not integrated on current `main`.

## Current Runtime Debt

- ambiguous `app_launch` result;
- startup does not yet surface built-in registration failure as strongly as other
  initialization failures;
- Desktop repeats some app-id scans and owns too many integration concerns;
- service scheduling is simple O(N) with one fixed budget/poll cap;
- temporary desktop bridges use translation-unit static state rather than final
  per-Desktop ownership.

See [CORE_HARDENING_PLAN.md](CORE_HARDENING_PLAN.md) and
[KNOWN_ISSUES.md](KNOWN_ISSUES.md).

## Forbidden Patterns

- backend polling/drawing in apps;
- nested menu/dialog/event loops;
- direct renderer flush;
- native filesystem calls bypassing `retro_fs`;
- unbounded asynchronous service work;
- platform assumptions without capabilities;
- app-owned destruction of Window Manager objects;
- undocumented ownership transfer;
- treating PR-only descriptor/context changes as integrated API.