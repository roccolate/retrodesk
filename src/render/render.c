#include "render/render.h"

#include <curses.h>
#include <stdlib.h>
#include <string.h>

#include "platform/platform_internal.h"

struct Renderer {
    WINDOW *screen;
    bool has_colors;
};

struct RenderTarget {
    WINDOW *win;
    int h;
    int w;
    int y;
    int x;
};

struct RenderContext {
    Renderer *renderer;
    RenderTarget *target;
    WINDOW *win;
};

static short style_to_pair(Renderer *renderer, const RenderStyle *style) {
    if (!renderer || !style || !renderer->has_colors) return 0;

    if (style->fg == RENDER_COLOR_WHITE && style->bg == RENDER_COLOR_BLUE) return 1;
    if (style->fg == RENDER_COLOR_BLACK && style->bg == RENDER_COLOR_WHITE) return 2;
    if (style->fg == RENDER_COLOR_BLACK && style->bg == RENDER_COLOR_CYAN) return 3;
    if (style->fg == RENDER_COLOR_YELLOW && style->bg == RENDER_COLOR_BLUE) return 4;
    if (style->fg == RENDER_COLOR_WHITE && style->bg == RENDER_COLOR_BLACK) return 5;
    if (style->fg == RENDER_COLOR_BLACK && style->bg == RENDER_COLOR_YELLOW) return 6;
    return 0;
}

static void apply_style(RenderContext *ctx, const RenderStyle *style) {
    if (!ctx || !ctx->win) return;

    wattrset(ctx->win, A_NORMAL);
    if (!style) return;

    short pair = style_to_pair(ctx->renderer, style);
    if (pair > 0) wattron(ctx->win, COLOR_PAIR(pair));
    if (style->reverse) wattron(ctx->win, A_REVERSE);
    if (style->bold) wattron(ctx->win, A_BOLD);
}

Renderer *renderer_create(const PlatformBackend *backend) {
    Renderer *renderer = calloc(1, sizeof(*renderer));
    if (!renderer) return NULL;

    renderer->screen = platform_native_stdscr(backend);
    renderer->has_colors = platform_native_has_colors(backend);
    if (renderer->has_colors) {
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLUE);
        init_pair(2, COLOR_BLACK, COLOR_WHITE);
        init_pair(3, COLOR_BLACK, COLOR_CYAN);
        init_pair(4, COLOR_YELLOW, COLOR_BLUE);
        init_pair(5, COLOR_WHITE, COLOR_BLACK);
        init_pair(6, COLOR_BLACK, COLOR_YELLOW);
    }
    return renderer;
}

void renderer_destroy(Renderer *renderer) {
    if (!renderer) return;
    free(renderer);
}

void renderer_get_screen_size(Renderer *renderer, int *rows, int *cols) {
    if (!renderer || !renderer->screen) return;
    int r = 0;
    int c = 0;
    getmaxyx(renderer->screen, r, c);
    if (rows) *rows = r;
    if (cols) *cols = c;
}

void renderer_clear(Renderer *renderer) {
    if (!renderer || !renderer->screen) return;
    werase(renderer->screen);
    wnoutrefresh(renderer->screen);
}

void renderer_flush(Renderer *renderer) {
    (void)renderer;
    doupdate();
}

RenderTarget *renderer_create_target(Renderer *renderer, int h, int w, int y, int x) {
    (void)renderer;
    RenderTarget *target = calloc(1, sizeof(*target));
    if (!target) return NULL;

    target->win = newwin(h, w, y, x);
    if (!target->win) {
        free(target);
        return NULL;
    }
    target->h = h;
    target->w = w;
    target->y = y;
    target->x = x;
    return target;
}

void renderer_destroy_target(RenderTarget *target) {
    if (!target) return;
    if (target->win) delwin(target->win);
    free(target);
}

void renderer_move_target(RenderTarget *target, int y, int x) {
    if (!target || !target->win) return;
    target->y = y;
    target->x = x;
    mvwin(target->win, y, x);
}

void renderer_resize_target(RenderTarget *target, int h, int w) {
    if (!target || !target->win) return;
    target->h = h;
    target->w = w;
    wresize(target->win, h, w);
}

void renderer_get_target_geometry(const RenderTarget *target, int *y, int *x, int *h,
                                  int *w) {
    if (!target) return;
    if (y) *y = target->y;
    if (x) *x = target->x;
    if (h) *h = target->h;
    if (w) *w = target->w;
}

static RenderContext *render_context_new(Renderer *renderer, RenderTarget *target,
                                         WINDOW *win) {
    RenderContext *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    ctx->renderer = renderer;
    ctx->target = target;
    ctx->win = win;
    return ctx;
}

RenderContext *renderer_begin(Renderer *renderer, RenderTarget *target) {
    if (!renderer || !target || !target->win) return NULL;
    return render_context_new(renderer, target, target->win);
}

RenderContext *renderer_begin_screen(Renderer *renderer) {
    if (!renderer || !renderer->screen) return NULL;
    return render_context_new(renderer, NULL, renderer->screen);
}

void renderer_end(Renderer *renderer, RenderContext *ctx) {
    (void)renderer;
    if (!ctx) return;
    if (ctx->win) wnoutrefresh(ctx->win);
    free(ctx);
}

void render_fill(RenderContext *ctx, char ch, const RenderStyle *style) {
    if (!ctx || !ctx->win) return;
    int rows = 0;
    int cols = 0;
    getmaxyx(ctx->win, rows, cols);
    apply_style(ctx, style);
    for (int y = 0; y < rows; ++y) {
        mvwhline(ctx->win, y, 0, ch, cols);
    }
}

void render_draw_box(RenderContext *ctx, const char *title, const RenderStyle *frame_style,
                     const RenderStyle *title_style) {
    if (!ctx || !ctx->win) return;
    apply_style(ctx, frame_style);
    box(ctx->win, 0, 0);
    if (title && title[0]) {
        int rows = 0;
        int cols = 0;
        getmaxyx(ctx->win, rows, cols);
        if (rows <= 0 || cols <= 0) return;
        int max_title = cols - 4;
        if (max_title < 0) max_title = 0;
        apply_style(ctx, title_style);
        mvwaddch(ctx->win, 0, 2, ' ');
        if (max_title > 0) {
            waddnstr(ctx->win, title, max_title);
        }
        if (2 + max_title < cols - 1) {
            waddch(ctx->win, ' ');
        }
    }
}

void render_draw_text(RenderContext *ctx, int y, int x, const char *text,
                      const RenderStyle *style) {
    if (!ctx || !ctx->win || !text) return;
    int rows = 0;
    int cols = 0;
    getmaxyx(ctx->win, rows, cols);
    if (y < 0 || y >= rows || x < 0 || x >= cols) return;
    int max_chars = cols - x;
    if (max_chars <= 0) return;
    /* Avoid writing into the final column, which can wrap/scroll in curses. */
    if (max_chars > 1) max_chars--;
    apply_style(ctx, style);
    wmove(ctx->win, y, x);
    waddnstr(ctx->win, text, max_chars);
}

void render_draw_hline(RenderContext *ctx, int y, int x, int len, char ch,
                       const RenderStyle *style) {
    if (!ctx || !ctx->win || len <= 0) return;
    apply_style(ctx, style);
    mvwhline(ctx->win, y, x, ch, len);
}
