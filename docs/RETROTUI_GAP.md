# RetroTUI Gap Map

`RetroTUI` is reference for desired behavior and known pitfalls.
It is not used as architecture template.

## Subsystem Gap Snapshot

| Subsystem | RetroTUI | RetroDesk current direction |
| --- | --- | --- |
| Desktop runtime | Mature, mixed patterns from rapid evolution | Rebuilding around explicit layered runtime |
| Window management | Multi-window behavior exists | Moving to dedicated `wm` ownership |
| App model | Broad app set | Formal descriptor/runtime being introduced |
| Input model | Practical but historically uneven on Linux TTY + gpm | Capability-driven, keyboard-first fallback policy |
| Render model | curses-heavy integration | Render abstraction with single flush point |
| Platform abstraction | Evolved organically | Explicit backend capabilities and contracts |
| Plugins/advanced features | Present in base project | Deferred until foundation hardening |

## Port Strategy

1. Port behavior contracts first (window/focus/events), not implementation style.
2. Port app features only after runtime and capability model are stable.
3. Keep Linux TTY fallback explicit to avoid repeating unreliable drag/click behavior.

## Known Risk Patterns to Avoid

- nested modal loops,
- backend details leaking into all modules,
- optimistic assumptions that mouse semantics are identical across terminals.
