# Architecture

## Layers

- `core`: CLI parsing, desktop lifecycle, global commands, shared services,
  runtime state, UTF-8 helpers, and the in-process clipboard.
- `wm`: window collection, z-order, focus, drag, move/resize, modal routing, and
  close lifecycle.
- `app`: app registry and app lifecycle contract.
- `render`: drawing abstraction, curses/ANSI adapters, draw lists, and the
  single-frame-flush policy.
- `platform`: backend initialization and normalized native event translation.
- `ui`: reusable widgets operating on internal abstractions, including the
  interactive statusbar/taskbar and UTF-8 editor widgets.
- `storage`: file-app storage contract and platform adapters.
- `apps`: built-in hosted apps implemented through the app runtime.
- `theme`: visual tokens consumed by `wm`, `ui`, and apps.

## Event Flow

1. `platform_poll_event()` returns a normalized `RetroEvent`.
2. `core` handles unambiguous global commands such as function keys and control
   chords.
3. Taskbar mouse hit testing can consume a click before window routing.
4. `wm_handle_event()` routes remaining events to the focused or targeted
   window.
5. App callbacks update their state and may request close or redraw.
6. Desktop cleanup reconciles app requests with window existence and app
   `can_close` policy.

Printable keys and ordinary `Tab` events belong to the focused application.
Global routing must not steal editor text.

## Render Flow

1. `core` decides whether a frame is dirty.
2. `renderer_clear()` starts the frame.
3. `wm_render()` asks windows/apps to append draw commands.
4. The overlay draw list renders the interactive taskbar after app windows.
5. `renderer_flush()` is called once at the end of the tick.

The bottom bar is still implemented by the reusable `StatusBar` widget, but its
snapshot mode is an interactive taskbar with app state, hit regions, instance
counts, and clock.

## App Lifecycle

1. The app descriptor is resolved from the registry.
2. Required capabilities are checked before launch.
3. An app instance and owned resource-path copy are allocated.
4. The app-owned window is created.
5. The descriptor `create` callback acquires app state.
6. Any failure after `create` begins invokes the matching `destroy` path.
7. The desktop owns the running-app slot; the app owns its private state.
8. Close requests pass through `can_close` before the window is removed.

This allows Notepad to defer `Ctrl+W` while it displays Save/Discard/Cancel.

## Taskbar Model

The desktop builds a snapshot from the running app table:

- pinned app id and label;
- number of running instances;
- whether one instance owns focus;
- whether the Launcher is open;
- current clock text.

The widget formats and renders that snapshot, records clickable regions, and
returns actions without directly accessing Window Manager internals. Desktop
code performs launch, focus, or instance cycling.

## Text and Unicode Model

`TextBuffer` stores logical lines as UTF-8 byte arrays. Cursor and selection
columns are byte offsets for storage/API compatibility, with these invariants:

- every public mutation normalizes offsets to valid codepoint boundaries;
- Left/Right, Backspace, and Delete operate on complete codepoints;
- vertical navigation preserves terminal display columns;
- rendering converts byte ranges to terminal cells;
- soft Word Wrap creates visual rows only and never rewrites logical text;
- Find returns byte-accurate ranges that become ordinary selections.

The shared clipboard accepts validated UTF-8 and is process-local. It is a core
service so multiple Notepad instances can exchange text without platform-native
clipboard dependencies.

## Storage Boundary

Apps call `retro_fs` rather than POSIX APIs. The active POSIX adapter owns path,
listing, validation, creation, rename, stat/version, and atomic-write details.
This prevents filesystem assumptions from leaking into app and widget code.

## Dependency Rules

- `core` depends on interfaces from `wm`, `app`, `render`, `platform`, and `ui`.
- `wm` depends on render/event contracts, not backend internals.
- `app` depends on runtime interfaces and event/render contracts.
- `apps` depend on app/runtime, reusable UI, core services, and high-level
  storage contracts.
- `ui` may use shared UTF-8 helpers but not platform-native terminal types.
- `storage` adapters own OS-specific filesystem details.
- `platform` and `render` may use backend-specific internals.

## Modal Policy

Modals are window flags or app state rendered through the main event loop. There
are no nested blocking event loops. Save As, Find, File Manager naming prompts,
and dirty-document confirmation all follow this rule.

## Ownership Policy

- `core` owns process-level runtime and service instances.
- `wm` owns windows and window draw lists.
- App runtime owns app instance allocation and lifecycle dispatch.
- Each app owns and destroys its private state.
- Clipboard data is owned by the shared core service.
- Storage path objects have explicit init/destroy ownership.

## Current Product Slice

The complete product slice is Linux/POSIX:

- File Manager consumes directory, create, rename, and open contracts.
- Notepad consumes UTF-8 read, atomic save, and version-conflict contracts.
- Taskbar and portable input behavior are shared runtime features.
- Diagnostics is hosted but intentionally does not provide a shell.

Windows builds validate portable layers through PDCurses and tests, but native
storage or deterministic app gating is still required before equivalent file-app
support can be claimed.
