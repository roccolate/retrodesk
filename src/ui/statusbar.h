#ifndef RETRODESK_UI_STATUSBAR_H
#define RETRODESK_UI_STATUSBAR_H

#include "render/render.h"

typedef struct StatusBar StatusBar;

StatusBar *statusbar_create(void);
void statusbar_set_text(StatusBar *sb, const char *fmt, ...);
void statusbar_render(StatusBar *sb, DrawList *draw_list, int screen_rows, int screen_cols,
                      const RenderStyle *style);
void statusbar_destroy(StatusBar *sb);

#endif
