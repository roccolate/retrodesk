# Architecture

## Target Layers

- `core`: desktop lifecycle, scheduler, runtime state.
- `wm`: window collection, z-order, focus, drag, close lifecycle.
- `app`: app registry and app lifecycle contract.
- `render`: drawing abstraction and single frame flush policy.
- `platform`: backend initialization and native event translation.
- `ui`: reusable widgets operating on internal abstractions.

## Event Flow

1. `platform_poll_event()` returns a `RetroEvent`.
2. `core` handles global commands/hotkeys.
3. `wm_handle_event()` routes event to focused/target windows.
4. App callbacks receive normalized events.
5. State mutations mark desktop dirty.

## Render Flow

1. `core` begins frame.
2. `wm_render()` draws all windows back-to-front.
3. `ui` draws overlays/status widgets.
4. `renderer_flush()` is called once at end of tick.

## Dependency Rules

- `core` depends on interfaces from `wm`, `app`, `render`, `platform`.
- `wm` depends on `render` and event types, not on backend internals.
- `app` depends on runtime interfaces and event/render contracts.
- `platform` and `render` can use `curses` internals.

## Modal Policy

Modals are window flags or overlays handled by `wm`, never nested blocking loops.

## Ownership Policy

- `core` owns process-level runtime and service instances.
- `wm` owns window objects and render targets.
- App runtime owns app instance memory and app lifecycle callbacks.
