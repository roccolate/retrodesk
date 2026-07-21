#!/usr/bin/env python3
"""Add permanent runtime coverage for Notepad undo/redo history."""

from pathlib import Path

path = Path("tests/desktop_runtime_test.c")
text = path.read_text(encoding="utf-8")


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one marker, found {count}")
    text = text.replace(old, new, 1)


unicode_helper = r'''static void type_notepad_codepoint(RetroAppInstance *instance,
                                    unsigned int codepoint) {
    RetroEvent event = {0};
    event.type = RETRO_EVENT_KEY;
    event.data.key.key_code = (int)codepoint;
    event.data.key.is_printable = true;
    event.data.key.text_codepoint = codepoint;
    app_handle_event(instance, &event);
}

'''
if "static void type_notepad_codepoint" not in text:
    marker = '''static void test_notepad_escape_does_not_close(void) {
'''
    replace_once(marker, unicode_helper + marker, "Unicode Notepad helper")


cross_platform_tests = r'''static void test_notepad_undo_redo_utf8(void) {
    RetroAppInstance instance = create_untitled_notepad();
    type_notepad_char(&instance, 'A');
    type_notepad_codepoint(&instance, 0x00F1u);

    TEST_REQUIRE(strcmp(notepad_line_for_test(&instance, 0), "Añ") == 0);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 3);
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) == 2);
    TEST_REQUIRE(notepad_redo_count_for_test(&instance) == 0);
    TEST_REQUIRE(notepad_dirty_for_test(&instance));

    RetroEvent undo = key_event(RETRO_KEY_CTRL_Z, '\0');
    RetroEvent redo = key_event(RETRO_KEY_CTRL_Y, '\0');

    app_handle_event(&instance, &undo);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&instance, 0), "A") == 0);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 1);
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) == 1);
    TEST_REQUIRE(notepad_redo_count_for_test(&instance) == 1);
    TEST_REQUIRE(notepad_dirty_for_test(&instance));

    app_handle_event(&instance, &undo);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&instance, 0), "") == 0);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 0);
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) == 0);
    TEST_REQUIRE(notepad_redo_count_for_test(&instance) == 2);
    TEST_REQUIRE(!notepad_dirty_for_test(&instance));

    app_handle_event(&instance, &redo);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&instance, 0), "A") == 0);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 1);
    TEST_REQUIRE(notepad_dirty_for_test(&instance));

    app_handle_event(&instance, &redo);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&instance, 0), "Añ") == 0);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 3);
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) == 2);
    TEST_REQUIRE(notepad_redo_count_for_test(&instance) == 0);

    app_handle_event(&instance, &undo);
    type_notepad_char(&instance, 'x');
    TEST_REQUIRE(strcmp(notepad_line_for_test(&instance, 0), "Ax") == 0);
    TEST_REQUIRE(notepad_redo_count_for_test(&instance) == 0);
    app_handle_event(&instance, &redo);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&instance, 0), "Ax") == 0);

    instance.descriptor->destroy(&instance);
}

static void test_notepad_history_limit_and_noop(void) {
    RetroAppInstance instance = create_untitled_notepad();
    RetroEvent backspace = key_event(RETRO_KEY_BS, '\0');
    app_handle_event(&instance, &backspace);
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) == 0);
    TEST_REQUIRE(!notepad_dirty_for_test(&instance));

    for (int i = 0; i < 105; ++i) type_notepad_char(&instance, 'a');
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) == 100);

    RetroEvent undo = key_event(RETRO_KEY_CTRL_Z, '\0');
    for (int i = 0; i < 100; ++i) app_handle_event(&instance, &undo);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&instance, 0), "aaaaa") == 0);
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) == 0);
    TEST_REQUIRE(notepad_redo_count_for_test(&instance) == 100);

    app_handle_event(&instance, &undo);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&instance, 0), "aaaaa") == 0);

    type_notepad_char(&instance, 'b');
    TEST_REQUIRE(strcmp(notepad_line_for_test(&instance, 0), "aaaaab") == 0);
    TEST_REQUIRE(notepad_redo_count_for_test(&instance) == 0);

    instance.descriptor->destroy(&instance);
}

'''
if "test_notepad_undo_redo_utf8" not in text:
    marker = "#if !defined(_WIN32)\nstatic void create_fixture_file"
    replace_once(
        marker,
        cross_platform_tests + marker,
        "cross-platform Notepad history tests",
    )


posix_test = r'''static void test_notepad_saved_baseline_tracks_undo(void) {
    char path_template[] = "/tmp/retrodesk-notepad-history-XXXXXX";
    int fd = mkstemp(path_template);
    TEST_REQUIRE(fd >= 0);
    FILE *stream = fdopen(fd, "w");
    TEST_REQUIRE(stream != NULL);
    TEST_REQUIRE(fputs("base", stream) >= 0);
    TEST_REQUIRE(fclose(stream) == 0);

    const RetroAppDescriptor *desc = notepad_app_descriptor();
    TEST_REQUIRE(desc != NULL);
    RetroAppContext ctx = {
        .desktop = NULL,
        .theme = retro_theme_get(RETRO_THEME_XP),
        .resource_path = path_template,
    };
    RetroAppInstance instance = {.descriptor = desc, .ctx = ctx};
    TEST_REQUIRE(desc->create(&instance, &ctx));

    type_notepad_char(&instance, 'x');
    TEST_REQUIRE(strcmp(notepad_line_for_test(&instance, 0), "basex") == 0);
    TEST_REQUIRE(notepad_dirty_for_test(&instance));

    RetroEvent save = key_event(RETRO_KEY_CTRL_S, '\0');
    app_handle_event(&instance, &save);
    TEST_REQUIRE(!notepad_dirty_for_test(&instance));

    RetroEvent undo = key_event(RETRO_KEY_CTRL_Z, '\0');
    app_handle_event(&instance, &undo);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&instance, 0), "base") == 0);
    TEST_REQUIRE(notepad_dirty_for_test(&instance));

    RetroEvent redo = key_event(RETRO_KEY_CTRL_Y, '\0');
    app_handle_event(&instance, &redo);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&instance, 0), "basex") == 0);
    TEST_REQUIRE(!notepad_dirty_for_test(&instance));

    stream = fopen(path_template, "r");
    TEST_REQUIRE(stream != NULL);
    char content[16] = {0};
    TEST_REQUIRE(fread(content, 1, sizeof(content) - 1, stream) == 5);
    TEST_REQUIRE(fclose(stream) == 0);
    TEST_REQUIRE(strcmp(content, "basex") == 0);

    desc->destroy(&instance);
    TEST_REQUIRE(unlink(path_template) == 0);
}

'''
if "test_notepad_saved_baseline_tracks_undo" not in text:
    marker = "static void test_notepad_save_before_close(void) {"
    replace_once(
        marker,
        posix_test + marker,
        "POSIX saved-baseline history test",
    )


old_main = '''    test_notepad_escape_does_not_close();
    test_notepad_close_cancel_and_discard();
    test_notepad_untitled_save_routes_to_save_as();
#if !defined(_WIN32)
    test_notepad_save_before_close();
'''
new_main = '''    test_notepad_escape_does_not_close();
    test_notepad_close_cancel_and_discard();
    test_notepad_untitled_save_routes_to_save_as();
    test_notepad_undo_redo_utf8();
    test_notepad_history_limit_and_noop();
#if !defined(_WIN32)
    test_notepad_saved_baseline_tracks_undo();
    test_notepad_save_before_close();
'''
if "test_notepad_saved_baseline_tracks_undo();" not in text:
    replace_once(old_main, new_main, "Notepad history test calls")

path.write_text(text, encoding="utf-8")
