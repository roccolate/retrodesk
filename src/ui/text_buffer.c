#include "ui/text_buffer.h"

#include <stdlib.h>
#include <string.h>

#include "core/key_chord.h"

enum {
    TEXT_BUFFER_INIT_LINES = 16,
    TEXT_BUFFER_LINE_INIT_CAP = 64,
};

/* A single line in the buffer. */
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

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

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
    while (new_cap < need) new_cap *= 2;
    char *next = realloc(line->data, new_cap);
    if (!next) return false;
    line->data = next;
    line->cap = new_cap;
    return true;
}

static bool text_line_set(TextLine *line, const char *str, size_t len) {
    if (!line) return false;
    if (!text_line_grow(line, len + 1)) return false;
    if (len > 0 && str) {
        memcpy(line->data, str, len);
    }
    line->data[len] = '\0';
    line->len = len;
    return true;
}

static bool text_buffer_grow_lines(TextBuffer *buf, size_t need) {
    if (!buf) return false;
    if (need <= buf->line_cap) return true;
    size_t new_cap = buf->line_cap ? buf->line_cap * 2 : TEXT_BUFFER_INIT_LINES;
    while (new_cap < need) new_cap *= 2;
    TextLine *next = realloc(buf->lines, new_cap * sizeof(TextLine));
    if (!next) return false;
    buf->lines = next;
    buf->line_cap = new_cap;
    return true;
}

/* Clamp cursor_col to the current line length. */
static void text_buffer_clamp_col(TextBuffer *buf) {
    if (!buf || buf->line_count == 0) return;
    size_t line_len = buf->lines[buf->cursor_row].len;
    if (buf->cursor_col > line_len) {
        buf->cursor_col = line_len;
    }
}


/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

TextBuffer *text_buffer_create(void) {
    TextBuffer *buf = calloc(1, sizeof(*buf));
    if (!buf) return NULL;
    if (!text_buffer_grow_lines(buf, TEXT_BUFFER_INIT_LINES)) {
        free(buf);
        return NULL;
    }
    /* Always start with one empty line. */
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
    buf->lines = NULL;
    free(buf);
}

/* ------------------------------------------------------------------ */
/* Content                                                             */
/* ------------------------------------------------------------------ */

void text_buffer_clear(TextBuffer *buf) {
    if (!buf) return;
    for (size_t i = 0; i < buf->line_count; ++i) {
        text_line_free(&buf->lines[i]);
    }
    /* Re-init with one empty line. */
    text_line_init(&buf->lines[0]);
    buf->line_count = 1;
    buf->cursor_row = 0;
    buf->cursor_col = 0;
    buf->scroll_row = 0;
    buf->scroll_col = 0;
}

bool text_buffer_set_text(TextBuffer *buf, const char *text) {
    if (!buf) return false;
    if (!text) text = "";

    /* Free existing lines. */
    for (size_t i = 0; i < buf->line_count; ++i) {
        text_line_free(&buf->lines[i]);
    }
    buf->line_count = 0;

    /* Split text by newlines. */
    const char *p = text;
    for (;;) {
        const char *nl = strchr(p, '\n');
        size_t seg_len = nl ? (size_t)(nl - p) : strlen(p);

        if (!text_buffer_grow_lines(buf, buf->line_count + 1)) {
            /* Ensure at least one line on failure. */
            if (buf->line_count == 0) {
                text_line_init(&buf->lines[0]);
                buf->line_count = 1;
            }
            buf->cursor_row = 0;
            buf->cursor_col = 0;
            return false;
        }
        if (!text_line_init(&buf->lines[buf->line_count])) {
            if (buf->line_count == 0) {
                text_line_init(&buf->lines[0]);
                buf->line_count = 1;
            }
            buf->cursor_row = 0;
            buf->cursor_col = 0;
            return false;
        }
        text_line_set(&buf->lines[buf->line_count], p, seg_len);
        buf->line_count++;

        if (!nl) break;
        p = nl + 1;
    }

    /* Place cursor at end. */
    buf->cursor_row = buf->line_count - 1;
    buf->cursor_col = buf->lines[buf->cursor_row].len;
    buf->scroll_row = 0;
    buf->scroll_col = 0;
    return true;
}

size_t text_buffer_line_count(const TextBuffer *buf) {
    if (!buf) return 0;
    return buf->line_count;
}

const char *text_buffer_line(const TextBuffer *buf, size_t row) {
    if (!buf || row >= buf->line_count) return "";
    return buf->lines[row].data;
}

size_t text_buffer_line_length(const TextBuffer *buf, size_t row) {
    if (!buf || row >= buf->line_count) return 0;
    return buf->lines[row].len;
}

/* ------------------------------------------------------------------ */
/* Cursor                                                              */
/* ------------------------------------------------------------------ */

size_t text_buffer_cursor_row(const TextBuffer *buf) {
    if (!buf) return 0;
    return buf->cursor_row;
}

size_t text_buffer_cursor_col(const TextBuffer *buf) {
    if (!buf) return 0;
    return buf->cursor_col;
}

void text_buffer_set_cursor(TextBuffer *buf, size_t row, size_t col) {
    if (!buf || buf->line_count == 0) return;
    if (row >= buf->line_count) row = buf->line_count - 1;
    buf->cursor_row = row;
    buf->cursor_col = col;
    text_buffer_clamp_col(buf);
}

size_t text_buffer_scroll_row(const TextBuffer *buf) {
    if (!buf) return 0;
    return buf->scroll_row;
}

size_t text_buffer_scroll_col(const TextBuffer *buf) {
    if (!buf) return 0;
    return buf->scroll_col;
}

/* ------------------------------------------------------------------ */
/* Editing                                                             */
/* ------------------------------------------------------------------ */

bool text_buffer_insert_char(TextBuffer *buf, char ch) {
    if (!buf || buf->line_count == 0 || ch == '\0') return false;

    TextLine *line = &buf->lines[buf->cursor_row];
    if (!text_line_grow(line, line->len + 2)) return false;

    /* Shift right from cursor col. */
    if (buf->cursor_col <= line->len) {
        memmove(line->data + buf->cursor_col + 1,
                line->data + buf->cursor_col,
                line->len - buf->cursor_col + 1);
    }
    line->data[buf->cursor_col] = ch;
    line->len++;
    buf->cursor_col++;
    return true;
}

bool text_buffer_insert_newline(TextBuffer *buf) {
    if (!buf || buf->line_count == 0) return false;

    TextLine *cur = &buf->lines[buf->cursor_row];
    size_t col = buf->cursor_col;
    if (col > cur->len) col = cur->len;

    /* Text after cursor moves to new line. */
    size_t tail_len = cur->len - col;

    /* Make room for the new line. */
    if (!text_buffer_grow_lines(buf, buf->line_count + 1)) return false;

    /* Shift lines down. */
    size_t insert_at = buf->cursor_row + 1;
    if (insert_at < buf->line_count) {
        memmove(&buf->lines[insert_at + 1],
                &buf->lines[insert_at],
                (buf->line_count - insert_at) * sizeof(TextLine));
    }

    /* Init new line with tail content. */
    if (!text_line_init(&buf->lines[insert_at])) {
        /* Roll back shift. */
        if (insert_at < buf->line_count) {
            memmove(&buf->lines[insert_at],
                    &buf->lines[insert_at + 1],
                    (buf->line_count - insert_at) * sizeof(TextLine));
        }
        return false;
    }
    text_line_set(&buf->lines[insert_at], cur->data + col, tail_len);
    buf->line_count++;

    /* Truncate current line at cursor. */
    /* Re-fetch pointer since grow may have reallocated. */
    cur = &buf->lines[buf->cursor_row];
    cur->data[col] = '\0';
    cur->len = col;

    /* Move cursor to beginning of new line. */
    buf->cursor_row = insert_at;
    buf->cursor_col = 0;
    return true;
}

bool text_buffer_delete_backward(TextBuffer *buf) {
    if (!buf || buf->line_count == 0) return false;

    if (buf->cursor_col > 0) {
        /* Delete character within the line. */
        TextLine *line = &buf->lines[buf->cursor_row];
        size_t col = buf->cursor_col;
        if (col > line->len) col = line->len;
        memmove(line->data + col - 1,
                line->data + col,
                line->len - col + 1);
        line->len--;
        buf->cursor_col = col - 1;
        return true;
    }

    /* At column 0: merge with previous line. */
    if (buf->cursor_row == 0) return false;

    size_t prev_row = buf->cursor_row - 1;
    TextLine *prev = &buf->lines[prev_row];
    TextLine *cur = &buf->lines[buf->cursor_row];
    size_t old_prev_len = prev->len;

    /* Append current line to previous. */
    if (cur->len > 0) {
        if (!text_line_grow(prev, prev->len + cur->len + 1)) return false;
        memcpy(prev->data + prev->len, cur->data, cur->len);
        prev->len += cur->len;
        prev->data[prev->len] = '\0';
    }

    /* Free current line and shift remaining lines up. */
    text_line_free(cur);
    size_t remove_at = buf->cursor_row;
    if (remove_at + 1 < buf->line_count) {
        memmove(&buf->lines[remove_at],
                &buf->lines[remove_at + 1],
                (buf->line_count - remove_at - 1) * sizeof(TextLine));
    }
    buf->line_count--;

    buf->cursor_row = prev_row;
    buf->cursor_col = old_prev_len;
    return true;
}

bool text_buffer_delete_forward(TextBuffer *buf) {
    if (!buf || buf->line_count == 0) return false;

    TextLine *line = &buf->lines[buf->cursor_row];

    if (buf->cursor_col < line->len) {
        /* Delete character at cursor. */
        memmove(line->data + buf->cursor_col,
                line->data + buf->cursor_col + 1,
                line->len - buf->cursor_col);
        line->len--;
        return true;
    }

    /* At end of line: merge with next line. */
    if (buf->cursor_row + 1 >= buf->line_count) return false;

    size_t next_row = buf->cursor_row + 1;
    TextLine *next = &buf->lines[next_row];

    /* Append next line to current. */
    if (next->len > 0) {
        if (!text_line_grow(line, line->len + next->len + 1)) return false;
        memcpy(line->data + line->len, next->data, next->len);
        line->len += next->len;
        line->data[line->len] = '\0';
    }

    /* Free next line and shift. */
    text_line_free(next);
    if (next_row + 1 < buf->line_count) {
        memmove(&buf->lines[next_row],
                &buf->lines[next_row + 1],
                (buf->line_count - next_row - 1) * sizeof(TextLine));
    }
    buf->line_count--;
    return true;
}

/* ------------------------------------------------------------------ */
/* Key handling                                                        */
/* ------------------------------------------------------------------ */

bool text_buffer_handle_key(TextBuffer *buf, const RetroKeyEvent *key) {
    if (!buf || !key) return false;

    int code = key->key_code;

    /* Enter / newline */
    if (code == RETRO_KEY_LF || code == RETRO_KEY_CR) {
        return text_buffer_insert_newline(buf);
    }

    /* Backspace */
    if (code == RETRO_KEY_BS || code == RETRO_KEY_DEL) {
        return text_buffer_delete_backward(buf);
    }

    /* Ctrl+A — home (beginning of line) */
    if (code == RETRO_KEY_CTRL_A) {
        if (buf->cursor_col == 0) return true;
        buf->cursor_col = 0;
        return true;
    }

    /* Ctrl+E — end of line */
    if (code == RETRO_KEY_CTRL_E) {
        size_t line_len = buf->lines[buf->cursor_row].len;
        if (buf->cursor_col == line_len) return true;
        buf->cursor_col = line_len;
        return true;
    }

    /* Ctrl+K — kill to end of line */
    if (code == RETRO_KEY_CTRL_K) {
        TextLine *line = &buf->lines[buf->cursor_row];
        if (buf->cursor_col < line->len) {
            line->data[buf->cursor_col] = '\0';
            line->len = buf->cursor_col;
        }
        return true;
    }

    /* Ctrl+D — delete forward */
    if (code == RETRO_KEY_CTRL_D) {
        return text_buffer_delete_forward(buf);
    }

    /* Arrow keys (portable chords translated by the platform layer). */
    if (code == RETRO_KEY_LEFT) {
        if (buf->cursor_col > 0) {
            buf->cursor_col--;
        } else if (buf->cursor_row > 0) {
            buf->cursor_row--;
            buf->cursor_col = buf->lines[buf->cursor_row].len;
        }
        return true;
    }
    if (code == RETRO_KEY_RIGHT) {
        if (buf->cursor_col < buf->lines[buf->cursor_row].len) {
            buf->cursor_col++;
        } else if (buf->cursor_row + 1 < buf->line_count) {
            buf->cursor_row++;
            buf->cursor_col = 0;
        }
        return true;
    }
    if (code == RETRO_KEY_UP) {
        if (buf->cursor_row > 0) {
            buf->cursor_row--;
            text_buffer_clamp_col(buf);
        }
        return true;
    }
    if (code == RETRO_KEY_DOWN) {
        if (buf->cursor_row + 1 < buf->line_count) {
            buf->cursor_row++;
            text_buffer_clamp_col(buf);
        }
        return true;
    }
    if (code == RETRO_KEY_HOME) {
        buf->cursor_col = 0;
        return true;
    }
    if (code == RETRO_KEY_END) {
        buf->cursor_col = buf->lines[buf->cursor_row].len;
        return true;
    }
    if (code == RETRO_KEY_DC) {
        return text_buffer_delete_forward(buf);
    }

    /* Printable character */
    if (key->is_printable && key->ascii > 0) {
        return text_buffer_insert_char(buf, key->ascii);
    }

    return false;
}

/* ------------------------------------------------------------------ */
/* Render                                                              */
/* ------------------------------------------------------------------ */

void text_buffer_render(const TextBuffer *buf, DrawList *draw_list,
                        int y, int x, int visible_rows, int visible_cols,
                        const RenderStyle *style,
                        const RenderStyle *cursor_style) {
    if (!buf || !draw_list || visible_rows <= 0 || visible_cols <= 0) return;

    /* Compute effective scroll (const-safe: don't mutate buf). */
    size_t s_row = buf->scroll_row;
    size_t s_col = buf->scroll_col;
    size_t vr = (size_t)visible_rows;
    size_t vc = (size_t)visible_cols;

    if (buf->cursor_row < s_row) s_row = buf->cursor_row;
    if (buf->cursor_row >= s_row + vr) s_row = buf->cursor_row - vr + 1;
    if (buf->cursor_col < s_col) s_col = buf->cursor_col;
    if (buf->cursor_col >= s_col + vc) s_col = buf->cursor_col - vc + 1;

    /* Render visible lines. */
    char line_buf[256];
    for (size_t r = 0; r < vr; ++r) {
        size_t buf_row = s_row + r;
        size_t out_len = 0;

        if (buf_row < buf->line_count) {
            const TextLine *tl = &buf->lines[buf_row];
            for (size_t c = s_col; c < tl->len && out_len < vc &&
                 out_len < sizeof(line_buf) - 1; ++c) {
                line_buf[out_len++] = tl->data[c];
            }
        }
        /* Pad with spaces. */
        while (out_len < vc && out_len < sizeof(line_buf) - 1) {
            line_buf[out_len++] = ' ';
        }
        line_buf[out_len] = '\0';

        draw_list_text(draw_list, y + (int)r, x, line_buf, style);
    }

    /* Draw cursor character with cursor_style. */
    if (cursor_style &&
        buf->cursor_row >= s_row && buf->cursor_row < s_row + vr &&
        buf->cursor_col >= s_col && buf->cursor_col < s_col + vc) {
        int cy = y + (int)(buf->cursor_row - s_row);
        int cx = x + (int)(buf->cursor_col - s_col);
        char cursor_ch[2];
        if (buf->cursor_col < buf->lines[buf->cursor_row].len) {
            cursor_ch[0] = buf->lines[buf->cursor_row].data[buf->cursor_col];
        } else {
            cursor_ch[0] = ' ';
        }
        cursor_ch[1] = '\0';
        draw_list_text(draw_list, cy, cx, cursor_ch, cursor_style);
    }
}
