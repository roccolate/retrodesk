#ifndef RETRODESK_WM_WINDOW_MAXIMIZE_H
#define RETRODESK_WM_WINDOW_MAXIMIZE_H

/* Header-only maximize state complements the opaque WindowManager without
   widening its internal window representation. Include this file only after
   wm/window_manager.h has declared the base WM API. */

enum {
    WM_MAXIMIZE_SLOT_CAP = 64,
    WM_MAXIMIZE_GROW_DELTA = 1000000,
};

typedef struct WmMaximizeSlot {
    WindowManager *wm;
    WindowId id;
    RetroWindow *window;
    bool maximized;
    int restore_y;
    int restore_x;
    int restore_h;
    int restore_w;
} WmMaximizeSlot;

static WmMaximizeSlot g_wm_maximize_slots[WM_MAXIMIZE_SLOT_CAP];

static inline WmMaximizeSlot *wm_maximize_slot(WindowManager *wm,
                                                WindowId id,
                                                bool create) {
    WmMaximizeSlot *empty = NULL;
    if (!wm || id == WINDOW_ID_INVALID) return NULL;
    RetroWindow *window = wm_window(wm, id);
    if (!window) return NULL;

    for (size_t i = 0; i < WM_MAXIMIZE_SLOT_CAP; ++i) {
        WmMaximizeSlot *slot = &g_wm_maximize_slots[i];
        if (slot->wm == wm && slot->window &&
            wm_window(wm, slot->id) != slot->window) {
            *slot = (WmMaximizeSlot){0};
        }
        if (slot->wm == wm && slot->id == id) return slot;
        if (!slot->wm && !empty) empty = slot;
    }

    if (!create || !empty) return NULL;
    empty->wm = wm;
    empty->id = id;
    empty->window = window;
    return empty;
}

static inline bool wm_apply_maximized_geometry(WindowManager *wm,
                                                WindowId id) {
    if (!wm || id == WINDOW_ID_INVALID || wm_active_window(wm) != id ||
        wm_window_is_minimized(wm, id)) {
        return false;
    }

    RetroWindow *window = wm_window(wm, id);
    if (!window) return false;

    int y = 0;
    int x = 0;
    int h = 0;
    int w = 0;
    retro_window_get_geometry(window, &y, &x, &h, &w);

    /* FIXED windows reject this move, keeping desktop chrome, modal launchers,
       and other fixed surfaces outside the maximize contract. */
    if (!wm_move_active_window(wm, -y, -x)) return false;

    (void)wm_resize_active_window(wm,
                                  WM_MAXIMIZE_GROW_DELTA,
                                  WM_MAXIMIZE_GROW_DELTA);
    retro_window_get_geometry(window, &y, &x, &h, &w);

    /* wm_resize_active_window may use the full terminal height. Reserve the
       final row for the taskbar, matching wm_create_window's usable area. */
    if (h > 4) (void)wm_resize_active_window(wm, -1, 0);

    retro_window_get_geometry(window, &y, &x, &h, &w);
    return y == 0 && x == 0 && h >= 4 && w >= 8;
}

static inline bool wm_maximize_window(WindowManager *wm, WindowId id) {
    if (!wm || id == WINDOW_ID_INVALID || wm_active_window(wm) != id ||
        wm_window_is_minimized(wm, id)) {
        return false;
    }

    WmMaximizeSlot *existing = wm_maximize_slot(wm, id, false);
    if (existing && existing->maximized) return false;

    RetroWindow *window = wm_window(wm, id);
    if (!window) return false;

    int y = 0;
    int x = 0;
    int h = 0;
    int w = 0;
    retro_window_get_geometry(window, &y, &x, &h, &w);

    if (!wm_apply_maximized_geometry(wm, id)) return false;

    WmMaximizeSlot *slot = wm_maximize_slot(wm, id, true);
    if (!slot) {
        int cy = 0;
        int cx = 0;
        int ch = 0;
        int cw = 0;
        retro_window_get_geometry(window, &cy, &cx, &ch, &cw);
        (void)wm_resize_active_window(wm, h - ch, w - cw);
        retro_window_get_geometry(window, &cy, &cx, NULL, NULL);
        (void)wm_move_active_window(wm, y - cy, x - cx);
        return false;
    }

    slot->restore_y = y;
    slot->restore_x = x;
    slot->restore_h = h;
    slot->restore_w = w;
    slot->maximized = true;
    return true;
}

static inline bool wm_unmaximize_window(WindowManager *wm, WindowId id) {
    WmMaximizeSlot *slot = wm_maximize_slot(wm, id, false);
    if (!slot || !slot->maximized || wm_active_window(wm) != id ||
        wm_window_is_minimized(wm, id)) {
        return false;
    }

    RetroWindow *window = wm_window(wm, id);
    if (!window) return false;

    int y = 0;
    int x = 0;
    int h = 0;
    int w = 0;
    retro_window_get_geometry(window, &y, &x, &h, &w);
    (void)wm_resize_active_window(wm,
                                  slot->restore_h - h,
                                  slot->restore_w - w);
    retro_window_get_geometry(window, &y, &x, NULL, NULL);
    (void)wm_move_active_window(wm,
                                slot->restore_y - y,
                                slot->restore_x - x);
    slot->maximized = false;
    return true;
}

static inline bool wm_window_is_maximized(WindowManager *wm, WindowId id) {
    WmMaximizeSlot *slot = wm_maximize_slot(wm, id, false);
    return slot && slot->maximized;
}

static inline bool wm_refresh_maximized_window(WindowManager *wm,
                                                WindowId id) {
    if (!wm_window_is_maximized(wm, id)) return false;
    return wm_apply_maximized_geometry(wm, id);
}

static inline bool wm_toggle_maximize_window(WindowManager *wm,
                                              WindowId id) {
    return wm_window_is_maximized(wm, id)
               ? wm_unmaximize_window(wm, id)
               : wm_maximize_window(wm, id);
}

#endif
