# App Runtime Contract

## App Model

Each app is represented by a descriptor with:

- stable `app_id`,
- display name,
- required capability mask,
- lifecycle callbacks (`create`, `event`, `render`, `can_close`, `destroy`),
- default window geometry and flags,
- optional `allow_multiple` behavior.

## Lifecycle

1. Register descriptor.
2. Launch app instance if capabilities are satisfied.
3. Optionally bind a runtime-owned `resource_path` for open-with-path flows.
4. Receive normalized events from the desktop/window manager.
5. Append draw commands into the provided `DrawList`.
6. Reject close through `can_close` when app state requires it.
7. Destroy and release resources on close.

If `create` fails after partial state acquisition, `destroy` is the rollback
hook.

## Ownership Rules

- Runtime owns app instance lifecycle.
- Runtime owns `resource_path_owned` and exposes it as `ctx.resource_path` for
  the lifetime of the instance.
- App owns only its private state.
- App cannot call backend polling or frame flush APIs directly.
- Apps may call high-level runtime APIs such as launching another app with a
  resource path, but must not bypass descriptors or capability checks.

## Built-In Apps

- `filemanager`: Linux/POSIX preview app for directory listing, keyboard
  navigation, parent navigation, refresh, and opening regular files in Notepad.
- `notepad`: Linux/POSIX preview app for text editing, open-with-path, save,
  Save As, conflict reporting, and dirty-close rejection.
- `diagnostics`: read-only runtime diagnostics view. It is not a shell or PTY.

## App Categories

- Built-in apps: shipped in repository.
- Optional apps: feature-gated by capability or build profile.
- Unsupported apps: explicitly rejected at launch with clear reason.

## Forbidden Patterns

- direct calls to `getch`, `wgetch`, `doupdate`,
- direct use of backend-native types in app headers,
- standalone app event loops,
- filesystem access that bypasses the storage abstraction for file-app
  workflows.
