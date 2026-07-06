#include "ui/tab.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum {
    TAB_INIT_CAP = 4,
    TAB_BRACKET_OVERHEAD = 3,   /* "[ " + "]" */
    TAB_TAB_GAP = 1,
};

typedef struct TabEntry {
    char *label;
    int x_offset;
    int width;
} TabEntry;

struct Tab {
    TabEntry *entries;
    size_t count;
    size_t capacity;
    size_t active;
    TabChangeCallback on_change;
    void *user_data;
};

/* --- internal helpers ------------------------------------------------- */

static char *tab_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = malloc(len + 1);
    if (!dup) return NULL;
    memcpy(dup, s, len + 1);
    return dup;
}

static bool tab_grow(Tab *tab, size_t need) {
    if (need <= tab->capacity) return true;
    size_t new_cap = tab->capacity ? tab->capacity * 2 : TAB_INIT_CAP;
    while (new_cap < need) new_cap *= 2;
    TabEntry *next = realloc(tab->entries, new_cap * sizeof(TabEntry));
    if (!next) return false;
    tab->entries = next;
    tab->capacity = new_cap;
    return true;
}

static void tab_recompute_layout(Tab *tab) {
    if (!tab) return;
    int x = 0;
    for (size_t i = 0; i < tab->count; i++) {
        tab->entries[i].x_offset = x;
        int label_len = (int)strlen(tab->entries[i].label);
        tab->entries[i].width = label_len + TAB_BRACKET_OVERHEAD;
        x += tab->entries[i].width;
        if (i + 1 < tab->count) x += TAB_TAB_GAP;
    }
}

static void tab_fire_change(Tab *tab, size_t old_index, size_t new_index) {
    if (old_index == new_index) return;
    if (tab->on_change) {
        tab->on_change(old_index, new_index, tab->user_data);
    }
}

/* --- lifecycle ------------------------------------------------------- */

Tab *tab_create(void) {
    Tab *tab = calloc(1, sizeof(*tab));
    if (!tab) return NULL;
    tab->active = 0;
    return tab;
}

void tab_destroy(Tab *tab) {
    if (!tab) return;
    for (size_t i = 0; i < tab->count; i++) {
        free(tab->entries[i].label);
        tab->entries[i].label = NULL;
    }
    free(tab->entries);
    tab->entries = NULL;
    tab->count = 0;
    tab->capacity = 0;
    free(tab);
}

/* --- tabs ------------------------------------------------------------ */

size_t tab_add(Tab *tab, const char *label) {
    if (!tab || !label) return (size_t)-1;
    if (!tab_grow(tab, tab->count + 1)) return (size_t)-1;
    TabEntry *e = &tab->entries[tab->count];
    e->label = tab_strdup(label);
    if (!e->label) return (size_t)-1;
    e->x_offset = 0;
    e->width = 0;
    tab->count++;
    tab_recompute_layout(tab);
    return tab->count - 1;
}

size_t tab_count(const Tab *tab) {
    if (!tab) return 0;
    return tab->count;
}

const char *tab_label(const Tab *tab, size_t index) {
    if (!tab || index >= tab->count) return "";
    return tab->entries[index].label;
}

/* --- active tab ------------------------------------------------------ */

size_t tab_active(const Tab *tab) {
    if (!tab || tab->count == 0) return (size_t)-1;
    return tab->active;
}

void tab_set_active(Tab *tab, size_t index) {
    if (!tab || tab->count == 0) return;
    if (index >= tab->count) index = tab->count - 1;
    size_t old = tab->active;
    tab->active = index;
    tab_fire_change(tab, old, index);
}

/* --- change callback ------------------------------------------------ */

void tab_set_change_callback(Tab *tab, TabChangeCallback callback,
                             void *user_data) {
    if (!tab) return;
    tab->on_change = callback;
    tab->user_data = user_data;
}

/* --- key handling ---------------------------------------------------- */

bool tab_handle_key(Tab *tab, const RetroKeyEvent *key) {
    if (!tab || !key) return false;
    if (tab->count == 0) return false;

    /* Number keys 1..9 jump to that tab. */
    if (key->is_printable && key->ascii >= '1' && key->ascii <= '9') {
        size_t target = (size_t)(key->ascii - '1');
        if (target < tab->count) {
            size_t old = tab->active;
            tab->active = target;
            tab_fire_change(tab, old, target);
            return true;
        }
        return false;
    }

#ifdef KEY_LEFT
    int code = key->key_code;
    if (code == KEY_LEFT) {
        size_t old = tab->active;
        if (tab->active == 0) {
            tab->active = tab->count - 1;
        } else {
            tab->active--;
        }
        tab_fire_change(tab, old, tab->active);
        return true;
    }
    if (code == KEY_RIGHT) {
        size_t old = tab->active;
        tab->active = (tab->active + 1) % tab->count;
        tab_fire_change(tab, old, tab->active);
        return true;
    }
    if (code == KEY_HOME) {
        tab_set_active(tab, 0);
        return true;
    }
    if (code == KEY_END) {
        tab_set_active(tab, tab->count - 1);
        return true;
    }
#endif

    /* Ctrl+Tab / Ctrl+Shift+Tab could be added once we track modifiers. */
    return false;
}

bool tab_handle_mouse(Tab *tab, const RetroMouseEvent *mouse,
                      int tab_y, int tab_x, int tab_w) {
    if (!tab || !mouse) return false;
    if (tab->count == 0) return false;
    if (tab_w <= 0) return false;
    if (mouse->y != tab_y) return false;
    int rel_x = mouse->x - tab_x;
    if (rel_x < 0 || rel_x >= tab_w) return false;
    if (!mouse->button1_clicked) return false;

    int cursor = 0;
    for (size_t i = 0; i < tab->count; i++) {
        int next = cursor + tab->entries[i].width;
        if (i + 1 < tab->count) next += TAB_TAB_GAP;
        if (rel_x >= cursor && rel_x < next) {
            size_t old = tab->active;
            tab->active = i;
            tab_fire_change(tab, old, i);
            return true;
        }
        cursor = next;
    }
    return false;
}

/* --- rendering ------------------------------------------------------- */

int tab_width(const Tab *tab) {
    if (!tab || tab->count == 0) return 0;
    int w = 0;
    for (size_t i = 0; i < tab->count; i++) {
        w += tab->entries[i].width;
        if (i + 1 < tab->count) w += TAB_TAB_GAP;
    }
    return w;
}

int tab_height(void) {
    return 1;
}

int tab_render(const Tab *tab, DrawList *draw_list,
               int y, int x, int max_width,
               const RenderStyle *normal_style,
               const RenderStyle *active_style) {
    if (!tab || !draw_list || !normal_style) return 0;
    if (max_width <= 0 || tab->count == 0) return 0;

    int cursor = x;
    int used = 0;
    for (size_t i = 0; i < tab->count; i++) {
        const TabEntry *e = &tab->entries[i];
        const RenderStyle *style = normal_style;
        if (i == tab->active && active_style) style = active_style;

        int seg_w = e->width;
        if (used + seg_w > max_width) seg_w = max_width - used;
        if (seg_w <= 0) break;

        /* Build "[ label ]" segment, truncating if necessary. */
        char seg[128];
        int label_len = (int)strlen(e->label);
        int interior = seg_w - 2; /* "[ " */
        if (interior < 1) interior = 1;
        int copy = label_len < interior ? label_len : interior;
        snprintf(seg, sizeof(seg), "[ %.*s", copy, e->label);
        size_t slen = strlen(seg);
        /* Add closing ']' if room. */
        if ((int)slen < seg_w - 1) {
            seg[slen++] = ']';
            seg[slen] = '\0';
        } else if ((int)slen < seg_w) {
            seg[slen] = ']';
            seg[slen + 1] = '\0';
            slen++;
        } else {
            seg[seg_w] = '\0';
            slen = (size_t)seg_w;
        }
        draw_list_text(draw_list, y, cursor, seg, style);

        cursor += seg_w;
        used += seg_w;
        if (i + 1 < tab->count && used + TAB_TAB_GAP <= max_width) {
            draw_list_text(draw_list, y, cursor, " ", normal_style);
            cursor += TAB_TAB_GAP;
            used += TAB_TAB_GAP;
        }
    }
    return used;
}