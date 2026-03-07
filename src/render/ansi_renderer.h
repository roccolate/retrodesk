#ifndef RETRODESK_RENDER_ANSI_RENDERER_H
#define RETRODESK_RENDER_ANSI_RENDERER_H

#include <stdbool.h>
#include <stdio.h>

#include "render/render.h"

typedef struct AnsiRenderer AnsiRenderer;

AnsiRenderer *ansi_renderer_create(void);
void ansi_renderer_destroy(AnsiRenderer *renderer);
bool ansi_renderer_begin_frame(AnsiRenderer *renderer, int rows, int cols);
void ansi_renderer_compose_draw_list(AnsiRenderer *renderer, int origin_y, int origin_x,
                                     int h, int w, const DrawList *list);
void ansi_renderer_flush(AnsiRenderer *renderer, FILE *out);
void ansi_renderer_reset(FILE *out);

#endif
