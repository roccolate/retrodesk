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

struct TextBuffer {
    TextLine *lines;
    size_t line_count;
    size_t line_cap;
    size_t cursor_row;
    size_t cursor_col;
    size_t scroll_row;
    size_t scroll_col;
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

static bool text_line_set(TextLine *line, const char *str, size_t len) {
    if (!line) return false;
    if (!text_line_grow(line, len + 1)) return false;
    if (len > 0 && str) memcpy(line->data, str, len);
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

    size_t pos = 0;
    while (pos < line->len) {
        if (pos == offset) return pos;
        size_t next = retro_utf8_next(line->data, line->len, pos);
        if (next <= pos || next > offset) return pos;
        pos = next;
    }
    return line->len;
}

static void text_buffer_clamp_col(TextBuffer *buf) {
    if (!buf || buf->line_count == 0) return;
    if (buf->cursor_row >= buf->line_count) {
        buf->cursor_row = buf->line_count - 1;
    }
    TextLine *line = &buf->lines[buf->cursor_row];
    buf->cursor_col = text_line_boundary_at_or_before(line, buf->cursor_col);
}

static size_t text_line_columns_to(const TextLine *line, size_t byte_offset) {
    if (!line) return 0;
    size_t end = text_line_boundary_at_or_before(line, byte_offset);
    return retro_utf8_columns(line->data, line->len, 0, end);
}

static size_t text_line_byte_for_columns(const TextLine *line,
                                         size_t columns) {
    if (!line) return 0;
    return retro_utf8_prefix_bytes(line->data, line->len, columns);
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
    for (size_t i = 0; i < buf->line_count; ++i) {
        text_line_free(&buf->lines[i]);
    }
    free(buf->lines);
    free(buf);
}

void text_buffer_clear(TextBuffer *buf) {
    if (!buf) return;
    for (size_t i = 0; i < buf->line_count; ++i) {
        text_line_free(&buf->lines[i]);
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
}

bool text_buffer_set_text(TextBuffer *buf, const char *text) {
    if (!buf) return false;
    if (!text) text = "";
    size_t text_len = strlen(text);
    if (!retro_utf8_validate(text, text_len)) return false;

    TextLine *next_lines = NULL;
    size_t next_count = 0;
    size_t next_cap = 0;
    const char *p = text;

    for (;;) {
        const char *nl = strchr(p, '\n');
        size_t segment_len = nl ? (size_t)(nl - p) : strlen(p);

        if (next_count == next_cap) {
            size_t grown = next_cap ? next_cap * 2 : TEXT_BUFFER_INIT_LINES;
            TextLine *candidate = realloc(next_lines, grown * sizeof(*candidate));
            if (!candidate) goto fail;
            next_lines = candidate;
            next_cap = grown;
        }
        memset(&next_lines[next_count], 0, sizeof(next_lines[next_count]));
        if (!text_line_init(&next_lines[next_count])) goto fail;
        if (!text_line_set(&next_lines[next_count], p, segment_len)) {
            text_line_free(&next_lines[next_count]);
            goto fail;
        }
        next_count++;

        if (!nl) break;
        p = nl + 1;
    }

    for (size_t i = 0; i < buf->line_count; ++i) {
        text_line_free(&buf->lines[i]);
    }
    free(buf->lines);
    buf->lines = next_lines;
    buf->line_count = next_count;
    buf->line_cap = next_cap;
    buf->cursor_row = next_count - 1;
    buf->cursor_col = buf->lines[buf->cursor_row].len;
    buf->scroll_row = 0;
    buf->scroll_col = 0;
    return true;

fail:
    for (size_t i = 0; i < next_count; ++i) text_line_free(&next_lines[i]);
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
    for (size_t i = 0; i < buf->line_count; ++i) {
        if (buf->lines[i].len > SIZE_MAX - total) return NULL;
        total += buf->lines[i].len;
        if (i + 1 < buf->line_count) {
            if (total == SIZE_MAX) return NULL;
            total++;
        }
    }

    char *out = malloc(total + 1);
    if (!out) return NULL;
    size_t offset = 0;
    for (size_t i = 0; i < buf->line_count; ++i) {
        memcpy(out + offset, buf->lines[i].data, buf->lines[i].len);
        offset += buf->lines[i].len;
        if (i + 1 < buf->line_count) out[offset++] = '\n';
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
    text_buffer_clamp_col(buf);
}

size_t text_buffer_scroll_row(const TextBuffer *buf) {
    return buf ? buf->scroll_row : 0;
}

size_t text_buffer_scroll_col(const TextBuffer *buf) {
    return buf ? buf->scroll_col : 0;
}

bool text_buffer_insert_codepoint(TextBuffer *buf, uint32_t codepoint) {
    if (!buf || buf->line_count == 0 || codepoint == 0 ||
        codepoint == '\n' || codepoint == '\r') {
        return false;
    }

    char encoded[4];
    size_t encoded_len = 0;
    if (!retro_utf8_encode(codepoint, encoded, &encoded_len)) return false;

    TextLine *line = &buf->lines[buf->cursor_row];
    text_buffer_clamp_col(buf);
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
    text_buffer_clamp_col(buf);

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

bool text_buffer_delete_backward(TextBuffer *buf) {
    if (!buf || buf->line_count == 0) return false;
    text_buffer_clamp_col(buf);

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
    text_buffer_clamp_col(buf);
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
    text_buffer_clamp_col(buf);
    TextLine *line = &buf->lines[buf->cursor_row];
    if (buf->cursor_col < line->len) {
        line->data[buf->cursor_col] = '\0';
        line->len = buf->cursor_col;
    }
    return true;
}

static bool text_buffer_kv_left(TextBuffer *buf) {
    text_buffer_clamp_col(buf);
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
    text_buffer_clamp_col(buf);
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

static const struct {
    int code;
    TextBufferKeyFn fn;
} text_buffer_key_bindings[] = {
    {RETRO_KEY_LF,     text_buffer_insert_newline},
    {RETRO_KEY_CR,     text_buffer_insert_newline},
    {RETRO_KEY_BS,     text_buffer_delete_backward},
    {RETRO_KEY_DEL,    text_buffer_delete_backward},
    {RETRO_KEY_CTRL_A, text_buffer_kv_home},
    {RETRO_KEY_CTRL_E, text_buffer_kv_end},
    {RETRO_KEY_CTRL_K, text_buffer_kv_kill_to_end},
    {RETRO_KEY_CTRL_D, text_buffer_delete_forward},
    {RETRO_KEY_LEFT,   text_buffer_kv_left},
    {RETRO_KEY_RIGHT,  text_buffer_kv_right},
    {RETRO_KEY_UP,     text_buffer_kv_up},
    {RETRO_KEY_DOWN,   text_buffer_kv_down},
    {RETRO_KEY_HOME,   text_buffer_kv_home},
    {RETRO_KEY_END,    text_buffer_kv_end},
    {RETRO_KEY_DC,     text_buffer_delete_forward},
};

bool text_buffer_handle_key(TextBuffer *buf, const RetroKeyEvent *key) {
    if (!buf || !key) return false;

    size_t binding_count = sizeof(text_buffer_key_bindings) /
                           sizeof(text_buffer_key_bindings[0]);
    for (size_t i = 0; i < binding_count; ++i) {
        if (text_buffer_key_bindings[i].code == key->key_code) {
            return text_buffer_key_bindings[i].fn(buf);
        }
    }

    if (key->is_printable) {
        uint32_t codepoint = key->text_codepoint;
        if (codepoint == 0 && key->ascii > 0) {
            codepoint = key->ascii;
        }
        if (codepoint >= 0x20u && codepoint != 0x7Fu) {
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
    size_t pos = start;
    while (line && pos < line->len) {
        uint32_t codepoint = 0;
        size_t byte_len = 0;
        if (!retro_utf8_decode(line->data, line->len, pos,
                               &codepoint, &byte_len)) {
            codepoint = '?';
            byte_len = 1;
        }
        int width_value = retro_utf8_width(codepoint);
        size_t width = width_value > 0 ? (size_t)width_value : 0;
        if (width > 0 && used_cols + width > visible_cols) break;
        if (out_len + byte_len >= capacity) break;
        memcpy(out + out_len, line->data + pos, byte_len);
        out_len += byte_len;
        used_cols += width;
        pos += byte_len;
    }

    while (used_cols < visible_cols && out_len + 1 < capacity) {
        out[out_len++] = ' ';
        used_cols++;
    }
    out[out_len] = '\0';
    return out;
}

void text_buffer_render(const TextBuffer *buf, DrawList *draw_list,
                        int y, int x, int visible_rows, int visible_cols,
                        const RenderStyle *style,
                        const RenderStyle *cursor_style) {
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

    for (size_t row = 0; row < rows; ++row) {
        size_t buffer_row = scroll_row + row;
        const TextLine *line =
            buffer_row < buf->line_count ? &buf->lines[buffer_row] : NULL;
        size_t line_start = line
            ? text_line_boundary_at_or_before(line, scroll_col)
            : 0;
        char *visible = text_line_visible_text(line, line_start, cols);
        if (!visible) continue;
        draw_list_text(draw_list, y + (int)row, x, visible, style);
        free(visible);
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
