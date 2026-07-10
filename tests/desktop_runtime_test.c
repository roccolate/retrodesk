#define RETRODESK_ENABLE_TEST_HOOKS

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "app/app_runtime.h"
#include "core/desktop.h"
#include "platform/platform.h"

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

static RetroEvent key_event(char ascii) {
    RetroEvent event = {0};
    event.type = RETRO_EVENT_KEY;
    event.data.key.key_code = (int)ascii;
    event.data.key.is_printable = (ascii >= 32 && ascii <= 126);
    event.data.key.ascii = event.data.key.is_printable ? (unsigned char)ascii : 0;
    return event;
}

static bool failing_create(RetroAppInstance *instance, const RetroAppContext *ctx) {
    assert(instance != NULL);
    assert(ctx != NULL);
    g_failing_create_calls++;
    instance->state = &g_failing_state_marker;
    return false;
}

static void failing_destroy(RetroAppInstance *instance) {
    assert(instance != NULL);
    assert(instance->state == &g_failing_state_marker);
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

bool platform_poll_event(PlatformBackend *platform, RetroEvent *out_event, int timeout_ms) {
    (void)timeout_ms;
    if (!platform || !out_event) return false;
    if (platform->event_index >= platform->event_count) return false;
    *out_event = platform->events[platform->event_index++];
    return true;
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
    assert(platform != NULL);

    DesktopConfig cfg = {
        .platform = platform,
        .input_timeout_ms = 20,
        .bench_mode = true,
        .render_backend = RENDER_BACKEND_CURSES,
        .theme_kind = RETRO_THEME_XP,
    };
    Desktop *desktop = desktop_create(&cfg);
    assert(desktop != NULL);

    /* Built-in apps require keyboard capability and should be rejected. */
    assert(desktop_app_count(desktop) == 0);
    assert(app_launch(desktop, "filemanager") == NULL);
    assert(app_launch(desktop, "terminal") == NULL);

    desktop_shutdown(desktop);
    platform_destroy(platform);
}

static void test_failed_create_calls_destroy(void) {
    PlatformBackend *platform = platform_stub_new(true);
    assert(platform != NULL);

    DesktopConfig cfg = {
        .platform = platform,
        .input_timeout_ms = 20,
        .bench_mode = true,
        .render_backend = RENDER_BACKEND_CURSES,
        .theme_kind = RETRO_THEME_XP,
    };
    Desktop *desktop = desktop_create(&cfg);
    assert(desktop != NULL);

    size_t app_count_before = desktop_app_count(desktop);
    size_t window_count_before = desktop_window_count(desktop);
    g_failing_create_calls = 0;
    g_failing_destroy_calls = 0;
    g_failing_state_marker = 42;

    assert(desktop_register_app_for_test(desktop, &k_failing_create_descriptor));
    assert(app_launch(desktop, "test-failing-create") == NULL);
    assert(g_failing_create_calls == 1);
    assert(g_failing_destroy_calls == 1);
    assert(desktop_app_count(desktop) == app_count_before);
    assert(desktop_window_count(desktop) == window_count_before);
    assert(desktop_app_window_id(desktop, "test-failing-create") == WINDOW_ID_INVALID);

    desktop_shutdown(desktop);
    platform_destroy(platform);
}

static void test_launch_and_clean_close(void) {
    PlatformBackend *platform = platform_stub_new(true);
    assert(platform != NULL);

    DesktopConfig cfg = {
        .platform = platform,
        .input_timeout_ms = 20,
        .bench_mode = false,
        .render_backend = RENDER_BACKEND_CURSES,
        .theme_kind = RETRO_THEME_XP,
    };
    Desktop *desktop = desktop_create(&cfg);
    assert(desktop != NULL);

    size_t base_apps = desktop_app_count(desktop);
    size_t base_windows = desktop_window_count(desktop);
    assert(base_apps >= 2);
    assert(base_windows >= base_apps + 1); /* + shell window */

    RetroAppInstance *terminal = app_launch(desktop, "terminal");
    assert(terminal != NULL);
    assert(desktop_app_count(desktop) == base_apps + 1);
    assert(desktop_window_count(desktop) == base_windows + 1);

    app_request_close(terminal);
    const RetroEvent events[] = {key_event('q')};
    platform_stub_set_events(platform, events, 1);
    assert(desktop_run(desktop) == EXIT_SUCCESS);

    /* Close request should have removed terminal app/window before exit. */
    assert(desktop_app_count(desktop) == base_apps);
    assert(desktop_window_count(desktop) == base_windows);

    desktop_shutdown(desktop);
    platform_destroy(platform);
}

static void test_repeat_create_run_shutdown(void) {
    for (int i = 0; i < 3; ++i) {
        PlatformBackend *platform = platform_stub_new(true);
        assert(platform != NULL);
        const RetroEvent events[] = {key_event('q')};
        platform_stub_set_events(platform, events, 1);

        DesktopConfig cfg = {
            .platform = platform,
            .input_timeout_ms = 20,
            .bench_mode = false,
            .render_backend = RENDER_BACKEND_CURSES,
            .theme_kind = RETRO_THEME_XP,
        };
        Desktop *desktop = desktop_create(&cfg);
        assert(desktop != NULL);
        assert(desktop_run(desktop) == EXIT_SUCCESS);
        desktop_shutdown(desktop);
        platform_destroy(platform);
    }
}

int main(void) {
    test_capability_rejection();
    test_failed_create_calls_destroy();
    test_launch_and_clean_close();
    test_repeat_create_run_shutdown();
    return 0;
}
