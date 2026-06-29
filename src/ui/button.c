#include "ui/button.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum {
    BUTTON_LABEL_INIT_CAP = 32,
    BUTTON_BRACKET_OVERHEAD = 4,  /* "[ " + " ]" */
};

struct Button {
    char *label;
    size_t label_len;
    bool focused;
    bool enabled;
    ButtonCallback callback;
    void *user_data;
};

/* --- internal helpers ------------------------------------------------- */

static char *button_strdup(const char *s) {
    if (!s) {
        s = "";
    }
    size_t len = strlen(s);
    char *dup = malloc(len + 1);
    if (!dup) {
        return NULL;
    }
    memcpy(dup, s, len + 1);
    return dup;
}

static void button_activate(Button *button) {
    if (!button || !button->enabled) {
        return;
    }
    if (button->callback) {
        button->callback(button, button->user_data);
    }
}

/* --- lifecycle -------------------------------------------------------- */

Button *button_create(const char *label) {
    Button *btn = calloc(1, sizeof(*btn));
    if (!btn) {
        return NULL;
    }
    btn->label = button_strdup(label);
    if (!btn->label) {
        free(btn);
        return NULL;
    }
    btn->label_len = strlen(btn->label);
    btn->focused = false;
    btn->enabled = true;
    btn->callback = NULL;
    btn->user_data = NULL;
    return btn;
}

void button_destroy(Button *button) {
    if (!button) {
        return;
    }
    free(button->label);
    button->label = NULL;
    free(button);
}

/* --- label ------------------------------------------------------------ */

const char *button_label(const Button *button) {
    if (!button) {
        return "";
    }
    return button->label;
}

bool button_set_label(Button *button, const char *label) {
    if (!button) {
        return false;
    }
    char *new_label = button_strdup(label);
    if (!new_label) {
        return false;
    }
    free(button->label);
    button->label = new_label;
    button->label_len = strlen(new_label);
    return true;
}

/* --- focus ------------------------------------------------------------ */

bool button_focused(const Button *button) {
    if (!button) {
        return false;
    }
    return button->focused;
}

void button_set_focused(Button *button, bool focused) {
    if (!button) {
        return;
    }
    button->focused = focused;
}

/* --- enabled ---------------------------------------------------------- */

bool button_enabled(const Button *button) {
    if (!button) {
        return false;
    }
    return button->enabled;
}

void button_set_enabled(Button *button, bool enabled) {
    if (!button) {
        return;
    }
    button->enabled = enabled;
}

/* --- callback --------------------------------------------------------- */

void button_set_callback(Button *button, ButtonCallback callback,
                         void *user_data) {
    if (!button) {
        return;
    }
    button->callback = callback;
    button->user_data = user_data;
}

/* --- event handling --------------------------------------------------- */

bool button_handle_key(Button *button, const RetroKeyEvent *key) {
    if (!button || !key) {
        return false;
    }
    if (!button->focused || !button->enabled) {
        return false;
    }
    /* Enter (key_code 10 or '\r' or '\n') or Space activates */
    bool is_enter = (key->key_code == '\n' || key->key_code == '\r' ||
                     key->key_code == 10 || key->key_code == 13);
    bool is_space = (key->is_printable && key->ascii == ' ');
    if (is_enter || is_space) {
        button_activate(button);
        return true;
    }
    return false;
}

bool button_handle_mouse(Button *button, const RetroMouseEvent *mouse,
                         int widget_y, int widget_x, int widget_w) {
    if (!button || !mouse) {
        return false;
    }
    if (!button->enabled) {
        return false;
    }
    if (!mouse->button1_clicked) {
        return false;
    }
    /* Check bounds */
    int rel_y = mouse->y - widget_y;
    int rel_x = mouse->x - widget_x;
    if (rel_y != 0) {
        return false;
    }
    if (rel_x < 0 || rel_x >= widget_w) {
        return false;
    }
    button_activate(button);
    return true;
}

/* --- render ----------------------------------------------------------- */

int button_width(const Button *button) {
    if (!button) {
        return 0;
    }
    return (int)button->label_len + BUTTON_BRACKET_OVERHEAD;
}

void button_render(const Button *button, DrawList *draw_list,
                   int y, int x,
                   const RenderStyle *normal_style,
                   const RenderStyle *focused_style,
                   const RenderStyle *disabled_style) {
    if (!button || !draw_list) {
        return;
    }
    const RenderStyle *style;
    if (!button->enabled) {
        style = disabled_style ? disabled_style : normal_style;
    } else if (button->focused) {
        style = focused_style ? focused_style : normal_style;
    } else {
        style = normal_style;
    }
    if (!style) {
        return;
    }
    /* Build "[ Label ]" string */
    int width = button_width(button);
    /* +1 for null terminator */
    size_t buf_size = (size_t)width + 1;
    char *buf = malloc(buf_size);
    if (!buf) {
        return;
    }
    snprintf(buf, buf_size, "[ %s ]", button->label);
    draw_list_text(draw_list, y, x, buf, style);
    free(buf);
}
