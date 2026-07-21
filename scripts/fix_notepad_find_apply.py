#!/usr/bin/env python3
"""Repair escapes and fixtures in the temporary Notepad find materializer."""

from pathlib import Path

path = Path("scripts/apply_notepad_find.py")
text = path.read_text(encoding="utf-8")

needle = "state->error[0] = '\\0';"
position = text.find(needle)
while position >= 0:
    opener = text.rfind('    """', 0, position)
    raw_opener = text.rfind('    r"""', 0, position)
    closer = text.rfind('""",', 0, position)
    if opener > raw_opener and opener > closer:
        text = text[:opener] + '    r"""' + text[opener + 7:]
        position += 1
    position = text.find(needle, position + len(needle))

fixture_old = 'TEST_REQUIRE(text_buffer_set_text(tb, "Árbol\\nniño árbol\\nARBOL"));'
fixture_new = 'TEST_REQUIRE(text_buffer_set_text(tb, "Árbol\\nniño árbol\\nÁRBOL"));'
if text.count(fixture_old) != 1:
    raise SystemExit("uppercase accented TextBuffer fixture marker missing")
text = text.replace(fixture_old, fixture_new, 1)

runtime_old = """    app_handle_event(&instance, &enter);
    type_notepad_char(&instance, 'A');
    type_notepad_char(&instance, 'R');
"""
runtime_new = """    app_handle_event(&instance, &enter);
    type_notepad_codepoint(&instance, 0x00C1u);
    type_notepad_char(&instance, 'R');
"""
if text.count(runtime_old) != 1:
    raise SystemExit("uppercase accented runtime fixture marker missing")
text = text.replace(runtime_old, runtime_new, 1)

if text.count('"ARBOL"') != 1:
    raise SystemExit("uppercase accented clipboard expectation marker missing")
text = text.replace('"ARBOL"', '"ÁRBOL"', 1)

unit_offset_old = """    TEST_REQUIRE(match.row == 2);
    TEST_REQUIRE(match.start_col == 0);
    TEST_REQUIRE(match.end_col == 5);
"""
unit_offset_new = """    TEST_REQUIRE(match.row == 2);
    TEST_REQUIRE(match.start_col == 0);
    TEST_REQUIRE(match.end_col == 6);
"""
if text.count(unit_offset_old) != 1:
    raise SystemExit("uppercase accented unit offset marker missing")
text = text.replace(unit_offset_old, unit_offset_new, 1)

runtime_offset_old = """    TEST_REQUIRE(notepad_cursor_row_for_test(&instance) == 2);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 5);
"""
runtime_offset_new = """    TEST_REQUIRE(notepad_cursor_row_for_test(&instance) == 2);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 6);
"""
if text.count(runtime_offset_old) != 1:
    raise SystemExit("uppercase accented runtime offset marker missing")
text = text.replace(runtime_offset_old, runtime_offset_new, 1)

path.write_text(text, encoding="utf-8")
