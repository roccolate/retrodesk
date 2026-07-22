#ifndef RETRODESK_WM_WINDOW_MANAGER_H
#define RETRODESK_WM_WINDOW_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

#include "core/event.h"
#include "render/render.h"

/* Window collection + z-order + focus + drag + modal policy.
   Produces draw lists for windows/apps; never touches backend draw APIs. */

/* Positive, monotonically increasing IDs. Live IDs are never reused.
   INT_MAX is issued once; later creation returns WINDOW_ID_INVALID. */
typedef int WindowId;
typedef unsigned int WindowFlags;

enum { WINDOW_ID_INVALID = -1 };

enum {
    WINDOW_FLAG_NORMAL = 0,
    /* Window is fixed in place: ignores HJKL moves and title-bar drag. */
    WINDOW_FLAG_FIXED = 1u << 0,
    /* Window receives exclusive input (mouse + keyboard) until dismissed.
       The WM ignores hits and key events targeted at any other window while
       a modal is on top. */
    WINDOW_FLAG_MODAL = 1u << 1,
    /* Visual hint: window is a transient popup (centered, no title-bar drag).
       Currently no runtime effect beyond a marker; reserved for v0.2. */
    WINDOW_FLAG_POPUP = 1u << 2,
    /* Marker: window is owned by an app instance. */
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

/* A minimized window remains alive with geometry and application state intact,
   but is excluded from focus, input hit testing, modal routing, and rendering. */
bool wm_minimize_window(WindowManager *wm, WindowId id);
bool wm_restore_window(WindowManager *wm, WindowId id);
bool wm_window_is_minimized(const WindowManager *wm, WindowId id);

/* Maximize/restore geometry is owned by each RetroWindow. Fixed and modal
   windows reject this contract. Minimized maximized windows retain their mode
   and refresh to current terminal geometry when restored or focused. */
bool wm_maximize_window(WindowManager *wm, WindowId id);
bool wm_unmaximize_window(WindowManager *wm, WindowId id);
bool wm_toggle_maximize_window(WindowManager *wm, WindowId id);
bool wm_window_is_maximized(const WindowManager *wm, WindowId id);
bool wm_refresh_maximized_window(WindowManager *wm, WindowId id);

bool wm_move_active_window(WindowManager *wm, int dy, int dx);
bool wm_resize_active_window(WindowManager *wm, int dh, int dw);
bool wm_get_drag_preview(const WindowManager *wm, WindowId *id, int *y, int *x);
bool wm_drag_is_enabled(const WindowManager *wm);
bool wm_drag_is_degraded(const WindowManager *wm);
int wm_drag_no_motion_sessions(const WindowManager *wm);
#ifdef RETRODESK_ENABLE_TEST_HOOKS
void wm_fail_next_growth_for_test(WindowManager *wm);
bool wm_set_next_id_for_test(WindowManager *wm, WindowId next_id);
#endif

bool wm_handle_event(WindowManager *wm, const RetroEvent *event);
void wm_render(WindowManager *wm, Renderer *renderer, const RetroTheme *theme);

RetroWindow *wm_window(WindowManager *wm, WindowId id);
void retro_window_get_geometry(const RetroWindow *window, int *y, int *x, int *h,
                               int *w);
void retro_window_request_close(RetroWindow *window);

#endif
