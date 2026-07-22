# Built-In Applications

> This document describes applications integrated on `main`. Proposed Notepad
> menu work in PR #24 is listed separately and is not part of the current product.

RetroDesk currently registers three hosted applications through the shared app
registry. They use the same Desktop event loop, Window Manager, DrawList render
contract, capability checks, lifecycle ownership, and platform-neutral storage
boundary.

## File Manager

File Manager starts from the user's home directory where the active adapter can
resolve it, otherwise from a deterministic fallback/current location. It does not
call POSIX, Win32, or DJGPP APIs directly from app/UI logic.

### Integrated workflows

- bounded deterministic directory listing;
- directories sorted before regular files and other kinds;
- keyboard viewport with visible retained selection;
- arrows and `J/K` navigation;
- Page Up/Down and Home/End;
- parent-directory navigation with Backspace;
- refresh with `F5`;
- hidden files disabled by default and toggled with `H`;
- directory open;
- regular validated-text open in a new Notepad instance;
- file-size and empty-directory display;
- rename with `F2` and no intentional overwrite;
- create directory with `F7`;
- create empty regular file with `F8`;
- deterministic validation of empty, `.`, `..`, slash, backslash, duplicate, and
  cancelled names;
- selection of a newly created or renamed item after a successful refresh.

### Storage behavior by platform

- POSIX uses native path/list/stat/create/mkdir/rename operations.
- Win32 uses the UTF-8/UTF-16 native adapter and supports Unicode filenames.
- DJGPP uses DOS path semantics and ASCII/DOS-oriented filenames while text
  content remains validated UTF-8.

Platform maturity still depends on interactive evidence; native adapter presence
is not by itself a stable-release claim.

### Current exclusions

- delete or trash/recovery;
- copy and move;
- recursive operations;
- progress/cancellation for long operations;
- previews, bookmarks, and dual-pane mode;
- arbitrary binary viewers;
- explicit symlink/reparse-point traversal workflows.

These exclusions are intentional safety boundaries. Destructive and recursive
features require overwrite, recovery, cancellation, partial-failure, link, and
cross-device policies first.

## Notepad

Notepad is a UTF-8 text editor built on `TextBuffer`, the Desktop clipboard
service, and `retro_fs`.

### Integrated workflows

- open regular validated text files from File Manager;
- multiple Notepad windows;
- codepoint-safe cursor movement, insertion, Backspace, and Delete;
- terminal-cell-aware horizontal/vertical rendering;
- multiline selection with Shift+arrows;
- Select All, Copy, Cut, and Paste;
- clipboard exchange among Notepad instances in the same Desktop;
- selection replacement by typing, Enter, Backspace, Delete, cut, or paste;
- bounded snapshot undo/redo with exact cursor restoration;
- maximum 100 states and 1 MiB per undo or redo stack;
- dirty state compared against the last successfully saved content;
- Find with next-result wraparound;
- ASCII and Latin-letter case folding with accents kept significant;
- soft Word Wrap over visual rows without changing logical text or saved bytes;
- Save and Save As;
- optimistic-concurrency conflict detection;
- recovery behavior on destruction where applicable;
- explicit Save/Discard/Cancel dirty close;
- transactional participation in global `Ctrl+Q` shutdown.

### Text model

Logical lines are stored as UTF-8 bytes. Cursor and selection columns are byte
offsets for storage/API compatibility, but every mutation normalizes them to
valid codepoint boundaries. Rendering translates byte ranges to terminal cells.

The editor does not claim Unicode normalization or complete width behavior for
all combining sequences, emoji, and ambiguous-width characters.

### Current presentation on `main`

The current editor exposes keyboard shortcuts and in-content prompts/status. It
does **not** yet include the native responsive File/Edit/View menu chrome proposed
in PR #24.

PR #24 proposes:

- `F11` app menus and reliable Alt mnemonics;
- File/Edit/View menus through the shared MenuBar widget;
- `Ctrl+N` and `Ctrl+O` opening separate windows;
- hosted-window responsive viewport/status chrome.

That branch is stacked on stale history and must be rebuilt before integration.

### Current exclusions

- Replace and Replace All;
- native system clipboard;
- mouse cursor placement and drag selection;
- wheel scrolling;
- Unicode normalization and accent-insensitive search;
- syntax highlighting, rich text, tabs, and multiple encodings;
- direct binary editing;
- memory-mapped or streaming very-large-file strategy.

## Diagnostics

Diagnostics presents read-only runtime information, including backend names,
capability state, theme, geometry, and degradation information.

Diagnostics is intentionally **not**:

- a shell;
- a PTY owner;
- a terminal emulator;
- an ANSI/CSI screen parser;
- a subprocess manager.

The optional app-service contract is preparation for future asynchronous apps,
not evidence that a terminal app already exists.

## Shared App Lifecycle

- Descriptors are registered in the app registry.
- Duplicate or unsupported descriptors are rejected deterministically.
- Required capability masks are checked before launch.
- App instance allocation and optional resource-path copies are runtime-owned.
- App windows are Window Manager-owned.
- Once `create` is invoked, a failed create runs matching `destroy` rollback.
- Apps append DrawList commands and never call renderer flush.
- Apps may request close; they do not destroy their own Window Manager window.
- `Ctrl+W` routes through app close policy.
- `Ctrl+Q` coordinates every app and commits only after all authorize.
- Apps with `on_service` receive bounded callbacks from the one Desktop loop.

## Window and Taskbar Behavior

The taskbar can:

- launch a closed built-in app;
- focus/raise a running background app;
- cycle multiple Notepad instances;
- minimize the focused app window;
- restore a minimized app window.

Ordinary app windows also support `F8` maximize/unmaximize and `F9` keyboard
move/resize. Fixed/modal desktop chrome rejects those operations.

## Platform Scope

| Profile | App/storage position |
| --- | --- |
| Linux/ncurses | Primary development/product profile with POSIX adapter |
| Windows/PDCurses | Native Win32 storage and automated app/storage tests integrated; current manual interaction evidence incomplete |
| DOS/DJGPP | Native reduced storage and automated DOSBox-X contract/smoke integrated; filenames and replacement semantics are reduced |
| Linux raw TTY/ANSI | App logic is shared; broader interactive terminal evidence incomplete |
| macOS | Expected POSIX path but currently unvalidated and unclaimed |

See [PROJECT_STATUS.md](PROJECT_STATUS.md), [STORAGE.md](STORAGE.md), and
[PORTABILITY.md](PORTABILITY.md) for the exact status and evidence boundaries.