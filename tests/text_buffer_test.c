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

static RetroKeyEvent key_codepoint(uint32_t codepoint) {
    RetroKeyEvent key = {0};
    key.key_code = (int)codepoint;
    key.is_printable = true;
    key.text_codepoint = codepoint;
    return key;
}

static RetroKeyEvent key_modified(int code,
                                  unsigned int modifiers) {
    RetroKeyEvent key = key_code(code);
    key.modifiers = modifiers;
    return key;
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

static void test_utf8_insert_and_roundtrip(void) {
    TextBuffer *tb = text_buffer_create();
    TEST_REQUIRE(tb != NULL);

    RetroKeyEvent enye = key_codepoint(0x00F1u);
    RetroKeyEvent o_acute = key_codepoint(0x00F3u);
    RetroKeyEvent u_diaeresis = key_codepoint(0x00FCu);
    TEST_REQUIRE(text_buffer_handle_key(tb, &enye));
    TEST_REQUIRE(text_buffer_handle_key(tb, &o_acute));
    TEST_REQUIRE(text_buffer_handle_key(tb, &u_diaeresis));

    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "ñóü") == 0);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 6);

    size_t length = 0;
    char *snapshot = text_buffer_to_text(tb, &length);
    TEST_REQUIRE(snapshot != NULL);
    TEST_REQUIRE(length == 6);
    TEST_REQUIRE(strcmp(snapshot, "ñóü") == 0);
    free(snapshot);

    text_buffer_destroy(tb);
    printf("  PASS: utf8_insert_and_roundtrip\n");
}

static void test_utf8_navigation_uses_codepoint_boundaries(void) {
    TextBuffer *tb = text_buffer_create();
    TEST_REQUIRE(text_buffer_set_text(tb, "año"));
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 4);

    RetroKeyEvent left = key_code(RETRO_KEY_LEFT);
    RetroKeyEvent right = key_code(RETRO_KEY_RIGHT);
    TEST_REQUIRE(text_buffer_handle_key(tb, &left));
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 3);
    TEST_REQUIRE(text_buffer_handle_key(tb, &left));
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 1);
    TEST_REQUIRE(text_buffer_handle_key(tb, &right));
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 3);

    text_buffer_set_cursor(tb, 0, 2);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 1);

    text_buffer_destroy(tb);
    printf("  PASS: utf8_navigation_uses_codepoint_boundaries\n");
}

static void test_utf8_backspace_and_delete_remove_whole_codepoints(void) {
    TextBuffer *tb = text_buffer_create();
    TEST_REQUIRE(text_buffer_set_text(tb, "niño"));

    RetroKeyEvent backspace = key_code(RETRO_KEY_BS);
    TEST_REQUIRE(text_buffer_handle_key(tb, &backspace));
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "niñ") == 0);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 4);
    TEST_REQUIRE(text_buffer_handle_key(tb, &backspace));
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "ni") == 0);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 2);

    TEST_REQUIRE(text_buffer_set_text(tb, "pingüino"));
    text_buffer_set_cursor(tb, 0, 4);
    RetroKeyEvent delete_forward = key_code(RETRO_KEY_DC);
    TEST_REQUIRE(text_buffer_handle_key(tb, &delete_forward));
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "pingino") == 0);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 4);

    text_buffer_destroy(tb);
    printf("  PASS: utf8_backspace_and_delete_remove_whole_codepoints\n");
}

static void test_utf8_vertical_navigation_preserves_display_column(void) {
    TextBuffer *tb = text_buffer_create();
    TEST_REQUIRE(text_buffer_set_text(tb, "éx\nabcdef"));
    text_buffer_set_cursor(tb, 0, 3);

    RetroKeyEvent down = key_code(RETRO_KEY_DOWN);
    TEST_REQUIRE(text_buffer_handle_key(tb, &down));
    TEST_REQUIRE(text_buffer_cursor_row(tb) == 1);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 2);

    RetroKeyEvent up = key_code(RETRO_KEY_UP);
    TEST_REQUIRE(text_buffer_handle_key(tb, &up));
    TEST_REQUIRE(text_buffer_cursor_row(tb) == 0);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 3);

    text_buffer_destroy(tb);
    printf("  PASS: utf8_vertical_navigation_preserves_display_column\n");
}

static void test_utf8_invalid_text_is_rejected_without_data_loss(void) {
    TextBuffer *tb = text_buffer_create();
    TEST_REQUIRE(text_buffer_set_text(tb, "safe"));
    const char invalid[] = {(char)0xC3, '\0'};
    TEST_REQUIRE(!text_buffer_set_text(tb, invalid));
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "safe") == 0);

    text_buffer_destroy(tb);
    printf("  PASS: utf8_invalid_text_is_rejected_without_data_loss\n");
}

static void test_utf8_render_uses_cell_columns(void) {
    TextBuffer *tb = text_buffer_create();
    DrawList *list = draw_list_create();
    TEST_REQUIRE(tb != NULL);
    TEST_REQUIRE(list != NULL);
    TEST_REQUIRE(text_buffer_set_text(tb, "niño"));
    text_buffer_set_cursor(tb, 0, 2);

    RenderStyle style = {
        RENDER_COLOR_WHITE, RENDER_COLOR_BLACK, false, false
    };
    RenderStyle cursor_style = {
        RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, true, false
    };
    text_buffer_render(tb, list, 0, 0, 1, 6,
                       &style, &cursor_style);

    TEST_REQUIRE(draw_list_count(list) == 2);
    DrawCommandView command = {0};
    TEST_REQUIRE(draw_list_get(list, 0, &command));
    TEST_REQUIRE(strcmp(command.text, "niño  ") == 0);
    TEST_REQUIRE(draw_list_get(list, 1, &command));
    TEST_REQUIRE(command.x == 2);
    TEST_REQUIRE(strcmp(command.text, "ñ") == 0);

    draw_list_destroy(list);
    text_buffer_destroy(tb);
    printf("  PASS: utf8_render_uses_cell_columns\n");
}

static void test_utf8_shift_selection_and_replacement(void) {
    TextBuffer *tb = text_buffer_create();
    TEST_REQUIRE(text_buffer_set_text(tb, "niño"));
    text_buffer_set_cursor(tb, 0, 2);

    RetroKeyEvent shift_right = key_modified(
        RETRO_KEY_RIGHT, RETRO_MOD_SHIFT);
    TEST_REQUIRE(text_buffer_handle_key(tb, &shift_right));
    TEST_REQUIRE(text_buffer_has_selection(tb));

    size_t length = 0;
    char *selected = text_buffer_selected_text(tb, &length);
    TEST_REQUIRE(selected != NULL);
    TEST_REQUIRE(length == 2);
    TEST_REQUIRE(strcmp(selected, "ñ") == 0);
    free(selected);

    RetroKeyEvent replacement = key_char('x');
    TEST_REQUIRE(text_buffer_handle_key(tb, &replacement));
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "nixo") == 0);
    TEST_REQUIRE(!text_buffer_has_selection(tb));
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 3);

    text_buffer_destroy(tb);
    printf("  PASS: utf8_shift_selection_and_replacement\n");
}

static void test_multiline_selection_delete(void) {
    TextBuffer *tb = text_buffer_create();
    TEST_REQUIRE(text_buffer_set_text(tb, "uno\nniño"));
    text_buffer_set_cursor(tb, 0, 1);

    RetroKeyEvent shift_down = key_modified(
        RETRO_KEY_DOWN, RETRO_MOD_SHIFT);
    TEST_REQUIRE(text_buffer_handle_key(tb, &shift_down));
    TEST_REQUIRE(text_buffer_has_selection(tb));

    size_t length = 0;
    char *selected = text_buffer_selected_text(tb, &length);
    TEST_REQUIRE(selected != NULL);
    TEST_REQUIRE(strcmp(selected, "no\nn") == 0);
    TEST_REQUIRE(length == 4);
    free(selected);

    TEST_REQUIRE(text_buffer_delete_selection(tb));
    TEST_REQUIRE(text_buffer_line_count(tb) == 1);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "uiño") == 0);
    TEST_REQUIRE(text_buffer_cursor_row(tb) == 0);
    TEST_REQUIRE(text_buffer_cursor_col(tb) == 1);
    TEST_REQUIRE(!text_buffer_has_selection(tb));

    text_buffer_destroy(tb);
    printf("  PASS: multiline_selection_delete\n");
}

static void test_select_all_and_atomic_insert(void) {
    TextBuffer *tb = text_buffer_create();
    TEST_REQUIRE(text_buffer_set_text(tb, "abc\ndef"));
    text_buffer_select_all(tb);
    TEST_REQUIRE(text_buffer_has_selection(tb));

    size_t length = 0;
    char *selected = text_buffer_selected_text(tb, &length);
    TEST_REQUIRE(selected != NULL);
    TEST_REQUIRE(length == 7);
    TEST_REQUIRE(strcmp(selected, "abc\ndef") == 0);
    free(selected);

    const char *replacement = "diseños";
    TEST_REQUIRE(text_buffer_insert_text(tb, replacement,
                                         strlen(replacement)));
    TEST_REQUIRE(text_buffer_line_count(tb) == 1);
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), replacement) == 0);
    TEST_REQUIRE(!text_buffer_has_selection(tb));

    text_buffer_destroy(tb);
    printf("  PASS: select_all_and_atomic_insert\n");
}

static void test_collapsed_shift_selection_stays_collapsed(void) {
    TextBuffer *tb = text_buffer_create();
    RetroKeyEvent shift_left = key_modified(
        RETRO_KEY_LEFT, RETRO_MOD_SHIFT);
    TEST_REQUIRE(text_buffer_handle_key(tb, &shift_left));
    TEST_REQUIRE(!text_buffer_has_selection(tb));

    RetroKeyEvent letter = key_char('a');
    TEST_REQUIRE(text_buffer_handle_key(tb, &letter));
    TEST_REQUIRE(strcmp(text_buffer_line(tb, 0), "a") == 0);
    TEST_REQUIRE(!text_buffer_has_selection(tb));

    text_buffer_destroy(tb);
    printf("  PASS: collapsed_shift_selection_stays_collapsed\n");
}

static void test_selection_render_overlay(void) {
    TextBuffer *tb = text_buffer_create();
    DrawList *list = draw_list_create();
    TEST_REQUIRE(tb != NULL);
    TEST_REQUIRE(list != NULL);
    TEST_REQUIRE(text_buffer_set_text(tb, "niño"));
    text_buffer_set_cursor(tb, 0, 2);

    RetroKeyEvent shift_right = key_modified(
        RETRO_KEY_RIGHT, RETRO_MOD_SHIFT);
    TEST_REQUIRE(text_buffer_handle_key(tb, &shift_right));

    RenderStyle style = {
        RENDER_COLOR_WHITE, RENDER_COLOR_BLACK, false, false
    };
    RenderStyle cursor = {
        RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, true, false
    };
    RenderStyle selection = {
        .fg = RENDER_COLOR_BLACK,
        .bg = RENDER_COLOR_CYAN,
        .reverse = true,
        .bold = false,
    };
    text_buffer_render_with_selection(
        tb, list, 0, 0, 1, 6, &style, &cursor, &selection);

    TEST_REQUIRE(draw_list_count(list) == 3);
    DrawCommandView command = {0};
    TEST_REQUIRE(draw_list_get(list, 1, &command));
    TEST_REQUIRE(command.x == 2);
    TEST_REQUIRE(strcmp(command.text, "ñ") == 0);
    TEST_REQUIRE(command.style.reverse);
    TEST_REQUIRE(draw_list_get(list, 2, &command));
    TEST_REQUIRE(command.x == 3);
    TEST_REQUIRE(strcmp(command.text, "o") == 0);

    draw_list_destroy(list);
    text_buffer_destroy(tb);
    printf("  PASS: selection_render_overlay\n");
}

static void test_utf8_find_next_and_wrap(void) {
    TextBuffer *tb = text_buffer_create();
    TEST_REQUIRE(tb != NULL);
    TEST_REQUIRE(text_buffer_set_text(tb, "Árbol\nniño árbol\nÁRBOL"));

    const char *query = "árbol";
    TextBufferMatch match = {0};
    TEST_REQUIRE(text_buffer_find_next(tb, query, strlen(query), true,
                                       0, 0, true, &match));
    TEST_REQUIRE(match.row == 0);
    TEST_REQUIRE(match.start_col == 0);
    TEST_REQUIRE(match.end_col == 6);

    TEST_REQUIRE(text_buffer_find_next(tb, query, strlen(query), true,
                                       match.row, match.end_col, true, &match));
    TEST_REQUIRE(match.row == 1);
    TEST_REQUIRE(match.start_col == 6);
    TEST_REQUIRE(match.end_col == 12);

    TEST_REQUIRE(text_buffer_find_next(tb, query, strlen(query), true,
                                       match.row, match.end_col, true, &match));
    TEST_REQUIRE(match.row == 2);
    TEST_REQUIRE(match.start_col == 0);
    TEST_REQUIRE(match.end_col == 6);

    TEST_REQUIRE(text_buffer_find_next(tb, query, strlen(query), true,
                                       match.row, match.end_col, true, &match));
    TEST_REQUIRE(match.row == 0);
    TEST_REQUIRE(text_buffer_select_match(tb, &match));
    size_t selected_length = 0;
    char *selected = text_buffer_selected_text(tb, &selected_length);
    TEST_REQUIRE(selected != NULL);
    TEST_REQUIRE(selected_length == 6);
    TEST_REQUIRE(strcmp(selected, "Árbol") == 0);
    free(selected);

    TEST_REQUIRE(text_buffer_find_next(tb, query, strlen(query), false,
                                       0, 0, true, &match));
    TEST_REQUIRE(match.row == 1);
    TEST_REQUIRE(match.start_col == 6);

    const char invalid[] = {(char)0xC3};
    TEST_REQUIRE(!text_buffer_find_next(tb, "", 0, true,
                                        0, 0, true, &match));
    TEST_REQUIRE(!text_buffer_find_next(tb, invalid, sizeof(invalid), true,
                                        0, 0, true, &match));
    TEST_REQUIRE(!text_buffer_find_next(tb, "missing", 7, true,
                                        0, 0, true, &match));

    text_buffer_destroy(tb);
    printf("  PASS: utf8_find_next_and_wrap\n");
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
    TextBufferMatch match = {0};
    TEST_REQUIRE(!text_buffer_find_next(NULL, "x", 1, true,
                                        0, 0, true, &match));
    TEST_REQUIRE(!text_buffer_select_match(NULL, &match));
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
    test_utf8_insert_and_roundtrip();
    test_utf8_navigation_uses_codepoint_boundaries();
    test_utf8_backspace_and_delete_remove_whole_codepoints();
    test_utf8_vertical_navigation_preserves_display_column();
    test_utf8_invalid_text_is_rejected_without_data_loss();
    test_utf8_render_uses_cell_columns();
    test_utf8_shift_selection_and_replacement();
    test_multiline_selection_delete();
    test_select_all_and_atomic_insert();
    test_collapsed_shift_selection_stays_collapsed();
    test_selection_render_overlay();
    test_utf8_find_next_and_wrap();
    test_null_safety();
    test_render_basic();
    printf("All text_buffer tests passed.\n");
    return 0;
}
