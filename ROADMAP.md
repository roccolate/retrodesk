# RetroDesk Roadmap

## Status tracking for automated development sessions.
## Mark items [DONE] as they're completed. Pick the next [TODO] item.

### Phase 1 — UI Widgets
- [DONE] 1.1 Text input widget (single-line, cursor, scroll, shortcuts)
- [TODO] 1.2 Scrollable list widget (selection, scroll, search filter)
- [TODO] 1.3 Menu bar widget (top bar with dropdown menus)
- [TODO] 1.4 Dialog/MessageBox widget (confirm, alert, input prompt)
- [TODO] 1.5 Multi-line text buffer (line array, insert/delete lines, scroll)

### Phase 2 — Functional Apps
- [TODO] 2.1 Notepad: multi-line editing with text buffer widget
- [TODO] 2.2 Notepad: file open/save (Ctrl+O, Ctrl+S)
- [TODO] 2.3 File Manager: real directory listing with scrollable list
- [TODO] 2.4 File Manager: navigate dirs (Enter), go up (..)
- [TODO] 2.5 File Manager: file operations (create, delete, rename)
- [TODO] 2.6 Terminal: PTY fork/exec with shell
- [TODO] 2.7 Terminal: ANSI color output rendering

### Phase 3 — Desktop Chrome
- [TODO] 3.1 Window resize (drag borders/corners)
- [TODO] 3.2 Desktop background (ASCII art or pattern fill)
- [TODO] 3.3 Taskbar (window list in statusbar, click to focus)
- [TODO] 3.4 Right-click context menu
- [TODO] 3.5 Desktop icons (app launchers on background)

### Phase 4 — Extra Apps
- [TODO] 4.1 Clock widget (real-time display)
- [TODO] 4.2 Calculator app
- [TODO] 4.3 Settings app (theme, keybindings at runtime)
- [TODO] 4.4 Help/About viewer
- [TODO] 4.5 Image viewer (ASCII art rendering)

## Notes
- Each automated session picks the next [TODO] item, implements it with tests, commits, and pushes.
- Bug review sessions scan all src/ files for issues and fix them.
- Update this file after each task.
