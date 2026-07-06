#include "ui/scroll_list.h"

#include <stdlib.h>
#include <string.h>

#include "core/key_chord.h"

enum {
    SCROLL_LIST_INIT_CAP = 16,
    SCROLL_LIST_ITEM_BUF = 256,
    SCROLL_LIST_PAGE_SIZE = 10,
};

struct ScrollList {
    char **items;
    size_t count;
    size_t capacity;
    size_t selected;
    size_t scroll_offset;
};

/* --- internal helpers ------------------------------------------------- */

static bool scroll_list_grow(ScrollList *list, size_t need) {
    if (need <= list->capacity) return true;
    size_t new_cap = list->capacity ? list->capacity * 2 : SCROLL_LIST_INIT_CAP;
    while (new_cap < need) new_cap *= 2;
    char **next = realloc(list->items, new_cap * sizeof(char *));
    if (!next) return false;
    list->items = next;
    list->capacity = new_cap;
    return true;
}

static char *scroll_list_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = malloc(len + 1);
    if (!dup) return NULL;
    memcpy(dup, s, len + 1);
    return dup;
}

static void scroll_list_free_items(ScrollList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i]);
        list->items[i] = NULL;
    }
    list->count = 0;
}

/* --- lifecycle -------------------------------------------------------- */

ScrollList *scroll_list_create(void) {
    ScrollList *list = calloc(1, sizeof(*list));
    if (!list) return NULL;
    return list;
}

void scroll_list_destroy(ScrollList *list) {
    if (!list) return;
    scroll_list_free_items(list);
    free(list->items);
    free(list);
}

/* --- item management -------------------------------------------------- */

bool scroll_list_set_items(ScrollList *list, const char *const *items,
                           size_t count) {
    if (!list) return false;
    scroll_list_free_items(list);
    if (count == 0 || !items) {
        list->selected = 0;
        list->scroll_offset = 0;
        return true;
    }
    if (!scroll_list_grow(list, count)) return false;
    for (size_t i = 0; i < count; i++) {
        list->items[i] = scroll_list_strdup(items[i] ? items[i] : "");
        if (!list->items[i]) {
            /* Rollback on allocation failure. */
            for (size_t j = 0; j < i; j++) {
                free(list->items[j]);
                list->items[j] = NULL;
            }
            return false;
        }
    }
    list->count = count;
    list->selected = 0;
    list->scroll_offset = 0;
    return true;
}

bool scroll_list_append(ScrollList *list, const char *item) {
    if (!list) return false;
    if (!scroll_list_grow(list, list->count + 1)) return false;
    list->items[list->count] = scroll_list_strdup(item ? item : "");
    if (!list->items[list->count]) return false;
    list->count++;
    return true;
}

void scroll_list_clear(ScrollList *list) {
    if (!list) return;
    scroll_list_free_items(list);
    list->selected = 0;
    list->scroll_offset = 0;
}

size_t scroll_list_count(const ScrollList *list) {
    if (!list) return 0;
    return list->count;
}

const char *scroll_list_item(const ScrollList *list, size_t index) {
    if (!list || index >= list->count) return NULL;
    return list->items[index];
}

/* --- selection -------------------------------------------------------- */

size_t scroll_list_selected(const ScrollList *list) {
    if (!list) return 0;
    return list->selected;
}

void scroll_list_select(ScrollList *list, size_t index) {
    if (!list || list->count == 0) return;
    if (index >= list->count) index = list->count - 1;
    list->selected = index;
}

/* --- scroll state ----------------------------------------------------- */

size_t scroll_list_scroll_offset(const ScrollList *list) {
    if (!list) return 0;
    return list->scroll_offset;
}

/* --- key handling ----------------------------------------------------- */

bool scroll_list_handle_key(ScrollList *list, const RetroKeyEvent *key) {
    if (!list || !key || list->count == 0) return false;

    int code = key->key_code;

    if (code == RETRO_KEY_UP) {
        if (list->selected > 0) list->selected--;
        return true;
    }
    if (code == RETRO_KEY_DOWN) {
        if (list->selected < list->count - 1) list->selected++;
        return true;
    }
    if (code == RETRO_KEY_HOME) {
        list->selected = 0;
        return true;
    }
    if (code == RETRO_KEY_END) {
        list->selected = list->count - 1;
        return true;
    }
    if (code == RETRO_KEY_PPAGE) {
        if (list->selected >= SCROLL_LIST_PAGE_SIZE) {
            list->selected -= SCROLL_LIST_PAGE_SIZE;
        } else {
            list->selected = 0;
        }
        return true;
    }
    if (code == RETRO_KEY_NPAGE) {
        list->selected += SCROLL_LIST_PAGE_SIZE;
        if (list->selected >= list->count) {
            list->selected = list->count - 1;
        }
        return true;
    }

    /* Ctrl+N — move down */
    if (code == RETRO_KEY_CTRL_N) {
        if (list->selected < list->count - 1) list->selected++;
        return true;
    }

    /* Ctrl+P — move up */
    if (code == RETRO_KEY_CTRL_P) {
        if (list->selected > 0) list->selected--;
        return true;
    }

    return false;
}

/* --- mouse handling --------------------------------------------------- */

bool scroll_list_handle_mouse(ScrollList *list, const RetroMouseEvent *mouse,
                              int widget_y, int widget_x,
                              int visible_height, int visible_width) {
    if (!list || !mouse || list->count == 0) return false;
    if (visible_height <= 0 || visible_width <= 0) return false;

    /* Scroll wheel */
    if (mouse->scroll_up) {
        if (list->selected > 0) list->selected--;
        return true;
    }
    if (mouse->scroll_down) {
        if (list->selected < list->count - 1) list->selected++;
        return true;
    }

    /* Click to select */
    if (mouse->button1_clicked || mouse->button1_pressed) {
        int rel_y = mouse->y - widget_y;
        int rel_x = mouse->x - widget_x;
        if (rel_y < 0 || rel_y >= visible_height) return false;
        if (rel_x < 0 || rel_x >= visible_width) return false;
        size_t clicked = list->scroll_offset + (size_t)rel_y;
        if (clicked < list->count) {
            list->selected = clicked;
            return true;
        }
    }

    return false;
}

/* --- render ----------------------------------------------------------- */

void scroll_list_render(const ScrollList *list, DrawList *draw_list,
                        int y, int x, int visible_height, int visible_width,
                        const RenderStyle *normal_style,
                        const RenderStyle *selected_style) {
    if (!list || !draw_list || visible_height <= 0 || visible_width <= 0) {
        return;
    }

    /* Compute effective scroll offset to keep selection visible. */
    size_t scroll = list->scroll_offset;
    size_t vh = (size_t)visible_height;
    if (list->count > 0) {
        if (list->selected < scroll) {
            scroll = list->selected;
        }
        if (list->selected >= scroll + vh) {
            scroll = list->selected - vh + 1;
        }
    }

    size_t vw = (size_t)visible_width;
    char line[SCROLL_LIST_ITEM_BUF];

    for (size_t row = 0; row < vh; row++) {
        size_t item_idx = scroll + row;
        const RenderStyle *style = normal_style;
        const char *text = "";

        if (item_idx < list->count) {
            text = list->items[item_idx];
            if (item_idx == list->selected && selected_style) {
                style = selected_style;
            }
        }

        /* Build padded line. */
        size_t text_len = strlen(text);
        size_t copy_len = text_len < vw ? text_len : vw;
        if (copy_len > sizeof(line) - 1) copy_len = sizeof(line) - 1;
        memcpy(line, text, copy_len);

        /* Pad with spaces. */
        size_t pad_end = vw < sizeof(line) - 1 ? vw : sizeof(line) - 1;
        for (size_t p = copy_len; p < pad_end; p++) {
            line[p] = ' ';
        }
        line[pad_end] = '\0';

        draw_list_text(draw_list, y + (int)row, x, line, style);
    }
}
