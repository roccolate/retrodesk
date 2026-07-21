#include "ui/text_input.h"

#include <stdlib.h>
#include <string.h>

#include "core/key_chord.h"
#include "core/utf8.h"

enum { TEXT_INPUT_INIT_CAP = 64 };

struct TextInput {
    char *buffer;
    size_t len;
    size_t capacity;
    size_t max_len;      /* 0 = unlimited */
    size_t cursor;       /* 0..len */
    size_t scroll_offset;
};

/* --- internal helpers ------------------------------------------------- */

static bool text_input_grow(TextInput *input, size_t need) {
    if (need <= input->capacity) return true;
    size_t new_cap = input->capacity ? input->capacity * 2 : TEXT_INPUT_INIT_CAP;
    while (new_cap < need) new_cap *= 2;
    char *next = realloc(input->buffer, new_cap);
    if (!next) return false;
    input->buffer = next;
    input->capacity = new_cap;
    return true;
}

/* --- lifecycle -------------------------------------------------------- */

TextInput *text_input_create(size_t max_len) {
    TextInput *input = calloc(1, sizeof(*input));
    if (!input) return NULL;
    input->max_len = max_len;
    if (!text_input_grow(input, TEXT_INPUT_INIT_CAP)) {
        free(input);
        return NULL;
    }
    input->buffer[0] = '\0';
    return input;
}

void text_input_destroy(TextInput *input) {
    if (!input) return;
    free(input->buffer);
    free(input);
}

/* --- state ------------------------------------------------------------ */

const char *text_input_text(const TextInput *input) {
    if (!input) return "";
    return input->buffer;
}

void text_input_set_text(TextInput *input, const char *text) {
    if (!input) return;
    if (!text) text = "";
    size_t source_len = strlen(text);
    size_t len = source_len;
    if (input->max_len > 0 && len > input->max_len) {
        len = retro_utf8_valid_prefix(text, input->max_len);
    }
    if (!text_input_grow(input, len + 1)) return;
    memcpy(input->buffer, text, len);
    input->buffer[len] = '\0';
    input->len = len;
    input->cursor = len;
    input->scroll_offset = 0;
}

void text_input_clear(TextInput *input) {
    if (!input) return;
    input->buffer[0] = '\0';
    input->len = 0;
    input->cursor = 0;
    input->scroll_offset = 0;
}

size_t text_input_cursor(const TextInput *input) {
    if (!input) return 0;
    return input->cursor;
}

size_t text_input_length(const TextInput *input) {
    if (!input) return 0;
    return input->len;
}

/* --- editing ---------------------------------------------------------- */

static bool text_input_insert_codepoint(TextInput *input, uint32_t codepoint) {
    if (!input || codepoint < 0x20u || codepoint == 0x7Fu) return false;
    char encoded[4] = {0};
    size_t encoded_len = 0;
    if (!retro_utf8_encode(codepoint, encoded, &encoded_len)) return false;
    if (input->max_len > 0 && input->len + encoded_len > input->max_len) return false;
    if (!text_input_grow(input, input->len + encoded_len + 1)) return false;

    memmove(input->buffer + input->cursor + encoded_len,
            input->buffer + input->cursor,
            input->len - input->cursor + 1);
    memcpy(input->buffer + input->cursor, encoded, encoded_len);
    input->len += encoded_len;
    input->cursor += encoded_len;
    return true;
}

static bool text_input_delete_backward(TextInput *input) {
    if (!input || input->cursor == 0) return false;
    size_t previous = retro_utf8_prev(input->buffer, input->cursor);
    memmove(input->buffer + previous,
            input->buffer + input->cursor,
            input->len - input->cursor + 1);
    input->len -= input->cursor - previous;
    input->cursor = previous;
    return true;
}

static bool text_input_delete_forward(TextInput *input) {
    if (!input || input->cursor >= input->len) return false;
    size_t next = retro_utf8_next(input->buffer, input->len, input->cursor);
    memmove(input->buffer + input->cursor,
            input->buffer + next,
            input->len - next + 1);
    input->len -= next - input->cursor;
    return true;
}

/* --- key handling ----------------------------------------------------- */

/* Handlers are kept short and self-contained so the dispatcher below
   remains a single linear table lookup. Each returns true if the
   event was consumed. */

static bool text_input_handle_home(TextInput *input) {
    if (input->cursor == 0) return true;
    input->cursor = 0;
    return true;
}

static bool text_input_handle_end(TextInput *input) {
    if (input->cursor == input->len) return true;
    input->cursor = input->len;
    return true;
}

static bool text_input_handle_left(TextInput *input) {
    if (input->cursor > 0) {
        input->cursor = retro_utf8_prev(input->buffer, input->cursor);
    }
    return true;
}

static bool text_input_handle_right(TextInput *input) {
    if (input->cursor < input->len) {
        input->cursor = retro_utf8_next(input->buffer, input->len, input->cursor);
    }
    return true;
}

static bool text_input_handle_kill_to_end(TextInput *input) {
    if (input->cursor < input->len) {
        input->buffer[input->cursor] = '\0';
        input->len = input->cursor;
    }
    return true;
}

static bool text_input_handle_clear(TextInput *input) {
    text_input_clear(input);
    return true;
}

typedef bool (*TextInputKeyFn)(TextInput *input);

static const struct {
    int code;
    TextInputKeyFn fn;
} text_input_key_bindings[] = {
    {RETRO_KEY_BS,       text_input_delete_backward},
    {RETRO_KEY_DEL,      text_input_delete_backward},
    {RETRO_KEY_CTRL_A,   text_input_handle_home},
    {RETRO_KEY_CTRL_E,   text_input_handle_end},
    {RETRO_KEY_CTRL_U,   text_input_handle_clear},
    {RETRO_KEY_CTRL_K,   text_input_handle_kill_to_end},
    {RETRO_KEY_CTRL_D,   text_input_delete_forward},
    {RETRO_KEY_LEFT,     text_input_handle_left},
    {RETRO_KEY_RIGHT,    text_input_handle_right},
    {RETRO_KEY_HOME,     text_input_handle_home},
    {RETRO_KEY_END,      text_input_handle_end},
    {RETRO_KEY_DC,       text_input_delete_forward},
};

bool text_input_handle_key(TextInput *input, const RetroKeyEvent *key) {
    if (!input || !key) return false;

    int code = key->key_code;
    size_t n = sizeof(text_input_key_bindings) / sizeof(text_input_key_bindings[0]);
    for (size_t i = 0; i < n; i++) {
        if (text_input_key_bindings[i].code == code) {
            return text_input_key_bindings[i].fn(input);
        }
    }

    uint32_t codepoint = key->text_codepoint;
    if (codepoint == 0 && key->is_printable && key->ascii > 0) {
        codepoint = key->ascii;
    }
    if (key->is_printable && codepoint >= 0x20u && codepoint != 0x7Fu) {
        return text_input_insert_codepoint(input, codepoint);
    }
    return false;
}

/* --- render ----------------------------------------------------------- */

void text_input_render(const TextInput *input, DrawList *draw_list,
                       int y, int x, int visible_width,
                       const RenderStyle *style,
                       const RenderStyle *cursor_style) {
    if (!input || !draw_list || visible_width <= 0) return;

    size_t start = 0;
    size_t width = (size_t)visible_width;
    while (start < input->cursor &&
           retro_utf8_columns(input->buffer, input->len, start, input->cursor) >= width) {
        start = retro_utf8_next(input->buffer, input->len, start);
    }

    char line[256];
    size_t out_len = 0;
    size_t cells = 0;
    size_t offset = start;
    while (offset < input->len && cells < width) {
        uint32_t cp = 0;
        size_t bytes = 0;
        if (!retro_utf8_decode(input->buffer, input->len, offset, &cp, &bytes)) {
            cp = 0xFFFDu;
            bytes = 1;
        }
        int cell_width = retro_utf8_width(cp);
        if (cell_width > 0 && cells + (size_t)cell_width > width) break;
        if (out_len + bytes >= sizeof(line)) break;
        memcpy(line + out_len, input->buffer + offset, bytes);
        out_len += bytes;
        if (cell_width > 0) cells += (size_t)cell_width;
        offset += bytes;
    }
    while (cells < width && out_len + 1 < sizeof(line)) {
        line[out_len++] = ' ';
        cells++;
    }
    line[out_len] = '\0';
    draw_list_text(draw_list, y, x, line, style);

    if (cursor_style) {
        size_t cursor_cells =
            retro_utf8_columns(input->buffer, input->len, start, input->cursor);
        int cursor_x = x + (int)cursor_cells;
        char cursor_text[5] = {' ', '\0', '\0', '\0', '\0'};
        if (input->cursor < input->len) {
            uint32_t cp = 0;
            size_t bytes = 0;
            if (retro_utf8_decode(input->buffer, input->len, input->cursor,
                                  &cp, &bytes) &&
                bytes < sizeof(cursor_text)) {
                memcpy(cursor_text, input->buffer + input->cursor, bytes);
                cursor_text[bytes] = '\0';
            }
        }
        draw_list_text(draw_list, y, cursor_x, cursor_text, cursor_style);
    }
}
