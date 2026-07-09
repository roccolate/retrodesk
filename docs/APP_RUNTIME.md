# App Runtime Contract

## App Model

Each app is represented by a descriptor with:

- stable `app_id`,
- display name,
- required capability mask,
- lifecycle callbacks (`create`, `on_event`, `on_render`, `destroy`),
- default window geometry and flags.

The app runtime is responsible for launching descriptors, creating owned
windows, routing normalized events, and releasing app state on close.

## Current Built-In Apps

Registered by `src/apps/apps.c`:

| App ID | Display Name | Current Scope |
| --- | --- | --- |
| `filemanager` | File Manager | Keyboard-navigable placeholder app |
| `notepad` | Notepad | Single-buffer placeholder editor |
| `terminal` | Terminal | Runtime capability diagnostics placeholder |

These apps prove the app runtime contract. They are not yet complete productivity
apps.

## Lifecycle

1. Register descriptor.
2. Launch app instance if capabilities are satisfied.
3. Create an app-owned window through `wm`.
4. Receive normalized events from runtime.
5. Append draw commands into the provided `DrawList`.
6. Destroy private app state and release the owned window on close.

## Ownership Rules

- Runtime owns app instance lifecycle.
- App owns only its private state.
- App cannot call backend polling or frame flush APIs directly.
- App windows are marked with `WINDOW_FLAG_APP_OWNED`.
- Apps interact with the outside world through normalized event/render/context
  contracts, not backend-native APIs.

## Capability Rules

- Apps declare `required_capabilities` in their descriptor.
- `app_launch` rejects apps whose required capability mask is not satisfied by
  the current `PlatformFeatures` profile.
- Unsupported capabilities must fail deterministically at launch or be guarded
  inside the app behind explicit feature checks.

## App Categories

- Built-in apps: shipped in repository and registered by `apps_register_builtin`.
- Optional apps: feature-gated by capability or build profile.
- Unsupported apps: explicitly rejected at launch with clear reason.

## Forbidden Patterns

- direct calls to `getch`, `wgetch`, `doupdate`,
- direct use of backend-native types in app headers,
- standalone app event loops,
- rendering directly to curses/PDCurses/ANSI from an app,
- app-specific special cases in `core` that bypass descriptors.
