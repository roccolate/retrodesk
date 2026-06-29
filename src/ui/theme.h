#ifndef RETRODESK_UI_THEME_H
#define RETRODESK_UI_THEME_H

#include <stdbool.h>

#include "render/render.h"

/* Theme tokens consumed by wm/ui. Runtime code must use these constants,
   not hardcoded palettes (see docs/FOUNDATION_PRINCIPLES.md). */

typedef enum RetroThemeKind {
    RETRO_THEME_XP = 0,
    RETRO_THEME_HACKER,
    RETRO_THEME_AMIGA,
    RETRO_THEME_WIN31,
} RetroThemeKind;

typedef struct RetroTheme {
    const char *name;
    RenderStyle window_frame_inactive;
    RenderStyle window_frame_active;
    RenderStyle window_frame_drag;
    RenderStyle window_title;
    RenderStyle window_body;
    RenderStyle statusbar;
    RenderStyle shell_text;
    RenderStyle shell_accent;
    RenderStyle launcher_text;
    RenderStyle launcher_selected;
} RetroTheme;

const RetroTheme *retro_theme_get(RetroThemeKind kind);
const char *retro_theme_name(RetroThemeKind kind);
bool retro_theme_parse(const char *name, RetroThemeKind *out_kind);

#endif
