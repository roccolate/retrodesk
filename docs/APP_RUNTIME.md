# App Runtime Contract

## App Model

Each app is represented by a descriptor with:

- stable `app_id`,
- display name,
- required capability mask,
- lifecycle callbacks (`create`, `event`, `render`, `destroy`),
- default window geometry and flags.

## Lifecycle

1. Register descriptor.
2. Launch app instance if capabilities are satisfied.
3. Receive normalized events from runtime.
4. Append draw commands into the provided `DrawList`.
5. Destroy and release resources on close.

## Ownership Rules

- Runtime owns app instance lifecycle.
- App owns only its private state.
- App cannot call backend polling or frame flush APIs directly.

## App Categories

- Built-in apps: shipped in repository.
- Optional apps: feature-gated by capability or build profile.
- Unsupported apps: explicitly rejected at launch with clear reason.

## Forbidden Patterns

- direct calls to `getch`, `wgetch`, `doupdate`,
- direct use of backend-native types in app headers,
- standalone app event loops.
