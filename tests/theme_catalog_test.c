#include "test_harness.h"

#include <string.h>

#include "ui/theme.h"
#include "ui/theme_surface.h"

static bool style_equal(RenderStyle a, RenderStyle b) {
    return a.fg == b.fg && a.bg == b.bg &&
           a.reverse == b.reverse && a.bold == b.bold;
}

int main(void) {
    const RetroTheme *xp = retro_theme_get(RETRO_THEME_XP);
    const RetroTheme *hacker = retro_theme_get(RETRO_THEME_HACKER);
    const RetroTheme *amiga = retro_theme_get(RETRO_THEME_AMIGA);
    const RetroTheme *win31 = retro_theme_get(RETRO_THEME_WIN31);

    TEST_REQUIRE(xp && hacker && amiga && win31);
    TEST_REQUIRE(strcmp(xp->name, "xp") == 0);
    TEST_REQUIRE(strcmp(hacker->name, "hacker") == 0);
    TEST_REQUIRE(strcmp(amiga->name, "amiga") == 0);
    TEST_REQUIRE(strcmp(win31->name, "win31") == 0);

    RetroThemeKind parsed = RETRO_THEME_XP;
    TEST_REQUIRE(retro_theme_parse("xp", &parsed) && parsed == RETRO_THEME_XP);
    TEST_REQUIRE(retro_theme_parse("hacker", &parsed) && parsed == RETRO_THEME_HACKER);
    TEST_REQUIRE(retro_theme_parse("amiga", &parsed) && parsed == RETRO_THEME_AMIGA);
    TEST_REQUIRE(retro_theme_parse("win31", &parsed) && parsed == RETRO_THEME_WIN31);
    TEST_REQUIRE(retro_theme_parse("3.1", &parsed) && parsed == RETRO_THEME_WIN31);
    TEST_REQUIRE(!retro_theme_parse("unknown", &parsed));

    TEST_REQUIRE(strcmp(retro_theme_name(RETRO_THEME_XP), "xp") == 0);

    for (int i = (int)RETRO_THEME_XP; i <= (int)RETRO_THEME_WIN31; ++i) {
        RetroThemeKind kind = (RetroThemeKind)i;
        const RetroTheme *core = retro_theme_get(kind);
        const RetroSurfaceTheme *surface = retro_surface_theme_get(kind);
        TEST_REQUIRE(surface != NULL);
        TEST_REQUIRE(strcmp(core->name, surface->name) == 0);
        TEST_REQUIRE(style_equal(core->statusbar, surface->match_statusbar));
        TEST_REQUIRE(style_equal(core->window_frame_active,
                                 surface->match_window_frame_active));
        TEST_REQUIRE(style_equal(core->window_title,
                                 surface->match_window_title));
        TEST_REQUIRE(style_equal(core->window_body,
                                 surface->match_window_body));
        TEST_REQUIRE(retro_surface_theme_match_statusbar(
                         &core->statusbar) == surface);
        TEST_REQUIRE(retro_surface_theme_match_window_chrome(
                         &core->window_frame_active,
                         &core->window_title,
                         &core->window_body) == surface);
        TEST_REQUIRE(!style_equal(surface->taskbar_menu,
                                  surface->taskbar_app_idle));
        TEST_REQUIRE(!style_equal(surface->launcher_header,
                                  surface->launcher_item));
        TEST_REQUIRE(!style_equal(surface->launcher_selected,
                                  surface->launcher_item));
    }

    return 0;
}
