# RetroTUI Gap Map

`RetroTUI` is the behavior reference and risk catalog for RetroDesk. It is not
the architecture template. Features are ported only when they fit the C runtime,
ownership rules, platform contracts, and test strategy.

## Current Parity Snapshot

| Subsystem | RetroTUI reference | RetroDesk status |
| --- | --- | --- |
| Desktop runtime | Broad and mature, with patterns accumulated during rapid evolution | Explicit layered runtime with one event loop and one frame flush |
| Window management | Multi-window behavior | Dedicated `wm` ownership, focus, z-order, drag, move/resize, modal routing, and close lifecycle |
| Taskbar/panel | App launcher and running-state panel | Interactive bottom taskbar with Apps, focus/running marks, instance counts, clock, and mouse hit testing |
| App model | Broad application set | Descriptor/runtime model with capability checks and rollback-aware lifecycle |
| Input | Practical but historically uneven across terminals | Normalized key/mouse events, modifier decoding, keyboard-first fallback, and restored terminal state |
| Rendering | curses-heavy integration | Draw-list abstraction with curses/ANSI backends and single flush point |
| Storage | Direct Python filesystem access | POSIX storage contract with UTF-8 validation, creation, rename, atomic writes, and conflicts |
| File Manager | Mature browsing and file workflows | Navigation, viewport, hidden files, refresh, text open, rename, new directory, and new file |
| Notepad | Full practical editor workflow | UTF-8 editing, selection, clipboard, undo/redo, Find, Word Wrap, save, and safe dirty close |
| Plugins/advanced apps | Present in the original project | Deferred until runtime and platform contracts mature |

## Completed Behavior Ports

- taskbar app-state model from the ArmoniOS/RetroTUI family of behavior;
- File Manager keyboard viewport and basic non-destructive mutations;
- Notepad Save/Discard/Cancel close flow;
- UTF-8-safe editor core and storage path;
- bounded undo/redo;
- shifted navigation and terminal control-key delivery;
- shared in-process clipboard and multiline selection;
- Find with result wraparound;
- soft Word Wrap over visual terminal rows.

## Remaining High-Value Gaps

### File Manager

- delete/trash;
- copy and move;
- recursive operations;
- previews, bookmarks, and dual-pane workflows;
- broader non-text file handling.

### Notepad

- Replace and Replace All;
- native system clipboard integration;
- mouse cursor placement and drag selection;
- richer Unicode normalization/width validation;
- syntax highlighting, tabs, and multiple encodings;
- very-large-file strategy.

### Desktop/runtime

- minimize and maximize contracts;
- session persistence and configuration UI;
- packaging and release artifacts;
- broader manual terminal/backend evidence;
- stable external extension/plugin ABI.

## Port Strategy

1. Port behavior contracts before implementation details.
2. Keep app features behind runtime descriptors, storage adapters, and capability
   checks.
3. Preserve keyboard-first operation on Linux virtual consoles.
4. Add lower-level and app-level regression tests before considering a port
   complete.
5. Refuse platform claims that are not backed by the relevant adapter and smoke
   evidence.

## Risk Patterns to Avoid

- nested modal loops;
- backend details leaking into apps or public UI headers;
- byte-at-a-time text editing that splits UTF-8;
- optimistic assumptions that terminal mouse/modifier semantics are universal;
- POSIX filesystem assumptions leaking into unsupported profiles;
- silent overwrite or stale-file save behavior;
- global hotkeys stealing printable text from focused applications;
- feature parity that bypasses lifecycle, ownership, or portability contracts.
