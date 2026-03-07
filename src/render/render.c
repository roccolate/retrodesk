#include "render/render.h"

#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "render/ansi_renderer.h"
#include "platform/platform_internal.h"

struct Renderer {
    WINDOW *screen;
    bool has_colors;
    RenderBackendKind backend_kind;
    AnsiRenderer *ansi;
};

struct RenderContext {
    Renderer *renderer;
    WINDOW *win;
    bool owns_window;
    int y;
    int x;
    int h;
    int w;
};

enum {
    DRAW_TEXT_CAP = 256,
};

typedef struct DrawCommand {
    DrawCommandType type;
    RenderStyle style;
    RenderStyle alt_style;
    int y;
    int x;
    int len;
    char ch;
    char text[DRAW_TEXT_CAP];
} DrawCommand;

struct DrawList {
    DrawCommand *commands;
    size_t count;
    size_t capacity;
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

static RenderStyle style_or_default(const RenderStyle *style) {
    RenderStyle out = {RENDER_COLOR_DEFAULT, RENDER_COLOR_DEFAULT, false, false};
    if (style) out = *style;
    return out;
}

static void draw_copy_text(char dst[DRAW_TEXT_CAP], const char *src) {
    if (!dst) return;
    dst[0] = '\0';
    if (!src) return;
    strncpy(dst, src, DRAW_TEXT_CAP - 1);
    dst[DRAW_TEXT_CAP - 1] = '\0';
}

static bool draw_list_reserve(DrawList *list, size_t want) {
    if (!list) return false;
    if (want <= list->capacity) return true;

    size_t next = list->capacity ? list->capacity * 2 : 16;
    while (next < want) next *= 2;

    DrawCommand *commands = realloc(list->commands, next * sizeof(*commands));
    if (!commands) return false;
    list->commands = commands;
    list->capacity = next;
    return true;
}

static DrawCommand *draw_list_push(DrawList *list, DrawCommandType type) {
    if (!list) return NULL;
    if (!draw_list_reserve(list, list->count + 1)) return NULL;
    DrawCommand *cmd = &list->commands[list->count++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = type;
    cmd->style = style_or_default(NULL);
    cmd->alt_style = style_or_default(NULL);
    return cmd;
}

static void renderer_query_ansi_size(int *rows, int *cols) {
    int r = 24;
    int c = 80;
#if !defined(_WIN32)
    struct winsize ws = {0};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
        r = ws.ws_row;
        c = ws.ws_col;
    }
#endif
    if (rows) *rows = r;
    if (cols) *cols = c;
}

static RenderBackendKind normalize_backend_kind(RenderBackendKind requested) {
    if (requested == RENDER_BACKEND_ANSI) return RENDER_BACKEND_ANSI;
    return RENDER_BACKEND_CURSES;
}

const char *renderer_backend_name(const Renderer *renderer) {
    if (!renderer) return "unknown";
    switch (renderer->backend_kind) {
    case RENDER_BACKEND_CURSES:
        return "curses";
    case RENDER_BACKEND_ANSI:
        return "ansi";
    case RENDER_BACKEND_AUTO:
    default:
        return "unknown";
    }
}

Renderer *renderer_create_with_backend(const PlatformBackend *backend,
                                       RenderBackendKind backend_kind) {
    Renderer *renderer = calloc(1, sizeof(*renderer));
    if (!renderer) return NULL;

    renderer->backend_kind = normalize_backend_kind(backend_kind);
    renderer->screen = NULL;
    renderer->has_colors = false;
    renderer->ansi = NULL;

    if (renderer->backend_kind == RENDER_BACKEND_CURSES) {
        renderer->screen = platform_native_stdscr(backend);
        if (!renderer->screen) {
            free(renderer);
            return NULL;
        }
        renderer->has_colors = platform_native_has_colors(backend);
    }
    if (renderer->backend_kind == RENDER_BACKEND_ANSI) {
        renderer->ansi = ansi_renderer_create();
        if (!renderer->ansi) {
            free(renderer);
            return NULL;
        }
    }

    if (renderer->backend_kind == RENDER_BACKEND_CURSES && renderer->has_colors) {
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

Renderer *renderer_create(const PlatformBackend *backend) {
    return renderer_create_with_backend(backend, RENDER_BACKEND_AUTO);
}

void renderer_destroy(Renderer *renderer) {
    if (!renderer) return;
    ansi_renderer_destroy(renderer->ansi);
    renderer->ansi = NULL;
    free(renderer);
}

void renderer_get_screen_size(Renderer *renderer, int *rows, int *cols) {
    if (!renderer) return;
    if (renderer->backend_kind == RENDER_BACKEND_ANSI) {
        renderer_query_ansi_size(rows, cols);
        return;
    }
    if (!renderer->screen) return;
    int r = 0;
    int c = 0;
    getmaxyx(renderer->screen, r, c);
    if (rows) *rows = r;
    if (cols) *cols = c;
}

void renderer_clear(Renderer *renderer) {
    if (!renderer) return;
    if (renderer->backend_kind == RENDER_BACKEND_ANSI) {
        int rows = 0;
        int cols = 0;
        renderer_query_ansi_size(&rows, &cols);
        ansi_renderer_begin_frame(renderer->ansi, rows, cols);
        return;
    }
    if (!renderer->screen) return;
    werase(renderer->screen);
    wnoutrefresh(renderer->screen);
}

void renderer_flush(Renderer *renderer) {
    if (!renderer) return;
    if (renderer->backend_kind == RENDER_BACKEND_ANSI) {
        ansi_renderer_flush(renderer->ansi, stdout);
        fflush(stdout);
        return;
    }
    doupdate();
}

static RenderContext *render_context_new(Renderer *renderer, WINDOW *win,
                                         bool owns_window, int y, int x, int h, int w) {
    RenderContext *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    ctx->renderer = renderer;
    ctx->win = win;
    ctx->owns_window = owns_window;
    ctx->y = y;
    ctx->x = x;
    ctx->h = h;
    ctx->w = w;
    return ctx;
}

RenderContext *renderer_begin_screen(Renderer *renderer) {
    int h = 0;
    int w = 0;
    if (!renderer) return NULL;
    if (renderer->backend_kind == RENDER_BACKEND_ANSI) {
        renderer_query_ansi_size(&h, &w);
        return render_context_new(renderer, NULL, false, 0, 0, h, w);
    }
    if (!renderer->screen) return NULL;
    getmaxyx(renderer->screen, h, w);
    return render_context_new(renderer, renderer->screen, false, 0, 0, h, w);
}

RenderContext *renderer_begin_region(Renderer *renderer, int h, int w, int y, int x) {
    if (!renderer || h <= 0 || w <= 0) return NULL;
    if (renderer->backend_kind == RENDER_BACKEND_ANSI) {
        return render_context_new(renderer, NULL, false, y, x, h, w);
    }
    WINDOW *win = newwin(h, w, y, x);
    if (!win) return NULL;
    return render_context_new(renderer, win, true, y, x, h, w);
}

void renderer_end(Renderer *renderer, RenderContext *ctx) {
    (void)renderer;
    if (!ctx) return;
    if (ctx->win) wnoutrefresh(ctx->win);
    if (ctx->owns_window && ctx->win) delwin(ctx->win);
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

DrawList *draw_list_create(void) {
    DrawList *list = calloc(1, sizeof(*list));
    return list;
}

void draw_list_destroy(DrawList *list) {
    if (!list) return;
    free(list->commands);
    free(list);
}

void draw_list_reset(DrawList *list) {
    if (!list) return;
    list->count = 0;
}

size_t draw_list_count(const DrawList *list) {
    if (!list) return 0;
    return list->count;
}

bool draw_list_get(const DrawList *list, size_t index, DrawCommandView *out) {
    if (!list || !out || index >= list->count) return false;
    const DrawCommand *cmd = &list->commands[index];
    out->type = cmd->type;
    out->style = cmd->style;
    out->alt_style = cmd->alt_style;
    out->y = cmd->y;
    out->x = cmd->x;
    out->len = cmd->len;
    out->ch = cmd->ch;
    out->text = cmd->text;
    return true;
}

bool draw_list_fill(DrawList *list, char ch, const RenderStyle *style) {
    DrawCommand *cmd = draw_list_push(list, DRAW_COMMAND_FILL);
    if (!cmd) return false;
    cmd->ch = ch;
    cmd->style = style_or_default(style);
    return true;
}

bool draw_list_box(DrawList *list, const char *title, const RenderStyle *frame_style,
                   const RenderStyle *title_style) {
    DrawCommand *cmd = draw_list_push(list, DRAW_COMMAND_BOX);
    if (!cmd) return false;
    cmd->style = style_or_default(frame_style);
    cmd->alt_style = style_or_default(title_style);
    draw_copy_text(cmd->text, title);
    return true;
}

bool draw_list_text(DrawList *list, int y, int x, const char *text,
                    const RenderStyle *style) {
    DrawCommand *cmd = draw_list_push(list, DRAW_COMMAND_TEXT);
    if (!cmd) return false;
    cmd->y = y;
    cmd->x = x;
    cmd->style = style_or_default(style);
    draw_copy_text(cmd->text, text);
    return true;
}

bool draw_list_hline(DrawList *list, int y, int x, int len, char ch,
                     const RenderStyle *style) {
    DrawCommand *cmd = draw_list_push(list, DRAW_COMMAND_HLINE);
    if (!cmd) return false;
    cmd->y = y;
    cmd->x = x;
    cmd->len = len;
    cmd->ch = ch;
    cmd->style = style_or_default(style);
    return true;
}

void renderer_draw_list(RenderContext *ctx, const DrawList *list) {
    if (!ctx || !list) return;
    if (ctx->renderer && ctx->renderer->backend_kind == RENDER_BACKEND_ANSI) {
        ansi_renderer_compose_draw_list(ctx->renderer->ansi, ctx->y, ctx->x, ctx->h,
                                        ctx->w, list);
        return;
    }

    for (size_t i = 0; i < list->count; ++i) {
        const DrawCommand *cmd = &list->commands[i];
        switch (cmd->type) {
        case DRAW_COMMAND_FILL:
            render_fill(ctx, cmd->ch, &cmd->style);
            break;
        case DRAW_COMMAND_BOX:
            render_draw_box(ctx, cmd->text, &cmd->style, &cmd->alt_style);
            break;
        case DRAW_COMMAND_TEXT:
            render_draw_text(ctx, cmd->y, cmd->x, cmd->text, &cmd->style);
            break;
        case DRAW_COMMAND_HLINE:
            render_draw_hline(ctx, cmd->y, cmd->x, cmd->len, cmd->ch, &cmd->style);
            break;
        default:
            break;
        }
    }
}
