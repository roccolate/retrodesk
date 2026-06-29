#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ui/button.h"

/* --- callback tracking ------------------------------------------------ */

static int g_callback_count;
static Button *g_callback_button;
static void *g_callback_data;

static void test_callback(Button *button, void *user_data) {
    g_callback_count++;
    g_callback_button = button;
    g_callback_data = user_data;
}

static void reset_callback_state(void) {
    g_callback_count = 0;
    g_callback_button = NULL;
    g_callback_data = NULL;
}

/* --- tests ------------------------------------------------------------ */

static void test_create_destroy(void) {
    Button *btn = button_create("OK");
    assert(btn != NULL);
    assert(strcmp(button_label(btn), "OK") == 0);
    assert(button_focused(btn) == false);
    assert(button_enabled(btn) == true);
    assert(button_width(btn) == 6);  /* "[ OK ]" */
    button_destroy(btn);

    /* NULL label creates empty button */
    btn = button_create(NULL);
    assert(btn != NULL);
    assert(strcmp(button_label(btn), "") == 0);
    assert(button_width(btn) == 4);  /* "[  ]" */
    button_destroy(btn);

    printf("  PASS: create_destroy\n");
}

static void test_set_label(void) {
    Button *btn = button_create("OK");
    bool ok = button_set_label(btn, "Cancel");
    assert(ok);
    assert(strcmp(button_label(btn), "Cancel") == 0);
    assert(button_width(btn) == 10);  /* "[ Cancel ]" */

    ok = button_set_label(btn, "");
    assert(ok);
    assert(strcmp(button_label(btn), "") == 0);
    assert(button_width(btn) == 4);

    button_destroy(btn);
    printf("  PASS: set_label\n");
}

static void test_focus(void) {
    Button *btn = button_create("OK");
    assert(!button_focused(btn));

    button_set_focused(btn, true);
    assert(button_focused(btn));

    button_set_focused(btn, false);
    assert(!button_focused(btn));

    button_destroy(btn);
    printf("  PASS: focus\n");
}

static void test_enabled(void) {
    Button *btn = button_create("OK");
    assert(button_enabled(btn));

    button_set_enabled(btn, false);
    assert(!button_enabled(btn));

    button_set_enabled(btn, true);
    assert(button_enabled(btn));

    button_destroy(btn);
    printf("  PASS: enabled\n");
}

static void test_key_enter_activates(void) {
    Button *btn = button_create("OK");
    reset_callback_state();
    int sentinel = 42;
    button_set_callback(btn, test_callback, &sentinel);
    button_set_focused(btn, true);

    RetroKeyEvent enter = {.key_code = '\n', .is_printable = false, .ascii = '\0'};
    bool consumed = button_handle_key(btn, &enter);
    assert(consumed);
    assert(g_callback_count == 1);
    assert(g_callback_button == btn);
    assert(g_callback_data == &sentinel);

    button_destroy(btn);
    printf("  PASS: key_enter_activates\n");
}

static void test_key_space_activates(void) {
    Button *btn = button_create("OK");
    reset_callback_state();
    button_set_callback(btn, test_callback, NULL);
    button_set_focused(btn, true);

    RetroKeyEvent space = {.key_code = ' ', .is_printable = true, .ascii = ' '};
    bool consumed = button_handle_key(btn, &space);
    assert(consumed);
    assert(g_callback_count == 1);

    button_destroy(btn);
    printf("  PASS: key_space_activates\n");
}

static void test_key_unfocused_ignored(void) {
    Button *btn = button_create("OK");
    reset_callback_state();
    button_set_callback(btn, test_callback, NULL);
    /* Not focused */

    RetroKeyEvent enter = {.key_code = '\n', .is_printable = false, .ascii = '\0'};
    bool consumed = button_handle_key(btn, &enter);
    assert(!consumed);
    assert(g_callback_count == 0);

    button_destroy(btn);
    printf("  PASS: key_unfocused_ignored\n");
}

static void test_key_disabled_ignored(void) {
    Button *btn = button_create("OK");
    reset_callback_state();
    button_set_callback(btn, test_callback, NULL);
    button_set_focused(btn, true);
    button_set_enabled(btn, false);

    RetroKeyEvent enter = {.key_code = '\n', .is_printable = false, .ascii = '\0'};
    bool consumed = button_handle_key(btn, &enter);
    assert(!consumed);
    assert(g_callback_count == 0);

    button_destroy(btn);
    printf("  PASS: key_disabled_ignored\n");
}

static void test_key_other_not_consumed(void) {
    Button *btn = button_create("OK");
    reset_callback_state();
    button_set_callback(btn, test_callback, NULL);
    button_set_focused(btn, true);

    RetroKeyEvent letter = {.key_code = 'a', .is_printable = true, .ascii = 'a'};
    bool consumed = button_handle_key(btn, &letter);
    assert(!consumed);
    assert(g_callback_count == 0);

    button_destroy(btn);
    printf("  PASS: key_other_not_consumed\n");
}

static void test_mouse_click_activates(void) {
    Button *btn = button_create("OK");
    reset_callback_state();
    button_set_callback(btn, test_callback, NULL);

    /* Button at y=5, x=10, width=6 ("[ OK ]") */
    RetroMouseEvent click = {
        .y = 5, .x = 12,
        .button1_clicked = true,
    };
    bool consumed = button_handle_mouse(btn, &click, 5, 10, 6);
    assert(consumed);
    assert(g_callback_count == 1);

    button_destroy(btn);
    printf("  PASS: mouse_click_activates\n");
}

static void test_mouse_outside_ignored(void) {
    Button *btn = button_create("OK");
    reset_callback_state();
    button_set_callback(btn, test_callback, NULL);

    /* Click outside vertically */
    RetroMouseEvent click = {
        .y = 6, .x = 12,
        .button1_clicked = true,
    };
    bool consumed = button_handle_mouse(btn, &click, 5, 10, 6);
    assert(!consumed);
    assert(g_callback_count == 0);

    /* Click outside horizontally (right) */
    RetroMouseEvent click2 = {
        .y = 5, .x = 16,
        .button1_clicked = true,
    };
    consumed = button_handle_mouse(btn, &click2, 5, 10, 6);
    assert(!consumed);
    assert(g_callback_count == 0);

    /* Click outside horizontally (left) */
    RetroMouseEvent click3 = {
        .y = 5, .x = 9,
        .button1_clicked = true,
    };
    consumed = button_handle_mouse(btn, &click3, 5, 10, 6);
    assert(!consumed);
    assert(g_callback_count == 0);

    button_destroy(btn);
    printf("  PASS: mouse_outside_ignored\n");
}

static void test_mouse_disabled_ignored(void) {
    Button *btn = button_create("OK");
    reset_callback_state();
    button_set_callback(btn, test_callback, NULL);
    button_set_enabled(btn, false);

    RetroMouseEvent click = {
        .y = 5, .x = 12,
        .button1_clicked = true,
    };
    bool consumed = button_handle_mouse(btn, &click, 5, 10, 6);
    assert(!consumed);
    assert(g_callback_count == 0);

    button_destroy(btn);
    printf("  PASS: mouse_disabled_ignored\n");
}

static void test_render_normal(void) {
    Button *btn = button_create("OK");
    DrawList *dl = draw_list_create();

    RenderStyle normal = {.fg = RENDER_COLOR_WHITE, .bg = RENDER_COLOR_BLACK,
                          .reverse = false, .bold = false};
    RenderStyle focused = {.fg = RENDER_COLOR_BLACK, .bg = RENDER_COLOR_WHITE,
                           .reverse = true, .bold = true};
    RenderStyle disabled = {.fg = RENDER_COLOR_BLACK, .bg = RENDER_COLOR_BLACK,
                            .reverse = false, .bold = false};

    button_render(btn, dl, 5, 10, &normal, &focused, &disabled);
    assert(draw_list_count(dl) == 1);

    DrawCommandView cmd;
    bool ok = draw_list_get(dl, 0, &cmd);
    assert(ok);
    assert(cmd.type == DRAW_COMMAND_TEXT);
    assert(cmd.y == 5);
    assert(cmd.x == 10);
    assert(strcmp(cmd.text, "[ OK ]") == 0);
    /* Normal style (not focused, enabled) */
    assert(cmd.style.reverse == false);

    draw_list_destroy(dl);
    button_destroy(btn);
    printf("  PASS: render_normal\n");
}

static void test_render_focused(void) {
    Button *btn = button_create("OK");
    button_set_focused(btn, true);
    DrawList *dl = draw_list_create();

    RenderStyle normal = {.fg = RENDER_COLOR_WHITE, .bg = RENDER_COLOR_BLACK,
                          .reverse = false, .bold = false};
    RenderStyle focused = {.fg = RENDER_COLOR_BLACK, .bg = RENDER_COLOR_WHITE,
                           .reverse = true, .bold = true};
    RenderStyle disabled = {.fg = RENDER_COLOR_BLACK, .bg = RENDER_COLOR_BLACK,
                            .reverse = false, .bold = false};

    button_render(btn, dl, 0, 0, &normal, &focused, &disabled);
    assert(draw_list_count(dl) == 1);

    DrawCommandView cmd;
    draw_list_get(dl, 0, &cmd);
    assert(cmd.style.reverse == true);
    assert(cmd.style.bold == true);

    draw_list_destroy(dl);
    button_destroy(btn);
    printf("  PASS: render_focused\n");
}

static void test_render_disabled(void) {
    Button *btn = button_create("OK");
    button_set_enabled(btn, false);
    DrawList *dl = draw_list_create();

    RenderStyle normal = {.fg = RENDER_COLOR_WHITE, .bg = RENDER_COLOR_BLACK,
                          .reverse = false, .bold = false};
    RenderStyle focused = {.fg = RENDER_COLOR_BLACK, .bg = RENDER_COLOR_WHITE,
                           .reverse = true, .bold = true};
    RenderStyle disabled = {.fg = RENDER_COLOR_BLACK, .bg = RENDER_COLOR_BLACK,
                            .reverse = false, .bold = false};

    button_render(btn, dl, 0, 0, &normal, &focused, &disabled);
    assert(draw_list_count(dl) == 1);

    DrawCommandView cmd;
    draw_list_get(dl, 0, &cmd);
    /* Should use disabled style */
    assert(cmd.style.fg == RENDER_COLOR_BLACK);

    draw_list_destroy(dl);
    button_destroy(btn);
    printf("  PASS: render_disabled\n");
}

static void test_no_callback(void) {
    /* Activating with no callback set should not crash */
    Button *btn = button_create("OK");
    button_set_focused(btn, true);

    RetroKeyEvent enter = {.key_code = '\n', .is_printable = false, .ascii = '\0'};
    bool consumed = button_handle_key(btn, &enter);
    assert(consumed);

    button_destroy(btn);
    printf("  PASS: no_callback\n");
}

static void test_null_safety(void) {
    assert(strcmp(button_label(NULL), "") == 0);
    assert(button_focused(NULL) == false);
    assert(button_enabled(NULL) == false);
    assert(button_width(NULL) == 0);
    assert(!button_set_label(NULL, "x"));
    assert(!button_handle_key(NULL, NULL));
    assert(!button_handle_mouse(NULL, NULL, 0, 0, 0));
    button_set_focused(NULL, true);
    button_set_enabled(NULL, true);
    button_set_callback(NULL, NULL, NULL);
    button_render(NULL, NULL, 0, 0, NULL, NULL, NULL);
    button_destroy(NULL);
    printf("  PASS: null_safety\n");
}

int main(void) {
    printf("button_test:\n");
    test_create_destroy();
    test_set_label();
    test_focus();
    test_enabled();
    test_key_enter_activates();
    test_key_space_activates();
    test_key_unfocused_ignored();
    test_key_disabled_ignored();
    test_key_other_not_consumed();
    test_mouse_click_activates();
    test_mouse_outside_ignored();
    test_mouse_disabled_ignored();
    test_render_normal();
    test_render_focused();
    test_render_disabled();
    test_no_callback();
    test_null_safety();
    printf("All button tests passed.\n");
    return 0;
}
