#include "ui/text_buffer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "core/key_chord.h"
#include "core/utf8.h"

enum {
    TEXT_BUFFER_INIT_LINES = 16,
    TEXT_BUFFER_LINE_INIT_CAP = 64,
};

typedef struct TextLine {
    char *data;
    size_t len;
    size_t cap;
} TextLine;

typedef struct TextPosition {
    size_t row;
    size_t col;
} TextPosition;

typedef struct TextRange {
    TextPosition start;
    TextPosition end;
} TextRange;

struct TextBuffer {
    TextLine *lines;
    size_t line_count;
    size_t line_cap;
    size_t cursor_row;
    size_t cursor_col;
    size_t scroll_row;
    size_t scroll_col;
    bool selection_active;
    size_t selection_anchor_row;
    size_t selection_anchor_col;
};

static bool text_line_init(TextLine *line) {
    if (!line) return false;
    line->data = malloc(TEXT_BUFFER_LINE_INIT_CAP);
    if (!line->data) return false;
    line->data[0] = '\0';
    line->len = 0;
    line->cap = TEXT_BUFFER_LINE_INIT_CAP;
    return true;
}

static void text_line_free(TextLine *line) {
    if (!line) return;
    free(line->data);
    line->data = NULL;
    line->len = 0;
    line->cap = 0;
}

static bool text_line_grow(TextLine *line, size_t need) {
    if (!line) return false;
    if (need <= line->cap) return true;
    size_t new_cap = line->cap ? line->cap * 2 : TEXT_BUFFER_LINE_INIT_CAP;
    while (new_cap < need) {
        if (new_cap > SIZE_MAX / 2) {
            new_cap = need;
            break;
        }
        new_cap *= 2;
    }
    char *next = realloc(line->data, new_cap);
    if (!next) return false;
    line->data = next;
    line->cap = new_cap;
    return true;
}

static bool text_line_set(TextLine *line, const char *text, size_t len) {
    if (!line) return false;
    if (!text_line_grow(line, len + 1)) return false;
    if (len > 0 && text) memcpy(line->data, text, len);
    line->data[len] = '\0';
    line->len = len;
    return true;
}

static bool text_buffer_grow_lines(TextBuffer *buf, size_t need) {
    if (!buf) return false;
    if (need <= buf->line_cap) return true;
    size_t new_cap = buf->line_cap ? buf->line_cap * 2 : TEXT_BUFFER_INIT_LINES;
    while (new_cap < need) {
        if (new_cap > SIZE_MAX / 2) {
            new_cap = need;
            break;
        }
        new_cap *= 2;
    }
    TextLine *next = realloc(buf->lines, new_cap * sizeof(*next));
    if (!next) return false;
    buf->lines = next;
    buf->line_cap = new_cap;
    return true;
}

static size_t text_line_boundary_at_or_before(const TextLine *line,
                                              size_t offset) {
    if (!line || !line->data) return 0;
    if (offset >= line->len) return line->len;

    size_t position = 0;
    while (position < line->len) {
        if (position == offset) return position;
        size_t next = retro_utf8_next(line->data, line->len, position);
        if (next <= position || next > offset) return position;
        position = next;
    }
    return line->len;
}

static void text_buffer_clamp_cursor(TextBuffer *buf) {
    if (!buf || buf->line_count == 0) return;
    if (buf->cursor_row >= buf->line_count) {
        buf->cursor_row = buf->line_count - 1;
    }
    TextLine *line = &buf->lines[buf->cursor_row];
    buf->cursor_col = text_line_boundary_at_or_before(line, buf->cursor_col);
}

static int text_position_compare(TextPosition left, TextPosition right) {
    if (left.row < right.row) return -1;
    if (left.row > right.row) return 1;
    if (left.col < right.col) return -1;
    if (left.col > right.col) return 1;
    return 0;
}

static bool text_buffer_selection_range(const TextBuffer *buf,
                                        TextRange *range) {
    if (!buf || !range || !buf->selection_active ||
        buf->line_count == 0) {
        return false;
    }

    TextPosition anchor = {
        buf->selection_anchor_row,
        buf->selection_anchor_col,
    };
    TextPosition cursor = {buf->cursor_row, buf->cursor_col};
    if (text_position_compare(anchor, cursor) == 0) return false;

    if (text_position_compare(anchor, cursor) < 0) {
        range->start = anchor;
        range->end = cursor;
    } else {
        range->start = cursor;
        range->end = anchor;
    }
    return true;
}

static size_t text_line_columns_to(const TextLine *line,
                                   size_t byte_offset) {
    if (!line) return 0;
    size_t end = text_line_boundary_at_or_before(line, byte_offset);
    return retro_utf8_columns(line->data, line->len, 0, end);
}

static size_t text_line_byte_for_columns(const TextLine *line,
                                         size_t columns) {
    if (!line) return 0;
    return retro_utf8_prefix_bytes(line->data, line->len, columns);
}

static size_t text_buffer_document_offset(const TextBuffer *buf,
                                          TextPosition position) {
    if (!buf || buf->line_count == 0) return 0;
    if (position.row >= buf->line_count) position.row = buf->line_count - 1;
    position.col = text_line_boundary_at_or_before(
        &buf->lines[position.row], position.col);

    size_t offset = 0;
    for (size_t row = 0; row < position.row; ++row) {
        offset += buf->lines[row].len;
        offset++;
    }
    return offset + position.col;
}

static void text_buffer_set_cursor_from_offset(TextBuffer *buf,
                                               size_t offset) {
    if (!buf || buf->line_count == 0) return;
    for (size_t row = 0; row < buf->line_count; ++row) {
        size_t line_len = buf->lines[row].len;
        if (offset <= line_len) {
            buf->cursor_row = row;
            buf->cursor_col = text_line_boundary_at_or_before(
                &buf->lines[row], offset);
            return;
        }
        offset -= line_len;
        if (row + 1 < buf->line_count && offset > 0) offset--;
    }
    buf->cursor_row = buf->line_count - 1;
    buf->cursor_col = buf->lines[buf->cursor_row].len;
}

TextBuffer *text_buffer_create(void) {
    TextBuffer *buf = calloc(1, sizeof(*buf));
    if (!buf) return NULL;
    if (!text_buffer_grow_lines(buf, TEXT_BUFFER_INIT_LINES)) {
        free(buf);
        return NULL;
    }
    if (!text_line_init(&buf->lines[0])) {
        free(buf->lines);
        free(buf);
        return NULL;
    }
    buf->line_count = 1;
    return buf;
}

void text_buffer_destroy(TextBuffer *buf) {
    if (!buf) return;
    for (size_t index = 0; index < buf->line_count; ++index) {
        text_line_free(&buf->lines[index]);
    }
    free(buf->lines);
    free(buf);
}

void text_buffer_clear_selection(TextBuffer *buf) {
    if (!buf) return;
    buf->selection_active = false;
    buf->selection_anchor_row = 0;
    buf->selection_anchor_col = 0;
}

void text_buffer_clear(TextBuffer *buf) {
    if (!buf) return;
    for (size_t index = 0; index < buf->line_count; ++index) {
        text_line_free(&buf->lines[index]);
    }
    if (!text_line_init(&buf->lines[0])) {
        buf->line_count = 0;
        return;
    }
    buf->line_count = 1;
    buf->cursor_row = 0;
    buf->cursor_col = 0;
    buf->scroll_row = 0;
    buf->scroll_col = 0;
    text_buffer_clear_selection(buf);
}

bool text_buffer_set_text(TextBuffer *buf, const char *text) {
    if (!buf) return false;
    if (!text) text = "";
    size_t text_len = strlen(text);
    if (!retro_utf8_validate(text, text_len)) return false;

    TextLine *next_lines = NULL;
    size_t next_count = 0;
    size_t next_cap = 0;
    const char *cursor = text;

    for (;;) {
        const char *newline = strchr(cursor, '\n');
        size_t segment_len = newline
                                 ? (size_t)(newline - cursor)
                                 : strlen(cursor);

        if (next_count == next_cap) {
            size_t grown = next_cap ? next_cap * 2 : TEXT_BUFFER_INIT_LINES;
            TextLine *candidate = realloc(next_lines,
                                          grown * sizeof(*candidate));
            if (!candidate) goto fail;
            next_lines = candidate;
            next_cap = grown;
        }
        memset(&next_lines[next_count], 0, sizeof(next_lines[next_count]));
        if (!text_line_init(&next_lines[next_count])) goto fail;
        if (!text_line_set(&next_lines[next_count], cursor, segment_len)) {
            text_line_free(&next_lines[next_count]);
            goto fail;
        }
        next_count++;

        if (!newline) break;
        cursor = newline + 1;
    }

    for (size_t index = 0; index < buf->line_count; ++index) {
        text_line_free(&buf->lines[index]);
    }
    free(buf->lines);
    buf->lines = next_lines;
    buf->line_count = next_count;
    buf->line_cap = next_cap;
    buf->cursor_row = next_count - 1;
    buf->cursor_col = buf->lines[buf->cursor_row].len;
    buf->scroll_row = 0;
    buf->scroll_col = 0;
    text_buffer_clear_selection(buf);
    return true;

fail:
    for (size_t index = 0; index < next_count; ++index) {
        text_line_free(&next_lines[index]);
    }
    free(next_lines);
    return false;
}

size_t text_buffer_line_count(const TextBuffer *buf) {
    return buf ? buf->line_count : 0;
}

const char *text_buffer_line(const TextBuffer *buf, size_t row) {
    if (!buf || row >= buf->line_count) return "";
    return buf->lines[row].data;
}

size_t text_buffer_line_length(const TextBuffer *buf, size_t row) {
    if (!buf || row >= buf->line_count) return 0;
    return buf->lines[row].len;
}

char *text_buffer_to_text(const TextBuffer *buf, size_t *length) {
    if (length) *length = 0;
    if (!buf || buf->line_count == 0) return NULL;

    size_t total = 0;
    for (size_t index = 0; index < buf->line_count; ++index) {
        if (buf->lines[index].len > SIZE_MAX - total) return NULL;
        total += buf->lines[index].len;
        if (index + 1 < buf->line_count) {
            if (total == SIZE_MAX) return NULL;
            total++;
        }
    }

    char *out = malloc(total + 1);
    if (!out) return NULL;
    size_t offset = 0;
    for (size_t index = 0; index < buf->line_count; ++index) {
        memcpy(out + offset, buf->lines[index].data,
               buf->lines[index].len);
        offset += buf->lines[index].len;
        if (index + 1 < buf->line_count) out[offset++] = '\n';
    }
    out[offset] = '\0';
    if (length) *length = offset;
    return out;
}

size_t text_buffer_cursor_row(const TextBuffer *buf) {
    return buf ? buf->cursor_row : 0;
}

size_t text_buffer_cursor_col(const TextBuffer *buf) {
    return buf ? buf->cursor_col : 0;
}

void text_buffer_set_cursor(TextBuffer *buf, size_t row, size_t col) {
    if (!buf || buf->line_count == 0) return;
    if (row >= buf->line_count) row = buf->line_count - 1;
    buf->cursor_row = row;
    buf->cursor_col = col;
    text_buffer_clamp_cursor(buf);
    text_buffer_clear_selection(buf);
}

bool text_buffer_has_selection(const TextBuffer *buf) {
    TextRange range = {0};
    return text_buffer_selection_range(buf, &range);
}

void text_buffer_select_all(TextBuffer *buf) {
    if (!buf || buf->line_count == 0) return;
    buf->selection_anchor_row = 0;
    buf->selection_anchor_col = 0;
    buf->cursor_row = buf->line_count - 1;
    buf->cursor_col = buf->lines[buf->cursor_row].len;
    buf->selection_active =
        buf->cursor_row != 0 || buf->cursor_col != 0;
}

char *text_buffer_selected_text(const TextBuffer *buf, size_t *length) {
    if (length) *length = 0;
    TextRange range = {0};
    if (!text_buffer_selection_range(buf, &range)) return NULL;

    size_t document_length = 0;
    char *document = text_buffer_to_text(buf, &document_length);
    if (!document) return NULL;

    size_t start = text_buffer_document_offset(buf, range.start);
    size_t end = text_buffer_document_offset(buf, range.end);
    if (start > end || end > document_length) {
        free(document);
        return NULL;
    }

    size_t selected_length = end - start;
    char *selected = malloc(selected_length + 1);
    if (!selected) {
        free(document);
        return NULL;
    }
    if (selected_length > 0) {
        memcpy(selected, document + start, selected_length);
    }
    selected[selected_length] = '\0';
    free(document);
    if (length) *length = selected_length;
    return selected;
}

size_t text_buffer_scroll_row(const TextBuffer *buf) {
    return buf ? buf->scroll_row : 0;
}

size_t text_buffer_scroll_col(const TextBuffer *buf) {
    return buf ? buf->scroll_col : 0;
}

static bool text_buffer_replace_range(TextBuffer *buf, TextRange range,
                                      const char *inserted,
                                      size_t inserted_length) {
    if (!buf || (!inserted && inserted_length != 0)) return false;
    if (!retro_utf8_validate(inserted, inserted_length)) return false;
    if (inserted_length > 0 && memchr(inserted, '\0', inserted_length)) {
        return false;
    }

    size_t document_length = 0;
    char *document = text_buffer_to_text(buf, &document_length);
    if (!document) return false;

    size_t start = text_buffer_document_offset(buf, range.start);
    size_t end = text_buffer_document_offset(buf, range.end);
    if (start > end || end > document_length ||
        start > SIZE_MAX - inserted_length ||
        start + inserted_length > SIZE_MAX - (document_length - end)) {
        free(document);
        return false;
    }

    size_t next_length = start + inserted_length +
                         (document_length - end);
    char *next = malloc(next_length + 1);
    if (!next) {
        free(document);
        return false;
    }

    if (start > 0) memcpy(next, document, start);
    if (inserted_length > 0) {
        memcpy(next + start, inserted, inserted_length);
    }
    if (document_length > end) {
        memcpy(next + start + inserted_length, document + end,
               document_length - end);
    }
    next[next_length] = '\0';

    bool replaced = text_buffer_set_text(buf, next);
    free(next);
    free(document);
    if (!replaced) return false;

    text_buffer_set_cursor_from_offset(buf, start + inserted_length);
    text_buffer_clear_selection(buf);
    return true;
}

bool text_buffer_insert_text(TextBuffer *buf, const char *text,
                             size_t length) {
    if (!buf || (!text && length != 0)) return false;
    TextRange range = {
        .start = {buf->cursor_row, buf->cursor_col},
        .end = {buf->cursor_row, buf->cursor_col},
    };
    (void)text_buffer_selection_range(buf, &range);
    if (length == 0 && text_position_compare(range.start, range.end) == 0) {
        return false;
    }
    return text_buffer_replace_range(buf, range, text, length);
}

bool text_buffer_insert_codepoint(TextBuffer *buf, uint32_t codepoint) {
    if (!buf || buf->line_count == 0 || codepoint == 0 ||
        codepoint == '\n' || codepoint == '\r') {
        return false;
    }

    char encoded[4];
    size_t encoded_len = 0;
    if (!retro_utf8_encode(codepoint, encoded, &encoded_len)) return false;
    if (text_buffer_has_selection(buf)) {
        return text_buffer_insert_text(buf, encoded, encoded_len);
    }

    TextLine *line = &buf->lines[buf->cursor_row];
    text_buffer_clamp_cursor(buf);
    if (line->len > SIZE_MAX - encoded_len - 1) return false;
    if (!text_line_grow(line, line->len + encoded_len + 1)) return false;

    memmove(line->data + buf->cursor_col + encoded_len,
            line->data + buf->cursor_col,
            line->len - buf->cursor_col + 1);
    memcpy(line->data + buf->cursor_col, encoded, encoded_len);
    line->len += encoded_len;
    buf->cursor_col += encoded_len;
    return true;
}

bool text_buffer_insert_char(TextBuffer *buf, char ch) {
    unsigned char byte = (unsigned char)ch;
    if (byte == 0) return false;
    return text_buffer_insert_codepoint(buf, (uint32_t)byte);
}

bool text_buffer_insert_newline(TextBuffer *buf) {
    if (!buf || buf->line_count == 0) return false;
    if (text_buffer_has_selection(buf)) {
        return text_buffer_insert_text(buf, "\n", 1);
    }
    text_buffer_clamp_cursor(buf);

    const TextLine *current = &buf->lines[buf->cursor_row];
    size_t col = buf->cursor_col;
    size_t tail_len = current->len - col;

    TextLine new_line = {0};
    if (!text_line_init(&new_line)) return false;
    if (!text_line_set(&new_line, current->data + col, tail_len)) {
        text_line_free(&new_line);
        return false;
    }

    if (!text_buffer_grow_lines(buf, buf->line_count + 1)) {
        text_line_free(&new_line);
        return false;
    }

    size_t insert_at = buf->cursor_row + 1;
    if (insert_at < buf->line_count) {
        memmove(&buf->lines[insert_at + 1],
                &buf->lines[insert_at],
                (buf->line_count - insert_at) * sizeof(TextLine));
    }
    buf->lines[insert_at] = new_line;
    buf->line_count++;

    TextLine *head = &buf->lines[buf->cursor_row];
    head->data[col] = '\0';
    head->len = col;
    buf->cursor_row = insert_at;
    buf->cursor_col = 0;
    return true;
}

bool text_buffer_delete_selection(TextBuffer *buf) {
    if (!buf || !text_buffer_has_selection(buf)) return false;
    return text_buffer_insert_text(buf, "", 0);
}

bool text_buffer_delete_backward(TextBuffer *buf) {
    if (!buf || buf->line_count == 0) return false;
    if (text_buffer_has_selection(buf)) {
        return text_buffer_delete_selection(buf);
    }
    text_buffer_clamp_cursor(buf);

    if (buf->cursor_col > 0) {
        TextLine *line = &buf->lines[buf->cursor_row];
        size_t previous = retro_utf8_prev(line->data, buf->cursor_col);
        size_t removed = buf->cursor_col - previous;
        memmove(line->data + previous,
                line->data + buf->cursor_col,
                line->len - buf->cursor_col + 1);
        line->len -= removed;
        buf->cursor_col = previous;
        return true;
    }

    if (buf->cursor_row == 0) return false;
    size_t previous_row = buf->cursor_row - 1;
    TextLine *previous = &buf->lines[previous_row];
    TextLine *current = &buf->lines[buf->cursor_row];
    size_t old_previous_len = previous->len;

    if (current->len > 0) {
        if (!text_line_grow(previous,
                            previous->len + current->len + 1)) {
            return false;
        }
        memcpy(previous->data + previous->len,
               current->data, current->len);
        previous->len += current->len;
        previous->data[previous->len] = '\0';
    }

    text_line_free(current);
    size_t remove_at = buf->cursor_row;
    if (remove_at + 1 < buf->line_count) {
        memmove(&buf->lines[remove_at],
                &buf->lines[remove_at + 1],
                (buf->line_count - remove_at - 1) * sizeof(TextLine));
    }
    buf->line_count--;
    buf->cursor_row = previous_row;
    buf->cursor_col = old_previous_len;
    return true;
}

bool text_buffer_delete_forward(TextBuffer *buf) {
    if (!buf || buf->line_count == 0) return false;
    if (text_buffer_has_selection(buf)) {
        return text_buffer_delete_selection(buf);
    }
    text_buffer_clamp_cursor(buf);
    TextLine *line = &buf->lines[buf->cursor_row];

    if (buf->cursor_col < line->len) {
        size_t next = retro_utf8_next(line->data, line->len,
                                      buf->cursor_col);
        if (next <= buf->cursor_col) return false;
        size_t removed = next - buf->cursor_col;
        memmove(line->data + buf->cursor_col,
                line->data + next,
                line->len - next + 1);
        line->len -= removed;
        return true;
    }

    if (buf->cursor_row + 1 >= buf->line_count) return false;
    size_t next_row = buf->cursor_row + 1;
    TextLine *next_line = &buf->lines[next_row];

    if (next_line->len > 0) {
        if (!text_line_grow(line, line->len + next_line->len + 1)) {
            return false;
        }
        memcpy(line->data + line->len,
               next_line->data, next_line->len);
        line->len += next_line->len;
        line->data[line->len] = '\0';
    }

    text_line_free(next_line);
    if (next_row + 1 < buf->line_count) {
        memmove(&buf->lines[next_row],
                &buf->lines[next_row + 1],
                (buf->line_count - next_row - 1) * sizeof(TextLine));
    }
    buf->line_count--;
    return true;
}

static bool text_buffer_kv_home(TextBuffer *buf) {
    buf->cursor_col = 0;
    return true;
}

static bool text_buffer_kv_end(TextBuffer *buf) {
    buf->cursor_col = buf->lines[buf->cursor_row].len;
    return true;
}

static bool text_buffer_kv_kill_to_end(TextBuffer *buf) {
    if (text_buffer_has_selection(buf)) {
        return text_buffer_delete_selection(buf);
    }
    text_buffer_clamp_cursor(buf);
    TextLine *line = &buf->lines[buf->cursor_row];
    if (buf->cursor_col >= line->len) return false;
    line->data[buf->cursor_col] = '\0';
    line->len = buf->cursor_col;
    return true;
}

static bool text_buffer_kv_left(TextBuffer *buf) {
    text_buffer_clamp_cursor(buf);
    if (buf->cursor_col > 0) {
        TextLine *line = &buf->lines[buf->cursor_row];
        buf->cursor_col = retro_utf8_prev(line->data, buf->cursor_col);
    } else if (buf->cursor_row > 0) {
        buf->cursor_row--;
        buf->cursor_col = buf->lines[buf->cursor_row].len;
    }
    return true;
}

static bool text_buffer_kv_right(TextBuffer *buf) {
    text_buffer_clamp_cursor(buf);
    TextLine *line = &buf->lines[buf->cursor_row];
    if (buf->cursor_col < line->len) {
        buf->cursor_col = retro_utf8_next(line->data, line->len,
                                          buf->cursor_col);
    } else if (buf->cursor_row + 1 < buf->line_count) {
        buf->cursor_row++;
        buf->cursor_col = 0;
    }
    return true;
}

static bool text_buffer_kv_up(TextBuffer *buf) {
    if (buf->cursor_row > 0) {
        size_t desired = text_line_columns_to(
            &buf->lines[buf->cursor_row], buf->cursor_col);
        buf->cursor_row--;
        buf->cursor_col = text_line_byte_for_columns(
            &buf->lines[buf->cursor_row], desired);
    }
    return true;
}

static bool text_buffer_kv_down(TextBuffer *buf) {
    if (buf->cursor_row + 1 < buf->line_count) {
        size_t desired = text_line_columns_to(
            &buf->lines[buf->cursor_row], buf->cursor_col);
        buf->cursor_row++;
        buf->cursor_col = text_line_byte_for_columns(
            &buf->lines[buf->cursor_row], desired);
    }
    return true;
}

typedef bool (*TextBufferKeyFn)(TextBuffer *buf);

typedef struct TextBufferKeyBinding {
    int code;
    TextBufferKeyFn function;
    bool movement;
} TextBufferKeyBinding;

static const TextBufferKeyBinding text_buffer_key_bindings[] = {
    {RETRO_KEY_LF, text_buffer_insert_newline, false},
    {RETRO_KEY_CR, text_buffer_insert_newline, false},
    {RETRO_KEY_BS, text_buffer_delete_backward, false},
    {RETRO_KEY_DEL, text_buffer_delete_backward, false},
    {RETRO_KEY_CTRL_A, text_buffer_kv_home, true},
    {RETRO_KEY_CTRL_E, text_buffer_kv_end, true},
    {RETRO_KEY_CTRL_K, text_buffer_kv_kill_to_end, false},
    {RETRO_KEY_CTRL_D, text_buffer_delete_forward, false},
    {RETRO_KEY_LEFT, text_buffer_kv_left, true},
    {RETRO_KEY_RIGHT, text_buffer_kv_right, true},
    {RETRO_KEY_UP, text_buffer_kv_up, true},
    {RETRO_KEY_DOWN, text_buffer_kv_down, true},
    {RETRO_KEY_HOME, text_buffer_kv_home, true},
    {RETRO_KEY_END, text_buffer_kv_end, true},
    {RETRO_KEY_DC, text_buffer_delete_forward, false},
};

static void text_buffer_prepare_movement(TextBuffer *buf,
                                         const RetroKeyEvent *key) {
    if (!buf || !key) return;
    if ((key->modifiers & RETRO_MOD_SHIFT) != 0) {
        if (!buf->selection_active) {
            buf->selection_active = true;
            buf->selection_anchor_row = buf->cursor_row;
            buf->selection_anchor_col = buf->cursor_col;
        }
    } else {
        text_buffer_clear_selection(buf);
    }
}

bool text_buffer_handle_key(TextBuffer *buf, const RetroKeyEvent *key) {
    if (!buf || !key) return false;

    size_t binding_count = sizeof(text_buffer_key_bindings) /
                           sizeof(text_buffer_key_bindings[0]);
    for (size_t index = 0; index < binding_count; ++index) {
        if (text_buffer_key_bindings[index].code == key->key_code) {
            if (text_buffer_key_bindings[index].movement) {
                text_buffer_prepare_movement(buf, key);
            }
            return text_buffer_key_bindings[index].function(buf);
        }
    }

    if (key->is_printable) {
        uint32_t codepoint = key->text_codepoint;
        if (codepoint == 0 && key->ascii > 0) codepoint = key->ascii;
        if (codepoint >= 0x20u && codepoint != 0x7fu) {
            return text_buffer_insert_codepoint(buf, codepoint);
        }
    }
    return false;
}

static size_t text_line_cursor_width(const TextLine *line,
                                     size_t cursor_col) {
    if (!line || cursor_col >= line->len) return 1;
    uint32_t codepoint = 0;
    size_t byte_len = 0;
    if (!retro_utf8_decode(line->data, line->len, cursor_col,
                           &codepoint, &byte_len)) {
        return 1;
    }
    (void)byte_len;
    int width = retro_utf8_width(codepoint);
    return width > 0 ? (size_t)width : 1;
}

static size_t text_line_scroll_start(const TextLine *line,
                                     size_t requested_start,
                                     size_t cursor_col,
                                     size_t visible_cols) {
    if (!line || visible_cols == 0) return 0;
    size_t start = text_line_boundary_at_or_before(line, requested_start);
    size_t cursor = text_line_boundary_at_or_before(line, cursor_col);
    if (cursor < start) start = cursor;

    size_t cursor_width = text_line_cursor_width(line, cursor);
    while (start < cursor) {
        size_t columns = retro_utf8_columns(line->data, line->len,
                                            start, cursor);
        if (columns + cursor_width <= visible_cols) break;
        size_t next = retro_utf8_next(line->data, line->len, start);
        if (next <= start) break;
        start = next;
    }
    return start;
}

static char *text_line_visible_text(const TextLine *line,
                                    size_t start,
                                    size_t visible_cols) {
    size_t remaining = line && start < line->len ? line->len - start : 0;
    if (remaining > SIZE_MAX - visible_cols - 1) return NULL;
    size_t capacity = remaining + visible_cols + 1;
    char *out = malloc(capacity);
    if (!out) return NULL;

    size_t out_len = 0;
    size_t used_cols = 0;
    size_t position = start;
    while (line && position < line->len) {
        uint32_t codepoint = 0;
        size_t byte_len = 0;
        if (!retro_utf8_decode(line->data, line->len, position,
                               &codepoint, &byte_len)) {
            codepoint = '?';
            byte_len = 1;
        }
        int width_value = retro_utf8_width(codepoint);
        size_t width = width_value > 0 ? (size_t)width_value : 0;
        if (width > 0 && used_cols + width > visible_cols) break;
        if (out_len + byte_len >= capacity) break;
        memcpy(out + out_len, line->data + position, byte_len);
        out_len += byte_len;
        used_cols += width;
        position += byte_len;
    }

    while (used_cols < visible_cols && out_len + 1 < capacity) {
        out[out_len++] = ' ';
        used_cols++;
    }
    out[out_len] = '\0';
    return out;
}

static char *text_line_range_text(const TextLine *line,
                                  size_t start, size_t end,
                                  size_t max_columns) {
    if (!line || start >= end || start >= line->len) return NULL;
    start = text_line_boundary_at_or_before(line, start);
    end = text_line_boundary_at_or_before(line, end);
    if (end <= start) return NULL;

    size_t capacity = end - start + 1;
    char *out = malloc(capacity);
    if (!out) return NULL;
    size_t out_len = 0;
    size_t used_columns = 0;
    size_t position = start;

    while (position < end) {
        uint32_t codepoint = 0;
        size_t byte_len = 0;
        if (!retro_utf8_decode(line->data, line->len, position,
                               &codepoint, &byte_len)) {
            free(out);
            return NULL;
        }
        int width_value = retro_utf8_width(codepoint);
        size_t width = width_value > 0 ? (size_t)width_value : 0;
        if (width > 0 && used_columns + width > max_columns) break;
        memcpy(out + out_len, line->data + position, byte_len);
        out_len += byte_len;
        used_columns += width;
        position += byte_len;
    }
    out[out_len] = '\0';
    return out;
}

static void text_buffer_render_selection_line(
    const TextBuffer *buf, DrawList *draw_list,
    const TextRange *range, size_t buffer_row,
    size_t line_start, size_t visible_cols,
    int y, int x, const RenderStyle *selection_style) {
    if (!buf || !draw_list || !range || !selection_style ||
        buffer_row >= buf->line_count || buffer_row < range->start.row ||
        buffer_row > range->end.row) {
        return;
    }

    const TextLine *line = &buf->lines[buffer_row];
    size_t selected_start =
        buffer_row == range->start.row ? range->start.col : 0;
    size_t selected_end =
        buffer_row == range->end.row ? range->end.col : line->len;
    selected_start = text_line_boundary_at_or_before(line, selected_start);
    selected_end = text_line_boundary_at_or_before(line, selected_end);

    if (selected_end > line_start && selected_start < line->len) {
        size_t visible_start = selected_start > line_start
                                   ? selected_start
                                   : line_start;
        size_t x_columns = retro_utf8_columns(
            line->data, line->len, line_start, visible_start);
        if (x_columns < visible_cols) {
            char *selected = text_line_range_text(
                line, visible_start, selected_end,
                visible_cols - x_columns);
            if (selected && selected[0]) {
                draw_list_text(draw_list, y, x + (int)x_columns,
                               selected, selection_style);
            }
            free(selected);
        }
    }

    if (buffer_row < range->end.row && line->len >= line_start) {
        size_t newline_x = retro_utf8_columns(
            line->data, line->len, line_start, line->len);
        if (newline_x < visible_cols) {
            draw_list_text(draw_list, y, x + (int)newline_x,
                           " ", selection_style);
        }
    }
}

void text_buffer_render_with_selection(
    const TextBuffer *buf, DrawList *draw_list,
    int y, int x, int visible_rows, int visible_cols,
    const RenderStyle *style, const RenderStyle *cursor_style,
    const RenderStyle *selection_style) {
    if (!buf || !draw_list || !style ||
        visible_rows <= 0 || visible_cols <= 0 ||
        buf->line_count == 0) {
        return;
    }

    size_t scroll_row = buf->scroll_row;
    size_t rows = (size_t)visible_rows;
    size_t cols = (size_t)visible_cols;
    if (buf->cursor_row < scroll_row) scroll_row = buf->cursor_row;
    if (buf->cursor_row >= scroll_row + rows) {
        scroll_row = buf->cursor_row - rows + 1;
    }

    const TextLine *cursor_line = &buf->lines[buf->cursor_row];
    size_t scroll_col = text_line_scroll_start(
        cursor_line, buf->scroll_col, buf->cursor_col, cols);
    TextRange selection = {0};
    bool has_selection = text_buffer_selection_range(buf, &selection);

    for (size_t row = 0; row < rows; ++row) {
        size_t buffer_row = scroll_row + row;
        const TextLine *line =
            buffer_row < buf->line_count ? &buf->lines[buffer_row] : NULL;
        size_t line_start = line
                                ? text_line_boundary_at_or_before(
                                      line, scroll_col)
                                : 0;
        char *visible = text_line_visible_text(line, line_start, cols);
        if (visible) {
            draw_list_text(draw_list, y + (int)row, x, visible, style);
            free(visible);
        }
        if (has_selection && line) {
            text_buffer_render_selection_line(
                buf, draw_list, &selection, buffer_row,
                line_start, cols, y + (int)row, x,
                selection_style);
        }
    }

    if (!cursor_style || buf->cursor_row < scroll_row ||
        buf->cursor_row >= scroll_row + rows) {
        return;
    }

    size_t cursor_x = retro_utf8_columns(
        cursor_line->data, cursor_line->len,
        scroll_col, buf->cursor_col);
    if (cursor_x >= cols) return;

    char cursor_text[5] = {' ', '\0', '\0', '\0', '\0'};
    if (buf->cursor_col < cursor_line->len) {
        uint32_t codepoint = 0;
        size_t byte_len = 0;
        if (retro_utf8_decode(cursor_line->data, cursor_line->len,
                              buf->cursor_col, &codepoint, &byte_len) &&
            byte_len <= 4) {
            (void)codepoint;
            memcpy(cursor_text,
                   cursor_line->data + buf->cursor_col,
                   byte_len);
            cursor_text[byte_len] = '\0';
        }
    }

    int cursor_y = y + (int)(buf->cursor_row - scroll_row);
    draw_list_text(draw_list, cursor_y, x + (int)cursor_x,
                   cursor_text, cursor_style);
}

void text_buffer_render(const TextBuffer *buf, DrawList *draw_list,
                        int y, int x, int visible_rows, int visible_cols,
                        const RenderStyle *style,
                        const RenderStyle *cursor_style) {
    text_buffer_render_with_selection(
        buf, draw_list, y, x, visible_rows, visible_cols,
        style, cursor_style, NULL);
}
