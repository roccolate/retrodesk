#include "test_harness.h"

#include <string.h>

#include "render/render.h"
#include "ui/launcher_menu.h"
#include "ui/statusbar.h"
#include "ui/theme_surface.h"

static bool style_equal(RenderStyle a, RenderStyle b) {
    return a.fg == b.fg && a.bg == b.bg &&
           a.reverse == b.reverse && a.bold == b.bold;
}

static StatusBarSnapshot taskbar_snapshot(bool menu_open) {
    StatusBarSnapshot snapshot = {0};
    snapshot.menu_open = menu_open;
    memcpy(snapshot.clock_text, "12:34:56", sizeof(snapshot.clock_text));
    snapshot.app_count = 3;
    snapshot.apps[0] = (StatusBarAppSnapshot){"filemanager", "Files", 1, true};
    snapshot.apps[1] = (StatusBarAppSnapshot){"notepad", "Notepad", 2, false};
    snapshot.apps[2] = (StatusBarAppSnapshot){"diagnostics", "Diag", 0, false};
    return snapshot;
}

static void test_legacy_status_text(void) {
    StatusBar *status = statusbar_create();
    DrawList *list = draw_list_create();
    TEST_REQUIRE(status != NULL);
    TEST_REQUIRE(list != NULL);

    RenderStyle style = {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, false};
    statusbar_set_text(status, "retro-status");
    statusbar_render(status, list, 6, 20, &style);

    TEST_REQUIRE(draw_list_count(list) == 2);

    DrawCommandView cmd = {0};
    TEST_REQUIRE(draw_list_get(list, 0, &cmd));
    TEST_REQUIRE(cmd.type == DRAW_COMMAND_HLINE);
    TEST_REQUIRE(cmd.y == 5);
    TEST_REQUIRE(cmd.x == 0);
    TEST_REQUIRE(cmd.len == 20);
    TEST_REQUIRE(cmd.ch == ' ');
    TEST_REQUIRE(style_equal(cmd.style, style));

    TEST_REQUIRE(draw_list_get(list, 1, &cmd));
    TEST_REQUIRE(cmd.type == DRAW_COMMAND_TEXT);
    TEST_REQUIRE(cmd.y == 5);
    TEST_REQUIRE(cmd.x == 1);
    TEST_REQUIRE(strcmp(cmd.text, "retro-status") == 0);
    TEST_REQUIRE(style_equal(cmd.style, style));

    draw_list_destroy(list);
    statusbar_destroy(status);
}

static void test_taskbar_wide_snapshot_and_hits(void) {
    StatusBar *status = statusbar_create();
    DrawList *list = draw_list_create();
    TEST_REQUIRE(status != NULL);
    TEST_REQUIRE(list != NULL);

    const RetroSurfaceTheme *xp =
        retro_surface_theme_get(RETRO_THEME_XP);
    StatusBarSnapshot snapshot = taskbar_snapshot(true);
    RenderStyle style = xp->taskbar_base;
    statusbar_set_snapshot(status, &snapshot);
    statusbar_render(status, list, 6, 80, &style);

    TEST_REQUIRE(draw_list_count(list) == 8);

    DrawCommandView cmd = {0};
    TEST_REQUIRE(draw_list_get(list, 0, &cmd));
    TEST_REQUIRE(style_equal(cmd.style, xp->taskbar_base));

    TEST_REQUIRE(draw_list_get(list, 1, &cmd));
    TEST_REQUIRE(cmd.type == DRAW_COMMAND_TEXT);
    TEST_REQUIRE(cmd.x == 0);
    TEST_REQUIRE(strcmp(cmd.text, " Apps ") == 0);
    TEST_REQUIRE(style_equal(cmd.style, xp->taskbar_menu_open));

    TEST_REQUIRE(draw_list_get(list, 2, &cmd));
    TEST_REQUIRE(cmd.x == 6);
    TEST_REQUIRE(strcmp(cmd.text, "|") == 0);
    TEST_REQUIRE(style_equal(cmd.style, xp->taskbar_base));

    TEST_REQUIRE(draw_list_get(list, 3, &cmd));
    TEST_REQUIRE(cmd.x == 8);
    TEST_REQUIRE(strcmp(cmd.text, " Files ") == 0);
    TEST_REQUIRE(style_equal(cmd.style, xp->taskbar_app_focused));

    TEST_REQUIRE(draw_list_get(list, 4, &cmd));
    TEST_REQUIRE(cmd.x == 16);
    TEST_REQUIRE(strcmp(cmd.text, " Notepad x2 ") == 0);
    TEST_REQUIRE(style_equal(cmd.style, xp->taskbar_app_running));

    TEST_REQUIRE(draw_list_get(list, 5, &cmd));
    TEST_REQUIRE(cmd.x == 29);
    TEST_REQUIRE(strcmp(cmd.text, " Diag ") == 0);
    TEST_REQUIRE(style_equal(cmd.style, xp->taskbar_app_idle));

    TEST_REQUIRE(draw_list_get(list, 6, &cmd));
    TEST_REQUIRE(cmd.x == 69);
    TEST_REQUIRE(strcmp(cmd.text, "|") == 0);
    TEST_REQUIRE(style_equal(cmd.style, xp->taskbar_base));

    TEST_REQUIRE(draw_list_get(list, 7, &cmd));
    TEST_REQUIRE(cmd.x == 70);
    TEST_REQUIRE(strcmp(cmd.text, " 12:34:56 ") == 0);
    TEST_REQUIRE(style_equal(cmd.style, xp->taskbar_clock));

    StatusBarAction action = statusbar_hit_test(status, 5, 1);
    TEST_REQUIRE(action.kind == STATUSBAR_ACTION_TOGGLE_MENU);

    action = statusbar_hit_test(status, 5, 6);
    TEST_REQUIRE(action.kind == STATUSBAR_ACTION_NONE);

    action = statusbar_hit_test(status, 5, 8);
    TEST_REQUIRE(action.kind == STATUSBAR_ACTION_ACTIVATE_APP);
    TEST_REQUIRE(action.app_index == 0);

    action = statusbar_hit_test(status, 5, 16);
    TEST_REQUIRE(action.kind == STATUSBAR_ACTION_ACTIVATE_APP);
    TEST_REQUIRE(action.app_index == 1);

    action = statusbar_hit_test(status, 5, 75);
    TEST_REQUIRE(action.kind == STATUSBAR_ACTION_NONE);

    action = statusbar_hit_test(status, 4, 1);
    TEST_REQUIRE(action.kind == STATUSBAR_ACTION_NONE);

    draw_list_destroy(list);
    statusbar_destroy(status);
}

static void test_taskbar_responsive_layouts(void) {
    const RetroSurfaceTheme *xp =
        retro_surface_theme_get(RETRO_THEME_XP);
    RenderStyle style = xp->taskbar_base;
    StatusBarSnapshot snapshot = taskbar_snapshot(false);
    DrawCommandView cmd = {0};

    StatusBar *status = statusbar_create();
    DrawList *list = draw_list_create();
    TEST_REQUIRE(status != NULL);
    TEST_REQUIRE(list != NULL);

    statusbar_set_snapshot(status, &snapshot);
    statusbar_render(status, list, 6, 40, &style);
    TEST_REQUIRE(draw_list_count(list) == 8);

    TEST_REQUIRE(draw_list_get(list, 1, &cmd));
    TEST_REQUIRE(strcmp(cmd.text, " Apps ") == 0);
    TEST_REQUIRE(style_equal(cmd.style, xp->taskbar_menu));

    TEST_REQUIRE(draw_list_get(list, 3, &cmd));
    TEST_REQUIRE(cmd.x == 8);
    TEST_REQUIRE(strcmp(cmd.text, " F ") == 0);
    TEST_REQUIRE(style_equal(cmd.style, xp->taskbar_app_focused));

    TEST_REQUIRE(draw_list_get(list, 4, &cmd));
    TEST_REQUIRE(cmd.x == 12);
    TEST_REQUIRE(strcmp(cmd.text, " N2 ") == 0);
    TEST_REQUIRE(style_equal(cmd.style, xp->taskbar_app_running));

    TEST_REQUIRE(draw_list_get(list, 5, &cmd));
    TEST_REQUIRE(cmd.x == 17);
    TEST_REQUIRE(strcmp(cmd.text, " D ") == 0);
    TEST_REQUIRE(style_equal(cmd.style, xp->taskbar_app_idle));

    TEST_REQUIRE(draw_list_get(list, 6, &cmd));
    TEST_REQUIRE(cmd.x == 29);
    TEST_REQUIRE(strcmp(cmd.text, "|") == 0);

    TEST_REQUIRE(draw_list_get(list, 7, &cmd));
    TEST_REQUIRE(cmd.x == 30);
    TEST_REQUIRE(strcmp(cmd.text, " 12:34:56 ") == 0);
    TEST_REQUIRE(style_equal(cmd.style, xp->taskbar_clock));

    StatusBarAction action = statusbar_hit_test(status, 5, 17);
    TEST_REQUIRE(action.kind == STATUSBAR_ACTION_ACTIVATE_APP);
    TEST_REQUIRE(action.app_index == 2);
    action = statusbar_hit_test(status, 5, 30);
    TEST_REQUIRE(action.kind == STATUSBAR_ACTION_NONE);

    draw_list_destroy(list);
    statusbar_destroy(status);

    status = statusbar_create();
    list = draw_list_create();
    TEST_REQUIRE(status != NULL);
    TEST_REQUIRE(list != NULL);

    statusbar_set_snapshot(status, &snapshot);
    statusbar_render(status, list, 6, 20, &style);
    TEST_REQUIRE(draw_list_count(list) == 6);

    TEST_REQUIRE(draw_list_get(list, 3, &cmd));
    TEST_REQUIRE(cmd.x == 8);
    TEST_REQUIRE(strcmp(cmd.text, " F ") == 0);

    TEST_REQUIRE(draw_list_get(list, 4, &cmd));
    TEST_REQUIRE(cmd.x == 12);
    TEST_REQUIRE(strcmp(cmd.text, " N2 ") == 0);

    TEST_REQUIRE(draw_list_get(list, 5, &cmd));
    TEST_REQUIRE(cmd.x == 17);
    TEST_REQUIRE(strcmp(cmd.text, " D ") == 0);

    action = statusbar_hit_test(status, 5, 19);
    TEST_REQUIRE(action.kind == STATUSBAR_ACTION_ACTIVATE_APP);
    TEST_REQUIRE(action.app_index == 2);

    draw_list_destroy(list);
    statusbar_destroy(status);
}

static void test_launcher_menu_layout_contract(void) {
    LauncherMenuSnapshot snapshot = {0};
    snapshot.brand = "RetroDesk";
    snapshot.section_label = "Applications";
    snapshot.item_count = 4;
    snapshot.primary_count = 3;
    snapshot.selected = 1;
    snapshot.items[0] =
        (LauncherMenuItemView){"Files", "Browse files", 'F'};
    snapshot.items[1] =
        (LauncherMenuItemView){"Notepad", "Edit text", 'N'};
    snapshot.items[2] =
        (LauncherMenuItemView){"Diagnostics", "Runtime status", 'D'};
    snapshot.items[3] =
        (LauncherMenuItemView){"Close active window", "Ctrl+W", 'X'};

    TEST_REQUIRE(launcher_menu_preferred_height(&snapshot) == 13);
    TEST_REQUIRE(launcher_menu_item_row(&snapshot, 0) == 4);
    TEST_REQUIRE(launcher_menu_item_row(&snapshot, 3) == 9);
    TEST_REQUIRE(launcher_menu_move_selection(&snapshot, 0, -1) == 3);
    TEST_REQUIRE(launcher_menu_find_accelerator(&snapshot, 'n') == 1);
    TEST_REQUIRE(launcher_menu_hit_test(&snapshot, 4, 5, 13, 46) == 1);
    TEST_REQUIRE(launcher_menu_hit_test(&snapshot, 6, 5, 13, 46) == -1);

    const RetroSurfaceTheme *xp =
        retro_surface_theme_get(RETRO_THEME_XP);
    LauncherMenuStyles styles = {
        .header = xp->launcher_header,
        .section = xp->launcher_section,
        .item = xp->launcher_item,
        .selected = xp->launcher_selected,
        .separator = xp->launcher_separator,
        .footer = xp->launcher_footer,
    };
    DrawList *list = draw_list_create();
    TEST_REQUIRE(list != NULL);

    launcher_menu_render_styled(&snapshot, list, 13, 46, &styles);
    TEST_REQUIRE(draw_list_count(list) == 11);

    DrawCommandView cmd = {0};
    TEST_REQUIRE(draw_list_get(list, 0, &cmd));
    TEST_REQUIRE(strcmp(cmd.text, "RetroDesk") == 0);
    TEST_REQUIRE(style_equal(cmd.style, xp->launcher_header));

    TEST_REQUIRE(draw_list_get(list, 1, &cmd));
    TEST_REQUIRE(strcmp(cmd.text, "Applications") == 0);
    TEST_REQUIRE(style_equal(cmd.style, xp->launcher_section));

    TEST_REQUIRE(draw_list_get(list, 2, &cmd));
    TEST_REQUIRE(style_equal(cmd.style, xp->launcher_separator));

    TEST_REQUIRE(draw_list_get(list, 3, &cmd));
    TEST_REQUIRE(strstr(cmd.text, "[F] Files") != NULL);
    TEST_REQUIRE(strstr(cmd.text, "Browse files") != NULL);
    TEST_REQUIRE(style_equal(cmd.style, xp->launcher_item));

    TEST_REQUIRE(draw_list_get(list, 4, &cmd));
    TEST_REQUIRE(strstr(cmd.text, "> [N] Notepad") != NULL);
    TEST_REQUIRE(style_equal(cmd.style, xp->launcher_selected));

    TEST_REQUIRE(draw_list_get(list, 7, &cmd));
    TEST_REQUIRE(strcmp(cmd.text, "Desktop") == 0);
    TEST_REQUIRE(style_equal(cmd.style, xp->launcher_section));

    TEST_REQUIRE(draw_list_get(list, 8, &cmd));
    TEST_REQUIRE(strstr(cmd.text, "[X] Close active window") != NULL);
    TEST_REQUIRE(style_equal(cmd.style, xp->launcher_item));

    TEST_REQUIRE(draw_list_get(list, 10, &cmd));
    TEST_REQUIRE(style_equal(cmd.style, xp->launcher_footer));

    draw_list_destroy(list);
}

int main(void) {
    test_legacy_status_text();
    test_taskbar_wide_snapshot_and_hits();
    test_taskbar_responsive_layouts();
    test_launcher_menu_layout_contract();
    return 0;
}
