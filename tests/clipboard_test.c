#include "test_harness.h"

#include <string.h>

#include "core/clipboard.h"

int main(void) {
    retro_clipboard_clear();
    TEST_REQUIRE(!retro_clipboard_has_text());

    const char *utf8 = "niño\npingüino";
    TEST_REQUIRE(retro_clipboard_set_text(utf8, strlen(utf8)));
    TEST_REQUIRE(retro_clipboard_has_text());

    size_t length = 0;
    const char *stored = retro_clipboard_text(&length);
    TEST_REQUIRE(length == strlen(utf8));
    TEST_REQUIRE(strcmp(stored, utf8) == 0);

    const char invalid_utf8[] = {(char)0xc3, '\0'};
    TEST_REQUIRE(!retro_clipboard_set_text(invalid_utf8, 1));
    TEST_REQUIRE(strcmp(retro_clipboard_text(NULL), utf8) == 0);

    const char embedded_nul[] = {'a', '\0', 'b'};
    TEST_REQUIRE(!retro_clipboard_set_text(embedded_nul,
                                           sizeof(embedded_nul)));
    TEST_REQUIRE(strcmp(retro_clipboard_text(NULL), utf8) == 0);

    TEST_REQUIRE(retro_clipboard_set_text("", 0));
    TEST_REQUIRE(!retro_clipboard_has_text());
    TEST_REQUIRE(strcmp(retro_clipboard_text(&length), "") == 0);
    TEST_REQUIRE(length == 0);

    retro_clipboard_clear();
    TEST_REQUIRE(!retro_clipboard_has_text());
    return 0;
}
