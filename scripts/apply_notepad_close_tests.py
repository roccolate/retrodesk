#!/usr/bin/env python3
"""Add permanent runtime coverage for the Notepad close-confirmation flow."""

from pathlib import Path


path = Path("tests/desktop_runtime_test.c")
text = path.read_text(encoding="utf-8")


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one marker, found {count}")
    text = text.replace(old, new, 1)


common_tests = r'''static RetroAppInstance create_untitled_notepad(void) {
    const RetroAppDescriptor *desc = notepad_app_descriptor();
    TEST_REQUIRE(desc != NULL);
    RetroAppContext ctx = {
        .desktop = NULL,
        .theme = retro_theme_get(RETRO_THEME_XP),
        .resource_path = NULL,
    };
    RetroAppInstance instance = {.descriptor = desc, .ctx = ctx};
    TEST_REQUIRE(desc->create(&instance, &ctx));
    return instance;
}

static void type_notepad_char(RetroAppInstance *instance, char ch) {
    RetroEvent event = key_event((unsigned char)ch, ch);
    app_handle_event(instance, &event);
}

static void test_notepad_escape_does_not_close(void) {
    RetroAppInstance instance = create_untitled_notepad();
    RetroEvent escape = key_event(RETRO_KEY_ESC, '\0');
    app_handle_event(&instance, &escape);
    TEST_REQUIRE(!app_is_close_requested(&instance));
    TEST_REQUIRE(!notepad_close_prompt_for_test(&instance));
    instance.descriptor->destroy(&instance);
}

static void test_notepad_close_cancel_and_discard(void) {
    RetroAppInstance instance = create_untitled_notepad();
    type_notepad_char(&instance, 'x');
    TEST_REQUIRE(notepad_dirty_for_test(&instance));

    TEST_REQUIRE(!instance.descriptor->can_close(&instance));
    TEST_REQUIRE(notepad_close_prompt_for_test(&instance));

    RetroEvent escape = key_event(RETRO_KEY_ESC, '\0');
    app_handle_event(&instance, &escape);
    TEST_REQUIRE(!notepad_close_prompt_for_test(&instance));
    TEST_REQUIRE(notepad_dirty_for_test(&instance));
    TEST_REQUIRE(!app_is_close_requested(&instance));

    TEST_REQUIRE(!instance.descriptor->can_close(&instance));
    RetroEvent discard = key_event('d', 'd');
    app_handle_event(&instance, &discard);
    TEST_REQUIRE(!notepad_close_prompt_for_test(&instance));
    TEST_REQUIRE(!notepad_dirty_for_test(&instance));
    TEST_REQUIRE(app_is_close_requested(&instance));
    TEST_REQUIRE(instance.descriptor->can_close(&instance));

    instance.descriptor->destroy(&instance);
}

static void test_notepad_untitled_save_routes_to_save_as(void) {
    RetroAppInstance instance = create_untitled_notepad();
    type_notepad_char(&instance, 'x');

    TEST_REQUIRE(!instance.descriptor->can_close(&instance));
    RetroEvent save = key_event('s', 's');
    app_handle_event(&instance, &save);
    TEST_REQUIRE(!notepad_close_prompt_for_test(&instance));
    TEST_REQUIRE(notepad_save_as_for_test(&instance));
    TEST_REQUIRE(notepad_close_after_save_for_test(&instance));
    TEST_REQUIRE(!app_is_close_requested(&instance));

    RetroEvent escape = key_event(RETRO_KEY_ESC, '\0');
    app_handle_event(&instance, &escape);
    TEST_REQUIRE(!notepad_save_as_for_test(&instance));
    TEST_REQUIRE(!notepad_close_after_save_for_test(&instance));
    TEST_REQUIRE(notepad_dirty_for_test(&instance));
    TEST_REQUIRE(!app_is_close_requested(&instance));

    instance.descriptor->destroy(&instance);
}

'''

if "test_notepad_close_cancel_and_discard" not in text:
    marker = "#if !defined(_WIN32)\nstatic void create_fixture_file"
    replace_once(
        marker,
        common_tests + marker,
        "cross-platform Notepad tests",
    )


posix_test = r'''static void test_notepad_save_before_close(void) {
    char path_template[] = "/tmp/retrodesk-notepad-close-XXXXXX";
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
    TEST_REQUIRE(notepad_dirty_for_test(&instance));
    TEST_REQUIRE(!desc->can_close(&instance));
    TEST_REQUIRE(notepad_close_prompt_for_test(&instance));

    RetroEvent save = key_event('s', 's');
    app_handle_event(&instance, &save);
    TEST_REQUIRE(app_is_close_requested(&instance));
    TEST_REQUIRE(!notepad_dirty_for_test(&instance));
    TEST_REQUIRE(desc->can_close(&instance));

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

if "test_notepad_save_before_close" not in text:
    replace_once(
        "static void test_filemanager_navigation_port(void) {",
        posix_test + "static void test_filemanager_navigation_port(void) {",
        "POSIX Notepad save test",
    )


old_main = '''int main(void) {
    test_capability_rejection();
    test_failed_create_calls_destroy();
    test_launch_and_clean_close();
    test_repeat_create_run_shutdown();
#if !defined(_WIN32)
    test_filemanager_navigation_port();
#endif
    return 0;
}
'''
new_main = '''int main(void) {
    test_capability_rejection();
    test_failed_create_calls_destroy();
    test_launch_and_clean_close();
    test_repeat_create_run_shutdown();
    test_notepad_escape_does_not_close();
    test_notepad_close_cancel_and_discard();
    test_notepad_untitled_save_routes_to_save_as();
#if !defined(_WIN32)
    test_notepad_save_before_close();
    test_filemanager_navigation_port();
#endif
    return 0;
}
'''

if "test_notepad_save_before_close();" not in text:
    replace_once(old_main, new_main, "test main")

path.write_text(text, encoding="utf-8")
