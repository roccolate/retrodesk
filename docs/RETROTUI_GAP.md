# RetroTUI Gap Map

`RetroTUI` is a reference for desired behavior and known pitfalls. It is not
used as an architecture template.

## Subsystem Gap Snapshot

| Subsystem | RetroTUI | RetroDesk current state |
| --- | --- | --- |
| Desktop runtime | Mature, mixed patterns from rapid evolution | Explicit layered runtime with `core`, `wm`, `app`, `apps`, `render`, `platform`, and `ui` |
| Window management | Multi-window behavior exists | Dedicated `wm` owns window collection, focus, z-order, close lifecycle, modal policy, keyboard movement, and capability-gated drag |
| App model | Broad app set | Formal app descriptors/runtime exist; built-in apps are currently placeholder workflows |
| Input model | Practical but historically uneven on Linux TTY + gpm | Capability-driven, keyboard-first fallback policy with curses/PDCurses and raw TTY input backends |
| Render model | curses-heavy integration | Render abstraction with DrawList, curses backend, ANSI diff renderer, and single flush policy |
| Platform abstraction | Evolved organically | Explicit backend capabilities and platform feature contracts |
| UI widgets | Broad, organic surface | Foundation widget set exists: text input, text buffer, scroll list, button, dialog, menu bar, progress bar, tab, status bar |
| Plugins/advanced features | Present in base project | Deferred until foundation hardening and app ABI maturity |

## Port Strategy

1. Port behavior contracts first (window/focus/events), not implementation
   style.
2. Keep all features behind the current runtime contracts: normalized events,
   DrawList rendering, app descriptors, and capability checks.
3. Port app features only after runtime and capability model are stable.
4. Keep Linux TTY fallback explicit to avoid repeating unreliable drag/click
   behavior.
5. Treat any feature that requires backend-native behavior as a platform/render
   concern, not an app/widget shortcut.

## Immediate Feature Gaps

- File Manager needs real directory listing and open-file behavior.
- Notepad needs cursor movement, editing semantics, open/save, and larger buffer
  management.
- Terminal needs either a real PTY/shell integration or a clearly scoped
  diagnostics-console identity.
- Settings app is not currently implemented.
- External plugin compatibility is intentionally deferred.

## Known Risk Patterns to Avoid

- nested modal loops,
- backend details leaking into all modules,
- optimistic assumptions that mouse semantics are identical across terminals,
- growing app features by bypassing app runtime descriptors,
- adding UI widgets without tests or docs.
