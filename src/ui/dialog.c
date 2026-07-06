#include "ui/dialog.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ui/button.h"
#include "ui/text_input.h"

enum {
    DIALOG_INIT_BUF = 128,
    DIALOG_INPUT_WIDTH = 32,
    DIALOG_MIN_WIDTH = 20,
    DIALOG_MIN_HEIGHT = 7,
    DIALOG_BUTTON_GAP = 2,
    DIALOG_FOCUS_INPUT = 0,
    DIALOG_FOCUS_OK = 1,
    DIALOG_FOCUS_CANCEL = 2,
};

typedef struct DialogLine {
    char *text;
} DialogLine;

struct Dialog {
    DialogType type;
    DialogResult result;
    char *title;
    char *message;
    DialogLine *lines;
    size_t line_count;
    size_t line_capacity;
    int input_focus;       /* DIALOG_FOCUS_* */
    Button *ok_button;
    Button *cancel_button;
    TextInput *input;
    size_t max_input_len;
};

/* --- internal helpers ------------------------------------------------- */

static char *dialog_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = malloc(len + 1);
    if (!dup) return NULL;
    memcpy(dup, s, len + 1);
    return dup;
}

static bool dialog_lines_grow(Dialog *dlg, size_t need) {
    if (need <= dlg->line_capacity) return true;
    size_t new_cap = dlg->line_capacity ? dlg->line_capacity * 2 : 8;
    while (new_cap < need) new_cap *= 2;
    DialogLine *next = realloc(dlg->lines, new_cap * sizeof(DialogLine));
    if (!next) return false;
    dlg->lines = next;
    dlg->line_capacity = new_cap;
    return true;
}

static void dialog_clear_lines(Dialog *dlg) {
    if (!dlg || !dlg->lines) return;
    for (size_t i = 0; i < dlg->line_count; i++) {
        free(dlg->lines[i].text);
        dlg->lines[i].text = NULL;
    }
    dlg->line_count = 0;
}

/* Split message on '\n' and word-wrap each paragraph to max_width. */
static bool dialog_split_message(Dialog *dlg, int max_width) {
    if (!dlg || !dlg->message || max_width <= 0) return false;
    dialog_clear_lines(dlg);

    const char *p = dlg->message;
    while (*p) {
        /* Find end of this physical line (until \n or end). */
        const char *line_end = p;
        while (*line_end && *line_end != '\n') line_end++;
        size_t line_len = (size_t)(line_end - p);

        /* Wrap this paragraph into max_width chunks. */
        if (line_len == 0) {
            /* Empty paragraph — produce one empty visual line. */
            if (!dialog_lines_grow(dlg, dlg->line_count + 1)) return false;
            dlg->lines[dlg->line_count].text = dialog_strdup("");
            if (!dlg->lines[dlg->line_count].text) return false;
            dlg->line_count++;
        } else {
            size_t offset = 0;
            while (offset < line_len) {
                size_t chunk = line_len - offset;
                if ((int)chunk > max_width) {
                    /* Try to break at last space within max_width. */
                    size_t break_at = (size_t)max_width;
                    for (size_t i = break_at; i > 0; i--) {
                        if (p[offset + i - 1] == ' ') {
                            break_at = i;
                            break;
                        }
                    }
                    if (break_at == 0 || break_at == (size_t)max_width) {
                        /* No space found, hard break at max_width. */
                        break_at = (size_t)max_width;
                    }
                    chunk = break_at;
                }
                char *piece = malloc(chunk + 1);
                if (!piece) return false;
                memcpy(piece, p + offset, chunk);
                piece[chunk] = '\0';
                /* Strip trailing space from wrapped lines. */
                while (chunk > 0 && piece[chunk - 1] == ' ') {
                    piece[--chunk] = '\0';
                }
                if (!dialog_lines_grow(dlg, dlg->line_count + 1)) {
                    free(piece);
                    return false;
                }
                dlg->lines[dlg->line_count].text = piece;
                dlg->line_count++;
                offset += chunk;
                /* Skip the separator space when breaking on a space. */
                if (offset < line_len && p[offset] == ' ') offset++;
            }
        }

        p = line_end;
        if (*p == '\n') p++;
    }

    /* Always at least one line. */
    if (dlg->line_count == 0) {
        if (!dialog_lines_grow(dlg, 1)) return false;
        dlg->lines[0].text = dialog_strdup("");
        if (!dlg->lines[0].text) return false;
        dlg->line_count = 1;
    }
    return true;
}

static int dialog_input_row(int msg_height) {
    /* After frame (1) + gap (1) + message + gap (1). */
    return 1 + 1 + msg_height + 1;
}

static int dialog_button_row(int msg_height, bool has_input) {
    if (has_input) {
        return dialog_input_row(msg_height) + 1 + 1;
    }
    return 1 + 1 + msg_height + 1;
}

static int dialog_total_height(const Dialog *dlg) {
    if (!dlg) return 0;
    int msg_h = (int)dlg->line_count;
    bool has_input = (dlg->type == DIALOG_INPUT);
    int rows = dialog_button_row(msg_h, has_input) + 1;
    if (rows < DIALOG_MIN_HEIGHT) rows = DIALOG_MIN_HEIGHT;
    return rows;
}

static void dialog_apply_focus(Dialog *dlg) {
    if (!dlg) return;
    bool has_input = (dlg->type == DIALOG_INPUT);

    if (dlg->input) {
        bool input_focused = has_input && (dlg->input_focus == DIALOG_FOCUS_INPUT);
        /* TextInput does not have a public focus API; presence of input
           means the parent forwards key events to it. */
        (void)input_focused;
    }
    if (dlg->ok_button) {
        bool ok_focused = !has_input || (dlg->input_focus == DIALOG_FOCUS_OK);
        button_set_focused(dlg->ok_button, ok_focused);
    }
    if (dlg->cancel_button) {
        bool cancel_focused = has_input
            ? (dlg->input_focus == DIALOG_FOCUS_CANCEL)
            : false;
        button_set_focused(dlg->cancel_button, cancel_focused);
    }
}

static void dialog_focus_cycle(Dialog *dlg, int delta) {
    if (!dlg) return;
    bool has_input = (dlg->type == DIALOG_INPUT);
    bool has_cancel = (dlg->type != DIALOG_MESSAGE);
    int count = 1; /* OK */
    if (has_input) count++;
    if (has_cancel) count++;

    int idx = dlg->input_focus;
    for (int i = 0; i < count; i++) {
        idx = (idx + delta + count) % count;
        /* For INPUT dialogs, never skip the input on cycle — it is always
           selectable. For non-input dialogs, input slot is unused. */
        if (!has_input && idx == DIALOG_FOCUS_INPUT) continue;
        break;
    }
    dlg->input_focus = idx;
    dialog_apply_focus(dlg);
}

static void dialog_activate_ok(Dialog *dlg) {
    if (!dlg) return;
    dlg->result = DIALOG_RESULT_OK;
}

static void dialog_activate_cancel(Dialog *dlg) {
    if (!dlg) return;
    dlg->result = DIALOG_RESULT_CANCEL;
}

static bool dialog_init_common(Dialog *dlg, DialogType type,
                               const char *title, const char *message) {
    dlg->type = type;
    dlg->result = DIALOG_RESULT_NONE;
    dlg->title = dialog_strdup(title ? title : "");
    dlg->message = dialog_strdup(message ? message : "");
    dlg->lines = NULL;
    dlg->line_count = 0;
    dlg->line_capacity = 0;
    dlg->input_focus = (type == DIALOG_INPUT) ? DIALOG_FOCUS_INPUT
                                              : DIALOG_FOCUS_OK;
    dlg->ok_button = NULL;
    dlg->cancel_button = NULL;
    dlg->input = NULL;
    dlg->max_input_len = 0;
    if (!dlg->title || !dlg->message) return false;
    return true;
}

/* --- lifecycle -------------------------------------------------------- */

Dialog *dialog_create_message(const char *title, const char *message) {
    Dialog *dlg = calloc(1, sizeof(*dlg));
    if (!dlg) return NULL;
    if (!dialog_init_common(dlg, DIALOG_MESSAGE, title, message)) {
        free(dlg->title);
        free(dlg->message);
        free(dlg);
        return NULL;
    }
    dlg->ok_button = button_create("OK");
    if (!dlg->ok_button) {
        free(dlg->title);
        free(dlg->message);
        free(dlg);
        return NULL;
    }
    if (!dialog_split_message(dlg, 60)) {
        button_destroy(dlg->ok_button);
        free(dlg->title);
        free(dlg->message);
        free(dlg->lines);
        free(dlg);
        return NULL;
    }
    dialog_apply_focus(dlg);
    return dlg;
}

Dialog *dialog_create_confirm(const char *title, const char *message) {
    Dialog *dlg = calloc(1, sizeof(*dlg));
    if (!dlg) return NULL;
    if (!dialog_init_common(dlg, DIALOG_CONFIRM, title, message)) {
        free(dlg->title);
        free(dlg->message);
        free(dlg);
        return NULL;
    }
    dlg->ok_button = button_create("OK");
    dlg->cancel_button = button_create("Cancel");
    if (!dlg->ok_button || !dlg->cancel_button) {
        button_destroy(dlg->ok_button);
        button_destroy(dlg->cancel_button);
        free(dlg->title);
        free(dlg->message);
        free(dlg);
        return NULL;
    }
    if (!dialog_split_message(dlg, 60)) {
        button_destroy(dlg->ok_button);
        button_destroy(dlg->cancel_button);
        free(dlg->title);
        free(dlg->message);
        free(dlg->lines);
        free(dlg);
        return NULL;
    }
    dialog_apply_focus(dlg);
    return dlg;
}

Dialog *dialog_create_input(const char *title, const char *message,
                            size_t max_input_len) {
    Dialog *dlg = calloc(1, sizeof(*dlg));
    if (!dlg) return NULL;
    if (!dialog_init_common(dlg, DIALOG_INPUT, title, message)) {
        free(dlg->title);
        free(dlg->message);
        free(dlg);
        return NULL;
    }
    dlg->max_input_len = max_input_len;
    dlg->ok_button = button_create("OK");
    dlg->cancel_button = button_create("Cancel");
    dlg->input = text_input_create(max_input_len);
    if (!dlg->ok_button || !dlg->cancel_button || !dlg->input) {
        button_destroy(dlg->ok_button);
        button_destroy(dlg->cancel_button);
        text_input_destroy(dlg->input);
        free(dlg->title);
        free(dlg->message);
        free(dlg);
        return NULL;
    }
    if (!dialog_split_message(dlg, 60)) {
        button_destroy(dlg->ok_button);
        button_destroy(dlg->cancel_button);
        text_input_destroy(dlg->input);
        free(dlg->title);
        free(dlg->message);
        free(dlg->lines);
        free(dlg);
        return NULL;
    }
    dialog_apply_focus(dlg);
    return dlg;
}

void dialog_destroy(Dialog *dlg) {
    if (!dlg) return;
    button_destroy(dlg->ok_button);
    button_destroy(dlg->cancel_button);
    text_input_destroy(dlg->input);
    dialog_clear_lines(dlg);
    free(dlg->lines);
    free(dlg->title);
    free(dlg->message);
    free(dlg);
}

/* --- accessors -------------------------------------------------------- */

DialogType dialog_type(const Dialog *dlg) {
    if (!dlg) return DIALOG_MESSAGE;
    return dlg->type;
}

DialogResult dialog_result(const Dialog *dlg) {
    if (!dlg) return DIALOG_RESULT_NONE;
    return dlg->result;
}

const char *dialog_input_text(const Dialog *dlg) {
    if (!dlg || !dlg->input) return "";
    return text_input_text(dlg->input);
}

bool dialog_set_input_text(Dialog *dlg, const char *text) {
    if (!dlg || !dlg->input) return false;
    text_input_set_text(dlg->input, text);
    return true;
}

/* --- event handling --------------------------------------------------- */

bool dialog_handle_key(Dialog *dlg, const RetroKeyEvent *key) {
    if (!dlg || !key) return false;
    if (dlg->result != DIALOG_RESULT_NONE) return false;

    int code = key->key_code;
    bool has_input = (dlg->type == DIALOG_INPUT);
    bool has_cancel = (dlg->type != DIALOG_MESSAGE);

    /* Esc cancels (or OK for MESSAGE). */
    if (code == 27 /* Escape */) {
        if (dlg->type == DIALOG_MESSAGE) {
            dialog_activate_ok(dlg);
        } else {
            dialog_activate_cancel(dlg);
        }
        return true;
    }

    /* Tab cycles focus. */
    if (code == '\t') {
        int delta = 1;
        dialog_focus_cycle(dlg, delta);
        return true;
    }
#ifdef KEY_BTAB
    if (code == KEY_BTAB) {
        dialog_focus_cycle(dlg, -1);
        return true;
    }
#endif

    /* Enter activates the currently focused button (or, when input is
       focused, just lets the input consume it via fallback). */
    if (code == '\n' || code == '\r' || code == 13 || code == 10) {
        if (has_input && dlg->input_focus == DIALOG_FOCUS_INPUT) {
            /* Let input handle it indirectly: consume the event so the
               parent does not also process Enter. The input field will
               keep focus; user can Tab to OK to confirm. */
            return false;
        }
        if (dlg->input_focus == DIALOG_FOCUS_CANCEL && has_cancel) {
            dialog_activate_cancel(dlg);
        } else {
            dialog_activate_ok(dlg);
        }
        return true;
    }

    /* Forward printable input and editing keys to the text input. */
    if (has_input && dlg->input_focus == DIALOG_FOCUS_INPUT) {
        return text_input_handle_key(dlg->input, key);
    }

    /* Otherwise, consume nothing — let the parent route arrow keys / etc. */
    return false;
}

bool dialog_handle_mouse(Dialog *dlg, const RetroMouseEvent *mouse,
                         int dialog_y, int dialog_x,
                         int dialog_w, int dialog_h) {
    if (!dlg || !mouse) return false;
    if (dlg->result != DIALOG_RESULT_NONE) return false;
    if (dialog_w <= 0 || dialog_h <= 0) return false;
    if (mouse->y < dialog_y || mouse->y >= dialog_y + dialog_h) return false;
    if (mouse->x < dialog_x || mouse->x >= dialog_x + dialog_w) return false;

    /* Click on OK button. */
    int btn_row = dialog_button_row((int)dlg->line_count, dlg->type == DIALOG_INPUT);
    int inner_y = dialog_y + btn_row;
    int inner_x = dialog_x + 2; /* left padding inside frame */

    int ok_w = button_width(dlg->ok_button);
    if (dlg->ok_button &&
        button_handle_mouse(dlg->ok_button, mouse, inner_y, inner_x, ok_w)) {
        dialog_activate_ok(dlg);
        return true;
    }
    if (dlg->cancel_button) {
        int cancel_x = inner_x + ok_w + DIALOG_BUTTON_GAP;
        int cancel_w = button_width(dlg->cancel_button);
        if (button_handle_mouse(dlg->cancel_button, mouse, inner_y, cancel_x,
                                cancel_w)) {
            dialog_activate_cancel(dlg);
            return true;
        }
    }

    /* Click on input field — focus it. */
    if (dlg->input && dlg->type == DIALOG_INPUT) {
        int input_row = dialog_input_row((int)dlg->line_count);
        if (mouse->y == dialog_y + input_row) {
            dlg->input_focus = DIALOG_FOCUS_INPUT;
            dialog_apply_focus(dlg);
            return true;
        }
    }
    return false;
}

/* --- sizing helpers --------------------------------------------------- */

int dialog_suggest_width(const Dialog *dlg, int max_w) {
    if (!dlg) return DIALOG_MIN_WIDTH;
    int widest = (int)strlen(dlg->title ? dlg->title : "");
    for (size_t i = 0; i < dlg->line_count; i++) {
        int len = (int)strlen(dlg->lines[i].text);
        if (len > widest) widest = len;
    }
    int buttons_total = button_width(dlg->ok_button);
    if (dlg->cancel_button) {
        buttons_total += DIALOG_BUTTON_GAP + button_width(dlg->cancel_button);
    }
    if (buttons_total > widest) widest = buttons_total;
    if (dlg->input) {
        int input_w = DIALOG_INPUT_WIDTH;
        if (input_w > widest) widest = input_w;
    }
    /* Add frame + padding (2 columns on each side). */
    int w = widest + 4;
    if (w < DIALOG_MIN_WIDTH) w = DIALOG_MIN_WIDTH;
    if (max_w > 0 && w > max_w) w = max_w;
    return w;
}

int dialog_suggest_height(const Dialog *dlg) {
    if (!dlg) return DIALOG_MIN_HEIGHT;
    int h = dialog_total_height(dlg);
    if (h < DIALOG_MIN_HEIGHT) h = DIALOG_MIN_HEIGHT;
    return h;
}

/* --- render ----------------------------------------------------------- */

void dialog_render(const Dialog *dlg, DrawList *draw_list,
                   int y, int x, int w, int h,
                   const DialogStyles *styles) {
    if (!dlg || !draw_list || !styles) return;
    if (w <= 0 || h <= 0) return;

    /* Frame box (border). */
    draw_list_box(draw_list, dlg->title, &styles->frame, &styles->title);

    /* Message lines. */
    int msg_start_row = 2; /* inside frame, after border + 1 blank line */
    int inner_x = x + 2;
    int inner_right = x + w - 2;
    int visible_lines = (h - 4);
    if (visible_lines < 0) visible_lines = 0;
    for (int i = 0; i < (int)dlg->line_count && i < visible_lines; i++) {
        draw_list_text(draw_list, y + msg_start_row + i, inner_x,
                       dlg->lines[i].text, &styles->message);
    }

    /* Optional input field. */
    if (dlg->input && dlg->type == DIALOG_INPUT) {
        int input_row = dialog_input_row((int)dlg->line_count);
        if (input_row + 1 < h) {
            int input_w = inner_right - inner_x;
            if (input_w < 4) input_w = 4;
            bool input_focused = (dlg->input_focus == DIALOG_FOCUS_INPUT);
            text_input_render(dlg->input, draw_list, y + input_row, inner_x,
                              input_w, &styles->input,
                              input_focused ? &styles->input_cursor : NULL);
        }
    }

    /* Buttons (centered horizontally inside frame). */
    int btn_row = dialog_button_row((int)dlg->line_count, dlg->type == DIALOG_INPUT);
    if (btn_row + 1 < h && dlg->ok_button) {
        int ok_w = button_width(dlg->ok_button);
        int cancel_w = dlg->cancel_button ? button_width(dlg->cancel_button) : 0;
        int total = ok_w + (cancel_w > 0 ? DIALOG_BUTTON_GAP + cancel_w : 0);
        int avail = inner_right - inner_x;
        int start_x = inner_x + (avail - total) / 2;
        if (start_x < inner_x) start_x = inner_x;

        button_render(dlg->ok_button, draw_list, y + btn_row, start_x,
                      &styles->btn_normal, &styles->btn_focused, NULL);
        if (dlg->cancel_button) {
            button_render(dlg->cancel_button, draw_list, y + btn_row,
                          start_x + ok_w + DIALOG_BUTTON_GAP,
                          &styles->btn_normal, &styles->btn_focused, NULL);
        }
    }
}