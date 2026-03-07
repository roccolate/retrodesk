#ifndef RETRODESK_PLATFORM_PLATFORM_H
#define RETRODESK_PLATFORM_PLATFORM_H

#include <stdbool.h>

#include "core/event.h"

typedef enum InputBackendKind {
    INPUT_BACKEND_UNKNOWN = 0,
    INPUT_BACKEND_NCURSES,
    INPUT_BACKEND_PDCURSES,
    INPUT_BACKEND_TTY_RAW,
} InputBackendKind;

enum {
    PLATFORM_CAP_KEYBOARD_BASIC = 1u << 0,
    PLATFORM_CAP_MOUSE_BASIC = 1u << 1,
    PLATFORM_CAP_DRAG_RELIABLE = 1u << 2,
    PLATFORM_CAP_RESIZE = 1u << 3,
    PLATFORM_CAP_COLOR = 1u << 4,
    PLATFORM_CAP_UNICODE_BASIC = 1u << 5,
    PLATFORM_CAP_DOUBLE_CLICK = 1u << 6,
    PLATFORM_CAP_RIGHT_CLICK = 1u << 7,
};

typedef struct PlatformFeatures {
    unsigned int capability_mask;
    InputBackendKind input_backend;
    bool keyboard_basic;
    bool mouse_basic;
    bool drag_reliable;
    bool resize_events;
    bool color;
    bool unicode_basic;
    bool double_click;
    bool right_click;
    bool linux_tty_keyboard_only;
} PlatformFeatures;

typedef struct PlatformConfig {
    bool bench_mode;
    int input_timeout_ms;
    InputBackendKind requested_input_backend;
} PlatformConfig;

typedef struct PlatformBackend PlatformBackend;

PlatformBackend *platform_create(const PlatformConfig *config);
bool platform_poll_event(PlatformBackend *platform, RetroEvent *out_event,
                         int timeout_ms);
const PlatformFeatures *platform_features(const PlatformBackend *platform);
const char *platform_backend_name(const PlatformBackend *platform);
void platform_destroy(PlatformBackend *platform);

static inline bool platform_features_support(const PlatformFeatures *features,
                                             unsigned int capability_mask) {
    return features &&
           ((features->capability_mask & capability_mask) == capability_mask);
}

#endif
