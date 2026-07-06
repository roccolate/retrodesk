#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ui/progress_bar.h"

static RenderStyle make_style(int fg, int bg, bool reverse, bool bold) {
    RenderStyle s = {
        .fg = (RenderColor)fg,
        .bg = (RenderColor)bg,
        .reverse = reverse,
        .bold = bold,
    };
    return s;
}

/* Helper: find a DRAW_COMMAND_TEXT at the given y/x and return its text. */
static const char *find_text_at(const DrawList *dl, int y, int x) {
    for (size_t i = 0; i < draw_list_count(dl); i++) {
        DrawCommandView cmd;
        if (draw_list_get(dl, i, &cmd) && cmd.type == DRAW_COMMAND_TEXT &&
            cmd.y == y && cmd.x == x && cmd.text) {
            return cmd.text;
        }
    }
    return NULL;
}

/* --- lifecycle ------------------------------------------------------- */

static void test_create_destroy(void) {
    ProgressBar *bar = progress_bar_create();
    assert(bar != NULL);
    assert(progress_bar_style(bar) == PROGRESS_BAR_DETERMINATE);
    assert(progress_bar_value(bar) == 0);
    assert(progress_bar_width(bar) == 20);
    assert(strcmp(progress_bar_label(bar), "") == 0);
    progress_bar_destroy(bar);
    progress_bar_destroy(NULL);
    printf("  PASS: create_destroy\n");
}

/* --- determinate ----------------------------------------------------- */

static void test_determinate_zero(void) {
    ProgressBar *bar = progress_bar_create();
    progress_bar_set_width(bar, 10);
    DrawList *dl = draw_list_create();
    RenderStyle frame = make_style(RENDER_COLOR_WHITE, RENDER_COLOR_BLACK, false, false);
    RenderStyle fill = make_style(RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, true, true);
    RenderStyle label = make_style(RENDER_COLOR_WHITE, RENDER_COLOR_BLACK, false, false);

    int used = progress_bar_render(bar, dl, 0, 0, 0, &frame, &fill, &label);
    /* [----------] 0%  -> 1 + 10 + 1 + 3 = 15 */
    assert(used == 15);

    /* Fill text at x=1 should be all dashes. */
    const char *fill_text = find_text_at(dl, 0, 1);
    assert(fill_text != NULL);
    assert(strlen(fill_text) == 10);
    for (int i = 0; i < 10; i++) assert(fill_text[i] == '-');

    /* Bracket at x=0. */
    const char *bracket = find_text_at(dl, 0, 0);
    assert(bracket && strcmp(bracket, "[") == 0);

    draw_list_destroy(dl);
    progress_bar_destroy(bar);
    printf("  PASS: determinate_zero\n");
}

static void test_determinate_full(void) {
    ProgressBar *bar = progress_bar_create();
    progress_bar_set_width(bar, 8);
    progress_bar_set_value(bar, 100);
    DrawList *dl = draw_list_create();
    RenderStyle frame = make_style(0, 0, false, false);
    RenderStyle fill = make_style(0, 0, true, true);
    RenderStyle label = make_style(0, 0, false, false);
    int used = progress_bar_render(bar, dl, 0, 0, 0, &frame, &fill, &label);
    /* [########] 100% -> 1 + 8 + 1 + 5 = 15 */
    assert(used == 15);

    const char *fill_text = find_text_at(dl, 0, 1);
    assert(fill_text && strlen(fill_text) == 8);
    for (int i = 0; i < 8; i++) assert(fill_text[i] == '#');

    draw_list_destroy(dl);
    progress_bar_destroy(bar);
    printf("  PASS: determinate_full\n");
}

static void test_determinate_partial(void) {
    ProgressBar *bar = progress_bar_create();
    progress_bar_set_width(bar, 10);
    progress_bar_set_value(bar, 50);
    DrawList *dl = draw_list_create();
    RenderStyle frame = make_style(0, 0, false, false);
    RenderStyle fill = make_style(0, 0, true, false);
    RenderStyle label = make_style(0, 0, false, false);
    progress_bar_render(bar, dl, 0, 0, 0, &frame, &fill, &label);

    const char *fill_text = find_text_at(dl, 0, 1);
    assert(fill_text != NULL);
    assert(strlen(fill_text) == 10);
    /* First 5 chars should be #, last 5 should be -. */
    for (int i = 0; i < 5; i++) assert(fill_text[i] == '#');
    for (int i = 5; i < 10; i++) assert(fill_text[i] == '-');

    draw_list_destroy(dl);
    progress_bar_destroy(bar);
    printf("  PASS: determinate_partial\n");
}

static void test_determinate_value_clamped(void) {
    ProgressBar *bar = progress_bar_create();
    progress_bar_set_value(bar, 250);
    assert(progress_bar_value(bar) == 100);
    progress_bar_set_value(bar, -10);
    assert(progress_bar_value(bar) == 0);
    progress_bar_destroy(bar);
    printf("  PASS: determinate_value_clamped\n");
}

/* --- indeterminate -------------------------------------------------- */

static void test_indeterminate_moves(void) {
    ProgressBar *bar = progress_bar_create();
    progress_bar_set_style(bar, PROGRESS_BAR_INDETERMINATE);
    progress_bar_set_width(bar, 10);

    /* Capture the inner pattern at several tick positions. */
    char first[32] = {0};
    char second[32] = {0};
    char third[32] = {0};
    for (int snapshot = 0; snapshot < 3; snapshot++) {
        DrawList *dl = draw_list_create();
        RenderStyle frame = make_style(0, 0, false, false);
        RenderStyle fill = make_style(0, 0, true, false);
        RenderStyle label = make_style(0, 0, false, false);
        progress_bar_render(bar, dl, 0, 0, 0, &frame, &fill, &label);
        const char *text = find_text_at(dl, 0, 1);
        assert(text != NULL);
        if (snapshot == 0) strncpy(first, text, sizeof(first) - 1);
        if (snapshot == 1) strncpy(second, text, sizeof(second) - 1);
        if (snapshot == 2) strncpy(third, text, sizeof(third) - 1);
        draw_list_destroy(dl);
        progress_bar_tick(bar);
    }
    /* At least two of the three snapshots must differ. */
    assert(strcmp(first, second) != 0 || strcmp(second, third) != 0);
    progress_bar_destroy(bar);
    printf("  PASS: indeterminate_moves\n");
}

static void test_indeterminate_block_size(void) {
    ProgressBar *bar = progress_bar_create();
    progress_bar_set_style(bar, PROGRESS_BAR_INDETERMINATE);
    progress_bar_set_width(bar, 16);

    /* Scan all tick positions and confirm the block of '=' is exactly
       PROGRESS_BAR_INDETERMINATE_BLOCK chars wide. */
    int span = 16 + 4 - 1; /* block width = 4 */
    int period = span * 2 - 2;
    for (int t = 0; t < period; t++) {
        DrawList *dl = draw_list_create();
        RenderStyle frame = make_style(0, 0, false, false);
        RenderStyle fill = make_style(0, 0, true, false);
        RenderStyle label = make_style(0, 0, false, false);
        progress_bar_render(bar, dl, 0, 0, 0, &frame, &fill, &label);
        const char *text = find_text_at(dl, 0, 1);
        assert(text && strlen(text) == 16);
        int block_count = 0;
        for (int i = 0; i < 16; i++) {
            if (text[i] == '=') block_count++;
        }
        assert(block_count == 4);
        draw_list_destroy(dl);
        progress_bar_tick(bar);
    }
    progress_bar_destroy(bar);
    printf("  PASS: indeterminate_block_size\n");
}

/* --- label ---------------------------------------------------------- */

static void test_label_determinate(void) {
    ProgressBar *bar = progress_bar_create();
    progress_bar_set_width(bar, 10);
    progress_bar_set_value(bar, 25);
    DrawList *dl = draw_list_create();
    RenderStyle frame = make_style(0, 0, false, false);
    RenderStyle fill = make_style(0, 0, true, false);
    RenderStyle label = make_style(0, 0, false, false);
    int used = progress_bar_render(bar, dl, 0, 0, 0, &frame, &fill, &label);
    /* [##--------] 25% -> 1 + 10 + 1 + 4 = 16 */
    assert(used == 16);

    /* Label " 25%" is appended starting at x=12. */
    const char *label_text = find_text_at(dl, 0, 12);
    assert(label_text && strcmp(label_text, " 25%") == 0);

    draw_list_destroy(dl);
    progress_bar_destroy(bar);
    printf("  PASS: label_determinate\n");
}

static void test_label_indeterminate(void) {
    ProgressBar *bar = progress_bar_create();
    progress_bar_set_style(bar, PROGRESS_BAR_INDETERMINATE);
    progress_bar_set_width(bar, 8);
    progress_bar_set_label(bar, "Loading...");
    DrawList *dl = draw_list_create();
    RenderStyle frame = make_style(0, 0, false, false);
    RenderStyle fill = make_style(0, 0, true, false);
    RenderStyle label = make_style(0, 0, false, false);
    int used = progress_bar_render(bar, dl, 0, 0, 0, &frame, &fill, &label);
    /* [        ] Loading... -> 1 + 8 + 1 + 11 = 21 */
    assert(used == 21);

    const char *label_text = find_text_at(dl, 0, 10);
    assert(label_text && strcmp(label_text, " Loading...") == 0);

    draw_list_destroy(dl);
    progress_bar_destroy(bar);
    printf("  PASS: label_indeterminate\n");
}

/* --- visible_width clamping ----------------------------------------- */

static void test_visible_width_clamps(void) {
    ProgressBar *bar = progress_bar_create();
    progress_bar_set_width(bar, 50);
    progress_bar_set_value(bar, 100);
    DrawList *dl = draw_list_create();
    RenderStyle frame = make_style(0, 0, false, false);
    RenderStyle fill = make_style(0, 0, true, false);
    RenderStyle label = make_style(0, 0, false, false);
    /* visible_width too small — should clamp inner width. */
    int used = progress_bar_render(bar, dl, 0, 0, 12, &frame, &fill, &label);
    assert(used == 12);
    /* inner width 12 - 2 - 5 = 5 */
    const char *fill_text = find_text_at(dl, 0, 1);
    assert(fill_text && strlen(fill_text) == 5);

    draw_list_destroy(dl);
    progress_bar_destroy(bar);
    printf("  PASS: visible_width_clamps\n");
}

/* --- null safety ---------------------------------------------------- */

static void test_null_safety(void) {
    progress_bar_destroy(NULL);
    progress_bar_set_style(NULL, PROGRESS_BAR_INDETERMINATE);
    assert(progress_bar_style(NULL) == PROGRESS_BAR_DETERMINATE);
    progress_bar_set_value(NULL, 50);
    assert(progress_bar_value(NULL) == 0);
    progress_bar_tick(NULL);
    progress_bar_set_width(NULL, 10);
    assert(progress_bar_width(NULL) == 0);
    progress_bar_set_label(NULL, "x");
    assert(strcmp(progress_bar_label(NULL), "") == 0);
    DrawList *dl = draw_list_create();
    RenderStyle s = make_style(0, 0, false, false);
    assert(progress_bar_render(NULL, dl, 0, 0, 0, &s, &s, &s) == 0);
    draw_list_destroy(dl);
    printf("  PASS: null_safety\n");
}

int main(void) {
    printf("progress_bar_test:\n");
    test_create_destroy();
    test_determinate_zero();
    test_determinate_full();
    test_determinate_partial();
    test_determinate_value_clamped();
    test_indeterminate_moves();
    test_indeterminate_block_size();
    test_label_determinate();
    test_label_indeterminate();
    test_visible_width_clamps();
    test_null_safety();
    printf("All progress_bar tests passed.\n");
    return 0;
}