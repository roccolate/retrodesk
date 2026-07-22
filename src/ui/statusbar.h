#ifndef RETRODESK_UI_STATUSBAR_H
#define RETRODESK_UI_STATUSBAR_H

#include <stdbool.h>
#include <stddef.h>

#include "render/render.h"

#define STATUSBAR_MAX_APPS 8

typedef struct StatusBarAppSnapshot {
    const char *app_id;
    const char *label;
    size_t instance_count;
    bool focused;
} StatusBarAppSnapshot;

typedef struct StatusBarSnapshot {
    bool menu_open;
    char clock_text[9];
    size_t app_count;
    StatusBarAppSnapshot apps[STATUSBAR_MAX_APPS];
} StatusBarSnapshot;

typedef enum StatusBarActionKind {
    STATUSBAR_ACTION_NONE = 0,
    STATUSBAR_ACTION_TOGGLE_MENU,
    STATUSBAR_ACTION_ACTIVATE_APP,
} StatusBarActionKind;

typedef struct StatusBarAction {
    StatusBarActionKind kind;
    size_t app_index;
    size_t instance_count;
    bool focused;
} StatusBarAction;

typedef struct StatusBar StatusBar;

StatusBar *statusbar_create(void);
void statusbar_set_text(StatusBar *sb, const char *fmt, ...);
void statusbar_set_snapshot(StatusBar *sb, const StatusBarSnapshot *snapshot);
void statusbar_render(StatusBar *sb, DrawList *draw_list, int screen_rows,
                      int screen_cols, const RenderStyle *style);
StatusBarAction statusbar_hit_test(const StatusBar *sb, int y, int x);
void statusbar_destroy(StatusBar *sb);

/* desktop.c includes this header after core/desktop.h. Keep the temporary
   launcher, taskbar, and operation-mode adaptations private to that
   translation unit so widgets retain the ordinary WM and StatusBar contracts. */
#if defined(RETRODESK_CORE_DESKTOP_H)
#include "ui/taskbar_window_bridge.h"
#include "ui/launcher_bridge.h"
#include "ui/window_mode_bridge.h"
#define statusbar_hit_test desktop_taskbar_hit_test
#define wm_focus_window desktop_taskbar_focus_window
#define wm_bring_to_front desktop_taskbar_bring_to_front
#define wm_create_window desktop_chrome_create_window
#define wm_render(wm_, renderer_, theme_)                                      \
    desktop_window_mode_render((wm_), (renderer_), (theme_),                   \
                               &desktop->window_mode,                           \
                               &desktop->window_resize_mode)
#define statusbar_render(sb_, list_, rows_, cols_, style_)                     \
    desktop_window_mode_statusbar_render(                                      \
        (sb_), (list_), (rows_), (cols_), (style_), desktop->wm,                \
        desktop->theme, desktop->window_mode, desktop->window_resize_mode)
#define wm_move_active_window(wm_, dy_, dx_)                                   \
    desktop_window_mode_move_active_window(                                    \
        (wm_), (dy_), (dx_), &desktop->window_mode,                            \
        &desktop->window_resize_mode, key->key_code)
#define wm_resize_active_window(wm_, dh_, dw_)                                 \
    desktop_window_mode_resize_active_window(                                  \
        (wm_), (dh_), (dw_), &desktop->window_mode,                            \
        &desktop->window_resize_mode, key->key_code)
#endif

#endif
