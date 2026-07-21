#define RETRODESK_ENABLE_TEST_HOOKS

#include "test_harness.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "app/app_runtime.h"
#include "apps/apps_internal.h"
#include "core/desktop.h"
#include "core/key_chord.h"
#include "platform/platform.h"
#include "ui/theme.h"

struct PlatformBackend {
    PlatformFeatures features;
    const char *name;
    const RetroEvent *events;
    size_t event_count;
    size_t event_index;
};

static int g_failing_create_calls;
static int g_failing_destroy_calls;
static int g_failing_state_marker;

static void platform_stub_update_mask(PlatformFeatures *features) {
    if (!features) return;
    unsigned int mask = 0;
    if (features->keyboard_basic) mask |= PLATFORM_CAP_KEYBOARD_BASIC;
    if (features->mouse_basic) mask |= PLATFORM_CAP_MOUSE_BASIC;
    if (features->drag_reliable) mask |= PLATFORM_CAP_DRAG_RELIABLE;
    if (features->resize_events) mask |= PLATFORM_CAP_RESIZE;
    if (features->color) mask |= PLATFORM_CAP_COLOR;
    if (features->unicode_basic) mask |= PLATFORM_CAP_UNICODE_BASIC;
    if (features->double_click) mask |= PLATFORM_CAP_DOUBLE_CLICK;
    if (features->right_click) mask |= PLATFORM_CAP_RIGHT_CLICK;
    features->capability_mask = mask;
}

static PlatformBackend *platform_stub_new(bool keyboard_basic) {
    PlatformBackend *platform = calloc(1, sizeof(*platform));
    if (!platform) return NULL;
    platform->name = "test-stub";
    platform->features.input_backend = INPUT_BACKEND_NCURSES;
    platform->features.keyboard_basic = keyboard_basic;
    platform->features.mouse_basic = false;
    platform->features.drag_reliable = false;
    platform->features.resize_events = false;
    platform->features.color = true;
    platform->features.unicode_basic = true;
    platform->features.double_click = false;
    platform->features.right_click = false;
    platform->features.linux_tty_keyboard_only = false;
    platform_stub_update_mask(&platform->features);
    return platform;
}

static void platform_stub_set_events(PlatformBackend *platform, const RetroEvent *events,
                                     size_t event_count) {
    if (!platform) return;
    platform->events = events;
    platform->event_count = event_count;
    platform->event_index = 0;
}

static RetroEvent key_event(int key_code, char ascii) {
    RetroEvent event = {0};
    event.type = RETRO_EVENT_KEY;
    event.data.key.key_code = key_code;
    event.data.key.is_printable = (ascii >= 32 && ascii <= 126);
    event.data.key.ascii = event.data.key.is_printable ? (unsigned char)ascii : 0;
    return event;
}

static bool failing_create(RetroAppInstance *instance, const RetroAppContext *ctx) {
    TEST_REQUIRE(instance != NULL);
    TEST_REQUIRE(ctx != NULL);
    g_failing_create_calls++;
    instance->state = &g_failing_state_marker;
    return false;
}

static void failing_destroy(RetroAppInstance *instance) {
    TEST_REQUIRE(instance != NULL);
    TEST_REQUIRE(instance->state == &g_failing_state_marker);
    g_failing_destroy_calls++;
    instance->state = NULL;
}

static const RetroAppDescriptor k_failing_create_descriptor = {
    .app_id = "test-failing-create",
    .display_name = "Failing Create",
    .required_capabilities = 0,
    .default_height = 6,
    .default_width = 24,
    .default_y = 2,
    .default_x = 4,
    .window_flags = WINDOW_FLAG_APP_OWNED,
    .create = failing_create,
    .on_event = NULL,
    .on_render = NULL,
    .destroy = failing_destroy,
};

RetroPollResult platform_poll_event(PlatformBackend *platform, RetroEvent *out_event,
                                    int timeout_ms) {
    (void)timeout_ms;
    if (!platform || !out_event) return RETRO_POLL_ERROR;
    if (platform->event_index >= platform->event_count) return RETRO_POLL_TIMEOUT;
    *out_event = platform->events[platform->event_index++];
    return RETRO_POLL_EVENT;
}

const PlatformFeatures *platform_features(const PlatformBackend *platform) {
    if (!platform) return NULL;
    return &platform->features;
}

const char *platform_backend_name(const PlatformBackend *platform) {
    if (!platform || !platform->name) return "test-stub";
    return platform->name;
}

void platform_destroy(PlatformBackend *platform) {
    free(platform);
}

PlatformBackend *platform_create(const PlatformConfig *config) {
    (void)config;
    return NULL;
}

static void test_capability_rejection(void) {
    PlatformBackend *platform = platform_stub_new(false);
    TEST_REQUIRE(platform != NULL);

    DesktopConfig cfg = {
        .platform = platform,
        .input_timeout_ms = 20,
        .bench_mode = true,
        .render_backend = RENDER_BACKEND_CURSES,
        .theme_kind = RETRO_THEME_XP,
    };
    Desktop *desktop = desktop_create(&cfg);
    TEST_REQUIRE(desktop != NULL);

    /* Built-in apps require keyboard capability and should be rejected. */
    TEST_REQUIRE(desktop_app_count(desktop) == 0);
    TEST_REQUIRE(app_launch(desktop, "filemanager") == NULL);
    TEST_REQUIRE(app_launch(desktop, "diagnostics") == NULL);

    desktop_shutdown(desktop);
    platform_destroy(platform);
}

static void test_failed_create_calls_destroy(void) {
    PlatformBackend *platform = platform_stub_new(true);
    TEST_REQUIRE(platform != NULL);

    DesktopConfig cfg = {
        .platform = platform,
        .input_timeout_ms = 20,
        .bench_mode = true,
        .render_backend = RENDER_BACKEND_CURSES,
        .theme_kind = RETRO_THEME_XP,
    };
    Desktop *desktop = desktop_create(&cfg);
    TEST_REQUIRE(desktop != NULL);

    size_t app_count_before = desktop_app_count(desktop);
    size_t window_count_before = desktop_window_count(desktop);
    g_failing_create_calls = 0;
    g_failing_destroy_calls = 0;
    g_failing_state_marker = 42;

    TEST_REQUIRE(desktop_register_app_for_test(desktop, &k_failing_create_descriptor));
    TEST_REQUIRE(app_launch(desktop, "test-failing-create") == NULL);
    TEST_REQUIRE(g_failing_create_calls == 1);
    TEST_REQUIRE(g_failing_destroy_calls == 1);
    TEST_REQUIRE(desktop_app_count(desktop) == app_count_before);
    TEST_REQUIRE(desktop_window_count(desktop) == window_count_before);
    TEST_REQUIRE(desktop_app_window_id(desktop, "test-failing-create") ==
                 WINDOW_ID_INVALID);

    desktop_shutdown(desktop);
    platform_destroy(platform);
}

static void test_launch_and_clean_close(void) {
    PlatformBackend *platform = platform_stub_new(true);
    TEST_REQUIRE(platform != NULL);

    DesktopConfig cfg = {
        .platform = platform,
        .input_timeout_ms = 20,
        .bench_mode = false,
        .render_backend = RENDER_BACKEND_CURSES,
        .theme_kind = RETRO_THEME_XP,
    };
    Desktop *desktop = desktop_create(&cfg);
    TEST_REQUIRE(desktop != NULL);

    size_t base_apps = desktop_app_count(desktop);
    size_t base_windows = desktop_window_count(desktop);
    TEST_REQUIRE(base_apps >= 1);
    TEST_REQUIRE(base_windows >= base_apps + 1); /* + shell window */

    RetroAppInstance *diagnostics = app_launch(desktop, "diagnostics");
    TEST_REQUIRE(diagnostics != NULL);
    TEST_REQUIRE(desktop_app_count(desktop) == base_apps + 1);
    TEST_REQUIRE(desktop_window_count(desktop) == base_windows + 1);

    app_request_close(diagnostics);
    const RetroEvent events[] = {key_event(RETRO_KEY_CTRL_Q, '\0')};
    platform_stub_set_events(platform, events, 1);
    TEST_REQUIRE(desktop_run(desktop) == EXIT_SUCCESS);

    /* Close request should have removed terminal app/window before exit. */
    TEST_REQUIRE(desktop_app_count(desktop) == base_apps);
    TEST_REQUIRE(desktop_window_count(desktop) == base_windows);

    desktop_shutdown(desktop);
    platform_destroy(platform);
}

static void test_repeat_create_run_shutdown(void) {
    for (int i = 0; i < 3; ++i) {
        PlatformBackend *platform = platform_stub_new(true);
        TEST_REQUIRE(platform != NULL);
        const RetroEvent events[] = {key_event(RETRO_KEY_CTRL_Q, '\0')};
        platform_stub_set_events(platform, events, 1);

        DesktopConfig cfg = {
            .platform = platform,
            .input_timeout_ms = 20,
            .bench_mode = false,
            .render_backend = RENDER_BACKEND_CURSES,
            .theme_kind = RETRO_THEME_XP,
        };
        Desktop *desktop = desktop_create(&cfg);
        TEST_REQUIRE(desktop != NULL);
        TEST_REQUIRE(desktop_run(desktop) == EXIT_SUCCESS);
        desktop_shutdown(desktop);
        platform_destroy(platform);
    }
}

static RetroAppInstance create_untitled_notepad(void) {
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

static void type_notepad_codepoint(RetroAppInstance *instance,
                                    unsigned int codepoint) {
    RetroEvent event = {0};
    event.type = RETRO_EVENT_KEY;
    event.data.key.key_code = (int)codepoint;
    event.data.key.is_printable = true;
    event.data.key.text_codepoint = codepoint;
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

static void test_notepad_undo_redo_utf8(void) {
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

#if !defined(_WIN32)
static void create_fixture_file(const char *directory, const char *name) {
    char path[512];
    int written = snprintf(path, sizeof(path), "%s/%s", directory, name);
    TEST_REQUIRE(written > 0 && (size_t)written < sizeof(path));
    FILE *stream = fopen(path, "w");
    TEST_REQUIRE(stream != NULL);
    TEST_REQUIRE(fputs("fixture\n", stream) >= 0);
    TEST_REQUIRE(fclose(stream) == 0);
}

static void remove_fixture_file(const char *directory, const char *name) {
    char path[512];
    int written = snprintf(path, sizeof(path), "%s/%s", directory, name);
    TEST_REQUIRE(written > 0 && (size_t)written < sizeof(path));
    TEST_REQUIRE(unlink(path) == 0);
}

static void fixture_path(char *out, size_t out_size, const char *directory,
                         const char *name) {
    int written = snprintf(out, out_size, "%s/%s", directory, name);
    TEST_REQUIRE(written > 0 && (size_t)written < out_size);
}

static void send_text(RetroAppInstance *instance, const char *text) {
    TEST_REQUIRE(instance != NULL);
    TEST_REQUIRE(text != NULL);
    for (const char *cursor = text; *cursor; ++cursor) {
        RetroEvent event = key_event((unsigned char)*cursor, *cursor);
        app_handle_event(instance, &event);
    }
}

static void submit_prompt(RetroAppInstance *instance) {
    RetroEvent enter = key_event(RETRO_KEY_CR, '\0');
    app_handle_event(instance, &enter);
}

static void populate_filemanager_fixture(const char *root, char *folder,
                                         size_t folder_size) {
    int folder_written = snprintf(folder, folder_size, "%s/folder", root);
    TEST_REQUIRE(folder_written > 0 && (size_t)folder_written < folder_size);
    TEST_REQUIRE(mkdir(folder, 0700) == 0);
    for (int i = 0; i < 18; ++i) {
        char name[32];
        int name_written = snprintf(name, sizeof(name), "file%02d.txt", i);
        TEST_REQUIRE(name_written > 0 && (size_t)name_written < sizeof(name));
        create_fixture_file(root, name);
    }
    create_fixture_file(root, ".hidden");
}

static void cleanup_filemanager_fixture(const char *root, const char *folder) {
    remove_fixture_file(root, ".hidden");
    for (int i = 0; i < 18; ++i) {
        char name[32];
        int name_written = snprintf(name, sizeof(name), "file%02d.txt", i);
        TEST_REQUIRE(name_written > 0 && (size_t)name_written < sizeof(name));
        remove_fixture_file(root, name);
    }
    TEST_REQUIRE(rmdir(folder) == 0);
    TEST_REQUIRE(rmdir(root) == 0);
}

static void test_notepad_saved_baseline_tracks_undo(void) {
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

static void test_notepad_save_before_close(void) {
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

static void test_filemanager_navigation_port(void) {
    char root_template[] = "/tmp/retrodesk-filemanager-XXXXXX";
    char *root = mkdtemp(root_template);
    TEST_REQUIRE(root != NULL);
    char folder[512];
    populate_filemanager_fixture(root, folder, sizeof(folder));

    const RetroAppDescriptor *desc = filemanager_app_descriptor();
    TEST_REQUIRE(desc != NULL);
    RetroAppContext ctx = {
        .desktop = NULL,
        .theme = retro_theme_get(RETRO_THEME_XP),
        .resource_path = root,
    };
    RetroAppInstance instance = {.descriptor = desc, .ctx = ctx};
    TEST_REQUIRE(desc->create(&instance, &ctx));

    TEST_REQUIRE(filemanager_item_count_for_test(&instance) == 20);
    TEST_REQUIRE(!filemanager_show_hidden_for_test(&instance));
    TEST_REQUIRE(!filemanager_has_item_for_test(&instance, ".hidden"));
    TEST_REQUIRE(strcmp(filemanager_selected_name_for_test(&instance), "..") == 0);

    RetroEvent page_down = key_event(RETRO_KEY_NPAGE, '\0');
    app_handle_event(&instance, &page_down);
    TEST_REQUIRE(strcmp(filemanager_selected_name_for_test(&instance), "file08.txt") == 0);
    app_handle_event(&instance, &page_down);
    TEST_REQUIRE(strcmp(filemanager_selected_name_for_test(&instance), "file17.txt") == 0);
    TEST_REQUIRE(filemanager_scroll_offset_for_test(&instance) == 8);

    RetroEvent refresh = key_event(RETRO_KEY_F5, '\0');
    app_handle_event(&instance, &refresh);
    TEST_REQUIRE(strcmp(filemanager_selected_name_for_test(&instance), "file17.txt") == 0);
    TEST_REQUIRE(filemanager_scroll_offset_for_test(&instance) == 8);

    RetroEvent hidden = key_event('h', 'h');
    app_handle_event(&instance, &hidden);
    TEST_REQUIRE(filemanager_show_hidden_for_test(&instance));
    TEST_REQUIRE(filemanager_item_count_for_test(&instance) == 21);
    TEST_REQUIRE(filemanager_has_item_for_test(&instance, ".hidden"));
    TEST_REQUIRE(strcmp(filemanager_selected_name_for_test(&instance), "file17.txt") == 0);
    TEST_REQUIRE(filemanager_scroll_offset_for_test(&instance) == 9);

    app_handle_event(&instance, &hidden);
    TEST_REQUIRE(!filemanager_show_hidden_for_test(&instance));
    TEST_REQUIRE(filemanager_item_count_for_test(&instance) == 20);
    TEST_REQUIRE(strcmp(filemanager_selected_name_for_test(&instance), "file17.txt") == 0);

    RetroEvent new_file = key_event(RETRO_KEY_F8, '\0');
    app_handle_event(&instance, &new_file);
    send_text(&instance, "created.txt");
    submit_prompt(&instance);
    TEST_REQUIRE(filemanager_item_count_for_test(&instance) == 21);
    TEST_REQUIRE(filemanager_has_item_for_test(&instance, "created.txt"));
    TEST_REQUIRE(strcmp(filemanager_selected_name_for_test(&instance), "created.txt") == 0);

    char created_path[512];
    fixture_path(created_path, sizeof(created_path), root, "created.txt");
    struct stat created_stat;
    TEST_REQUIRE(lstat(created_path, &created_stat) == 0);
    TEST_REQUIRE(S_ISREG(created_stat.st_mode));
    TEST_REQUIRE(created_stat.st_size == 0);

    RetroEvent new_directory = key_event(RETRO_KEY_F7, '\0');
    app_handle_event(&instance, &new_directory);
    send_text(&instance, "new-folder");
    submit_prompt(&instance);
    TEST_REQUIRE(filemanager_item_count_for_test(&instance) == 22);
    TEST_REQUIRE(filemanager_has_item_for_test(&instance, "new-folder"));
    TEST_REQUIRE(strcmp(filemanager_selected_name_for_test(&instance), "new-folder") == 0);

    char new_folder_path[512];
    fixture_path(new_folder_path, sizeof(new_folder_path), root, "new-folder");
    struct stat folder_stat;
    TEST_REQUIRE(lstat(new_folder_path, &folder_stat) == 0);
    TEST_REQUIRE(S_ISDIR(folder_stat.st_mode));

    RetroEvent rename = key_event(RETRO_KEY_F2, '\0');
    app_handle_event(&instance, &rename);
    RetroEvent clear_name = key_event(RETRO_KEY_CTRL_U, '\0');
    app_handle_event(&instance, &clear_name);
    send_text(&instance, "renamed-folder");
    submit_prompt(&instance);
    TEST_REQUIRE(filemanager_item_count_for_test(&instance) == 22);
    TEST_REQUIRE(!filemanager_has_item_for_test(&instance, "new-folder"));
    TEST_REQUIRE(filemanager_has_item_for_test(&instance, "renamed-folder"));
    TEST_REQUIRE(strcmp(filemanager_selected_name_for_test(&instance),
                        "renamed-folder") == 0);

    char renamed_folder_path[512];
    fixture_path(renamed_folder_path, sizeof(renamed_folder_path), root,
                 "renamed-folder");
    TEST_REQUIRE(lstat(new_folder_path, &folder_stat) < 0);
    TEST_REQUIRE(lstat(renamed_folder_path, &folder_stat) == 0);
    TEST_REQUIRE(S_ISDIR(folder_stat.st_mode));

    /* Existing destinations must not be overwritten; Esc cancels the prompt. */
    app_handle_event(&instance, &new_file);
    send_text(&instance, "created.txt");
    submit_prompt(&instance);
    TEST_REQUIRE(filemanager_item_count_for_test(&instance) == 22);
    TEST_REQUIRE(strcmp(filemanager_selected_name_for_test(&instance),
                        "renamed-folder") == 0);
    RetroEvent cancel = key_event(RETRO_KEY_ESC, '\0');
    app_handle_event(&instance, &cancel);

    /* Separators and pseudo-directory names are rejected before touching disk. */
    app_handle_event(&instance, &new_file);
    send_text(&instance, "bad/name");
    submit_prompt(&instance);
    TEST_REQUIRE(filemanager_item_count_for_test(&instance) == 22);
    TEST_REQUIRE(!filemanager_has_item_for_test(&instance, "bad/name"));
    app_handle_event(&instance, &cancel);

    TEST_REQUIRE(unlink(created_path) == 0);
    TEST_REQUIRE(rmdir(renamed_folder_path) == 0);
    desc->destroy(&instance);
    TEST_REQUIRE(instance.state == NULL);
    cleanup_filemanager_fixture(root, folder);
}
#endif

int main(void) {
    test_capability_rejection();
    test_failed_create_calls_destroy();
    test_launch_and_clean_close();
    test_repeat_create_run_shutdown();
    test_notepad_escape_does_not_close();
    test_notepad_close_cancel_and_discard();
    test_notepad_untitled_save_routes_to_save_as();
    test_notepad_undo_redo_utf8();
    test_notepad_history_limit_and_noop();
#if !defined(_WIN32)
    test_notepad_saved_baseline_tracks_undo();
    test_notepad_save_before_close();
    test_filemanager_navigation_port();
#endif
    return 0;
}
