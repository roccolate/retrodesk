#include "ui/menu_bar.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "core/key_chord.h"

enum {
    MENU_BAR_MENU_INIT_CAP = 4,
    MENU_BAR_ITEM_INIT_CAP = 4,
    MENU_BAR_LABEL_GAP = 2,        /* spaces between top-level menus */
    MENU_BAR_DROPDOWN_PAD = 2,     /* left/right padding inside dropdown */
    MENU_BAR_ITEM_PAD = 1,         /* vertical padding between items */
};

struct MenuBar {
    MenuBarMenu *menus;
    size_t menu_count;
    size_t menu_capacity;
    size_t open_menu;
    size_t open_item;
};

/* --- internal helpers ------------------------------------------------- */

static char *menu_bar_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = malloc(len + 1);
    if (!dup) return NULL;
    memcpy(dup, s, len + 1);
    return dup;
}

static bool menu_bar_grow_menus(MenuBar *bar, size_t need) {
    if (need <= bar->menu_capacity) return true;
    size_t new_cap = bar->menu_capacity ? bar->menu_capacity * 2
                                       : MENU_BAR_MENU_INIT_CAP;
    while (new_cap < need) new_cap *= 2;
    MenuBarMenu *next = realloc(bar->menus, new_cap * sizeof(MenuBarMenu));
    if (!next) return false;
    bar->menus = next;
    bar->menu_capacity = new_cap;
    return true;
}

static bool menu_bar_grow_items(MenuBarMenu *menu, size_t need) {
    if (need <= menu->item_capacity) return true;
    size_t new_cap = menu->item_capacity ? menu->item_capacity * 2
                                         : MENU_BAR_ITEM_INIT_CAP;
    while (new_cap < need) new_cap *= 2;
    MenuBarItem *next = realloc(menu->items, new_cap * sizeof(MenuBarItem));
    if (!next) return false;
    menu->items = next;
    menu->item_capacity = new_cap;
    return true;
}

static void menu_bar_clear_items(MenuBarMenu *menu) {
    if (!menu || !menu->items) return;
    for (size_t i = 0; i < menu->item_count; i++) {
        free(menu->items[i].label);
        menu->items[i].label = NULL;
    }
    free(menu->items);
    menu->items = NULL;
    menu->item_count = 0;
    menu->item_capacity = 0;
}

static int menu_bar_shortcut_offset(const char *label, char shortcut) {
    /* The caller conventionally places the shortcut as the first letter of
       the label. We highlight that single character. If the shortcut does
       not match label[0], fall back to position 0. */
    if (!label || shortcut == 0) return -1;
    if (label[0] == shortcut) return 0;
    return 0;
}

static int menu_bar_menu_text_width(const MenuBarMenu *menu) {
    if (!menu || !menu->label) return 0;
    return (int)strlen(menu->label);
}

static int menu_bar_item_text_width(const MenuBarItem *item) {
    if (!item) return 0;
    if (item->kind == MENU_BAR_ITEM_SEPARATOR) return 0;
    return (int)strlen(item->label ? item->label : "");
}

static void menu_bar_recompute_layout(MenuBar *bar) {
    if (!bar) return;
    int x = MENU_BAR_LABEL_GAP;
    for (size_t i = 0; i < bar->menu_count; i++) {
        MenuBarMenu *m = &bar->menus[i];
        m->x_offset = x;
        m->width = menu_bar_menu_text_width(m) + MENU_BAR_LABEL_GAP;
        x += m->width;
    }
}

static int menu_bar_item_row(const MenuBarItem *items, size_t idx) {
    /* Compute the rendered row of item idx, accounting for separators
       and padding. */
    int row = 1; /* top border */
    for (size_t i = 0; i < idx && items; i++) {
        if (items[i].kind == MENU_BAR_ITEM_SEPARATOR) {
            row += 1 + MENU_BAR_ITEM_PAD;
        } else {
            row += 1 + MENU_BAR_ITEM_PAD;
        }
    }
    return row;
}

/* --- lifecycle ------------------------------------------------------- */

MenuBar *menu_bar_create(void) {
    MenuBar *bar = calloc(1, sizeof(*bar));
    if (!bar) return NULL;
    bar->open_menu = MENU_BAR_NO_MENU;
    bar->open_item = MENU_BAR_NO_MENU;
    return bar;
}

void menu_bar_destroy(MenuBar *bar) {
    if (!bar) return;
    for (size_t i = 0; i < bar->menu_count; i++) {
        free(bar->menus[i].label);
        bar->menus[i].label = NULL;
        menu_bar_clear_items(&bar->menus[i]);
    }
    free(bar->menus);
    bar->menus = NULL;
    bar->menu_count = 0;
    bar->menu_capacity = 0;
    free(bar);
}

/* --- menus ----------------------------------------------------------- */

bool menu_bar_add_menu(MenuBar *bar, const char *label, char shortcut) {
    if (!bar || !label) return false;
    if (!menu_bar_grow_menus(bar, bar->menu_count + 1)) return false;
    MenuBarMenu *m = &bar->menus[bar->menu_count];
    m->label = menu_bar_strdup(label);
    if (!m->label) return false;
    m->shortcut = shortcut;
    m->items = NULL;
    m->item_count = 0;
    m->item_capacity = 0;
    m->x_offset = 0;
    m->width = 0;
    bar->menu_count++;
    menu_bar_recompute_layout(bar);
    return true;
}

size_t menu_bar_menu_count(const MenuBar *bar) {
    if (!bar) return 0;
    return bar->menu_count;
}

const char *menu_bar_menu_label(const MenuBar *bar, size_t menu_index) {
    if (!bar || menu_index >= bar->menu_count) return "";
    return bar->menus[menu_index].label;
}

char menu_bar_menu_shortcut(const MenuBar *bar, size_t menu_index) {
    if (!bar || menu_index >= bar->menu_count) return 0;
    return bar->menus[menu_index].shortcut;
}

/* --- items ----------------------------------------------------------- */

bool menu_bar_add_item(MenuBar *bar, size_t menu_index,
                       const char *label, char shortcut,
                       MenuBarItemCallback callback, void *user_data) {
    if (!bar || !label) return false;
    if (menu_index >= bar->menu_count) return false;
    MenuBarMenu *menu = &bar->menus[menu_index];
    if (!menu_bar_grow_items(menu, menu->item_count + 1)) return false;
    MenuBarItem *it = &menu->items[menu->item_count];
    it->kind = MENU_BAR_ITEM_ACTION;
    it->label = menu_bar_strdup(label);
    if (!it->label) return false;
    it->shortcut = shortcut;
    it->callback = callback;
    it->user_data = user_data;
    it->x_offset = 0;
    menu->item_count++;
    return true;
}

bool menu_bar_add_separator(MenuBar *bar, size_t menu_index) {
    if (!bar) return false;
    if (menu_index >= bar->menu_count) return false;
    MenuBarMenu *menu = &bar->menus[menu_index];
    if (!menu_bar_grow_items(menu, menu->item_count + 1)) return false;
    MenuBarItem *it = &menu->items[menu->item_count];
    it->kind = MENU_BAR_ITEM_SEPARATOR;
    it->label = NULL;
    it->shortcut = 0;
    it->callback = NULL;
    it->user_data = NULL;
    it->x_offset = 0;
    menu->item_count++;
    return true;
}

size_t menu_bar_item_count(const MenuBar *bar, size_t menu_index) {
    if (!bar || menu_index >= bar->menu_count) return 0;
    return bar->menus[menu_index].item_count;
}

/* --- open state ------------------------------------------------------ */

size_t menu_bar_open_menu(const MenuBar *bar) {
    if (!bar || bar->open_menu == MENU_BAR_NO_MENU) return MENU_BAR_NO_MENU;
    return bar->open_menu;
}

size_t menu_bar_open_item(const MenuBar *bar) {
    if (!bar) return MENU_BAR_NO_MENU;
    if (bar->open_menu == MENU_BAR_NO_MENU) return MENU_BAR_NO_MENU;
    return bar->open_item;
}

void menu_bar_open(MenuBar *bar, size_t menu_index) {
    if (!bar) return;
    if (menu_index >= bar->menu_count) return;
    bar->open_menu = menu_index;
    bar->open_item = (bar->menus[menu_index].item_count > 0) ? 0
                                                              : MENU_BAR_NO_MENU;
}

void menu_bar_close(MenuBar *bar) {
    if (!bar) return;
    bar->open_menu = MENU_BAR_NO_MENU;
    bar->open_item = MENU_BAR_NO_MENU;
}

bool menu_bar_is_open(const MenuBar *bar) {
    if (!bar) return false;
    return bar->open_menu != MENU_BAR_NO_MENU;
}

/* --- key handling ---------------------------------------------------- */

static bool menu_bar_shortcut_matches(char sc, char ascii) {
    if (sc == 0 || ascii == 0) return false;
    /* Case-insensitive compare. */
    char a = ascii;
    if (a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
    char s = sc;
    if (s >= 'a' && s <= 'z') s = (char)(s - 'a' + 'A');
    return a == s;
}

static void menu_bar_activate_current(MenuBar *bar) {
    if (!bar) return;
    if (bar->open_menu == MENU_BAR_NO_MENU) return;
    if (bar->open_item == MENU_BAR_NO_MENU) return;
    MenuBarMenu *m = &bar->menus[bar->open_menu];
    if (bar->open_item >= m->item_count) return;
    MenuBarItem *it = &m->items[bar->open_item];
    if (it->kind == MENU_BAR_ITEM_SEPARATOR) return;
    if (it->callback) {
        it->callback(bar->open_menu, bar->open_item, it->user_data);
    }
    menu_bar_close(bar);
}

bool menu_bar_handle_key(MenuBar *bar, const RetroKeyEvent *key) {
    if (!bar || !key) return false;
    if (bar->menu_count == 0) return false;

    int code = key->key_code;
    bool open = menu_bar_is_open(bar);

    /* Escape always closes when open. */
    if (code == RETRO_KEY_ESC) {
        if (open) {
            menu_bar_close(bar);
            return true;
        }
        return false;
    }

    /* F10 toggles the first menu. */
    if (code == RETRO_KEY_F10) {
        if (open) {
            menu_bar_close(bar);
        } else {
            menu_bar_open(bar, 0);
        }
        return true;
    }

    /* Arrow keys: Left/Right cycle menus (open or close after last). */
    if (code == RETRO_KEY_LEFT) {
        if (open) {
            if (bar->open_menu == 0) {
                menu_bar_open(bar, bar->menu_count - 1);
            } else {
                menu_bar_open(bar, bar->open_menu - 1);
            }
        }
        return true;
    }
    if (code == RETRO_KEY_RIGHT) {
        if (open) {
            if (bar->open_menu + 1 >= bar->menu_count) {
                menu_bar_open(bar, 0);
            } else {
                menu_bar_open(bar, bar->open_menu + 1);
            }
        }
        return true;
    }

    /* Up/Down navigate items. */
    if (code == RETRO_KEY_UP) {
        if (!open) return false;
        MenuBarMenu *m = &bar->menus[bar->open_menu];
        if (m->item_count == 0) return true;
        size_t idx = (bar->open_item == MENU_BAR_NO_MENU) ? 0
                                                          : bar->open_item;
        for (size_t step = 0; step < m->item_count; step++) {
            idx = (idx == 0) ? m->item_count - 1 : idx - 1;
            if (m->items[idx].kind == MENU_BAR_ITEM_ACTION) break;
        }
        bar->open_item = idx;
        return true;
    }
    if (code == RETRO_KEY_DOWN) {
        if (!open) return false;
        MenuBarMenu *m = &bar->menus[bar->open_menu];
        if (m->item_count == 0) return true;
        size_t idx = (bar->open_item == MENU_BAR_NO_MENU) ? 0
                                                          : bar->open_item;
        for (size_t step = 0; step < m->item_count; step++) {
            idx = (idx + 1) % m->item_count;
            if (m->items[idx].kind == MENU_BAR_ITEM_ACTION) break;
        }
        bar->open_item = idx;
        return true;
    }

    /* Enter activates the current item. */
    if (code == RETRO_KEY_LF || code == RETRO_KEY_CR) {
        if (open) {
            menu_bar_activate_current(bar);
            return true;
        }
        return false;
    }

    /* Printable letter: matches top-level shortcut (opens menu) or,
       if a dropdown is open, an item shortcut (activates item). */
    if (key->is_printable && key->ascii != 0) {
        if (open) {
            MenuBarMenu *m = &bar->menus[bar->open_menu];
            for (size_t i = 0; i < m->item_count; i++) {
                if (m->items[i].kind == MENU_BAR_ITEM_ACTION &&
                    menu_bar_shortcut_matches(m->items[i].shortcut,
                                              key->ascii)) {
                    bar->open_item = i;
                    menu_bar_activate_current(bar);
                    return true;
                }
            }
            return true; /* consume printable when open to avoid leaks */
        }
        /* When closed: match a top-level menu shortcut to open it. */
        for (size_t i = 0; i < bar->menu_count; i++) {
            if (menu_bar_shortcut_matches(bar->menus[i].shortcut,
                                          key->ascii)) {
                menu_bar_open(bar, i);
                return true;
            }
        }
    }

    return false;
}

bool menu_bar_handle_mouse(MenuBar *bar, const RetroMouseEvent *mouse,
                           int bar_y, int bar_x, int bar_w) {
    if (!bar || !mouse) return false;
    if (bar->menu_count == 0) return false;
    if (bar_w <= 0) return false;
    int rel_y = mouse->y - bar_y;
    int rel_x = mouse->x - bar_x;
    if (rel_y != 0) return false;

    /* Click on a top-level menu opens it. */
    if (rel_x < 0 || rel_x >= bar_w) return false;
    for (size_t i = 0; i < bar->menu_count; i++) {
        MenuBarMenu *m = &bar->menus[i];
        if (rel_x >= m->x_offset && rel_x < m->x_offset + m->width) {
            if (mouse->button1_clicked) {
                if (menu_bar_is_open(bar) && bar->open_menu == i) {
                    menu_bar_close(bar);
                } else {
                    menu_bar_open(bar, i);
                }
                return true;
            }
            return false;
        }
    }
    return false;
}

/* --- rendering ------------------------------------------------------- */

int menu_bar_width(const MenuBar *bar) {
    if (!bar || bar->menu_count == 0) return 0;
    int w = MENU_BAR_LABEL_GAP;
    for (size_t i = 0; i < bar->menu_count; i++) {
        w += menu_bar_menu_text_width(&bar->menus[i]) + MENU_BAR_LABEL_GAP;
    }
    return w;
}

int menu_bar_dropdown_width(const MenuBar *bar, size_t menu_index) {
    if (!bar || menu_index >= bar->menu_count) return 0;
    MenuBarMenu *m = &bar->menus[menu_index];
    int w = 0;
    for (size_t i = 0; i < m->item_count; i++) {
        int len = menu_bar_item_text_width(&m->items[i]);
        if (m->items[i].shortcut != 0 && m->items[i].kind == MENU_BAR_ITEM_ACTION) {
            len += 2; /* gap before shortcut */
        }
        if (len > w) w = len;
    }
    return w + 2 * MENU_BAR_DROPDOWN_PAD;
}

int menu_bar_dropdown_height(const MenuBar *bar, size_t menu_index) {
    if (!bar || menu_index >= bar->menu_count) return 0;
    MenuBarMenu *m = &bar->menus[menu_index];
    /* top border + each item (1 row + 1 pad) + bottom border */
    int h = 1;
    for (size_t i = 0; i < m->item_count; i++) {
        h += 1 + MENU_BAR_ITEM_PAD;
    }
    h += 1;
    return h;
}

void menu_bar_render(const MenuBar *bar, DrawList *draw_list,
                     int y, int x, int w,
                     const RenderStyle *normal_style,
                     const RenderStyle *selected_style,
                     const RenderStyle *shortcut_style,
                     const RenderStyle *shortcut_selected_style) {
    if (!bar || !draw_list || !normal_style) return;
    if (w <= 0) return;
    (void)selected_style;
    (void)shortcut_style;
    (void)shortcut_selected_style;

    for (size_t i = 0; i < bar->menu_count; i++) {
        MenuBarMenu *m = &bar->menus[i];
        const RenderStyle *style = normal_style;
        if (menu_bar_is_open(bar) && bar->open_menu == i) {
            style = selected_style ? selected_style : normal_style;
        }
        if (!m->label) continue;
        int mx = x + m->x_offset;
        int len = menu_bar_menu_text_width(m);
        if (len <= 0) continue;
        if (mx >= x + w) break;
        char buf[64];
        snprintf(buf, sizeof(buf), "%s", m->label);
        draw_list_text(draw_list, y, mx, buf, style);
    }
}

int menu_bar_render_dropdown(const MenuBar *bar, DrawList *draw_list,
                             int y, int x,
                             const RenderStyle *item_normal_style,
                             const RenderStyle *item_selected_style,
                             const RenderStyle *separator_style,
                             const RenderStyle *shortcut_style,
                             const RenderStyle *frame_style) {
    if (!bar || !draw_list) return 0;
    if (!menu_bar_is_open(bar)) return 0;
    size_t menu_index = bar->open_menu;
    if (menu_index >= bar->menu_count) return 0;
    MenuBarMenu *m = &bar->menus[menu_index];
    int dw = menu_bar_dropdown_width(bar, menu_index);
    int dh = menu_bar_dropdown_height(bar, menu_index);

    (void)frame_style;
    (void)separator_style;
    (void)shortcut_style;

    /* Fill the dropdown rectangle with spaces (one text command per row). */
    char line[256];
    if (dw > 0 && (size_t)dw < sizeof(line)) {
        for (int i = 0; i < dw; i++) line[i] = ' ';
        line[dw] = '\0';
        for (int row = 0; row < dh; row++) {
            draw_list_text(draw_list, y + row, x, line, item_normal_style);
        }
    }

    for (size_t i = 0; i < m->item_count; i++) {
        MenuBarItem *it = &m->items[i];
        int row = y + menu_bar_item_row(m->items, i);
        if (it->kind == MENU_BAR_ITEM_SEPARATOR) {
            /* Separator rendered as a horizontal line of dashes. */
            int sep_len = dw - 2 * MENU_BAR_DROPDOWN_PAD;
            if (sep_len > 0) {
                char dashes[64];
                int n = sep_len < (int)sizeof(dashes) - 1
                            ? sep_len : (int)sizeof(dashes) - 1;
                for (int k = 0; k < n; k++) dashes[k] = '-';
                dashes[n] = '\0';
                draw_list_text(draw_list, row, x + MENU_BAR_DROPDOWN_PAD,
                               dashes, separator_style);
            }
            continue;
        }
        const RenderStyle *style = item_normal_style;
        if (bar->open_item == i && item_selected_style) {
            style = item_selected_style;
        }
        char text[128];
        snprintf(text, sizeof(text), "%s", it->label ? it->label : "");
        draw_list_text(draw_list, row, x + MENU_BAR_DROPDOWN_PAD, text, style);
        if (it->shortcut != 0 && shortcut_style) {
            int sc_offset = menu_bar_shortcut_offset(it->label, it->shortcut);
            if (sc_offset >= 0 && (size_t)sc_offset < strlen(text)) {
                char sc_buf[2] = {(char)it->shortcut, 0};
                draw_list_text(draw_list, row,
                               x + MENU_BAR_DROPDOWN_PAD + sc_offset,
                               sc_buf, shortcut_style);
            }
        }
    }

    return dh;
}