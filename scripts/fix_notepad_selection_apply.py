#!/usr/bin/env python3
"""Make the temporary Notepad selection materializer target Delete precisely."""

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

path.write_text(text.replace(old, new, 1), encoding="utf-8")
