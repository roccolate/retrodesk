# Architecture

## Runtime Layers

- `core`: CLI option parsing, desktop lifecycle, global hotkeys, launcher,
  diagnostics, runtime ownership, and the main event/render loop.
- `wm`: window collection, z-order, focus, drag, close lifecycle, keyboard
  movement, modal policy, and window draw/event routing.
- `app`: app registry and app lifecycle contract (`RetroAppDescriptor`,
  `RetroAppInstance`, app context, capability checks).
- `apps`: built-in app descriptors and built-in app registration.
- `render`: drawing abstraction, `DrawList`, backend adapters (curses/ANSI),
  and the single frame flush policy.
- `platform`: backend initialization, feature/capability discovery, and native
  event translation (`platform` facade + curses/tty backend modules).
- `ui`: reusable widgets and visual tokens operating on internal abstractions
  (`text_input`, `text_buffer`, `scroll_list`, `button`, `dialog`, `menu_bar`,
  `progress_bar`, `tab`, `statusbar`, `theme`).

## Shared Vocabulary

- `RetroEvent` and related event structs live in `src/core/event.h`.
- Backend-neutral key codes and key classification helpers live in
  `src/core/key_chord.h` / `src/core/key_chord.c`.
- `PlatformFeatures` is the capability source used by desktop and apps.
- Themes are implemented under `ui` as `RetroTheme` / `RetroThemeKind`; there
  is no separate `src/theme/` layer.

## Event Flow

1. `platform_poll_event()` returns a normalized `RetroEvent`.
2. `core` handles process/global commands and hotkeys.
3. `wm_handle_event()` routes unconsumed events to the focused or targeted
   window.
4. App callbacks receive normalized events through the app runtime contract.
5. State mutations mark the desktop dirty when a redraw is required.

## Render Flow

1. `core` begins the frame.
2. `wm_render()` asks windows/apps to append draw commands.
3. `ui` components append their own draw commands through the same abstractions.
4. `render` executes draw lists to backend contexts.
5. The renderer flushes once at the end of the tick.

## Dependency Rules

- `core` may depend on interfaces from `wm`, `app`, `apps`, `render`,
  `platform`, and `ui`.
- `wm` depends on normalized event/render/theme contracts, not backend internals.
- `app` depends on runtime interfaces and event/render/capability contracts.
- Built-in `apps` depend on app runtime, normalized events, draw lists, and
  themes.
- `ui` depends on normalized event/render/theme contracts.
- `platform` and `render` are the only layers allowed to use backend-specific
  internals such as curses/PDCurses calls or ANSI terminal control.

## Modal Policy

Modals are window flags or overlays handled by `wm`, never nested blocking
loops. A modal window blocks event delivery to non-modal windows until it is
closed or dismissed.

## Ownership Policy

- `core` owns process-level runtime and service instances.
- `wm` owns window objects and window ordering/focus state.
- App runtime owns app instance memory and lifecycle callbacks.
- Apps own only their private state.
- Render/platform backends own only backend-specific resources and must release
  them through explicit destroy/shutdown paths.
