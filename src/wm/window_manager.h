#ifndef RETRODESK_WM_WINDOW_MANAGER_H
#define RETRODESK_WM_WINDOW_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

#include "core/event.h"
#include "render/render.h"

typedef int WindowId;
typedef unsigned int WindowFlags;

enum {
    WINDOW_FLAG_NORMAL = 0,
    WINDOW_FLAG_MODAL = 1u << 0,
    WINDOW_FLAG_POPUP = 1u << 1,
    WINDOW_FLAG_FIXED = 1u << 2,
    WINDOW_FLAG_APP_OWNED = 1u << 3,
};

typedef struct RetroWindow RetroWindow;
typedef struct WindowManager WindowManager;
typedef struct RetroTheme RetroTheme;

typedef void (*WindowDrawCallback)(RetroWindow *window, DrawList *draw_list,
                                   void *user_data);
typedef void (*WindowEventCallback)(RetroWindow *window, const RetroEvent *event,
                                    void *user_data);

typedef struct RetroWindowSpec {
    int height;
    int width;
    int y;
    int x;
    const char *title;
    WindowFlags flags;
    WindowDrawCallback draw_cb;
    WindowEventCallback event_cb;
    void *user_data;
} RetroWindowSpec;

WindowManager *wm_create(Renderer *renderer);
void wm_destroy(WindowManager *wm);
void wm_set_drag_enabled(WindowManager *wm, bool enabled);
void wm_set_drag_auto_degrade(WindowManager *wm, bool enabled);

WindowId wm_create_window(WindowManager *wm, const RetroWindowSpec *spec);
void wm_close_window(WindowManager *wm, WindowId id);
void wm_focus_window(WindowManager *wm, WindowId id);
void wm_bring_to_front(WindowManager *wm, WindowId id);
bool wm_cycle_focus(WindowManager *wm);
WindowId wm_active_window(const WindowManager *wm);
bool wm_window_exists(const WindowManager *wm, WindowId id);
size_t wm_window_count(const WindowManager *wm);
bool wm_move_active_window(WindowManager *wm, int dy, int dx);
bool wm_get_drag_preview(const WindowManager *wm, WindowId *id, int *y, int *x);
bool wm_drag_is_enabled(const WindowManager *wm);
bool wm_drag_is_degraded(const WindowManager *wm);
int wm_drag_no_motion_sessions(const WindowManager *wm);

bool wm_handle_event(WindowManager *wm, const RetroEvent *event);
void wm_render(WindowManager *wm, Renderer *renderer, const RetroTheme *theme);

RetroWindow *wm_window(WindowManager *wm, WindowId id);
WindowId retro_window_id(const RetroWindow *window);
void retro_window_get_geometry(const RetroWindow *window, int *y, int *x, int *h,
                               int *w);
void retro_window_move(RetroWindow *window, int y, int x);
void retro_window_set_title(RetroWindow *window, const char *title);
bool retro_window_is_active(const RetroWindow *window);
void retro_window_request_close(RetroWindow *window);

#endif
