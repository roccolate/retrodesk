#include "test_harness.h"

#include <string.h>

#include "core/clipboard.h"

int main(void) {
    RetroClipboard *first = retro_clipboard_create();
    RetroClipboard *second = retro_clipboard_create();
    TEST_REQUIRE(first != NULL);
    TEST_REQUIRE(second != NULL);
    TEST_REQUIRE(!retro_clipboard_has_text(first));
    TEST_REQUIRE(!retro_clipboard_has_text(second));

    const char *utf8 = "niño\npingüino";
    TEST_REQUIRE(retro_clipboard_set_text(first, utf8, strlen(utf8)));
    TEST_REQUIRE(retro_clipboard_has_text(first));
    TEST_REQUIRE(!retro_clipboard_has_text(second));

    size_t length = 0;
    const char *stored = retro_clipboard_text(first, &length);
    TEST_REQUIRE(length == strlen(utf8));
    TEST_REQUIRE(strcmp(stored, utf8) == 0);
    TEST_REQUIRE(strcmp(retro_clipboard_text(second, NULL), "") == 0);

    const char invalid_utf8[] = {(char)0xc3, '\0'};
    TEST_REQUIRE(!retro_clipboard_set_text(first, invalid_utf8, 1));
    TEST_REQUIRE(strcmp(retro_clipboard_text(first, NULL), utf8) == 0);

    const char embedded_nul[] = {'a', '\0', 'b'};
    TEST_REQUIRE(!retro_clipboard_set_text(first, embedded_nul,
                                            sizeof(embedded_nul)));
    TEST_REQUIRE(strcmp(retro_clipboard_text(first, NULL), utf8) == 0);

    TEST_REQUIRE(retro_clipboard_set_text(first, "", 0));
    TEST_REQUIRE(!retro_clipboard_has_text(first));
    TEST_REQUIRE(strcmp(retro_clipboard_text(first, &length), "") == 0);
    TEST_REQUIRE(length == 0);

    TEST_REQUIRE(!retro_clipboard_set_text(NULL, "x", 1));
    TEST_REQUIRE(!retro_clipboard_has_text(NULL));
    TEST_REQUIRE(strcmp(retro_clipboard_text(NULL, &length), "") == 0);
    TEST_REQUIRE(length == 0);
    retro_clipboard_clear(NULL);
    retro_clipboard_destroy(NULL);

    retro_clipboard_destroy(first);
    retro_clipboard_destroy(second);
    return 0;
}
