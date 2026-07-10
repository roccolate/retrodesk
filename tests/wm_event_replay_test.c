#include <assert.h>
#include <stdbool.h>

#include "wm/window_manager.h"

typedef struct WindowSpy {
    int key_events;
    int mouse_events;
    bool close_on_x;
} WindowSpy;

static void spy_draw(RetroWindow *window, DrawList *draw_list, void *user_data) {
    (void)window;
    (void)user_data;
    RenderStyle style = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false};
    draw_list_text(draw_list, 1, 1, "spy", &style);
}

static void spy_event(RetroWindow *window, const RetroEvent *event, void *user_data) {
    WindowSpy *spy = (WindowSpy *)user_data;
    if (!window || !event || !spy) return;

    if (event->type == RETRO_EVENT_KEY) {
        spy->key_events++;
        if (spy->close_on_x && event->data.key.ascii == 'x') {
            retro_window_request_close(window);
        }
    } else if (event->type == RETRO_EVENT_MOUSE) {
        spy->mouse_events++;
    }
}

static RetroEvent key_event(int key_code, char ascii) {
    RetroEvent event = {0};
    event.type = RETRO_EVENT_KEY;
    event.data.key.key_code = key_code;
    event.data.key.ascii = ascii;
    event.data.key.is_printable = (ascii >= 32 && ascii <= 126);
    return event;
}

static RetroEvent mouse_event(int y, int x, bool moved, bool b1_pressed, bool b1_released,
                              bool b1_clicked) {
    RetroEvent event = {0};
    event.type = RETRO_EVENT_MOUSE;
    event.data.mouse.y = y;
    event.data.mouse.x = x;
    event.data.mouse.moved = moved;
    event.data.mouse.button1_pressed = b1_pressed;
    event.data.mouse.button1_released = b1_released;
    event.data.mouse.button1_clicked = b1_clicked;
    return event;
}

int main(void) {
    Renderer *renderer = renderer_create(NULL);
    assert(renderer != NULL);

    WindowManager *wm = wm_create(renderer);
    assert(wm != NULL);

    WindowSpy spy_a = {0};
    WindowSpy spy_b = {0};

    RetroWindowSpec a_spec = {
        .height = 8,
        .width = 22,
        .y = 1,
        .x = 1,
        .title = "A",
        .flags = WINDOW_FLAG_APP_OWNED,
        .draw_cb = spy_draw,
        .event_cb = spy_event,
        .user_data = &spy_a,
    };
    RetroWindowSpec b_spec = {
        .height = 8,
        .width = 22,
        .y = 3,
        .x = 6,
        .title = "B",
        .flags = WINDOW_FLAG_APP_OWNED,
        .draw_cb = spy_draw,
        .event_cb = spy_event,
        .user_data = &spy_b,
    };
    RetroWindowSpec fixed_spec = {
        .height = 5,
        .width = 18,
        .y = 0,
        .x = 0,
        .title = "Fixed",
        .flags = WINDOW_FLAG_FIXED,
        .draw_cb = spy_draw,
        .event_cb = NULL,
        .user_data = NULL,
    };

    WindowId a = wm_create_window(wm, &a_spec);
    WindowId b = wm_create_window(wm, &b_spec);
    assert(a > 0 && b > 0);
    assert(wm_window_count(wm) == 2);
    assert(wm_active_window(wm) == b);

    RetroEvent click_a = mouse_event(2, 2, false, false, false, true);
    assert(wm_handle_event(wm, &click_a));
    assert(wm_active_window(wm) == a);

    RetroEvent click_overlap = mouse_event(4, 8, false, false, false, true);
    assert(wm_handle_event(wm, &click_overlap));
    assert(wm_active_window(wm) == a);

    assert(wm_cycle_focus(wm));
    assert(wm_active_window(wm) == b);

    WindowId fixed = wm_create_window(wm, &fixed_spec);
    assert(fixed > 0);
    assert(wm_window_count(wm) == 3);
    assert(wm_active_window(wm) == fixed);

    assert(wm_cycle_focus(wm));
    assert(wm_active_window(wm) == a);
    assert(wm_cycle_focus(wm));
    assert(wm_active_window(wm) == b);

    int by = 0, bx = 0, bh = 0, bw = 0;
    retro_window_get_geometry(wm_window(wm, b), &by, &bx, &bh, &bw);
    RetroEvent drag_start = mouse_event(by, bx + 1, false, true, false, false);
    RetroEvent drag_move = mouse_event(by + 2, bx + 4, true, false, false, false);
    RetroEvent drag_end = mouse_event(by + 2, bx + 4, false, false, true, false);
    assert(wm_handle_event(wm, &drag_start));
    assert(wm_handle_event(wm, &drag_move));
    assert(wm_handle_event(wm, &drag_end));

    int ny = 0, nx = 0;
    retro_window_get_geometry(wm_window(wm, b), &ny, &nx, NULL, NULL);
    assert(ny == by + 2);
    assert(nx == bx + 3);

    assert(spy_b.key_events == 0);
    RetroEvent key_a = key_event('a', 'a');
    assert(wm_handle_event(wm, &key_a));
    assert(spy_b.key_events == 1);

    spy_b.close_on_x = true;
    RetroEvent key_x = key_event('x', 'x');
    assert(wm_handle_event(wm, &key_x));
    assert(!wm_window_exists(wm, b));
    assert(wm_window_count(wm) == 2);
    assert(wm_active_window(wm) == a);
    assert(wm_window_exists(wm, fixed));

    wm_set_drag_enabled(wm, true);
    wm_set_drag_auto_degrade(wm, true);
    for (int i = 0; i < 3; ++i) {
        int ay = 0, ax = 0;
        retro_window_get_geometry(wm_window(wm, a), &ay, &ax, NULL, NULL);
        RetroEvent no_move_start = mouse_event(ay, ax + 1, false, true, false, false);
        RetroEvent no_move_end = mouse_event(ay, ax + 1, false, false, true, false);
        assert(wm_handle_event(wm, &no_move_start));
        assert(wm_handle_event(wm, &no_move_end));
    }
    assert(wm_drag_is_degraded(wm));
    assert(!wm_drag_is_enabled(wm));
    assert(wm_drag_no_motion_sessions(wm) >= 3);

    wm_render(wm, renderer, NULL);

    wm_destroy(wm);
    renderer_destroy(renderer);
    return 0;
}
