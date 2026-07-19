#include "test_harness.h"
#include <string.h>

#include "ui/theme.h"

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
    return 0;
}
