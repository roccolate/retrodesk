#include "test_harness.h"

#include <string.h>

#include "render/render.h"
#include "ui/statusbar.h"

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

    TEST_REQUIRE(draw_list_get(list, 1, &cmd));
    TEST_REQUIRE(cmd.type == DRAW_COMMAND_TEXT);
    TEST_REQUIRE(cmd.y == 5);
    TEST_REQUIRE(cmd.x == 1);
    TEST_REQUIRE(strcmp(cmd.text, "retro-status") == 0);

    draw_list_destroy(list);
    statusbar_destroy(status);
}

static void test_taskbar_wide_snapshot_and_hits(void) {
    StatusBar *status = statusbar_create();
    DrawList *list = draw_list_create();
    TEST_REQUIRE(status != NULL);
    TEST_REQUIRE(list != NULL);

    StatusBarSnapshot snapshot = taskbar_snapshot(true);
    RenderStyle style = {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, false};
    statusbar_set_snapshot(status, &snapshot);
    statusbar_render(status, list, 6, 80, &style);

    TEST_REQUIRE(draw_list_count(list) == 8);

    DrawCommandView cmd = {0};
    TEST_REQUIRE(draw_list_get(list, 1, &cmd));
    TEST_REQUIRE(cmd.type == DRAW_COMMAND_TEXT);
    TEST_REQUIRE(cmd.x == 0);
    TEST_REQUIRE(strcmp(cmd.text, " Apps ") == 0);
    TEST_REQUIRE(cmd.style.reverse);
    TEST_REQUIRE(cmd.style.bold);

    TEST_REQUIRE(draw_list_get(list, 2, &cmd));
    TEST_REQUIRE(cmd.x == 6);
    TEST_REQUIRE(strcmp(cmd.text, "|") == 0);

    TEST_REQUIRE(draw_list_get(list, 3, &cmd));
    TEST_REQUIRE(cmd.x == 8);
    TEST_REQUIRE(strcmp(cmd.text, " Files ") == 0);
    TEST_REQUIRE(cmd.style.reverse);
    TEST_REQUIRE(cmd.style.bold);

    TEST_REQUIRE(draw_list_get(list, 4, &cmd));
    TEST_REQUIRE(cmd.x == 16);
    TEST_REQUIRE(strcmp(cmd.text, " Notepad x2 ") == 0);
    TEST_REQUIRE(!cmd.style.reverse);
    TEST_REQUIRE(cmd.style.bold);

    TEST_REQUIRE(draw_list_get(list, 5, &cmd));
    TEST_REQUIRE(cmd.x == 29);
    TEST_REQUIRE(strcmp(cmd.text, " Diag ") == 0);
    TEST_REQUIRE(!cmd.style.reverse);
    TEST_REQUIRE(!cmd.style.bold);

    TEST_REQUIRE(draw_list_get(list, 6, &cmd));
    TEST_REQUIRE(cmd.x == 69);
    TEST_REQUIRE(strcmp(cmd.text, "|") == 0);

    TEST_REQUIRE(draw_list_get(list, 7, &cmd));
    TEST_REQUIRE(cmd.x == 70);
    TEST_REQUIRE(strcmp(cmd.text, " 12:34:56 ") == 0);
    TEST_REQUIRE(cmd.style.reverse);
    TEST_REQUIRE(cmd.style.bold);

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
    RenderStyle style = {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, false};
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
    TEST_REQUIRE(cmd.style.bold);
    TEST_REQUIRE(!cmd.style.reverse);

    TEST_REQUIRE(draw_list_get(list, 3, &cmd));
    TEST_REQUIRE(cmd.x == 8);
    TEST_REQUIRE(strcmp(cmd.text, " F ") == 0);

    TEST_REQUIRE(draw_list_get(list, 4, &cmd));
    TEST_REQUIRE(cmd.x == 12);
    TEST_REQUIRE(strcmp(cmd.text, " N2 ") == 0);

    TEST_REQUIRE(draw_list_get(list, 5, &cmd));
    TEST_REQUIRE(cmd.x == 17);
    TEST_REQUIRE(strcmp(cmd.text, " D ") == 0);

    TEST_REQUIRE(draw_list_get(list, 6, &cmd));
    TEST_REQUIRE(cmd.x == 29);
    TEST_REQUIRE(strcmp(cmd.text, "|") == 0);

    TEST_REQUIRE(draw_list_get(list, 7, &cmd));
    TEST_REQUIRE(cmd.x == 30);
    TEST_REQUIRE(strcmp(cmd.text, " 12:34:56 ") == 0);

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

int main(void) {
    test_legacy_status_text();
    test_taskbar_wide_snapshot_and_hits();
    test_taskbar_responsive_layouts();
    return 0;
}
