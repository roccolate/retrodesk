#include "test_harness.h"
#include <string.h>

#include "core/utf8.h"
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

    char long_utf8[261];
    for (size_t i = 0; i < 130; ++i) {
        long_utf8[i * 2] = (char)0xc3;
        long_utf8[i * 2 + 1] = (char)0xa9;
    }
    long_utf8[260] = '\0';

    TEST_REQUIRE(draw_list_text(list, 0, 0, long_utf8, &body));
    TEST_REQUIRE(draw_list_get(list, 0, &cmd));
    size_t clipped_length = strlen(cmd.text);
    TEST_REQUIRE(clipped_length == 254);
    TEST_REQUIRE(retro_utf8_validate(cmd.text, clipped_length));

    draw_list_destroy(list);
    return 0;
}
