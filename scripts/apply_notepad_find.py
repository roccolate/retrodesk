#!/usr/bin/env python3
"""Materialize the UTF-8 Notepad find checkpoint on its source branch."""

from pathlib import Path


def replace_once(path: str, old: str, new: str, label: str) -> None:
    file_path = Path(path)
    text = file_path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one marker, found {count}")
    file_path.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "src/ui/text_buffer.h",
    "typedef struct TextBuffer TextBuffer;\n",
    """typedef struct TextBuffer TextBuffer;

typedef struct TextBufferMatch {
    size_t row;
    size_t start_col;
    size_t end_col;
} TextBufferMatch;
""",
    "TextBufferMatch declaration",
)

replace_once(
    "src/ui/text_buffer.h",
    """/* Returns selected UTF-8 text with LF separators. The caller owns it. */
char *text_buffer_selected_text(const TextBuffer *buf, size_t *length);
""",
    """/* Returns selected UTF-8 text with LF separators. The caller owns it. */
char *text_buffer_selected_text(const TextBuffer *buf, size_t *length);
/* Select a validated match range and place the cursor at its end. */
bool text_buffer_select_match(TextBuffer *buf, const TextBufferMatch *match);
/* Find the next single-line UTF-8 query from the supplied byte boundary. */
bool text_buffer_find_next(const TextBuffer *buf,
                           const char *query, size_t query_length,
                           bool case_insensitive,
                           size_t start_row, size_t start_col,
                           bool wrap, TextBufferMatch *match);
""",
    "TextBuffer find API",
)

search_impl = r'''static size_t text_line_boundary_at_or_after(const TextLine *line,
                                                  size_t offset) {
    if (!line || !line->data || offset >= line->len) {
        return line ? line->len : 0;
    }
    size_t before = text_line_boundary_at_or_before(line, offset);
    if (before == offset) return before;
    size_t after = retro_utf8_next(line->data, line->len, before);
    return after > before && after <= line->len ? after : line->len;
}

static uint32_t text_search_fold(uint32_t codepoint) {
    if (codepoint >= 'A' && codepoint <= 'Z') return codepoint + 0x20u;
    if ((codepoint >= 0x00C0u && codepoint <= 0x00D6u) ||
        (codepoint >= 0x00D8u && codepoint <= 0x00DEu)) {
        return codepoint + 0x20u;
    }
    if (codepoint == 0x0178u) return 0x00FFu;
    return codepoint;
}

static bool text_line_query_matches(const TextLine *line, size_t position,
                                    const char *query, size_t query_length,
                                    bool case_insensitive,
                                    size_t *match_end) {
    if (!line || !query || !match_end) return false;
    size_t line_offset = position;
    size_t query_offset = 0;

    while (query_offset < query_length) {
        if (line_offset >= line->len) return false;
        uint32_t line_codepoint = 0;
        uint32_t query_codepoint = 0;
        size_t line_bytes = 0;
        size_t query_bytes = 0;
        if (!retro_utf8_decode(line->data, line->len, line_offset,
                               &line_codepoint, &line_bytes) ||
            !retro_utf8_decode(query, query_length, query_offset,
                               &query_codepoint, &query_bytes)) {
            return false;
        }
        if (case_insensitive) {
            line_codepoint = text_search_fold(line_codepoint);
            query_codepoint = text_search_fold(query_codepoint);
        }
        if (line_codepoint != query_codepoint) return false;
        line_offset += line_bytes;
        query_offset += query_bytes;
    }

    *match_end = line_offset;
    return true;
}

static bool text_line_find_range(const TextLine *line,
                                 const char *query, size_t query_length,
                                 bool case_insensitive,
                                 size_t start, size_t limit,
                                 size_t *match_start, size_t *match_end) {
    if (!line || !query || !match_start || !match_end) return false;
    if (limit > line->len) limit = line->len;
    start = text_line_boundary_at_or_after(line, start);
    limit = text_line_boundary_at_or_before(line, limit);

    size_t position = start;
    while (position < limit) {
        size_t end = 0;
        if (text_line_query_matches(line, position, query, query_length,
                                    case_insensitive, &end) &&
            end <= limit) {
            *match_start = position;
            *match_end = end;
            return true;
        }
        size_t next = retro_utf8_next(line->data, line->len, position);
        if (next <= position) break;
        position = next;
    }
    return false;
}

bool text_buffer_select_match(TextBuffer *buf, const TextBufferMatch *match) {
    if (!buf || !match || match->row >= buf->line_count) return false;
    TextLine *line = &buf->lines[match->row];
    size_t start = text_line_boundary_at_or_before(line, match->start_col);
    size_t end = text_line_boundary_at_or_before(line, match->end_col);
    if (start >= end) return false;

    buf->selection_anchor_row = match->row;
    buf->selection_anchor_col = start;
    buf->cursor_row = match->row;
    buf->cursor_col = end;
    buf->selection_active = true;
    return true;
}

bool text_buffer_find_next(const TextBuffer *buf,
                           const char *query, size_t query_length,
                           bool case_insensitive,
                           size_t start_row, size_t start_col,
                           bool wrap, TextBufferMatch *match) {
    if (match) *match = (TextBufferMatch){0};
    if (!buf || !query || !match || buf->line_count == 0 ||
        query_length == 0 || !retro_utf8_validate(query, query_length) ||
        memchr(query, '\0', query_length) ||
        memchr(query, '\n', query_length) ||
        memchr(query, '\r', query_length)) {
        return false;
    }

    if (start_row >= buf->line_count) start_row = buf->line_count - 1;
    start_col = text_line_boundary_at_or_after(
        &buf->lines[start_row], start_col);

    for (size_t row = start_row; row < buf->line_count; ++row) {
        const TextLine *line = &buf->lines[row];
        size_t found_start = 0;
        size_t found_end = 0;
        size_t row_start = row == start_row ? start_col : 0;
        if (text_line_find_range(line, query, query_length,
                                 case_insensitive, row_start, line->len,
                                 &found_start, &found_end)) {
            *match = (TextBufferMatch){row, found_start, found_end};
            return true;
        }
    }

    if (!wrap) return false;
    for (size_t row = 0; row < start_row; ++row) {
        const TextLine *line = &buf->lines[row];
        size_t found_start = 0;
        size_t found_end = 0;
        if (text_line_find_range(line, query, query_length,
                                 case_insensitive, 0, line->len,
                                 &found_start, &found_end)) {
            *match = (TextBufferMatch){row, found_start, found_end};
            return true;
        }
    }

    size_t found_start = 0;
    size_t found_end = 0;
    if (text_line_find_range(&buf->lines[start_row], query, query_length,
                             case_insensitive, 0, start_col,
                             &found_start, &found_end)) {
        *match = (TextBufferMatch){start_row, found_start, found_end};
        return true;
    }
    return false;
}

'''
replace_once(
    "src/ui/text_buffer.c",
    "size_t text_buffer_scroll_row(const TextBuffer *buf) {\n",
    search_impl + "size_t text_buffer_scroll_row(const TextBuffer *buf) {\n",
    "TextBuffer find implementation",
)

replace_once(
    "src/apps/notepad_app.c",
    """    bool has_path;
    bool save_as;
    bool close_prompt;
""",
    """    bool has_path;
    bool save_as;
    bool search_mode;
    bool search_has_match;
    bool close_prompt;
""",
    "Notepad search state",
)

search_handlers = r'''static bool np_find_next(NotepadState *state) {
    if (!state || !state->buffer || !state->path_input) return false;
    const char *query = text_input_text(state->path_input);
    size_t query_length = text_input_length(state->path_input);
    if (query_length == 0) {
        snprintf(state->error, sizeof(state->error),
                 "Find: enter text to search");
        return false;
    }

    TextBufferMatch match = {0};
    size_t start_row = text_buffer_cursor_row(state->buffer);
    size_t start_col = text_buffer_cursor_col(state->buffer);
    if (!text_buffer_find_next(state->buffer, query, query_length, true,
                               start_row, start_col, true, &match)) {
        snprintf(state->error, sizeof(state->error),
                 "Find: no match");
        state->search_has_match = false;
        return false;
    }
    if (!text_buffer_select_match(state->buffer, &match)) return false;
    state->search_has_match = true;
    state->error[0] = '\0';
    return true;
}

static void np_handle_search(NotepadState *state,
                             const RetroKeyEvent *key) {
    if (!state || !key) return;
    if (key->key_code == RETRO_KEY_ESC) {
        state->search_mode = false;
        state->search_has_match = false;
        state->error[0] = '\0';
        return;
    }
    if (key->key_code == RETRO_KEY_CR || key->key_code == RETRO_KEY_LF) {
        (void)np_find_next(state);
        return;
    }
    if (text_input_handle_key(state->path_input, key)) {
        state->search_has_match = false;
        text_buffer_clear_selection(state->buffer);
        state->error[0] = '\0';
    }
}

'''
replace_once(
    "src/apps/notepad_app.c",
    "static void np_handle_save_as(RetroAppInstance *instance,\n",
    search_handlers + "static void np_handle_save_as(RetroAppInstance *instance,\n",
    "Notepad search handlers",
)

replace_once(
    "src/apps/notepad_app.c",
    """    if (state->save_as) {
        np_handle_save_as(instance, state, key);
        return;
    }

    if (key->key_code == RETRO_KEY_CTRL_A) {
""",
    """    if (state->save_as) {
        np_handle_save_as(instance, state, key);
        return;
    }

    if (state->search_mode) {
        np_handle_search(state, key);
        return;
    }

    if (key->key_code == RETRO_KEY_CTRL_F) {
        state->search_mode = true;
        state->search_has_match = false;
        text_input_clear(state->path_input);
        text_buffer_clear_selection(state->buffer);
        state->error[0] = '\0';
        return;
    }

    if (key->key_code == RETRO_KEY_CTRL_A) {
""",
    "Notepad search dispatch",
)

replace_once(
    "src/apps/notepad_app.c",
    """        } else {
            state->save_as = true;
        }
        return;
    }

    if (key->key_code == RETRO_KEY_F3) {
        state->close_after_save = false;
        state->save_as = true;
        return;
    }
""",
    """        } else {
            state->search_mode = false;
            text_input_clear(state->path_input);
            state->save_as = true;
        }
        return;
    }

    if (key->key_code == RETRO_KEY_F3) {
        state->close_after_save = false;
        state->search_mode = false;
        text_input_clear(state->path_input);
        state->save_as = true;
        return;
    }
""",
    "Notepad modal input reset",
)

replace_once(
    "src/apps/notepad_app.c",
    """        state->close_prompt = false;
        state->save_as = true;
        state->close_after_save = true;
""",
    """        state->close_prompt = false;
        state->search_mode = false;
        text_input_clear(state->path_input);
        state->save_as = true;
        state->close_after_save = true;
""",
    "Notepad close Save As reset",
)

replace_once(
    "src/apps/notepad_app.c",
    """    if (!state->save_as && !state->close_prompt) {
        draw_list_text(draw_list, 3, 2,
                       "Shift+arrows select | Ctrl+A/C/X/V all/copy/cut/paste",
                       text);
    }
""",
    """    if (!state->save_as && !state->close_prompt && !state->search_mode) {
        draw_list_text(draw_list, 3, 2,
                       "Shift+arrows select | Ctrl+A/C/X/V | Ctrl+F find",
                       text);
    }
""",
    "Notepad find hint",
)

replace_once(
    "src/apps/notepad_app.c",
    """    } else if (state->save_as) {
        draw_list_text(draw_list, 3, 2,
                       "Path (Enter save, Esc cancel):", accent);
        text_input_render(state->path_input, draw_list, 4, 2, 64,
                          text, cursor);
    } else {
""",
    """    } else if (state->save_as) {
        draw_list_text(draw_list, 3, 2,
                       "Path (Enter save, Esc cancel):", accent);
        text_input_render(state->path_input, draw_list, 4, 2, 64,
                          text, cursor);
    } else if (state->search_mode) {
        draw_list_text(draw_list, 3, 2,
                       "Find (Enter next, Esc close):", accent);
        text_input_render(state->path_input, draw_list, 4, 2, 64,
                          text, cursor);
        text_buffer_render_with_selection(
            state->buffer, draw_list, 6, 2, 9, 64,
            text, cursor, &selection);
    } else {
""",
    "Notepad find render",
)

replace_once(
    "src/apps/notepad_app.c",
    """size_t notepad_cursor_col_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return 0;
    const NotepadState *state = instance->state;
    return text_buffer_cursor_col(state->buffer);
}

const RetroAppDescriptor *notepad_app_descriptor(void) {
""",
    """size_t notepad_cursor_col_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return 0;
    const NotepadState *state = instance->state;
    return text_buffer_cursor_col(state->buffer);
}

size_t notepad_cursor_row_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return 0;
    const NotepadState *state = instance->state;
    return text_buffer_cursor_row(state->buffer);
}

bool notepad_search_mode_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return false;
    const NotepadState *state = instance->state;
    return state->search_mode;
}

const RetroAppDescriptor *notepad_app_descriptor(void) {
""",
    "Notepad find probes",
)

replace_once(
    "src/apps/apps_internal.h",
    """const char *notepad_line_for_test(const RetroAppInstance *instance, size_t row);
size_t notepad_cursor_col_for_test(const RetroAppInstance *instance);
""",
    """const char *notepad_line_for_test(const RetroAppInstance *instance, size_t row);
size_t notepad_cursor_col_for_test(const RetroAppInstance *instance);
size_t notepad_cursor_row_for_test(const RetroAppInstance *instance);
bool notepad_search_mode_for_test(const RetroAppInstance *instance);
""",
    "Notepad find probe declarations",
)

text_buffer_test = r'''static void test_utf8_find_next_and_wrap(void) {
    TextBuffer *tb = text_buffer_create();
    TEST_REQUIRE(tb != NULL);
    TEST_REQUIRE(text_buffer_set_text(tb, "Árbol\nniño árbol\nARBOL"));

    const char *query = "árbol";
    TextBufferMatch match = {0};
    TEST_REQUIRE(text_buffer_find_next(tb, query, strlen(query), true,
                                       0, 0, true, &match));
    TEST_REQUIRE(match.row == 0);
    TEST_REQUIRE(match.start_col == 0);
    TEST_REQUIRE(match.end_col == 6);

    TEST_REQUIRE(text_buffer_find_next(tb, query, strlen(query), true,
                                       match.row, match.end_col, true, &match));
    TEST_REQUIRE(match.row == 1);
    TEST_REQUIRE(match.start_col == 6);
    TEST_REQUIRE(match.end_col == 12);

    TEST_REQUIRE(text_buffer_find_next(tb, query, strlen(query), true,
                                       match.row, match.end_col, true, &match));
    TEST_REQUIRE(match.row == 2);
    TEST_REQUIRE(match.start_col == 0);
    TEST_REQUIRE(match.end_col == 5);

    TEST_REQUIRE(text_buffer_find_next(tb, query, strlen(query), true,
                                       match.row, match.end_col, true, &match));
    TEST_REQUIRE(match.row == 0);
    TEST_REQUIRE(text_buffer_select_match(tb, &match));
    size_t selected_length = 0;
    char *selected = text_buffer_selected_text(tb, &selected_length);
    TEST_REQUIRE(selected != NULL);
    TEST_REQUIRE(selected_length == 6);
    TEST_REQUIRE(strcmp(selected, "Árbol") == 0);
    free(selected);

    TEST_REQUIRE(text_buffer_find_next(tb, query, strlen(query), false,
                                       0, 0, true, &match));
    TEST_REQUIRE(match.row == 1);
    TEST_REQUIRE(match.start_col == 6);

    const char invalid[] = {(char)0xC3};
    TEST_REQUIRE(!text_buffer_find_next(tb, "", 0, true,
                                        0, 0, true, &match));
    TEST_REQUIRE(!text_buffer_find_next(tb, invalid, sizeof(invalid), true,
                                        0, 0, true, &match));
    TEST_REQUIRE(!text_buffer_find_next(tb, "missing", 7, true,
                                        0, 0, true, &match));

    text_buffer_destroy(tb);
    printf("  PASS: utf8_find_next_and_wrap\n");
}

'''
replace_once(
    "tests/text_buffer_test.c",
    "static void test_null_safety(void) {\n",
    text_buffer_test + "static void test_null_safety(void) {\n",
    "TextBuffer find tests",
)
replace_once(
    "tests/text_buffer_test.c",
    """    test_selection_render_overlay();
    test_null_safety();
""",
    """    test_selection_render_overlay();
    test_utf8_find_next_and_wrap();
    test_null_safety();
""",
    "TextBuffer find test registration",
)
replace_once(
    "tests/text_buffer_test.c",
    """    TEST_REQUIRE(!text_buffer_set_text(NULL, "test"));
    text_buffer_clear(NULL);
""",
    """    TEST_REQUIRE(!text_buffer_set_text(NULL, "test"));
    TextBufferMatch match = {0};
    TEST_REQUIRE(!text_buffer_find_next(NULL, "x", 1, true,
                                        0, 0, true, &match));
    TEST_REQUIRE(!text_buffer_select_match(NULL, &match));
    text_buffer_clear(NULL);
""",
    "TextBuffer find null safety",
)

runtime_test = r'''static void test_notepad_utf8_find(void) {
    retro_clipboard_clear();
    RetroAppInstance instance = create_untitled_notepad();

    type_notepad_codepoint(&instance, 0x00C1u);
    type_notepad_char(&instance, 'r');
    type_notepad_char(&instance, 'b');
    type_notepad_char(&instance, 'o');
    type_notepad_char(&instance, 'l');
    RetroEvent enter = key_event(RETRO_KEY_CR, '\0');
    app_handle_event(&instance, &enter);
    type_notepad_char(&instance, 'n');
    type_notepad_char(&instance, 'i');
    type_notepad_codepoint(&instance, 0x00F1u);
    type_notepad_char(&instance, 'o');
    type_notepad_char(&instance, ' ');
    type_notepad_codepoint(&instance, 0x00E1u);
    type_notepad_char(&instance, 'r');
    type_notepad_char(&instance, 'b');
    type_notepad_char(&instance, 'o');
    type_notepad_char(&instance, 'l');
    app_handle_event(&instance, &enter);
    type_notepad_char(&instance, 'A');
    type_notepad_char(&instance, 'R');
    type_notepad_char(&instance, 'B');
    type_notepad_char(&instance, 'O');
    type_notepad_char(&instance, 'L');

    size_t history_before_find = notepad_undo_count_for_test(&instance);
    RetroEvent find = key_event(RETRO_KEY_CTRL_F, '\0');
    app_handle_event(&instance, &find);
    TEST_REQUIRE(notepad_search_mode_for_test(&instance));
    type_notepad_codepoint(&instance, 0x00E1u);
    type_notepad_char(&instance, 'r');
    type_notepad_char(&instance, 'b');
    type_notepad_char(&instance, 'o');
    type_notepad_char(&instance, 'l');
    app_handle_event(&instance, &enter);
    TEST_REQUIRE(notepad_cursor_row_for_test(&instance) == 0);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 6);
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) == history_before_find);

    RetroEvent escape = key_event(RETRO_KEY_ESC, '\0');
    app_handle_event(&instance, &escape);
    TEST_REQUIRE(!notepad_search_mode_for_test(&instance));
    RetroEvent copy = key_event(RETRO_KEY_CTRL_C, '\0');
    app_handle_event(&instance, &copy);
    TEST_REQUIRE(strcmp(retro_clipboard_text(NULL), "Árbol") == 0);

    app_handle_event(&instance, &find);
    type_notepad_codepoint(&instance, 0x00E1u);
    type_notepad_char(&instance, 'r');
    type_notepad_char(&instance, 'b');
    type_notepad_char(&instance, 'o');
    type_notepad_char(&instance, 'l');
    app_handle_event(&instance, &enter);
    TEST_REQUIRE(notepad_cursor_row_for_test(&instance) == 1);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 12);
    app_handle_event(&instance, &escape);
    app_handle_event(&instance, &copy);
    TEST_REQUIRE(strcmp(retro_clipboard_text(NULL), "árbol") == 0);

    app_handle_event(&instance, &find);
    type_notepad_codepoint(&instance, 0x00E1u);
    type_notepad_char(&instance, 'r');
    type_notepad_char(&instance, 'b');
    type_notepad_char(&instance, 'o');
    type_notepad_char(&instance, 'l');
    app_handle_event(&instance, &enter);
    TEST_REQUIRE(notepad_cursor_row_for_test(&instance) == 2);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 5);
    app_handle_event(&instance, &escape);
    app_handle_event(&instance, &copy);
    TEST_REQUIRE(strcmp(retro_clipboard_text(NULL), "ARBOL") == 0);

    TEST_REQUIRE(notepad_undo_count_for_test(&instance) == history_before_find);
    instance.descriptor->destroy(&instance);
    retro_clipboard_clear();
}

'''
replace_once(
    "tests/desktop_runtime_test.c",
    "static void test_notepad_shift_selection_replacement(void) {\n",
    runtime_test + "static void test_notepad_shift_selection_replacement(void) {\n",
    "Notepad find runtime test",
)
replace_once(
    "tests/desktop_runtime_test.c",
    """    test_notepad_selection_clipboard_between_instances();
    test_notepad_shift_selection_replacement();
""",
    """    test_notepad_selection_clipboard_between_instances();
    test_notepad_utf8_find();
    test_notepad_shift_selection_replacement();
""",
    "Notepad find test registration",
)

print("Notepad UTF-8 find port applied")
