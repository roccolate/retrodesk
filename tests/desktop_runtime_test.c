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
#if !defined(_WIN32)
    test_filemanager_navigation_port();
#endif
    return 0;
}
