#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ui/scroll_list.h"

static void test_create_destroy(void) {
    ScrollList *sl = scroll_list_create();
    assert(sl != NULL);
    assert(scroll_list_count(sl) == 0);
    assert(scroll_list_selected(sl) == 0);
    assert(scroll_list_scroll_offset(sl) == 0);
    assert(scroll_list_item(sl, 0) == NULL);
    scroll_list_destroy(sl);
    printf("  PASS: create_destroy\n");
}

static void test_set_items(void) {
    ScrollList *sl = scroll_list_create();
    const char *items[] = {"Alpha", "Beta", "Gamma", "Delta"};
    bool ok = scroll_list_set_items(sl, items, 4);
    assert(ok);
    assert(scroll_list_count(sl) == 4);
    assert(strcmp(scroll_list_item(sl, 0), "Alpha") == 0);
    assert(strcmp(scroll_list_item(sl, 1), "Beta") == 0);
    assert(strcmp(scroll_list_item(sl, 2), "Gamma") == 0);
    assert(strcmp(scroll_list_item(sl, 3), "Delta") == 0);
    assert(scroll_list_item(sl, 4) == NULL);
    assert(scroll_list_selected(sl) == 0);

    /* Replace items resets selection. */
    const char *items2[] = {"One", "Two"};
    ok = scroll_list_set_items(sl, items2, 2);
    assert(ok);
    assert(scroll_list_count(sl) == 2);
    assert(scroll_list_selected(sl) == 0);

    scroll_list_destroy(sl);
    printf("  PASS: set_items\n");
}

static void test_append(void) {
    ScrollList *sl = scroll_list_create();
    assert(scroll_list_append(sl, "first"));
    assert(scroll_list_append(sl, "second"));
    assert(scroll_list_append(sl, "third"));
    assert(scroll_list_count(sl) == 3);
    assert(strcmp(scroll_list_item(sl, 0), "first") == 0);
    assert(strcmp(scroll_list_item(sl, 2), "third") == 0);
    scroll_list_destroy(sl);
    printf("  PASS: append\n");
}

static void test_clear(void) {
    ScrollList *sl = scroll_list_create();
    const char *items[] = {"A", "B", "C"};
    scroll_list_set_items(sl, items, 3);
    scroll_list_select(sl, 2);
    scroll_list_clear(sl);
    assert(scroll_list_count(sl) == 0);
    assert(scroll_list_selected(sl) == 0);
    assert(scroll_list_scroll_offset(sl) == 0);
    scroll_list_destroy(sl);
    printf("  PASS: clear\n");
}

static void test_select(void) {
    ScrollList *sl = scroll_list_create();
    const char *items[] = {"A", "B", "C", "D"};
    scroll_list_set_items(sl, items, 4);

    scroll_list_select(sl, 2);
    assert(scroll_list_selected(sl) == 2);

    /* Clamp to last item. */
    scroll_list_select(sl, 100);
    assert(scroll_list_selected(sl) == 3);

    /* Select on empty list — no crash. */
    scroll_list_clear(sl);
    scroll_list_select(sl, 0);
    assert(scroll_list_selected(sl) == 0);

    scroll_list_destroy(sl);
    printf("  PASS: select\n");
}

static void test_key_navigation(void) {
    ScrollList *sl = scroll_list_create();
    const char *items[] = {"A", "B", "C", "D", "E"};
    scroll_list_set_items(sl, items, 5);

    /* Ctrl+N — down */
    RetroKeyEvent ctrl_n = {.key_code = 14, .is_printable = false, .ascii = '\0'};
    bool consumed = scroll_list_handle_key(sl, &ctrl_n);
    assert(consumed);
    assert(scroll_list_selected(sl) == 1);

    scroll_list_handle_key(sl, &ctrl_n);
    scroll_list_handle_key(sl, &ctrl_n);
    assert(scroll_list_selected(sl) == 3);

    /* Ctrl+P — up */
    RetroKeyEvent ctrl_p = {.key_code = 16, .is_printable = false, .ascii = '\0'};
    scroll_list_handle_key(sl, &ctrl_p);
    assert(scroll_list_selected(sl) == 2);

    /* Ctrl+P at top — stays at 0 */
    scroll_list_select(sl, 0);
    consumed = scroll_list_handle_key(sl, &ctrl_p);
    assert(!consumed || scroll_list_selected(sl) == 0);

    /* Ctrl+N at bottom — stays at last */
    scroll_list_select(sl, 4);
    consumed = scroll_list_handle_key(sl, &ctrl_n);
    assert(!consumed || scroll_list_selected(sl) == 4);

    scroll_list_destroy(sl);
    printf("  PASS: key_navigation\n");
}

#ifdef KEY_UP
static void test_arrow_keys(void) {
    ScrollList *sl = scroll_list_create();
    const char *items[] = {"A", "B", "C", "D", "E"};
    scroll_list_set_items(sl, items, 5);

    RetroKeyEvent down = {.key_code = KEY_DOWN, .is_printable = false, .ascii = '\0'};
    RetroKeyEvent up = {.key_code = KEY_UP, .is_printable = false, .ascii = '\0'};
    RetroKeyEvent home = {.key_code = KEY_HOME, .is_printable = false, .ascii = '\0'};
    RetroKeyEvent end = {.key_code = KEY_END, .is_printable = false, .ascii = '\0'};

    scroll_list_handle_key(sl, &down);
    scroll_list_handle_key(sl, &down);
    assert(scroll_list_selected(sl) == 2);

    scroll_list_handle_key(sl, &up);
    assert(scroll_list_selected(sl) == 1);

    scroll_list_handle_key(sl, &end);
    assert(scroll_list_selected(sl) == 4);

    scroll_list_handle_key(sl, &home);
    assert(scroll_list_selected(sl) == 0);

    scroll_list_destroy(sl);
    printf("  PASS: arrow_keys\n");
}

static void test_page_keys(void) {
    ScrollList *sl = scroll_list_create();
    /* Create 25 items. */
    for (int i = 0; i < 25; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "Item %02d", i);
        scroll_list_append(sl, buf);
    }

    RetroKeyEvent pgdn = {.key_code = KEY_NPAGE, .is_printable = false, .ascii = '\0'};
    RetroKeyEvent pgup = {.key_code = KEY_PPAGE, .is_printable = false, .ascii = '\0'};

    scroll_list_handle_key(sl, &pgdn);
    assert(scroll_list_selected(sl) == 10);

    scroll_list_handle_key(sl, &pgdn);
    assert(scroll_list_selected(sl) == 20);

    /* PgDn clamps to last. */
    scroll_list_handle_key(sl, &pgdn);
    assert(scroll_list_selected(sl) == 24);

    scroll_list_handle_key(sl, &pgup);
    assert(scroll_list_selected(sl) == 14);

    /* PgUp clamps to 0. */
    scroll_list_handle_key(sl, &pgup);
    assert(scroll_list_selected(sl) == 4);
    scroll_list_handle_key(sl, &pgup);
    assert(scroll_list_selected(sl) == 0);

    scroll_list_destroy(sl);
    printf("  PASS: page_keys\n");
}
#endif

static void test_render_basic(void) {
    ScrollList *sl = scroll_list_create();
    const char *items[] = {"Alpha", "Beta", "Gamma"};
    scroll_list_set_items(sl, items, 3);

    DrawList *dl = draw_list_create();
    RenderStyle normal = {.fg = RENDER_COLOR_WHITE, .bg = RENDER_COLOR_BLACK,
                          .reverse = false, .bold = false};
    RenderStyle selected = {.fg = RENDER_COLOR_BLACK, .bg = RENDER_COLOR_WHITE,
                            .reverse = true, .bold = false};

    scroll_list_render(sl, dl, 0, 0, 3, 10, &normal, &selected);

    /* Should produce 3 draw commands (one per visible row). */
    assert(draw_list_count(dl) == 3);

    /* First item is selected — should use selected style (reverse). */
    DrawCommandView cmd;
    bool ok = draw_list_get(dl, 0, &cmd);
    assert(ok);
    assert(cmd.type == DRAW_COMMAND_TEXT);
    assert(cmd.style.reverse == true);

    /* Second item — normal style. */
    ok = draw_list_get(dl, 1, &cmd);
    assert(ok);
    assert(cmd.style.reverse == false);

    draw_list_destroy(dl);
    scroll_list_destroy(sl);
    printf("  PASS: render_basic\n");
}

static void test_render_scroll(void) {
    ScrollList *sl = scroll_list_create();
    for (int i = 0; i < 20; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "Item %02d", i);
        scroll_list_append(sl, buf);
    }

    /* Select item 15, render with visible_height=5 */
    scroll_list_select(sl, 15);

    DrawList *dl = draw_list_create();
    RenderStyle normal = {.fg = RENDER_COLOR_WHITE, .bg = RENDER_COLOR_BLACK,
                          .reverse = false, .bold = false};
    RenderStyle selected = {.fg = RENDER_COLOR_BLACK, .bg = RENDER_COLOR_WHITE,
                            .reverse = true, .bold = false};

    scroll_list_render(sl, dl, 0, 0, 5, 10, &normal, &selected);

    /* Should produce exactly 5 draw commands. */
    assert(draw_list_count(dl) == 5);

    /* The selected item (15) should appear somewhere in the 5 rows. */
    bool found_selected = false;
    for (size_t i = 0; i < 5; i++) {
        DrawCommandView cmd;
        draw_list_get(dl, i, &cmd);
        if (cmd.style.reverse) {
            found_selected = true;
        }
    }
    assert(found_selected);

    draw_list_destroy(dl);
    scroll_list_destroy(sl);
    printf("  PASS: render_scroll\n");
}

static void test_mouse_click(void) {
    ScrollList *sl = scroll_list_create();
    const char *items[] = {"A", "B", "C", "D", "E"};
    scroll_list_set_items(sl, items, 5);

    /* Click on row 2 (widget starts at y=5, x=3) */
    RetroMouseEvent click = {
        .y = 7, .x = 5,
        .button1_clicked = true,
    };
    bool consumed = scroll_list_handle_mouse(sl, &click, 5, 3, 5, 10);
    assert(consumed);
    assert(scroll_list_selected(sl) == 2);

    /* Click outside — not consumed. */
    RetroMouseEvent outside = {
        .y = 20, .x = 5,
        .button1_clicked = true,
    };
    consumed = scroll_list_handle_mouse(sl, &outside, 5, 3, 5, 10);
    assert(!consumed);

    scroll_list_destroy(sl);
    printf("  PASS: mouse_click\n");
}

static void test_mouse_scroll(void) {
    ScrollList *sl = scroll_list_create();
    const char *items[] = {"A", "B", "C", "D", "E"};
    scroll_list_set_items(sl, items, 5);

    RetroMouseEvent scroll_down = {.scroll_down = true};
    scroll_list_handle_mouse(sl, &scroll_down, 0, 0, 3, 10);
    assert(scroll_list_selected(sl) == 1);

    RetroMouseEvent scroll_up = {.scroll_up = true};
    scroll_list_handle_mouse(sl, &scroll_up, 0, 0, 3, 10);
    assert(scroll_list_selected(sl) == 0);

    /* Scroll up at top — stays. */
    scroll_list_handle_mouse(sl, &scroll_up, 0, 0, 3, 10);
    assert(scroll_list_selected(sl) == 0);

    scroll_list_destroy(sl);
    printf("  PASS: mouse_scroll\n");
}

static void test_null_safety(void) {
    assert(scroll_list_count(NULL) == 0);
    assert(scroll_list_selected(NULL) == 0);
    assert(scroll_list_scroll_offset(NULL) == 0);
    assert(scroll_list_item(NULL, 0) == NULL);
    assert(!scroll_list_set_items(NULL, NULL, 0));
    assert(!scroll_list_append(NULL, "x"));
    assert(!scroll_list_handle_key(NULL, NULL));
    assert(!scroll_list_handle_mouse(NULL, NULL, 0, 0, 0, 0));
    scroll_list_clear(NULL);
    scroll_list_select(NULL, 0);
    scroll_list_destroy(NULL);
    scroll_list_render(NULL, NULL, 0, 0, 5, 10, NULL, NULL);
    printf("  PASS: null_safety\n");
}

static void test_empty_list_key(void) {
    ScrollList *sl = scroll_list_create();
    RetroKeyEvent ctrl_n = {.key_code = 14, .is_printable = false, .ascii = '\0'};
    bool consumed = scroll_list_handle_key(sl, &ctrl_n);
    assert(!consumed);
    scroll_list_destroy(sl);
    printf("  PASS: empty_list_key\n");
}

static void test_set_items_null_entries(void) {
    ScrollList *sl = scroll_list_create();
    const char *items[] = {"A", NULL, "C"};
    bool ok = scroll_list_set_items(sl, items, 3);
    assert(ok);
    assert(scroll_list_count(sl) == 3);
    assert(strcmp(scroll_list_item(sl, 1), "") == 0);
    scroll_list_destroy(sl);
    printf("  PASS: set_items_null_entries\n");
}

int main(void) {
    printf("scroll_list_test:\n");
    test_create_destroy();
    test_set_items();
    test_append();
    test_clear();
    test_select();
    test_key_navigation();
#ifdef KEY_UP
    test_arrow_keys();
    test_page_keys();
#endif
    test_render_basic();
    test_render_scroll();
    test_mouse_click();
    test_mouse_scroll();
    test_null_safety();
    test_empty_list_key();
    test_set_items_null_entries();
    printf("All scroll_list tests passed.\n");
    return 0;
}
