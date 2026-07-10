#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apps/apps_internal.h"
#include "core/desktop.h"
#include "platform/platform.h"
#include "wm/window_manager.h"

#ifndef RETROCORE_SPEC_DIR
#error "RETROCORE_SPEC_DIR must point at a retrocore-spec checkout"
#endif

#define STR2(x) #x
#define STR(x) STR2(x)

struct PlatformBackend {
    PlatformFeatures features;
    const char *name;
    const RetroEvent *events;
    size_t event_count;
    size_t next_event;
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

static void platform_stub_set_events(PlatformBackend *platform, const RetroEvent *events,
                                     size_t event_count) {
    assert(platform != NULL);
    platform->events = events;
    platform->event_count = event_count;
    platform->next_event = 0;
}

bool platform_poll_event(PlatformBackend *platform, RetroEvent *out_event, int timeout_ms) {
    (void)timeout_ms;
    if (!platform || !out_event) return false;
    if (platform->next_event >= platform->event_count) return false;
    *out_event = platform->events[platform->next_event++];
    return true;
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
    if (strcmp(app_id, "notes") == 0) return "notepad";
    return app_id;
}

static bool event_type_is_supported(const char *type) {
    return strcmp(type, "launch_app") == 0 || strcmp(type, "pointer_down") == 0 ||
           strcmp(type, "pointer_move") == 0 || strcmp(type, "pointer_up") == 0 ||
           strcmp(type, "focus_next") == 0 || strcmp(type, "close_window") == 0;
}

static void assert_supported_event_types(const char *fixture) {
    const char *cursor = fixture;
    while ((cursor = strstr(cursor, "\"type\"")) != NULL) {
        const char *colon = strchr(cursor, ':');
        const char *start;
        const char *end;
        char type[64];
        size_t len;

        assert(colon != NULL);
        start = strchr(colon, '"');
        assert(start != NULL);
        start++;
        end = strchr(start, '"');
        assert(end != NULL);
        len = (size_t)(end - start);
        assert(len < sizeof(type));
        memcpy(type, start, len);
        type[len] = '\0';

        if (!event_type_is_supported(type)) {
            fprintf(stderr,
                    "TODO: retrocore fixture event type '%s' is not supported by "
                    "retrocore_event_fixture_test yet\n",
                    type);
            assert(!"unsupported retrocore event type in fixture");
        }
        cursor = end + 1;
    }
}

static bool find_event_field_int(const char *fixture, const char *event_type,
                                 const char *field_name, int *out_value) {
    char type_pattern[96];
    char field_pattern[64];
    const char *event;
    const char *field;
    char *endptr;
    long value;

    snprintf(type_pattern, sizeof(type_pattern), "\"type\": \"%s\"", event_type);
    event = strstr(fixture, type_pattern);
    if (!event) return false;

    snprintf(field_pattern, sizeof(field_pattern), "\"%s\":", field_name);
    field = strstr(event, field_pattern);
    if (!field) return false;
    field += strlen(field_pattern);
    while (*field == ' ' || *field == '\t') field++;

    value = strtol(field, &endptr, 10);
    if (field == endptr) return false;
    if (out_value) *out_value = (int)value;
    return true;
}

static RetroEvent make_quit_event(void) {
    RetroEvent event = {0};
    event.type = RETRO_EVENT_KEY;
    event.data.key.key_code = 'q';
    event.data.key.is_printable = true;
    event.data.key.ascii = 'q';
    return event;
}

static RetroEvent make_focus_next_event(void) {
    RetroEvent event = {0};
    event.type = RETRO_EVENT_KEY;
    event.data.key.key_code = '\t';
    event.data.key.ascii = '\0';
    event.data.key.is_printable = false;
    return event;
}

static RetroEvent make_close_focused_window_event(void) {
    RetroEvent event = {0};
    event.type = RETRO_EVENT_KEY;
    event.data.key.key_code = 'w';
    event.data.key.ascii = 'w';
    event.data.key.is_printable = true;
    return event;
}

static RetroEvent make_pointer_down_event(int x, int y) {
    RetroEvent event = {0};
    event.type = RETRO_EVENT_MOUSE;
    event.data.mouse.x = x;
    event.data.mouse.y = y;
    event.data.mouse.button1_pressed = true;
    return event;
}

static RetroEvent make_pointer_move_event(int x, int y) {
    RetroEvent event = {0};
    event.type = RETRO_EVENT_MOUSE;
    event.data.mouse.x = x;
    event.data.mouse.y = y;
    event.data.mouse.moved = true;
    return event;
}

static RetroEvent make_pointer_up_event(int x, int y) {
    RetroEvent event = {0};
    event.type = RETRO_EVENT_MOUSE;
    event.data.mouse.x = x;
    event.data.mouse.y = y;
    event.data.mouse.button1_released = true;
    return event;
}

static Desktop *create_fixture_desktop(PlatformBackend *platform, bool bench_mode) {
    DesktopConfig cfg = {
        .platform = platform,
        .input_timeout_ms = 20,
        .bench_mode = bench_mode,
        .render_backend = RENDER_BACKEND_CURSES,
        .theme_kind = RETRO_THEME_XP,
    };
    return desktop_create(&cfg);
}

static WindowId replay_launch_existing_app(Desktop *desktop, const char *logical_app_id,
                                           size_t *out_app_count_before,
                                           size_t *out_window_count_before) {
    const char *retrodesk_app = retrocore_app_to_retrodesk(logical_app_id);
    WindowId window = desktop_app_window_id(desktop, retrodesk_app);
    size_t app_count_before;
    size_t window_count_before;

    assert(window != WINDOW_ID_INVALID);

    app_count_before = desktop_app_count(desktop);
    window_count_before = desktop_window_count(desktop);

    /* RetroDesk launches File Manager and Notepad by default in the current
       foundation desktop. Replaying retrocore launch_app for those logical apps
       should focus existing instances instead of creating duplicates. */
    assert(app_launch(desktop, retrodesk_app) == NULL);
    assert(desktop_app_count(desktop) == app_count_before);
    assert(desktop_window_count(desktop) == window_count_before);
    assert(desktop_active_window(desktop) == window);

    if (out_app_count_before) *out_app_count_before = app_count_before;
    if (out_window_count_before) *out_window_count_before = window_count_before;
    return window;
}

static WindowId replay_launch_files(Desktop *desktop, size_t *out_app_count_before,
                                    size_t *out_window_count_before) {
    return replay_launch_existing_app(desktop, "files", out_app_count_before,
                                      out_window_count_before);
}

static bool read_drag_delta(const char *fixture, int *out_dx, int *out_dy) {
    int down_x;
    int down_y;
    int move_x;
    int move_y;

    if (!find_event_field_int(fixture, "pointer_down", "x", &down_x)) return false;
    if (!find_event_field_int(fixture, "pointer_down", "y", &down_y)) return false;
    if (!find_event_field_int(fixture, "pointer_move", "x", &move_x)) return false;
    if (!find_event_field_int(fixture, "pointer_move", "y", &move_y)) return false;

    if (out_dx) *out_dx = move_x - down_x;
    if (out_dy) *out_dy = move_y - down_y;
    return true;
}

static void assert_window_drag_geometry(const char *fixture) {
    const RetroAppDescriptor *files_desc = filemanager_app_descriptor();
    Renderer *renderer;
    WindowManager *wm;
    WindowId window_id;
    RetroWindowSpec spec;
    RetroWindow *window;
    RetroEvent down;
    RetroEvent move;
    RetroEvent up;
    int dx;
    int dy;
    int final_y = 0;
    int final_x = 0;
    int final_h = 0;
    int final_w = 0;
    int local_down_x;
    int local_down_y;

    assert(files_desc != NULL);
    assert(read_drag_delta(fixture, &dx, &dy));

    renderer = renderer_create(NULL);
    assert(renderer != NULL);
    wm = wm_create(renderer);
    assert(wm != NULL);

    spec = (RetroWindowSpec){
        .height = files_desc->default_height,
        .width = files_desc->default_width,
        .y = files_desc->default_y,
        .x = files_desc->default_x,
        .title = files_desc->display_name,
        .flags = files_desc->window_flags,
        .draw_cb = NULL,
        .event_cb = NULL,
        .user_data = NULL,
    };
    window_id = wm_create_window(wm, &spec);
    assert(window_id != WINDOW_ID_INVALID);

    local_down_x = files_desc->default_x + 4;
    local_down_y = files_desc->default_y;
    down = make_pointer_down_event(local_down_x, local_down_y);
    move = make_pointer_move_event(local_down_x + dx, local_down_y + dy);
    up = make_pointer_up_event(local_down_x + dx, local_down_y + dy);

    assert(wm_handle_event(wm, &down));
    assert(wm_handle_event(wm, &move));
    assert(wm_handle_event(wm, &up));

    window = wm_window(wm, window_id);
    assert(window != NULL);
    retro_window_get_geometry(window, &final_y, &final_x, &final_h, &final_w);
    assert(final_y == files_desc->default_y + dy);
    assert(final_x == files_desc->default_x + dx);
    assert(final_h == files_desc->default_height);
    assert(final_w == files_desc->default_width);
    assert(wm_active_window(wm) == window_id);

    wm_destroy(wm);
    renderer_destroy(renderer);
}

static void replay_open_files_and_focus(void) {
    const char *fixture_path = STR(RETROCORE_SPEC_DIR) "/fixtures/events/open-files-and-focus.json";
    char *fixture = read_text_file(fixture_path);
    PlatformBackend *platform;
    Desktop *desktop;

    assert(fixture != NULL);
    assert(strstr(fixture, "\"type\": \"launch_app\"") != NULL);
    assert(strstr(fixture, "\"app\": \"files\"") != NULL);
    assert(strstr(fixture, "\"focused\": true") != NULL);
    assert_supported_event_types(fixture);

    platform = platform_stub_new();
    assert(platform != NULL);
    desktop = create_fixture_desktop(platform, true);
    assert(desktop != NULL);

    (void)replay_launch_files(desktop, NULL, NULL);

    desktop_shutdown(desktop);
    platform_destroy(platform);
    free(fixture);
}

static void replay_window_drag_basic_if_available(void) {
    const char *fixture_path = STR(RETROCORE_SPEC_DIR) "/fixtures/events/window-drag-basic.json";
    char *fixture = read_text_file(fixture_path);
    PlatformBackend *platform;
    Desktop *desktop;
    WindowId files_window;
    size_t app_count_before;
    size_t window_count_before;
    const RetroAppDescriptor *files_desc;
    int dx;
    int dy;
    int local_down_x;
    int local_down_y;
    RetroEvent events[4];

    if (!fixture) {
        printf("retrocore: skipping window-drag-basic fixture; file not found at %s\n",
               fixture_path);
        return;
    }

    assert(strstr(fixture, "\"type\": \"launch_app\"") != NULL);
    assert(strstr(fixture, "\"type\": \"pointer_down\"") != NULL);
    assert(strstr(fixture, "\"type\": \"pointer_move\"") != NULL);
    assert(strstr(fixture, "\"type\": \"pointer_up\"") != NULL);
    assert(strstr(fixture, "\"app\": \"files\"") != NULL);
    assert(strstr(fixture, "\"focused\": true") != NULL);
    assert_supported_event_types(fixture);
    assert(read_drag_delta(fixture, &dx, &dy));

    platform = platform_stub_new();
    assert(platform != NULL);
    desktop = create_fixture_desktop(platform, false);
    assert(desktop != NULL);

    files_window = replay_launch_files(desktop, &app_count_before, &window_count_before);

    files_desc = filemanager_app_descriptor();
    assert(files_desc != NULL);

    /* The retrocore fixture uses logical coordinates. The RetroDesk adapter
       translates the start point to the local File Manager title bar while
       preserving the fixture's drag delta. This exercises real desktop/WM event
       routing without requiring projects to share identical initial geometry.
       WM-level coordinate assertions are covered by assert_window_drag_geometry. */
    local_down_x = files_desc->default_x + 4;
    local_down_y = files_desc->default_y;

    events[0] = make_pointer_down_event(local_down_x, local_down_y);
    events[1] = make_pointer_move_event(local_down_x + dx, local_down_y + dy);
    events[2] = make_pointer_up_event(local_down_x + dx, local_down_y + dy);
    events[3] = make_quit_event();
    platform_stub_set_events(platform, events, sizeof(events) / sizeof(events[0]));

    assert(desktop_run(desktop) == EXIT_SUCCESS);
    assert(desktop_app_window_id(desktop, retrocore_app_to_retrodesk("files")) == files_window);
    assert(desktop_active_window(desktop) == files_window);
    assert(desktop_app_count(desktop) == app_count_before);
    assert(desktop_window_count(desktop) == window_count_before);

    assert_window_drag_geometry(fixture);

    desktop_shutdown(desktop);
    platform_destroy(platform);
    free(fixture);
}

static void replay_focus_next_basic_if_available(void) {
    const char *fixture_path = STR(RETROCORE_SPEC_DIR) "/fixtures/events/focus-next-basic.json";
    char *fixture = read_text_file(fixture_path);
    PlatformBackend *platform;
    Desktop *desktop;
    WindowId files_window;
    WindowId notes_window;
    size_t app_count_before;
    size_t window_count_before;
    RetroEvent events[2];

    if (!fixture) {
        printf("retrocore: skipping focus-next-basic fixture; file not found at %s\n",
               fixture_path);
        return;
    }

    assert(strstr(fixture, "\"type\": \"launch_app\"") != NULL);
    assert(strstr(fixture, "\"type\": \"focus_next\"") != NULL);
    assert(strstr(fixture, "\"app\": \"files\"") != NULL);
    assert(strstr(fixture, "\"app\": \"notes\"") != NULL);
    assert(strstr(fixture, "\"focused\": true") != NULL);
    assert(strstr(fixture, "\"focused\": false") != NULL);
    assert_supported_event_types(fixture);

    platform = platform_stub_new();
    assert(platform != NULL);
    desktop = create_fixture_desktop(platform, false);
    assert(desktop != NULL);

    files_window = replay_launch_existing_app(desktop, "files", NULL, NULL);
    notes_window = replay_launch_existing_app(desktop, "notes", NULL, NULL);
    assert(files_window != notes_window);
    assert(desktop_active_window(desktop) == notes_window);

    app_count_before = desktop_app_count(desktop);
    window_count_before = desktop_window_count(desktop);

    events[0] = make_focus_next_event();
    events[1] = make_quit_event();
    platform_stub_set_events(platform, events, sizeof(events) / sizeof(events[0]));

    assert(desktop_run(desktop) == EXIT_SUCCESS);
    assert(desktop_active_window(desktop) == files_window);
    assert(desktop_app_window_id(desktop, retrocore_app_to_retrodesk("files")) == files_window);
    assert(desktop_app_window_id(desktop, retrocore_app_to_retrodesk("notes")) == notes_window);
    assert(desktop_app_count(desktop) == app_count_before);
    assert(desktop_window_count(desktop) == window_count_before);

    desktop_shutdown(desktop);
    platform_destroy(platform);
    free(fixture);
}

static void replay_close_focused_window_if_available(void) {
    const char *fixture_path = STR(RETROCORE_SPEC_DIR) "/fixtures/events/close-focused-window.json";
    char *fixture = read_text_file(fixture_path);
    PlatformBackend *platform;
    Desktop *desktop;
    WindowId files_window;
    size_t app_count_before;
    size_t window_count_before;
    RetroEvent events[2];

    if (!fixture) {
        printf("retrocore: skipping close-focused-window fixture; file not found at %s\n",
               fixture_path);
        return;
    }

    assert(strstr(fixture, "\"type\": \"launch_app\"") != NULL);
    assert(strstr(fixture, "\"type\": \"close_window\"") != NULL);
    assert(strstr(fixture, "\"window\": \"focused\"") != NULL);
    assert(strstr(fixture, "\"app\": \"files\"") != NULL);
    assert(strstr(fixture, "\"exists\": false") != NULL);
    assert_supported_event_types(fixture);

    platform = platform_stub_new();
    assert(platform != NULL);
    desktop = create_fixture_desktop(platform, false);
    assert(desktop != NULL);

    files_window = replay_launch_files(desktop, &app_count_before, &window_count_before);
    assert(app_count_before > 0);
    assert(window_count_before > 0);

    events[0] = make_close_focused_window_event();
    events[1] = make_quit_event();
    platform_stub_set_events(platform, events, sizeof(events) / sizeof(events[0]));

    assert(desktop_run(desktop) == EXIT_SUCCESS);
    assert(desktop_app_window_id(desktop, retrocore_app_to_retrodesk("files")) == WINDOW_ID_INVALID);
    assert(desktop_active_window(desktop) != files_window);
    assert(desktop_app_count(desktop) == app_count_before - 1u);
    assert(desktop_window_count(desktop) == window_count_before - 1u);

    desktop_shutdown(desktop);
    platform_destroy(platform);
    free(fixture);
}

int main(void) {
    replay_open_files_and_focus();
    replay_window_drag_basic_if_available();
    replay_focus_next_basic_if_available();
    replay_close_focused_window_if_available();
    return 0;
}
