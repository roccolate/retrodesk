#include "core/desktop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app/app_runtime.h"
#include "core/checked_size.h"
#include "core/desktop_chrome.h"
#include "apps/apps.h"
#include "core/key_chord.h"
#include "ui/launcher_menu.h"
#include "ui/statusbar.h"
#include "ui/theme_surface.h"
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
    const char *detail;
    char accelerator;
    LauncherAction action;
} LauncherItem;

typedef struct LauncherState {
    bool open;
    int selected;
    WindowId window_id;
} LauncherState;

static const int k_active_service_poll_ms = 16;
static const size_t k_app_service_budget = 8192;

struct Desktop {
    PlatformBackend *platform;
    Renderer *renderer;
    AppRegistry *app_registry;
    RetroClipboard *clipboard;
    WindowManager *wm;
    StatusBar *statusbar;
    DrawList *overlay_draw_list;
    const RetroTheme *theme;
    DesktopConfig config;
    DesktopCapabilities capabilities;
    DesktopDiagnostics diagnostics;
    bool running;
    bool needs_redraw;
    bool window_mode;
    bool window_resize_mode;
    bool shutdown_requested;
    size_t shutdown_index;
    WindowId shutdown_waiting_window;
    time_t last_status_tick;
    WindowId shell_window_id;
    LauncherState launcher;
    RunningApp *apps;
    size_t app_count;
    size_t app_capacity;
    bool fail_next_app_growth_for_test;
};

static const LauncherItem k_launcher_items[] = {
    {"Files", "Browse files", 'F', LAUNCHER_ACTION_FILEMANAGER},
    {"Notepad", "Edit text", 'N', LAUNCHER_ACTION_NOTEPAD},
    {"Diagnostics", "Runtime status", 'D', LAUNCHER_ACTION_TERMINAL},
    {"Close active window", "Ctrl+W", 'X', LAUNCHER_ACTION_CLOSE_ACTIVE},
    {"Close menu", "Esc", 'Q', LAUNCHER_ACTION_CLOSE_MENU},
};

static LauncherMenuSnapshot desktop_launcher_snapshot(
    const Desktop *desktop) {
    LauncherMenuSnapshot snapshot = {0};
    snapshot.brand = "RetroDesk";
    snapshot.section_label = "Applications";
    snapshot.item_count =
        sizeof(k_launcher_items) / sizeof(k_launcher_items[0]);
    if (snapshot.item_count > LAUNCHER_MENU_MAX_ITEMS) {
        snapshot.item_count = LAUNCHER_MENU_MAX_ITEMS;
    }
    snapshot.primary_count = snapshot.item_count < 3
                                 ? snapshot.item_count
                                 : 3;
    snapshot.selected = desktop ? desktop->launcher.selected : 0;
    for (size_t i = 0; i < snapshot.item_count; ++i) {
        snapshot.items[i] = (LauncherMenuItemView){
            k_launcher_items[i].label,
            k_launcher_items[i].detail,
            k_launcher_items[i].accelerator,
        };
    }
    return snapshot;
}

typedef struct TaskbarCatalogItem {
    const char *app_id;
    const char *label;
} TaskbarCatalogItem;

static const TaskbarCatalogItem k_taskbar_catalog[] = {
    {"filemanager", "Files"},
    {"notepad", "Notepad"},
    {"diagnostics", "Diag"},
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

static bool desktop_add_app(Desktop *desktop, RetroAppInstance *app,
                  WindowId window_id) {
    if (!desktop || !app || desktop->app_count == SIZE_MAX) return false;

    size_t required_count = desktop->app_count + 1;
    if (required_count > desktop->app_capacity) {
        size_t next_capacity = 0;
        if (!retro_checked_capacity_grow(desktop->app_capacity, required_count,
                               sizeof(*desktop->apps), 8,
                               &next_capacity)) {
  return false;
        }
        if (desktop->fail_next_app_growth_for_test) {
  desktop->fail_next_app_growth_for_test = false;
  return false;
        }

        RunningApp *next = realloc(desktop->apps,
                         next_capacity * sizeof(*desktop->apps));
        if (!next) return false;
        desktop->apps = next;
        desktop->app_capacity = next_capacity;
    }

    desktop->apps[desktop->app_count].app = app;
    desktop->apps[desktop->app_count].window_id = window_id;
    desktop->app_count = required_count;
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
        Desktop *desktop = app->ctx.desktop;
        if (!desktop || !desktop->shutdown_requested) {
            retro_window_request_close(window);
        }
    }
}

static bool desktop_request_app_close(Desktop *desktop, WindowId window_id) {
    if (!desktop || window_id == WINDOW_ID_INVALID) return false;
    for (size_t i = 0; i < desktop->app_count; ++i) {
        RunningApp *running = &desktop->apps[i];
        if (running->window_id != window_id || !running->app) continue;
        RetroCloseResult result = app_request_close(running->app);
        if (result != RETRO_CLOSE_ALLOWED) return false;
        wm_close_window(desktop->wm, window_id);
        return true;
    }
    return false;
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
        app_launch(desktop, "diagnostics");
        break;
    case LAUNCHER_ACTION_CLOSE_ACTIVE: {
        /* Close the launcher first so the previously-active window regains
           focus; then close whichever window is active now (excluding shell). */
        desktop_launcher_close(desktop);
        WindowId active = wm_active_window(desktop->wm);
        if (active != WINDOW_ID_INVALID && active != desktop->shell_window_id) {
            (void)desktop_request_app_close(desktop, active);
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

static void launcher_draw(RetroWindow *window, DrawList *draw_list,
                          void *user_data) {
    Desktop *desktop = (Desktop *)user_data;
    if (!desktop || !window || !draw_list) return;

    int height = 0;
    int width = 0;
    retro_window_get_geometry(window, NULL, NULL, &height, &width);
    LauncherMenuSnapshot snapshot = desktop_launcher_snapshot(desktop);
    const RetroSurfaceTheme *surface =
        retro_surface_theme_get(desktop->config.theme_kind);
    LauncherMenuStyles styles = {
        .header = surface->launcher_header,
        .section = surface->launcher_section,
        .item = surface->launcher_item,
        .selected = surface->launcher_selected,
        .separator = surface->launcher_separator,
        .footer = surface->launcher_footer,
    };
    launcher_menu_render_styled(&snapshot, draw_list, height, width, &styles);
}

static void launcher_select(Desktop *desktop,
                            const LauncherMenuSnapshot *snapshot,
                            int target) {
    if (!desktop || !snapshot) return;
    int selected = launcher_menu_normalize_selection(snapshot, target);
    if (selected < 0 || selected == desktop->launcher.selected) return;
    desktop->launcher.selected = selected;
    desktop_request_redraw(desktop);
}

static void launcher_execute_selected(Desktop *desktop) {
    if (!desktop) return;
    LauncherMenuSnapshot snapshot = desktop_launcher_snapshot(desktop);
    int selected = launcher_menu_normalize_selection(
        &snapshot, desktop->launcher.selected);
    if (selected < 0 || (size_t)selected >= snapshot.item_count) return;
    desktop_launcher_execute(desktop, k_launcher_items[selected].action);
}

static void launcher_event(RetroWindow *window, const RetroEvent *event,
                           void *user_data) {
    Desktop *desktop = (Desktop *)user_data;
    if (!desktop || !window || !event) return;

    LauncherMenuSnapshot snapshot = desktop_launcher_snapshot(desktop);
    if (launcher_menu_count(&snapshot) == 0) return;

    if (event->type == RETRO_EVENT_MOUSE) {
        const RetroMouseEvent *mouse = &event->data.mouse;
        if (!mouse->has_local_coordinates) return;
        int height = 0;
        int width = 0;
        retro_window_get_geometry(window, NULL, NULL, &height, &width);
        int hit = launcher_menu_hit_test(&snapshot, mouse->local_y,
                                         mouse->local_x, height, width);
        if (hit < 0) return;
        if (mouse->moved || mouse->button1_pressed ||
            mouse->button1_clicked) {
            launcher_select(desktop, &snapshot, hit);
        }
        if (mouse->button1_clicked) launcher_execute_selected(desktop);
        return;
    }

    if (event->type != RETRO_EVENT_KEY) return;
    int key = event->data.key.key_code;
    unsigned char ch = event->data.key.ascii;

    if (key == RETRO_KEY_ESC || ch == 'q' || ch == 'Q') {
        desktop_launcher_close(desktop);
        return;
    }
    if (key == RETRO_KEY_UP || ch == 'w' || ch == 'W' ||
        ch == 'k' || ch == 'K') {
        launcher_select(desktop, &snapshot,
                        launcher_menu_move_selection(
                            &snapshot, desktop->launcher.selected, -1));
        return;
    }
    if (key == RETRO_KEY_DOWN || ch == 's' || ch == 'S' ||
        ch == 'j' || ch == 'J') {
        launcher_select(desktop, &snapshot,
                        launcher_menu_move_selection(
                            &snapshot, desktop->launcher.selected, 1));
        return;
    }
    if (key == RETRO_KEY_HOME) {
        launcher_select(desktop, &snapshot, 0);
        return;
    }
    if (key == RETRO_KEY_END) {
        launcher_select(desktop, &snapshot,
                        (int)launcher_menu_count(&snapshot) - 1);
        return;
    }

    int accelerator = launcher_menu_find_accelerator(&snapshot, ch);
    if (accelerator >= 0) {
        launcher_select(desktop, &snapshot, accelerator);
        launcher_execute_selected(desktop);
        return;
    }
    if (key == RETRO_KEY_LF || key == RETRO_KEY_CR || ch == ' ') {
        launcher_execute_selected(desktop);
    }
}

static void desktop_launcher_open(Desktop *desktop) {
    if (!desktop || desktop->launcher.open) return;

    int rows = 0;
    int cols = 0;
    renderer_get_screen_size(desktop->renderer, &rows, &cols);
    desktop->launcher.selected = 0;
    LauncherMenuSnapshot snapshot = desktop_launcher_snapshot(desktop);

    int h = launcher_menu_preferred_height(&snapshot);
    int w = LAUNCHER_MENU_PREFERRED_WIDTH;
    if (h > rows - 1) h = rows - 1;
    if (w > cols) w = cols;
    if (h < 6 || w < LAUNCHER_MENU_MIN_WIDTH) return;

    int y = rows - 1 - h;
    if (y < 0) y = 0;
    RetroWindowSpec spec = {
        .height = h,
        .width = w,
        .y = y,
        .x = 0,
        .title = "RetroDesk",
        .flags = WINDOW_FLAG_MODAL | WINDOW_FLAG_POPUP | WINDOW_FLAG_FIXED,
        .draw_cb = launcher_draw,
        .event_cb = launcher_event,
        .user_data = desktop,
    };

    WindowId wid = wm_create_window(desktop->wm, &spec);
    if (wid == WINDOW_ID_INVALID) return;

    desktop->launcher.open = true;
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
        "F1/F10 launcher | F6 focus | F9 move/resize | Ctrl+W close | Ctrl+Q quit",
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
    desktop->shutdown_waiting_window = WINDOW_ID_INVALID;

    desktop->app_registry = app_registry_create();
    if (!desktop->app_registry) goto fail;

    desktop->clipboard = retro_clipboard_create();
    if (!desktop->clipboard) goto fail;

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

    desktop->needs_redraw = true;
    return desktop;

fail:
    desktop_shutdown(desktop);
    return NULL;
}

RetroAppInstance *app_launch_with_path(Desktop *desktop, const char *app_id,
                                       const char *resource_path) {
    if (!desktop || !app_id) return NULL;

    /* If an instance of this app is already running, focus it instead. */
    const RetroAppDescriptor *existing_desc =
        app_registry_find(desktop->app_registry, app_id);
    if (existing_desc && !existing_desc->allow_multiple &&
        desktop_app_is_running(desktop, app_id)) {
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
    instance->ctx.clipboard = desktop->clipboard;
    instance->ctx.capabilities = &desktop->capabilities;
    instance->ctx.theme = desktop->theme;
    if (resource_path) {
        instance->resource_path_owned = strdup(resource_path);
        if (!instance->resource_path_owned) {
            free(instance);
            return NULL;
        }
        instance->ctx.resource_path = instance->resource_path_owned;
    }

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
        free(instance->resource_path_owned);
        free(instance);
        return NULL;
    }
    instance->ctx.window_id = wid;

    if (desc->create && !desc->create(instance, &instance->ctx)) {
        /* create may have acquired partial state; once invoked, destroy is
           always the matching rollback hook, even when create reports failure. */
        if (desc->destroy) desc->destroy(instance);
        wm_close_window(desktop->wm, wid);
        free(instance->resource_path_owned);
        free(instance);
        return NULL;
    }

    if (!desktop_add_app(desktop, instance, wid)) {
        if (desc->destroy) desc->destroy(instance);
        wm_close_window(desktop->wm, wid);
        free(instance->resource_path_owned);
        free(instance);
        return NULL;
    }

    desktop->needs_redraw = true;
    return instance;
}

RetroAppInstance *app_launch(Desktop *desktop, const char *app_id) {
    return app_launch_with_path(desktop, app_id, NULL);
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

void desktop_fail_next_app_growth_for_test(Desktop *desktop) {
    if (desktop) desktop->fail_next_app_growth_for_test = true;
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

        if (should_close && window_exists && !desktop->shutdown_requested) {
            wm_close_window(desktop->wm, slot->window_id);
            window_exists = false;
        }

        if (!window_exists || (should_close && !desktop->shutdown_requested)) {
            desktop_remove_app_at(desktop, i);
            continue;
        }
        ++i;
    }
}

static RunningApp *desktop_find_running_app(Desktop *desktop,
                                            WindowId window_id) {
    if (!desktop || window_id == WINDOW_ID_INVALID) return NULL;
    for (size_t i = 0; i < desktop->app_count; ++i) {
        if (desktop->apps[i].window_id == window_id) return &desktop->apps[i];
    }
    return NULL;
}

static void desktop_reset_shutdown_requests(Desktop *desktop) {
    if (!desktop) return;
    for (size_t i = 0; i < desktop->app_count; ++i) {
        app_reset_close_request(desktop->apps[i].app);
    }
}

static void desktop_cancel_shutdown(Desktop *desktop) {
    if (!desktop) return;
    desktop_reset_shutdown_requests(desktop);
    desktop->shutdown_requested = false;
    desktop->shutdown_index = 0;
    desktop->shutdown_waiting_window = WINDOW_ID_INVALID;
    desktop_request_redraw(desktop);
}

static void desktop_commit_shutdown(Desktop *desktop) {
    if (!desktop) return;
    desktop->shutdown_requested = false;
    desktop->shutdown_waiting_window = WINDOW_ID_INVALID;
    for (size_t i = 0; i < desktop->app_count; ++i) {
        if (wm_window_exists(desktop->wm, desktop->apps[i].window_id)) {
            wm_close_window(desktop->wm, desktop->apps[i].window_id);
        }
    }
    desktop_cleanup_apps(desktop);
    desktop->shutdown_index = 0;
    desktop->running = false;
}

static void desktop_continue_shutdown(Desktop *desktop) {
    if (!desktop || !desktop->shutdown_requested) return;

    if (desktop->shutdown_waiting_window != WINDOW_ID_INVALID) {
        RunningApp *waiting = desktop_find_running_app(
            desktop, desktop->shutdown_waiting_window);
        if (!waiting || !waiting->app) {
            desktop_cancel_shutdown(desktop);
            return;
        }
        if (app_is_close_pending(waiting->app)) return;
        if (app_close_result(waiting->app) == RETRO_CLOSE_CANCELLED) {
            desktop_cancel_shutdown(desktop);
            return;
        }
        if (!app_is_close_requested(waiting->app)) {
            desktop_cancel_shutdown(desktop);
            return;
        }
        desktop->shutdown_waiting_window = WINDOW_ID_INVALID;
        desktop->shutdown_index++;
    }

    while (desktop->shutdown_index < desktop->app_count) {
        RunningApp *running = &desktop->apps[desktop->shutdown_index];
        RetroCloseResult result = app_request_close(running->app);
        if (result == RETRO_CLOSE_ALLOWED) {
            desktop->shutdown_index++;
            continue;
        }
        if (result == RETRO_CLOSE_CANCELLED) {
            desktop_cancel_shutdown(desktop);
            return;
        }
        desktop->shutdown_waiting_window = running->window_id;
        wm_focus_window(desktop->wm, running->window_id);
        wm_bring_to_front(desktop->wm, running->window_id);
        desktop_request_redraw(desktop);
        return;
    }

    desktop_commit_shutdown(desktop);
}

static void desktop_begin_shutdown(Desktop *desktop) {
    if (!desktop || desktop->shutdown_requested) return;
    if (desktop->launcher.open) desktop_launcher_close(desktop);
    desktop->window_mode = false;
    desktop->window_resize_mode = false;
    desktop->shutdown_requested = true;
    desktop->shutdown_index = 0;
    desktop->shutdown_waiting_window = WINDOW_ID_INVALID;
    desktop_continue_shutdown(desktop);
}

static void desktop_update_taskbar_snapshot(Desktop *desktop,
                                            const char *clock_text) {
    if (!desktop || !desktop->statusbar) return;

    StatusBarSnapshot snapshot = {0};
    snapshot.menu_open = desktop->launcher.open;
    snprintf(snapshot.clock_text, sizeof(snapshot.clock_text), "%s",
             clock_text ? clock_text : "--:--:--");

    size_t catalog_count =
        sizeof(k_taskbar_catalog) / sizeof(k_taskbar_catalog[0]);
    if (catalog_count > STATUSBAR_MAX_APPS) {
        catalog_count = STATUSBAR_MAX_APPS;
    }
    snapshot.app_count = catalog_count;

    WindowId active = wm_active_window(desktop->wm);
    for (size_t catalog_index = 0;
         catalog_index < catalog_count; ++catalog_index) {
        const TaskbarCatalogItem *catalog =
            &k_taskbar_catalog[catalog_index];
        StatusBarAppSnapshot *app =
            &snapshot.apps[catalog_index];
        app->app_id = catalog->app_id;
        app->label = catalog->label;

        for (size_t i = 0; i < desktop->app_count; ++i) {
            RetroAppInstance *instance = desktop->apps[i].app;
            if (!instance || !instance->descriptor ||
                !instance->descriptor->app_id ||
                strcmp(instance->descriptor->app_id,
                       catalog->app_id) != 0) {
                continue;
            }
            app->instance_count++;
            if (desktop->apps[i].window_id == active) {
                app->focused = true;
            }
        }
    }
    statusbar_set_snapshot(desktop->statusbar, &snapshot);
}

static void desktop_update_status(Desktop *desktop) {
    time_t now = time(NULL);
    if (now == desktop->last_status_tick && !desktop->needs_redraw) {
        return;
    }
    bool tick_changed = now != desktop->last_status_tick;
    desktop->last_status_tick = now;

    desktop->diagnostics.drag_enabled =
        wm_drag_is_enabled(desktop->wm);
    desktop->diagnostics.drag_degraded =
        wm_drag_is_degraded(desktop->wm);

    struct tm tm_buf;
    struct tm *tm_now = NULL;
#if defined(_WIN32)
    if (localtime_s(&tm_buf, &now) == 0) tm_now = &tm_buf;
#else
    tm_now = localtime_r(&now, &tm_buf);
#endif
    char timestr[9] = "--:--:--";
    if (tm_now) {
        strftime(timestr, sizeof(timestr), "%H:%M:%S", tm_now);
    }
    desktop_update_taskbar_snapshot(desktop, timestr);

    if (tick_changed) desktop->needs_redraw = true;
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

static void desktop_activate_taskbar_app(
    Desktop *desktop, const char *app_id, bool clicked_app_focused) {
    if (!desktop || !app_id) return;

    size_t first = desktop->app_count;
    size_t next = desktop->app_count;
    bool active_seen = false;
    WindowId active = wm_active_window(desktop->wm);

    for (size_t i = 0; i < desktop->app_count; ++i) {
        RetroAppInstance *instance = desktop->apps[i].app;
        if (!instance || !instance->descriptor ||
            !instance->descriptor->app_id ||
            strcmp(instance->descriptor->app_id, app_id) != 0) {
            continue;
        }
        if (first == desktop->app_count) first = i;
        if (active_seen && next == desktop->app_count) next = i;
        if (desktop->apps[i].window_id == active) {
            active_seen = true;
        }
    }

    if (first == desktop->app_count) {
        (void)app_launch(desktop, app_id);
        return;
    }

    size_t target =
        active_seen && next < desktop->app_count ? next : first;
    WindowId target_window = desktop->apps[target].window_id;
    (void)desktop_chrome_activate_taskbar_window(
        desktop->wm, target_window, clicked_app_focused);
    desktop_request_redraw(desktop);
}

static bool desktop_handle_taskbar_mouse(
    Desktop *desktop, const RetroMouseEvent *mouse) {
    if (!desktop || !mouse || !mouse->button1_clicked) return false;
    if (desktop->shutdown_requested) return true;

    StatusBarAction action =
        statusbar_hit_test(desktop->statusbar, mouse->y, mouse->x);
    if (action.kind == STATUSBAR_ACTION_NONE) return false;

    if (action.kind == STATUSBAR_ACTION_TOGGLE_MENU) {
        if (desktop->launcher.open) {
            desktop_launcher_close(desktop);
        } else {
            desktop_launcher_open(desktop);
        }
        desktop_request_redraw(desktop);
        return true;
    }

    size_t catalog_count =
        sizeof(k_taskbar_catalog) / sizeof(k_taskbar_catalog[0]);
    if (action.kind == STATUSBAR_ACTION_ACTIVATE_APP &&
        action.app_index < catalog_count) {
        if (desktop->launcher.open) {
            desktop_launcher_close(desktop);
        }
        desktop_activate_taskbar_app(
            desktop,
            k_taskbar_catalog[action.app_index].app_id,
            action.focused);
        return true;
    }
    return false;
}

static bool desktop_handle_key_command(Desktop *desktop, const RetroKeyEvent *key) {
    if (!desktop || !key) return false;

    if (desktop->shutdown_requested) {
        return key->key_code == RETRO_KEY_CTRL_Q;
    }

    /* Global accelerators must not consume printable text.  The previous
       implementation stole q/m/w/1..3/HJKL from every app, making editing
       impossible.  Control chords and function keys are unambiguous. */
    if (desktop->launcher.open) {
        if (key->key_code == RETRO_KEY_CTRL_Q) {
            desktop_begin_shutdown(desktop);
            return true;
        }
        if (key->key_code == RETRO_KEY_F10) {
            desktop_launcher_close(desktop);
            return true;
        }
        return false;
    }

    if (desktop->window_mode) {
        if (key->key_code == RETRO_KEY_ESC) {
            desktop->window_mode = false;
            desktop->window_resize_mode = false;
            desktop_request_redraw(desktop);
            return true;
        }
        if (key->key_code == RETRO_KEY_TAB) {
            desktop->window_resize_mode = !desktop->window_resize_mode;
            desktop_request_redraw(desktop);
            return true;
        }
        int dy = 0;
        int dx = 0;
        if (key->key_code == RETRO_KEY_UP) dy = -1;
        if (key->key_code == RETRO_KEY_DOWN) dy = 1;
        if (key->key_code == RETRO_KEY_LEFT) dx = -1;
        if (key->key_code == RETRO_KEY_RIGHT) dx = 1;
        bool changed = desktop->window_resize_mode
                           ? wm_resize_active_window(desktop->wm, dy, dx)
                           : wm_move_active_window(desktop->wm, dy, dx);
        if (changed) desktop_request_redraw(desktop);
        return true;
    }

    if (key->key_code == RETRO_KEY_CTRL_Q) {
        desktop_begin_shutdown(desktop);
        return true;
    }

    if (key->key_code == RETRO_KEY_F1 ||
        key->key_code == RETRO_KEY_F10) {
        desktop_launcher_open(desktop);
        return true;
    }

    if (key->key_code == RETRO_KEY_F6) {
        wm_cycle_focus(desktop->wm);
        desktop_request_redraw(desktop);
        return true;
    }

    if (key->key_code == RETRO_KEY_F9) {
        desktop->window_mode = true;
        desktop->window_resize_mode = false;
        desktop_request_redraw(desktop);
        return true;
    }

    if (key->key_code == RETRO_KEY_CTRL_W) {
        WindowId active = wm_active_window(desktop->wm);
        if (active != WINDOW_ID_INVALID && active != desktop->shell_window_id) {
            (void)desktop_request_app_close(desktop, active);
        }
        return true;
    }

    /* Tab and all printable characters belong to the focused app. */
    return false;
}

static bool desktop_dispatch_event(Desktop *desktop,
                                   const RetroEvent *event) {
    if (!desktop || !event) return false;

    bool consumed = false;
    if (event->type == RETRO_EVENT_KEY) {
        consumed = desktop_handle_key_command(desktop, &event->data.key);
    } else if (event->type == RETRO_EVENT_MOUSE) {
        consumed = desktop_handle_taskbar_mouse(desktop, &event->data.mouse);
    }
    if (!consumed) {
        (void)desktop_chrome_handle_window_event(
            desktop->wm, event);
    }
    desktop_cleanup_apps(desktop);
    desktop_continue_shutdown(desktop);
    desktop_request_redraw(desktop);
    return true;
}

bool desktop_dispatch_event_for_test(Desktop *desktop,
                                     const RetroEvent *event) {
    return desktop_dispatch_event(desktop, event);
}

RetroAppInstance *desktop_app_instance_for_test(Desktop *desktop,
                                                const char *app_id) {
    if (!desktop || !app_id) return NULL;
    for (size_t i = 0; i < desktop->app_count; ++i) {
        RetroAppInstance *instance = desktop->apps[i].app;
        if (instance && instance->descriptor &&
            instance->descriptor->app_id &&
            strcmp(instance->descriptor->app_id, app_id) == 0) {
            return instance;
        }
    }
    return NULL;
}

bool desktop_shutdown_pending_for_test(const Desktop *desktop) {
    return desktop && desktop->shutdown_requested;
}

bool desktop_launcher_open_for_test(const Desktop *desktop) {
    return desktop && desktop->launcher.open;
}

int desktop_launcher_selected_for_test(const Desktop *desktop) {
    return desktop ? desktop->launcher.selected : -1;
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
    retro_clipboard_destroy(desktop->clipboard);
    desktop->clipboard = NULL;
    draw_list_destroy(desktop->overlay_draw_list);
    statusbar_destroy(desktop->statusbar);
    wm_destroy(desktop->wm);
    renderer_destroy(desktop->renderer);
    free(desktop);
}
