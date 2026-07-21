#!/usr/bin/env python3
"""Repair temporary Notepad selection materializer markers and escapes."""

from pathlib import Path

path = Path("scripts/apply_notepad_selection_clipboard.py")
text = path.read_text(encoding="utf-8")

old = """    (
        '''    if (text_buffer_has_selection(buf)) {
        return text_buffer_delete_selection(buf);
    }
    text_buffer_clamp_cursor(buf);
    TextLine *line = &buf->lines[buf->cursor_row];
''',
        '''    if (text_buffer_has_selection(buf)) {
        return text_buffer_delete_selection(buf);
    }
    text_buffer_clear_collapsed_selection(buf);
    text_buffer_clamp_cursor(buf);
    TextLine *line = &buf->lines[buf->cursor_row];
''',
        "delete collapsed selection",
    ),
"""

new = """    (
        '''bool text_buffer_delete_forward(TextBuffer *buf) {
    if (!buf || buf->line_count == 0) return false;
    if (text_buffer_has_selection(buf)) {
        return text_buffer_delete_selection(buf);
    }
    text_buffer_clamp_cursor(buf);
    TextLine *line = &buf->lines[buf->cursor_row];
''',
        '''bool text_buffer_delete_forward(TextBuffer *buf) {
    if (!buf || buf->line_count == 0) return false;
    if (text_buffer_has_selection(buf)) {
        return text_buffer_delete_selection(buf);
    }
    text_buffer_clear_collapsed_selection(buf);
    text_buffer_clamp_cursor(buf);
    TextLine *line = &buf->lines[buf->cursor_row];
''',
        "delete collapsed selection",
    ),
"""

count = text.count(old)
if count != 1:
    raise SystemExit(f"Delete materializer tuple: expected one marker, found {count}")
text = text.replace(old, new, 1)

for normal, raw, label in (
    ("shortcut_replacement = '''", "shortcut_replacement = r'''",
     "shortcut replacement raw literal"),
    ("old_escape = '''", "old_escape = r'''",
     "Escape marker raw literal"),
    ("new_escape = '''", "new_escape = r'''",
     "Escape replacement raw literal"),
):
    count = text.count(normal)
    if count != 1:
        raise SystemExit(f"{label}: expected one marker, found {count}")
    text = text.replace(normal, raw, 1)

path.write_text(text, encoding="utf-8")
