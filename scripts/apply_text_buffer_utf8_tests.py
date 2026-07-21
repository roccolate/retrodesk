#!/usr/bin/env python3
"""Add UTF-8 regression tests to the TextBuffer test suite."""

from pathlib import Path

path = Path("tests/text_buffer_test.c")
text = path.read_text(encoding="utf-8")


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one marker, found {count}")
    text = text.replace(old, new, 1)


helper = r'''static RetroKeyEvent key_codepoint(uint32_t codepoint) {
    RetroKeyEvent key = {0};
    key.key_code = (int)codepoint;
    key.is_printable = true;
    key.text_codepoint = codepoint;
    return key;
}

'''
if "static RetroKeyEvent key_codepoint" not in text:
    replace_once(
        "static void test_create_destroy(void) {",
        helper + "static void test_create_destroy(void) {",
        "Unicode key helper",
    )


tests = r'''static void test_utf8_insert_and_roundtrip(void) {
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

'''
if "test_utf8_insert_and_roundtrip" not in text:
    replace_once(
        "static void test_null_safety(void) {",
        tests + "static void test_null_safety(void) {",
        "UTF-8 TextBuffer tests",
    )

old_main_tail = '''    test_line_capacity_growth();
    test_null_safety();
    test_render_basic();
'''
new_main_tail = '''    test_line_capacity_growth();
    test_utf8_insert_and_roundtrip();
    test_utf8_navigation_uses_codepoint_boundaries();
    test_utf8_backspace_and_delete_remove_whole_codepoints();
    test_utf8_vertical_navigation_preserves_display_column();
    test_utf8_invalid_text_is_rejected_without_data_loss();
    test_utf8_render_uses_cell_columns();
    test_null_safety();
    test_render_basic();
'''
if "test_utf8_render_uses_cell_columns();" not in text:
    replace_once(old_main_tail, new_main_tail, "UTF-8 test calls")

path.write_text(text, encoding="utf-8")
