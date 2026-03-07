#include "ui/theme.h"

#include <string.h>

static const RetroTheme k_theme_xp = {
    .name = "xp",
    .window_frame_inactive = {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, false},
    .window_frame_active = {RENDER_COLOR_YELLOW, RENDER_COLOR_BLUE, false, true},
    .window_frame_drag = {RENDER_COLOR_BLACK, RENDER_COLOR_YELLOW, false, true},
    .window_title = {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
    .window_body = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
    .statusbar = {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, false},
    .shell_text = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
    .shell_accent = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, true},
    .launcher_text = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
    .launcher_selected = {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, true},
};

static const RetroTheme k_theme_hacker = {
    .name = "hacker",
    .window_frame_inactive = {RENDER_COLOR_GREEN, RENDER_COLOR_BLACK, false, false},
    .window_frame_active = {RENDER_COLOR_GREEN, RENDER_COLOR_BLACK, false, true},
    .window_frame_drag = {RENDER_COLOR_BLACK, RENDER_COLOR_GREEN, false, true},
    .window_title = {RENDER_COLOR_GREEN, RENDER_COLOR_BLACK, false, true},
    .window_body = {RENDER_COLOR_GREEN, RENDER_COLOR_BLACK, false, false},
    .statusbar = {RENDER_COLOR_BLACK, RENDER_COLOR_GREEN, false, false},
    .shell_text = {RENDER_COLOR_GREEN, RENDER_COLOR_BLACK, false, false},
    .shell_accent = {RENDER_COLOR_GREEN, RENDER_COLOR_BLACK, false, true},
    .launcher_text = {RENDER_COLOR_GREEN, RENDER_COLOR_BLACK, false, false},
    .launcher_selected = {RENDER_COLOR_BLACK, RENDER_COLOR_GREEN, false, true},
};

static const RetroTheme k_theme_amiga = {
    .name = "amiga",
    .window_frame_inactive = {RENDER_COLOR_WHITE, RENDER_COLOR_CYAN, false, false},
    .window_frame_active = {RENDER_COLOR_YELLOW, RENDER_COLOR_CYAN, false, true},
    .window_frame_drag = {RENDER_COLOR_BLACK, RENDER_COLOR_YELLOW, false, true},
    .window_title = {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, true},
    .window_body = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
    .statusbar = {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
    .shell_text = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
    .shell_accent = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, true},
    .launcher_text = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
    .launcher_selected = {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, true},
};

static const RetroTheme k_theme_win31 = {
    .name = "win31",
    .window_frame_inactive = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
    .window_frame_active = {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
    .window_frame_drag = {RENDER_COLOR_BLACK, RENDER_COLOR_YELLOW, false, true},
    .window_title = {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
    .window_body = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
    .statusbar = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
    .shell_text = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
    .shell_accent = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, true},
    .launcher_text = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
    .launcher_selected = {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
};

const RetroTheme *retro_theme_get(RetroThemeKind kind) {
    switch (kind) {
    case RETRO_THEME_HACKER:
        return &k_theme_hacker;
    case RETRO_THEME_AMIGA:
        return &k_theme_amiga;
    case RETRO_THEME_WIN31:
        return &k_theme_win31;
    case RETRO_THEME_XP:
    default:
        return &k_theme_xp;
    }
}

const char *retro_theme_name(RetroThemeKind kind) {
    return retro_theme_get(kind)->name;
}

bool retro_theme_parse(const char *name, RetroThemeKind *out_kind) {
    if (!name || !out_kind) return false;
    if (strcmp(name, "xp") == 0) {
        *out_kind = RETRO_THEME_XP;
        return true;
    }
    if (strcmp(name, "hacker") == 0) {
        *out_kind = RETRO_THEME_HACKER;
        return true;
    }
    if (strcmp(name, "amiga") == 0) {
        *out_kind = RETRO_THEME_AMIGA;
        return true;
    }
    if (strcmp(name, "win31") == 0 || strcmp(name, "3.1") == 0) {
        *out_kind = RETRO_THEME_WIN31;
        return true;
    }
    return false;
}
