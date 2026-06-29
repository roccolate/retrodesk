#ifndef RETRODESK_RENDER_RENDER_H
#define RETRODESK_RENDER_RENDER_H

#include <stdbool.h>
#include <stddef.h>

#include "platform/platform.h"

/* Render contract: `wm`/`app`/`ui` produce DrawLists; only this module
   executes backend draw calls (curses or ANSI). Per-frame state is owned
   by the Renderer; callers must not touch WINDOW* or FILE* directly. */

typedef struct Renderer Renderer;
typedef struct RenderContext RenderContext;
typedef struct DrawList DrawList;

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

typedef enum DrawCommandType {
    DRAW_COMMAND_FILL = 0,
    DRAW_COMMAND_BOX,
    DRAW_COMMAND_TEXT,
    DRAW_COMMAND_HLINE,
} DrawCommandType;

typedef struct DrawCommandView {
    DrawCommandType type;
    RenderStyle style;
    RenderStyle alt_style;
    int y;
    int x;
    int len;
    char ch;
    const char *text;
} DrawCommandView;

typedef enum RenderBackendKind {
    RENDER_BACKEND_AUTO = 0,
    RENDER_BACKEND_CURSES,
    RENDER_BACKEND_ANSI,
} RenderBackendKind;

Renderer *renderer_create(const PlatformBackend *backend);
Renderer *renderer_create_with_backend(const PlatformBackend *backend,
                                       RenderBackendKind backend_kind);
void renderer_destroy(Renderer *renderer);
const char *renderer_backend_name(const Renderer *renderer);

void renderer_get_screen_size(Renderer *renderer, int *rows, int *cols);
void renderer_clear(Renderer *renderer);
void renderer_flush(Renderer *renderer);

RenderContext *renderer_begin_screen(Renderer *renderer);
RenderContext *renderer_begin_region(Renderer *renderer, int h, int w, int y, int x);
void renderer_end(Renderer *renderer, RenderContext *ctx);

void render_fill(RenderContext *ctx, char ch, const RenderStyle *style);
void render_draw_box(RenderContext *ctx, const char *title, const RenderStyle *frame_style,
                     const RenderStyle *title_style);
void render_draw_text(RenderContext *ctx, int y, int x, const char *text,
                      const RenderStyle *style);
void render_draw_hline(RenderContext *ctx, int y, int x, int len, char ch,
                       const RenderStyle *style);

DrawList *draw_list_create(void);
void draw_list_destroy(DrawList *list);
void draw_list_reset(DrawList *list);
size_t draw_list_count(const DrawList *list);
bool draw_list_get(const DrawList *list, size_t index, DrawCommandView *out);
bool draw_list_fill(DrawList *list, char ch, const RenderStyle *style);
bool draw_list_box(DrawList *list, const char *title, const RenderStyle *frame_style,
                   const RenderStyle *title_style);
bool draw_list_text(DrawList *list, int y, int x, const char *text,
                    const RenderStyle *style);
bool draw_list_hline(DrawList *list, int y, int x, int len, char ch,
                     const RenderStyle *style);
void renderer_draw_list(RenderContext *ctx, const DrawList *list);

#endif
