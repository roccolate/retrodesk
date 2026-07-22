# Keyboard, Mouse, and Taskbar Reference

> Describes behavior integrated on `main`. Backend support can vary; RetroDesk
> degrades unsupported modifiers or pointer features rather than guessing.

## Routing Rules

Global commands are limited to unambiguous function keys and control chords.
Printable text, ordinary `Tab`, and unclaimed function keys are delivered to the
focused application.

Important consequences:

- `F2` remains app-local for File Manager Rename;
- `F4` reaches Notepad Word Wrap;
- `F10` is the global Launcher key;
- proposed Notepad `F11` menus from PR #24 are not integrated on `main`;
- unsupported modified-key sequences degrade to deterministic unmodified behavior.

## Global Desktop Shortcuts

| Key | Action |
| --- | --- |
| `F1` | Open the Launcher |
| `F10` | Open the Launcher; close it when the Launcher is focused |
| `F6` | Focus the next eligible visible window |
| `F8` | Maximize or restore the active ordinary window |
| `F9` | Enter keyboard MOVE mode for the active ordinary window |
| `Ctrl+W` | Request close of the active app window |
| `Ctrl+Q` | Begin coordinated shutdown; dirty apps may Save, Discard, or Cancel |

A dirty app may defer `Ctrl+W`. Global shutdown is transactional: applications
remain alive until every running app authorizes the operation. Cancelling any
deferred close resets the negotiation and destroys no app window.

## Window Focus, Minimize, and Maximize

### Focus

- `F6` skips minimized and ineligible fixed/modal windows.
- Clicking an ordinary window focuses and raises it according to modal policy.
- Explicitly focusing a minimized app restores it first.

### Maximize

- `F8` toggles maximize/unmaximize.
- Double-clicking the active title bar performs the same toggle when the backend
  reports double click.
- Maximized windows cannot be moved/resized until restored.
- Fixed and modal windows reject maximize.

### Keyboard move/resize mode

Press `F9` while an ordinary, non-maximized app window is active.

| Key | Action |
| --- | --- |
| Arrow keys | Move or resize by one unit |
| `Tab` | Toggle between MOVE and RESIZE |
| `Enter` | Finish the operation |
| `F9` | Finish the operation |
| `Esc` | Finish/cancel the operation mode |

The active frame uses the operation theme style and a HUD above the taskbar shows
the mode, live `width x height @ x,y` geometry, and available controls. Changing
focus automatically ends the mode. Attempting `F9` on a fixed/no window or a
maximized window produces a brief explanatory HUD instead of a stuck mode.

## Launcher

The Launcher is a modal Start-menu-style surface above the bottom-left taskbar.

| Key | Action |
| --- | --- |
| Up / `W` / `K` | Move selection up |
| Down / `S` / `J` | Move selection down |
| `Home` | First action |
| `End` | Last action |
| `Enter` / Space | Execute selected action |
| `F`, `N`, `D`, `X` | Direct accelerator for the matching Files, Notepad, Diagnostics, or Close action |
| `Esc` / `Q` | Close the Launcher |
| `F10` | Close the Launcher |

Mouse movement/press/click can select Launcher rows when the backend provides
reliable pointer events. A click activates the selected row.

## Taskbar

The bottom status widget renders a themed taskbar. The old `*`/`.` text markers
are no longer the visual contract.

Typical wide layout:

```text
Apps | Files | Notepad x2 | Diag                         19:42:33
```

The actual colors/reverse/bold treatment depend on XP, Hacker, Amiga, or Win31.

### Mouse behavior

- Click Apps: toggle Launcher.
- Click a closed app: launch it.
- Click a running background app: focus and raise it.
- Click the focused app: minimize its active window.
- Click a minimized app: restore, focus, and raise it.
- Click an app with multiple instances: cycle deterministically.
- Clock and separator regions are not app buttons.

Full labels switch to compact labels when necessary. The current catalog contains
Files, Notepad, and Diagnostics. A general overflow menu for a larger catalog is
not yet implemented.

## File Manager

| Key | Action |
| --- | --- |
| Up/Down or `J`/`K` | Move selection |
| Page Up/Page Down | Move by one viewport page |
| `Home` / `End` | First or last entry |
| `Enter` | Open selected directory or regular validated text file |
| Backspace | Parent directory |
| `F2` | Rename selected file or directory |
| `F5` | Refresh while retaining selection where possible |
| `F7` | Create directory |
| `F8` | Create empty file |
| `H` | Toggle hidden files |
| `Esc` | Cancel active prompt, otherwise close File Manager |

Inside a rename/create prompt:

- `Enter` confirms;
- `Esc` cancels;
- invalid or conflicting names keep the prompt active and preserve existing data.

## Notepad

| Key | Action |
| --- | --- |
| Arrow keys | Move cursor |
| Shift + arrows | Extend or shrink selection |
| `Ctrl+A` | Select all |
| `Ctrl+C` | Copy selection to Desktop clipboard |
| `Ctrl+X` | Cut selection as one undoable edit |
| `Ctrl+V` | Paste as one undoable edit, replacing selection |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Ctrl+F` | Open Find |
| `Enter` in Find | Select next result and wrap when needed |
| `Ctrl+S` | Save, or enter Save As for untitled document |
| `F3` | Save As |
| `F4` | Toggle soft Word Wrap |
| `Esc` | Cancel prompt/search or clear selection |
| `Ctrl+W` | Request close; dirty document shows Save/Discard/Cancel |

Dirty close prompt:

- `S`: save before closing;
- `D`: discard and close;
- `Esc`: cancel and preserve document.

Find is case-insensitive for ASCII and supported Latin letter case, but accents
remain significant: `árbol` matches `ÁRBOL`, not `ARBOL`. No Unicode
normalization is performed.

`F11`, `Ctrl+N`, `Ctrl+O`, and native File/Edit/View menus belong to open PR #24
and are not current `main` shortcuts.

## Terminal Control Keys

The curses backend uses `cbreak()` rather than permanent `raw()` mode. On POSIX it
selectively releases editor controls from terminal flow control/signal handling
where available so Copy/Paste/Undo/Redo reach apps. The original terminal mode is
restored during normal shutdown and initialization rollback.

A process killed externally may not execute cleanup; `stty sane` or `reset` may
be required.

## Pointer and Modifier Degradation

- Mouse, drag, right click, double click, and resize events are capability-gated.
- Linux `TERM=linux` remains keyboard-first.
- Raw-TTY decoding supports tested xterm-style modified navigation sequences.
- Unknown or malformed sequences are consumed/rejected deterministically and do
  not leak bytes into app text.
- Advanced pointer semantics are not assumed across all curses/PDCurses hosts.