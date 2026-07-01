#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/desktop.h"
#include "platform/platform.h"

#ifndef RETROCORE_SPEC_DIR
#error "RETROCORE_SPEC_DIR must point at a retrocore-spec checkout"
#endif

#define STR2(x) #x
#define STR(x) STR2(x)

struct PlatformBackend {
    PlatformFeatures features;
    const char *name;
};

static void platform_stub_update_mask(PlatformFeatures *features) {
    if (!features) return;
    unsigned int mask = 0;
    if (features->keyboard_basic) mask |= PLATFORM_CAP_KEYBOARD_BASIC;
    if (features->mouse_basic) mask |= PLATFORM_CAP_MOUSE_BASIC;
    if (features->drag_reliable) mask |= PLATFORM_CAP_DRAG_RELIABLE;
    if (features->resize_events) mask |= PLATFORM_CAP_RESIZE;
    if (features->color) mask |= PLATFORM_CAP_COLOR;
    if (features->unicode_basic) mask |= PLATFORM_CAP_UNICODE_BASIC;
    if (features->double_click) mask |= PLATFORM_CAP_DOUBLE_CLICK;
    if (features->right_click) mask |= PLATFORM_CAP_RIGHT_CLICK;
    features->capability_mask = mask;
}

static PlatformBackend *platform_stub_new(void) {
    PlatformBackend *platform = calloc(1, sizeof(*platform));
    if (!platform) return NULL;
    platform->name = "retrocore-fixture-stub";
    platform->features.input_backend = INPUT_BACKEND_NCURSES;
    platform->features.keyboard_basic = true;
    platform->features.mouse_basic = true;
    platform->features.drag_reliable = true;
    platform->features.resize_events = false;
    platform->features.color = true;
    platform->features.unicode_basic = true;
    platform->features.double_click = false;
    platform->features.right_click = true;
    platform->features.linux_tty_keyboard_only = false;
    platform_stub_update_mask(&platform->features);
    return platform;
}

bool platform_poll_event(PlatformBackend *platform, RetroEvent *out_event, int timeout_ms) {
    (void)platform;
    (void)out_event;
    (void)timeout_ms;
    return false;
}

const PlatformFeatures *platform_features(const PlatformBackend *platform) {
    if (!platform) return NULL;
    return &platform->features;
}

const char *platform_backend_name(const PlatformBackend *platform) {
    if (!platform || !platform->name) return "retrocore-fixture-stub";
    return platform->name;
}

void platform_destroy(PlatformBackend *platform) {
    free(platform);
}

PlatformBackend *platform_create(const PlatformConfig *config) {
    (void)config;
    return NULL;
}

static char *read_text_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    char *buf;
    long len;

    if (!fp) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    len = ftell(fp);
    if (len < 0) {
        fclose(fp);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    buf = calloc((size_t)len + 1u, 1u);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    if (fread(buf, 1u, (size_t)len, fp) != (size_t)len) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    return buf;
}

static const char *retrocore_app_to_retrodesk(const char *app_id) {
    if (strcmp(app_id, "files") == 0) return "filemanager";
    return app_id;
}

static void replay_open_files_and_focus(void) {
    const char *fixture_path = STR(RETROCORE_SPEC_DIR) "/fixtures/events/open-files-and-focus.json";
    char *fixture = read_text_file(fixture_path);
    PlatformBackend *platform;
    DesktopConfig cfg;
    Desktop *desktop;
    const char *retrodesk_app;
    WindowId files_window;
    size_t app_count_before;
    size_t window_count_before;

    assert(fixture != NULL);
    assert(strstr(fixture, "\"type\": \"launch_app\"") != NULL);
    assert(strstr(fixture, "\"app\": \"files\"") != NULL);
    assert(strstr(fixture, "\"focused\": true") != NULL);

    platform = platform_stub_new();
    assert(platform != NULL);

    cfg = (DesktopConfig){
        .platform = platform,
        .input_timeout_ms = 20,
        .bench_mode = true,
        .render_backend = RENDER_BACKEND_CURSES,
        .theme_kind = RETRO_THEME_XP,
    };
    desktop = desktop_create(&cfg);
    assert(desktop != NULL);

    retrodesk_app = retrocore_app_to_retrodesk("files");
    files_window = desktop_app_window_id(desktop, retrodesk_app);
    assert(files_window != WINDOW_ID_INVALID);

    app_count_before = desktop_app_count(desktop);
    window_count_before = desktop_window_count(desktop);

    /* The fixture says launch files and expect it focused. In RetroDesk the
       app is named filemanager and is launched by default, so app_launch()
       focuses the existing instance instead of creating a duplicate. */
    assert(app_launch(desktop, retrodesk_app) == NULL);
    assert(desktop_app_count(desktop) == app_count_before);
    assert(desktop_window_count(desktop) == window_count_before);
    assert(desktop_active_window(desktop) == files_window);

    desktop_shutdown(desktop);
    platform_destroy(platform);
    free(fixture);
}

int main(void) {
    replay_open_files_and_focus();
    return 0;
}
