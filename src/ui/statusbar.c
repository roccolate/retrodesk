#include "ui/statusbar.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

struct StatusBar {
    char text[256];
};

StatusBar *statusbar_create(void) {
    StatusBar *sb = calloc(1, sizeof(*sb));
    return sb;
}

void statusbar_set_text(StatusBar *sb, const char *fmt, ...) {
    if (!sb || !fmt) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(sb->text, sizeof(sb->text), fmt, args);
    va_end(args);
}

void statusbar_render(StatusBar *sb, DrawList *draw_list, int screen_rows, int screen_cols,
                      const RenderStyle *style) {
    if (!sb || !draw_list || screen_rows <= 0 || screen_cols <= 0) return;
    int y = screen_rows - 1;
    draw_list_hline(draw_list, y, 0, screen_cols, ' ', style);
    draw_list_text(draw_list, y, 1, sb->text, style);
}

void statusbar_destroy(StatusBar *sb) {
    if (!sb) return;
    free(sb);
}
