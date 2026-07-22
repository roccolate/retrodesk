#ifndef RETRODESK_CORE_DESKTOP_CHROME_H
#define RETRODESK_CORE_DESKTOP_CHROME_H

#include <stdbool.h>

#include "core/event.h"
#include "wm/window_manager.h"

/* Desktop-private chrome routing. This layer maps global desktop gestures onto
   ordinary Window Manager APIs; it does not own windows, poll input or render. */
bool desktop_chrome_handle_window_event(WindowManager *wm,
                                        const RetroEvent *event);
bool desktop_chrome_activate_taskbar_window(
    WindowManager *wm, WindowId id, bool clicked_app_focused);

#endif
