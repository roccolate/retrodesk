#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ui/tab.h"

static RenderStyle make_style(int fg, int bg, bool reverse, bool bold) {
    RenderStyle s = {
        .fg = (RenderColor)fg,
        .bg = (RenderColor)bg,
        .reverse = reverse,
        .bold = bold,
    };
    return s;
}

/* --- callback tracking ----------------------------------------------- */

static int g_change_count;
static size_t g_old_index;
static size_t g_new_index;
static void *g_user_data;

static void on_change(size_t old_idx, size_t new_idx, void *ud) {
    g_change_count++;
    g_old_index = old_idx;
    g_new_index = new_idx;
    g_user_data = ud;
}

static void reset_callback_state(void) {
    g_change_count = 0;
    g_old_index = (size_t)-1;
    g_new_index = (size_t)-1;
    g_user_data = NULL;
}

/* --- lifecycle ------------------------------------------------------- */

static void test_create_destroy(void) {
    Tab *tab = tab_create();
    assert(tab != NULL);
    assert(tab_count(tab) == 0);
    assert(tab_active(tab) == (size_t)-1);
    tab_destroy(tab);
    tab_destroy(NULL);
    printf("  PASS: create_destroy\n");
}

/* --- add / count / label -------------------------------------------- */

static void test_add_tabs(void) {
    Tab *tab = tab_create();
    size_t i0 = tab_add(tab, "Info");
    size_t i1 = tab_add(tab, "Logs");
    size_t i2 = tab_add(tab, "Stats");
    assert(i0 == 0);
    assert(i1 == 1);
    assert(i2 == 2);
    assert(tab_count(tab) == 3);
    assert(strcmp(tab_label(tab, 0), "Info") == 0);
    assert(strcmp(tab_label(tab, 1), "Logs") == 0);
    assert(strcmp(tab_label(tab, 2), "Stats") == 0);
    /* Out of bounds. */
    assert(strcmp(tab_label(tab, 99), "") == 0);
    /* NULL label rejected. */
    assert(tab_add(tab, NULL) == (size_t)-1);
    tab_destroy(tab);
    printf("  PASS: add_tabs\n");
}

/* --- active + callback --------------------------------------------- */

static void test_active_and_callback(void) {
    Tab *tab = tab_create();
    int sentinel = 7;
    tab_set_change_callback(tab, on_change, &sentinel);
    tab_add(tab, "A");
    tab_add(tab, "B");

    /* Default active is 0. */
    assert(tab_active(tab) == 0);

    reset_callback_state();
    tab_set_active(tab, 1);
    assert(tab_active(tab) == 1);
    assert(g_change_count == 1);
    assert(g_old_index == 0);
    assert(g_new_index == 1);
    assert(g_user_data == &sentinel);

    /* Setting the same active index does not fire. */
    reset_callback_state();
    tab_set_active(tab, 1);
    assert(g_change_count == 0);

    /* Out-of-range clamps to last. */
    tab_set_active(tab, 99);
    assert(tab_active(tab) == 1);

    tab_destroy(tab);
    printf("  PASS: active_and_callback\n");
}

/* --- key handling --------------------------------------------------- */

static void test_key_right_cycles(void) {
    Tab *tab = tab_create();
    tab_add(tab, "A");
    tab_add(tab, "B");
    tab_add(tab, "C");
    tab_set_change_callback(tab, on_change, NULL);

#ifdef KEY_RIGHT
    RetroKeyEvent r = {.key_code = KEY_RIGHT, .is_printable = false, .ascii = 0};
    tab_handle_key(tab, &r);
    assert(tab_active(tab) == 1);
    tab_handle_key(tab, &r);
    assert(tab_active(tab) == 2);
    tab_handle_key(tab, &r); /* wraps */
    assert(tab_active(tab) == 0);
#endif
    tab_destroy(tab);
    printf("  PASS: key_right_cycles\n");
}

static void test_key_left_wraps(void) {
    Tab *tab = tab_create();
    tab_add(tab, "A");
    tab_add(tab, "B");
    tab_add(tab, "C");
    tab_set_change_callback(tab, on_change, NULL);

#ifdef KEY_LEFT
    RetroKeyEvent l = {.key_code = KEY_LEFT, .is_printable = false, .ascii = 0};
    tab_handle_key(tab, &l); /* 0 -> 2 (wrap) */
    assert(tab_active(tab) == 2);
    tab_handle_key(tab, &l); /* 2 -> 1 */
    assert(tab_active(tab) == 1);
#endif
    tab_destroy(tab);
    printf("  PASS: key_left_wraps\n");
}

static void test_key_home_end(void) {
    Tab *tab = tab_create();
    tab_add(tab, "A");
    tab_add(tab, "B");
    tab_add(tab, "C");
    tab_set_active(tab, 1);

#ifdef KEY_END
    RetroKeyEvent e = {.key_code = KEY_END, .is_printable = false, .ascii = 0};
    tab_handle_key(tab, &e);
    assert(tab_active(tab) == 2);
#endif
#ifdef KEY_HOME
    RetroKeyEvent h = {.key_code = KEY_HOME, .is_printable = false, .ascii = 0};
    tab_handle_key(tab, &h);
    assert(tab_active(tab) == 0);
#endif
    tab_destroy(tab);
    printf("  PASS: key_home_end\n");
}

static void test_key_number_jumps(void) {
    Tab *tab = tab_create();
    tab_add(tab, "A");
    tab_add(tab, "B");
    tab_add(tab, "C");
    tab_set_change_callback(tab, on_change, NULL);

    RetroKeyEvent two = {.key_code = '2', .is_printable = true, .ascii = '2'};
    bool consumed = tab_handle_key(tab, &two);
    assert(consumed);
    assert(tab_active(tab) == 1);

    /* Out-of-range number key does not consume. */
    reset_callback_state();
    RetroKeyEvent nine = {.key_code = '9', .is_printable = true, .ascii = '9'};
    consumed = tab_handle_key(tab, &nine);
    assert(!consumed);
    assert(g_change_count == 0);

    tab_destroy(tab);
    printf("  PASS: key_number_jumps\n");
}

/* --- mouse handling ------------------------------------------------- */

static void test_mouse_click_selects(void) {
    Tab *tab = tab_create();
    tab_add(tab, "A");
    tab_add(tab, "B");
    tab_add(tab, "C");
    tab_set_change_callback(tab, on_change, NULL);

    /* "[ A] [ B] [ C]" — A spans columns 0..3, B 5..7, C 9..11 */
    RetroMouseEvent click = {
        .y = 0,
        .x = 6, /* inside "[ B]" */
        .button1_clicked = true,
    };
    bool consumed = tab_handle_mouse(tab, &click, 0, 0, 12);
    assert(consumed);
    assert(tab_active(tab) == 1);

    /* Click on a row other than the tab row is ignored. */
    RetroMouseEvent miss = {
        .y = 5, .x = 6,
        .button1_clicked = true,
    };
    consumed = tab_handle_mouse(tab, &miss, 0, 0, 12);
    assert(!consumed);

    tab_destroy(tab);
    printf("  PASS: mouse_click_selects\n");
}

static void test_mouse_no_click_ignored(void) {
    Tab *tab = tab_create();
    tab_add(tab, "A");
    RetroMouseEvent move = {
        .y = 0, .x = 1,
        .moved = true,
        .button1_clicked = false,
    };
    bool consumed = tab_handle_mouse(tab, &move, 0, 0, 80);
    assert(!consumed);
    tab_destroy(tab);
    printf("  PASS: mouse_no_click_ignored\n");
}

/* --- rendering ------------------------------------------------------ */

static void test_render_outputs_segments(void) {
    Tab *tab = tab_create();
    tab_add(tab, "Info");
    tab_add(tab, "Logs");
    DrawList *dl = draw_list_create();
    RenderStyle normal = make_style(RENDER_COLOR_WHITE, RENDER_COLOR_BLACK, false, false);
    RenderStyle active = make_style(RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, true, true);

    int used = tab_render(tab, dl, 0, 0, 80, &normal, &active);
    /* "[ Info] [Logs]" = 7 + 1 + 7 = 15. The format is "[ label]" with a
       leading space inside the brackets (same for active and inactive). */
    assert(used == 15);

    bool saw_info = false, saw_logs = false;
    for (size_t i = 0; i < draw_list_count(dl); i++) {
        DrawCommandView cmd;
        if (draw_list_get(dl, i, &cmd) && cmd.type == DRAW_COMMAND_TEXT &&
            cmd.text) {
            if (strstr(cmd.text, "Info")) saw_info = true;
            if (strstr(cmd.text, "Logs")) saw_logs = true;
            /* Active tab uses active_style (reverse + bold). */
            if (cmd.x == 0) {
                assert(cmd.style.reverse == true);
            }
        }
    }
    assert(saw_info);
    assert(saw_logs);

    draw_list_destroy(dl);
    tab_destroy(tab);
    printf("  PASS: render_outputs_segments\n");
}

static void test_render_truncates_to_max_width(void) {
    Tab *tab = tab_create();
    tab_add(tab, "A very long tab");
    tab_add(tab, "Another");
    DrawList *dl = draw_list_create();
    RenderStyle normal = make_style(0, 0, false, false);
    RenderStyle active = make_style(0, 0, false, false);
    int used = tab_render(tab, dl, 0, 0, 8, &normal, &active);
    assert(used == 8);
    draw_list_destroy(dl);
    tab_destroy(tab);
    printf("  PASS: render_truncates_to_max_width\n");
}

/* --- sizing --------------------------------------------------------- */

static void test_sizing(void) {
    Tab *tab = tab_create();
    tab_add(tab, "A");
    tab_add(tab, "BB");
    tab_add(tab, "CCC");
    /* "[ A] [ BB] [ CCC]" = 4 + 5 + 6 + 2 gaps = 17 */
    assert(tab_width(tab) == 17);
    assert(tab_height() == 1);
    tab_destroy(tab);
    printf("  PASS: sizing\n");
}

/* --- null safety --------------------------------------------------- */

static void test_null_safety(void) {
    tab_destroy(NULL);
    assert(tab_count(NULL) == 0);
    assert(strcmp(tab_label(NULL, 0), "") == 0);
    assert(tab_active(NULL) == (size_t)-1);
    tab_set_active(NULL, 0);
    tab_set_change_callback(NULL, NULL, NULL);
    RetroKeyEvent k = {.key_code = '1', .is_printable = true, .ascii = '1'};
    assert(!tab_handle_key(NULL, &k));
    RetroMouseEvent m = {.button1_clicked = true};
    assert(!tab_handle_mouse(NULL, &m, 0, 0, 80));
    DrawList *dl = draw_list_create();
    RenderStyle s = make_style(0, 0, false, false);
    assert(tab_render(NULL, dl, 0, 0, 80, &s, &s) == 0);
    assert(tab_width(NULL) == 0);
    draw_list_destroy(dl);
    printf("  PASS: null_safety\n");
}

int main(void) {
    printf("tab_test:\n");
    test_create_destroy();
    test_add_tabs();
    test_active_and_callback();
    test_key_right_cycles();
    test_key_left_wraps();
    test_key_home_end();
    test_key_number_jumps();
    test_mouse_click_selects();
    test_mouse_no_click_ignored();
    test_render_outputs_segments();
    test_render_truncates_to_max_width();
    test_sizing();
    test_null_safety();
    printf("All tab tests passed.\n");
    return 0;
}