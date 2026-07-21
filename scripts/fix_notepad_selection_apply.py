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

old_cmake = """test_source_marker = "        src/core/desktop.c\\n        src/core/utf8.c\\n"
test_source_replacement = (
    "        src/core/desktop.c\\n"
    "        src/core/clipboard.c\\n"
    "        src/core/utf8.c\\n"
)
if text.count("        src/core/clipboard.c\\n") < 2:
    count = text.count(test_source_marker)
    if count != 2:
        raise SystemExit(
            f"clipboard runtime test sources: expected two markers, found {count}"
        )
    text = text.replace(test_source_marker, test_source_replacement)
"""

new_cmake = """test_source_marker = "        src/core/desktop.c\\n"
test_source_replacement = (
    "        src/core/desktop.c\\n"
    "        src/core/clipboard.c\\n"
)
for target_name in ("desktop_runtime_test", "retrocore_event_fixture_test"):
    target_marker = f"add_executable({target_name}\\n"
    target_start = text.find(target_marker)
    if target_start < 0:
        raise SystemExit(f"clipboard runtime source target missing: {target_name}")
    target_end = text.find("\\n    )", target_start)
    if target_end < 0:
        raise SystemExit(f"clipboard runtime source target unterminated: {target_name}")
    target_block = text[target_start:target_end]
    if "src/core/clipboard.c" not in target_block:
        count = target_block.count(test_source_marker)
        if count != 1:
            raise SystemExit(
                f"clipboard runtime source marker in {target_name}: "
                f"expected one, found {count}"
            )
        replaced = target_block.replace(
            test_source_marker, test_source_replacement, 1
        )
        text = text[:target_start] + replaced + text[target_end:]
"""

count = text.count(old_cmake)
if count != 1:
    raise SystemExit(f"CMake runtime source block: expected one marker, found {count}")
text = text.replace(old_cmake, new_cmake, 1)

path.write_text(text, encoding="utf-8")
