#include "ui/statusbar.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    STATUSBAR_TEXT_CAP = 256,
    STATUSBAR_LABEL_CAP = 48,
    STATUSBAR_MAX_REGIONS = STATUSBAR_MAX_APPS + 1,
};

typedef enum StatusBarRegionKind {
    STATUSBAR_REGION_NONE = 0,
    STATUSBAR_REGION_MENU,
    STATUSBAR_REGION_APP,
} StatusBarRegionKind;

typedef struct StatusBarRegion {
    int x;
    int width;
    StatusBarRegionKind kind;
    size_t app_index;
} StatusBarRegion;

struct StatusBar {
    char text[STATUSBAR_TEXT_CAP];
    bool snapshot_mode;
    StatusBarSnapshot snapshot;
    int rendered_row;
    int rendered_cols;
    size_t region_count;
    StatusBarRegion regions[STATUSBAR_MAX_REGIONS];
    char labels[STATUSBAR_MAX_APPS][STATUSBAR_LABEL_CAP];
};

StatusBar *statusbar_create(void) {
    StatusBar *sb = calloc(1, sizeof(*sb));
    if (sb) sb->rendered_row = -1;
    return sb;
}

void statusbar_set_text(StatusBar *sb, const char *fmt, ...) {
    if (!sb || !fmt) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(sb->text, sizeof(sb->text), fmt, args);
    va_end(args);
    sb->snapshot_mode = false;
}

void statusbar_set_snapshot(StatusBar *sb, const StatusBarSnapshot *snapshot) {
    if (!sb || !snapshot) return;
    sb->snapshot = *snapshot;
    if (sb->snapshot.app_count > STATUSBAR_MAX_APPS) {
        sb->snapshot.app_count = STATUSBAR_MAX_APPS;
    }
    sb->snapshot.clock_text[sizeof(sb->snapshot.clock_text) - 1] = '\0';
    sb->snapshot_mode = true;
}

static char status_mark(const StatusBarAppSnapshot *app) {
    if (!app || app->instance_count == 0) return ' ';
    return app->focused ? '*' : '.';
}

static void format_label(const StatusBarAppSnapshot *app, bool compact,
                         char *out, size_t out_size) {
    const char *label = app && app->label && app->label[0] ? app->label : "?";
    char mark = status_mark(app);
    if (compact) {
        (void)snprintf(out, out_size, "[%c%c]", label[0], mark);
    } else if (app && app->instance_count > 1) {
        (void)snprintf(out, out_size, "[%s:%zu%c]", label,
                       app->instance_count, mark);
    } else {
        (void)snprintf(out, out_size, "[%s%c]", label, mark);
    }
}

static bool needs_compact_labels(const StatusBar *sb, int cols) {
    int required = 6 + 1 + 8;
    for (size_t i = 0; i < sb->snapshot.app_count; ++i) {
        char label[STATUSBAR_LABEL_CAP];
        format_label(&sb->snapshot.apps[i], false, label, sizeof(label));
        required += 1 + (int)strlen(label);
    }
    return required > cols;
}

static bool add_region(StatusBar *sb, int x, int width,
                       StatusBarRegionKind kind, size_t app_index) {
    if (!sb || width <= 0 || sb->region_count >= STATUSBAR_MAX_REGIONS) {
        return false;
    }
    StatusBarRegion *region = &sb->regions[sb->region_count++];
    region->x = x;
    region->width = width;
    region->kind = kind;
    region->app_index = app_index;
    return true;
}

static void render_snapshot(StatusBar *sb, DrawList *draw_list,
                            int rows, int cols, const RenderStyle *style) {
    int y = rows - 1;
    int clock_x = cols >= 8 ? cols - 8 : -1;
    int content_limit = clock_x >= 0 ? clock_x - 1 : cols;
    int cursor = 0;
    bool compact = needs_compact_labels(sb, cols);
    RenderStyle active = *style;
    active.reverse = !active.reverse;
    active.bold = true;

    sb->rendered_row = y;
    sb->rendered_cols = cols;
    sb->region_count = 0;

    (void)draw_list_hline(draw_list, y, 0, cols, ' ', style);

    if (content_limit >= 6) {
        const RenderStyle *menu_style = sb->snapshot.menu_open ? &active : style;
        (void)draw_list_text(draw_list, y, cursor, "[Apps]", menu_style);
        (void)add_region(sb, cursor, 6, STATUSBAR_REGION_MENU, 0);
        cursor += 7;
    }

    for (size_t i = 0; i < sb->snapshot.app_count; ++i) {
        format_label(&sb->snapshot.apps[i], compact,
                     sb->labels[i], sizeof(sb->labels[i]));
        int width = (int)strlen(sb->labels[i]);
        if (cursor + width > content_limit) break;
        const RenderStyle *app_style = sb->snapshot.apps[i].focused ? &active : style;
        (void)draw_list_text(draw_list, y, cursor, sb->labels[i], app_style);
        (void)add_region(sb, cursor, width, STATUSBAR_REGION_APP, i);
        cursor += width + 1;
    }

    if (clock_x >= 0) {
        (void)draw_list_text(draw_list, y, clock_x,
                             sb->snapshot.clock_text, style);
    }
}

void statusbar_render(StatusBar *sb, DrawList *draw_list, int screen_rows,
                      int screen_cols, const RenderStyle *style) {
    if (!sb || !draw_list || !style || screen_rows <= 0 || screen_cols <= 0) {
        return;
    }
    if (sb->snapshot_mode) {
        render_snapshot(sb, draw_list, screen_rows, screen_cols, style);
        return;
    }

    sb->rendered_row = screen_rows - 1;
    sb->rendered_cols = screen_cols;
    sb->region_count = 0;
    (void)draw_list_hline(draw_list, sb->rendered_row, 0, screen_cols, ' ', style);
    (void)draw_list_text(draw_list, sb->rendered_row, 1, sb->text, style);
}

StatusBarAction statusbar_hit_test(const StatusBar *sb, int y, int x) {
    StatusBarAction none = {STATUSBAR_ACTION_NONE, 0};
    if (!sb || !sb->snapshot_mode || y != sb->rendered_row ||
        x < 0 || x >= sb->rendered_cols) {
        return none;
    }
    for (size_t i = 0; i < sb->region_count; ++i) {
        const StatusBarRegion *region = &sb->regions[i];
        if (x < region->x || x >= region->x + region->width) continue;
        if (region->kind == STATUSBAR_REGION_MENU) {
            StatusBarAction action = {STATUSBAR_ACTION_TOGGLE_MENU, 0};
            return action;
        }
        if (region->kind == STATUSBAR_REGION_APP) {
            StatusBarAction action = {
                STATUSBAR_ACTION_ACTIVATE_APP,
                region->app_index,
            };
            return action;
        }
    }
    return none;
}

void statusbar_destroy(StatusBar *sb) {
    free(sb);
}
