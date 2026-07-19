#include "test_harness.h"
#include <stdbool.h>
#include <stddef.h>

#include "platform/platform.h"

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

    return 0;
}
