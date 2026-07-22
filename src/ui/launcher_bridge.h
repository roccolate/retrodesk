#ifndef RETRODESK_UI_LAUNCHER_BRIDGE_H
#define RETRODESK_UI_LAUNCHER_BRIDGE_H

#include <limits.h>
#include <string.h>

#include "core/key_chord.h"
#include "ui/launcher_menu.h"
#include "wm/window_manager.h"

/* Temporary compile-unit bridge used only by core/desktop.c. It adapts the
   existing private launcher callbacks into the v0.5 Start-menu presentation
   without widening Desktop's public API. Remove this bridge when launcher
   ownership moves out of desktop.c during the planned Desktop decomposition. */
typedef struct LauncherChromeBridge {
    int selected;
    WindowEventCallback legacy_event;
} LauncherChromeBridge;

static LauncherChromeBridge g_launcher_chrome_bridge;

static inline LauncherMenuSnapshot launcher_chrome_snapshot(void) {
    LauncherMenuSnapshot snapshot = {0};
    snapshot.brand = "RetroDesk";
    snapshot.section_label = "Applications";
    snapshot.item_count = 5;
    snapshot.primary_count = 3;
    snapshot.selected = g_launcher_chrome_bridge.selected;
    snapshot.items[0] =
        (LauncherMenuItemView){"Files", "Browse files", 'F'};
    snapshot.items[1] =
        (LauncherMenuItemView){"Notepad", "Edit text", 'N'};
    snapshot.items[2] =
        (LauncherMenuItemView){"Diagnostics", "Runtime status", 'D'};
    snapshot.items[3] =
        (LauncherMenuItemView){"Close active window", "Ctrl+W", 'X'};
    snapshot.items[4] =
        (LauncherMenuItemView){"Close menu", "Esc", 'Q'};
    return snapshot;
}

static inline void launcher_chrome_send_key(RetroWindow *window,
                                            void *user_data,
                                            int key_code,
                                            unsigned char ascii) {
    if (!g_launcher_chrome_bridge.legacy_event) return;
    RetroEvent synthetic = {0};
    synthetic.type = RETRO_EVENT_KEY;
    synthetic.data.key.key_code = key_code;
    synthetic.data.key.ascii = ascii;
    synthetic.data.key.is_printable = ascii >= 32 && ascii <= 126;
    synthetic.data.key.text_codepoint = synthetic.data.key.is_printable
                                            ? (unsigned int)ascii
                                            : 0;
    g_launcher_chrome_bridge.legacy_event(window, &synthetic, user_data);
}

static inline void launcher_chrome_select(RetroWindow *window,
                                          void *user_data,
                                          int target) {
    LauncherMenuSnapshot snapshot = launcher_chrome_snapshot();
    int normalized = launcher_menu_normalize_selection(&snapshot, target);
    if (normalized < 0) return;

    while (g_launcher_chrome_bridge.selected != normalized) {
        launcher_chrome_send_key(window, user_data, 's', 's');
        g_launcher_chrome_bridge.selected =
            launcher_menu_move_selection(&snapshot,
                                         g_launcher_chrome_bridge.selected, 1);
    }
}

static inline void launcher_chrome_draw(RetroWindow *window,
                                        DrawList *draw_list,
                                        void *user_data) {
    (void)user_data;
    if (!window || !draw_list) return;

    int height = 0;
    int width = 0;
    retro_window_get_geometry(window, NULL, NULL, &height, &width);

    RenderStyle text = {
        RENDER_COLOR_BLACK,
        RENDER_COLOR_WHITE,
        false,
        false,
    };
    DrawCommandView fill = {0};
    if (draw_list_get(draw_list, 0, &fill)) text = fill.style;

    RenderStyle selected = text;
    selected.reverse = !selected.reverse;
    selected.bold = true;

    LauncherMenuSnapshot snapshot = launcher_chrome_snapshot();
    launcher_menu_render(&snapshot, draw_list, height, width,
                         &text, &selected);
}

static inline void launcher_chrome_event(RetroWindow *window,
                                         const RetroEvent *event,
                                         void *user_data) {
    if (!window || !event || !g_launcher_chrome_bridge.legacy_event) return;

    LauncherMenuSnapshot snapshot = launcher_chrome_snapshot();

    if (event->type == RETRO_EVENT_MOUSE) {
        const RetroMouseEvent *mouse = &event->data.mouse;
        if (!mouse->has_local_coordinates) return;

        int height = 0;
        int width = 0;
        retro_window_get_geometry(window, NULL, NULL, &height, &width);
        int hit = launcher_menu_hit_test(&snapshot,
                                         mouse->local_y,
                                         mouse->local_x,
                                         height,
                                         width);
        if (hit < 0) return;
        if (mouse->moved || mouse->button1_pressed || mouse->button1_clicked) {
            launcher_chrome_select(window, user_data, hit);
        }
        if (mouse->button1_clicked) {
            launcher_chrome_send_key(window, user_data, '\n', 0);
        }
        return;
    }

    if (event->type != RETRO_EVENT_KEY) {
        g_launcher_chrome_bridge.legacy_event(window, event, user_data);
        return;
    }

    int key = event->data.key.key_code;
    unsigned char ch = event->data.key.ascii;

    if (key == RETRO_KEY_ESC || ch == 'q' || ch == 'Q') {
        g_launcher_chrome_bridge.legacy_event(window, event, user_data);
        return;
    }

    if (key == RETRO_KEY_UP || ch == 'w' || ch == 'W' ||
        ch == 'k' || ch == 'K') {
        launcher_chrome_send_key(window, user_data, 'w', 'w');
        g_launcher_chrome_bridge.selected =
            launcher_menu_move_selection(&snapshot,
                                         g_launcher_chrome_bridge.selected, -1);
        return;
    }

    if (key == RETRO_KEY_DOWN || ch == 's' || ch == 'S' ||
        ch == 'j' || ch == 'J') {
        launcher_chrome_send_key(window, user_data, 's', 's');
        g_launcher_chrome_bridge.selected =
            launcher_menu_move_selection(&snapshot,
                                         g_launcher_chrome_bridge.selected, 1);
        return;
    }

    if (key == RETRO_KEY_HOME) {
        launcher_chrome_select(window, user_data, 0);
        return;
    }
    if (key == RETRO_KEY_END) {
        launcher_chrome_select(window, user_data,
                               (int)launcher_menu_count(&snapshot) - 1);
        return;
    }

    int accelerator = launcher_menu_find_accelerator(&snapshot, ch);
    if (accelerator >= 0 && ch != 'q' && ch != 'Q') {
        launcher_chrome_select(window, user_data, accelerator);
        launcher_chrome_send_key(window, user_data, '\n', 0);
        return;
    }

    if (key == RETRO_KEY_LF || key == RETRO_KEY_CR || ch == ' ') {
        g_launcher_chrome_bridge.legacy_event(window, event, user_data);
        return;
    }

    g_launcher_chrome_bridge.legacy_event(window, event, user_data);
}

static inline WindowId desktop_chrome_create_window(
    WindowManager *wm, const RetroWindowSpec *spec) {
    if (!wm || !spec) return WINDOW_ID_INVALID;
    if (!spec->title || strcmp(spec->title, "Launcher") != 0 ||
        spec->width < LAUNCHER_MENU_MIN_WIDTH || spec->height < 10) {
        return wm_create_window(wm, spec);
    }

    LauncherMenuSnapshot snapshot = launcher_chrome_snapshot();
    RetroWindowSpec adapted = *spec;
    adapted.height = launcher_menu_preferred_height(&snapshot);
    adapted.width = spec->width < LAUNCHER_MENU_PREFERRED_WIDTH
                        ? spec->width
                        : LAUNCHER_MENU_PREFERRED_WIDTH;
    adapted.y = INT_MAX;
    adapted.x = 0;
    adapted.title = "RetroDesk";
    adapted.draw_cb = launcher_chrome_draw;
    adapted.event_cb = launcher_chrome_event;

    g_launcher_chrome_bridge.selected = 0;
    g_launcher_chrome_bridge.legacy_event = spec->event_cb;

    WindowId id = wm_create_window(wm, &adapted);
    if (id == WINDOW_ID_INVALID) {
        memset(&g_launcher_chrome_bridge, 0,
               sizeof(g_launcher_chrome_bridge));
    }
    return id;
}

#endif
