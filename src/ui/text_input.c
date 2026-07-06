#include "ui/text_input.h"

#include <stdlib.h>
#include <string.h>

#include "core/key_chord.h"

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
    size_t len = strlen(text);
    if (input->max_len > 0 && len > input->max_len) {
        len = input->max_len;
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

static bool text_input_insert_char(TextInput *input, char ch) {
    if (!input || ch <= 0) return false;
    if (input->max_len > 0 && input->len >= input->max_len) return false;
    if (!text_input_grow(input, input->len + 2)) return false;

    /* Shift right from cursor. */
    memmove(input->buffer + input->cursor + 1,
            input->buffer + input->cursor,
            input->len - input->cursor + 1); /* +1 for null terminator */
    input->buffer[input->cursor] = ch;
    input->len++;
    input->cursor++;
    return true;
}

static bool text_input_delete_backward(TextInput *input) {
    if (!input || input->cursor == 0) return false;
    memmove(input->buffer + input->cursor - 1,
            input->buffer + input->cursor,
            input->len - input->cursor + 1);
    input->cursor--;
    input->len--;
    return true;
}

static bool text_input_delete_forward(TextInput *input) {
    if (!input || input->cursor >= input->len) return false;
    memmove(input->buffer + input->cursor,
            input->buffer + input->cursor + 1,
            input->len - input->cursor); /* includes null terminator */
    input->len--;
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
    if (input->cursor > 0) input->cursor--;
    return true;
}

static bool text_input_handle_right(TextInput *input) {
    if (input->cursor < input->len) input->cursor++;
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

    /* Fallthrough: printable ASCII characters are inserted verbatim. */
    if (key->is_printable && key->ascii > 0) {
        return text_input_insert_char(input, key->ascii);
    }
    return false;
}

/* --- render ----------------------------------------------------------- */

void text_input_render(const TextInput *input, DrawList *draw_list,
                       int y, int x, int visible_width,
                       const RenderStyle *style,
                       const RenderStyle *cursor_style) {
    if (!input || !draw_list || visible_width <= 0) return;

    /* We need a mutable copy of scroll_offset for rendering. The const
       qualifier on input prevents updating it here, so we compute the
       effective offset locally. */
    size_t scroll = input->scroll_offset;
    size_t vw = (size_t)visible_width;
    if (input->cursor < scroll) {
        scroll = input->cursor;
    }
    if (input->cursor >= scroll + vw) {
        scroll = input->cursor - vw + 1;
    }

    /* Build visible text. */
    char line[256];
    size_t out_len = 0;
    for (size_t i = scroll; i < input->len && out_len < vw && out_len < sizeof(line) - 1; ++i) {
        line[out_len++] = input->buffer[i];
    }
    /* Pad with spaces to fill visible_width. */
    while (out_len < vw && out_len < sizeof(line) - 1) {
        line[out_len++] = ' ';
    }
    line[out_len] = '\0';

    /* Draw the full line with base style. */
    draw_list_text(draw_list, y, x, line, style);

    /* Draw cursor character with cursor_style (reverse). */
    if (cursor_style) {
        int cursor_x = x + (int)(input->cursor - scroll);
        char cursor_ch[2];
        if (input->cursor < input->len) {
            cursor_ch[0] = input->buffer[input->cursor];
        } else {
            cursor_ch[0] = ' ';
        }
        cursor_ch[1] = '\0';
        draw_list_text(draw_list, y, cursor_x, cursor_ch, cursor_style);
    }
}
