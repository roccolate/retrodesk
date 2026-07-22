#ifndef RETRODESK_UI_THEME_SURFACE_H
#define RETRODESK_UI_THEME_SURFACE_H

#include <stdbool.h>

#include "ui/theme.h"

/*
 * Desktop-surface tokens complement the core window theme without forcing
 * taskbar and Launcher widgets to synthesize visual states with ad-hoc
 * reverse/bold mutations. The match fields mirror the authoritative core
 * theme chrome and let translation-unit-private UI adapters recover the
 * selected theme from the styles already emitted by the Window Manager.
 */
typedef struct RetroSurfaceTheme {
    const char *name;

    RenderStyle match_statusbar;
    RenderStyle match_window_frame_active;
    RenderStyle match_window_title;
    RenderStyle match_window_body;

    RenderStyle taskbar_base;
    RenderStyle taskbar_menu;
    RenderStyle taskbar_menu_open;
    RenderStyle taskbar_app_idle;
    RenderStyle taskbar_app_running;
    RenderStyle taskbar_app_focused;
    RenderStyle taskbar_clock;

    RenderStyle launcher_header;
    RenderStyle launcher_section;
    RenderStyle launcher_item;
    RenderStyle launcher_selected;
    RenderStyle launcher_separator;
    RenderStyle launcher_footer;
} RetroSurfaceTheme;

static inline bool retro_surface_style_equal(const RenderStyle *a,
                                             const RenderStyle *b) {
    return a && b && a->fg == b->fg && a->bg == b->bg &&
           a->reverse == b->reverse && a->bold == b->bold;
}

static inline const RetroSurfaceTheme *retro_surface_theme_get(
    RetroThemeKind kind) {
    static const RetroSurfaceTheme xp = {
        .name = "xp",
        .match_statusbar = {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, false},
        .match_window_frame_active =
            {RENDER_COLOR_YELLOW, RENDER_COLOR_BLUE, false, true},
        .match_window_title =
            {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
        .match_window_body =
            {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
        .taskbar_base =
            {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, false},
        .taskbar_menu =
            {RENDER_COLOR_BLACK, RENDER_COLOR_GREEN, false, true},
        .taskbar_menu_open =
            {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
        .taskbar_app_idle =
            {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, false},
        .taskbar_app_running =
            {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, true},
        .taskbar_app_focused =
            {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
        .taskbar_clock =
            {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, true, true},
        .launcher_header =
            {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
        .launcher_section =
            {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, true},
        .launcher_item =
            {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
        .launcher_selected =
            {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
        .launcher_separator =
            {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, false},
        .launcher_footer =
            {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, false},
    };
    static const RetroSurfaceTheme hacker = {
        .name = "hacker",
        .match_statusbar = {RENDER_COLOR_BLACK, RENDER_COLOR_GREEN, false, false},
        .match_window_frame_active =
            {RENDER_COLOR_GREEN, RENDER_COLOR_BLACK, false, true},
        .match_window_title =
            {RENDER_COLOR_GREEN, RENDER_COLOR_BLACK, false, true},
        .match_window_body =
            {RENDER_COLOR_GREEN, RENDER_COLOR_BLACK, false, false},
        .taskbar_base =
            {RENDER_COLOR_BLACK, RENDER_COLOR_GREEN, false, false},
        .taskbar_menu =
            {RENDER_COLOR_GREEN, RENDER_COLOR_BLACK, false, true},
        .taskbar_menu_open =
            {RENDER_COLOR_BLACK, RENDER_COLOR_GREEN, false, true},
        .taskbar_app_idle =
            {RENDER_COLOR_BLACK, RENDER_COLOR_GREEN, false, false},
        .taskbar_app_running =
            {RENDER_COLOR_GREEN, RENDER_COLOR_BLACK, false, true},
        .taskbar_app_focused =
            {RENDER_COLOR_BLACK, RENDER_COLOR_GREEN, false, true},
        .taskbar_clock =
            {RENDER_COLOR_GREEN, RENDER_COLOR_BLACK, false, true},
        .launcher_header =
            {RENDER_COLOR_BLACK, RENDER_COLOR_GREEN, false, true},
        .launcher_section =
            {RENDER_COLOR_GREEN, RENDER_COLOR_BLACK, false, true},
        .launcher_item =
            {RENDER_COLOR_GREEN, RENDER_COLOR_BLACK, false, false},
        .launcher_selected =
            {RENDER_COLOR_BLACK, RENDER_COLOR_GREEN, false, true},
        .launcher_separator =
            {RENDER_COLOR_GREEN, RENDER_COLOR_BLACK, false, false},
        .launcher_footer =
            {RENDER_COLOR_GREEN, RENDER_COLOR_BLACK, false, false},
    };
    static const RetroSurfaceTheme amiga = {
        .name = "amiga",
        .match_statusbar = {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
        .match_window_frame_active =
            {RENDER_COLOR_YELLOW, RENDER_COLOR_CYAN, false, true},
        .match_window_title =
            {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, true},
        .match_window_body =
            {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
        .taskbar_base =
            {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
        .taskbar_menu =
            {RENDER_COLOR_BLACK, RENDER_COLOR_YELLOW, false, true},
        .taskbar_menu_open =
            {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, true},
        .taskbar_app_idle =
            {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, false},
        .taskbar_app_running =
            {RENDER_COLOR_YELLOW, RENDER_COLOR_BLUE, false, true},
        .taskbar_app_focused =
            {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, true},
        .taskbar_clock =
            {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, true},
        .launcher_header =
            {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, true},
        .launcher_section =
            {RENDER_COLOR_YELLOW, RENDER_COLOR_BLUE, false, true},
        .launcher_item =
            {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
        .launcher_selected =
            {RENDER_COLOR_BLACK, RENDER_COLOR_CYAN, false, true},
        .launcher_separator =
            {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
        .launcher_footer =
            {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, false},
    };
    static const RetroSurfaceTheme win31 = {
        .name = "win31",
        .match_statusbar = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
        .match_window_frame_active =
            {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
        .match_window_title =
            {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
        .match_window_body =
            {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
        .taskbar_base =
            {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
        .taskbar_menu =
            {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, true},
        .taskbar_menu_open =
            {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
        .taskbar_app_idle =
            {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
        .taskbar_app_running =
            {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, true},
        .taskbar_app_focused =
            {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
        .taskbar_clock =
            {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, true, true},
        .launcher_header =
            {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
        .launcher_section =
            {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, true},
        .launcher_item =
            {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
        .launcher_selected =
            {RENDER_COLOR_WHITE, RENDER_COLOR_BLUE, false, true},
        .launcher_separator =
            {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
        .launcher_footer =
            {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false},
    };

    switch (kind) {
    case RETRO_THEME_HACKER:
        return &hacker;
    case RETRO_THEME_AMIGA:
        return &amiga;
    case RETRO_THEME_WIN31:
        return &win31;
    case RETRO_THEME_XP:
    default:
        return &xp;
    }
}

static inline const RetroSurfaceTheme *retro_surface_theme_match_statusbar(
    const RenderStyle *statusbar) {
    for (int i = (int)RETRO_THEME_XP; i <= (int)RETRO_THEME_WIN31; ++i) {
        const RetroSurfaceTheme *theme =
            retro_surface_theme_get((RetroThemeKind)i);
        if (retro_surface_style_equal(statusbar, &theme->match_statusbar)) {
            return theme;
        }
    }
    return NULL;
}

static inline const RetroSurfaceTheme *retro_surface_theme_match_window_chrome(
    const RenderStyle *frame, const RenderStyle *title,
    const RenderStyle *body) {
    for (int i = (int)RETRO_THEME_XP; i <= (int)RETRO_THEME_WIN31; ++i) {
        const RetroSurfaceTheme *theme =
            retro_surface_theme_get((RetroThemeKind)i);
        if (retro_surface_style_equal(frame,
                                      &theme->match_window_frame_active) &&
            retro_surface_style_equal(title, &theme->match_window_title) &&
            retro_surface_style_equal(body, &theme->match_window_body)) {
            return theme;
        }
    }
    return NULL;
}

#endif
