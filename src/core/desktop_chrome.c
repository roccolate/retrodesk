#include "core/desktop_chrome.h"

#include "core/key_chord.h"

bool desktop_chrome_handle_window_event(WindowManager *wm,
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
        int width = 0;
        retro_window_get_geometry(window, &y, &x, NULL, &width);
        if (window && event->data.mouse.y == y &&
            event->data.mouse.x >= x &&
            event->data.mouse.x < x + width &&
            wm_toggle_maximize_window(wm, active)) {
            return true;
        }
    }

    return wm_handle_event(wm, event);
}
