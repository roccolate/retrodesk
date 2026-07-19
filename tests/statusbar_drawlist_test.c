#include "test_harness.h"
#include <string.h>

#include "render/render.h"
#include "ui/statusbar.h"

int main(void) {
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
    return 0;
}
