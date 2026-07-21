#include "render/render.h"

#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "core/utf8.h"
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

typedef struct ColorPairEntry {
    short fg;
    short bg;
    short pair;
} ColorPairEntry;

/* curses color pairs are global to the screen; one cache is enough across
   renderer instances. Defaults fall back to pair 0 (terminal default). */
enum { COLOR_PAIR_CACHE_CAP = 64 };
static ColorPairEntry g_color_pair_cache[COLOR_PAIR_CACHE_CAP];
static int g_color_pair_count = 0;

static short style_to_pair(Renderer *renderer, const RenderStyle *style) {
    (void)renderer;
    if (!style) return 0;
    if (style->fg == RENDER_COLOR_DEFAULT && style->bg == RENDER_COLOR_DEFAULT) {
        return 0;
    }
    /* RENDER_COLOR_DEFAULT maps to -1, which ncurses accepts after
       use_default_colors() has been called (see renderer_create_with_backend). */
    short fg = (short)style->fg;
    short bg = (short)style->bg;

    for (int i = 0; i < g_color_pair_count; ++i) {
        if (g_color_pair_cache[i].fg == fg && g_color_pair_cache[i].bg == bg) {
            return g_color_pair_cache[i].pair;
        }
    }

    if (g_color_pair_count >= COLOR_PAIR_CACHE_CAP) return 0;

    short pair = (short)(g_color_pair_count + 1);
    if (init_pair(pair, fg, bg) != OK) return 0;
    g_color_pair_cache[g_color_pair_count].fg = fg;
    g_color_pair_cache[g_color_pair_count].bg = bg;
    g_color_pair_cache[g_color_pair_count].pair = pair;
    g_color_pair_count++;
    return pair;
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
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, DRAW_TEXT_CAP, "%s", src);
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
        /* Allow -1 (RENDER_COLOR_DEFAULT) as fg/bg in init_pair, mapping
           to the terminal's native foreground/background color. */
        use_default_colors();
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
    /* Reset the global color pair cache so a new renderer starts fresh. */
    g_color_pair_count = 0;
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
    if (!ctx) return;
    if (!ctx->win) {
        /* ANSI backend: compose directly into the cell grid. */
        if (ctx->renderer && ctx->renderer->ansi) {
            DrawList *tmp = draw_list_create();
            if (!tmp) return;
            draw_list_fill(tmp, ch, style);
            ansi_renderer_compose_draw_list(ctx->renderer->ansi, ctx->y, ctx->x,
                                            ctx->h, ctx->w, tmp);
            draw_list_destroy(tmp);
        }
        return;
    }
    apply_style(ctx, style);
    for (int y = 0; y < ctx->h; ++y) {
        mvwhline(ctx->win, y, 0, ch, ctx->w);
    }
}

void render_draw_box(RenderContext *ctx, const char *title, const RenderStyle *frame_style,
                     const RenderStyle *title_style) {
    if (!ctx) return;
    if (!ctx->win) {
        if (ctx->renderer && ctx->renderer->ansi) {
            DrawList *tmp = draw_list_create();
            if (!tmp) return;
            draw_list_box(tmp, title, frame_style, title_style);
            ansi_renderer_compose_draw_list(ctx->renderer->ansi, ctx->y, ctx->x,
                                            ctx->h, ctx->w, tmp);
            draw_list_destroy(tmp);
        }
        return;
    }
    apply_style(ctx, frame_style);
    box(ctx->win, 0, 0);
    if (title && title[0]) {
        if (ctx->h <= 0 || ctx->w <= 0) return;
        int max_title = ctx->w - 4;
        if (max_title < 0) max_title = 0;
        apply_style(ctx, title_style);
        mvwaddch(ctx->win, 0, 2, ' ');
        if (max_title > 0) {
            waddnstr(ctx->win, title, max_title);
        }
        if (2 + max_title < ctx->w - 1) {
            waddch(ctx->win, ' ');
        }
    }
}

void render_draw_text(RenderContext *ctx, int y, int x, const char *text,
                      const RenderStyle *style) {
    if (!ctx || !text) return;
    if (!ctx->win) {
        if (ctx->renderer && ctx->renderer->ansi) {
            DrawList *tmp = draw_list_create();
            if (!tmp) return;
            draw_list_text(tmp, y, x, text, style);
            ansi_renderer_compose_draw_list(ctx->renderer->ansi, ctx->y, ctx->x,
                                            ctx->h, ctx->w, tmp);
            draw_list_destroy(tmp);
        }
        return;
    }
    if (y < 0 || y >= ctx->h || x < 0 || x >= ctx->w) return;
    int max_columns = ctx->w - x - 1;
    if (max_columns <= 0) return;

    size_t text_len = strlen(text);
    size_t byte_len =
        retro_utf8_prefix_bytes(text, text_len, (size_t)max_columns);
    char clipped[DRAW_TEXT_CAP];
    if (byte_len >= sizeof(clipped)) byte_len = sizeof(clipped) - 1;
    memcpy(clipped, text, byte_len);
    clipped[byte_len] = '\0';

    apply_style(ctx, style);
    wmove(ctx->win, y, x);
    waddstr(ctx->win, clipped);
}

void render_draw_hline(RenderContext *ctx, int y, int x, int len, char ch,
                       const RenderStyle *style) {
    if (!ctx || len <= 0) return;
    if (!ctx->win) {
        if (ctx->renderer && ctx->renderer->ansi) {
            DrawList *tmp = draw_list_create();
            if (!tmp) return;
            draw_list_hline(tmp, y, x, len, ch, style);
            ansi_renderer_compose_draw_list(ctx->renderer->ansi, ctx->y, ctx->x,
                                            ctx->h, ctx->w, tmp);
            draw_list_destroy(tmp);
        }
        return;
    }
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
