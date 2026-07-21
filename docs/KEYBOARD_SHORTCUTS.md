# Keyboard and Taskbar Reference

This document describes the shortcuts implemented on `main`. Terminal support can
vary by backend; unsupported modifier sequences degrade to their unmodified key
rather than being guessed.

## Global desktop shortcuts

| Key | Action |
| --- | --- |
| `F1` | Open the Launcher |
| `F2` | Open the Launcher; close it when it is already focused |
| `F6` | Focus the next window |
| `F9` | Enter move/resize mode for the active window |
| `Ctrl+W` | Request close of the active app window |
| `Ctrl+Q` | Quit RetroDesk |

Printable keys and ordinary `Tab` events are left to the focused application.
A dirty app may reject `Ctrl+W` temporarily and present its own close workflow.

## Move/resize mode

After pressing `F9`:

| Key | Action |
| --- | --- |
| Arrow keys | Move or resize the active window |
| `Tab` | Toggle between move and resize |
| `Esc` | Leave move/resize mode |

## Launcher

| Key | Action |
| --- | --- |
| `W` / `K` | Move selection up |
| `S` / `J` | Move selection down |
| `Enter` or `Space` | Execute the selected action |
| `Esc` or `Q` | Close the Launcher |
| `F2` | Close the Launcher |

## Taskbar

The bottom status widget operates as an interactive taskbar:

```text
[Apps] [Files*] [Notepad:2.] [Diag ]                         19:42:33
```

- `*` means the app owns the focused window.
- `.` means the app is running but not focused.
- A blank mark means no instance is running.
- `:N` is the number of running instances.
- Clicking `Apps` toggles the Launcher.
- Clicking a closed app launches it.
- Clicking a running app focuses it.
- Clicking an already focused app with multiple instances cycles to the next
  instance.
- Labels compact automatically on narrow terminals.

## File Manager

| Key | Action |
| --- | --- |
| Arrow keys or `J` / `K` | Move selection |
| `Page Up` / `Page Down` | Move by one page |
| `Home` / `End` | First or last entry |
| `Enter` | Open directory or regular text file |
| `Backspace` | Parent directory |
| `F2` | Rename selected file or directory |
| `F5` | Refresh |
| `F7` | Create directory |
| `F8` | Create empty file |
| `H` | Toggle hidden files |
| `Esc` | Close File Manager when no prompt is active |

Inside a rename/create prompt, `Enter` confirms and `Esc` cancels.

## Notepad

| Key | Action |
| --- | --- |
| Arrow keys | Move cursor |
| `Shift` + arrows | Extend or shrink selection |
| `Ctrl+A` | Select all |
| `Ctrl+C` | Copy selection |
| `Ctrl+X` | Cut selection |
| `Ctrl+V` | Paste, replacing the selection when present |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Ctrl+F` | Open Find; `Enter` selects the next result |
| `Ctrl+S` | Save, or enter Save As for an untitled document |
| `F3` | Save As |
| `F4` | Toggle soft Word Wrap |
| `Esc` | Cancel the active prompt/search or clear a selection |
| `Ctrl+W` | Close; dirty documents show Save/Discard/Cancel |

The close prompt accepts `S` to save, `D` to discard, and `Esc` to cancel.
Find is case-insensitive for ASCII and Latin letter case, but accents remain
significant: `árbol` matches `ÁRBOL`, not `ARBOL`.

## Terminal control keys

The curses backend remains in `cbreak()` mode, but releases editor control keys
from terminal flow control and signal handling where the host supports it. The
original terminal mode is restored during shutdown. RetroDesk does not switch to
permanent `raw()` mode.
