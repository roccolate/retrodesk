#ifndef RETRODESK_CORE_DESKTOP_WINDOW_MODE_H
#define RETRODESK_CORE_DESKTOP_WINDOW_MODE_H

#include <stdbool.h>

#include "render/render.h"
#include "ui/theme.h"
#include "wm/window_manager.h"

typedef struct DesktopWindowMode {
    bool active;
    bool resize;
    WindowId target_window;
    bool blocked_notice;
    bool blocked_maximized;
} DesktopWindowMode;

void desktop_window_mode_init(DesktopWindowMode *mode);
void desktop_window_mode_clear(DesktopWindowMode *mode);
bool desktop_window_mode_start(DesktopWindowMode *mode, WindowManager *wm);
bool desktop_window_mode_handle_key(DesktopWindowMode *mode,
                                    WindowManager *wm,
                                    int key_code,
                                    bool *redraw_required);
const RetroTheme *desktop_window_mode_render_theme(
    DesktopWindowMode *mode,
    WindowManager *wm,
    const RetroTheme *theme,
    RetroTheme *operation_theme);
void desktop_window_mode_render_hud(DesktopWindowMode *mode,
                                    WindowManager *wm,
                                    const RetroTheme *theme,
                                    DrawList *draw_list,
                                    int screen_rows,
                                    int screen_cols,
                                    const RenderStyle *status_style);

bool desktop_window_mode_is_active(const DesktopWindowMode *mode);
bool desktop_window_mode_is_resize(const DesktopWindowMode *mode);
WindowId desktop_window_mode_target(const DesktopWindowMode *mode);
bool desktop_window_mode_has_blocked_notice(const DesktopWindowMode *mode);
bool desktop_window_mode_blocked_maximized(const DesktopWindowMode *mode);

#endif
