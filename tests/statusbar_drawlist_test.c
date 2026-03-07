#include <assert.h>
#include <string.h>

#include "render/render.h"
#include "ui/statusbar.h"

int main(void) {
    StatusBar *status = statusbar_create();
    DrawList *list = draw_list_create();
    assert(status != NULL);
    assert(list != NULL);

    RenderStyle style = {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, false};
    statusbar_set_text(status, "retro-status");
    statusbar_render(status, list, 6, 20, &style);

    assert(draw_list_count(list) == 2);

    DrawCommandView cmd = {0};
    assert(draw_list_get(list, 0, &cmd));
    assert(cmd.type == DRAW_COMMAND_HLINE);
    assert(cmd.y == 5);
    assert(cmd.x == 0);
    assert(cmd.len == 20);
    assert(cmd.ch == ' ');

    assert(draw_list_get(list, 1, &cmd));
    assert(cmd.type == DRAW_COMMAND_TEXT);
    assert(cmd.y == 5);
    assert(cmd.x == 1);
    assert(strcmp(cmd.text, "retro-status") == 0);

    draw_list_destroy(list);
    statusbar_destroy(status);
    return 0;
}
