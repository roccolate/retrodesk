#include "test_harness.h"

#include <string.h>

#include "core/utf8.h"

int main(void) {
    const char *word = "canción";
    size_t len = strlen(word);
    TEST_REQUIRE(retro_utf8_validate(word, len));
    TEST_REQUIRE(retro_utf8_columns(word, len, 0, len) == 7);

    uint32_t cp = 0;
    size_t bytes = 0;
    TEST_REQUIRE(retro_utf8_decode("ñ", strlen("ñ"), 0, &cp, &bytes));
    TEST_REQUIRE(cp == 0x00F1u);
    TEST_REQUIRE(bytes == 2);

    char encoded[4] = {0};
    size_t encoded_len = 0;
    TEST_REQUIRE(retro_utf8_encode(0x00E1u, encoded, &encoded_len));
    TEST_REQUIRE(encoded_len == 2);
    TEST_REQUIRE(memcmp(encoded, "á", 2) == 0);

    TEST_REQUIRE(retro_utf8_prev("año", strlen("año")) == 3);
    TEST_REQUIRE(retro_utf8_next("año", strlen("año"), 1) == 3);
    TEST_REQUIRE(retro_utf8_prefix_bytes("niño", strlen("niño"), 3) == 4);

    const char invalid[] = {(char)0xC3, 'x'};
    TEST_REQUIRE(!retro_utf8_validate(invalid, sizeof(invalid)));
    return 0;
}
