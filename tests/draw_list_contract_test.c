#include "test_harness.h"
#include <string.h>

#include "render/render.h"

int main(void) {
    DrawList *list = draw_list_create();
    TEST_REQUIRE(list != NULL);
    TEST_REQUIRE(draw_list_count(list) == 0);

    RenderStyle body = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false};
    RenderStyle frame = {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, false};
    RenderStyle title = {RENDER_COLOR_YELLOW, RENDER_COLOR_BLUE, false, true};

    TEST_REQUIRE(draw_list_fill(list, ' ', &body));
    TEST_REQUIRE(draw_list_box(list, "Window", &frame, &title));
    TEST_REQUIRE(draw_list_text(list, 2, 3, "Hello", &body));
    TEST_REQUIRE(draw_list_hline(list, 4, 1, 7, '-', &frame));
    TEST_REQUIRE(draw_list_count(list) == 4);

    DrawCommandView cmd = {0};

    TEST_REQUIRE(draw_list_get(list, 0, &cmd));
    TEST_REQUIRE(cmd.type == DRAW_COMMAND_FILL);
    TEST_REQUIRE(cmd.ch == ' ');
    TEST_REQUIRE(cmd.style.fg == body.fg);
    TEST_REQUIRE(cmd.style.bg == body.bg);

    TEST_REQUIRE(draw_list_get(list, 1, &cmd));
    TEST_REQUIRE(cmd.type == DRAW_COMMAND_BOX);
    TEST_REQUIRE(strcmp(cmd.text, "Window") == 0);
    TEST_REQUIRE(cmd.style.fg == frame.fg);
    TEST_REQUIRE(cmd.alt_style.fg == title.fg);

    TEST_REQUIRE(draw_list_get(list, 2, &cmd));
    TEST_REQUIRE(cmd.type == DRAW_COMMAND_TEXT);
    TEST_REQUIRE(cmd.y == 2);
    TEST_REQUIRE(cmd.x == 3);
    TEST_REQUIRE(strcmp(cmd.text, "Hello") == 0);

    TEST_REQUIRE(draw_list_get(list, 3, &cmd));
    TEST_REQUIRE(cmd.type == DRAW_COMMAND_HLINE);
    TEST_REQUIRE(cmd.y == 4);
    TEST_REQUIRE(cmd.x == 1);
    TEST_REQUIRE(cmd.len == 7);
    TEST_REQUIRE(cmd.ch == '-');

    TEST_REQUIRE(!draw_list_get(list, 4, &cmd));

    draw_list_reset(list);
    TEST_REQUIRE(draw_list_count(list) == 0);
    TEST_REQUIRE(!draw_list_get(list, 0, &cmd));

    draw_list_destroy(list);
    return 0;
}
