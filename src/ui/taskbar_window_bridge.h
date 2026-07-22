#ifndef RETRODESK_UI_TASKBAR_WINDOW_BRIDGE_H
#define RETRODESK_UI_TASKBAR_WINDOW_BRIDGE_H

#include <stdbool.h>

#include "wm/window_manager.h"

/* Translation-unit-private adapter for core/desktop.c. The public StatusBar
   remains a pure action source; this bridge gives taskbar clicks the familiar
   desktop behavior without widening Desktop's public API. */
typedef struct TaskbarWindowBridgeState {
    bool activation_pending;
    bool clicked_app_focused;
    bool skip_bring_to_front;
    WindowId target_window;
} TaskbarWindowBridgeState;

static TaskbarWindowBridgeState g_taskbar_window_bridge = {
    false,
    false,
    false,
    WINDOW_ID_INVALID,
};

static inline StatusBarAction desktop_taskbar_hit_test(
    const StatusBar *statusbar, int y, int x) {
    StatusBarAction action = statusbar_hit_test(statusbar, y, x);
    g_taskbar_window_bridge.activation_pending =
        action.kind == STATUSBAR_ACTION_ACTIVATE_APP &&
        action.instance_count > 0;
    g_taskbar_window_bridge.clicked_app_focused = action.focused;
    g_taskbar_window_bridge.skip_bring_to_front = false;
    g_taskbar_window_bridge.target_window = WINDOW_ID_INVALID;
    return action;
}

static inline void desktop_taskbar_focus_window(WindowManager *wm,
                                                WindowId id) {
    if (!g_taskbar_window_bridge.activation_pending) {
        wm_focus_window(wm, id);
        return;
    }

    g_taskbar_window_bridge.activation_pending = false;
    g_taskbar_window_bridge.target_window = id;

    if (wm_window_is_minimized(wm, id)) {
        (void)wm_restore_window(wm, id);
        wm_focus_window(wm, id);
        return;
    }

    if (g_taskbar_window_bridge.clicked_app_focused &&
        wm_active_window(wm) == id && wm_minimize_window(wm, id)) {
        g_taskbar_window_bridge.skip_bring_to_front = true;
        return;
    }

    wm_focus_window(wm, id);
}

static inline void desktop_taskbar_bring_to_front(WindowManager *wm,
                                                   WindowId id) {
    if (g_taskbar_window_bridge.skip_bring_to_front &&
        g_taskbar_window_bridge.target_window == id) {
        g_taskbar_window_bridge.skip_bring_to_front = false;
        g_taskbar_window_bridge.target_window = WINDOW_ID_INVALID;
        return;
    }

    g_taskbar_window_bridge.skip_bring_to_front = false;
    g_taskbar_window_bridge.target_window = WINDOW_ID_INVALID;
    wm_bring_to_front(wm, id);
}

#endif
