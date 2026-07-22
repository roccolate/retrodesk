#ifndef RETRODESK_UI_WINDOW_MAXIMIZE_BRIDGE_H
#define RETRODESK_UI_WINDOW_MAXIMIZE_BRIDGE_H

#include "core/key_chord.h"
#include "wm/window_manager.h"

/* Translation-unit-private desktop input adapter. The maximize contract owns
   geometry state; this bridge maps portable F8 and title-bar double-click input
   onto it without stealing ordinary application events. */
static inline void desktop_maximize_after_focus(WindowManager *wm,
                                                WindowId id) {
    if (wm_window_is_maximized(wm, id)) {
        (void)wm_refresh_maximized_window(wm, id);
    }
}

static inline bool desktop_maximize_move_active_window(WindowManager *wm,
                                                        int dy, int dx) {
    WindowId active = wm_active_window(wm);
    if (wm_window_is_maximized(wm, active)) return false;
    return wm_move_active_window(wm, dy, dx);
}

static inline bool desktop_maximize_resize_active_window(WindowManager *wm,
                                                          int dh, int dw) {
    WindowId active = wm_active_window(wm);
    if (wm_window_is_maximized(wm, active)) return false;
    return wm_resize_active_window(wm, dh, dw);
}

static inline bool desktop_maximize_handle_event(WindowManager *wm,
                                                  const RetroEvent *event) {
    if (!wm || !event) return false;

    WindowId active = wm_active_window(wm);
    if (event->type == RETRO_EVENT_KEY &&
        event->data.key.key_code == RETRO_KEY_F8 &&
        active != WINDOW_ID_INVALID &&
        wm_toggle_maximize_window(wm, active)) {
        return true;
    }

    if (event->type == RETRO_EVENT_MOUSE &&
        event->data.mouse.button1_dblclick &&
        active != WINDOW_ID_INVALID) {
        RetroWindow *window = wm_window(wm, active);
        int y = 0;
        int x = 0;
        int h = 0;
        int w = 0;
        retro_window_get_geometry(window, &y, &x, &h, &w);
        if (event->data.mouse.y == y &&
            event->data.mouse.x >= x &&
            event->data.mouse.x < x + w &&
            wm_toggle_maximize_window(wm, active)) {
            return true;
        }
    }

    bool handled = wm_handle_event(wm, event);
    if (event->type == RETRO_EVENT_RESIZE) {
        active = wm_active_window(wm);
        if (wm_window_is_maximized(wm, active)) {
            (void)wm_refresh_maximized_window(wm, active);
        }
    }
    return handled;
}

#endif
