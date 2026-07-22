#define RETRODESK_ENABLE_TEST_HOOKS

#include "test_harness.h"
#include <limits.h>
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
#include "core/clipboard.h"
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
    size_t poll_count;
    int last_timeout_ms;
    int minimum_timeout_ms;
    bool close_when_exhausted;
};

static int g_failing_create_calls;
static int g_failing_destroy_calls;
static int g_failing_state_marker;
static size_t g_service_calls;
static size_t g_service_budget;

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
    platform->minimum_timeout_ms = INT_MAX;
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

static RetroEvent modified_key_event(int key_code,
                                     unsigned int modifiers) {
    RetroEvent event = key_event(key_code, '\0');
    event.data.key.modifiers = modifiers;
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

static bool service_create(RetroAppInstance *instance,
                           const RetroAppContext *ctx) {
    TEST_REQUIRE(instance != NULL);
    TEST_REQUIRE(ctx != NULL);
    instance->state = &g_service_calls;
    return true;
}

static RetroAppServiceResult service_tick(RetroAppInstance *instance,
                                          size_t work_budget) {
    TEST_REQUIRE(instance != NULL);
    TEST_REQUIRE(instance->state == &g_service_calls);
    TEST_REQUIRE(work_budget > 0);
    g_service_calls++;
    g_service_budget = work_budget;
    return RETRO_APP_SERVICE_REDRAW;
}

static void service_destroy(RetroAppInstance *instance) {
    TEST_REQUIRE(instance != NULL);
    instance->state = NULL;
}

static const RetroAppDescriptor k_service_descriptor = {
    .app_id = "test-service",
    .display_name = "Service",
    .required_capabilities = 0,
    .default_height = 6,
    .default_width = 24,
    .default_y = 2,
    .default_x = 4,
    .window_flags = WINDOW_FLAG_APP_OWNED,
    .create = service_create,
    .on_service = service_tick,
    .destroy = service_destroy,
};

RetroPollResult platform_poll_event(PlatformBackend *platform, RetroEvent *out_event,
                                    int timeout_ms) {
    if (!platform || !out_event) return RETRO_POLL_ERROR;
    platform->poll_count++;
    platform->last_timeout_ms = timeout_ms;
    if (timeout_ms < platform->minimum_timeout_ms) {
        platform->minimum_timeout_ms = timeout_ms;
    }
    if (platform->event_index >= platform->event_count) {
        return platform->close_when_exhausted
                   ? RETRO_POLL_CLOSED
                   : RETRO_POLL_TIMEOUT;
    }
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

static void init_growth_descriptor(RetroAppDescriptor *desc,
                         const char *app_id) {
    TEST_REQUIRE(desc != NULL);
    TEST_REQUIRE(app_id != NULL);
    memset(desc, 0, sizeof(*desc));
    desc->app_id = app_id;
    desc->display_name = app_id;
    desc->default_height = 6;
    desc->default_width = 20;
    desc->default_y = 1;
    desc->default_x = 1;
    desc->window_flags = WINDOW_FLAG_APP_OWNED;
}

static void test_desktop_growth_failure_preserves_state(void) {
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
    TEST_REQUIRE(desktop_app_count(desktop) < 8);

    RetroAppDescriptor descriptors[16] = {0};
    char app_ids[16][32] = {{0}};
    size_t used = 0;
    while (desktop_app_count(desktop) < 8) {
        TEST_REQUIRE(used < 15);
        snprintf(app_ids[used], sizeof(app_ids[used]), "test-growth-%u",
       (unsigned int)used);
        init_growth_descriptor(&descriptors[used], app_ids[used]);
        TEST_REQUIRE(desktop_register_app_for_test(desktop,
                                         &descriptors[used]));
        TEST_REQUIRE(app_launch(desktop, app_ids[used]) != NULL);
        used++;
    }

    TEST_REQUIRE(used > 0);
    WindowId anchor = desktop_app_window_id(desktop, app_ids[0]);
    TEST_REQUIRE(anchor != WINDOW_ID_INVALID);
    size_t stable_app_count = desktop_app_count(desktop);
    size_t stable_window_count = desktop_window_count(desktop);

    snprintf(app_ids[used], sizeof(app_ids[used]), "test-growth-%u",
   (unsigned int)used);
    init_growth_descriptor(&descriptors[used], app_ids[used]);
    TEST_REQUIRE(desktop_register_app_for_test(desktop,
                                     &descriptors[used]));

    desktop_fail_next_app_growth_for_test(desktop);
    TEST_REQUIRE(app_launch(desktop, app_ids[used]) == NULL);
    TEST_REQUIRE(desktop_app_count(desktop) == stable_app_count);
    TEST_REQUIRE(desktop_window_count(desktop) == stable_window_count);
    TEST_REQUIRE(desktop_app_window_id(desktop, app_ids[0]) == anchor);
    TEST_REQUIRE(desktop_app_window_id(desktop, app_ids[used]) ==
       WINDOW_ID_INVALID);

    TEST_REQUIRE(app_launch(desktop, app_ids[used]) != NULL);
    TEST_REQUIRE(desktop_app_count(desktop) == stable_app_count + 1);
    TEST_REQUIRE(desktop_window_count(desktop) == stable_window_count + 1);

    desktop_shutdown(desktop);
    platform_destroy(platform);
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

    RetroEvent close = key_event(RETRO_KEY_CTRL_W, '\0');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &close));

    /* A normal close removes only the active Diagnostics instance. */
    TEST_REQUIRE(desktop_app_count(desktop) == base_apps);
    TEST_REQUIRE(desktop_window_count(desktop) == base_windows);

    const RetroEvent events[] = {key_event(RETRO_KEY_CTRL_Q, '\0')};
    platform_stub_set_events(platform, events, 1);
    TEST_REQUIRE(desktop_run(desktop) == EXIT_SUCCESS);

    /* A clean global shutdown commits every app close atomically. */
    TEST_REQUIRE(desktop_app_count(desktop) == 0);
    TEST_REQUIRE(desktop_window_count(desktop) == base_windows - base_apps);

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

static void test_app_service_tick_contract(void) {
    PlatformBackend *plain_platform = platform_stub_new(true);
    TEST_REQUIRE(plain_platform != NULL);
    plain_platform->close_when_exhausted = true;
    DesktopConfig plain_cfg = {
        .platform = plain_platform,
        .input_timeout_ms = 100,
        .bench_mode = false,
        .render_backend = RENDER_BACKEND_CURSES,
        .theme_kind = RETRO_THEME_XP,
    };
    Desktop *plain_desktop = desktop_create(&plain_cfg);
    TEST_REQUIRE(plain_desktop != NULL);
    TEST_REQUIRE(desktop_run(plain_desktop) == EXIT_SUCCESS);
    TEST_REQUIRE(plain_platform->poll_count == 1);
    TEST_REQUIRE(plain_platform->last_timeout_ms == 100);
    desktop_shutdown(plain_desktop);
    platform_destroy(plain_platform);

    PlatformBackend *service_platform = platform_stub_new(true);
    TEST_REQUIRE(service_platform != NULL);
    service_platform->close_when_exhausted = true;
    DesktopConfig service_cfg = {
        .platform = service_platform,
        .input_timeout_ms = 100,
        .bench_mode = false,
        .render_backend = RENDER_BACKEND_CURSES,
        .theme_kind = RETRO_THEME_XP,
    };
    Desktop *service_desktop = desktop_create(&service_cfg);
    TEST_REQUIRE(service_desktop != NULL);
    TEST_REQUIRE(desktop_register_app_for_test(
        service_desktop, &k_service_descriptor));
    g_service_calls = 0;
    g_service_budget = 0;
    TEST_REQUIRE(app_launch(service_desktop, "test-service") != NULL);
    TEST_REQUIRE(desktop_run(service_desktop) == EXIT_SUCCESS);
    TEST_REQUIRE(g_service_calls == 1);
    TEST_REQUIRE(g_service_budget == 8192);
    TEST_REQUIRE(service_platform->poll_count == 1);
    TEST_REQUIRE(service_platform->last_timeout_ms == 16);
    TEST_REQUIRE(service_platform->minimum_timeout_ms == 16);
    desktop_shutdown(service_desktop);
    platform_destroy(service_platform);
}

static RetroAppInstance create_untitled_notepad_with_clipboard(
    RetroClipboard *clipboard) {
    const RetroAppDescriptor *desc = notepad_app_descriptor();
    TEST_REQUIRE(desc != NULL);
    RetroAppContext ctx = {
        .desktop = NULL,
        .clipboard = clipboard,
        .theme = retro_theme_get(RETRO_THEME_XP),
        .resource_path = NULL,
    };
    RetroAppInstance instance = {.descriptor = desc, .ctx = ctx};
    TEST_REQUIRE(desc->create(&instance, &ctx));
    return instance;
}

static RetroAppInstance create_untitled_notepad(void) {
    return create_untitled_notepad_with_clipboard(NULL);
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

    TEST_REQUIRE(app_request_close(&instance) == RETRO_CLOSE_DEFERRED);
    TEST_REQUIRE(notepad_close_prompt_for_test(&instance));

    RetroEvent escape = key_event(RETRO_KEY_ESC, '\0');
    app_handle_event(&instance, &escape);
    TEST_REQUIRE(!notepad_close_prompt_for_test(&instance));
    TEST_REQUIRE(notepad_dirty_for_test(&instance));
    TEST_REQUIRE(!app_is_close_requested(&instance));

    TEST_REQUIRE(app_request_close(&instance) == RETRO_CLOSE_DEFERRED);
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

    TEST_REQUIRE(app_request_close(&instance) == RETRO_CLOSE_DEFERRED);
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

static void test_notepad_selection_clipboard_between_instances(void) {
    RetroClipboard *clipboard = retro_clipboard_create();
    TEST_REQUIRE(clipboard != NULL);
    RetroAppInstance first = create_untitled_notepad_with_clipboard(clipboard);
    RetroAppInstance second = create_untitled_notepad_with_clipboard(clipboard);

    type_notepad_char(&first, 'n');
    type_notepad_char(&first, 'i');
    type_notepad_codepoint(&first, 0x00F1u);
    type_notepad_char(&first, 'o');
    TEST_REQUIRE(strcmp(notepad_line_for_test(&first, 0), "niño") == 0);

    size_t history_before_copy = notepad_undo_count_for_test(&first);
    RetroEvent select_all = key_event(RETRO_KEY_CTRL_A, '\0');
    RetroEvent copy = key_event(RETRO_KEY_CTRL_C, '\0');
    app_handle_event(&first, &select_all);
    app_handle_event(&first, &copy);
    TEST_REQUIRE(notepad_undo_count_for_test(&first) == history_before_copy);
    TEST_REQUIRE(retro_clipboard_has_text(clipboard));
    TEST_REQUIRE(strcmp(retro_clipboard_text(clipboard, NULL), "niño") == 0);

    RetroEvent paste = key_event(RETRO_KEY_CTRL_V, '\0');
    app_handle_event(&second, &paste);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&second, 0), "niño") == 0);
    TEST_REQUIRE(notepad_undo_count_for_test(&second) == 1);
    TEST_REQUIRE(notepad_dirty_for_test(&second));

    RetroEvent undo = key_event(RETRO_KEY_CTRL_Z, '\0');
    app_handle_event(&second, &undo);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&second, 0), "") == 0);
    TEST_REQUIRE(!notepad_dirty_for_test(&second));

    RetroEvent cut = key_event(RETRO_KEY_CTRL_X, '\0');
    app_handle_event(&first, &cut);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&first, 0), "") == 0);
    TEST_REQUIRE(strcmp(retro_clipboard_text(clipboard, NULL), "niño") == 0);
    app_handle_event(&first, &undo);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&first, 0), "niño") == 0);

    first.descriptor->destroy(&first);
    second.descriptor->destroy(&second);
    retro_clipboard_destroy(clipboard);
}

static void test_notepad_utf8_find(void) {
    RetroClipboard *clipboard = retro_clipboard_create();
    TEST_REQUIRE(clipboard != NULL);
    RetroAppInstance instance = create_untitled_notepad_with_clipboard(clipboard);

    type_notepad_codepoint(&instance, 0x00C1u);
    type_notepad_char(&instance, 'r');
    type_notepad_char(&instance, 'b');
    type_notepad_char(&instance, 'o');
    type_notepad_char(&instance, 'l');
    RetroEvent enter = key_event(RETRO_KEY_CR, '\0');
    app_handle_event(&instance, &enter);
    type_notepad_char(&instance, 'n');
    type_notepad_char(&instance, 'i');
    type_notepad_codepoint(&instance, 0x00F1u);
    type_notepad_char(&instance, 'o');
    type_notepad_char(&instance, ' ');
    type_notepad_codepoint(&instance, 0x00E1u);
    type_notepad_char(&instance, 'r');
    type_notepad_char(&instance, 'b');
    type_notepad_char(&instance, 'o');
    type_notepad_char(&instance, 'l');
    app_handle_event(&instance, &enter);
    type_notepad_codepoint(&instance, 0x00C1u);
    type_notepad_char(&instance, 'R');
    type_notepad_char(&instance, 'B');
    type_notepad_char(&instance, 'O');
    type_notepad_char(&instance, 'L');

    size_t history_before_find = notepad_undo_count_for_test(&instance);
    RetroEvent find = key_event(RETRO_KEY_CTRL_F, '\0');
    app_handle_event(&instance, &find);
    TEST_REQUIRE(notepad_search_mode_for_test(&instance));
    type_notepad_codepoint(&instance, 0x00E1u);
    type_notepad_char(&instance, 'r');
    type_notepad_char(&instance, 'b');
    type_notepad_char(&instance, 'o');
    type_notepad_char(&instance, 'l');
    app_handle_event(&instance, &enter);
    TEST_REQUIRE(notepad_cursor_row_for_test(&instance) == 0);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 6);
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) == history_before_find);

    RetroEvent escape = key_event(RETRO_KEY_ESC, '\0');
    app_handle_event(&instance, &escape);
    TEST_REQUIRE(!notepad_search_mode_for_test(&instance));
    RetroEvent copy = key_event(RETRO_KEY_CTRL_C, '\0');
    app_handle_event(&instance, &copy);
    TEST_REQUIRE(strcmp(retro_clipboard_text(clipboard, NULL), "Árbol") == 0);

    app_handle_event(&instance, &find);
    type_notepad_codepoint(&instance, 0x00E1u);
    type_notepad_char(&instance, 'r');
    type_notepad_char(&instance, 'b');
    type_notepad_char(&instance, 'o');
    type_notepad_char(&instance, 'l');
    app_handle_event(&instance, &enter);
    TEST_REQUIRE(notepad_cursor_row_for_test(&instance) == 1);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 12);
    app_handle_event(&instance, &escape);
    app_handle_event(&instance, &copy);
    TEST_REQUIRE(strcmp(retro_clipboard_text(clipboard, NULL), "árbol") == 0);

    app_handle_event(&instance, &find);
    type_notepad_codepoint(&instance, 0x00E1u);
    type_notepad_char(&instance, 'r');
    type_notepad_char(&instance, 'b');
    type_notepad_char(&instance, 'o');
    type_notepad_char(&instance, 'l');
    app_handle_event(&instance, &enter);
    TEST_REQUIRE(notepad_cursor_row_for_test(&instance) == 2);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 6);
    app_handle_event(&instance, &escape);
    app_handle_event(&instance, &copy);
    TEST_REQUIRE(strcmp(retro_clipboard_text(clipboard, NULL), "ÁRBOL") == 0);

    TEST_REQUIRE(notepad_undo_count_for_test(&instance) == history_before_find);
    instance.descriptor->destroy(&instance);
    retro_clipboard_destroy(clipboard);
}

static void test_notepad_word_wrap_navigation(void) {
    RetroClipboard *clipboard = retro_clipboard_create();
    TEST_REQUIRE(clipboard != NULL);
    RetroAppInstance instance = create_untitled_notepad_with_clipboard(clipboard);
    TEST_REQUIRE(!notepad_wrap_mode_for_test(&instance));
    TEST_REQUIRE(!notepad_dirty_for_test(&instance));
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) == 0);

    RetroEvent toggle = key_event(RETRO_KEY_F4, '\0');
    app_handle_event(&instance, &toggle);
    TEST_REQUIRE(notepad_wrap_mode_for_test(&instance));
    TEST_REQUIRE(!notepad_dirty_for_test(&instance));
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) == 0);

    for (int index = 0; index < 70; ++index) {
        type_notepad_char(&instance, 'a');
    }
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 70);
    size_t history_before_navigation = notepad_undo_count_for_test(&instance);

    RetroEvent up = key_event(RETRO_KEY_UP, '\0');
    app_handle_event(&instance, &up);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 6);
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) ==
                 history_before_navigation);

    RetroEvent down = key_event(RETRO_KEY_DOWN, '\0');
    app_handle_event(&instance, &down);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 70);
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) ==
                 history_before_navigation);

    RetroEvent shift_up = modified_key_event(
        RETRO_KEY_UP, RETRO_MOD_SHIFT);
    app_handle_event(&instance, &shift_up);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 6);
    RetroEvent copy = key_event(RETRO_KEY_CTRL_C, '\0');
    app_handle_event(&instance, &copy);
    TEST_REQUIRE(retro_clipboard_has_text(clipboard));
    TEST_REQUIRE(strlen(retro_clipboard_text(clipboard, NULL)) == 64);
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) ==
                 history_before_navigation);

    RetroEvent escape = key_event(RETRO_KEY_ESC, '\0');
    app_handle_event(&instance, &escape);
    app_handle_event(&instance, &down);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 70);

    app_handle_event(&instance, &toggle);
    TEST_REQUIRE(!notepad_wrap_mode_for_test(&instance));
    app_handle_event(&instance, &up);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 70);
    TEST_REQUIRE(notepad_undo_count_for_test(&instance) ==
                 history_before_navigation);

    instance.descriptor->destroy(&instance);
    retro_clipboard_destroy(clipboard);
}

static void test_notepad_shift_selection_replacement(void) {
    RetroAppInstance instance = create_untitled_notepad();
    type_notepad_char(&instance, 'n');
    type_notepad_char(&instance, 'i');
    type_notepad_codepoint(&instance, 0x00F1u);
    type_notepad_char(&instance, 'o');

    RetroEvent left = key_event(RETRO_KEY_LEFT, '\0');
    app_handle_event(&instance, &left);
    app_handle_event(&instance, &left);
    RetroEvent shift_right = modified_key_event(
        RETRO_KEY_RIGHT, RETRO_MOD_SHIFT);
    app_handle_event(&instance, &shift_right);
    type_notepad_char(&instance, 'x');

    TEST_REQUIRE(strcmp(notepad_line_for_test(&instance, 0), "nixo") == 0);
    TEST_REQUIRE(notepad_cursor_col_for_test(&instance) == 3);

    RetroEvent undo = key_event(RETRO_KEY_CTRL_Z, '\0');
    app_handle_event(&instance, &undo);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&instance, 0), "niño") == 0);

    instance.descriptor->destroy(&instance);
}

static Desktop *create_test_desktop(PlatformBackend **out_platform) {
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
    if (out_platform) *out_platform = platform;
    return desktop;
}

static void destroy_test_desktop(Desktop *desktop,
                                 PlatformBackend *platform) {
    desktop_shutdown(desktop);
    platform_destroy(platform);
}

static void test_desktop_clipboards_are_isolated(void) {
    PlatformBackend *first_platform = NULL;
    PlatformBackend *second_platform = NULL;
    Desktop *first_desktop = create_test_desktop(&first_platform);
    Desktop *second_desktop = create_test_desktop(&second_platform);
    TEST_REQUIRE(first_desktop != NULL);
    TEST_REQUIRE(second_desktop != NULL);

    RetroAppInstance *first = app_launch(first_desktop, "notepad");
    RetroAppInstance *second = app_launch(second_desktop, "notepad");
    TEST_REQUIRE(first != NULL);
    TEST_REQUIRE(second != NULL);

    type_notepad_char(first, 'x');
    RetroEvent select_all = key_event(RETRO_KEY_CTRL_A, '\0');
    RetroEvent copy = key_event(RETRO_KEY_CTRL_C, '\0');
    RetroEvent paste = key_event(RETRO_KEY_CTRL_V, '\0');
    RetroEvent escape = key_event(RETRO_KEY_ESC, '\0');
    app_handle_event(first, &select_all);
    app_handle_event(first, &copy);

    app_handle_event(second, &paste);
    TEST_REQUIRE(strcmp(notepad_line_for_test(second, 0), "") == 0);

    type_notepad_char(second, 'y');
    app_handle_event(second, &select_all);
    app_handle_event(second, &copy);

    app_handle_event(first, &escape);
    app_handle_event(first, &paste);
    TEST_REQUIRE(strcmp(notepad_line_for_test(first, 0), "xx") == 0);

    destroy_test_desktop(first_desktop, first_platform);
    destroy_test_desktop(second_desktop, second_platform);
}

static int g_key_sink_f2_count;

static bool key_sink_create(RetroAppInstance *instance,
                            const RetroAppContext *ctx) {
    TEST_REQUIRE(instance != NULL);
    TEST_REQUIRE(ctx != NULL);
    instance->state = &g_key_sink_f2_count;
    return true;
}

static void key_sink_event(RetroAppInstance *instance,
                           const RetroEvent *event) {
    TEST_REQUIRE(instance != NULL);
    if (event && event->type == RETRO_EVENT_KEY &&
        event->data.key.key_code == RETRO_KEY_F2) {
        g_key_sink_f2_count++;
    }
}

static void key_sink_destroy(RetroAppInstance *instance) {
    TEST_REQUIRE(instance != NULL);
    instance->state = NULL;
}

static const RetroAppDescriptor k_key_sink_descriptor = {
    .app_id = "test-key-sink",
    .display_name = "Key Sink",
    .required_capabilities = PLATFORM_CAP_KEYBOARD_BASIC,
    .default_height = 6,
    .default_width = 24,
    .default_y = 2,
    .default_x = 4,
    .window_flags = WINDOW_FLAG_APP_OWNED,
    .create = key_sink_create,
    .on_event = key_sink_event,
    .destroy = key_sink_destroy,
};

static void test_ctrl_q_cancel_preserves_all_apps(void) {
    PlatformBackend *platform = NULL;
    Desktop *desktop = create_test_desktop(&platform);
    TEST_REQUIRE(desktop != NULL);

    RetroAppInstance *notepad = app_launch(desktop, "notepad");
    TEST_REQUIRE(notepad != NULL);
    type_notepad_char(notepad, 'x');
    size_t app_count = desktop_app_count(desktop);

    RetroEvent quit = key_event(RETRO_KEY_CTRL_Q, '\0');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &quit));
    TEST_REQUIRE(desktop_shutdown_pending_for_test(desktop));
    TEST_REQUIRE(notepad_close_prompt_for_test(notepad));
    TEST_REQUIRE(desktop_app_count(desktop) == app_count);

    RetroEvent cancel = key_event(RETRO_KEY_ESC, '\0');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &cancel));
    TEST_REQUIRE(!desktop_shutdown_pending_for_test(desktop));
    TEST_REQUIRE(desktop_app_count(desktop) == app_count);
    TEST_REQUIRE(notepad_dirty_for_test(notepad));

    destroy_test_desktop(desktop, platform);
}

static void test_ctrl_q_discard_commits_shutdown(void) {
    PlatformBackend *platform = NULL;
    Desktop *desktop = create_test_desktop(&platform);
    TEST_REQUIRE(desktop != NULL);

    RetroAppInstance *notepad = app_launch(desktop, "notepad");
    TEST_REQUIRE(notepad != NULL);
    type_notepad_char(notepad, 'x');

    RetroEvent quit = key_event(RETRO_KEY_CTRL_Q, '\0');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &quit));
    TEST_REQUIRE(desktop_shutdown_pending_for_test(desktop));
    TEST_REQUIRE(desktop_app_count(desktop) >= 2);

    RetroEvent discard = key_event('d', 'd');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &discard));
    TEST_REQUIRE(!desktop_shutdown_pending_for_test(desktop));
    TEST_REQUIRE(desktop_app_count(desktop) == 0);

    destroy_test_desktop(desktop, platform);
}

static void test_desktop_f2_reaches_focused_app(void) {
    PlatformBackend *platform = NULL;
    Desktop *desktop = create_test_desktop(&platform);
    TEST_REQUIRE(desktop_register_app_for_test(
        desktop, &k_key_sink_descriptor));
    g_key_sink_f2_count = 0;
    RetroAppInstance *sink = app_launch(desktop, "test-key-sink");
    TEST_REQUIRE(sink != NULL);
    TEST_REQUIRE(desktop_active_window(desktop) == sink->ctx.window_id);

    RetroEvent f2 = key_event(RETRO_KEY_F2, '\0');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &f2));
    TEST_REQUIRE(g_key_sink_f2_count == 1);
    TEST_REQUIRE(desktop_active_window(desktop) == sink->ctx.window_id);
    destroy_test_desktop(desktop, platform);
}

static void test_launcher_close_respects_dirty_notepad(void) {
    PlatformBackend *platform = NULL;
    Desktop *desktop = create_test_desktop(&platform);
    RetroAppInstance *notepad = app_launch(desktop, "notepad");
    TEST_REQUIRE(notepad != NULL);
    type_notepad_char(notepad, 'x');
    TEST_REQUIRE(notepad_dirty_for_test(notepad));
    size_t app_count = desktop_app_count(desktop);

    RetroEvent launcher = key_event(RETRO_KEY_F10, '\0');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &launcher));
    for (int i = 0; i < 3; ++i) {
        RetroEvent down = key_event('j', 'j');
        TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &down));
    }
    RetroEvent enter = key_event(RETRO_KEY_CR, '\0');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &enter));

    TEST_REQUIRE(desktop_app_count(desktop) == app_count);
    TEST_REQUIRE(desktop_app_instance_for_test(desktop, "notepad") == notepad);
    TEST_REQUIRE(notepad_close_prompt_for_test(notepad));
    TEST_REQUIRE(notepad_dirty_for_test(notepad));
    destroy_test_desktop(desktop, platform);
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


static void test_notepad_failed_save_as_preserves_existing_identity(void) {
    char original[] = "/tmp/retrodesk-notepad-original-XXXXXX";
    int original_fd = mkstemp(original);
    TEST_REQUIRE(original_fd >= 0);
    FILE *stream = fdopen(original_fd, "w");
    TEST_REQUIRE(stream != NULL);
    TEST_REQUIRE(fputs("base", stream) >= 0);
    TEST_REQUIRE(fclose(stream) == 0);

    char blocker[] = "/tmp/retrodesk-notepad-blocker-XXXXXX";
    int blocker_fd = mkstemp(blocker);
    TEST_REQUIRE(blocker_fd >= 0);
    TEST_REQUIRE(close(blocker_fd) == 0);
    char invalid_path[512];
    fixture_path(invalid_path, sizeof(invalid_path), blocker,
                 "cannot-create.txt");

    const RetroAppDescriptor *desc = notepad_app_descriptor();
    TEST_REQUIRE(desc != NULL);
    RetroAppContext ctx = {
        .desktop = NULL,
        .theme = retro_theme_get(RETRO_THEME_XP),
        .resource_path = original,
    };
    RetroAppInstance instance = {.descriptor = desc, .ctx = ctx};
    TEST_REQUIRE(desc->create(&instance, &ctx));
    type_notepad_char(&instance, 'x');

    RetroEvent save_as = key_event(RETRO_KEY_F3, '\0');
    app_handle_event(&instance, &save_as);
    TEST_REQUIRE(notepad_save_as_for_test(&instance));
    send_text(&instance, invalid_path);
    submit_prompt(&instance);

    TEST_REQUIRE(notepad_save_as_for_test(&instance));
    TEST_REQUIRE(notepad_has_path_for_test(&instance));
    TEST_REQUIRE(strcmp(notepad_path_for_test(&instance), original) == 0);
    TEST_REQUIRE(notepad_dirty_for_test(&instance));
    TEST_REQUIRE(notepad_error_for_test(&instance)[0] != '\0');

    RetroEvent escape = key_event(RETRO_KEY_ESC, '\0');
    app_handle_event(&instance, &escape);
    TEST_REQUIRE(!notepad_save_as_for_test(&instance));

    RetroEvent save = key_event(RETRO_KEY_CTRL_S, '\0');
    app_handle_event(&instance, &save);
    TEST_REQUIRE(!notepad_dirty_for_test(&instance));

    stream = fopen(original, "r");
    TEST_REQUIRE(stream != NULL);
    char content[16] = {0};
    TEST_REQUIRE(fread(content, 1, sizeof(content) - 1, stream) == 5);
    TEST_REQUIRE(fclose(stream) == 0);
    TEST_REQUIRE(strcmp(content, "basex") == 0);

    desc->destroy(&instance);
    TEST_REQUIRE(unlink(blocker) == 0);
    TEST_REQUIRE(unlink(original) == 0);
}

static void test_notepad_failed_close_save_as_remains_pending(void) {
    char blocker[] = "/tmp/retrodesk-notepad-close-blocker-XXXXXX";
    int blocker_fd = mkstemp(blocker);
    TEST_REQUIRE(blocker_fd >= 0);
    TEST_REQUIRE(close(blocker_fd) == 0);
    char invalid_path[512];
    fixture_path(invalid_path, sizeof(invalid_path), blocker,
                 "cannot-create.txt");

    RetroAppInstance instance = create_untitled_notepad();
    type_notepad_char(&instance, 'x');
    TEST_REQUIRE(app_request_close(&instance) == RETRO_CLOSE_DEFERRED);
    RetroEvent save = key_event('s', 's');
    app_handle_event(&instance, &save);
    TEST_REQUIRE(notepad_save_as_for_test(&instance));
    TEST_REQUIRE(notepad_close_after_save_for_test(&instance));

    send_text(&instance, invalid_path);
    submit_prompt(&instance);

    TEST_REQUIRE(notepad_save_as_for_test(&instance));
    TEST_REQUIRE(notepad_close_after_save_for_test(&instance));
    TEST_REQUIRE(!notepad_has_path_for_test(&instance));
    TEST_REQUIRE(strcmp(notepad_path_for_test(&instance), "") == 0);
    TEST_REQUIRE(notepad_dirty_for_test(&instance));
    TEST_REQUIRE(notepad_error_for_test(&instance)[0] != '\0');
    TEST_REQUIRE(!app_is_close_requested(&instance));

    RetroEvent escape = key_event(RETRO_KEY_ESC, '\0');
    app_handle_event(&instance, &escape);
    TEST_REQUIRE(!notepad_save_as_for_test(&instance));
    TEST_REQUIRE(!notepad_close_after_save_for_test(&instance));
    TEST_REQUIRE(!app_is_close_requested(&instance));

    instance.descriptor->destroy(&instance);
    TEST_REQUIRE(unlink(blocker) == 0);
}

static void test_notepad_successful_save_as_commits_identity(void) {
    char target[] = "/tmp/retrodesk-notepad-save-as-XXXXXX";
    int fd = mkstemp(target);
    TEST_REQUIRE(fd >= 0);
    TEST_REQUIRE(close(fd) == 0);
    TEST_REQUIRE(unlink(target) == 0);

    RetroAppInstance instance = create_untitled_notepad();
    type_notepad_char(&instance, 'x');
    RetroEvent save_as = key_event(RETRO_KEY_F3, '\0');
    app_handle_event(&instance, &save_as);
    send_text(&instance, target);
    submit_prompt(&instance);

    TEST_REQUIRE(!notepad_save_as_for_test(&instance));
    TEST_REQUIRE(notepad_has_path_for_test(&instance));
    TEST_REQUIRE(strcmp(notepad_path_for_test(&instance), target) == 0);
    TEST_REQUIRE(!notepad_dirty_for_test(&instance));
    TEST_REQUIRE(notepad_error_for_test(&instance)[0] == '\0');

    FILE *stream = fopen(target, "r");
    TEST_REQUIRE(stream != NULL);
    char content[8] = {0};
    TEST_REQUIRE(fread(content, 1, sizeof(content) - 1, stream) == 1);
    TEST_REQUIRE(fclose(stream) == 0);
    TEST_REQUIRE(strcmp(content, "x") == 0);

    instance.descriptor->destroy(&instance);
    TEST_REQUIRE(unlink(target) == 0);
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
    TEST_REQUIRE(app_request_close(&instance) == RETRO_CLOSE_DEFERRED);
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
    test_desktop_growth_failure_preserves_state();
    test_capability_rejection();
    test_failed_create_calls_destroy();
    test_launch_and_clean_close();
    test_repeat_create_run_shutdown();
    test_app_service_tick_contract();
    test_notepad_escape_does_not_close();
    test_notepad_close_cancel_and_discard();
    test_notepad_untitled_save_routes_to_save_as();
    test_notepad_undo_redo_utf8();
    test_notepad_history_limit_and_noop();
    test_notepad_selection_clipboard_between_instances();
    test_notepad_utf8_find();
    test_notepad_word_wrap_navigation();
    test_notepad_shift_selection_replacement();
    test_desktop_clipboards_are_isolated();
    test_desktop_f2_reaches_focused_app();
    test_launcher_close_respects_dirty_notepad();
    test_ctrl_q_cancel_preserves_all_apps();
    test_ctrl_q_discard_commits_shutdown();
#if !defined(_WIN32)
    test_notepad_failed_save_as_preserves_existing_identity();
    test_notepad_failed_close_save_as_remains_pending();
    test_notepad_successful_save_as_commits_identity();
    test_notepad_saved_baseline_tracks_undo();
    test_notepad_save_before_close();
    test_filemanager_navigation_port();
#endif
    return 0;
}
