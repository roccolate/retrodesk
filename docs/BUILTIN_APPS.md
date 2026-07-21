# Built-In Applications

RetroDesk currently ships three hosted applications. They run through the shared
app registry, window manager, event loop, render abstraction, and capability
checks.

## File Manager

The File Manager starts in `$HOME` when available and falls back to the current
directory. It uses the storage contract rather than calling filesystem APIs from
UI code.

Implemented workflows:

- bounded, sorted directory listing;
- directories before regular files;
- keyboard scrolling with retained selection;
- hidden files off by default with an `H` toggle;
- parent navigation and refresh;
- opening directories;
- opening regular text files in a new Notepad instance;
- file sizes and empty-directory state;
- creating empty files and directories;
- renaming files or directories without intentional overwrite.

Current exclusions:

- delete/trash;
- copy and move;
- recursive operations;
- previews and bookmarks;
- dual-pane workflows;
- arbitrary binary-file viewers.

## Notepad

Notepad is a UTF-8 text editor built on `TextBuffer` and the POSIX storage
adapter.

Implemented workflows:

- open regular text files from File Manager;
- native `File`, `Edit`, and `View` menus using the shared RetroDesk menu widget;
- safe `New` and `Open` commands that launch another Notepad instance rather
  than replacing unsaved work in the current document;
- a viewport that follows the hosted window size, with a line/column, UTF-8,
  wrap, and modified-state status row;
- UTF-8-safe cursor movement, insertion, Backspace, and Delete;
- display-cell-aware rendering for accented and wide characters;
- multiline selection with Shift+arrows;
- shared in-process clipboard with Select All, Copy, Cut, and Paste;
- bounded snapshot undo/redo: at most 100 states and 1 MiB per stack;
- Find with next-result wraparound;
- soft Word Wrap that leaves logical lines and saved bytes unchanged;
- Save and Save As;
- atomic writes with stale-version conflict detection;
- dirty-title marker;
- explicit Save / Discard / Cancel workflow when closing a dirty document.

Text positions are stored as UTF-8 byte offsets internally, but mutations always
normalize them to codepoint boundaries. Rendering converts those offsets to
terminal display cells.

Current exclusions:

- Replace and Replace All;
- system clipboard integration outside the RetroDesk process;
- rich text, syntax highlighting, tabs, and multiple encodings;
- normalization or accent-insensitive search;
- direct binary or very large file editing.

## Diagnostics

Diagnostics presents runtime/backend information. It is intentionally not a
terminal emulator and does not spawn a shell or PTY.

## App lifecycle behavior

- Apps are launched through descriptors registered in the app registry.
- Unsupported capabilities reject launch deterministically.
- App creation failures run the matching destroy path for partially acquired
  state.
- `Ctrl+W` requests close through the app lifecycle; apps can defer close to
  resolve dirty state.
- Multiple Notepad instances are supported.
- The taskbar can launch, focus, or cycle app instances.

## Platform scope

The complete File Manager and Notepad storage workflows are currently a
Linux/POSIX product slice. Windows builds and tests exercise portable runtime
code, but a native Windows storage adapter or explicit product gating is still
required before claiming equivalent file-app support.
