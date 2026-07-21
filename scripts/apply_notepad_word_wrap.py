#!/usr/bin/env python3
"""Materialize the UTF-8 Notepad word-wrap checkpoint."""

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
    """/* Event handling — returns true if the key was consumed. */
bool text_buffer_handle_key(TextBuffer *buf, const RetroKeyEvent *key);

/* Compatibility renderer without selection highlighting. */
""",
    """/* Event handling — returns true if the key was consumed. */
bool text_buffer_handle_key(TextBuffer *buf, const RetroKeyEvent *key);
/* Wrapped handling changes Up/Down to move across visual rows. */
bool text_buffer_handle_key_wrapped(TextBuffer *buf,
                                    const RetroKeyEvent *key,
                                    size_t wrap_columns);
/* Visual-row probes use terminal cell columns and preserve UTF-8 boundaries. */
size_t text_buffer_visual_row_count(const TextBuffer *buf,
                                    size_t wrap_columns);
size_t text_buffer_cursor_visual_row(const TextBuffer *buf,
                                     size_t wrap_columns);

/* Compatibility renderer without selection highlighting. */
""",
    "TextBuffer wrapped navigation API",
)

replace_once(
    "src/ui/text_buffer.h",
    """/* Selection-aware renderer used by editors. */
void text_buffer_render_with_selection(
    const TextBuffer *buf, DrawList *draw_list,
    int y, int x, int visible_rows, int visible_cols,
    const RenderStyle *style, const RenderStyle *cursor_style,
    const RenderStyle *selection_style);
""",
    """/* Selection-aware renderer used by editors. */
void text_buffer_render_with_selection(
    const TextBuffer *buf, DrawList *draw_list,
    int y, int x, int visible_rows, int visible_cols,
    const RenderStyle *style, const RenderStyle *cursor_style,
    const RenderStyle *selection_style);
/* Soft-wrap renderer; logical text and saved bytes remain unchanged. */
void text_buffer_render_wrapped_with_selection(
    const TextBuffer *buf, DrawList *draw_list,
    int y, int x, int visible_rows, int visible_cols,
    const RenderStyle *style, const RenderStyle *cursor_style,
    const RenderStyle *selection_style);
""",
    "TextBuffer wrapped render API",
)

wrap_navigation = r'''typedef struct TextWrapChunk {
    size_t row;
    size_t start;
    size_t end;
} TextWrapChunk;

static size_t text_line_wrap_end(const TextLine *line, size_t start,
                                 size_t wrap_columns) {
    if (!line || !line->data || start >= line->len || wrap_columns == 0) {
        return line ? line->len : 0;
    }

    size_t position = text_line_boundary_at_or_before(line, start);
    size_t used_columns = 0;
    while (position < line->len) {
        uint32_t codepoint = 0;
        size_t byte_len = 0;
        if (!retro_utf8_decode(line->data, line->len, position,
                               &codepoint, &byte_len)) {
            codepoint = '?';
            byte_len = 1;
        }
        int width_value = retro_utf8_width(codepoint);
        size_t width = width_value > 0 ? (size_t)width_value : 0;
        if (width > 0 && used_columns > 0 &&
            used_columns + width > wrap_columns) {
            break;
        }
        if (width > wrap_columns && used_columns == 0) {
            position += byte_len;
            break;
        }
        if (width > 0 && used_columns + width > wrap_columns) break;
        used_columns += width;
        position += byte_len;
    }
    if (position <= start) {
        size_t next = retro_utf8_next(line->data, line->len, start);
        return next > start ? next : line->len;
    }
    return position;
}

static size_t text_line_wrap_chunk_count(const TextLine *line,
                                         size_t wrap_columns) {
    if (!line || line->len == 0 || wrap_columns == 0) return 1;
    size_t count = 0;
    size_t start = 0;
    while (start < line->len) {
        size_t end = text_line_wrap_end(line, start, wrap_columns);
        if (end <= start) break;
        count++;
        start = end;
    }
    return count > 0 ? count : 1;
}

size_t text_buffer_visual_row_count(const TextBuffer *buf,
                                    size_t wrap_columns) {
    if (!buf || buf->line_count == 0 || wrap_columns == 0) return 0;
    size_t total = 0;
    for (size_t row = 0; row < buf->line_count; ++row) {
        size_t count = text_line_wrap_chunk_count(
            &buf->lines[row], wrap_columns);
        if (total > SIZE_MAX - count) return SIZE_MAX;
        total += count;
    }
    return total;
}

static bool text_buffer_wrap_chunk_at_visual(const TextBuffer *buf,
                                             size_t wrap_columns,
                                             size_t visual_row,
                                             TextWrapChunk *chunk) {
    if (!buf || !chunk || buf->line_count == 0 || wrap_columns == 0) {
        return false;
    }

    size_t index = 0;
    for (size_t row = 0; row < buf->line_count; ++row) {
        const TextLine *line = &buf->lines[row];
        if (line->len == 0) {
            if (index == visual_row) {
                *chunk = (TextWrapChunk){row, 0, 0};
                return true;
            }
            index++;
            continue;
        }

        size_t start = 0;
        while (start < line->len) {
            size_t end = text_line_wrap_end(line, start, wrap_columns);
            if (end <= start) return false;
            if (index == visual_row) {
                *chunk = (TextWrapChunk){row, start, end};
                return true;
            }
            index++;
            start = end;
        }
    }
    return false;
}

static bool text_buffer_wrap_chunk_for_cursor(const TextBuffer *buf,
                                              size_t wrap_columns,
                                              TextWrapChunk *chunk,
                                              size_t *visual_row) {
    if (!buf || !chunk || !visual_row || buf->line_count == 0 ||
        wrap_columns == 0 || buf->cursor_row >= buf->line_count) {
        return false;
    }

    size_t visual = 0;
    for (size_t row = 0; row < buf->cursor_row; ++row) {
        visual += text_line_wrap_chunk_count(
            &buf->lines[row], wrap_columns);
    }

    const TextLine *line = &buf->lines[buf->cursor_row];
    if (line->len == 0) {
        *chunk = (TextWrapChunk){buf->cursor_row, 0, 0};
        *visual_row = visual;
        return true;
    }

    size_t cursor = text_line_boundary_at_or_before(line, buf->cursor_col);
    size_t start = 0;
    while (start < line->len) {
        size_t end = text_line_wrap_end(line, start, wrap_columns);
        if (end <= start) return false;
        if (cursor < end || end == line->len) {
            *chunk = (TextWrapChunk){buf->cursor_row, start, end};
            *visual_row = visual;
            return true;
        }
        visual++;
        start = end;
    }
    return false;
}

size_t text_buffer_cursor_visual_row(const TextBuffer *buf,
                                     size_t wrap_columns) {
    TextWrapChunk chunk = {0};
    size_t visual_row = 0;
    return text_buffer_wrap_chunk_for_cursor(
               buf, wrap_columns, &chunk, &visual_row)
               ? visual_row
               : 0;
}

static bool text_buffer_move_wrapped_vertical(TextBuffer *buf,
                                              int direction,
                                              size_t wrap_columns) {
    if (!buf || wrap_columns == 0 || direction == 0) return false;
    TextWrapChunk current = {0};
    size_t current_visual = 0;
    if (!text_buffer_wrap_chunk_for_cursor(
            buf, wrap_columns, &current, &current_visual)) {
        return false;
    }

    size_t target_visual = current_visual;
    if (direction < 0) {
        if (current_visual == 0) return true;
        target_visual--;
    } else {
        size_t total = text_buffer_visual_row_count(buf, wrap_columns);
        if (current_visual + 1 >= total) return true;
        target_visual++;
    }

    TextWrapChunk target = {0};
    if (!text_buffer_wrap_chunk_at_visual(
            buf, wrap_columns, target_visual, &target)) {
        return false;
    }

    const TextLine *current_line = &buf->lines[current.row];
    size_t desired = retro_utf8_columns(
        current_line->data, current_line->len,
        current.start, buf->cursor_col);
    const TextLine *target_line = &buf->lines[target.row];
    size_t target_length = target.end - target.start;
    size_t relative = retro_utf8_prefix_bytes(
        target_line->data + target.start, target_length, desired);
    size_t target_col = target.start + relative;
    if (target.end < target_line->len && target_col >= target.end &&
        target.end > target.start) {
        target_col = retro_utf8_prev(target_line->data, target.end);
    }

    buf->cursor_row = target.row;
    buf->cursor_col = target_col;
    return true;
}

bool text_buffer_handle_key_wrapped(TextBuffer *buf,
                                    const RetroKeyEvent *key,
                                    size_t wrap_columns) {
    if (!buf || !key) return false;
    if (wrap_columns == 0 ||
        (key->key_code != RETRO_KEY_UP &&
         key->key_code != RETRO_KEY_DOWN)) {
        return text_buffer_handle_key(buf, key);
    }

    text_buffer_prepare_movement(buf, key);
    return text_buffer_move_wrapped_vertical(
        buf, key->key_code == RETRO_KEY_UP ? -1 : 1,
        wrap_columns);
}

'''
replace_once(
    "src/ui/text_buffer.c",
    "static size_t text_line_cursor_width(const TextLine *line,\n",
    wrap_navigation + "static size_t text_line_cursor_width(const TextLine *line,\n",
    "TextBuffer wrapped navigation implementation",
)

wrap_render = r'''static char *text_line_wrapped_chunk_text(const TextLine *line,
                                          size_t start, size_t end,
                                          size_t visible_cols) {
    if (end < start) end = start;
    size_t bytes = end - start;
    if (bytes > SIZE_MAX - visible_cols - 1) return NULL;
    size_t capacity = bytes + visible_cols + 1;
    char *out = malloc(capacity);
    if (!out) return NULL;

    size_t out_len = 0;
    size_t used_columns = 0;
    size_t position = start;
    while (line && position < end) {
        uint32_t codepoint = 0;
        size_t byte_len = 0;
        if (!retro_utf8_decode(line->data, line->len, position,
                               &codepoint, &byte_len) ||
            position + byte_len > end) {
            free(out);
            return NULL;
        }
        int width_value = retro_utf8_width(codepoint);
        size_t width = width_value > 0 ? (size_t)width_value : 0;
        if (out_len + byte_len >= capacity) break;
        memcpy(out + out_len, line->data + position, byte_len);
        out_len += byte_len;
        used_columns += width;
        position += byte_len;
    }
    while (used_columns < visible_cols && out_len + 1 < capacity) {
        out[out_len++] = ' ';
        used_columns++;
    }
    out[out_len] = '\0';
    return out;
}

static void text_buffer_render_selection_chunk(
    const TextBuffer *buf, DrawList *draw_list,
    const TextRange *range, const TextWrapChunk *chunk,
    size_t visible_cols, int y, int x,
    const RenderStyle *selection_style) {
    if (!buf || !draw_list || !range || !chunk || !selection_style ||
        chunk->row >= buf->line_count || chunk->row < range->start.row ||
        chunk->row > range->end.row) {
        return;
    }

    const TextLine *line = &buf->lines[chunk->row];
    size_t selected_start =
        chunk->row == range->start.row ? range->start.col : 0;
    size_t selected_end =
        chunk->row == range->end.row ? range->end.col : line->len;
    if (selected_start < chunk->start) selected_start = chunk->start;
    if (selected_end > chunk->end) selected_end = chunk->end;
    selected_start = text_line_boundary_at_or_before(line, selected_start);
    selected_end = text_line_boundary_at_or_before(line, selected_end);

    if (selected_end > selected_start) {
        size_t x_columns = retro_utf8_columns(
            line->data, line->len, chunk->start, selected_start);
        if (x_columns < visible_cols) {
            char *selected = text_line_range_text(
                line, selected_start, selected_end,
                visible_cols - x_columns);
            if (selected && selected[0]) {
                draw_list_text(draw_list, y, x + (int)x_columns,
                               selected, selection_style);
            }
            free(selected);
        }
    }

    if (chunk->end == line->len && chunk->row < range->end.row) {
        size_t newline_x = retro_utf8_columns(
            line->data, line->len, chunk->start, line->len);
        if (newline_x < visible_cols) {
            draw_list_text(draw_list, y, x + (int)newline_x,
                           " ", selection_style);
        }
    }
}

void text_buffer_render_wrapped_with_selection(
    const TextBuffer *buf, DrawList *draw_list,
    int y, int x, int visible_rows, int visible_cols,
    const RenderStyle *style, const RenderStyle *cursor_style,
    const RenderStyle *selection_style) {
    if (!buf || !draw_list || !style || visible_rows <= 0 ||
        visible_cols <= 0 || buf->line_count == 0) {
        return;
    }

    size_t rows = (size_t)visible_rows;
    size_t cols = (size_t)visible_cols;
    TextWrapChunk cursor_chunk = {0};
    size_t cursor_visual = 0;
    if (!text_buffer_wrap_chunk_for_cursor(
            buf, cols, &cursor_chunk, &cursor_visual)) {
        return;
    }
    size_t scroll_visual =
        cursor_visual >= rows ? cursor_visual - rows + 1 : 0;
    TextRange selection = {0};
    bool has_selection = text_buffer_selection_range(buf, &selection);

    for (size_t row = 0; row < rows; ++row) {
        TextWrapChunk chunk = {0};
        size_t visual = scroll_visual + row;
        if (!text_buffer_wrap_chunk_at_visual(buf, cols, visual, &chunk)) {
            char *blank = text_line_wrapped_chunk_text(NULL, 0, 0, cols);
            if (blank) {
                draw_list_text(draw_list, y + (int)row, x, blank, style);
                free(blank);
            }
            continue;
        }
        const TextLine *line = &buf->lines[chunk.row];
        char *visible = text_line_wrapped_chunk_text(
            line, chunk.start, chunk.end, cols);
        if (visible) {
            draw_list_text(draw_list, y + (int)row, x, visible, style);
            free(visible);
        }
        if (has_selection) {
            text_buffer_render_selection_chunk(
                buf, draw_list, &selection, &chunk, cols,
                y + (int)row, x, selection_style);
        }
    }

    if (!cursor_style || cursor_visual < scroll_visual ||
        cursor_visual >= scroll_visual + rows) {
        return;
    }

    const TextLine *cursor_line = &buf->lines[buf->cursor_row];
    size_t cursor_x = retro_utf8_columns(
        cursor_line->data, cursor_line->len,
        cursor_chunk.start, buf->cursor_col);
    size_t cursor_byte = buf->cursor_col;
    if (cursor_x >= cols && cursor_chunk.end > cursor_chunk.start) {
        cursor_byte = retro_utf8_prev(
            cursor_line->data, cursor_chunk.end);
        cursor_x = retro_utf8_columns(
            cursor_line->data, cursor_line->len,
            cursor_chunk.start, cursor_byte);
    }
    if (cursor_x >= cols) return;

    char cursor_text[5] = {' ', '\0', '\0', '\0', '\0'};
    if (cursor_byte < cursor_line->len) {
        uint32_t codepoint = 0;
        size_t byte_len = 0;
        if (retro_utf8_decode(cursor_line->data, cursor_line->len,
                              cursor_byte, &codepoint, &byte_len) &&
            byte_len <= 4) {
            (void)codepoint;
            memcpy(cursor_text, cursor_line->data + cursor_byte, byte_len);
            cursor_text[byte_len] = '\0';
        }
    }

    int cursor_y = y + (int)(cursor_visual - scroll_visual);
    draw_list_text(draw_list, cursor_y, x + (int)cursor_x,
                   cursor_text, cursor_style);
}

'''
replace_once(
    "src/ui/text_buffer.c",
    "void text_buffer_render_with_selection(\n",
    wrap_render + "void text_buffer_render_with_selection(\n",
    "TextBuffer wrapped renderer",
)

replace_once(
    "src/apps/notepad_app.c",
    """enum {
    NP_HISTORY_MAX_STATES = 100,
    NP_HISTORY_MAX_BYTES = 1024 * 1024,
};
""",
    """enum {
    NP_HISTORY_MAX_STATES = 100,
    NP_HISTORY_MAX_BYTES = 1024 * 1024,
    NP_WRAP_COLUMNS = 64,
};
""",
    "Notepad wrap width",
)

replace_once(
    "src/apps/notepad_app.c",
    """    bool search_mode;
    bool search_has_match;
    bool close_prompt;
""",
    """    bool search_mode;
    bool search_has_match;
    bool wrap_mode;
    bool close_prompt;
""",
    "Notepad wrap state",
)

replace_once(
    "src/apps/notepad_app.c",
    """    bool consumed = text_buffer_handle_key(state->buffer, key);
""",
    """    bool consumed = state->wrap_mode
                        ? text_buffer_handle_key_wrapped(
                              state->buffer, key, NP_WRAP_COLUMNS)
                        : text_buffer_handle_key(state->buffer, key);
""",
    "Notepad wrapped editor dispatch",
)

replace_once(
    "src/apps/notepad_app.c",
    """    if (key->key_code == RETRO_KEY_F3) {
        state->close_after_save = false;
        state->search_mode = false;
        text_input_clear(state->path_input);
        state->save_as = true;
        return;
    }

    if (key->key_code == RETRO_KEY_ESC) {
""",
    r'''    if (key->key_code == RETRO_KEY_F3) {
        state->close_after_save = false;
        state->search_mode = false;
        text_input_clear(state->path_input);
        state->save_as = true;
        return;
    }

    if (key->key_code == RETRO_KEY_F4) {
        state->wrap_mode = !state->wrap_mode;
        state->error[0] = '\0';
        return;
    }

    if (key->key_code == RETRO_KEY_ESC) {
''',
    "Notepad F4 wrap toggle",
)

render_helper = r'''static void np_render_buffer(const NotepadState *state,
                             DrawList *draw_list,
                             int y, int rows,
                             const RenderStyle *text,
                             const RenderStyle *cursor,
                             const RenderStyle *selection) {
    if (!state || !draw_list || !text || !cursor || !selection) return;
    if (state->wrap_mode) {
        text_buffer_render_wrapped_with_selection(
            state->buffer, draw_list, y, 2, rows, NP_WRAP_COLUMNS,
            text, cursor, selection);
    } else {
        text_buffer_render_with_selection(
            state->buffer, draw_list, y, 2, rows, NP_WRAP_COLUMNS,
            text, cursor, selection);
    }
}

'''
replace_once(
    "src/apps/notepad_app.c",
    "static void np_render_close_prompt(const NotepadState *state,\n",
    render_helper + "static void np_render_close_prompt(const NotepadState *state,\n",
    "Notepad wrap render helper",
)

replace_once(
    "src/apps/notepad_app.c",
    """    if (!state->save_as && !state->close_prompt && !state->search_mode) {
        draw_list_text(draw_list, 3, 2,
                       "Shift+arrows select | Ctrl+A/C/X/V | Ctrl+F find",
                       text);
    }
""",
    """    if (!state->save_as && !state->close_prompt && !state->search_mode) {
        char edit_hint[128];
        snprintf(edit_hint, sizeof(edit_hint),
                 "Shift+arrows select | Ctrl+A/C/X/V | Ctrl+F | F4 wrap:%s",
                 state->wrap_mode ? "on" : "off");
        draw_list_text(draw_list, 3, 2, edit_hint, text);
    }
""",
    "Notepad wrap hint",
)

replace_once(
    "src/apps/notepad_app.c",
    """    if (state->close_prompt) {
        text_buffer_render_with_selection(
            state->buffer, draw_list, 4, 2, 11, 64,
            text, cursor, &selection);
        np_render_close_prompt(state, draw_list, text, accent, cursor);
""",
    """    if (state->close_prompt) {
        np_render_buffer(state, draw_list, 4, 11,
                         text, cursor, &selection);
        np_render_close_prompt(state, draw_list, text, accent, cursor);
""",
    "Notepad wrapped close render",
)

replace_once(
    "src/apps/notepad_app.c",
    """        text_buffer_render_with_selection(
            state->buffer, draw_list, 6, 2, 9, 64,
            text, cursor, &selection);
    } else {
        text_buffer_render_with_selection(
            state->buffer, draw_list, 4, 2, 11, 64,
            text, cursor, &selection);
    }
""",
    """        np_render_buffer(state, draw_list, 6, 9,
                         text, cursor, &selection);
    } else {
        np_render_buffer(state, draw_list, 4, 11,
                         text, cursor, &selection);
    }
""",
    "Notepad wrapped normal and search render",
)

replace_once(
    "src/apps/notepad_app.c",
    """bool notepad_search_mode_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return false;
    const NotepadState *state = instance->state;
    return state->search_mode;
}

const RetroAppDescriptor *notepad_app_descriptor(void) {
""",
    """bool notepad_search_mode_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return false;
    const NotepadState *state = instance->state;
    return state->search_mode;
}

bool notepad_wrap_mode_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return false;
    const NotepadState *state = instance->state;
    return state->wrap_mode;
}

const RetroAppDescriptor *notepad_app_descriptor(void) {
""",
    "Notepad wrap probe",
)

replace_once(
    "src/apps/apps_internal.h",
    """size_t notepad_cursor_row_for_test(const RetroAppInstance *instance);
bool notepad_search_mode_for_test(const RetroAppInstance *instance);
""",
    """size_t notepad_cursor_row_for_test(const RetroAppInstance *instance);
bool notepad_search_mode_for_test(const RetroAppInstance *instance);
bool notepad_wrap_mode_for_test(const RetroAppInstance *instance);
""",
    "Notepad wrap probe declaration",
)

text_buffer_test = r'''static void test_utf8_wrapped_render_and_navigation(void) {
    TextBuffer *tb = text_buffer_create();
    DrawList *list = draw_list_create();
    TEST_REQUIRE(tb != NULL);
    TEST_REQUIRE(list != NULL);
    TEST_REQUIRE(text_buffer_set_text(tb, "abñcdEF"));
    text_buffer_set_cursor(tb, 0, 8);

    TEST_REQUIRE(text_buffer_visual_row_count(tb, 4) == 2);
    TEST_REQUIRE(text_buffer_cursor_visual_row(tb, 4) == 1);

    RetroKeyEvent shift_up = key_modified(RETRO_KEY_UP, RETRO_MOD_SHIFT);
    TEST_REQUIRE(text_buffer_handle_key_wrapped(tb, &shift_up, 4));
    TEST_REQUIRE(text_buffer_cursor_row(tb) == 0);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 4);
    TEST_REQUIRE(text_buffer_cursor_visual_row(tb, 4) == 0);
    TEST_REQUIRE(text_buffer_has_selection(tb));

    size_t selected_length = 0;
    char *selected = text_buffer_selected_text(tb, &selected_length);
    TEST_REQUIRE(selected != NULL);
    TEST_REQUIRE(selected_length == 4);
    TEST_REQUIRE(strcmp(selected, "cdEF") == 0);
    free(selected);

    RenderStyle style = {
        RENDER_COLOR_WHITE, RENDER_COLOR_BLACK, false, false
    };
    RenderStyle cursor = {
        RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, true, false
    };
    RenderStyle selection = {
        .fg = RENDER_COLOR_BLACK,
        .bg = RENDER_COLOR_CYAN,
        .reverse = true,
        .bold = false,
    };
    text_buffer_render_wrapped_with_selection(
        tb, list, 0, 0, 2, 4, &style, &cursor, &selection);
    TEST_REQUIRE(draw_list_count(list) == 5);

    DrawCommandView command = {0};
    TEST_REQUIRE(draw_list_get(list, 0, &command));
    TEST_REQUIRE(strcmp(command.text, "abñc") == 0);
    TEST_REQUIRE(draw_list_get(list, 1, &command));
    TEST_REQUIRE(command.x == 3);
    TEST_REQUIRE(strcmp(command.text, "c") == 0);
    TEST_REQUIRE(draw_list_get(list, 2, &command));
    TEST_REQUIRE(strcmp(command.text, "dEF ") == 0);
    TEST_REQUIRE(draw_list_get(list, 3, &command));
    TEST_REQUIRE(command.x == 0);
    TEST_REQUIRE(strcmp(command.text, "dEF") == 0);
    TEST_REQUIRE(draw_list_get(list, 4, &command));
    TEST_REQUIRE(command.y == 0);
    TEST_REQUIRE(command.x == 3);
    TEST_REQUIRE(strcmp(command.text, "c") == 0);

    RetroKeyEvent down = key_special(RETRO_KEY_DOWN);
    TEST_REQUIRE(text_buffer_handle_key_wrapped(tb, &down, 4));
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 8);
    TEST_REQUIRE(text_buffer_cursor_visual_row(tb, 4) == 1);
    TEST_REQUIRE(!text_buffer_has_selection(tb));

    draw_list_destroy(list);
    text_buffer_destroy(tb);
    printf("  PASS: utf8_wrapped_render_and_navigation\n");
}

'''
replace_once(
    "tests/text_buffer_test.c",
    "static void test_utf8_find_next_and_wrap(void) {\n",
    text_buffer_test + "static void test_utf8_find_next_and_wrap(void) {\n",
    "TextBuffer word-wrap test",
)

replace_once(
    "tests/text_buffer_test.c",
    """    test_selection_render_overlay();
    test_utf8_find_next_and_wrap();
""",
    """    test_selection_render_overlay();
    test_utf8_wrapped_render_and_navigation();
    test_utf8_find_next_and_wrap();
""",
    "TextBuffer word-wrap test registration",
)

replace_once(
    "tests/text_buffer_test.c",
    """    TEST_REQUIRE(!text_buffer_handle_key(NULL, NULL));
    TEST_REQUIRE(!text_buffer_set_text(NULL, "test"));
""",
    """    TEST_REQUIRE(!text_buffer_handle_key(NULL, NULL));
    TEST_REQUIRE(!text_buffer_handle_key_wrapped(NULL, NULL, 4));
    TEST_REQUIRE(text_buffer_visual_row_count(NULL, 4) == 0);
    TEST_REQUIRE(text_buffer_cursor_visual_row(NULL, 4) == 0);
    TEST_REQUIRE(!text_buffer_set_text(NULL, "test"));
""",
    "TextBuffer word-wrap null safety",
)

runtime_test = r'''static void test_notepad_word_wrap_navigation(void) {
    RetroAppInstance instance = create_untitled_notepad();
    TEST_REQUIRE(!notepad_wrap_mode_for_test(&instance));
    TEST_REQUIRE(!notepad_dirty_for_test(&instance));
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) == 0);

    RetroEvent toggle = key_event(RETRO_KEY_F4, '\0');
    app_handle_event(&instance, &toggle);
    TEST_REQUIRE(notepad_wrap_mode_for_test(&instance));
    TEST_REQUIRE(!notepad_dirty_for_test(&instance));
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) == 0);

    for (int index = 0; index < 70; ++index) {
        type_notepad_char(&instance, 'a');
    }
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 70);
    size_t history_before_navigation = notepad_undo_count_for_test(&instance);

    RetroEvent up = key_event(RETRO_KEY_UP, '\0');
    app_handle_event(&instance, &up);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 6);
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) ==
                 history_before_navigation);

    RetroEvent down = key_event(RETRO_KEY_DOWN, '\0');
    app_handle_event(&instance, &down);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 70);
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) ==
                 history_before_navigation);

    RetroEvent shift_up = modified_key_event(
        RETRO_KEY_UP, RETRO_MOD_SHIFT);
    app_handle_event(&instance, &shift_up);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 6);
    RetroEvent copy = key_event(RETRO_KEY_CTRL_C, '\0');
    app_handle_event(&instance, &copy);
    TEST_REQUIRE(retro_clipboard_has_text());
    TEST_REQUIRE(strlen(retro_clipboard_text(NULL)) == 64);
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) ==
                 history_before_navigation);

    RetroEvent escape = key_event(RETRO_KEY_ESC, '\0');
    app_handle_event(&instance, &escape);
    app_handle_event(&instance, &down);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 70);

    app_handle_event(&instance, &toggle);
    TEST_REQUIRE(!notepad_wrap_mode_for_test(&instance));
    app_handle_event(&instance, &up);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 70);
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) ==
                 history_before_navigation);

    instance.descriptor->destroy(&instance);
    retro_clipboard_clear();
}

'''
replace_once(
    "tests/desktop_runtime_test.c",
    "static void test_notepad_shift_selection_replacement(void) {\n",
    runtime_test + "static void test_notepad_shift_selection_replacement(void) {\n",
    "Notepad word-wrap runtime test",
)

replace_once(
    "tests/desktop_runtime_test.c",
    """    test_notepad_utf8_find();
    test_notepad_shift_selection_replacement();
""",
    """    test_notepad_utf8_find();
    test_notepad_word_wrap_navigation();
    test_notepad_shift_selection_replacement();
""",
    "Notepad word-wrap test registration",
)

print("Notepad UTF-8 word-wrap port applied")
