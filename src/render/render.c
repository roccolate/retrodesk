#include "render/render.h"

#include <stdint.h>
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
    bool uses_color_pair_cache;
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

typedef struct DrawCommand {
    DrawCommandKind kind;
    int y;
    int x;
    int h;
    int w;
    int value;
    RenderStyle style;
    char text[256];
} DrawCommand;

struct DrawList {
    DrawCommand *commands;
    size_t count;
    size_t capacity;
};

static short g_pair_fg[RENDER_COLOR_PAIR_LIMIT];
static short g_pair_bg[RENDER_COLOR_PAIR_LIMIT];
static int g_pair_count = 1;
static int g_curses_renderer_refs = 0;

static bool render_style_equal(RenderStyle a, RenderStyle b) {
    return a.fg == b.fg && a.bg == b.bg && a.attrs == b.attrs;
}

static bool render_command_equal(const DrawCommand *a, const DrawCommand *b) {
    if (!a || !b) return false;
    if (a->kind != b->kind || a->y != b->y || a->x != b->x ||
        a->h != b->h || a->w != b->w || a->value != b->value ||
        !render_style_equal(a->style, b->style)) {
        return false;
    }
    return strcmp(a->text, b->text) == 0;
}

static int render_color_pair(short fg, short bg) {
    if (g_pair_count <= 0 || g_pair_count >= RENDER_COLOR_PAIR_LIMIT) return 0;

    for (int i = 1; i < g_pair_count; ++i) {
        if (g_pair_fg[i] == fg && g_pair_bg[i] == bg) return i;
    }

    int id = g_pair_count++;
    if (init_pair((short)id, fg, bg) == ERR) {
        g_pair_count--;
        return 0;
    }
    g_pair_fg[id] = fg;
    g_pair_bg[id] = bg;
    return id;
}

static int render_curses_attrs(const Renderer *renderer, RenderStyle style) {
    int attrs = 0;
    if (renderer && renderer->has_colors) {
        int pair = render_color_pair(style.fg, style.bg);
        if (pair > 0) attrs |= COLOR_PAIR(pair);
    }
    if (style.attrs & RENDER_ATTR_BOLD) attrs |= A_BOLD;
    if (style.attrs & RENDER_ATTR_REVERSE) attrs |= A_REVERSE;
    if (style.attrs & RENDER_ATTR_UNDERLINE) attrs |= A_UNDERLINE;
    return attrs;
}

static int draw_list_reserve(DrawList *list, size_t additional) {
    if (!list) return 0;
    if (additional > SIZE_MAX - list->count) return 0;

    size_t required = list->count + additional;
    if (required <= list->capacity) return 1;
    if (required > SIZE_MAX / sizeof(*list->commands)) return 0;

    size_t next_capacity = list->capacity ? list->capacity : 32;
    while (next_capacity < required) {
        if (next_capacity > SIZE_MAX / 2) {
            next_capacity = required;
            break;
        }
        next_capacity *= 2;
    }
    if (next_capacity < required ||
        next_capacity > SIZE_MAX / sizeof(*list->commands)) {
        return 0;
    }

    DrawCommand *next = realloc(list->commands,
                                next_capacity * sizeof(*list->commands));
    if (!next) return 0;
    list->commands = next;
    list->capacity = next_capacity;
    return 1;
}

static void draw_list_push(DrawList *list, DrawCommand command) {
    if (!list || !draw_list_reserve(list, 1)) return;
    list->commands[list->count++] = command;
}

Renderer *renderer_create(PlatformBackend *platform) {
    return renderer_create_with_backend(platform, RENDER_BACKEND_CURSES);
}

Renderer *renderer_create_with_backend(PlatformBackend *platform,
                                       RenderBackendKind backend) {
    Renderer *renderer = calloc(1, sizeof(*renderer));
    if (!renderer) return NULL;

    renderer->backend_kind = backend;
    if (backend == RENDER_BACKEND_ANSI) {
        renderer->ansi = ansi_renderer_create(stdout);
        if (!renderer->ansi) {
            free(renderer);
            return NULL;
        }
        return renderer;
    }

    if (!platform) {
        free(renderer);
        return NULL;
    }
    renderer->screen = platform_native_stdscr(platform);
    renderer->has_colors = platform_native_has_colors(platform);
    renderer->uses_color_pair_cache = renderer->has_colors;
    if (renderer->uses_color_pair_cache) {
        if (g_curses_renderer_refs == 0) g_pair_count = 1;
        g_curses_renderer_refs++;
    }
    return renderer;
}

void renderer_destroy(Renderer *renderer) {
    if (!renderer) return;
    if (renderer->uses_color_pair_cache && g_curses_renderer_refs > 0) {
        g_curses_renderer_refs--;
        if (g_curses_renderer_refs == 0) g_pair_count = 1;
    }
    ansi_renderer_destroy(renderer->ansi);
    free(renderer);
}

void renderer_get_screen_size(const Renderer *renderer, int *rows, int *cols) {
    if (rows) *rows = 0;
    if (cols) *cols = 0;
    if (!renderer) return;

    if (renderer->backend_kind == RENDER_BACKEND_ANSI) {
        int r = 25;
        int c = 80;
#if !defined(_WIN32)
        struct winsize size;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0) {
            if (size.ws_row > 0) r = size.ws_row;
            if (size.ws_col > 0) c = size.ws_col;
        }
#endif
        if (rows) *rows = r;
        if (cols) *cols = c;
        return;
    }

    int r = 0;
    int c = 0;
    if (renderer->screen) getmaxyx(renderer->screen, r, c);
    if (rows) *rows = r;
    if (cols) *cols = c;
}

void renderer_begin_frame(Renderer *renderer) {
    if (!renderer) return;
    if (renderer->backend_kind == RENDER_BACKEND_ANSI) {
        ansi_renderer_begin_frame(renderer->ansi);
        return;
    }
    if (renderer->screen) werase(renderer->screen);
}

void renderer_end_frame(Renderer *renderer) {
    if (!renderer) return;
    if (renderer->backend_kind == RENDER_BACKEND_ANSI) {
        ansi_renderer_end_frame(renderer->ansi);
        return;
    }
    if (renderer->screen) wnoutrefresh(renderer->screen);
}

void renderer_flush(Renderer *renderer) {
    if (!renderer) return;
    if (renderer->backend_kind == RENDER_BACKEND_ANSI) {
        ansi_renderer_flush(renderer->ansi);
        return;
    }
    doupdate();
}

RenderContext *renderer_create_window(Renderer *renderer, int y, int x, int h,
                                      int w) {
    if (!renderer || h <= 0 || w <= 0) return NULL;
    RenderContext *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    ctx->renderer = renderer;
    ctx->y = y;
    ctx->x = x;
    ctx->h = h;
    ctx->w = w;

    if (renderer->backend_kind == RENDER_BACKEND_ANSI) return ctx;

    ctx->win = newwin(h, w, y, x);
    if (!ctx->win) {
        free(ctx);
        return NULL;
    }
    ctx->owns_window = true;
    return ctx;
}

void renderer_destroy_window(RenderContext *ctx) {
    if (!ctx) return;
    if (ctx->owns_window && ctx->win) delwin(ctx->win);
    free(ctx);
}

void renderer_move_resize_window(RenderContext *ctx, int y, int x, int h,
                                 int w) {
    if (!ctx || h <= 0 || w <= 0) return;
    ctx->y = y;
    ctx->x = x;
    ctx->h = h;
    ctx->w = w;
    if (!ctx->win) return;
    mvwin(ctx->win, y, x);
    wresize(ctx->win, h, w);
}

DrawList *draw_list_create(void) {
    return calloc(1, sizeof(DrawList));
}

void draw_list_destroy(DrawList *list) {
    if (!list) return;
    free(list->commands);
    free(list);
}

void draw_list_clear(DrawList *list) {
    if (list) list->count = 0;
}

size_t draw_list_count(const DrawList *list) {
    return list ? list->count : 0;
}

DrawCommandKind draw_list_command_kind(const DrawList *list, size_t index) {
    if (!list || index >= list->count) return DRAW_COMMAND_NONE;
    return list->commands[index].kind;
}

bool draw_list_equal(const DrawList *a, const DrawList *b) {
    if (a == b) return true;
    if (!a || !b || a->count != b->count) return false;
    for (size_t i = 0; i < a->count; ++i) {
        if (!render_command_equal(&a->commands[i], &b->commands[i])) return false;
    }
    return true;
}

void draw_list_text(DrawList *list, int y, int x, const char *text,
                    const RenderStyle *style) {
    if (!list || !text || !style) return;
    DrawCommand command = {
        .kind = DRAW_COMMAND_TEXT,
        .y = y,
        .x = x,
        .style = *style,
    };
    snprintf(command.text, sizeof(command.text), "%s", text);
    draw_list_push(list, command);
}

void draw_list_fill(DrawList *list, int y, int x, int h, int w, char ch,
                    const RenderStyle *style) {
    if (!list || !style || h <= 0 || w <= 0) return;
    DrawCommand command = {
        .kind = DRAW_COMMAND_FILL,
        .y = y,
        .x = x,
        .h = h,
        .w = w,
        .value = (unsigned char)ch,
        .style = *style,
    };
    draw_list_push(list, command);
}

void draw_list_frame(DrawList *list, int y, int x, int h, int w,
                     const RenderStyle *style) {
    if (!list || !style || h <= 1 || w <= 1) return;
    DrawCommand command = {
        .kind = DRAW_COMMAND_FRAME,
        .y = y,
        .x = x,
        .h = h,
        .w = w,
        .style = *style,
    };
    draw_list_push(list, command);
}

void draw_list_progress(DrawList *list, int y, int x, int width, int value,
                        const RenderStyle *style) {
    if (!list || !style || width <= 0) return;
    DrawCommand command = {
        .kind = DRAW_COMMAND_PROGRESS,
        .y = y,
        .x = x,
        .w = width,
        .value = value,
        .style = *style,
    };
    draw_list_push(list, command);
}

static void renderer_execute_curses(RenderContext *ctx, const DrawCommand *command) {
    if (!ctx || !ctx->win || !command) return;
    int attrs = render_curses_attrs(ctx->renderer, command->style);
    wattrset(ctx->win, attrs);

    switch (command->kind) {
    case DRAW_COMMAND_TEXT:
        mvwaddnstr(ctx->win, command->y, command->x, command->text,
                   (int)strlen(command->text));
        break;
    case DRAW_COMMAND_FILL:
        for (int row = 0; row < command->h; ++row) {
            for (int col = 0; col < command->w; ++col) {
                mvwaddch(ctx->win, command->y + row, command->x + col,
                         (chtype)command->value);
            }
        }
        break;
    case DRAW_COMMAND_FRAME:
        wborder(ctx->win, 0, 0, 0, 0, 0, 0, 0, 0);
        break;
    case DRAW_COMMAND_PROGRESS: {
        int value = command->value;
        if (value < 0) value = 0;
        if (value > 100) value = 100;
        int filled = (command->w * value) / 100;
        for (int col = 0; col < command->w; ++col) {
            mvwaddch(ctx->win, command->y, command->x + col,
                     col < filled ? '#' : '-');
        }
        break;
    }
    case DRAW_COMMAND_NONE:
    default:
        break;
    }
}

void renderer_execute(RenderContext *ctx, const DrawList *list) {
    if (!ctx || !list) return;
    if (ctx->renderer->backend_kind == RENDER_BACKEND_ANSI) {
        ansi_renderer_execute(ctx->renderer->ansi, ctx->y, ctx->x, ctx->h,
                              ctx->w, list);
        return;
    }

    if (!ctx->win) return;
    werase(ctx->win);
    for (size_t i = 0; i < list->count; ++i) {
        renderer_execute_curses(ctx, &list->commands[i]);
    }
    wnoutrefresh(ctx->win);
}

void renderer_draw_statusbar(Renderer *renderer, int row, const char *text,
                             const RenderStyle *style) {
    if (!renderer || !text || !style) return;
    if (renderer->backend_kind == RENDER_BACKEND_ANSI) {
        ansi_renderer_draw_statusbar(renderer->ansi, row, text, style);
        return;
    }
    if (!renderer->screen) return;

    int rows = 0;
    int cols = 0;
    getmaxyx(renderer->screen, rows, cols);
    if (row < 0 || row >= rows || cols <= 0) return;
    wattrset(renderer->screen, render_curses_attrs(renderer, *style));
    mvwhline(renderer->screen, row, 0, ' ', cols);
    mvwaddnstr(renderer->screen, row, 1, text, cols > 2 ? cols - 2 : 0);
    wnoutrefresh(renderer->screen);
}
