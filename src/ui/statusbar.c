#include "ui/statusbar.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    STATUSBAR_TEXT_CAP = 256,
    STATUSBAR_LABEL_CAP = 48,
    STATUSBAR_MAX_REGIONS = STATUSBAR_MAX_APPS + 1,
    STATUSBAR_MENU_WIDTH = 6,
    STATUSBAR_CLOCK_WIDTH = 10,
    STATUSBAR_CLOCK_MIN_COLS = 24,
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

static void format_label(const StatusBarAppSnapshot *app, bool compact,
                         char *out, size_t out_size) {
    const char *label = app && app->label && app->label[0] ? app->label : "?";
    size_t instances = app ? app->instance_count : 0;

    if (compact) {
        if (instances > 1) {
            (void)snprintf(out, out_size, " %c%zu ", label[0], instances);
        } else {
            (void)snprintf(out, out_size, " %c ", label[0]);
        }
    } else if (instances > 1) {
        (void)snprintf(out, out_size, " %s x%zu ", label, instances);
    } else {
        (void)snprintf(out, out_size, " %s ", label);
    }
}

static int taskbar_content_start(int content_limit) {
    if (content_limit < STATUSBAR_MENU_WIDTH) return 0;
    return content_limit >= STATUSBAR_MENU_WIDTH + 2
               ? STATUSBAR_MENU_WIDTH + 2
               : STATUSBAR_MENU_WIDTH;
}

static bool needs_compact_labels(const StatusBar *sb, int content_limit) {
    int required = taskbar_content_start(content_limit);
    for (size_t i = 0; i < sb->snapshot.app_count; ++i) {
        char label[STATUSBAR_LABEL_CAP];
        format_label(&sb->snapshot.apps[i], false, label, sizeof(label));
        required += (int)strlen(label);
        if (i + 1 < sb->snapshot.app_count) required += 1;
    }
    return required > content_limit;
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

static RenderStyle menu_style(const RenderStyle *base, bool open) {
    RenderStyle style = *base;
    style.bold = true;
    if (open) style.reverse = !style.reverse;
    return style;
}

static RenderStyle app_style(const RenderStyle *base,
                             const StatusBarAppSnapshot *app) {
    RenderStyle style = *base;
    if (app && app->instance_count > 0) style.bold = true;
    if (app && app->focused) style.reverse = !style.reverse;
    return style;
}

static RenderStyle clock_style(const RenderStyle *base) {
    RenderStyle style = *base;
    style.bold = true;
    style.reverse = !style.reverse;
    return style;
}

static void render_snapshot(StatusBar *sb, DrawList *draw_list,
                            int rows, int cols, const RenderStyle *style) {
    int y = rows - 1;
    bool show_clock = cols >= STATUSBAR_CLOCK_MIN_COLS;
    int clock_x = show_clock ? cols - STATUSBAR_CLOCK_WIDTH : -1;
    int clock_separator_x = show_clock ? clock_x - 1 : -1;
    int content_limit = show_clock ? clock_separator_x : cols;
    int cursor = 0;
    bool compact = needs_compact_labels(sb, content_limit);

    sb->rendered_row = y;
    sb->rendered_cols = cols;
    sb->region_count = 0;

    (void)draw_list_hline(draw_list, y, 0, cols, ' ', style);

    if (content_limit >= STATUSBAR_MENU_WIDTH) {
        RenderStyle button = menu_style(style, sb->snapshot.menu_open);
        (void)draw_list_text(draw_list, y, cursor, " Apps ", &button);
        (void)add_region(sb, cursor, STATUSBAR_MENU_WIDTH,
                         STATUSBAR_REGION_MENU, 0);
        cursor += STATUSBAR_MENU_WIDTH;

        if (cursor < content_limit) {
            (void)draw_list_text(draw_list, y, cursor, "|", style);
            cursor += 2;
        }
    }

    for (size_t i = 0; i < sb->snapshot.app_count; ++i) {
        format_label(&sb->snapshot.apps[i], compact,
                     sb->labels[i], sizeof(sb->labels[i]));
        int width = (int)strlen(sb->labels[i]);
        if (cursor + width > content_limit) break;

        RenderStyle button = app_style(style, &sb->snapshot.apps[i]);
        (void)draw_list_text(draw_list, y, cursor, sb->labels[i], &button);
        (void)add_region(sb, cursor, width, STATUSBAR_REGION_APP, i);
        cursor += width;
        if (cursor < content_limit) cursor++;
    }

    if (show_clock) {
        const char *source = sb->snapshot.clock_text[0]
                                 ? sb->snapshot.clock_text
                                 : "--:--:--";
        char clock_text[STATUSBAR_CLOCK_WIDTH + 1];
        RenderStyle clock = clock_style(style);
        (void)snprintf(clock_text, sizeof(clock_text), " %.8s ", source);
        (void)draw_list_text(draw_list, y, clock_separator_x, "|", style);
        (void)draw_list_text(draw_list, y, clock_x, clock_text, &clock);
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
