#include "test_harness.h"

#include <string.h>

#include "render/render.h"
#include "ui/statusbar.h"

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

static void test_taskbar_snapshot_and_hits(void) {
    StatusBar *status = statusbar_create();
    DrawList *list = draw_list_create();
    TEST_REQUIRE(status != NULL);
    TEST_REQUIRE(list != NULL);

    StatusBarSnapshot snapshot = {0};
    snapshot.menu_open = true;
    memcpy(snapshot.clock_text, "12:34:56", sizeof(snapshot.clock_text));
    snapshot.app_count = 3;
    snapshot.apps[0] = (StatusBarAppSnapshot){"filemanager", "Files", 1, true};
    snapshot.apps[1] = (StatusBarAppSnapshot){"notepad", "Notepad", 2, false};
    snapshot.apps[2] = (StatusBarAppSnapshot){"diagnostics", "Diag", 0, false};

    RenderStyle style = {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, false};
    statusbar_set_snapshot(status, &snapshot);
    statusbar_render(status, list, 6, 80, &style);

    TEST_REQUIRE(draw_list_count(list) == 6);

    DrawCommandView cmd = {0};
    TEST_REQUIRE(draw_list_get(list, 1, &cmd));
    TEST_REQUIRE(cmd.type == DRAW_COMMAND_TEXT);
    TEST_REQUIRE(cmd.x == 0);
    TEST_REQUIRE(strcmp(cmd.text, "[Apps]") == 0);
    TEST_REQUIRE(cmd.style.reverse);

    TEST_REQUIRE(draw_list_get(list, 2, &cmd));
    TEST_REQUIRE(cmd.x == 7);
    TEST_REQUIRE(strcmp(cmd.text, "[Files*]") == 0);
    TEST_REQUIRE(cmd.style.reverse);

    TEST_REQUIRE(draw_list_get(list, 3, &cmd));
    TEST_REQUIRE(cmd.x == 16);
    TEST_REQUIRE(strcmp(cmd.text, "[Notepad:2.]") == 0);

    TEST_REQUIRE(draw_list_get(list, 4, &cmd));
    TEST_REQUIRE(strcmp(cmd.text, "[Diag ]") == 0);

    TEST_REQUIRE(draw_list_get(list, 5, &cmd));
    TEST_REQUIRE(cmd.x == 72);
    TEST_REQUIRE(strcmp(cmd.text, "12:34:56") == 0);

    StatusBarAction action = statusbar_hit_test(status, 5, 1);
    TEST_REQUIRE(action.kind == STATUSBAR_ACTION_TOGGLE_MENU);

    action = statusbar_hit_test(status, 5, 7);
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

int main(void) {
    test_legacy_status_text();
    test_taskbar_snapshot_and_hits();
    return 0;
}
