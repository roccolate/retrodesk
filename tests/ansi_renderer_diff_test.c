#include "test_harness.h"
#include <stdio.h>

#include "render/ansi_renderer.h"
#include "render/render.h"

int main(void) {
    FILE *out = tmpfile();
    TEST_REQUIRE(out != NULL);
    AnsiRenderer *renderer = ansi_renderer_create();
    TEST_REQUIRE(renderer != NULL);

    DrawList *list = draw_list_create();
    TEST_REQUIRE(list != NULL);
    RenderStyle style = {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, false};
    TEST_REQUIRE(draw_list_fill(list, ' ', &style));
    TEST_REQUIRE(draw_list_text(list, 0, 0, "A", &style));

    TEST_REQUIRE(ansi_renderer_begin_frame(renderer, 3, 8));
    ansi_renderer_compose_draw_list(renderer, 0, 0, 3, 8, list);
    ansi_renderer_flush(renderer, out);
    long first_size = ftell(out);
    TEST_REQUIRE(first_size > 0);

    TEST_REQUIRE(ansi_renderer_begin_frame(renderer, 3, 8));
    ansi_renderer_compose_draw_list(renderer, 0, 0, 3, 8, list);
    ansi_renderer_flush(renderer, out);
    long second_size = ftell(out);
    TEST_REQUIRE(second_size > first_size);

    long delta = second_size - first_size;
    TEST_REQUIRE(delta < first_size);

    ansi_renderer_destroy(renderer);
    draw_list_destroy(list);
    fclose(out);
    return 0;
}
