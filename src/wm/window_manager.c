#include "wm/window_manager.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/checked_size.h"
#include "ui/theme.h"

struct RetroWindow {
    WindowId id;
    int y;
    int x;
    int h;
    int w;
    char title[64];
    WindowFlags flags;
    bool is_active;
    bool minimized;
    bool maximized;
    int restore_y;
    int restore_x;
    int restore_h;
    int restore_w;
    bool close_requested;
    DrawList *draw_list;
    WindowDrawCallback draw_cb;
    WindowEventCallback event_cb;
    void *user_data;
};

struct WindowManager {
    Renderer *renderer;
    RetroWindow **windows;
    size_t count;
    size_t capacity;
    bool fail_next_growth_for_test;
    WindowId next_id;
    WindowId active_id;
    bool drag_enabled;
    bool dragging;
    WindowId drag_window_id;
    int drag_off_y;
    int drag_off_x;
    bool drag_session_moved;
    int drag_no_motion_sessions;
    bool drag_degraded;
    bool drag_auto_degrade;
};

static int wm_find_index(const WindowManager *wm, WindowId id) {
    if (!wm || id == WINDOW_ID_INVALID) return WINDOW_ID_INVALID;
    for (size_t i = 0; i < wm->count; ++i) {
        if (wm->windows[i] && wm->windows[i]->id == id) return (int)i;
    }
    return WINDOW_ID_INVALID;
}

static RetroWindow *wm_find_window(const WindowManager *wm, WindowId id) {
    int idx = wm_find_index(wm, id);
    return (idx < 0) ? NULL : wm->windows[idx];
}

static bool wm_reserve(WindowManager *wm, size_t want) {
    if (!wm) return false;
    if (want <= wm->capacity) return true;

    size_t next_capacity = 0;
    if (!retro_checked_capacity_grow(wm->capacity, want,
                           sizeof(*wm->windows), 8,
                           &next_capacity)) {
        return false;
    }
    if (wm->fail_next_growth_for_test) {
        wm->fail_next_growth_for_test = false;
        return false;
    }

    RetroWindow **next = realloc(wm->windows,
                       next_capacity * sizeof(*wm->windows));
    if (!next) return false;
    wm->windows = next;
    wm->capacity = next_capacity;
    return true;
}

static bool window_is_visible(const RetroWindow *window) {
    return window && !window->minimized && !window->close_requested;
}

static bool wm_peek_next_id(const WindowManager *wm, WindowId *out_id) {
    if (!wm || !out_id || wm->next_id <= 0) return false;
    if (wm_find_index(wm, wm->next_id) >= 0) return false;
    *out_id = wm->next_id;
    return true;
}

static void wm_commit_window_id(WindowManager *wm, WindowId id) {
    if (!wm) return;
    wm->next_id = id == INT_MAX ? WINDOW_ID_INVALID : id + 1;
}

static void wm_update_active_flags(WindowManager *wm) {
    if (!wm) return;
    for (size_t i = 0; i < wm->count; ++i) {
        RetroWindow *window = wm->windows[i];
        window->is_active = window_is_visible(window) &&
                            window->id == wm->active_id;
    }
}

static void wm_select_topmost_visible(WindowManager *wm) {
    if (!wm) return;
    RetroWindow *active = wm_find_window(wm, wm->active_id);
    if (window_is_visible(active)) {
        wm_update_active_flags(wm);
        return;
    }

    wm->active_id = WINDOW_ID_INVALID;
    for (size_t i = wm->count; i > 0; --i) {
        RetroWindow *candidate = wm->windows[i - 1];
        if (!window_is_visible(candidate)) continue;
        wm->active_id = candidate->id;
        break;
    }
    wm_update_active_flags(wm);
}

static void wm_clamp_window(WindowManager *wm, RetroWindow *window) {
    if (!wm || !window) return;
    int rows = 0;
    int cols = 0;
    renderer_get_screen_size(wm->renderer, &rows, &cols);
    int max_y = rows - window->h - 1;
    int max_x = cols - window->w;
    if (max_y < 0) max_y = 0;
    if (max_x < 0) max_x = 0;
    if (window->y < 0) window->y = 0;
    if (window->x < 0) window->x = 0;
    if (window->y > max_y) window->y = max_y;
    if (window->x > max_x) window->x = max_x;
}

static bool window_can_maximize(const RetroWindow *window) {
    return window && !window->close_requested &&
           (window->flags & (WINDOW_FLAG_FIXED | WINDOW_FLAG_MODAL)) == 0;
}

static bool wm_apply_maximized_geometry(WindowManager *wm,
                                        RetroWindow *window) {
    if (!wm || !window_can_maximize(window) || window->minimized) return false;
    int rows = 0;
    int cols = 0;
    renderer_get_screen_size(wm->renderer, &rows, &cols);
    if (rows < 5 || cols < 8) return false;
    window->y = 0;
    window->x = 0;
    window->h = rows - 1;
    window->w = cols;
    return true;
}

static bool wm_apply_restore_geometry(WindowManager *wm,
                                      RetroWindow *window) {
    if (!wm || !window) return false;
    int rows = 0;
    int cols = 0;
    renderer_get_screen_size(wm->renderer, &rows, &cols);
    if (rows < 5 || cols < 8) return false;

    int max_h = rows - 1;
    int max_w = cols;
    window->h = window->restore_h < 4 ? 4 : window->restore_h;
    window->w = window->restore_w < 8 ? 8 : window->restore_w;
    if (window->h > max_h) window->h = max_h;
    if (window->w > max_w) window->w = max_w;
    window->y = window->restore_y;
    window->x = window->restore_x;
    wm_clamp_window(wm, window);
    return true;
}

static void wm_cleanup_closed(WindowManager *wm) {
    if (!wm) return;

    for (size_t i = 0; i < wm->count;) {
        RetroWindow *window = wm->windows[i];
        if (!window || !window->close_requested) {
            ++i;
            continue;
        }

        if (window->id == wm->active_id) wm->active_id = WINDOW_ID_INVALID;
        if (window->id == wm->drag_window_id) {
            wm->dragging = false;
            wm->drag_window_id = WINDOW_ID_INVALID;
        }

        draw_list_destroy(window->draw_list);
        free(window);

        for (size_t j = i + 1; j < wm->count; ++j) {
            wm->windows[j - 1] = wm->windows[j];
        }
        wm->count--;
    }

    wm_select_topmost_visible(wm);
}

static bool window_contains(const RetroWindow *window, int y, int x) {
    if (!window_is_visible(window)) return false;
    if (y < window->y || y >= window->y + window->h) return false;
    if (x < window->x || x >= window->x + window->w) return false;
    return true;
}

static bool window_title_hit(const RetroWindow *window, int y, int x) {
    if (!window_contains(window, y, x)) return false;
    return (y - window->y) == 0;
}

static bool window_is_focusable(const RetroWindow *window) {
    if (!window_is_visible(window)) return false;
    return (window->flags & WINDOW_FLAG_FIXED) == 0;
}

static RetroWindow *wm_top_window_at(WindowManager *wm, int y, int x) {
    if (!wm) return NULL;
    for (size_t i = wm->count; i > 0; --i) {
        RetroWindow *window = wm->windows[i - 1];
        if (window_contains(window, y, x)) return window;
    }
    return NULL;
}

/* Returns the topmost visible modal window, or NULL if none. */
static RetroWindow *wm_topmost_modal(const WindowManager *wm) {
    if (!wm) return NULL;
    for (size_t i = wm->count; i > 0; --i) {
        RetroWindow *window = wm->windows[i - 1];
        if (window_is_visible(window) &&
            (window->flags & WINDOW_FLAG_MODAL)) {
            return window;
        }
    }
    return NULL;
}

WindowManager *wm_create(Renderer *renderer) {
    if (!renderer) return NULL;
    WindowManager *wm = calloc(1, sizeof(*wm));
    if (!wm) return NULL;

    wm->renderer = renderer;
    wm->next_id = 1;
    wm->active_id = WINDOW_ID_INVALID;
    wm->drag_window_id = WINDOW_ID_INVALID;
    wm->drag_enabled = true;
    return wm;
}

void wm_destroy(WindowManager *wm) {
    if (!wm) return;
    for (size_t i = 0; i < wm->count; ++i) {
        draw_list_destroy(wm->windows[i]->draw_list);
        free(wm->windows[i]);
    }
    free(wm->windows);
    free(wm);
}

void wm_set_drag_enabled(WindowManager *wm, bool enabled) {
    if (!wm) return;
    wm->drag_enabled = enabled;
    if (!enabled) {
        wm->dragging = false;
        wm->drag_window_id = WINDOW_ID_INVALID;
        wm->drag_session_moved = false;
    } else {
        wm->drag_degraded = false;
        wm->drag_no_motion_sessions = 0;
    }
}

void wm_set_drag_auto_degrade(WindowManager *wm, bool enabled) {
    if (!wm) return;
    wm->drag_auto_degrade = enabled;
    if (!enabled) {
        wm->drag_no_motion_sessions = 0;
        wm->drag_degraded = false;
        if (!wm->drag_enabled) wm->drag_enabled = true;
    }
}

WindowId wm_create_window(WindowManager *wm, const RetroWindowSpec *spec) {
    if (!wm || !spec || wm->count == SIZE_MAX) return WINDOW_ID_INVALID;

    WindowId new_id = WINDOW_ID_INVALID;
    if (!wm_peek_next_id(wm, &new_id)) return WINDOW_ID_INVALID;

    int rows = 0;
    int cols = 0;
    renderer_get_screen_size(wm->renderer, &rows, &cols);
    if (rows < 4 || cols < 8) return WINDOW_ID_INVALID;

    size_t required_count = wm->count + 1;
    if (!wm_reserve(wm, required_count)) return WINDOW_ID_INVALID;

    RetroWindow *window = calloc(1, sizeof(*window));
    if (!window) return WINDOW_ID_INVALID;
    window->id = new_id;

    int max_h = rows - 1; /* reserve bottom row for status bar */
    int max_w = cols;
    if (max_h < 4) max_h = 4;
    if (max_w < 8) max_w = 8;

    window->h = spec->height < 4 ? 4 : spec->height;
    window->w = spec->width < 8 ? 8 : spec->width;
    if (window->h > max_h) window->h = max_h;
    if (window->w > max_w) window->w = max_w;
    window->y = spec->y;
    window->x = spec->x;
    window->flags = spec->flags;
    window->draw_cb = spec->draw_cb;
    window->event_cb = spec->event_cb;
    window->user_data = spec->user_data;
    snprintf(window->title, sizeof(window->title), "%s",
   spec->title ? spec->title : "Window");

    if (window->y < 0 || window->x < 0) {
        int cascade = (int)(wm->count % 5);
        window->y = 1 + cascade;
        window->x = 2 + cascade * 3;
    }

    window->draw_list = draw_list_create();
    if (!window->draw_list) {
        free(window);
        return WINDOW_ID_INVALID;
    }

    wm_clamp_window(wm, window);
    wm->windows[wm->count] = window;
    wm->count = required_count;
    wm_commit_window_id(wm, new_id);
    wm->active_id = window->id;
    wm_update_active_flags(wm);
    return window->id;
}

void wm_close_window(WindowManager *wm, WindowId id) {
    if (!wm || id == WINDOW_ID_INVALID) return;
    RetroWindow *window = wm_find_window(wm, id);
    if (!window) return;
    window->close_requested = true;
    wm_cleanup_closed(wm);
}

void wm_focus_window(WindowManager *wm, WindowId id) {
    if (!wm) return;
    RetroWindow *window = wm_find_window(wm, id);
    if (!window || window->close_requested) return;
    /* Explicit focus is also the recovery path for programmatic launches,
       shutdown prompts, and taskbar restoration. */
    window->minimized = false;
    wm->active_id = id;
    if (window->maximized) (void)wm_apply_maximized_geometry(wm, window);
    wm_update_active_flags(wm);
}

void wm_bring_to_front(WindowManager *wm, WindowId id) {
    if (!wm) return;
    int idx = wm_find_index(wm, id);
    if (idx < 0 || !window_is_visible(wm->windows[idx]) ||
        (size_t)idx == wm->count - 1) {
        return;
    }

    RetroWindow *window = wm->windows[idx];
    for (size_t i = (size_t)idx + 1; i < wm->count; ++i) {
        wm->windows[i - 1] = wm->windows[i];
    }
    wm->windows[wm->count - 1] = window;
}

bool wm_cycle_focus(WindowManager *wm) {
    if (!wm || wm->count < 2) return false;
    /* Do not cycle focus while a visible modal window is active. */
    if (wm_topmost_modal(wm)) return false;

    int idx = wm_find_index(wm, wm->active_id);
    if (idx < 0) idx = (int)wm->count - 1;

    for (size_t step = 1; step <= wm->count; ++step) {
        size_t next = ((size_t)idx + step) % wm->count;
        RetroWindow *candidate = wm->windows[next];
        if (!window_is_focusable(candidate)) continue;
        if (candidate->id == wm->active_id) return false;
        wm->active_id = candidate->id;
        wm_bring_to_front(wm, wm->active_id);
        wm_update_active_flags(wm);
        return true;
    }

    return false;
}

WindowId wm_active_window(const WindowManager *wm) {
    if (!wm) return WINDOW_ID_INVALID;
    return wm->active_id;
}

bool wm_window_exists(const WindowManager *wm, WindowId id) {
    return wm_find_index(wm, id) >= 0;
}

size_t wm_window_count(const WindowManager *wm) {
    if (!wm) return 0;
    return wm->count;
}

bool wm_minimize_window(WindowManager *wm, WindowId id) {
    if (!wm || id == WINDOW_ID_INVALID) return false;
    RetroWindow *window = wm_find_window(wm, id);
    if (!window || window->minimized ||
        (window->flags & (WINDOW_FLAG_FIXED | WINDOW_FLAG_MODAL)) != 0) {
        return false;
    }

    window->minimized = true;
    window->is_active = false;
    if (window->id == wm->drag_window_id) {
        wm->dragging = false;
        wm->drag_window_id = WINDOW_ID_INVALID;
        wm->drag_session_moved = false;
    }
    if (window->id == wm->active_id) {
        wm->active_id = WINDOW_ID_INVALID;
    }
    wm_select_topmost_visible(wm);
    return true;
}

bool wm_restore_window(WindowManager *wm, WindowId id) {
    if (!wm || id == WINDOW_ID_INVALID) return false;
    RetroWindow *window = wm_find_window(wm, id);
    if (!window || !window->minimized) return false;
    window->minimized = false;
    if (window->maximized) (void)wm_apply_maximized_geometry(wm, window);
    wm_update_active_flags(wm);
    return true;
}

bool wm_window_is_minimized(const WindowManager *wm, WindowId id) {
    RetroWindow *window = wm_find_window(wm, id);
    return window && window->minimized;
}

bool wm_maximize_window(WindowManager *wm, WindowId id) {
    if (!wm || id == WINDOW_ID_INVALID || wm->active_id != id) return false;
    RetroWindow *window = wm_find_window(wm, id);
    if (!window_can_maximize(window) || window->minimized ||
        window->maximized) {
        return false;
    }

    window->restore_y = window->y;
    window->restore_x = window->x;
    window->restore_h = window->h;
    window->restore_w = window->w;
    if (!wm_apply_maximized_geometry(wm, window)) return false;
    window->maximized = true;
    return true;
}

bool wm_unmaximize_window(WindowManager *wm, WindowId id) {
    if (!wm || id == WINDOW_ID_INVALID || wm->active_id != id) return false;
    RetroWindow *window = wm_find_window(wm, id);
    if (!window || !window->maximized || window->minimized) return false;
    if (!wm_apply_restore_geometry(wm, window)) return false;
    window->maximized = false;
    return true;
}

bool wm_toggle_maximize_window(WindowManager *wm, WindowId id) {
    return wm_window_is_maximized(wm, id)
               ? wm_unmaximize_window(wm, id)
               : wm_maximize_window(wm, id);
}

bool wm_window_is_maximized(const WindowManager *wm, WindowId id) {
    RetroWindow *window = wm_find_window(wm, id);
    return window && window->maximized;
}

bool wm_refresh_maximized_window(WindowManager *wm, WindowId id) {
    RetroWindow *window = wm_find_window(wm, id);
    if (!window || !window->maximized) return false;
    return wm_apply_maximized_geometry(wm, window);
}

bool wm_move_active_window(WindowManager *wm, int dy, int dx) {
    if (!wm) return false;
    RetroWindow *active = wm_find_window(wm, wm->active_id);
    if (!window_is_visible(active)) return false;
    if ((active->flags & WINDOW_FLAG_FIXED) || active->maximized) {
        return false;
    }
    active->y += dy;
    active->x += dx;
    wm_clamp_window(wm, active);
    return true;
}

bool wm_resize_active_window(WindowManager *wm, int dh, int dw) {
    if (!wm) return false;
    RetroWindow *window = wm_find_window(wm, wm->active_id);
    if (!window_is_visible(window) ||
        (window->flags & WINDOW_FLAG_FIXED) || window->maximized) {
        return false;
    }
    int rows = 0;
    int cols = 0;
    renderer_get_screen_size(wm->renderer, &rows, &cols);
    int min_h = 4;
    int min_w = 12;
    int next_h = window->h + dh;
    int next_w = window->w + dw;
    if (next_h < min_h) next_h = min_h;
    if (next_w < min_w) next_w = min_w;
    if (rows > 0 && window->y + next_h > rows) next_h = rows - window->y;
    if (cols > 0 && window->x + next_w > cols) next_w = cols - window->x;
    if (next_h < min_h || next_w < min_w) return false;
    if (next_h == window->h && next_w == window->w) return false;
    window->h = next_h;
    window->w = next_w;
    return true;
}

bool wm_get_drag_preview(const WindowManager *wm, WindowId *id, int *y, int *x) {
    if (!wm || !wm->dragging || wm->drag_window_id == WINDOW_ID_INVALID) return false;
    RetroWindow *drag = wm_find_window(wm, wm->drag_window_id);
    if (!window_is_visible(drag)) return false;
    if (id) *id = drag->id;
    if (y) *y = drag->y;
    if (x) *x = drag->x;
    return true;
}

bool wm_drag_is_enabled(const WindowManager *wm) {
    return wm && wm->drag_enabled;
}

bool wm_drag_is_degraded(const WindowManager *wm) {
    return wm && wm->drag_degraded;
}

int wm_drag_no_motion_sessions(const WindowManager *wm) {
    if (!wm) return 0;
    return wm->drag_no_motion_sessions;
}

static void wm_handle_mouse(WindowManager *wm, const RetroMouseEvent *mouse) {
    if (!wm || !mouse) return;

    RetroWindow *modal = wm_topmost_modal(wm);
    RetroWindow *hit = wm_top_window_at(wm, mouse->y, mouse->x);

    /* While a modal window is open, hits outside it are ignored. The modal
       window itself receives the event regardless of cursor position so it
       can dim or react to clicks anywhere. */
    if (modal) {
        if (modal->event_cb) {
            RetroEvent evt = {0};
            evt.type = RETRO_EVENT_MOUSE;
            evt.data.mouse = *mouse;
            evt.data.mouse.local_y = mouse->y - modal->y - 1;
            evt.data.mouse.local_x = mouse->x - modal->x - 1;
            evt.data.mouse.has_local_coordinates =
                evt.data.mouse.local_y >= 0 && evt.data.mouse.local_x >= 0 &&
                evt.data.mouse.local_y < modal->h - 2 &&
                evt.data.mouse.local_x < modal->w - 2;
            modal->event_cb(modal, &evt, modal->user_data);
        }
        wm_cleanup_closed(wm);
        return;
    }

    if (hit && (mouse->button1_pressed || mouse->button1_clicked)) {
        wm_focus_window(wm, hit->id);
        wm_bring_to_front(wm, hit->id);
    }

    if (wm->drag_enabled && mouse->button1_pressed && !mouse->button1_clicked &&
        hit && !(hit->flags & WINDOW_FLAG_FIXED) && !hit->maximized &&
        window_title_hit(hit, mouse->y, mouse->x)) {
        wm->dragging = true;
        wm->drag_window_id = hit->id;
        wm->drag_off_y = mouse->y - hit->y;
        wm->drag_off_x = mouse->x - hit->x;
        wm->drag_session_moved = false;
    }

    if (wm->dragging && wm->drag_window_id != WINDOW_ID_INVALID && mouse->moved) {
        RetroWindow *drag = wm_find_window(wm, wm->drag_window_id);
        if (window_is_visible(drag)) {
            drag->y = mouse->y - wm->drag_off_y;
            drag->x = mouse->x - wm->drag_off_x;
            wm_clamp_window(wm, drag);
            /* Count as "real drag" only if movement happened before release.
               Some terminals only report final pointer location on release. */
            if (!(mouse->button1_released || mouse->button1_clicked)) {
                wm->drag_session_moved = true;
            }
        }
    }

    if (wm->dragging && (mouse->button1_released || mouse->button1_clicked)) {
        if (wm->drag_session_moved) {
            wm->drag_no_motion_sessions = 0;
        } else if (wm->drag_auto_degrade) {
            wm->drag_no_motion_sessions++;
            if (wm->drag_no_motion_sessions >= 3) {
                wm->drag_enabled = false;
                wm->drag_degraded = true;
            }
        }
        wm->dragging = false;
        wm->drag_window_id = WINDOW_ID_INVALID;
        wm->drag_session_moved = false;
    }

    if (!hit) return;
    if (hit->event_cb) {
        RetroEvent evt = {0};
        evt.type = RETRO_EVENT_MOUSE;
        evt.data.mouse = *mouse;
        evt.data.mouse.local_y = mouse->y - hit->y - 1;
        evt.data.mouse.local_x = mouse->x - hit->x - 1;
        evt.data.mouse.has_local_coordinates =
            evt.data.mouse.local_y >= 0 && evt.data.mouse.local_x >= 0 &&
            evt.data.mouse.local_y < hit->h - 2 && evt.data.mouse.local_x < hit->w - 2;
        hit->event_cb(hit, &evt, hit->user_data);
    }
}

bool wm_handle_event(WindowManager *wm, const RetroEvent *event) {
    if (!wm || !event) return false;

    if (event->type == RETRO_EVENT_MOUSE) {
        wm_handle_mouse(wm, &event->data.mouse);
        wm_cleanup_closed(wm);
        return true;
    }

    if (event->type == RETRO_EVENT_RESIZE) {
        for (size_t i = 0; i < wm->count; ++i) {
            RetroWindow *window = wm->windows[i];
            if (window->maximized && !window->minimized) {
                (void)wm_apply_maximized_geometry(wm, window);
            } else {
                wm_clamp_window(wm, window);
            }
        }
        wm_cleanup_closed(wm);
        return true;
    }

    if (event->type == RETRO_EVENT_KEY) {
        RetroWindow *target = wm_find_window(wm, wm->active_id);
        RetroWindow *modal = wm_topmost_modal(wm);
        if (modal) target = modal;
        if (window_is_visible(target) && target->event_cb) {
            target->event_cb(target, event, target->user_data);
        }
        wm_cleanup_closed(wm);
        return true;
    }

    return false;
}

void wm_render(WindowManager *wm, Renderer *renderer, const RetroTheme *theme) {
    if (!wm || !renderer) return;

    const RetroTheme *active_theme = theme ? theme : retro_theme_get(RETRO_THEME_XP);

    for (size_t i = 0; i < wm->count; ++i) {
        RetroWindow *window = wm->windows[i];
        if (!window_is_visible(window) || !window->draw_list) continue;

        const RenderStyle *frame = &active_theme->window_frame_inactive;
        if (wm->dragging && window->id == wm->drag_window_id) {
            frame = &active_theme->window_frame_drag;
        } else if (window->is_active) {
            frame = &active_theme->window_frame_active;
        }

        draw_list_reset(window->draw_list);
        draw_list_fill(window->draw_list, ' ', &active_theme->window_body);
        draw_list_box(window->draw_list, window->title, frame, &active_theme->window_title);
        if (window->draw_cb) {
            window->draw_cb(window, window->draw_list, window->user_data);
        }
        if (wm->dragging && window->id == wm->drag_window_id) {
            draw_list_text(window->draw_list, 1, 2, "Dragging...",
                           &active_theme->window_body);
        }

        RenderContext *ctx = renderer_begin_region(renderer, window->h, window->w,
                                                   window->y, window->x);
        if (!ctx) continue;
        renderer_draw_list(ctx, window->draw_list);
        renderer_end(renderer, ctx);
    }
}

void wm_fail_next_growth_for_test(WindowManager *wm) {
    if (wm) wm->fail_next_growth_for_test = true;
}

bool wm_set_next_id_for_test(WindowManager *wm, WindowId next_id) {
    if (!wm || next_id <= 0 || wm_find_index(wm, next_id) >= 0) return false;
    wm->next_id = next_id;
    return true;
}

RetroWindow *wm_window(WindowManager *wm, WindowId id) {
    return wm_find_window(wm, id);
}

void retro_window_get_geometry(const RetroWindow *window, int *y, int *x, int *h,
                               int *w) {
    if (!window) return;
    if (y) *y = window->y;
    if (x) *x = window->x;
    if (h) *h = window->h;
    if (w) *w = window->w;
}

void retro_window_request_close(RetroWindow *window) {
    if (!window) return;
    window->close_requested = true;
}
