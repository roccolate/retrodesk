#include <assert.h>
#include <string.h>

#include "ui/theme.h"

int main(void) {
    const RetroTheme *xp = retro_theme_get(RETRO_THEME_XP);
    const RetroTheme *hacker = retro_theme_get(RETRO_THEME_HACKER);
    const RetroTheme *amiga = retro_theme_get(RETRO_THEME_AMIGA);
    const RetroTheme *win31 = retro_theme_get(RETRO_THEME_WIN31);

    assert(xp && hacker && amiga && win31);
    assert(strcmp(xp->name, "xp") == 0);
    assert(strcmp(hacker->name, "hacker") == 0);
    assert(strcmp(amiga->name, "amiga") == 0);
    assert(strcmp(win31->name, "win31") == 0);

    RetroThemeKind parsed = RETRO_THEME_XP;
    assert(retro_theme_parse("xp", &parsed) && parsed == RETRO_THEME_XP);
    assert(retro_theme_parse("hacker", &parsed) && parsed == RETRO_THEME_HACKER);
    assert(retro_theme_parse("amiga", &parsed) && parsed == RETRO_THEME_AMIGA);
    assert(retro_theme_parse("win31", &parsed) && parsed == RETRO_THEME_WIN31);
    assert(retro_theme_parse("3.1", &parsed) && parsed == RETRO_THEME_WIN31);
    assert(!retro_theme_parse("unknown", &parsed));

    assert(strcmp(retro_theme_name(RETRO_THEME_XP), "xp") == 0);
    return 0;
}
