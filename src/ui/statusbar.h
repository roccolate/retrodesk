#ifndef RETRODESK_UI_STATUSBAR_H
#define RETRODESK_UI_STATUSBAR_H

#include <stdbool.h>
#include <stddef.h>

#include "render/render.h"

#define STATUSBAR_MAX_APPS 8

typedef struct StatusBarAppSnapshot {
    const char *app_id;
    const char *label;
    size_t instance_count;
    bool focused;
} StatusBarAppSnapshot;

typedef struct StatusBarSnapshot {
    bool menu_open;
    char clock_text[9];
    size_t app_count;
    StatusBarAppSnapshot apps[STATUSBAR_MAX_APPS];
} StatusBarSnapshot;

typedef enum StatusBarActionKind {
    STATUSBAR_ACTION_NONE = 0,
    STATUSBAR_ACTION_TOGGLE_MENU,
    STATUSBAR_ACTION_ACTIVATE_APP,
} StatusBarActionKind;

typedef struct StatusBarAction {
    StatusBarActionKind kind;
    size_t app_index;
} StatusBarAction;

typedef struct StatusBar StatusBar;

StatusBar *statusbar_create(void);
void statusbar_set_text(StatusBar *sb, const char *fmt, ...);
void statusbar_set_snapshot(StatusBar *sb, const StatusBarSnapshot *snapshot);
void statusbar_render(StatusBar *sb, DrawList *draw_list, int screen_rows,
                      int screen_cols, const RenderStyle *style);
StatusBarAction statusbar_hit_test(const StatusBar *sb, int y, int x);
void statusbar_destroy(StatusBar *sb);

/* desktop.c includes this header after core/desktop.h. Keep the temporary
   launcher adaptation private to that one translation unit so widget tests and
   other UI consumers retain the ordinary Window Manager link contract. */
#if defined(RETRODESK_CORE_DESKTOP_H)
#include "ui/launcher_bridge.h"
#define wm_create_window desktop_chrome_create_window
#endif

#endif
