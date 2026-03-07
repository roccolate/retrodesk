#include "render/render.h"

#include <stdlib.h>

struct Renderer {
    int rows;
    int cols;
};

struct RenderContext {
    int unused;
};

struct DrawList {
    size_t count;
};

Renderer *renderer_create(const PlatformBackend *backend) {
    (void)backend;
    Renderer *renderer = calloc(1, sizeof(*renderer));
    if (!renderer) return NULL;
    renderer->rows = 40;
    renderer->cols = 120;
    return renderer;
}

Renderer *renderer_create_with_backend(const PlatformBackend *backend,
                                       RenderBackendKind backend_kind) {
    (void)backend_kind;
    return renderer_create(backend);
}

void renderer_destroy(Renderer *renderer) {
    free(renderer);
}

const char *renderer_backend_name(const Renderer *renderer) {
    (void)renderer;
    return "stub";
}

void renderer_get_screen_size(Renderer *renderer, int *rows, int *cols) {
    if (!renderer) return;
    if (rows) *rows = renderer->rows;
    if (cols) *cols = renderer->cols;
}

void renderer_clear(Renderer *renderer) {
    (void)renderer;
}

void renderer_flush(Renderer *renderer) {
    (void)renderer;
}

RenderContext *renderer_begin_screen(Renderer *renderer) {
    (void)renderer;
    return calloc(1, sizeof(RenderContext));
}

RenderContext *renderer_begin_region(Renderer *renderer, int h, int w, int y, int x) {
    (void)renderer;
    (void)h;
    (void)w;
    (void)y;
    (void)x;
    return calloc(1, sizeof(RenderContext));
}

void renderer_end(Renderer *renderer, RenderContext *ctx) {
    (void)renderer;
    free(ctx);
}

void render_fill(RenderContext *ctx, char ch, const RenderStyle *style) {
    (void)ctx;
    (void)ch;
    (void)style;
}

void render_draw_box(RenderContext *ctx, const char *title, const RenderStyle *frame_style,
                     const RenderStyle *title_style) {
    (void)ctx;
    (void)title;
    (void)frame_style;
    (void)title_style;
}

void render_draw_text(RenderContext *ctx, int y, int x, const char *text,
                      const RenderStyle *style) {
    (void)ctx;
    (void)y;
    (void)x;
    (void)text;
    (void)style;
}

void render_draw_hline(RenderContext *ctx, int y, int x, int len, char ch,
                       const RenderStyle *style) {
    (void)ctx;
    (void)y;
    (void)x;
    (void)len;
    (void)ch;
    (void)style;
}

DrawList *draw_list_create(void) {
    return calloc(1, sizeof(DrawList));
}

void draw_list_destroy(DrawList *list) {
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
    (void)list;
    (void)index;
    (void)out;
    return false;
}

static bool draw_list_push(DrawList *list) {
    if (!list) return false;
    list->count++;
    return true;
}

bool draw_list_fill(DrawList *list, char ch, const RenderStyle *style) {
    (void)ch;
    (void)style;
    return draw_list_push(list);
}

bool draw_list_box(DrawList *list, const char *title, const RenderStyle *frame_style,
                   const RenderStyle *title_style) {
    (void)title;
    (void)frame_style;
    (void)title_style;
    return draw_list_push(list);
}

bool draw_list_text(DrawList *list, int y, int x, const char *text,
                    const RenderStyle *style) {
    (void)y;
    (void)x;
    (void)text;
    (void)style;
    return draw_list_push(list);
}

bool draw_list_hline(DrawList *list, int y, int x, int len, char ch,
                     const RenderStyle *style) {
    (void)y;
    (void)x;
    (void)len;
    (void)ch;
    (void)style;
    return draw_list_push(list);
}

void renderer_draw_list(RenderContext *ctx, const DrawList *list) {
    (void)ctx;
    (void)list;
}
