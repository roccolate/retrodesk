#include "core/desktop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app/app_runtime.h"
#include "apps/apps.h"
#include "ui/statusbar.h"
#include "wm/window_manager.h"

typedef struct RunningApp {
    RetroAppInstance *app;
    WindowId window_id;
} RunningApp;

typedef enum LauncherAction {
    LAUNCHER_ACTION_FILEMANAGER = 0,
    LAUNCHER_ACTION_NOTEPAD,
    LAUNCHER_ACTION_TERMINAL,
    LAUNCHER_ACTION_CLOSE_ACTIVE,
    LAUNCHER_ACTION_CLOSE_MENU,
} LauncherAction;

typedef struct LauncherItem {
    const char *label;
    LauncherAction action;
} LauncherItem;

typedef struct LauncherState {
    bool open;
    int selected;
    WindowId window_id;
} LauncherState;

struct Desktop {
    PlatformBackend *platform;
    Renderer *renderer;
    AppRegistry *app_registry;
    WindowManager *wm;
    StatusBar *statusbar;
    DrawList *overlay_draw_list;
    const RetroTheme *theme;
    DesktopConfig config;
    DesktopCapabilities capabilities;
    DesktopDiagnostics diagnostics;
    bool running;
    bool needs_redraw;
    time_t last_status_tick;
    WindowId shell_window_id;
    LauncherState launcher;
    RunningApp *apps;
    size_t app_count;
    size_t app_capacity;
};

static const LauncherItem k_launcher_items[] = {
    {"Launch File Manager", LAUNCHER_ACTION_FILEMANAGER},
    {"Launch Notepad", LAUNCHER_ACTION_NOTEPAD},
    {"Launch Terminal", LAUNCHER_ACTION_TERMINAL},
    {"Close Active Window", LAUNCHER_ACTION_CLOSE_ACTIVE},
    {"Close Launcher", LAUNCHER_ACTION_CLOSE_MENU},
};

static bool desktop_app_is_running(const Desktop *desktop, const char *app_id) {
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
    if (!desktop || !app) return false;
    if (desktop->app_count == desktop->app_capacity) {
        size_t next_cap = desktop->app_capacity ? desktop->app_capacity * 2 : 8;
        RunningApp *next = realloc(desktop->apps, next_cap * sizeof(*next));
        if (!next) return false;
        desktop->apps = next;
        desktop->app_capacity = next_cap;
    }
    desktop->apps[desktop->app_count].app = app;
    desktop->apps[desktop->app_count].window_id = window_id;
    desktop->app_count++;
    return true;
}

static void desktop_remove_app_at(Desktop *desktop, size_t index) {
    if (!desktop || index >= desktop->app_count) return;
    app_destroy(desktop->apps[index].app);
    for (size_t i = index + 1; i < desktop->app_count; ++i) {
        desktop->apps[i - 1] = desktop->apps[i];
    }
    desktop->app_count--;
}

static void app_window_draw(RetroWindow *window, DrawList *draw_list, void *user_data) {
    (void)window;
    RetroAppInstance *app = (RetroAppInstance *)user_data;
    app_render(app, draw_list);
}

static void app_window_event(RetroWindow *window, const RetroEvent *event, void *user_data) {
    RetroAppInstance *app = (RetroAppInstance *)user_data;
    app_handle_event(app, event);
    if (app_is_close_requested(app)) {
        retro_window_request_close(window);
    }
}

static void desktop_launcher_close(Desktop *desktop) {
    if (!desktop || !desktop->launcher.open) return;
    if (desktop->launcher.window_id != WINDOW_ID_INVALID &&
        wm_window_exists(desktop->wm, desktop->launcher.window_id)) {
        wm_close_window(desktop->wm, desktop->launcher.window_id);
    }
    desktop->launcher.open = false;
    desktop->launcher.window_id = WINDOW_ID_INVALID;
}

static void desktop_launcher_execute(Desktop *desktop, LauncherAction action) {
    if (!desktop) return;
    switch (action) {
    case LAUNCHER_ACTION_FILEMANAGER:
        app_launch(desktop, "filemanager");
        break;
    case LAUNCHER_ACTION_NOTEPAD:
        app_launch(desktop, "notepad");
        break;
    case LAUNCHER_ACTION_TERMINAL:
        app_launch(desktop, "terminal");
        break;
    case LAUNCHER_ACTION_CLOSE_ACTIVE: {
        /* Close the launcher first so the previously-active window regains
           focus; then close whichever window is active now (excluding shell). */
        desktop_launcher_close(desktop);
        WindowId active = wm_active_window(desktop->wm);
        if (active != WINDOW_ID_INVALID && active != desktop->shell_window_id) {
            wm_close_window(desktop->wm, active);
        }
        desktop_request_redraw(desktop);
        return;
    }
    case LAUNCHER_ACTION_CLOSE_MENU:
    default:
        break;
    }
    desktop_launcher_close(desktop);
    desktop_request_redraw(desktop);
}

static void launcher_draw(RetroWindow *window, DrawList *draw_list, void *user_data) {
    (void)window;
    Desktop *desktop = (Desktop *)user_data;
    if (!desktop) return;

    const RenderStyle *text = &desktop->theme->launcher_text;
    const RenderStyle *selected = &desktop->theme->launcher_selected;

    draw_list_text(draw_list, 1, 2, "Launcher (non-blocking)", text);
    draw_list_text(draw_list, 2, 2,
                   "w/s or j/k to move, Enter to execute, Esc to close", text);

    size_t count = sizeof(k_launcher_items) / sizeof(k_launcher_items[0]);
    for (size_t i = 0; i < count; ++i) {
        const RenderStyle *row = ((int)i == desktop->launcher.selected) ? selected : text;
        draw_list_text(draw_list, 4 + (int)i, 3, k_launcher_items[i].label, row);
    }
}

static void launcher_event(RetroWindow *window, const RetroEvent *event, void *user_data) {
    (void)window;
    Desktop *desktop = (Desktop *)user_data;
    if (!desktop || !event || event->type != RETRO_EVENT_KEY) return;

    int key = event->data.key.key_code;
    unsigned char ch = event->data.key.ascii;
    size_t count = sizeof(k_launcher_items) / sizeof(k_launcher_items[0]);
    if (count == 0) return;

    if (key == 27 || ch == 'q') {
        desktop_launcher_close(desktop);
        return;
    }

    if (ch == 'w' || ch == 'k' || ch == 'W' || ch == 'K') {
        desktop->launcher.selected--;
        if (desktop->launcher.selected < 0) desktop->launcher.selected = (int)count - 1;
        return;
    }

    if (ch == 's' || ch == 'j' || ch == 'S' || ch == 'J') {
        desktop->launcher.selected = (desktop->launcher.selected + 1) % (int)count;
        return;
    }

    if (key == '\n' || key == '\r' || ch == ' ') {
        LauncherAction action = k_launcher_items[desktop->launcher.selected].action;
        desktop_launcher_execute(desktop, action);
    }
}

static void desktop_launcher_open(Desktop *desktop) {
    if (!desktop || desktop->launcher.open) return;

    int rows = 0;
    int cols = 0;
    renderer_get_screen_size(desktop->renderer, &rows, &cols);

    int h = 12;
    int w = 52;
    if (h > rows - 1) h = rows - 1;
    if (w > cols) w = cols;
    if (h < 6 || w < 20) return;

    RetroWindowSpec spec = {
        .height = h,
        .width = w,
        .y = (rows - h) / 2,
        .x = (cols - w) / 2,
        .title = "Launcher",
        .flags = WINDOW_FLAG_MODAL | WINDOW_FLAG_POPUP | WINDOW_FLAG_FIXED,
        .draw_cb = launcher_draw,
        .event_cb = launcher_event,
        .user_data = desktop,
    };

    WindowId wid = wm_create_window(desktop->wm, &spec);
    if (wid == WINDOW_ID_INVALID) return;

    desktop->launcher.open = true;
    desktop->launcher.selected = 0;
    desktop->launcher.window_id = wid;
    wm_focus_window(desktop->wm, wid);
    wm_bring_to_front(desktop->wm, wid);
    desktop_request_redraw(desktop);
}

static void shell_draw(RetroWindow *window, DrawList *draw_list, void *user_data) {
    Desktop *desktop = (Desktop *)user_data;
    const RenderStyle *text = &desktop->theme->shell_text;
    const RenderStyle *accent = &desktop->theme->shell_accent;
    char line[128];
    int y = 0, x = 0, h = 0, w = 0;

    retro_window_get_geometry(window, &y, &x, &h, &w);

    draw_list_text(draw_list, 1, 2, "RetroDesk Foundation Runtime", accent);
    draw_list_text(
        draw_list, 2, 2,
        "Hotkeys: q quit | Tab focus | m launcher | 1/2/3 apps | w close | HJKL move | Esc close app",
        text);

    snprintf(line, sizeof(line), "Window: %dx%d @ %d,%d", w, h, x, y);
    draw_list_text(draw_list, 4, 2, line, text);

    snprintf(line, sizeof(line), "Input: %s | Render: %s | mouse=%s drag=%s",
             desktop->diagnostics.backend_name,
             desktop->diagnostics.render_backend_name,
             desktop->diagnostics.mouse_enabled ? "on" : "off",
             desktop->diagnostics.drag_enabled ? "on" : "off");
    draw_list_text(draw_list, 5, 2, line, text);

    snprintf(line, sizeof(line), "resize=%s linux_tty_keyboard_only=%s",
             desktop->diagnostics.resize_enabled ? "on" : "off",
             desktop->diagnostics.linux_tty_keyboard_only ? "yes" : "no");
    draw_list_text(draw_list, 6, 2, line, text);

    if (desktop->diagnostics.drag_degraded) {
        draw_list_text(draw_list, 7, 2, "Drag degraded automatically: use H/J/K/L", text);
    }
}

static void desktop_fill_capabilities(Desktop *desktop,
                                      const PlatformFeatures *platform_features) {
    desktop->capabilities = *platform_features;
}

static void desktop_fill_diagnostics(Desktop *desktop) {
    desktop->diagnostics.backend_name = platform_backend_name(desktop->platform);
    desktop->diagnostics.render_backend_name =
        desktop->renderer ? renderer_backend_name(desktop->renderer) : "unknown";
    desktop->diagnostics.theme_name =
        desktop->theme ? desktop->theme->name : retro_theme_name(RETRO_THEME_XP);
    desktop->diagnostics.mouse_enabled = desktop->capabilities.mouse_basic;
    if (desktop->wm) {
        desktop->diagnostics.drag_enabled = wm_drag_is_enabled(desktop->wm);
        desktop->diagnostics.drag_degraded = wm_drag_is_degraded(desktop->wm);
        desktop->capabilities.drag_reliable = desktop->diagnostics.drag_enabled;
    } else {
        desktop->diagnostics.drag_enabled = desktop->capabilities.drag_reliable;
        desktop->diagnostics.drag_degraded = false;
    }
    desktop->diagnostics.resize_enabled = desktop->capabilities.resize_events;
    desktop->diagnostics.linux_tty_keyboard_only =
        desktop->capabilities.linux_tty_keyboard_only;
}

Desktop *desktop_create(const DesktopConfig *config) {
    if (!config || !config->platform) return NULL;

    Desktop *desktop = calloc(1, sizeof(*desktop));
    if (!desktop) return NULL;

    desktop->platform = config->platform;
    desktop->config = *config;
    if (desktop->config.input_timeout_ms <= 0) desktop->config.input_timeout_ms = 100;
    if (desktop->config.render_backend == RENDER_BACKEND_AUTO) {
        desktop->config.render_backend = RENDER_BACKEND_CURSES;
    }
    desktop->theme = retro_theme_get(desktop->config.theme_kind);
    desktop->last_status_tick = (time_t)-1;
    desktop->launcher.window_id = WINDOW_ID_INVALID;

    desktop->app_registry = app_registry_create();
    if (!desktop->app_registry) goto fail;

    const PlatformFeatures *features = platform_features(desktop->platform);
    if (!features) goto fail;

    desktop_fill_capabilities(desktop, features);

    desktop->renderer =
        renderer_create_with_backend(desktop->platform, desktop->config.render_backend);
    if (!desktop->renderer) goto fail;

    desktop->wm = wm_create(desktop->renderer);
    if (!desktop->wm) goto fail;

    wm_set_drag_enabled(desktop->wm, desktop->capabilities.drag_reliable);
    wm_set_drag_auto_degrade(desktop->wm, desktop->capabilities.linux_tty_keyboard_only);

    desktop->statusbar = statusbar_create();
    if (!desktop->statusbar) goto fail;

    desktop->overlay_draw_list = draw_list_create();
    if (!desktop->overlay_draw_list) goto fail;

    RetroWindowSpec shell_spec = {
        .height = 10,
        .width = 72,
        .y = 1,
        .x = 2,
        .title = "Desktop",
        .flags = WINDOW_FLAG_FIXED,
        .draw_cb = shell_draw,
        .event_cb = NULL,
        .user_data = desktop,
    };
    desktop->shell_window_id = wm_create_window(desktop->wm, &shell_spec);
    if (desktop->shell_window_id == WINDOW_ID_INVALID) goto fail;

    desktop_fill_diagnostics(desktop);

    apps_register_builtin(desktop->app_registry);
    app_launch(desktop, "filemanager");
    app_launch(desktop, "notepad");

    desktop->needs_redraw = true;
    return desktop;

fail:
    desktop_shutdown(desktop);
    return NULL;
}

RetroAppInstance *app_launch(Desktop *desktop, const char *app_id) {
    if (!desktop || !app_id) return NULL;

    /* If an instance of this app is already running, focus it instead. */
    if (desktop_app_is_running(desktop, app_id)) {
        for (size_t i = 0; i < desktop->app_count; ++i) {
            const RetroAppInstance *inst = desktop->apps[i].app;
            if (inst && inst->descriptor && inst->descriptor->app_id &&
                strcmp(inst->descriptor->app_id, app_id) == 0) {
                wm_focus_window(desktop->wm, desktop->apps[i].window_id);
                wm_bring_to_front(desktop->wm, desktop->apps[i].window_id);
                desktop->needs_redraw = true;
                return NULL;
            }
        }
    }

    const RetroAppDescriptor *desc = app_registry_find(desktop->app_registry, app_id);
    if (!desc) return NULL;

    if ((desc->required_capabilities & desktop->capabilities.capability_mask) !=
        desc->required_capabilities) {
        return NULL;
    }

    RetroAppInstance *instance = calloc(1, sizeof(*instance));
    if (!instance) return NULL;

    instance->descriptor = desc;
    instance->ctx.desktop = desktop;
    instance->ctx.capabilities = &desktop->capabilities;
    instance->ctx.theme = desktop->theme;

    RetroWindowSpec spec = {
        .height = desc->default_height,
        .width = desc->default_width,
        .y = desc->default_y,
        .x = desc->default_x,
        .title = desc->display_name,
        .flags = desc->window_flags,
        .draw_cb = app_window_draw,
        .event_cb = app_window_event,
        .user_data = instance,
    };

    WindowId wid = wm_create_window(desktop->wm, &spec);
    if (wid == WINDOW_ID_INVALID) {
        free(instance);
        return NULL;
    }
    instance->ctx.window_id = wid;

    if (desc->create && !desc->create(instance, &instance->ctx)) {
        if (desc->destroy) desc->destroy(instance);
        wm_close_window(desktop->wm, wid);
        free(instance);
        return NULL;
    }

    if (!desktop_add_app(desktop, instance, wid)) {
        if (desc->destroy) desc->destroy(instance);
        wm_close_window(desktop->wm, wid);
        free(instance);
        return NULL;
    }

    desktop->needs_redraw = true;
    return instance;
}

void desktop_request_redraw(Desktop *desktop) {
    if (!desktop) return;
    desktop->needs_redraw = true;
}

const DesktopDiagnostics *desktop_diagnostics(const Desktop *desktop) {
    if (!desktop) return NULL;
    return &desktop->diagnostics;
}

size_t desktop_app_count(const Desktop *desktop) {
    if (!desktop) return 0;
    return desktop->app_count;
}

size_t desktop_window_count(const Desktop *desktop) {
    if (!desktop || !desktop->wm) return 0;
    return wm_window_count(desktop->wm);
}

WindowId desktop_active_window(const Desktop *desktop) {
    if (!desktop || !desktop->wm) return WINDOW_ID_INVALID;
    return wm_active_window(desktop->wm);
}

WindowId desktop_app_window_id(const Desktop *desktop, const char *app_id) {
    if (!desktop || !app_id) return WINDOW_ID_INVALID;

    for (size_t i = 0; i < desktop->app_count; ++i) {
        const RetroAppInstance *inst = desktop->apps[i].app;
        if (inst && inst->descriptor && inst->descriptor->app_id &&
            strcmp(inst->descriptor->app_id, app_id) == 0) {
            return desktop->apps[i].window_id;
        }
    }

    return WINDOW_ID_INVALID;
}

bool desktop_register_app_for_test(Desktop *desktop, const RetroAppDescriptor *desc) {
    if (!desktop || !desktop->app_registry || !desc) return false;
    return app_registry_register(desktop->app_registry, desc);
}

static void desktop_cleanup_apps(Desktop *desktop) {
    if (!desktop) return;

    if (desktop->launcher.open && desktop->launcher.window_id != WINDOW_ID_INVALID &&
        !wm_window_exists(desktop->wm, desktop->launcher.window_id)) {
        desktop->launcher.open = false;
        desktop->launcher.window_id = WINDOW_ID_INVALID;
    }

    for (size_t i = 0; i < desktop->app_count;) {
        RunningApp *slot = &desktop->apps[i];
        bool should_close = app_is_close_requested(slot->app);
        bool window_exists = wm_window_exists(desktop->wm, slot->window_id);

        if (should_close && window_exists) {
            wm_close_window(desktop->wm, slot->window_id);
            window_exists = false;
        }

        if (!window_exists || should_close) {
            desktop_remove_app_at(desktop, i);
            continue;
        }
        ++i;
    }
}

static void desktop_update_status(Desktop *desktop) {
    time_t now = time(NULL);
    if (now == desktop->last_status_tick && !desktop->needs_redraw) return;
    desktop->last_status_tick = now;

    desktop->diagnostics.drag_enabled = wm_drag_is_enabled(desktop->wm);
    desktop->diagnostics.drag_degraded = wm_drag_is_degraded(desktop->wm);

    struct tm tm_buf;
    struct tm *tm_now = NULL;
#if defined(_WIN32)
    if (localtime_s(&tm_buf, &now) == 0) tm_now = &tm_buf;
#else
    tm_now = localtime_r(&now, &tm_buf);
#endif
    char timestr[32] = "--:--:--";
    if (tm_now) strftime(timestr, sizeof(timestr), "%H:%M:%S", tm_now);

    WindowId drag_id = WINDOW_ID_INVALID;
    int drag_y = 0;
    int drag_x = 0;
    bool dragging = wm_get_drag_preview(desktop->wm, &drag_id, &drag_y, &drag_x);

    const char *mouse = desktop->diagnostics.mouse_enabled ? "on" : "off";
    const char *drag = desktop->diagnostics.drag_enabled ? "on" : "off";
    char middle[96];

    if (desktop->diagnostics.drag_degraded) {
        snprintf(middle, sizeof(middle), "mouse=%s drag=auto-off(%d) use HJKL", mouse,
                 wm_drag_no_motion_sessions(desktop->wm));
    } else if (dragging) {
        snprintf(middle, sizeof(middle), "mouse=%s drag=%s #%d->%d,%d", mouse, drag, drag_id,
                 drag_x, drag_y);
    } else {
        snprintf(middle, sizeof(middle), "mouse=%s drag=%s", mouse, drag);
    }

    statusbar_set_text(
        desktop->statusbar,
        "RetroDesk | windows=%zu apps=%zu | in=%s out=%s theme=%s | %s | %s",
        wm_window_count(desktop->wm), desktop->app_count,
        desktop->diagnostics.backend_name, desktop->diagnostics.render_backend_name,
        desktop->diagnostics.theme_name, middle, timestr);
}

static void desktop_render_frame(Desktop *desktop) {
    const RenderStyle *status_style = &desktop->theme->statusbar;
    int rows = 0;
    int cols = 0;

    renderer_get_screen_size(desktop->renderer, &rows, &cols);
    renderer_clear(desktop->renderer);
    wm_render(desktop->wm, desktop->renderer, desktop->theme);

    RenderContext *screen = renderer_begin_screen(desktop->renderer);
    if (screen) {
        draw_list_reset(desktop->overlay_draw_list);
        statusbar_render(desktop->statusbar, desktop->overlay_draw_list, rows, cols,
                         status_style);
        renderer_draw_list(screen, desktop->overlay_draw_list);
        renderer_end(desktop->renderer, screen);
    }

    renderer_flush(desktop->renderer);
    desktop->needs_redraw = false;
}

static bool desktop_handle_key_command(Desktop *desktop, const RetroKeyEvent *key) {
    if (!desktop || !key) return false;

    /* While the launcher (modal) is open, only 'q' (quit) and 'm' (toggle)
       are handled at desktop level; everything else goes to the modal. */
    if (desktop->launcher.open) {
        if (key->ascii == 'q') {
            desktop->running = false;
            return true;
        }
        if (key->ascii == 'm' || key->ascii == 'M') {
            desktop_launcher_close(desktop);
            return true;
        }
        return false;
    }

    if (key->ascii == 'q') {
        desktop->running = false;
        return true;
    }

    if (key->ascii == 'm' || key->ascii == 'M') {
        desktop_launcher_open(desktop);
        return true;
    }

    if (key->key_code == '\t') {
        wm_cycle_focus(desktop->wm);
        desktop_request_redraw(desktop);
        return true;
    }

    if (key->ascii == 'H' || key->ascii == 'J' || key->ascii == 'K' ||
        key->ascii == 'L') {
        int dy = (key->ascii == 'J') - (key->ascii == 'K');
        int dx = (key->ascii == 'L') - (key->ascii == 'H');
        if (wm_move_active_window(desktop->wm, dy, dx)) {
            desktop_request_redraw(desktop);
        }
        return true;
    }

    switch (key->ascii) {
    case '1':
        app_launch(desktop, "filemanager");
        return true;
    case '2':
        app_launch(desktop, "notepad");
        return true;
    case '3':
        app_launch(desktop, "terminal");
        return true;
    case 'w': {
        WindowId active = wm_active_window(desktop->wm);
        if (active != WINDOW_ID_INVALID && active != desktop->shell_window_id) {
            wm_close_window(desktop->wm, active);
        }
        return true;
    }
    default:
        return false;
    }
}

int desktop_run(Desktop *desktop) {
    if (!desktop) return EXIT_FAILURE;

    if (desktop->config.bench_mode) {
        desktop_update_status(desktop);
        desktop_render_frame(desktop);
        return EXIT_SUCCESS;
    }

    desktop->running = true;
    while (desktop->running) {
        RetroEvent event = {0};
        bool has_event = platform_poll_event(desktop->platform, &event,
                                             desktop->config.input_timeout_ms);

        if (has_event) {
            bool consumed = false;
            if (event.type == RETRO_EVENT_KEY) {
                consumed = desktop_handle_key_command(desktop, &event.data.key);
            }
            if (!consumed) {
                wm_handle_event(desktop->wm, &event);
            }
            desktop_cleanup_apps(desktop);
            desktop_request_redraw(desktop);
        }

        desktop_update_status(desktop);
        if (desktop->needs_redraw) {
            desktop_render_frame(desktop);
        }
    }

    return EXIT_SUCCESS;
}

void desktop_shutdown(Desktop *desktop) {
    if (!desktop) return;

    for (size_t i = 0; i < desktop->app_count; ++i) {
        app_destroy(desktop->apps[i].app);
    }
    free(desktop->apps);
    desktop->apps = NULL;
    desktop->app_count = 0;
    desktop->app_capacity = 0;

    app_registry_destroy(desktop->app_registry);
    desktop->app_registry = NULL;
    draw_list_destroy(desktop->overlay_draw_list);
    statusbar_destroy(desktop->statusbar);
    wm_destroy(desktop->wm);
    renderer_destroy(desktop->renderer);
    free(desktop);
}
