#include "render/ansi_renderer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct AnsiCell {
    char ch;
    RenderStyle style;
} AnsiCell;

struct AnsiRenderer {
    int rows;
    int cols;
    AnsiCell *prev;
    AnsiCell *curr;
    bool prev_valid;
};

static int ansi_fg_code(RenderColor color) {
    switch (color) {
    case RENDER_COLOR_BLACK:
        return 30;
    case RENDER_COLOR_RED:
        return 31;
    case RENDER_COLOR_GREEN:
        return 32;
    case RENDER_COLOR_YELLOW:
        return 33;
    case RENDER_COLOR_BLUE:
        return 34;
    case RENDER_COLOR_MAGENTA:
        return 35;
    case RENDER_COLOR_CYAN:
        return 36;
    case RENDER_COLOR_WHITE:
        return 37;
    case RENDER_COLOR_DEFAULT:
    default:
        return 39;
    }
}

static int ansi_bg_code(RenderColor color) {
    switch (color) {
    case RENDER_COLOR_BLACK:
        return 40;
    case RENDER_COLOR_RED:
        return 41;
    case RENDER_COLOR_GREEN:
        return 42;
    case RENDER_COLOR_YELLOW:
        return 43;
    case RENDER_COLOR_BLUE:
        return 44;
    case RENDER_COLOR_MAGENTA:
        return 45;
    case RENDER_COLOR_CYAN:
        return 46;
    case RENDER_COLOR_WHITE:
        return 47;
    case RENDER_COLOR_DEFAULT:
    default:
        return 49;
    }
}

static void ansi_move(FILE *out, int y, int x) {
    if (!out) return;
    fprintf(out, "\033[%d;%dH", y + 1, x + 1);
}

static void ansi_style(FILE *out, const RenderStyle *style) {
    if (!out || !style) return;
    fprintf(out, "\033[0;%d;%d", ansi_fg_code(style->fg), ansi_bg_code(style->bg));
    if (style->bold) fputs(";1", out);
    if (style->reverse) fputs(";7", out);
    fputc('m', out);
}

static void ansi_repeat(FILE *out, char ch, int count) {
    if (!out || count <= 0) return;
    for (int i = 0; i < count; ++i) fputc((ch == '\0') ? ' ' : ch, out);
}

static bool ansi_style_equal(const RenderStyle *a, const RenderStyle *b) {
    if (!a || !b) return false;
    return a->fg == b->fg && a->bg == b->bg && a->reverse == b->reverse &&
           a->bold == b->bold;
}

static bool ansi_cell_equal(const AnsiCell *a, const AnsiCell *b) {
    if (!a || !b) return false;
    return a->ch == b->ch && ansi_style_equal(&a->style, &b->style);
}

static RenderStyle ansi_default_style(void) {
    RenderStyle style = {RENDER_COLOR_DEFAULT, RENDER_COLOR_DEFAULT, false, false};
    return style;
}

AnsiRenderer *ansi_renderer_create(void) {
    return calloc(1, sizeof(AnsiRenderer));
}

void ansi_renderer_destroy(AnsiRenderer *renderer) {
    if (!renderer) return;
    free(renderer->prev);
    free(renderer->curr);
    free(renderer);
}

static bool ansi_state_resize_if_needed(AnsiRenderer *renderer, int rows, int cols) {
    if (!renderer || rows <= 0 || cols <= 0) return false;
    if (renderer->rows == rows && renderer->cols == cols && renderer->prev &&
        renderer->curr) {
        return true;
    }

    size_t count = (size_t)rows * (size_t)cols;
    AnsiCell *next_prev = calloc(count, sizeof(*next_prev));
    AnsiCell *next_curr = calloc(count, sizeof(*next_curr));
    if (!next_prev || !next_curr) {
        free(next_prev);
        free(next_curr);
        return false;
    }

    free(renderer->prev);
    free(renderer->curr);
    renderer->prev = next_prev;
    renderer->curr = next_curr;
    renderer->rows = rows;
    renderer->cols = cols;
    renderer->prev_valid = false;
    return true;
}

static void ansi_clear_curr(AnsiRenderer *renderer) {
    if (!renderer || !renderer->curr || renderer->rows <= 0 || renderer->cols <= 0) {
        return;
    }
    size_t count = (size_t)renderer->rows * (size_t)renderer->cols;
    RenderStyle def = ansi_default_style();
    for (size_t i = 0; i < count; ++i) {
        renderer->curr[i].ch = ' ';
        renderer->curr[i].style = def;
    }
}

static void ansi_set_cell(AnsiRenderer *renderer, int y, int x, char ch,
                          const RenderStyle *style) {
    if (!renderer || !renderer->curr || y < 0 || x < 0 || y >= renderer->rows ||
        x >= renderer->cols) {
        return;
    }
    size_t idx = (size_t)y * (size_t)renderer->cols + (size_t)x;
    renderer->curr[idx].ch = (ch == '\0') ? ' ' : ch;
    renderer->curr[idx].style = style ? *style : ansi_default_style();
}

static void ansi_compose_fill(AnsiRenderer *renderer, int origin_y, int origin_x, int h,
                              int w, const DrawCommandView *cmd) {
    if (!renderer || !cmd || h <= 0 || w <= 0) return;
    for (int yy = 0; yy < h; ++yy) {
        int y = origin_y + yy;
        for (int xx = 0; xx < w; ++xx) {
            ansi_set_cell(renderer, y, origin_x + xx, cmd->ch, &cmd->style);
        }
    }
}

static void ansi_compose_box(AnsiRenderer *renderer, int origin_y, int origin_x, int h,
                             int w, const DrawCommandView *cmd) {
    if (!renderer || !cmd || h <= 0 || w <= 0) return;

    if (w == 1) {
        ansi_set_cell(renderer, origin_y, origin_x, '+', &cmd->style);
        if (h > 1) {
            ansi_set_cell(renderer, origin_y + h - 1, origin_x, '+', &cmd->style);
        }
    } else {
        ansi_set_cell(renderer, origin_y, origin_x, '+', &cmd->style);
        for (int xx = 1; xx < w - 1; ++xx) {
            ansi_set_cell(renderer, origin_y, origin_x + xx, '-', &cmd->style);
        }
        ansi_set_cell(renderer, origin_y, origin_x + w - 1, '+', &cmd->style);

        if (h > 1) {
            ansi_set_cell(renderer, origin_y + h - 1, origin_x, '+', &cmd->style);
            for (int xx = 1; xx < w - 1; ++xx) {
                ansi_set_cell(renderer, origin_y + h - 1, origin_x + xx, '-',
                              &cmd->style);
            }
            ansi_set_cell(renderer, origin_y + h - 1, origin_x + w - 1, '+',
                          &cmd->style);
        }
    }

    if (w > 1 && h > 2) {
        for (int yy = 1; yy < h - 1; ++yy) {
            ansi_set_cell(renderer, origin_y + yy, origin_x, '|', &cmd->style);
            ansi_set_cell(renderer, origin_y + yy, origin_x + w - 1, '|', &cmd->style);
        }
    }

    if (cmd->text && cmd->text[0] && w > 4) {
        size_t len = strlen(cmd->text);
        int max_title = w - 4;
        if ((int)len > max_title) len = (size_t)max_title;
        ansi_set_cell(renderer, origin_y, origin_x + 2, ' ', &cmd->alt_style);
        for (size_t i = 0; i < len; ++i) {
            ansi_set_cell(renderer, origin_y, origin_x + 3 + (int)i, cmd->text[i],
                          &cmd->alt_style);
        }
        if ((int)len < max_title) {
            ansi_set_cell(renderer, origin_y, origin_x + 3 + (int)len, ' ',
                          &cmd->alt_style);
        }
    }
}

static void ansi_compose_text(AnsiRenderer *renderer, int origin_y, int origin_x, int h,
                              int w, const DrawCommandView *cmd) {
    if (!renderer || !cmd || !cmd->text || h <= 0 || w <= 0) return;
    if (cmd->y < 0 || cmd->y >= h || cmd->x < 0 || cmd->x >= w) return;

    size_t len = strlen(cmd->text);
    int max_chars = w - cmd->x;
    if ((int)len > max_chars) len = (size_t)max_chars;
    for (size_t i = 0; i < len; ++i) {
        ansi_set_cell(renderer, origin_y + cmd->y, origin_x + cmd->x + (int)i,
                      cmd->text[i], &cmd->style);
    }
}

static void ansi_compose_hline(AnsiRenderer *renderer, int origin_y, int origin_x, int h,
                               int w, const DrawCommandView *cmd) {
    if (!renderer || !cmd || h <= 0 || w <= 0 || cmd->len <= 0) return;
    if (cmd->y < 0 || cmd->y >= h) return;

    int start_x = cmd->x;
    int len = cmd->len;
    if (start_x < 0) {
        len += start_x;
        start_x = 0;
    }
    if (start_x >= w || len <= 0) return;
    if (start_x + len > w) len = w - start_x;

    for (int i = 0; i < len; ++i) {
        ansi_set_cell(renderer, origin_y + cmd->y, origin_x + start_x + i, cmd->ch,
                      &cmd->style);
    }
}

bool ansi_renderer_begin_frame(AnsiRenderer *renderer, int rows, int cols) {
    if (!ansi_state_resize_if_needed(renderer, rows, cols)) return false;
    ansi_clear_curr(renderer);
    return true;
}

void ansi_renderer_compose_draw_list(AnsiRenderer *renderer, int origin_y, int origin_x,
                                     int h, int w, const DrawList *list) {
    if (!renderer || !list || h <= 0 || w <= 0 || !renderer->curr) return;

    size_t count = draw_list_count(list);
    for (size_t i = 0; i < count; ++i) {
        DrawCommandView cmd = {0};
        if (!draw_list_get(list, i, &cmd)) continue;
        switch (cmd.type) {
        case DRAW_COMMAND_FILL:
            ansi_compose_fill(renderer, origin_y, origin_x, h, w, &cmd);
            break;
        case DRAW_COMMAND_BOX:
            ansi_compose_box(renderer, origin_y, origin_x, h, w, &cmd);
            break;
        case DRAW_COMMAND_TEXT:
            ansi_compose_text(renderer, origin_y, origin_x, h, w, &cmd);
            break;
        case DRAW_COMMAND_HLINE:
            ansi_compose_hline(renderer, origin_y, origin_x, h, w, &cmd);
            break;
        default:
            break;
        }
    }
}

void ansi_renderer_flush(AnsiRenderer *renderer, FILE *out) {
    if (!renderer || !out || !renderer->curr || renderer->rows <= 0 || renderer->cols <= 0) {
        return;
    }

    if (!renderer->prev_valid) {
        fputs("\033[2J\033[H", out);
    }

    RenderStyle active_style = ansi_default_style();
    bool has_active_style = false;
    size_t count = (size_t)renderer->rows * (size_t)renderer->cols;
    for (size_t idx = 0; idx < count; ++idx) {
        if (renderer->prev_valid &&
            ansi_cell_equal(&renderer->prev[idx], &renderer->curr[idx])) {
            continue;
        }
        int y = (int)(idx / (size_t)renderer->cols);
        int x = (int)(idx % (size_t)renderer->cols);
        ansi_move(out, y, x);
        if (!has_active_style ||
            !ansi_style_equal(&active_style, &renderer->curr[idx].style)) {
            ansi_style(out, &renderer->curr[idx].style);
            active_style = renderer->curr[idx].style;
            has_active_style = true;
        }
        ansi_repeat(out, renderer->curr[idx].ch, 1);
        renderer->prev[idx] = renderer->curr[idx];
    }

    renderer->prev_valid = true;
    ansi_renderer_reset(out);
}

void ansi_renderer_reset(FILE *out) {
    if (!out) return;
    fputs("\033[0m", out);
}
