#ifndef RETRODESK_UI_MENU_BAR_H
#define RETRODESK_UI_MENU_BAR_H

#include <stdbool.h>
#include <stddef.h>

#include "core/event.h"
#include "render/render.h"

/* Top-level menu bar widget (File|Edit|View|Help ...) with dropdowns.
   Produces DrawList commands; never calls backend draw APIs directly.
   Each menu item has an optional shortcut letter (typically the first
   character of the label) rendered with shortcut_style. */

#define MENU_BAR_NO_MENU ((size_t)-1)

typedef void (*MenuBarItemCallback)(size_t menu_index, size_t item_index,
                                    void *user_data);

typedef enum MenuBarItemKind {
    MENU_BAR_ITEM_ACTION = 0,
    MENU_BAR_ITEM_SEPARATOR,
} MenuBarItemKind;

typedef struct MenuBarItem {
    MenuBarItemKind kind;
    char *label;            /* NULL for separators */
    char shortcut;          /* 0 if none */
    MenuBarItemCallback callback;
    void *user_data;
    int x_offset;           /* x position within dropdown (computed on render) */
} MenuBarItem;

typedef struct MenuBarMenu {
    char *label;
    char shortcut;
    MenuBarItem *items;
    size_t item_count;
    size_t item_capacity;
    int x_offset;           /* x position on the bar (computed on render) */
    int width;              /* rendered width including trailing gap */
} MenuBarMenu;

typedef struct MenuBar MenuBar;

/* --- lifecycle ------------------------------------------------------- */

MenuBar *menu_bar_create(void);
void menu_bar_destroy(MenuBar *bar);

/* --- menus ----------------------------------------------------------- */

bool menu_bar_add_menu(MenuBar *bar, const char *label, char shortcut);
size_t menu_bar_menu_count(const MenuBar *bar);
const char *menu_bar_menu_label(const MenuBar *bar, size_t menu_index);
char menu_bar_menu_shortcut(const MenuBar *bar, size_t menu_index);

/* --- items ----------------------------------------------------------- */

bool menu_bar_add_item(MenuBar *bar, size_t menu_index,
                       const char *label, char shortcut,
                       MenuBarItemCallback callback, void *user_data);
bool menu_bar_add_separator(MenuBar *bar, size_t menu_index);
size_t menu_bar_item_count(const MenuBar *bar, size_t menu_index);

/* --- open state ------------------------------------------------------ */

/* Returns MENU_BAR_NO_MENU if no dropdown is open. */
size_t menu_bar_open_menu(const MenuBar *bar);
size_t menu_bar_open_item(const MenuBar *bar);
void menu_bar_open(MenuBar *bar, size_t menu_index);
void menu_bar_close(MenuBar *bar);

/* Returns true if the dropdown is currently visible. */
bool menu_bar_is_open(const MenuBar *bar);

/* --- events ---------------------------------------------------------- */

/* Handles F10/arrows/enter/escape and printable letters (shortcuts).
   Returns true if the event was consumed. */
bool menu_bar_handle_key(MenuBar *bar, const RetroKeyEvent *key);

/* Handles clicks on the bar and within the dropdown.
   bar_y/bar_x/bar_w: position of the top-level bar.
   dropdown_y/dropdown_x: position of the currently open dropdown, if any. */
bool menu_bar_handle_mouse(MenuBar *bar, const RetroMouseEvent *mouse,
                           int bar_y, int bar_x, int bar_w);

/* --- rendering ------------------------------------------------------- */

void menu_bar_render(const MenuBar *bar, DrawList *draw_list,
                     int y, int x, int w,
                     const RenderStyle *normal_style,
                     const RenderStyle *selected_style,
                     const RenderStyle *shortcut_style,
                     const RenderStyle *shortcut_selected_style);

/* Renders the open dropdown just below the top-level bar. Returns the
   number of rows used so the caller can layout content beneath.
   If no dropdown is open this is a no-op and returns 0. */
int menu_bar_render_dropdown(const MenuBar *bar, DrawList *draw_list,
                             int y, int x,
                             const RenderStyle *item_normal_style,
                             const RenderStyle *item_selected_style,
                             const RenderStyle *separator_style,
                             const RenderStyle *shortcut_style,
                             const RenderStyle *frame_style);

/* Computed widths for layout. */
int menu_bar_width(const MenuBar *bar);
int menu_bar_dropdown_width(const MenuBar *bar, size_t menu_index);
int menu_bar_dropdown_height(const MenuBar *bar, size_t menu_index);

#endif