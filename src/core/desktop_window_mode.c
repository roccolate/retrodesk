#include "core/desktop_window_mode.h"

#include "core/key_chord.h"
#include "ui/window_mode_hud.h"

static bool desktop_window_mode_can_transform(WindowManager *wm,
                                              WindowId active) {
    if (!wm || active == WINDOW_ID_INVALID ||
        wm_window_is_minimized(wm, active) ||
        wm_window_is_maximized(wm, active)) {
        return false;
    }

    /* A zero-delta move is the public WM capability probe. Fixed windows reject
       it; ordinary visible windows keep identical geometry. */
    return wm_move_active_window(wm, 0, 0);
}

static void desktop_window_mode_block(DesktopWindowMode *mode,
                                      WindowManager *wm,
                                      WindowId active) {
    if (!mode) return;
    mode->blocked_notice = true;
    mode->blocked_maximized =
        wm && active != WINDOW_ID_INVALID &&
        wm_window_is_maximized(wm, active);
    desktop_window_mode_clear(mode);
}

static bool desktop_window_mode_sync(DesktopWindowMode *mode,
                                     WindowManager *wm) {
    if (!mode || !wm || !mode->active) return false;
    WindowId active = wm_active_window(wm);
    if (active != mode->target_window) {
        desktop_window_mode_clear(mode);
        return false;
    }
    if (!desktop_window_mode_can_transform(wm, active)) {
        desktop_window_mode_block(mode, wm, active);
        return false;
    }
    return true;
}

void desktop_window_mode_init(DesktopWindowMode *mode) {
    if (!mode) return;
    *mode = (DesktopWindowMode){0};
    mode->target_window = WINDOW_ID_INVALID;
}

void desktop_window_mode_clear(DesktopWindowMode *mode) {
    if (!mode) return;
    mode->active = false;
    mode->resize = false;
    mode->target_window = WINDOW_ID_INVALID;
}

bool desktop_window_mode_start(DesktopWindowMode *mode, WindowManager *wm) {
    if (!mode || !wm) return false;
    WindowId active = wm_active_window(wm);
    mode->blocked_notice = false;
    mode->blocked_maximized = false;
    mode->resize = false;
    mode->target_window = active;
    if (!desktop_window_mode_can_transform(wm, active)) {
        desktop_window_mode_block(mode, wm, active);
    } else {
        mode->active = true;
    }
    return true;
}

bool desktop_window_mode_handle_key(DesktopWindowMode *mode,
                                    WindowManager *wm,
                                    int key_code,
                                    bool *redraw_required) {
    if (redraw_required) *redraw_required = false;
    if (!mode || !wm || !mode->active) return false;

    if (window_mode_hud_finish_key(key_code)) {
        desktop_window_mode_clear(mode);
        if (redraw_required) *redraw_required = true;
        return true;
    }
    if (key_code == RETRO_KEY_TAB) {
        mode->resize = !mode->resize;
        if (redraw_required) *redraw_required = true;
        return true;
    }

    int dy = 0;
    int dx = 0;
    if (key_code == RETRO_KEY_UP) dy = -1;
    if (key_code == RETRO_KEY_DOWN) dy = 1;
    if (key_code == RETRO_KEY_LEFT) dx = -1;
    if (key_code == RETRO_KEY_RIGHT) dx = 1;
    if (dy == 0 && dx == 0) return true;

    bool changed = mode->resize
                       ? wm_resize_active_window(wm, dy, dx)
                       : wm_move_active_window(wm, dy, dx);
    if (redraw_required) *redraw_required = changed;
    return true;
}

const RetroTheme *desktop_window_mode_render_theme(
    DesktopWindowMode *mode,
    WindowManager *wm,
    const RetroTheme *theme,
    RetroTheme *operation_theme) {
    if (!theme || !operation_theme ||
        !desktop_window_mode_sync(mode, wm)) {
        return theme;
    }
    *operation_theme = *theme;
    operation_theme->window_frame_active = theme->window_frame_drag;
    return operation_theme;
}

void desktop_window_mode_render_hud(DesktopWindowMode *mode,
                                    WindowManager *wm,
                                    const RetroTheme *theme,
                                    DrawList *draw_list,
                                    int screen_rows,
                                    int screen_cols,
                                    const RenderStyle *status_style) {
    if (!mode || !wm || !draw_list) return;

    WindowModeHudSnapshot snapshot = {0};
    if (mode->active) {
        WindowId active = wm_active_window(wm);
        RetroWindow *window = wm_window(wm, active);
        if (!window || active != mode->target_window) return;
        snapshot.active = true;
        snapshot.resize = mode->resize;
        snapshot.transformable = true;
        retro_window_get_geometry(window, &snapshot.y, &snapshot.x,
                                  &snapshot.h, &snapshot.w);
    } else if (mode->blocked_notice) {
        snapshot.active = true;
        snapshot.transformable = false;
        snapshot.maximized = mode->blocked_maximized;
        mode->blocked_notice = false;
        mode->blocked_maximized = false;
    } else {
        return;
    }

    RenderStyle fallback = status_style
                               ? *status_style
                               : (RenderStyle){RENDER_COLOR_BLACK,
                                               RENDER_COLOR_WHITE,
                                               false,
                                               true};
    const RenderStyle *hud_style = theme
                                       ? &theme->window_frame_drag
                                       : &fallback;
    (void)window_mode_hud_render(&snapshot, draw_list, screen_rows,
                                 screen_cols, hud_style);
}

bool desktop_window_mode_is_active(const DesktopWindowMode *mode) {
    return mode && mode->active;
}

bool desktop_window_mode_is_resize(const DesktopWindowMode *mode) {
    return mode && mode->resize;
}

WindowId desktop_window_mode_target(const DesktopWindowMode *mode) {
    return mode ? mode->target_window : WINDOW_ID_INVALID;
}

bool desktop_window_mode_has_blocked_notice(const DesktopWindowMode *mode) {
    return mode && mode->blocked_notice;
}

bool desktop_window_mode_blocked_maximized(const DesktopWindowMode *mode) {
    return mode && mode->blocked_maximized;
}
