#include "test_harness.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "platform/platform.h"
#include "storage/retro_fs.h"

#if defined(__DJGPP__)
static bool unsupported_list_callback(const RetroFsEntry *entry, void *userdata) {
    (void)entry;
    (void)userdata;
    return true;
}
#endif

int main(void) {
    PlatformFeatures f = {0};

    f.capability_mask = PLATFORM_CAP_KEYBOARD_BASIC | PLATFORM_CAP_MOUSE_BASIC |
                        PLATFORM_CAP_DRAG_RELIABLE;
    TEST_REQUIRE(platform_features_support(&f, PLATFORM_CAP_KEYBOARD_BASIC));
    TEST_REQUIRE(platform_features_support(
        &f, PLATFORM_CAP_KEYBOARD_BASIC | PLATFORM_CAP_MOUSE_BASIC));
    TEST_REQUIRE(!platform_features_support(&f, PLATFORM_CAP_RIGHT_CLICK));
    TEST_REQUIRE(!platform_features_support(
        &f, PLATFORM_CAP_KEYBOARD_BASIC | PLATFORM_CAP_RIGHT_CLICK));
    TEST_REQUIRE(!platform_features_support(NULL, PLATFORM_CAP_KEYBOARD_BASIC));

    TEST_REQUIRE(platform_input_backend_supported(INPUT_BACKEND_UNKNOWN));
    TEST_REQUIRE(platform_input_backend_supported(INPUT_BACKEND_NCURSES));
    TEST_REQUIRE(platform_input_backend_supported(INPUT_BACKEND_PDCURSES));
#if defined(_WIN32) || defined(__DJGPP__)
    TEST_REQUIRE(!platform_input_backend_supported(INPUT_BACKEND_TTY_RAW));
#else
    TEST_REQUIRE(platform_input_backend_supported(INPUT_BACKEND_TTY_RAW));
#endif

#if defined(__DJGPP__)
    RetroFsPath path = {0};
    TEST_REQUIRE(retro_fs_path_init(&path, ".") == RETRO_FS_OK);
    TEST_REQUIRE(strstr(retro_fs_path_cstr(&path), "storage unavailable") != NULL);
    TEST_REQUIRE(retro_fs_list(&path, 1, unsupported_list_callback, NULL) ==
                 RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_create_file(&path) == RETRO_FS_UNSUPPORTED);
    TEST_REQUIRE(retro_fs_write_atomic(&path, "", 0, NULL, NULL) ==
                 RETRO_FS_UNSUPPORTED);
    retro_fs_path_destroy(&path);
#endif

    return 0;
}
