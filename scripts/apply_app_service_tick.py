#!/usr/bin/env python3
from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    file_path = Path(path)
    text = file_path.read_text()
    if new in text:
        return
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}")
    file_path.write_text(text.replace(old, new, 1))


replace_once(
    "src/app/app_runtime.h",
    """typedef enum RetroCloseResult {
    RETRO_CLOSE_ALLOWED = 0,
    RETRO_CLOSE_DEFERRED,
    RETRO_CLOSE_CANCELLED,
} RetroCloseResult;

typedef struct RetroAppContext {
""",
    """typedef enum RetroCloseResult {
    RETRO_CLOSE_ALLOWED = 0,
    RETRO_CLOSE_DEFERRED,
    RETRO_CLOSE_CANCELLED,
} RetroCloseResult;

/* Optional background services run from the single Desktop loop. Callbacks must
   not block and must honor the supplied work budget. REDRAW reports that visible
   app state changed; unknown future flags are ignored by older hosts. */
typedef enum RetroAppServiceResult {
    RETRO_APP_SERVICE_IDLE = 0,
    RETRO_APP_SERVICE_REDRAW = 1u << 0,
} RetroAppServiceResult;

typedef struct RetroAppContext {
""",
)
replace_once(
    "src/app/app_runtime.h",
    """    bool (*create)(RetroAppInstance *instance, const RetroAppContext *ctx);
    void (*on_event)(RetroAppInstance *instance, const RetroEvent *event);
    void (*on_render)(RetroAppInstance *instance, DrawList *draw_list);
""",
    """    bool (*create)(RetroAppInstance *instance, const RetroAppContext *ctx);
    void (*on_event)(RetroAppInstance *instance, const RetroEvent *event);
    RetroAppServiceResult (*on_service)(RetroAppInstance *instance,
                                        size_t work_budget);
    void (*on_render)(RetroAppInstance *instance, DrawList *draw_list);
""",
)
replace_once(
    "src/app/app_runtime.h",
    """void app_handle_event(RetroAppInstance *app, const RetroEvent *event);
void app_render(RetroAppInstance *app, DrawList *draw_list);
""",
    """void app_handle_event(RetroAppInstance *app, const RetroEvent *event);
RetroAppServiceResult app_service(RetroAppInstance *app, size_t work_budget);
void app_render(RetroAppInstance *app, DrawList *draw_list);
""",
)

replace_once(
    "src/app/app_runtime.c",
    """void app_handle_event(RetroAppInstance *app, const RetroEvent *event) {
    if (!app || !event || !app->descriptor || !app->descriptor->on_event) return;
    app->descriptor->on_event(app, event);
}

void app_render(RetroAppInstance *app, DrawList *draw_list) {
""",
    """void app_handle_event(RetroAppInstance *app, const RetroEvent *event) {
    if (!app || !event || !app->descriptor || !app->descriptor->on_event) return;
    app->descriptor->on_event(app, event);
}

RetroAppServiceResult app_service(RetroAppInstance *app, size_t work_budget) {
    if (!app || !app->descriptor || !app->descriptor->on_service ||
        work_budget == 0) {
        return RETRO_APP_SERVICE_IDLE;
    }
    return app->descriptor->on_service(app, work_budget);
}

void app_render(RetroAppInstance *app, DrawList *draw_list) {
""",
)

replace_once(
    "src/core/desktop.c",
    """typedef struct LauncherState {
    bool open;
    int selected;
    WindowId window_id;
} LauncherState;

struct Desktop {
""",
    """typedef struct LauncherState {
    bool open;
    int selected;
    WindowId window_id;
} LauncherState;

static const int k_active_service_poll_ms = 16;
static const size_t k_app_service_budget = 8192;

struct Desktop {
""",
)
replace_once(
    "src/core/desktop.c",
    """static bool desktop_app_is_running(const Desktop *desktop, const char *app_id) {
    if (!desktop || !app_id) return false;
    for (size_t i = 0; i < desktop->app_count; ++i) {
        const RetroAppInstance *inst = desktop->apps[i].app;
        if (inst && inst->descriptor && inst->descriptor->app_id &&
            strcmp(inst->descriptor->app_id, app_id) == 0) {
            return true;
        }
    }
    return false;
}

static bool desktop_add_app(Desktop *desktop, RetroAppInstance *app, WindowId window_id) {
""",
    """static bool desktop_app_is_running(const Desktop *desktop, const char *app_id) {
    if (!desktop || !app_id) return false;
    for (size_t i = 0; i < desktop->app_count; ++i) {
        const RetroAppInstance *inst = desktop->apps[i].app;
        if (inst && inst->descriptor && inst->descriptor->app_id &&
            strcmp(inst->descriptor->app_id, app_id) == 0) {
            return true;
        }
    }
    return false;
}

static bool desktop_has_active_services(const Desktop *desktop) {
    if (!desktop) return false;
    for (size_t i = 0; i < desktop->app_count; ++i) {
        const RetroAppInstance *app = desktop->apps[i].app;
        if (app && app->descriptor && app->descriptor->on_service) return true;
    }
    return false;
}

static void desktop_service_apps(Desktop *desktop) {
    if (!desktop) return;
    for (size_t i = 0; i < desktop->app_count; ++i) {
        RetroAppInstance *app = desktop->apps[i].app;
        RetroAppServiceResult result = app_service(app, k_app_service_budget);
        if ((result & RETRO_APP_SERVICE_REDRAW) != 0) {
            desktop->needs_redraw = true;
        }
    }
}

static int desktop_poll_timeout_ms(const Desktop *desktop) {
    if (!desktop) return 0;
    int timeout_ms = desktop->config.input_timeout_ms;
    if (desktop_has_active_services(desktop) &&
        timeout_ms > k_active_service_poll_ms) {
        timeout_ms = k_active_service_poll_ms;
    }
    return timeout_ms;
}

static bool desktop_add_app(Desktop *desktop, RetroAppInstance *app, WindowId window_id) {
""",
)
replace_once(
    "src/core/desktop.c",
    """    desktop->running = true;
    while (desktop->running) {
        RetroEvent event = {0};
        RetroPollResult poll_result = platform_poll_event(
            desktop->platform, &event, desktop->config.input_timeout_ms);

        if (poll_result == RETRO_POLL_EVENT) {
            (void)desktop_dispatch_event(desktop, &event);
        } else if (poll_result == RETRO_POLL_CLOSED ||
                   poll_result == RETRO_POLL_ERROR) {
            desktop->running = false;
            return poll_result == RETRO_POLL_CLOSED
                       ? EXIT_SUCCESS
                       : EXIT_FAILURE;
        }

        desktop_update_status(desktop);
        if (desktop->needs_redraw) {
            desktop_render_frame(desktop);
        }
    }
""",
    """    desktop->running = true;
    while (desktop->running) {
        /* Background services are part of the one Desktop loop. Each app receives
           one bounded callback before the blocking platform poll. */
        desktop_service_apps(desktop);
        desktop_cleanup_apps(desktop);
        desktop_continue_shutdown(desktop);

        RetroEvent event = {0};
        RetroPollResult poll_result = platform_poll_event(
            desktop->platform, &event, desktop_poll_timeout_ms(desktop));

        if (poll_result == RETRO_POLL_EVENT) {
            (void)desktop_dispatch_event(desktop, &event);
        } else if (poll_result == RETRO_POLL_CLOSED ||
                   poll_result == RETRO_POLL_ERROR) {
            desktop->running = false;
            return poll_result == RETRO_POLL_CLOSED
                       ? EXIT_SUCCESS
                       : EXIT_FAILURE;
        }

        desktop_update_status(desktop);
        if (desktop->needs_redraw) {
            desktop_render_frame(desktop);
        }
    }
""",
)

replace_once(
    "tests/desktop_runtime_test.c",
    """#include "test_harness.h"
#include <stdbool.h>
""",
    """#include "test_harness.h"
#include <limits.h>
#include <stdbool.h>
""",
)
replace_once(
    "tests/desktop_runtime_test.c",
    """    const RetroEvent *events;
    size_t event_count;
    size_t event_index;
};

static int g_failing_create_calls;
""",
    """    const RetroEvent *events;
    size_t event_count;
    size_t event_index;
    size_t poll_count;
    int last_timeout_ms;
    int minimum_timeout_ms;
    bool close_when_exhausted;
};

static int g_failing_create_calls;
""",
)
replace_once(
    "tests/desktop_runtime_test.c",
    """static int g_failing_create_calls;
static int g_failing_destroy_calls;
static int g_failing_state_marker;
""",
    """static int g_failing_create_calls;
static int g_failing_destroy_calls;
static int g_failing_state_marker;
static size_t g_service_calls;
static size_t g_service_budget;
""",
)
replace_once(
    "tests/desktop_runtime_test.c",
    """    platform->features.linux_tty_keyboard_only = false;
    platform_stub_update_mask(&platform->features);
    return platform;
}
""",
    """    platform->features.linux_tty_keyboard_only = false;
    platform->minimum_timeout_ms = INT_MAX;
    platform_stub_update_mask(&platform->features);
    return platform;
}
""",
)
replace_once(
    "tests/desktop_runtime_test.c",
    """static const RetroAppDescriptor k_failing_create_descriptor = {
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
""",
    """static const RetroAppDescriptor k_failing_create_descriptor = {
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
""",
)
replace_once(
    "tests/desktop_runtime_test.c",
    """static RetroAppInstance create_untitled_notepad_with_clipboard(
    RetroClipboard *clipboard) {
""",
    """static void test_app_service_tick_contract(void) {
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
""",
)
replace_once(
    "tests/desktop_runtime_test.c",
    """    test_launch_and_clean_close();
    test_repeat_create_run_shutdown();
    test_notepad_escape_does_not_close();
""",
    """    test_launch_and_clean_close();
    test_repeat_create_run_shutdown();
    test_app_service_tick_contract();
    test_notepad_escape_does_not_close();
""",
)

replace_once(
    "docs/ARCHITECTURE.md",
    """## Event Flow

1. `platform_poll_event()` returns a normalized `RetroEvent`.
2. `core` handles unambiguous global commands such as function keys and control
   chords.
3. Taskbar mouse hit testing can consume a click before window routing.
4. `wm_handle_event()` routes remaining events to the focused or targeted
   window.
5. App callbacks update their state and may request close or redraw.
6. Desktop cleanup reconciles app requests with window existence and app
   `can_close` policy.
""",
    """## Event Flow

1. Optional app services receive one non-blocking, budgeted callback from the
   single Desktop loop.
2. `platform_poll_event()` returns a normalized `RetroEvent`.
3. `core` handles unambiguous global commands such as function keys and control
   chords.
4. Taskbar mouse hit testing can consume a click before window routing.
5. `wm_handle_event()` routes remaining events to the focused or targeted
   window.
6. App callbacks update their state and may request close or redraw.
7. Desktop cleanup reconciles app requests with window existence and app
   `can_close` policy.
""",
)
replace_once(
    "docs/ARCHITECTURE.md",
    """This allows Notepad to defer `Ctrl+W` while it displays Save/Discard/Cancel.

## Taskbar Model
""",
    """This allows Notepad to defer `Ctrl+W` while it displays Save/Discard/Cancel.

## App Service Policy

Apps that own asynchronous resources may expose `on_service`. Desktop invokes
that callback from the ordinary main loop with an 8 KiB work budget. A service
must never block, poll input, render directly, or flush a backend. Returning
`RETRO_APP_SERVICE_REDRAW` only marks the frame dirty. While at least one service
is active, the platform-event wait is capped at 16 ms; apps without services keep
the configured timeout. This is the foundation for PTY, network, or subprocess
adapters without introducing worker-owned UI state or a nested event loop.

## Taskbar Model
""",
)

replace_once(
    "docs/TESTING.md",
    """- desktop create/run/shutdown and partial-init rollback;
- capability-based app rejection and create-failure cleanup;
""",
    """- desktop create/run/shutdown and partial-init rollback;
- capability-based app rejection and create-failure cleanup;
- budgeted app services and active-service poll-timeout clamping;
""",
)
replace_once(
    "docs/TESTING.md",
    """- App-runtime changes test launch rejection, rollback, close, and cleanup.
""",
    """- App-runtime changes test launch rejection, rollback, service budgets,
  timeout policy, close, and cleanup.
""",
)
