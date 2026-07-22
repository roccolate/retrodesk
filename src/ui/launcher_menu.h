#ifndef RETRODESK_UI_LAUNCHER_MENU_H
#define RETRODESK_UI_LAUNCHER_MENU_H

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "render/render.h"

#define LAUNCHER_MENU_MAX_ITEMS 8
#define LAUNCHER_MENU_PREFERRED_WIDTH 46
#define LAUNCHER_MENU_MIN_WIDTH 24

typedef struct LauncherMenuItemView {
    const char *label;
    const char *detail;
    char accelerator;
} LauncherMenuItemView;

typedef struct LauncherMenuSnapshot {
    const char *brand;
    const char *section_label;
    size_t item_count;
    size_t primary_count;
    int selected;
    LauncherMenuItemView items[LAUNCHER_MENU_MAX_ITEMS];
} LauncherMenuSnapshot;

typedef struct LauncherMenuStyles {
    RenderStyle header;
    RenderStyle section;
    RenderStyle item;
    RenderStyle selected;
    RenderStyle separator;
    RenderStyle footer;
} LauncherMenuStyles;

static inline size_t launcher_menu_count(const LauncherMenuSnapshot *snapshot) {
    if (!snapshot) return 0;
    return snapshot->item_count > LAUNCHER_MENU_MAX_ITEMS
               ? LAUNCHER_MENU_MAX_ITEMS
               : snapshot->item_count;
}

static inline size_t launcher_menu_primary_count(
    const LauncherMenuSnapshot *snapshot) {
    size_t count = launcher_menu_count(snapshot);
    if (!snapshot) return 0;
    return snapshot->primary_count > count ? count : snapshot->primary_count;
}

static inline bool launcher_menu_has_secondary(
    const LauncherMenuSnapshot *snapshot) {
    return launcher_menu_primary_count(snapshot) < launcher_menu_count(snapshot);
}

static inline int launcher_menu_preferred_height(
    const LauncherMenuSnapshot *snapshot) {
    size_t count = launcher_menu_count(snapshot);
    return (int)count + (launcher_menu_has_secondary(snapshot) ? 9 : 7);
}

static inline int launcher_menu_item_row(const LauncherMenuSnapshot *snapshot,
                                         size_t index) {
    size_t count = launcher_menu_count(snapshot);
    size_t primary = launcher_menu_primary_count(snapshot);
    if (index >= count) return -1;
    if (index < primary) return 4 + (int)index;
    return 6 + (int)primary + (int)(index - primary);
}

static inline int launcher_menu_normalize_selection(
    const LauncherMenuSnapshot *snapshot, int selected) {
    size_t count = launcher_menu_count(snapshot);
    if (count == 0) return -1;
    int normalized = selected % (int)count;
    if (normalized < 0) normalized += (int)count;
    return normalized;
}

static inline int launcher_menu_move_selection(
    const LauncherMenuSnapshot *snapshot, int selected, int delta) {
    int normalized = launcher_menu_normalize_selection(snapshot, selected);
    if (normalized < 0) return -1;
    return launcher_menu_normalize_selection(snapshot, normalized + delta);
}

static inline int launcher_menu_find_accelerator(
    const LauncherMenuSnapshot *snapshot, unsigned char key) {
    size_t count = launcher_menu_count(snapshot);
    int wanted = tolower((int)key);
    for (size_t i = 0; i < count; ++i) {
        unsigned char accelerator =
            (unsigned char)snapshot->items[i].accelerator;
        if (accelerator && tolower((int)accelerator) == wanted) {
            return (int)i;
        }
    }
    return -1;
}

static inline int launcher_menu_hit_test(const LauncherMenuSnapshot *snapshot,
                                         int local_y, int local_x,
                                         int window_rows, int window_cols) {
    if (!snapshot || local_y < 0 || local_x < 0 || window_rows < 3 ||
        window_cols < 3 || local_x >= window_cols - 2) {
        return -1;
    }

    int draw_row = local_y + 1;
    size_t count = launcher_menu_count(snapshot);
    for (size_t i = 0; i < count; ++i) {
        int item_row = launcher_menu_item_row(snapshot, i);
        if (item_row >= window_rows - 1) break;
        if (draw_row == item_row) return (int)i;
    }
    return -1;
}

static inline void launcher_menu_write(char *row, size_t row_size,
                                       size_t offset, const char *text) {
    if (!row || row_size == 0 || !text || offset >= row_size - 1) return;
    size_t available = row_size - 1 - offset;
    size_t length = strlen(text);
    if (length > available) length = available;
    memcpy(row + offset, text, length);
}

static inline void launcher_menu_render_styled(
    const LauncherMenuSnapshot *snapshot, DrawList *draw_list,
    int window_rows, int window_cols, const LauncherMenuStyles *styles) {
    if (!snapshot || !draw_list || !styles ||
        window_rows < 6 || window_cols < LAUNCHER_MENU_MIN_WIDTH) {
        return;
    }

    size_t count = launcher_menu_count(snapshot);
    size_t primary = launcher_menu_primary_count(snapshot);
    bool secondary = launcher_menu_has_secondary(snapshot);
    bool compact = window_cols < 38;
    int inner_width = window_cols - 2;
    if (inner_width > 95) inner_width = 95;

    draw_list_text(draw_list, 1, 2,
                   snapshot->brand && snapshot->brand[0]
                       ? snapshot->brand
                       : "RetroDesk",
                   &styles->header);
    draw_list_text(draw_list, 2, 2,
                   snapshot->section_label && snapshot->section_label[0]
                       ? snapshot->section_label
                       : "Applications",
                   &styles->section);
    draw_list_hline(draw_list, 3, 1, window_cols - 2, '-',
                    &styles->separator);

    for (size_t i = 0; i < count; ++i) {
        if (secondary && i == primary) {
            int separator_row = 4 + (int)primary;
            if (separator_row < window_rows - 1) {
                draw_list_hline(draw_list, separator_row, 1,
                                window_cols - 2, '-',
                                &styles->separator);
            }
            if (separator_row + 1 < window_rows - 1) {
                draw_list_text(draw_list, separator_row + 1, 2,
                               "Desktop", &styles->section);
            }
        }

        int row_y = launcher_menu_item_row(snapshot, i);
        if (row_y < 0 || row_y >= window_rows - 1) continue;

        char row[96];
        memset(row, ' ', (size_t)inner_width);
        row[inner_width] = '\0';

        bool selected = (int)i ==
                        launcher_menu_normalize_selection(snapshot,
                                                          snapshot->selected);
        launcher_menu_write(row, sizeof(row), 0, selected ? " > " : "   ");

        char shortcut[6] = "";
        if (snapshot->items[i].accelerator) {
            (void)snprintf(shortcut, sizeof(shortcut), "[%c] ",
                           snapshot->items[i].accelerator);
            launcher_menu_write(row, sizeof(row), 3, shortcut);
        }

        const char *label = snapshot->items[i].label &&
                                    snapshot->items[i].label[0]
                                ? snapshot->items[i].label
                                : "Unnamed";
        launcher_menu_write(row, sizeof(row), 7, label);

        if (!compact && snapshot->items[i].detail &&
            snapshot->items[i].detail[0]) {
            size_t detail_column = 26;
            if (detail_column < (size_t)inner_width) {
                launcher_menu_write(row, sizeof(row), detail_column,
                                    snapshot->items[i].detail);
            }
        }

        draw_list_text(draw_list, row_y, 1, row,
                       selected ? &styles->selected : &styles->item);
    }

    int footer_separator = secondary ? 6 + (int)count : 4 + (int)count;
    if (footer_separator < window_rows - 1) {
        draw_list_hline(draw_list, footer_separator, 1,
                        window_cols - 2, '-', &styles->separator);
    }
    if (footer_separator + 1 < window_rows - 1) {
        draw_list_text(draw_list, footer_separator + 1, 2,
                       compact ? "Enter open | Esc close"
                               : "Enter open   Esc close   Arrows navigate",
                       &styles->footer);
    }
}

static inline void launcher_menu_render(
    const LauncherMenuSnapshot *snapshot, DrawList *draw_list,
    int window_rows, int window_cols, const RenderStyle *text_style,
    const RenderStyle *selected_style) {
    if (!text_style || !selected_style) return;
    LauncherMenuStyles styles = {
        .header = *text_style,
        .section = *text_style,
        .item = *text_style,
        .selected = *selected_style,
        .separator = *text_style,
        .footer = *text_style,
    };
    styles.header.bold = true;
    styles.section.bold = true;
    launcher_menu_render_styled(snapshot, draw_list, window_rows,
                                window_cols, &styles);
}

#endif
