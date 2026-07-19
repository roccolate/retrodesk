#include "test_harness.h"
#include <stdio.h>
#include <string.h>

#include "ui/text_buffer.h"

/* Helper to simulate a printable key press. */
static RetroKeyEvent key_char(char ch) {
    RetroKeyEvent k = {0};
    k.key_code = (int)ch;
    k.is_printable = true;
    k.ascii = ch;
    return k;
}

/* Helper to simulate a control/special key press. */
static RetroKeyEvent key_code(int code) {
    RetroKeyEvent k = {0};
    k.key_code = code;
    k.is_printable = false;
    k.ascii = '\0';
    return k;
}

static void test_create_destroy(void) {
    TextBuffer *tb = text_buffer_create();
    TEST_REQUIRE(tb != NULL);
    TEST_REQUIRE(text_buffer_line_count(tb) == 1);
    TEST_REQUIRE(text_buffer_cursor_row(tb) == 0);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 0);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "") == 0);
    text_buffer_destroy(tb);
    printf("  PASS: create_destroy\n");
}

static void test_insert_chars(void) {
    TextBuffer *tb = text_buffer_create();

    RetroKeyEvent k;
    k = key_char('H'); text_buffer_handle_key(tb, &k);
    k = key_char('i'); text_buffer_handle_key(tb, &k);
    k = key_char('!'); text_buffer_handle_key(tb, &k);

    TEST_REQUIRE(text_buffer_line_count(tb) == 1);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "Hi!") == 0);
    TEST_REQUIRE(text_buffer_cursor_row(tb) == 0);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 3);

    text_buffer_destroy(tb);
    printf("  PASS: insert_chars\n");
}

static void test_newline(void) {
    TextBuffer *tb = text_buffer_create();

    RetroKeyEvent k;
    k = key_char('A'); text_buffer_handle_key(tb, &k);
    k = key_char('B'); text_buffer_handle_key(tb, &k);
    k = key_code('\n'); text_buffer_handle_key(tb, &k);
    k = key_char('C'); text_buffer_handle_key(tb, &k);
    k = key_char('D'); text_buffer_handle_key(tb, &k);

    TEST_REQUIRE(text_buffer_line_count(tb) == 2);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "AB") == 0);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 1), "CD") == 0);
    TEST_REQUIRE(text_buffer_cursor_row(tb) == 1);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 2);

    text_buffer_destroy(tb);
    printf("  PASS: newline\n");
}

static void test_newline_mid_line(void) {
    TextBuffer *tb = text_buffer_create();

    text_buffer_set_text(tb, "ABCDEF");
    text_buffer_set_cursor(tb, 0, 3);

    /* Insert newline at cursor position 3 */
    RetroKeyEvent k = key_code('\n');
    text_buffer_handle_key(tb, &k);

    TEST_REQUIRE(text_buffer_line_count(tb) == 2);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "ABC") == 0);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 1), "DEF") == 0);
    TEST_REQUIRE(text_buffer_cursor_row(tb) == 1);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 0);

    text_buffer_destroy(tb);
    printf("  PASS: newline_mid_line\n");
}

static void test_backspace_within_line(void) {
    TextBuffer *tb = text_buffer_create();

    text_buffer_set_text(tb, "ABC");
    RetroKeyEvent k = key_code(127);
    text_buffer_handle_key(tb, &k);

    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "AB") == 0);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 2);

    text_buffer_destroy(tb);
    printf("  PASS: backspace_within_line\n");
}

static void test_backspace_merge_lines(void) {
    TextBuffer *tb = text_buffer_create();

    text_buffer_set_text(tb, "AB\nCD");
    TEST_REQUIRE(text_buffer_line_count(tb) == 2);

    /* Place cursor at start of line 1. */
    text_buffer_set_cursor(tb, 1, 0);
    RetroKeyEvent k = key_code(127);
    text_buffer_handle_key(tb, &k);

    TEST_REQUIRE(text_buffer_line_count(tb) == 1);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "ABCD") == 0);
    TEST_REQUIRE(text_buffer_cursor_row(tb) == 0);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 2);

    text_buffer_destroy(tb);
    printf("  PASS: backspace_merge_lines\n");
}

static void test_backspace_at_start(void) {
    TextBuffer *tb = text_buffer_create();

    text_buffer_set_cursor(tb, 0, 0);
    RetroKeyEvent k = key_code(127);
    bool consumed = text_buffer_handle_key(tb, &k);

    TEST_REQUIRE(!consumed);
    TEST_REQUIRE(text_buffer_line_count(tb) == 1);

    text_buffer_destroy(tb);
    printf("  PASS: backspace_at_start\n");
}

static void test_delete_forward_within_line(void) {
    TextBuffer *tb = text_buffer_create();

    text_buffer_set_text(tb, "ABCD");
    text_buffer_set_cursor(tb, 0, 1);

    RetroKeyEvent k = key_code(4); /* Ctrl+D */
    text_buffer_handle_key(tb, &k);

    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "ACD") == 0);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 1);

    text_buffer_destroy(tb);
    printf("  PASS: delete_forward_within_line\n");
}

static void test_delete_forward_merge_lines(void) {
    TextBuffer *tb = text_buffer_create();

    text_buffer_set_text(tb, "AB\nCD");
    text_buffer_set_cursor(tb, 0, 2); /* end of first line */

    RetroKeyEvent k = key_code(4); /* Ctrl+D */
    text_buffer_handle_key(tb, &k);

    TEST_REQUIRE(text_buffer_line_count(tb) == 1);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "ABCD") == 0);

    text_buffer_destroy(tb);
    printf("  PASS: delete_forward_merge_lines\n");
}

static void test_delete_forward_at_end(void) {
    TextBuffer *tb = text_buffer_create();

    text_buffer_set_text(tb, "AB");
    /* cursor at end after set_text */

    RetroKeyEvent k = key_code(4);
    bool consumed = text_buffer_handle_key(tb, &k);

    TEST_REQUIRE(!consumed);

    text_buffer_destroy(tb);
    printf("  PASS: delete_forward_at_end\n");
}

static void test_set_text_multiline(void) {
    TextBuffer *tb = text_buffer_create();

    text_buffer_set_text(tb, "Line1\nLine2\nLine3");
    TEST_REQUIRE(text_buffer_line_count(tb) == 3);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "Line1") == 0);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 1), "Line2") == 0);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 2), "Line3") == 0);
    TEST_REQUIRE(text_buffer_cursor_row(tb) == 2);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 5);

    text_buffer_destroy(tb);
    printf("  PASS: set_text_multiline\n");
}

static void test_set_text_trailing_newline(void) {
    TextBuffer *tb = text_buffer_create();

    text_buffer_set_text(tb, "A\n");
    TEST_REQUIRE(text_buffer_line_count(tb) == 2);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "A") == 0);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 1), "") == 0);

    text_buffer_destroy(tb);
    printf("  PASS: set_text_trailing_newline\n");
}

static void test_clear(void) {
    TextBuffer *tb = text_buffer_create();

    text_buffer_set_text(tb, "Hello\nWorld");
    text_buffer_clear(tb);

    TEST_REQUIRE(text_buffer_line_count(tb) == 1);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "") == 0);
    TEST_REQUIRE(text_buffer_cursor_row(tb) == 0);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 0);

    text_buffer_destroy(tb);
    printf("  PASS: clear\n");
}

static void test_set_cursor_clamp(void) {
    TextBuffer *tb = text_buffer_create();

    text_buffer_set_text(tb, "AB\nCDEF");

    /* Clamp row */
    text_buffer_set_cursor(tb, 100, 0);
    TEST_REQUIRE(text_buffer_cursor_row(tb) == 1);

    /* Clamp col to line length */
    text_buffer_set_cursor(tb, 0, 100);
    TEST_REQUIRE(text_buffer_cursor_row(tb) == 0);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 2);

    text_buffer_destroy(tb);
    printf("  PASS: set_cursor_clamp\n");
}

static void test_ctrl_shortcuts(void) {
    TextBuffer *tb = text_buffer_create();

    text_buffer_set_text(tb, "Hello World");

    /* Ctrl+A — home */
    RetroKeyEvent ctrl_a = key_code(1);
    text_buffer_handle_key(tb, &ctrl_a);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 0);

    /* Ctrl+E — end */
    RetroKeyEvent ctrl_e = key_code(5);
    text_buffer_handle_key(tb, &ctrl_e);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 11);

    /* Ctrl+K — kill to end */
    text_buffer_set_cursor(tb, 0, 5);
    RetroKeyEvent ctrl_k = key_code(11);
    text_buffer_handle_key(tb, &ctrl_k);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "Hello") == 0);

    text_buffer_destroy(tb);
    printf("  PASS: ctrl_shortcuts\n");
}

static void test_line_accessors_bounds(void) {
    TextBuffer *tb = text_buffer_create();

    /* Out of bounds access returns safe defaults. */
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 100), "") == 0);
    TEST_REQUIRE(text_buffer_line_length(tb, 100) == 0);

    text_buffer_destroy(tb);
    printf("  PASS: line_accessors_bounds\n");
}

static void test_multiple_newlines(void) {
    TextBuffer *tb = text_buffer_create();

    /* Three newlines in a row creates four lines. */
    RetroKeyEvent k = key_code('\n');
    text_buffer_handle_key(tb, &k);
    text_buffer_handle_key(tb, &k);
    text_buffer_handle_key(tb, &k);

    TEST_REQUIRE(text_buffer_line_count(tb) == 4);
    TEST_REQUIRE(text_buffer_cursor_row(tb) == 3);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 0);

    text_buffer_destroy(tb);
    printf("  PASS: multiple_newlines\n");
}

static void test_line_capacity_growth(void) {
    TextBuffer *tb = text_buffer_create();
    TEST_REQUIRE(tb != NULL);

    for (int i = 0; i < 16; ++i) {
        TEST_REQUIRE(text_buffer_insert_char(tb, (char)('A' + i)));
        TEST_REQUIRE(text_buffer_insert_newline(tb));
    }

    TEST_REQUIRE(text_buffer_line_count(tb) == 17);
    for (size_t row = 0; row < 16; ++row) {
        const char expected[] = {(char)('A' + (int)row), '\0'};
        TEST_REQUIRE(strcmp(text_buffer_line(tb, row), expected) == 0);
    }
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 16), "") == 0);
    TEST_REQUIRE(text_buffer_cursor_row(tb) == 16);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 0);

    text_buffer_destroy(tb);
    printf("  PASS: line_capacity_growth\n");
}

static void test_null_safety(void) {
    /* All functions should handle NULL gracefully. */
    TEST_REQUIRE(text_buffer_line_count(NULL) == 0);
    TEST_REQUIRE(text_buffer_cursor_row(NULL) == 0);
    TEST_REQUIRE(text_buffer_cursor_col(NULL) == 0);
    TEST_REQUIRE(text_buffer_scroll_row(NULL) == 0);
    TEST_REQUIRE(text_buffer_scroll_col(NULL) == 0);
    TEST_REQUIRE(strcmp(text_buffer_line(NULL, 0), "") == 0);
    TEST_REQUIRE(text_buffer_line_length(NULL, 0) == 0);
    TEST_REQUIRE(!text_buffer_insert_char(NULL, 'A'));
    TEST_REQUIRE(!text_buffer_insert_newline(NULL));
    TEST_REQUIRE(!text_buffer_delete_backward(NULL));
    TEST_REQUIRE(!text_buffer_delete_forward(NULL));
    TEST_REQUIRE(!text_buffer_handle_key(NULL, NULL));
    TEST_REQUIRE(!text_buffer_set_text(NULL, "test"));
    text_buffer_clear(NULL);
    text_buffer_set_cursor(NULL, 0, 0);
    text_buffer_destroy(NULL);
    text_buffer_render(NULL, NULL, 0, 0, 10, 40, NULL, NULL);
    printf("  PASS: null_safety\n");
}

static void test_render_basic(void) {
    TextBuffer *tb = text_buffer_create();
    DrawList *dl = draw_list_create();

    text_buffer_set_text(tb, "Line1\nLine2");
    text_buffer_set_cursor(tb, 0, 0);

    RenderStyle style = {RENDER_COLOR_WHITE, RENDER_COLOR_BLACK, false, false};
    RenderStyle cursor_style = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, true, false};

    text_buffer_render(tb, dl, 0, 0, 5, 20, &style, &cursor_style);

    /* Should have rendered lines + cursor. */
    TEST_REQUIRE(draw_list_count(dl) > 0);

    draw_list_destroy(dl);
    text_buffer_destroy(tb);
    printf("  PASS: render_basic\n");
}

int main(void) {
    printf("text_buffer_test:\n");
    test_create_destroy();
    test_insert_chars();
    test_newline();
    test_newline_mid_line();
    test_backspace_within_line();
    test_backspace_merge_lines();
    test_backspace_at_start();
    test_delete_forward_within_line();
    test_delete_forward_merge_lines();
    test_delete_forward_at_end();
    test_set_text_multiline();
    test_set_text_trailing_newline();
    test_clear();
    test_set_cursor_clamp();
    test_ctrl_shortcuts();
    test_line_accessors_bounds();
    test_multiple_newlines();
    test_line_capacity_growth();
    test_null_safety();
    test_render_basic();
    printf("All text_buffer tests passed.\n");
    return 0;
}
