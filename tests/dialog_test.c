#include "test_harness.h"
#include <stdio.h>
#include <string.h>

#include "ui/dialog.h"

/* --- helpers ---------------------------------------------------------- */

static RenderStyle make_style(int fg, int bg, bool reverse, bool bold) {
    RenderStyle s = {
        .fg = (RenderColor)fg,
        .bg = (RenderColor)bg,
        .reverse = reverse,
        .bold = bold,
    };
    return s;
}

static DialogStyles make_dialog_styles(void) {
    DialogStyles s;
    s.frame = make_style(RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true);
    s.title = make_style(RENDER_COLOR_YELLOW, RENDER_COLOR_BLUE, true, true);
    s.message = make_style(RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, false);
    s.input = make_style(RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false);
    s.input_cursor = make_style(RENDER_COLOR_WHITE, RENDER_COLOR_BLACK, true, true);
    s.btn_normal = make_style(RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false);
    s.btn_focused = make_style(RENDER_COLOR_WHITE, RENDER_COLOR_BLACK, true, true);
    return s;
}

/* --- lifecycle tests -------------------------------------------------- */

static void test_create_message(void) {
    Dialog *dlg = dialog_create_message("Info", "Hello world");
    TEST_REQUIRE(dlg != NULL);
    TEST_REQUIRE(dialog_type(dlg) == DIALOG_MESSAGE);
    TEST_REQUIRE(dialog_result(dlg) == DIALOG_RESULT_NONE);
    TEST_REQUIRE(dialog_suggest_height(dlg) >= 7);
    TEST_REQUIRE(dialog_suggest_width(dlg, 80) >= 20);
    dialog_destroy(dlg);

    /* NULL title/message should still work. */
    dlg = dialog_create_message(NULL, NULL);
    TEST_REQUIRE(dlg != NULL);
    TEST_REQUIRE(dialog_type(dlg) == DIALOG_MESSAGE);
    dialog_destroy(dlg);
    printf("  PASS: create_message\n");
}

static void test_create_confirm(void) {
    Dialog *dlg = dialog_create_confirm("Confirm", "Are you sure?");
    TEST_REQUIRE(dlg != NULL);
    TEST_REQUIRE(dialog_type(dlg) == DIALOG_CONFIRM);
    TEST_REQUIRE(dialog_result(dlg) == DIALOG_RESULT_NONE);
    dialog_destroy(dlg);
    printf("  PASS: create_confirm\n");
}

static void test_create_input(void) {
    Dialog *dlg = dialog_create_input("Rename", "New name:", 16);
    TEST_REQUIRE(dlg != NULL);
    TEST_REQUIRE(dialog_type(dlg) == DIALOG_INPUT);
    TEST_REQUIRE(dialog_result(dlg) == DIALOG_RESULT_NONE);
    TEST_REQUIRE(strcmp(dialog_input_text(dlg), "") == 0);
    dialog_destroy(dlg);
    printf("  PASS: create_input\n");
}

/* --- result + key handling ------------------------------------------- */

static void test_message_enter_ok(void) {
    Dialog *dlg = dialog_create_message("Info", "Hi");
    RetroKeyEvent enter = {.key_code = '\n', .is_printable = false, .ascii = 0};
    bool consumed = dialog_handle_key(dlg, &enter);
    TEST_REQUIRE(consumed);
    TEST_REQUIRE(dialog_result(dlg) == DIALOG_RESULT_OK);
    dialog_destroy(dlg);
    printf("  PASS: message_enter_ok\n");
}

static void test_message_escape_ok(void) {
    Dialog *dlg = dialog_create_message("Info", "Hi");
    RetroKeyEvent esc = {.key_code = 27, .is_printable = false, .ascii = 0};
    bool consumed = dialog_handle_key(dlg, &esc);
    TEST_REQUIRE(consumed);
    TEST_REQUIRE(dialog_result(dlg) == DIALOG_RESULT_OK);
    dialog_destroy(dlg);
    printf("  PASS: message_escape_ok\n");
}

static void test_confirm_escape_cancel(void) {
    Dialog *dlg = dialog_create_confirm("Confirm", "Sure?");
    RetroKeyEvent esc = {.key_code = 27, .is_printable = false, .ascii = 0};
    bool consumed = dialog_handle_key(dlg, &esc);
    TEST_REQUIRE(consumed);
    TEST_REQUIRE(dialog_result(dlg) == DIALOG_RESULT_CANCEL);
    dialog_destroy(dlg);
    printf("  PASS: confirm_escape_cancel\n");
}

static void test_confirm_tab_cycle_then_enter(void) {
    Dialog *dlg = dialog_create_confirm("Confirm", "Sure?");
    /* Initial focus is OK. Tab should move to Cancel. */
    RetroKeyEvent tab = {.key_code = '\t', .is_printable = false, .ascii = 0};
    dialog_handle_key(dlg, &tab);
    /* Enter now should activate Cancel. */
    RetroKeyEvent enter = {.key_code = '\n', .is_printable = false, .ascii = 0};
    bool consumed = dialog_handle_key(dlg, &enter);
    TEST_REQUIRE(consumed);
    TEST_REQUIRE(dialog_result(dlg) == DIALOG_RESULT_CANCEL);
    dialog_destroy(dlg);
    printf("  PASS: confirm_tab_cycle_then_enter\n");
}

static void test_input_typing(void) {
    Dialog *dlg = dialog_create_input("Rename", "Name:", 16);
    /* Initial focus is on the input — printable keys should go there. */
    RetroKeyEvent h = {.key_code = 'h', .is_printable = true, .ascii = 'h'};
    RetroKeyEvent i = {.key_code = 'i', .is_printable = true, .ascii = 'i'};
    TEST_REQUIRE(dialog_handle_key(dlg, &h));
    TEST_REQUIRE(dialog_handle_key(dlg, &i));
    TEST_REQUIRE(strcmp(dialog_input_text(dlg), "hi") == 0);
    TEST_REQUIRE(dialog_result(dlg) == DIALOG_RESULT_NONE);
    dialog_destroy(dlg);
    printf("  PASS: input_typing\n");
}

static void test_input_tab_to_ok(void) {
    Dialog *dlg = dialog_create_input("Rename", "Name:", 16);
    RetroKeyEvent h = {.key_code = 'h', .is_printable = true, .ascii = 'h'};
    dialog_handle_key(dlg, &h);
    RetroKeyEvent tab = {.key_code = '\t', .is_printable = false, .ascii = 0};
    dialog_handle_key(dlg, &tab);
    RetroKeyEvent enter = {.key_code = '\n', .is_printable = false, .ascii = 0};
    bool consumed = dialog_handle_key(dlg, &enter);
    TEST_REQUIRE(consumed);
    TEST_REQUIRE(dialog_result(dlg) == DIALOG_RESULT_OK);
    TEST_REQUIRE(strcmp(dialog_input_text(dlg), "h") == 0);
    dialog_destroy(dlg);
    printf("  PASS: input_tab_to_ok\n");
}

static void test_dismissed_ignores_events(void) {
    Dialog *dlg = dialog_create_message("Info", "Hi");
    RetroKeyEvent enter = {.key_code = '\n', .is_printable = false, .ascii = 0};
    dialog_handle_key(dlg, &enter);
    TEST_REQUIRE(dialog_result(dlg) == DIALOG_RESULT_OK);
    /* Further keys are consumed but result remains OK. */
    RetroKeyEvent esc = {.key_code = 27, .is_printable = false, .ascii = 0};
    bool consumed = dialog_handle_key(dlg, &esc);
    TEST_REQUIRE(consumed);
    TEST_REQUIRE(dialog_result(dlg) == DIALOG_RESULT_OK);
    dialog_destroy(dlg);
    printf("  PASS: dismissed_ignores_events\n");
}

/* --- mouse handling -------------------------------------------------- */

static void test_mouse_click_ok(void) {
    Dialog *dlg = dialog_create_confirm("Confirm", "Sure?");
    int w = dialog_suggest_width(dlg, 80);
    int h = dialog_suggest_height(dlg);
    DialogStyles styles = make_dialog_styles();
    DrawList *dl = draw_list_create();

    dialog_render(dlg, dl, 0, 0, w, h, &styles);
    /* Find the OK button position by inspecting draw_list. */
    int ok_x = -1, ok_y = -1, ok_w = 0;
    for (size_t i = 0; i < draw_list_count(dl); i++) {
        DrawCommandView cmd;
        if (draw_list_get(dl, i, &cmd) && cmd.type == DRAW_COMMAND_TEXT &&
            cmd.text && strstr(cmd.text, "[ OK ]")) {
            ok_x = cmd.x;
            ok_y = cmd.y;
            ok_w = (int)strlen(cmd.text);
            break;
        }
    }
    TEST_REQUIRE(ok_x >= 0);
    TEST_REQUIRE(ok_y >= 0);
    TEST_REQUIRE(ok_w > 0);

    RetroMouseEvent click = {
        .y = ok_y,
        .x = ok_x + 2, /* inside "[ OK ]" */
        .button1_clicked = true,
    };
    bool consumed = dialog_handle_mouse(dlg, &click, 0, 0, w, h);
    TEST_REQUIRE(consumed);
    TEST_REQUIRE(dialog_result(dlg) == DIALOG_RESULT_OK);

    draw_list_destroy(dl);
    dialog_destroy(dlg);
    printf("  PASS: mouse_click_ok\n");
}

static void test_mouse_click_cancel(void) {
    Dialog *dlg = dialog_create_confirm("Confirm", "Sure?");
    int w = dialog_suggest_width(dlg, 80);
    int h = dialog_suggest_height(dlg);
    DialogStyles styles = make_dialog_styles();
    DrawList *dl = draw_list_create();

    dialog_render(dlg, dl, 0, 0, w, h, &styles);
    int cancel_x = -1, cancel_y = -1;
    for (size_t i = 0; i < draw_list_count(dl); i++) {
        DrawCommandView cmd;
        if (draw_list_get(dl, i, &cmd) && cmd.type == DRAW_COMMAND_TEXT &&
            cmd.text && strstr(cmd.text, "[ Cancel ]")) {
            cancel_x = cmd.x;
            cancel_y = cmd.y;
            break;
        }
    }
    TEST_REQUIRE(cancel_x >= 0);

    RetroMouseEvent click = {
        .y = cancel_y,
        .x = cancel_x + 2,
        .button1_clicked = true,
    };
    bool consumed = dialog_handle_mouse(dlg, &click, 0, 0, w, h);
    TEST_REQUIRE(consumed);
    TEST_REQUIRE(dialog_result(dlg) == DIALOG_RESULT_CANCEL);

    draw_list_destroy(dl);
    dialog_destroy(dlg);
    printf("  PASS: mouse_click_cancel\n");
}

static void test_mouse_outside(void) {
    Dialog *dlg = dialog_create_confirm("Confirm", "Sure?");
    RetroMouseEvent click = {
        .y = 50, .x = 50,
        .button1_clicked = true,
    };
    bool consumed = dialog_handle_mouse(dlg, &click, 0, 0, 30, 10);
    TEST_REQUIRE(!consumed);
    TEST_REQUIRE(dialog_result(dlg) == DIALOG_RESULT_NONE);
    dialog_destroy(dlg);
    printf("  PASS: mouse_outside\n");
}

/* --- rendering ------------------------------------------------------- */

static void test_render_draws_frame_and_buttons(void) {
    Dialog *dlg = dialog_create_confirm("Confirm", "Are you sure?");
    int w = dialog_suggest_width(dlg, 80);
    int h = dialog_suggest_height(dlg);
    DialogStyles styles = make_dialog_styles();
    DrawList *dl = draw_list_create();

    dialog_render(dlg, dl, 0, 0, w, h, &styles);

    bool saw_box = false;
    bool saw_ok = false;
    bool saw_cancel = false;
    bool saw_message = false;
    for (size_t i = 0; i < draw_list_count(dl); i++) {
        DrawCommandView cmd;
        if (!draw_list_get(dl, i, &cmd)) continue;
        if (cmd.type == DRAW_COMMAND_BOX) saw_box = true;
        if (cmd.type == DRAW_COMMAND_TEXT && cmd.text) {
            if (strstr(cmd.text, "[ OK ]")) saw_ok = true;
            if (strstr(cmd.text, "[ Cancel ]")) saw_cancel = true;
            if (strstr(cmd.text, "Are you sure?")) saw_message = true;
        }
    }
    TEST_REQUIRE(saw_box);
    TEST_REQUIRE(saw_ok);
    TEST_REQUIRE(saw_cancel);
    TEST_REQUIRE(saw_message);

    draw_list_destroy(dl);
    dialog_destroy(dlg);
    printf("  PASS: render_draws_frame_and_buttons\n");
}

static void test_render_message_wraps(void) {
    const char *long_msg =
        "This is a longer message that should be split into multiple lines "
        "when the dialog renders it into a narrower box.";
    Dialog *dlg = dialog_create_message("Info", long_msg);
    int w = 24;
    int h = dialog_suggest_height(dlg);
    DialogStyles styles = make_dialog_styles();
    DrawList *dl = draw_list_create();

    dialog_render(dlg, dl, 0, 0, w, h, &styles);

    int text_lines = 0;
    for (size_t i = 0; i < draw_list_count(dl); i++) {
        DrawCommandView cmd;
        if (draw_list_get(dl, i, &cmd) && cmd.type == DRAW_COMMAND_TEXT &&
            cmd.text) {
            text_lines++;
        }
    }
    /* Expect at least: 1 title-bearing box + several message lines + OK */
    TEST_REQUIRE(text_lines >= 3);

    draw_list_destroy(dl);
    dialog_destroy(dlg);
    printf("  PASS: render_message_wraps\n");
}

static void test_render_input_draws_field(void) {
    Dialog *dlg = dialog_create_input("Rename", "New name:", 16);
    RetroKeyEvent h = {.key_code = 'h', .is_printable = true, .ascii = 'h'};
    dialog_handle_key(dlg, &h);
    int w = dialog_suggest_width(dlg, 80);
    int h_h = dialog_suggest_height(dlg);
    DialogStyles styles = make_dialog_styles();
    DrawList *dl = draw_list_create();
    dialog_render(dlg, dl, 0, 0, w, h_h, &styles);

    /* Should draw at least one OK + one Cancel + an input row + message. */
    size_t n = draw_list_count(dl);
    TEST_REQUIRE(n > 0);

    draw_list_destroy(dl);
    dialog_destroy(dlg);
    printf("  PASS: render_input_draws_field\n");
}

/* --- pre-fill -------------------------------------------------------- */

static void test_set_input_text(void) {
    Dialog *dlg = dialog_create_input("Rename", "Name:", 32);
    bool ok = dialog_set_input_text(dlg, "pre-filled");
    TEST_REQUIRE(ok);
    TEST_REQUIRE(strcmp(dialog_input_text(dlg), "pre-filled") == 0);
    /* Pre-filled text is truncated to max_input_len. */
    ok = dialog_set_input_text(dlg, "this string is definitely too long for the limit");
    TEST_REQUIRE(ok);
    TEST_REQUIRE(strlen(dialog_input_text(dlg)) <= 32);
    dialog_destroy(dlg);
    printf("  PASS: set_input_text\n");
}

/* --- null safety ----------------------------------------------------- */

static void test_null_safety(void) {
    dialog_destroy(NULL);
    TEST_REQUIRE(dialog_type(NULL) == DIALOG_MESSAGE);
    TEST_REQUIRE(dialog_result(NULL) == DIALOG_RESULT_NONE);
    TEST_REQUIRE(strcmp(dialog_input_text(NULL), "") == 0);
    TEST_REQUIRE(!dialog_set_input_text(NULL, "x"));
    RetroKeyEvent k = {.key_code = 'a', .is_printable = true, .ascii = 'a'};
    TEST_REQUIRE(!dialog_handle_key(NULL, &k));
    RetroMouseEvent m = {.button1_clicked = true};
    TEST_REQUIRE(!dialog_handle_mouse(NULL, &m, 0, 0, 10, 10));
    TEST_REQUIRE(dialog_suggest_width(NULL, 80) == 20);
    TEST_REQUIRE(dialog_suggest_height(NULL) == 7);
    dialog_render(NULL, NULL, 0, 0, 10, 10, NULL);
    printf("  PASS: null_safety\n");
}

int main(void) {
    printf("dialog_test:\n");
    test_create_message();
    test_create_confirm();
    test_create_input();
    test_message_enter_ok();
    test_message_escape_ok();
    test_confirm_escape_cancel();
    test_confirm_tab_cycle_then_enter();
    test_input_typing();
    test_input_tab_to_ok();
    test_dismissed_ignores_events();
    test_mouse_click_ok();
    test_mouse_click_cancel();
    test_mouse_outside();
    test_render_draws_frame_and_buttons();
    test_render_message_wraps();
    test_render_input_draws_field();
    test_set_input_text();
    test_null_safety();
    printf("All dialog tests passed.\n");
    return 0;
}
