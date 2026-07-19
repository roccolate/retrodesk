# RetroTUI Gap Map

`RetroTUI` is reference for desired behavior and known pitfalls.
It is not used as architecture template.

## Subsystem Gap Snapshot

| Subsystem | RetroTUI | RetroDesk current direction |
| --- | --- | --- |
| Desktop runtime | Mature, mixed patterns from rapid evolution | Rebuilding around explicit layered runtime |
| Window management | Multi-window behavior exists | Moving to dedicated `wm` ownership |
| App model | Broad app set | Formal descriptor/runtime exists; Linux app preview in progress |
| Input model | Practical but historically uneven on Linux TTY + gpm | Capability-driven, keyboard-first fallback policy |
| Render model | curses-heavy integration | Render abstraction with single flush point |
| Platform abstraction | Evolved organically | Explicit backend capabilities and contracts |
| Storage/files | Practical Python filesystem access | POSIX storage contract preview; native adapters pending |
| Plugins/advanced features | Present in base project | Deferred until foundation hardening |

## Port Strategy

1. Port behavior contracts first (window/focus/events), not implementation style.
2. Port app features only through runtime descriptors, storage adapters, and
   capability checks.
3. Keep Linux TTY fallback explicit to avoid repeating unreliable drag/click behavior.

## Known Risk Patterns to Avoid

- nested modal loops,
- backend details leaking into all modules,
- optimistic assumptions that mouse semantics are identical across terminals,
- POSIX filesystem assumptions leaking into profiles that do not support them.
