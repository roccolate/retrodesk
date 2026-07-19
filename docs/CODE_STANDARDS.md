# RetroDesk Roadmap — v1.0

## Code Standards

Status legend for roadmap checklists:

- `[DONE]`: implemented and covered by the current contract.
- `[PARTIAL]`: useful code exists, but release-quality UX, portability, or test
  coverage is incomplete.
- `[TODO]`: not implemented or not part of the current product surface.

Every automated session MUST follow these rules:

### Style
- C11, `-Wall -Wextra -Wpedantic -Werror` clean
- All public functions: null-check every pointer parameter
- No magic numbers — use `enum { }` or `#define` constants
- Functions under 60 lines — split if larger
- One responsibility per function
- Consistent naming: `module_verb_noun` (e.g. `text_input_handle_key`)

### Safety
- Every `malloc`/`calloc`/`realloc` must be null-checked
- Every `memmove`/`memcpy` must validate bounds first
- No buffer overflows — validate lengths before writing
- Null state before `free()` to prevent use-after-free
- `snprintf` over `sprintf`, always
- No `strncpy` without explicit null termination

### Architecture
- Widgets are standalone — no dependencies between widgets
- Widgets never call backend draw APIs — they produce DrawList commands
- Apps never poll input — they receive events from the desktop
- Platform details stay in `platform/` — nothing else includes `<curses.h>` directly
- New features include unit tests
- Tests must be runnable without a terminal (no initscr in tests)

### Commits
- Build + all tests pass before every commit
- One logical change per commit
- Commit message format: `type: description` (feat/fix/refactor/docs/test)
- Push after every successful commit
- Update ROADMAP.md marking items [DONE]

---

## Phase 1 — UI Toolkit

Foundation widgets that all apps will use.

- [DONE] 1.01 Text input widget (single-line, cursor, viewport scroll, shortcuts)
- [DONE] 1.02 Scrollable list widget (selection, scroll, item count display)
- [DONE] 1.03 Multi-line text buffer (line array, insert/delete, cursor row+col)
- [DONE] 1.04 Button widget ([OK] [Cancel], clickeable, keyboard focusable)
- [DONE] 1.05 Dialog system (message box, confirm, input prompt)
- [DONE] 1.06 Menu bar widget (File|Edit|View|Help with dropdown items)
- [DONE] 1.07 Progress bar widget (percentage, determinate/indeterminate)
- [DONE] 1.08 Tab widget (switchable views within a window)

## Phase 2 — Core Runtime Upgrades

Make the WM feel like a real desktop.

- [TODO] 2.01 Unicode box drawing (┌─┐│└─┘ instead of +-+|, with ASCII fallback)
- [TODO] 2.02 Window resize (drag right/bottom border to resize)
- [TODO] 2.03 Window minimize (button on titlebar, window hides)
- [TODO] 2.04 Window maximize (double-click titlebar or button, fills screen)
- [TODO] 2.05 Proper keybindings (Alt+F4 close, Alt+Tab cycle, replace bare letters)
- [TODO] 2.06 Window shadows (1-char shadow on right and bottom edges)
- [TODO] 2.07 Mouse cursor indicator (visible position marker)

## Phase 3 — Functional Apps

Apps that actually work. Each app uses Phase 1 widgets.

- [PARTIAL] 3.01 File Manager: real directory listing (POSIX preview; scroll-list integration still pending)
- [DONE] 3.02 File Manager: navigate dirs (Enter to open, Backspace/.. to go up)
- [TODO] 3.03 File Manager: file operations (create, delete, rename) with dialogs
- [PARTIAL] 3.04 File Manager: open regular files in Notepad through app runtime
- [DONE] 3.05 Notepad: multi-line editing with text buffer widget
- [PARTIAL] 3.06 Notepad: file open/save (resource-path open, Ctrl+S, F3 Save As; Ctrl+O/dialog pending)
- [TODO] 3.07 Notepad: word wrap, line numbers, status line (row:col)
- [TODO] 3.08 Terminal: PTY fork/exec shell (/bin/sh)
- [TODO] 3.09 Terminal: ANSI color output rendering in window
- [TODO] 3.10 Terminal: scroll history (scrollback buffer)
- [TODO] 3.11 Settings app: change theme at runtime, show keybindings
- [TODO] 3.12 About/Help: version, credits, keyboard shortcuts reference

## Phase 4 — Desktop Chrome

The desktop experience beyond windows.

- [TODO] 4.01 Taskbar: window list with app names, click to focus/restore
- [TODO] 4.02 Taskbar: clock on the right side
- [TODO] 4.03 Desktop background: configurable pattern or ASCII art
- [TODO] 4.04 Right-click context menu (on desktop and in apps)
- [TODO] 4.05 Start menu / improved launcher (categories, search filter)
- [TODO] 4.06 Notification toasts (transient messages for completed operations)

## Phase 5 — Polish & Release

Ship it.

- [TODO] 5.01 Config file (~/.retrodeskrc) for persistent preferences
- [TODO] 5.02 Window open/close animations (cascade reveal)
- [PARTIAL] 5.03 --version flag exists; version string in About pending
- [TODO] 5.04 README with GIF screenshots of the desktop in action
- [PARTIAL] 5.05 CI: GitHub Actions build+test matrix for Linux/Windows; green release evidence pending
- [TODO] 5.06 make install + packaging notes
- [TODO] 5.07 Man page (retrodesk.1)
- [TODO] 5.08 Final audit pass + v1.0 tag

## Bonus Apps (post-1.0 or if time permits)

- [ ] Calculator
- [ ] Clock/Calendar widget
- [ ] System Monitor (top-like)
- [ ] Hex Viewer
- [ ] Image Viewer (ASCII art rendering)

---

## Progress

| Phase | Total | Done | Partial | Remaining |
|-------|-------|------|---------|-----------|
| 1 — UI Toolkit | 8 | 8 | 0 | 0 |
| 2 — Core Runtime | 7 | 0 | 0 | 7 |
| 3 — Apps | 12 | 2 | 3 | 7 |
| 4 — Desktop Chrome | 6 | 0 | 0 | 6 |
| 5 — Polish & Release | 8 | 0 | 2 | 6 |
| **Total** | **41** | **10** | **5** | **26** |
