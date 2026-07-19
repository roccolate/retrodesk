#include "test_harness.h"
#include <stdio.h>
#include <string.h>

#include "core/key_chord.h"
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
    TEST_REQUIRE(tab != NULL);
    TEST_REQUIRE(tab_count(tab) == 0);
    TEST_REQUIRE(tab_active(tab) == (size_t)-1);
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
    TEST_REQUIRE(i0 == 0);
    TEST_REQUIRE(i1 == 1);
    TEST_REQUIRE(i2 == 2);
    TEST_REQUIRE(tab_count(tab) == 3);
    TEST_REQUIRE(strcmp(tab_label(tab, 0), "Info") == 0);
    TEST_REQUIRE(strcmp(tab_label(tab, 1), "Logs") == 0);
    TEST_REQUIRE(strcmp(tab_label(tab, 2), "Stats") == 0);
    /* Out of bounds. */
    TEST_REQUIRE(strcmp(tab_label(tab, 99), "") == 0);
    /* NULL label rejected. */
    TEST_REQUIRE(tab_add(tab, NULL) == (size_t)-1);
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
    TEST_REQUIRE(tab_active(tab) == 0);

    reset_callback_state();
    tab_set_active(tab, 1);
    TEST_REQUIRE(tab_active(tab) == 1);
    TEST_REQUIRE(g_change_count == 1);
    TEST_REQUIRE(g_old_index == 0);
    TEST_REQUIRE(g_new_index == 1);
    TEST_REQUIRE(g_user_data == &sentinel);

    /* Setting the same active index does not fire. */
    reset_callback_state();
    tab_set_active(tab, 1);
    TEST_REQUIRE(g_change_count == 0);

    /* Out-of-range clamps to last. */
    tab_set_active(tab, 99);
    TEST_REQUIRE(tab_active(tab) == 1);

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

    RetroKeyEvent r = {.key_code = RETRO_KEY_RIGHT, .is_printable = false, .ascii = 0};
    tab_handle_key(tab, &r);
    TEST_REQUIRE(tab_active(tab) == 1);
    tab_handle_key(tab, &r);
    TEST_REQUIRE(tab_active(tab) == 2);
    tab_handle_key(tab, &r); /* wraps */
    TEST_REQUIRE(tab_active(tab) == 0);
    tab_destroy(tab);
    printf("  PASS: key_right_cycles\n");
}

static void test_key_left_wraps(void) {
    Tab *tab = tab_create();
    tab_add(tab, "A");
    tab_add(tab, "B");
    tab_add(tab, "C");
    tab_set_change_callback(tab, on_change, NULL);

    RetroKeyEvent l = {.key_code = RETRO_KEY_LEFT, .is_printable = false, .ascii = 0};
    tab_handle_key(tab, &l); /* 0 -> 2 (wrap) */
    TEST_REQUIRE(tab_active(tab) == 2);
    tab_handle_key(tab, &l); /* 2 -> 1 */
    TEST_REQUIRE(tab_active(tab) == 1);
    tab_destroy(tab);
    printf("  PASS: key_left_wraps\n");
}

static void test_key_home_end(void) {
    Tab *tab = tab_create();
    tab_add(tab, "A");
    tab_add(tab, "B");
    tab_add(tab, "C");
    tab_set_active(tab, 1);

    RetroKeyEvent e = {.key_code = RETRO_KEY_END, .is_printable = false, .ascii = 0};
    tab_handle_key(tab, &e);
    TEST_REQUIRE(tab_active(tab) == 2);
    RetroKeyEvent h = {.key_code = RETRO_KEY_HOME, .is_printable = false, .ascii = 0};
    tab_handle_key(tab, &h);
    TEST_REQUIRE(tab_active(tab) == 0);
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
    TEST_REQUIRE(consumed);
    TEST_REQUIRE(tab_active(tab) == 1);

    /* Out-of-range number key does not consume. */
    reset_callback_state();
    RetroKeyEvent nine = {.key_code = '9', .is_printable = true, .ascii = '9'};
    consumed = tab_handle_key(tab, &nine);
    TEST_REQUIRE(!consumed);
    TEST_REQUIRE(g_change_count == 0);

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
    TEST_REQUIRE(consumed);
    TEST_REQUIRE(tab_active(tab) == 1);

    /* Click on a row other than the tab row is ignored. */
    RetroMouseEvent miss = {
        .y = 5, .x = 6,
        .button1_clicked = true,
    };
    consumed = tab_handle_mouse(tab, &miss, 0, 0, 12);
    TEST_REQUIRE(!consumed);

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
    TEST_REQUIRE(!consumed);
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
    TEST_REQUIRE(used == 15);

    bool saw_info = false, saw_logs = false;
    for (size_t i = 0; i < draw_list_count(dl); i++) {
        DrawCommandView cmd;
        if (draw_list_get(dl, i, &cmd) && cmd.type == DRAW_COMMAND_TEXT &&
            cmd.text) {
            if (strstr(cmd.text, "Info")) saw_info = true;
            if (strstr(cmd.text, "Logs")) saw_logs = true;
            /* Active tab uses active_style (reverse + bold). */
            if (cmd.x == 0) {
                TEST_REQUIRE(cmd.style.reverse == true);
            }
        }
    }
    TEST_REQUIRE(saw_info);
    TEST_REQUIRE(saw_logs);

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
    TEST_REQUIRE(used == 8);
    draw_list_destroy(dl);
    tab_destroy(tab);
    printf("  PASS: render_truncates_to_max_width\n");
}

static void test_render_boundary_widths(void) {
    const int widths[] = {127, 128, 129};
    char label[130];
    memset(label, 'A', sizeof(label));

    for (size_t i = 0; i < sizeof(widths) / sizeof(widths[0]); ++i) {
        int width = widths[i];
        label[width] = '\0';

        Tab *tab = tab_create();
        DrawList *dl = draw_list_create();
        RenderStyle style = make_style(0, 0, false, false);
        TEST_REQUIRE(tab != NULL);
        TEST_REQUIRE(dl != NULL);
        TEST_REQUIRE(tab_add(tab, label) == 0);
        TEST_REQUIRE(tab_render(tab, dl, 0, 0, width, &style, &style) == width);

        size_t rendered = 0;
        for (size_t command_index = 0;
             command_index < draw_list_count(dl);
             ++command_index) {
            DrawCommandView command = {0};
            TEST_REQUIRE(draw_list_get(dl, command_index, &command));
            TEST_REQUIRE(command.type == DRAW_COMMAND_TEXT);
            TEST_REQUIRE(command.text != NULL);
            rendered += strlen(command.text);
        }
        TEST_REQUIRE(rendered == (size_t)width);

        draw_list_destroy(dl);
        tab_destroy(tab);
        memset(label, 'A', sizeof(label));
    }
    printf("  PASS: render_boundary_widths\n");
}

/* --- sizing --------------------------------------------------------- */

static void test_sizing(void) {
    Tab *tab = tab_create();
    tab_add(tab, "A");
    tab_add(tab, "BB");
    tab_add(tab, "CCC");
    /* "[ A] [ BB] [ CCC]" = 4 + 5 + 6 + 2 gaps = 17 */
    TEST_REQUIRE(tab_width(tab) == 17);
    TEST_REQUIRE(tab_height() == 1);
    tab_destroy(tab);
    printf("  PASS: sizing\n");
}

/* --- null safety --------------------------------------------------- */

static void test_null_safety(void) {
    tab_destroy(NULL);
    TEST_REQUIRE(tab_count(NULL) == 0);
    TEST_REQUIRE(strcmp(tab_label(NULL, 0), "") == 0);
    TEST_REQUIRE(tab_active(NULL) == (size_t)-1);
    tab_set_active(NULL, 0);
    tab_set_change_callback(NULL, NULL, NULL);
    RetroKeyEvent k = {.key_code = '1', .is_printable = true, .ascii = '1'};
    TEST_REQUIRE(!tab_handle_key(NULL, &k));
    RetroMouseEvent m = {.button1_clicked = true};
    TEST_REQUIRE(!tab_handle_mouse(NULL, &m, 0, 0, 80));
    DrawList *dl = draw_list_create();
    RenderStyle s = make_style(0, 0, false, false);
    TEST_REQUIRE(tab_render(NULL, dl, 0, 0, 80, &s, &s) == 0);
    TEST_REQUIRE(tab_width(NULL) == 0);
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
    test_render_boundary_widths();
    test_sizing();
    test_null_safety();
    printf("All tab tests passed.\n");
    return 0;
}
