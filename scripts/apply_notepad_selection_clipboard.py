#!/usr/bin/env python3
"""Connect TextBuffer selection and the shared clipboard to Notepad."""

from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one marker, found {count}")
    return text.replace(old, new, 1)


# ---------------------------------------------------------------------------
# TextBuffer collapsed-selection invariant
# ---------------------------------------------------------------------------

path = Path("src/ui/text_buffer.c")
text = path.read_text(encoding="utf-8")

helper = '''static void text_buffer_clear_collapsed_selection(TextBuffer *buf) {
    if (buf && buf->selection_active && !text_buffer_has_selection(buf)) {
        text_buffer_clear_selection(buf);
    }
}

'''
if "text_buffer_clear_collapsed_selection" not in text:
    marker = "void text_buffer_select_all(TextBuffer *buf) {\n"
    text = replace_once(text, marker, helper + marker,
                        "collapsed-selection helper")

for old, new, label in [
    (
        '''    if (text_buffer_has_selection(buf)) {
        return text_buffer_insert_text(buf, encoded, encoded_len);
    }

    TextLine *line = &buf->lines[buf->cursor_row];
''',
        '''    if (text_buffer_has_selection(buf)) {
        return text_buffer_insert_text(buf, encoded, encoded_len);
    }
    text_buffer_clear_collapsed_selection(buf);

    TextLine *line = &buf->lines[buf->cursor_row];
''',
        "codepoint collapsed selection",
    ),
    (
        '''    if (text_buffer_has_selection(buf)) {
        return text_buffer_insert_text(buf, "\\n", 1);
    }
    text_buffer_clamp_cursor(buf);
''',
        '''    if (text_buffer_has_selection(buf)) {
        return text_buffer_insert_text(buf, "\\n", 1);
    }
    text_buffer_clear_collapsed_selection(buf);
    text_buffer_clamp_cursor(buf);
''',
        "newline collapsed selection",
    ),
    (
        '''    if (text_buffer_has_selection(buf)) {
        return text_buffer_delete_selection(buf);
    }
    text_buffer_clamp_cursor(buf);

    if (buf->cursor_col > 0) {
''',
        '''    if (text_buffer_has_selection(buf)) {
        return text_buffer_delete_selection(buf);
    }
    text_buffer_clear_collapsed_selection(buf);
    text_buffer_clamp_cursor(buf);

    if (buf->cursor_col > 0) {
''',
        "backspace collapsed selection",
    ),
    (
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
]:
    if new not in text:
        text = replace_once(text, old, new, label)

path.write_text(text, encoding="utf-8")


# ---------------------------------------------------------------------------
# Notepad integration
# ---------------------------------------------------------------------------

path = Path("src/apps/notepad_app.c")
text = path.read_text(encoding="utf-8")

if '#include "core/clipboard.h"' not in text:
    text = replace_once(
        text,
        '#include "core/key_chord.h"\n',
        '#include "core/clipboard.h"\n#include "core/key_chord.h"\n',
        "Notepad clipboard include",
    )

clipboard_helpers = r'''static void np_finish_manual_edit(NotepadState *state,
                                  NotepadHistoryEntry *before,
                                  bool captured, bool changed) {
    if (!state || !before) return;
    if (changed) {
        if (captured) {
            np_history_stack_push(&state->undo, before);
            np_history_stack_clear(&state->redo);
        }
        state->force_close = false;
        state->error[0] = '\0';
        np_refresh_dirty(state);
    }
    np_history_entry_destroy(before);
}

static bool np_copy_selection(NotepadState *state) {
    if (!state || !text_buffer_has_selection(state->buffer)) return false;
    size_t length = 0;
    char *selected = text_buffer_selected_text(state->buffer, &length);
    if (!selected) return false;
    bool copied = retro_clipboard_set_text(selected, length);
    free(selected);
    if (copied) {
        state->error[0] = '\0';
    } else {
        snprintf(state->error, sizeof(state->error),
                 "Clipboard: unable to copy selection");
    }
    return copied;
}

static void np_cut_selection(NotepadState *state) {
    if (!state || !np_copy_selection(state)) return;
    NotepadHistoryEntry before = {0};
    bool captured = np_history_capture(state, &before);
    if (!captured) np_clear_history(state);
    bool changed = text_buffer_delete_selection(state->buffer);
    np_finish_manual_edit(state, &before, captured, changed);
}

static void np_paste_clipboard(NotepadState *state) {
    if (!state || !retro_clipboard_has_text()) return;
    size_t length = 0;
    const char *clipboard = retro_clipboard_text(&length);
    NotepadHistoryEntry before = {0};
    bool captured = np_history_capture(state, &before);
    if (!captured) np_clear_history(state);
    bool changed = text_buffer_insert_text(state->buffer,
                                           clipboard, length);
    np_finish_manual_edit(state, &before, captured, changed);
}

'''
if "static bool np_copy_selection" not in text:
    marker = "static bool np_save(NotepadState *state) {\n"
    text = replace_once(text, marker, clipboard_helpers + marker,
                        "Notepad clipboard helpers")

shortcut_block = '''    if (key->key_code == RETRO_KEY_CTRL_Z) {
        np_undo(state);
        return;
    }
'''
shortcut_replacement = '''    if (key->key_code == RETRO_KEY_CTRL_A) {
        text_buffer_select_all(state->buffer);
        state->error[0] = '\0';
        return;
    }

    if (key->key_code == RETRO_KEY_CTRL_C) {
        (void)np_copy_selection(state);
        return;
    }

    if (key->key_code == RETRO_KEY_CTRL_X) {
        np_cut_selection(state);
        return;
    }

    if (key->key_code == RETRO_KEY_CTRL_V) {
        np_paste_clipboard(state);
        return;
    }

''' + shortcut_block
if "np_paste_clipboard(state);" not in text:
    text = replace_once(text, shortcut_block, shortcut_replacement,
                        "Notepad clipboard shortcuts")

old_escape = '''    if (key->key_code == RETRO_KEY_ESC) {
        state->error[0] = '\0';
        return;
    }
'''
new_escape = '''    if (key->key_code == RETRO_KEY_ESC) {
        if (text_buffer_has_selection(state->buffer)) {
            text_buffer_clear_selection(state->buffer);
        }
        state->error[0] = '\0';
        return;
    }
'''
if new_escape not in text:
    text = replace_once(text, old_escape, new_escape,
                        "Notepad Escape selection")

old_styles = '''    const RenderStyle *accent = &theme->shell_accent;
    const RenderStyle *cursor = &theme->launcher_selected;
    char title[192];
'''
new_styles = '''    const RenderStyle *accent = &theme->shell_accent;
    const RenderStyle *cursor = &theme->launcher_selected;
    RenderStyle selection = theme->launcher_selected;
    selection.bold = false;
    char title[192];
'''
if new_styles not in text:
    text = replace_once(text, old_styles, new_styles,
                        "Notepad selection style")

old_hint = '''    draw_list_text(draw_list, 2, 2,
                   "Ctrl+S save | F3 Save As | Ctrl+Z/Y undo/redo | Ctrl+W close",
                   text);

'''
new_hint = '''    draw_list_text(draw_list, 2, 2,
                   "Ctrl+S save | F3 Save As | Ctrl+Z/Y undo/redo | Ctrl+W close",
                   text);
    if (!state->save_as && !state->close_prompt) {
        draw_list_text(draw_list, 3, 2,
                       "Shift+arrows select | Ctrl+A/C/X/V all/copy/cut/paste",
                       text);
    }

'''
if new_hint not in text:
    text = replace_once(text, old_hint, new_hint,
                        "Notepad selection hint")

old_render = '''        text_buffer_render(state->buffer, draw_list, 4, 2, 11, 64,
                           text, cursor);
'''
new_render = '''        text_buffer_render_with_selection(
            state->buffer, draw_list, 4, 2, 11, 64,
            text, cursor, &selection);
'''
render_count = text.count(old_render)
if render_count:
    if render_count != 2:
        raise SystemExit(
            f"Notepad selection render: expected two markers, found {render_count}"
        )
    text = text.replace(old_render, new_render)

path.write_text(text, encoding="utf-8")


# ---------------------------------------------------------------------------
# CMake + DJGPP source manifests
# ---------------------------------------------------------------------------

path = Path("CMakeLists.txt")
text = path.read_text(encoding="utf-8")

source_marker = "    src/core/cli.c\n    src/core/desktop.c\n"
source_replacement = (
    "    src/core/cli.c\n"
    "    src/core/clipboard.c\n"
    "    src/core/desktop.c\n"
)
if "    src/core/clipboard.c\n" not in text[:4000]:
    count = text.count(source_marker)
    if count != 2:
        raise SystemExit(
            f"clipboard product sources: expected two markers, found {count}"
        )
    text = text.replace(source_marker, source_replacement)

test_source_marker = "        src/core/desktop.c\n        src/core/utf8.c\n"
test_source_replacement = (
    "        src/core/desktop.c\n"
    "        src/core/clipboard.c\n"
    "        src/core/utf8.c\n"
)
if text.count("        src/core/clipboard.c\n") < 2:
    count = text.count(test_source_marker)
    if count != 2:
        raise SystemExit(
            f"clipboard runtime test sources: expected two markers, found {count}"
        )
    text = text.replace(test_source_marker, test_source_replacement)

clipboard_target = '''        add_executable(clipboard_test
            tests/clipboard_test.c
            src/core/clipboard.c
            src/core/utf8.c
        )
        target_include_directories(clipboard_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
        add_test(NAME clipboard_test COMMAND clipboard_test)

'''
if "add_executable(clipboard_test" not in text:
    marker = '''        add_executable(app_registry_test
'''
    text = replace_once(text, marker, clipboard_target + marker,
                        "clipboard test target")

if "        clipboard_test\n" not in text:
    text = replace_once(
        text,
        "        utf8_test\n        app_registry_test\n",
        "        utf8_test\n        clipboard_test\n        app_registry_test\n",
        "clipboard test catalog",
    )

path.write_text(text, encoding="utf-8")

path = Path("Makefile.djgpp")
text = path.read_text(encoding="utf-8")
if "\tsrc\\core\\clipboard.c \\\n" not in text:
    text = replace_once(
        text,
        "\tsrc\\core\\cli.c \\\n\tsrc\\core\\desktop.c \\\n",
        "\tsrc\\core\\cli.c \\\n"
        "\tsrc\\core\\clipboard.c \\\n"
        "\tsrc\\core\\desktop.c \\\n",
        "DJGPP clipboard source",
    )
path.write_text(text, encoding="utf-8")


# ---------------------------------------------------------------------------
# TextBuffer tests
# ---------------------------------------------------------------------------

path = Path("tests/text_buffer_test.c")
text = path.read_text(encoding="utf-8")

modifier_helper = r'''static RetroKeyEvent key_modified(int code,
                                  unsigned int modifiers) {
    RetroKeyEvent key = key_code(code);
    key.modifiers = modifiers;
    return key;
}

'''
if "static RetroKeyEvent key_modified" not in text:
    marker = "static void test_create_destroy(void) {\n"
    text = replace_once(text, marker, modifier_helper + marker,
                        "TextBuffer modifier helper")

selection_tests = r'''static void test_utf8_shift_selection_and_replacement(void) {
    TextBuffer *tb = text_buffer_create();
    TEST_REQUIRE(text_buffer_set_text(tb, "niño"));
    text_buffer_set_cursor(tb, 0, 2);

    RetroKeyEvent shift_right = key_modified(
        RETRO_KEY_RIGHT, RETRO_MOD_SHIFT);
    TEST_REQUIRE(text_buffer_handle_key(tb, &shift_right));
    TEST_REQUIRE(text_buffer_has_selection(tb));

    size_t length = 0;
    char *selected = text_buffer_selected_text(tb, &length);
    TEST_REQUIRE(selected != NULL);
    TEST_REQUIRE(length == 2);
    TEST_REQUIRE(strcmp(selected, "ñ") == 0);
    free(selected);

    RetroKeyEvent replacement = key_char('x');
    TEST_REQUIRE(text_buffer_handle_key(tb, &replacement));
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "nixo") == 0);
    TEST_REQUIRE(!text_buffer_has_selection(tb));
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 3);

    text_buffer_destroy(tb);
    printf("  PASS: utf8_shift_selection_and_replacement\n");
}

static void test_multiline_selection_delete(void) {
    TextBuffer *tb = text_buffer_create();
    TEST_REQUIRE(text_buffer_set_text(tb, "uno\nniño"));
    text_buffer_set_cursor(tb, 0, 1);

    RetroKeyEvent shift_down = key_modified(
        RETRO_KEY_DOWN, RETRO_MOD_SHIFT);
    TEST_REQUIRE(text_buffer_handle_key(tb, &shift_down));
    TEST_REQUIRE(text_buffer_has_selection(tb));

    size_t length = 0;
    char *selected = text_buffer_selected_text(tb, &length);
    TEST_REQUIRE(selected != NULL);
    TEST_REQUIRE(strcmp(selected, "no\nn") == 0);
    TEST_REQUIRE(length == 4);
    free(selected);

    TEST_REQUIRE(text_buffer_delete_selection(tb));
    TEST_REQUIRE(text_buffer_line_count(tb) == 1);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "uiño") == 0);
    TEST_REQUIRE(text_buffer_cursor_row(tb) == 0);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 1);
    TEST_REQUIRE(!text_buffer_has_selection(tb));

    text_buffer_destroy(tb);
    printf("  PASS: multiline_selection_delete\n");
}

static void test_select_all_and_atomic_insert(void) {
    TextBuffer *tb = text_buffer_create();
    TEST_REQUIRE(text_buffer_set_text(tb, "abc\ndef"));
    text_buffer_select_all(tb);
    TEST_REQUIRE(text_buffer_has_selection(tb));

    size_t length = 0;
    char *selected = text_buffer_selected_text(tb, &length);
    TEST_REQUIRE(selected != NULL);
    TEST_REQUIRE(length == 7);
    TEST_REQUIRE(strcmp(selected, "abc\ndef") == 0);
    free(selected);

    const char *replacement = "diseños";
    TEST_REQUIRE(text_buffer_insert_text(tb, replacement,
                                         strlen(replacement)));
    TEST_REQUIRE(text_buffer_line_count(tb) == 1);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), replacement) == 0);
    TEST_REQUIRE(!text_buffer_has_selection(tb));

    text_buffer_destroy(tb);
    printf("  PASS: select_all_and_atomic_insert\n");
}

static void test_collapsed_shift_selection_stays_collapsed(void) {
    TextBuffer *tb = text_buffer_create();
    RetroKeyEvent shift_left = key_modified(
        RETRO_KEY_LEFT, RETRO_MOD_SHIFT);
    TEST_REQUIRE(text_buffer_handle_key(tb, &shift_left));
    TEST_REQUIRE(!text_buffer_has_selection(tb));

    RetroKeyEvent letter = key_char('a');
    TEST_REQUIRE(text_buffer_handle_key(tb, &letter));
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "a") == 0);
    TEST_REQUIRE(!text_buffer_has_selection(tb));

    text_buffer_destroy(tb);
    printf("  PASS: collapsed_shift_selection_stays_collapsed\n");
}

static void test_selection_render_overlay(void) {
    TextBuffer *tb = text_buffer_create();
    DrawList *list = draw_list_create();
    TEST_REQUIRE(tb != NULL);
    TEST_REQUIRE(list != NULL);
    TEST_REQUIRE(text_buffer_set_text(tb, "niño"));
    text_buffer_set_cursor(tb, 0, 2);

    RetroKeyEvent shift_right = key_modified(
        RETRO_KEY_RIGHT, RETRO_MOD_SHIFT);
    TEST_REQUIRE(text_buffer_handle_key(tb, &shift_right));

    RenderStyle style = {
        RENDER_COLOR_WHITE, RENDER_COLOR_BLACK, false, false
    };
    RenderStyle cursor = {
        RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, true, false
    };
    RenderStyle selection = {
        RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, true
    };
    text_buffer_render_with_selection(
        tb, list, 0, 0, 1, 6, &style, &cursor, &selection);

    TEST_REQUIRE(draw_list_count(list) == 3);
    DrawCommandView command = {0};
    TEST_REQUIRE(draw_list_get(list, 1, &command));
    TEST_REQUIRE(command.x == 2);
    TEST_REQUIRE(strcmp(command.text, "ñ") == 0);
    TEST_REQUIRE(command.style.reverse);
    TEST_REQUIRE(draw_list_get(list, 2, &command));
    TEST_REQUIRE(command.x == 3);
    TEST_REQUIRE(strcmp(command.text, "o") == 0);

    draw_list_destroy(list);
    text_buffer_destroy(tb);
    printf("  PASS: selection_render_overlay\n");
}

'''
if "test_utf8_shift_selection_and_replacement" not in text:
    marker = "static void test_null_safety(void) {\n"
    text = replace_once(text, marker, selection_tests + marker,
                        "TextBuffer selection tests")

old_calls = '''    test_utf8_render_uses_cell_columns();
    test_null_safety();
'''
new_calls = '''    test_utf8_render_uses_cell_columns();
    test_utf8_shift_selection_and_replacement();
    test_multiline_selection_delete();
    test_select_all_and_atomic_insert();
    test_collapsed_shift_selection_stays_collapsed();
    test_selection_render_overlay();
    test_null_safety();
'''
if "test_selection_render_overlay();" not in text:
    text = replace_once(text, old_calls, new_calls,
                        "TextBuffer selection test calls")

path.write_text(text, encoding="utf-8")


# ---------------------------------------------------------------------------
# Notepad integration tests
# ---------------------------------------------------------------------------

path = Path("tests/desktop_runtime_test.c")
text = path.read_text(encoding="utf-8")

if '#include "core/clipboard.h"' not in text:
    text = replace_once(
        text,
        '#include "core/desktop.h"\n',
        '#include "core/clipboard.h"\n#include "core/desktop.h"\n',
        "runtime clipboard include",
    )

modified_helper = r'''static RetroEvent modified_key_event(int key_code,
                                     unsigned int modifiers) {
    RetroEvent event = key_event(key_code, '\0');
    event.data.key.modifiers = modifiers;
    return event;
}

'''
if "static RetroEvent modified_key_event" not in text:
    marker = "static bool failing_create("
    text = replace_once(text, marker, modified_helper + marker,
                        "runtime modifier helper")

runtime_test = r'''static void test_notepad_selection_clipboard_between_instances(void) {
    retro_clipboard_clear();
    RetroAppInstance first = create_untitled_notepad();
    RetroAppInstance second = create_untitled_notepad();

    type_notepad_char(&first, 'n');
    type_notepad_char(&first, 'i');
    type_notepad_codepoint(&first, 0x00F1u);
    type_notepad_char(&first, 'o');
    TEST_REQUIRE(strcmp(notepad_line_for_test(&first, 0), "niño") == 0);

    size_t history_before_copy = notepad_undo_count_for_test(&first);
    RetroEvent select_all = key_event(RETRO_KEY_CTRL_A, '\0');
    RetroEvent copy = key_event(RETRO_KEY_CTRL_C, '\0');
    app_handle_event(&first, &select_all);
    app_handle_event(&first, &copy);
    TEST_REQUIRE(notepad_undo_count_for_test(&first) == history_before_copy);
    TEST_REQUIRE(retro_clipboard_has_text());
    TEST_REQUIRE(strcmp(retro_clipboard_text(NULL), "niño") == 0);

    RetroEvent paste = key_event(RETRO_KEY_CTRL_V, '\0');
    app_handle_event(&second, &paste);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&second, 0), "niño") == 0);
    TEST_REQUIRE(notepad_undo_count_for_test(&second) == 1);
    TEST_REQUIRE(notepad_dirty_for_test(&second));

    RetroEvent undo = key_event(RETRO_KEY_CTRL_Z, '\0');
    app_handle_event(&second, &undo);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&second, 0), "") == 0);
    TEST_REQUIRE(!notepad_dirty_for_test(&second));

    RetroEvent cut = key_event(RETRO_KEY_CTRL_X, '\0');
    app_handle_event(&first, &cut);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&first, 0), "") == 0);
    TEST_REQUIRE(strcmp(retro_clipboard_text(NULL), "niño") == 0);
    app_handle_event(&first, &undo);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&first, 0), "niño") == 0);

    first.descriptor->destroy(&first);
    second.descriptor->destroy(&second);
    retro_clipboard_clear();
}

static void test_notepad_shift_selection_replacement(void) {
    RetroAppInstance instance = create_untitled_notepad();
    type_notepad_char(&instance, 'n');
    type_notepad_char(&instance, 'i');
    type_notepad_codepoint(&instance, 0x00F1u);
    type_notepad_char(&instance, 'o');

    RetroEvent left = key_event(RETRO_KEY_LEFT, '\0');
    app_handle_event(&instance, &left);
    app_handle_event(&instance, &left);
    RetroEvent shift_right = modified_key_event(
        RETRO_KEY_RIGHT, RETRO_MOD_SHIFT);
    app_handle_event(&instance, &shift_right);
    type_notepad_char(&instance, 'x');

    TEST_REQUIRE(strcmp(notepad_line_for_test(&instance, 0), "nixo") == 0);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 3);

    RetroEvent undo = key_event(RETRO_KEY_CTRL_Z, '\0');
    app_handle_event(&instance, &undo);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&instance, 0), "niño") == 0);

    instance.descriptor->destroy(&instance);
}

'''
if "test_notepad_selection_clipboard_between_instances" not in text:
    marker = "#if !defined(_WIN32)\nstatic void create_fixture_file"
    text = replace_once(text, marker, runtime_test + marker,
                        "runtime selection clipboard tests")

old_runtime_calls = '''    test_notepad_undo_redo_utf8();
    test_notepad_history_limit_and_noop();
#if !defined(_WIN32)
'''
new_runtime_calls = '''    test_notepad_undo_redo_utf8();
    test_notepad_history_limit_and_noop();
    test_notepad_selection_clipboard_between_instances();
    test_notepad_shift_selection_replacement();
#if !defined(_WIN32)
'''
if "test_notepad_shift_selection_replacement();" not in text:
    text = replace_once(text, old_runtime_calls, new_runtime_calls,
                        "runtime selection test calls")

path.write_text(text, encoding="utf-8")
