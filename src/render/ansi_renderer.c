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
    if (color >= RENDER_COLOR_BLACK && color <= RENDER_COLOR_WHITE) return 30 + color;
    return 39; /* default */
}

static int ansi_bg_code(RenderColor color) {
    if (color >= RENDER_COLOR_BLACK && color <= RENDER_COLOR_WHITE) return 40 + color;
    return 49; /* default */
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
    char c = (ch == '\0') ? ' ' : ch;
    for (int i = 0; i < count; ++i) fputc(c, out);
}

/* Emit `count` cells starting at (y, x) as a same-style run. Caller guarantees
   all cells in the run share the same style. */
static void ansi_emit_run(FILE *out, const AnsiCell *cells, int count) {
    if (!out || !cells || count <= 0) return;

    /* Fast path: every cell holds the same character — repeat it as-is. */
    char first = cells[0].ch;
    bool all_same = true;
    for (int i = 1; i < count; ++i) {
        if (cells[i].ch != first) {
            all_same = false;
            break;
        }
    }
    if (all_same) {
        ansi_repeat(out, first, count);
        return;
    }

    /* Mixed chars: emit one fputc per cell. We avoid building a temp
       buffer here because cells can change char class mid-run (rare). */
    for (int i = 0; i < count; ++i) {
        ansi_repeat(out, cells[i].ch, 1);
    }
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
    if (!renderer || !renderer->curr || !cmd || h <= 0 || w <= 0) return;
    char fill_ch = (cmd->ch == '\0') ? ' ' : cmd->ch;
    RenderStyle fill_style = cmd->style;
    for (int yy = 0; yy < h; ++yy) {
        int y = origin_y + yy;
        if (y < 0 || y >= renderer->rows) continue;
        int x_start = origin_x < 0 ? 0 : origin_x;
        int x_end = origin_x + w;
        if (x_end > renderer->cols) x_end = renderer->cols;
        for (int xx = x_start; xx < x_end; ++xx) {
            size_t idx = (size_t)y * (size_t)renderer->cols + (size_t)xx;
            renderer->curr[idx].ch = fill_ch;
            renderer->curr[idx].style = fill_style;
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
    int cols = renderer->cols;
    int rows = renderer->rows;
    size_t total = (size_t)rows * (size_t)cols;

    for (size_t idx = 0; idx < total;) {
        /* Skip cells that already match the previous frame. */
        if (renderer->prev_valid && ansi_cell_equal(&renderer->prev[idx], &renderer->curr[idx])) {
            ++idx;
            continue;
        }

        int y = (int)(idx / (size_t)cols);
        int x = (int)(idx % (size_t)cols);

        /* Find the longest run starting at idx that stays within the same
           row and shares a single style. Small gaps of unchanged cells
           (up to BRIDGE_GAP) are included in the run rather than emitting
           a separate cursor-move sequence (a move costs ~6-8 bytes while
           bridging costs 1 byte per gap cell). */
        enum { BRIDGE_GAP = 4 };
        size_t run_end = idx + 1;
        size_t row_limit = (idx / (size_t)cols + 1) * (size_t)cols;
        if (run_end > row_limit) run_end = row_limit;
        if (run_end > total) run_end = total;
        const RenderStyle *style = &renderer->curr[idx].style;
        while (run_end < total && run_end < row_limit) {
            if (!ansi_style_equal(style, &renderer->curr[run_end].style)) {
                break;
            }
            if (renderer->prev_valid &&
                ansi_cell_equal(&renderer->prev[run_end], &renderer->curr[run_end])) {
                /* Peek ahead: if a changed cell with the same style appears
                   within BRIDGE_GAP, include the unchanged cells. */
                size_t gap = 0;
                size_t peek = run_end;
                while (peek < total && peek < row_limit && gap < BRIDGE_GAP) {
                    if (!ansi_style_equal(style, &renderer->curr[peek].style)) break;
                    if (!(renderer->prev_valid &&
                          ansi_cell_equal(&renderer->prev[peek], &renderer->curr[peek]))) {
                        break;
                    }
                    ++peek;
                    ++gap;
                }
                /* If we found a dirty cell within the gap, bridge it. */
                if (peek < total && peek < row_limit &&
                    ansi_style_equal(style, &renderer->curr[peek].style) &&
                    !(renderer->prev_valid &&
                      ansi_cell_equal(&renderer->prev[peek], &renderer->curr[peek]))) {
                    run_end = peek + 1;
                    continue;
                }
                break;
            }
            ++run_end;
        }

        int run_len = (int)(run_end - idx);
        ansi_move(out, y, x);
        if (!has_active_style || !ansi_style_equal(&active_style, style)) {
            ansi_style(out, style);
            active_style = *style;
            has_active_style = true;
        }
        ansi_emit_run(out, &renderer->curr[idx], run_len);

        for (size_t i = idx; i < run_end; ++i) {
            renderer->prev[i] = renderer->curr[i];
        }
        idx = run_end;
    }

    renderer->prev_valid = true;
    ansi_renderer_reset(out);
}

void ansi_renderer_reset(FILE *out) {
    if (!out) return;
    fputs("\033[0m", out);
}
