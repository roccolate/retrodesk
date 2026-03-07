#include <assert.h>
#include <string.h>

#include "render/render.h"

int main(void) {
    DrawList *list = draw_list_create();
    assert(list != NULL);
    assert(draw_list_count(list) == 0);

    RenderStyle body = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false};
    RenderStyle frame = {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, false};
    RenderStyle title = {RENDER_COLOR_YELLOW, RENDER_COLOR_BLUE, false, true};

    assert(draw_list_fill(list, ' ', &body));
    assert(draw_list_box(list, "Window", &frame, &title));
    assert(draw_list_text(list, 2, 3, "Hello", &body));
    assert(draw_list_hline(list, 4, 1, 7, '-', &frame));
    assert(draw_list_count(list) == 4);

    DrawCommandView cmd = {0};

    assert(draw_list_get(list, 0, &cmd));
    assert(cmd.type == DRAW_COMMAND_FILL);
    assert(cmd.ch == ' ');
    assert(cmd.style.fg == body.fg);
    assert(cmd.style.bg == body.bg);

    assert(draw_list_get(list, 1, &cmd));
    assert(cmd.type == DRAW_COMMAND_BOX);
    assert(strcmp(cmd.text, "Window") == 0);
    assert(cmd.style.fg == frame.fg);
    assert(cmd.alt_style.fg == title.fg);

    assert(draw_list_get(list, 2, &cmd));
    assert(cmd.type == DRAW_COMMAND_TEXT);
    assert(cmd.y == 2);
    assert(cmd.x == 3);
    assert(strcmp(cmd.text, "Hello") == 0);

    assert(draw_list_get(list, 3, &cmd));
    assert(cmd.type == DRAW_COMMAND_HLINE);
    assert(cmd.y == 4);
    assert(cmd.x == 1);
    assert(cmd.len == 7);
    assert(cmd.ch == '-');

    assert(!draw_list_get(list, 4, &cmd));

    draw_list_reset(list);
    assert(draw_list_count(list) == 0);
    assert(!draw_list_get(list, 0, &cmd));

    draw_list_destroy(list);
    return 0;
}
