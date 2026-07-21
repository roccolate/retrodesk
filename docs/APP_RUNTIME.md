# App Runtime Contract

## App Model

Each app is represented by a descriptor with:

- stable `app_id`;
- display name;
- required capability mask;
- lifecycle callbacks (`create`, `event`, `render`, `can_close`, `destroy`);
- default window geometry and flags;
- optional `allow_multiple` behavior.

## Lifecycle

1. Register the descriptor.
2. Resolve the descriptor and verify required capabilities.
3. Allocate the app instance and optional runtime-owned resource path.
4. Create the app-owned window.
5. Invoke `create` to acquire private app state.
6. Receive normalized events through the desktop/window manager.
7. Append draw commands into the supplied `DrawList`.
8. Route close requests through `can_close` when present.
9. Destroy app state, release the window, and free runtime-owned resources.

If `create` has been called and then reports failure, `destroy` is the matching
rollback hook for partially acquired state.

## Ownership Rules

- Desktop/runtime owns app instance allocation and the running-app table.
- Window Manager owns windows and window draw lists.
- Runtime owns `resource_path_owned` and exposes it as `ctx.resource_path` for the
  lifetime of the instance.
- Each app owns only its private state and destroys it through its descriptor.
- Apps never poll a backend or flush a frame directly.
- Apps may call high-level runtime APIs such as launching another app with a
  resource path, but cannot bypass descriptors or capability checks.
- File apps use `retro_fs`; they do not call filesystem APIs directly from UI
  behavior.

## Close Contract

`Ctrl+W` is handled globally as a close request for the active app window. The
app can accept immediately or defer through `can_close`.

Notepad uses this contract to display Save/Discard/Cancel without a nested event
loop. While close is deferred, the app keeps ownership of its state and window.
A successful save or explicit discard authorizes the next close path.

## Multiple Instances and Taskbar

Descriptors can opt into multiple instances. Notepad currently does so.

The desktop taskbar derives app state from the running-app table and can:

- launch a closed app;
- focus an existing app window;
- cycle to the next instance when the focused app has multiple windows.

The taskbar widget returns actions; desktop code performs lifecycle and Window
Manager operations.

## Built-In Apps

- `filemanager`: Linux/POSIX File Manager with keyboard viewport, hidden-file
  toggle, refresh, directory/text open, rename, new directory, and new file.
- `notepad`: UTF-8 text editor with selection, clipboard, undo/redo, Find, Word
  Wrap, Save/Save As, conflict reporting, and safe dirty close.
- `diagnostics`: read-only runtime diagnostics. It is not a shell or PTY.

Detailed behavior is documented in [BUILTIN_APPS.md](BUILTIN_APPS.md).

## App Categories

- Built-in apps: compiled and registered by the repository.
- Optional apps: feature-gated by capability or build profile.
- Unsupported apps: rejected deterministically at launch.

## Forbidden Patterns

- direct calls to `getch`, `wgetch`, `doupdate`, or backend polling;
- backend-native types in app/public headers;
- standalone or nested app event loops;
- direct renderer flushes from apps;
- filesystem access that bypasses `retro_fs`;
- silent ownership transfer or close behavior outside the descriptor contract.
