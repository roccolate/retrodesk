#ifndef RETRODESK_UI_WINDOW_MODE_HUD_H
#define RETRODESK_UI_WINDOW_MODE_HUD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "core/key_chord.h"
#include "render/render.h"

typedef struct WindowModeHudSnapshot {
    bool active;
    bool resize;
    bool transformable;
    bool maximized;
    int y;
    int x;
    int h;
    int w;
} WindowModeHudSnapshot;

static inline bool window_mode_hud_finish_key(int key_code) {
    return key_code == RETRO_KEY_ESC || key_code == RETRO_KEY_F9 ||
           key_code == RETRO_KEY_CR || key_code == RETRO_KEY_LF;
}

static inline int window_mode_hud_row(int screen_rows) {
    return screen_rows >= 2 ? screen_rows - 2 : 0;
}

static inline bool window_mode_hud_format(const WindowModeHudSnapshot *snapshot,
                                          int screen_cols,
                                          char *buffer,
                                          size_t buffer_size) {
    if (!snapshot || !snapshot->active || !buffer || buffer_size == 0 ||
        screen_cols <= 0) {
        return false;
    }

    int written = 0;
    if (!snapshot->transformable) {
        if (snapshot->maximized) {
            written = screen_cols >= 58
                          ? snprintf(buffer, buffer_size,
                                     " MAXIMIZED  F8 restore before move/resize | Esc/F9 finish ")
                          : screen_cols >= 30
                                ? snprintf(buffer, buffer_size,
                                           " MAXIMIZED | F8 restore | Esc ")
                                : snprintf(buffer, buffer_size,
                                           " MAXIMIZED F8 ");
        } else {
            written = screen_cols >= 58
                          ? snprintf(buffer, buffer_size,
                                     " NO MOVABLE WINDOW  Focus an app | Esc/F9 finish ")
                          : screen_cols >= 30
                                ? snprintf(buffer, buffer_size,
                                           " NO MOVABLE WINDOW | Esc ")
                                : snprintf(buffer, buffer_size,
                                           " NO WINDOW ");
        }
    } else {
        const char *mode = snapshot->resize ? "RESIZE" : "MOVE";
        const char *alternate = snapshot->resize ? "move" : "resize";
        if (screen_cols >= 72) {
            written = snprintf(buffer, buffer_size,
                               " %s  %dx%d @ %d,%d   Arrows adjust | Tab %s | Enter/F9/Esc finish ",
                               mode, snapshot->w, snapshot->h,
                               snapshot->x, snapshot->y, alternate);
        } else if (screen_cols >= 42) {
            written = snprintf(buffer, buffer_size,
                               " %s %dx%d@%d,%d | Arrows | Tab | Esc ",
                               mode, snapshot->w, snapshot->h,
                               snapshot->x, snapshot->y);
        } else {
            written = snprintf(buffer, buffer_size,
                               " %s %dx%d@%d,%d ",
                               mode, snapshot->w, snapshot->h,
                               snapshot->x, snapshot->y);
        }
    }

    return written >= 0 && (size_t)written < buffer_size;
}

static inline bool window_mode_hud_render(const WindowModeHudSnapshot *snapshot,
                                          DrawList *draw_list,
                                          int screen_rows,
                                          int screen_cols,
                                          const RenderStyle *style) {
    if (!snapshot || !snapshot->active || !draw_list || !style ||
        screen_rows < 2 || screen_cols <= 0) {
        return false;
    }

    char text[160];
    if (!window_mode_hud_format(snapshot, screen_cols, text, sizeof(text))) {
        return false;
    }

    size_t text_len = strlen(text);
    int x = text_len < (size_t)screen_cols
                ? (screen_cols - (int)text_len) / 2
                : 0;
    int row = window_mode_hud_row(screen_rows);
    return draw_list_hline(draw_list, row, 0, screen_cols, ' ', style) &&
           draw_list_text(draw_list, row, x, text, style);
}

#endif
