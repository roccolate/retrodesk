#ifndef RETRODESK_UI_WINDOW_MODE_BRIDGE_H
#define RETRODESK_UI_WINDOW_MODE_BRIDGE_H

#include <stdbool.h>

#include "ui/theme.h"
#include "ui/window_maximize_bridge.h"
#include "ui/window_mode_hud.h"
#include "wm/window_manager.h"

typedef struct WindowModeBridgeState {
    WindowId target_window;
    bool blocked_notice;
    bool blocked_maximized;
} WindowModeBridgeState;

static WindowModeBridgeState g_window_mode_bridge = {
    WINDOW_ID_INVALID,
    false,
    false,
};

static inline void desktop_window_mode_clear(bool *window_mode,
                                             bool *resize_mode) {
    if (window_mode) *window_mode = false;
    if (resize_mode) *resize_mode = false;
    g_window_mode_bridge.target_window = WINDOW_ID_INVALID;
}

static inline bool desktop_window_mode_can_transform(WindowManager *wm,
                                                      WindowId active) {
    if (!wm || active == WINDOW_ID_INVALID ||
        wm_window_is_minimized(wm, active) ||
        wm_window_is_maximized(wm, active)) {
        return false;
    }

    /* A zero-delta move is the public WM capability probe. Fixed windows reject
       it; ordinary visible windows remain geometrically unchanged. */
    return wm_move_active_window(wm, 0, 0);
}

static inline void desktop_window_mode_render(WindowManager *wm,
                                              Renderer *renderer,
                                              const RetroTheme *theme,
                                              bool *window_mode,
                                              bool *resize_mode) {
    const RetroTheme *render_theme = theme;
    RetroTheme operation_theme;
    bool active_mode = window_mode && *window_mode;

    if (!active_mode) {
        g_window_mode_bridge.target_window = WINDOW_ID_INVALID;
    } else {
        WindowId active = wm_active_window(wm);
        if (g_window_mode_bridge.target_window == WINDOW_ID_INVALID) {
            g_window_mode_bridge.target_window = active;
        } else if (g_window_mode_bridge.target_window != active) {
            desktop_window_mode_clear(window_mode, resize_mode);
            active_mode = false;
        }

        if (active_mode && !desktop_window_mode_can_transform(wm, active)) {
            g_window_mode_bridge.blocked_notice = true;
            g_window_mode_bridge.blocked_maximized =
                wm_window_is_maximized(wm, active);
            desktop_window_mode_clear(window_mode, resize_mode);
            active_mode = false;
        }

        if (active_mode && theme) {
            operation_theme = *theme;
            operation_theme.window_frame_active = theme->window_frame_drag;
            render_theme = &operation_theme;
        }
    }

    wm_render(wm, renderer, render_theme);
}

static inline void desktop_window_mode_statusbar_render(
    StatusBar *statusbar,
    DrawList *draw_list,
    int screen_rows,
    int screen_cols,
    const RenderStyle *status_style,
    WindowManager *wm,
    const RetroTheme *theme,
    bool window_mode,
    bool resize_mode) {
    statusbar_render(statusbar, draw_list, screen_rows, screen_cols,
                     status_style);

    WindowModeHudSnapshot snapshot = {0};
    if (window_mode) {
        WindowId active = wm_active_window(wm);
        RetroWindow *window = wm_window(wm, active);
        snapshot.active = true;
        snapshot.resize = resize_mode;
        snapshot.transformable =
            active != WINDOW_ID_INVALID && window != NULL &&
            !wm_window_is_minimized(wm, active) &&
            !wm_window_is_maximized(wm, active);
        snapshot.maximized = wm_window_is_maximized(wm, active);
        retro_window_get_geometry(window, &snapshot.y, &snapshot.x,
                                  &snapshot.h, &snapshot.w);
    } else if (g_window_mode_bridge.blocked_notice) {
        snapshot.active = true;
        snapshot.transformable = false;
        snapshot.maximized = g_window_mode_bridge.blocked_maximized;
        g_window_mode_bridge.blocked_notice = false;
        g_window_mode_bridge.blocked_maximized = false;
    } else {
        return;
    }

    RenderStyle fallback = status_style
                               ? *status_style
                               : (RenderStyle){RENDER_COLOR_BLACK,
                                               RENDER_COLOR_WHITE,
                                               false,
                                               true};
    const RenderStyle *hud_style = theme ? &theme->window_frame_drag : &fallback;
    (void)window_mode_hud_render(&snapshot, draw_list, screen_rows,
                                 screen_cols, hud_style);
}

static inline bool desktop_window_mode_move_active_window(
    WindowManager *wm,
    int dy,
    int dx,
    bool *window_mode,
    bool *resize_mode,
    int key_code) {
    if (window_mode_hud_finish_key(key_code)) {
        desktop_window_mode_clear(window_mode, resize_mode);
        return false;
    }
    if (dy == 0 && dx == 0) return false;
    return desktop_maximize_move_active_window(wm, dy, dx);
}

static inline bool desktop_window_mode_resize_active_window(
    WindowManager *wm,
    int dh,
    int dw,
    bool *window_mode,
    bool *resize_mode,
    int key_code) {
    if (window_mode_hud_finish_key(key_code)) {
        desktop_window_mode_clear(window_mode, resize_mode);
        return false;
    }
    if (dh == 0 && dw == 0) return false;
    return desktop_maximize_resize_active_window(wm, dh, dw);
}

#endif
