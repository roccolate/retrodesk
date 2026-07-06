#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ui/menu_bar.h"

/* --- callback tracking ----------------------------------------------- */

static int g_activate_count;
static size_t g_last_menu;
static size_t g_last_item;
static void *g_last_user_data;

static void on_activate(size_t menu_index, size_t item_index, void *user_data) {
    g_activate_count++;
    g_last_menu = menu_index;
    g_last_item = item_index;
    g_last_user_data = user_data;
}

static void reset_callback_state(void) {
    g_activate_count = 0;
    g_last_menu = MENU_BAR_NO_MENU;
    g_last_item = MENU_BAR_NO_MENU;
    g_last_user_data = NULL;
}

static RenderStyle make_style(int fg, int bg, bool reverse, bool bold) {
    RenderStyle s = {
        .fg = (RenderColor)fg,
        .bg = (RenderColor)bg,
        .reverse = reverse,
        .bold = bold,
    };
    return s;
}

/* --- lifecycle / menu / items ---------------------------------------- */

static void test_create_destroy(void) {
    MenuBar *bar = menu_bar_create();
    assert(bar != NULL);
    assert(menu_bar_menu_count(bar) == 0);
    assert(!menu_bar_is_open(bar));
    assert(menu_bar_open_menu(bar) == MENU_BAR_NO_MENU);
    menu_bar_destroy(bar);

    /* NULL destroy must not crash. */
    menu_bar_destroy(NULL);
    printf("  PASS: create_destroy\n");
}

static void test_add_menus(void) {
    MenuBar *bar = menu_bar_create();
    bool ok = menu_bar_add_menu(bar, "File", 'F');
    assert(ok);
    ok = menu_bar_add_menu(bar, "Edit", 'E');
    assert(ok);
    ok = menu_bar_add_menu(bar, "View", 'V');
    assert(ok);
    ok = menu_bar_add_menu(bar, "Help", 'H');
    assert(ok);
    assert(menu_bar_menu_count(bar) == 4);
    assert(strcmp(menu_bar_menu_label(bar, 0), "File") == 0);
    assert(menu_bar_menu_shortcut(bar, 1) == 'E');
    /* NULL label rejected. */
    assert(!menu_bar_add_menu(bar, NULL, 0));
    menu_bar_destroy(bar);
    printf("  PASS: add_menus\n");
}

static void test_add_items(void) {
    MenuBar *bar = menu_bar_create();
    menu_bar_add_menu(bar, "File", 'F');
    bool ok = menu_bar_add_item(bar, 0, "Open...", 'O', on_activate, NULL);
    assert(ok);
    ok = menu_bar_add_item(bar, 0, "Save", 'S', on_activate, NULL);
    assert(ok);
    menu_bar_add_separator(bar, 0);
    ok = menu_bar_add_item(bar, 0, "Quit", 'Q', on_activate, NULL);
    assert(ok);
    assert(menu_bar_item_count(bar, 0) == 4);
    /* Invalid menu index. */
    assert(!menu_bar_add_item(bar, 5, "X", 'X', NULL, NULL));
    assert(!menu_bar_add_separator(bar, 5));
    menu_bar_destroy(bar);
    printf("  PASS: add_items\n");
}

/* --- open / close state ---------------------------------------------- */

static void test_open_close(void) {
    MenuBar *bar = menu_bar_create();
    menu_bar_add_menu(bar, "File", 'F');
    menu_bar_add_menu(bar, "Edit", 'E');
    menu_bar_add_item(bar, 0, "Open", 'O', on_activate, NULL);
    menu_bar_add_item(bar, 0, "Save", 'S', on_activate, NULL);

    menu_bar_open(bar, 1);
    assert(menu_bar_is_open(bar));
    assert(menu_bar_open_menu(bar) == 1);
    assert(menu_bar_open_item(bar) == 0);

    menu_bar_close(bar);
    assert(!menu_bar_is_open(bar));
    assert(menu_bar_open_menu(bar) == MENU_BAR_NO_MENU);

    /* Open with empty items picks no item. */
    menu_bar_add_menu(bar, "Empty", 'E');
    menu_bar_open(bar, 2);
    assert(menu_bar_is_open(bar));
    assert(menu_bar_open_item(bar) == MENU_BAR_NO_MENU);

    menu_bar_destroy(bar);
    printf("  PASS: open_close\n");
}

/* --- key handling ---------------------------------------------------- */

static void test_key_shortcut_opens(void) {
    MenuBar *bar = menu_bar_create();
    menu_bar_add_menu(bar, "File", 'F');
    menu_bar_add_menu(bar, "Edit", 'E');
    menu_bar_add_item(bar, 1, "Copy", 'C', on_activate, NULL);

    RetroKeyEvent f = {.key_code = 'F', .is_printable = true, .ascii = 'F'};
    bool consumed = menu_bar_handle_key(bar, &f);
    assert(consumed);
    assert(menu_bar_is_open(bar));
    assert(menu_bar_open_menu(bar) == 0);

    menu_bar_close(bar);
    RetroKeyEvent e = {.key_code = 'e', .is_printable = true, .ascii = 'e'};
    consumed = menu_bar_handle_key(bar, &e);
    assert(consumed);
    assert(menu_bar_open_menu(bar) == 1);

    menu_bar_destroy(bar);
    printf("  PASS: key_shortcut_opens\n");
}

static void test_key_shortcut_activates(void) {
    MenuBar *bar = menu_bar_create();
    menu_bar_add_menu(bar, "File", 'F');
    menu_bar_add_item(bar, 0, "Quit", 'Q', on_activate, NULL);
    menu_bar_open(bar, 0);

    reset_callback_state();
    RetroKeyEvent q = {.key_code = 'q', .is_printable = true, .ascii = 'q'};
    bool consumed = menu_bar_handle_key(bar, &q);
    assert(consumed);
    assert(g_activate_count == 1);
    assert(g_last_menu == 0);
    assert(g_last_item == 0);
    assert(!menu_bar_is_open(bar));

    menu_bar_destroy(bar);
    printf("  PASS: key_shortcut_activates\n");
}

static void test_key_escape_closes(void) {
    MenuBar *bar = menu_bar_create();
    menu_bar_add_menu(bar, "File", 'F');
    menu_bar_open(bar, 0);
    RetroKeyEvent esc = {.key_code = 27, .is_printable = false, .ascii = 0};
    bool consumed = menu_bar_handle_key(bar, &esc);
    assert(consumed);
    assert(!menu_bar_is_open(bar));
    menu_bar_destroy(bar);
    printf("  PASS: key_escape_closes\n");
}

static void test_key_f10_toggles(void) {
    MenuBar *bar = menu_bar_create();
    menu_bar_add_menu(bar, "File", 'F');
    RetroKeyEvent f10 = {.key_code = 271, .is_printable = false, .ascii = 0};
    bool consumed = menu_bar_handle_key(bar, &f10);
    assert(consumed);
    assert(menu_bar_is_open(bar));
    consumed = menu_bar_handle_key(bar, &f10);
    assert(consumed);
    assert(!menu_bar_is_open(bar));
    menu_bar_destroy(bar);
    printf("  PASS: key_f10_toggles\n");
}

static void test_key_arrow_navigation(void) {
    MenuBar *bar = menu_bar_create();
    menu_bar_add_menu(bar, "File", 'F');
    menu_bar_add_menu(bar, "Edit", 'E');
    menu_bar_add_menu(bar, "View", 'V');
    menu_bar_add_item(bar, 0, "Open", 'O', on_activate, NULL);
    menu_bar_add_item(bar, 0, "Save", 'S', on_activate, NULL);

    RetroKeyEvent f = {.key_code = 'F', .is_printable = true, .ascii = 'F'};
    menu_bar_handle_key(bar, &f);
    assert(menu_bar_open_menu(bar) == 0);

#ifdef KEY_RIGHT
    RetroKeyEvent right = {.key_code = KEY_RIGHT, .is_printable = false, .ascii = 0};
    menu_bar_handle_key(bar, &right);
    assert(menu_bar_open_menu(bar) == 1);
    menu_bar_handle_key(bar, &right);
    assert(menu_bar_open_menu(bar) == 2);
    menu_bar_handle_key(bar, &right);
    assert(menu_bar_open_menu(bar) == 0);  /* wrap */
    RetroKeyEvent left = {.key_code = KEY_LEFT, .is_printable = false, .ascii = 0};
    menu_bar_handle_key(bar, &left);
    assert(menu_bar_open_menu(bar) == 2);  /* wrap backward */
#endif

#ifdef KEY_DOWN
    RetroKeyEvent down = {.key_code = KEY_DOWN, .is_printable = false, .ascii = 0};
    menu_bar_handle_key(bar, &down);
    assert(menu_bar_open_item(bar) == 1);
    RetroKeyEvent down2 = {.key_code = KEY_DOWN, .is_printable = false, .ascii = 0};
    menu_bar_handle_key(bar, &down2);
    assert(menu_bar_open_item(bar) == 0);  /* wrap */
#endif

    menu_bar_destroy(bar);
    printf("  PASS: key_arrow_navigation\n");
}

static void test_key_enter_activates(void) {
    MenuBar *bar = menu_bar_create();
    menu_bar_add_menu(bar, "File", 'F');
    int sentinel = 42;
    menu_bar_add_item(bar, 0, "Open", 'O', on_activate, &sentinel);
    menu_bar_open(bar, 0);

    reset_callback_state();
    RetroKeyEvent enter = {.key_code = '\n', .is_printable = false, .ascii = 0};
    bool consumed = menu_bar_handle_key(bar, &enter);
    assert(consumed);
    assert(g_activate_count == 1);
    assert(g_last_user_data == &sentinel);
    assert(!menu_bar_is_open(bar));
    menu_bar_destroy(bar);
    printf("  PASS: key_enter_activates\n");
}

static void test_key_skip_separators(void) {
    MenuBar *bar = menu_bar_create();
    menu_bar_add_menu(bar, "File", 'F');
    menu_bar_add_item(bar, 0, "Open", 'O', NULL, NULL);
    menu_bar_add_separator(bar, 0);
    menu_bar_add_item(bar, 0, "Quit", 'Q', on_activate, NULL);
    menu_bar_open(bar, 0);
    assert(menu_bar_open_item(bar) == 0);

#ifdef KEY_DOWN
    RetroKeyEvent down = {.key_code = KEY_DOWN, .is_printable = false, .ascii = 0};
    menu_bar_handle_key(bar, &down);
    /* Should skip separator and land on "Quit" (index 2). */
    assert(menu_bar_open_item(bar) == 2);

    RetroKeyEvent up = {.key_code = KEY_UP, .is_printable = false, .ascii = 0};
    menu_bar_handle_key(bar, &up);
    /* Should skip separator going up and land on "Open" (index 0). */
    assert(menu_bar_open_item(bar) == 0);
#endif
    menu_bar_destroy(bar);
    printf("  PASS: key_skip_separators\n");
}

/* --- mouse handling -------------------------------------------------- */

static void test_mouse_click_menu_opens(void) {
    MenuBar *bar = menu_bar_create();
    menu_bar_add_menu(bar, "File", 'F');
    menu_bar_add_menu(bar, "Edit", 'E');
    int w = menu_bar_width(bar);

    RetroMouseEvent click = {
        .y = 0,
        .x = 2, /* somewhere over "File" (after leading gap) */
        .button1_clicked = true,
    };
    bool consumed = menu_bar_handle_mouse(bar, &click, 0, 0, w);
    assert(consumed);
    assert(menu_bar_is_open(bar));
    assert(menu_bar_open_menu(bar) == 0);

    /* Click same menu again closes it. */
    consumed = menu_bar_handle_mouse(bar, &click, 0, 0, w);
    assert(consumed);
    assert(!menu_bar_is_open(bar));

    menu_bar_destroy(bar);
    printf("  PASS: mouse_click_menu_opens\n");
}

static void test_mouse_outside(void) {
    MenuBar *bar = menu_bar_create();
    menu_bar_add_menu(bar, "File", 'F');
    RetroMouseEvent click = {
        .y = 5,
        .x = 10,
        .button1_clicked = true,
    };
    bool consumed = menu_bar_handle_mouse(bar, &click, 0, 0, 80);
    assert(!consumed);
    menu_bar_destroy(bar);
    printf("  PASS: mouse_outside\n");
}

/* --- rendering ------------------------------------------------------- */

static void test_render_draws_labels(void) {
    MenuBar *bar = menu_bar_create();
    menu_bar_add_menu(bar, "File", 'F');
    menu_bar_add_menu(bar, "Edit", 'E');
    DrawList *dl = draw_list_create();
    RenderStyle normal = make_style(RENDER_COLOR_WHITE, RENDER_COLOR_BLACK, false, false);
    RenderStyle sel = make_style(RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, true, false);
    menu_bar_render(bar, dl, 0, 0, 80, &normal, &sel, &sel, &sel);

    bool saw_file = false;
    bool saw_edit = false;
    for (size_t i = 0; i < draw_list_count(dl); i++) {
        DrawCommandView cmd;
        if (draw_list_get(dl, i, &cmd) && cmd.type == DRAW_COMMAND_TEXT &&
            cmd.text) {
            if (strcmp(cmd.text, "File") == 0) saw_file = true;
            if (strcmp(cmd.text, "Edit") == 0) saw_edit = true;
        }
    }
    assert(saw_file);
    assert(saw_edit);
    draw_list_destroy(dl);
    menu_bar_destroy(bar);
    printf("  PASS: render_draws_labels\n");
}

static void test_render_dropdown(void) {
    MenuBar *bar = menu_bar_create();
    menu_bar_add_menu(bar, "File", 'F');
    menu_bar_add_item(bar, 0, "Open...", 'O', NULL, NULL);
    menu_bar_add_separator(bar, 0);
    menu_bar_add_item(bar, 0, "Quit", 'Q', NULL, NULL);
    menu_bar_open(bar, 0);

    DrawList *dl = draw_list_create();
    RenderStyle item_normal = make_style(RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false);
    RenderStyle item_sel = make_style(RENDER_COLOR_WHITE, RENDER_COLOR_BLACK, true, true);
    RenderStyle sep = make_style(RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false);
    RenderStyle sc = make_style(RENDER_COLOR_RED, RENDER_COLOR_WHITE, false, true);
    RenderStyle frame = make_style(RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false);

    int rows = menu_bar_render_dropdown(bar, dl, 1, 0,
                                         &item_normal, &item_sel,
                                         &sep, &sc, &frame);
    assert(rows > 0);

    bool saw_open = false;
    bool saw_quit = false;
    bool saw_dashes = false;
    for (size_t i = 0; i < draw_list_count(dl); i++) {
        DrawCommandView cmd;
        if (draw_list_get(dl, i, &cmd) && cmd.type == DRAW_COMMAND_TEXT &&
            cmd.text) {
            if (strcmp(cmd.text, "Open...") == 0) saw_open = true;
            if (strcmp(cmd.text, "Quit") == 0) saw_quit = true;
            if (strchr(cmd.text, '-') && strlen(cmd.text) > 2) saw_dashes = true;
        }
    }
    assert(saw_open);
    assert(saw_quit);
    assert(saw_dashes);

    draw_list_destroy(dl);
    menu_bar_destroy(bar);
    printf("  PASS: render_dropdown\n");
}

/* --- sizing --------------------------------------------------------- */

static void test_sizing(void) {
    MenuBar *bar = menu_bar_create();
    menu_bar_add_menu(bar, "File", 'F');
    menu_bar_add_item(bar, 0, "Open...", 'O', NULL, NULL);
    menu_bar_add_item(bar, 0, "Save", 'S', NULL, NULL);
    int w = menu_bar_width(bar);
    assert(w >= 4 + 4 + 2 * 2); /* "File" + gaps */
    int dw = menu_bar_dropdown_width(bar, 0);
    assert(dw > 6); /* at least "Open..." + padding */
    int dh = menu_bar_dropdown_height(bar, 0);
    assert(dh >= 4); /* top + 2 items + bottom */
    menu_bar_destroy(bar);
    printf("  PASS: sizing\n");
}

/* --- null safety ----------------------------------------------------- */

static void test_null_safety(void) {
    menu_bar_destroy(NULL);
    assert(menu_bar_menu_count(NULL) == 0);
    assert(strcmp(menu_bar_menu_label(NULL, 0), "") == 0);
    assert(menu_bar_menu_shortcut(NULL, 0) == 0);
    assert(menu_bar_item_count(NULL, 0) == 0);
    assert(menu_bar_open_menu(NULL) == MENU_BAR_NO_MENU);
    assert(menu_bar_open_item(NULL) == MENU_BAR_NO_MENU);
    assert(!menu_bar_is_open(NULL));
    menu_bar_open(NULL, 0);
    menu_bar_close(NULL);
    RetroKeyEvent k = {.key_code = 'a', .is_printable = true, .ascii = 'a'};
    assert(!menu_bar_handle_key(NULL, &k));
    RetroMouseEvent m = {.button1_clicked = true};
    assert(!menu_bar_handle_mouse(NULL, &m, 0, 0, 80));
    DrawList *dl = draw_list_create();
    RenderStyle s = make_style(0, 0, false, false);
    menu_bar_render(NULL, dl, 0, 0, 80, &s, &s, &s, &s);
    assert(menu_bar_render_dropdown(NULL, dl, 0, 0, &s, &s, &s, &s, &s) == 0);
    assert(menu_bar_width(NULL) == 0);
    assert(menu_bar_dropdown_width(NULL, 0) == 0);
    assert(menu_bar_dropdown_height(NULL, 0) == 0);
    draw_list_destroy(dl);
    printf("  PASS: null_safety\n");
}

int main(void) {
    printf("menu_bar_test:\n");
    test_create_destroy();
    test_add_menus();
    test_add_items();
    test_open_close();
    test_key_shortcut_opens();
    test_key_shortcut_activates();
    test_key_escape_closes();
    test_key_f10_toggles();
    test_key_arrow_navigation();
    test_key_enter_activates();
    test_key_skip_separators();
    test_mouse_click_menu_opens();
    test_mouse_outside();
    test_render_draws_labels();
    test_render_dropdown();
    test_sizing();
    test_null_safety();
    printf("All menu_bar tests passed.\n");
    return 0;
}