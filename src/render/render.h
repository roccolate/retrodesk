#ifndef RETRODESK_RENDER_RENDER_H
#define RETRODESK_RENDER_RENDER_H

#include <stdbool.h>

#include "platform/platform.h"

typedef struct Renderer Renderer;
typedef struct RenderTarget RenderTarget;
typedef struct RenderContext RenderContext;

typedef enum RenderColor {
    RENDER_COLOR_DEFAULT = -1,
    RENDER_COLOR_BLACK = 0,
    RENDER_COLOR_RED = 1,
    RENDER_COLOR_GREEN = 2,
    RENDER_COLOR_YELLOW = 3,
    RENDER_COLOR_BLUE = 4,
    RENDER_COLOR_MAGENTA = 5,
    RENDER_COLOR_CYAN = 6,
    RENDER_COLOR_WHITE = 7,
} RenderColor;

typedef struct RenderStyle {
    RenderColor fg;
    RenderColor bg;
    bool reverse;
    bool bold;
} RenderStyle;

typedef struct RenderRect {
    int y;
    int x;
    int h;
    int w;
} RenderRect;

Renderer *renderer_create(const PlatformBackend *backend);
void renderer_destroy(Renderer *renderer);

void renderer_get_screen_size(Renderer *renderer, int *rows, int *cols);
void renderer_clear(Renderer *renderer);
void renderer_flush(Renderer *renderer);

RenderTarget *renderer_create_target(Renderer *renderer, int h, int w, int y, int x);
void renderer_destroy_target(RenderTarget *target);
void renderer_move_target(RenderTarget *target, int y, int x);
void renderer_resize_target(RenderTarget *target, int h, int w);
void renderer_get_target_geometry(const RenderTarget *target, int *y, int *x, int *h,
                                  int *w);

RenderContext *renderer_begin(Renderer *renderer, RenderTarget *target);
RenderContext *renderer_begin_screen(Renderer *renderer);
void renderer_end(Renderer *renderer, RenderContext *ctx);

void render_fill(RenderContext *ctx, char ch, const RenderStyle *style);
void render_draw_box(RenderContext *ctx, const char *title, const RenderStyle *frame_style,
                     const RenderStyle *title_style);
void render_draw_text(RenderContext *ctx, int y, int x, const char *text,
                      const RenderStyle *style);
void render_draw_hline(RenderContext *ctx, int y, int x, int len, char ch,
                       const RenderStyle *style);

#endif
