#include <assert.h>
#include <stdbool.h>

#include "app/app_runtime.h"

static bool dummy_create(RetroAppInstance *instance, const RetroAppContext *ctx) {
    (void)instance;
    (void)ctx;
    return true;
}

static void dummy_event(RetroAppInstance *instance, const RetroEvent *event) {
    (void)instance;
    (void)event;
}

static void dummy_render(RetroAppInstance *instance, RenderContext *ctx) {
    (void)instance;
    (void)ctx;
}

static void dummy_destroy(RetroAppInstance *instance) {
    (void)instance;
}

int main(void) {
    app_registry_reset();

    const RetroAppDescriptor app_a = {
        .app_id = "test-a",
        .display_name = "Test A",
        .required_capabilities = PLATFORM_CAP_KEYBOARD_BASIC,
        .default_height = 8,
        .default_width = 24,
        .default_y = 1,
        .default_x = 1,
        .window_flags = WINDOW_FLAG_APP_OWNED,
        .create = dummy_create,
        .on_event = dummy_event,
        .on_render = dummy_render,
        .destroy = dummy_destroy,
    };

    const RetroAppDescriptor app_b = {
        .app_id = "test-b",
        .display_name = "Test B",
        .required_capabilities = 0,
        .default_height = 8,
        .default_width = 24,
        .default_y = 2,
        .default_x = 2,
        .window_flags = WINDOW_FLAG_APP_OWNED,
        .create = dummy_create,
        .on_event = dummy_event,
        .on_render = dummy_render,
        .destroy = dummy_destroy,
    };

    assert(app_register(&app_a));
    assert(app_register(&app_b));
    assert(!app_register(&app_a)); /* duplicate app_id should fail */

    assert(app_registry_count() == 2);
    assert(app_find("test-a") == &app_a);
    assert(app_find("test-b") == &app_b);
    assert(app_find("missing") == NULL);

    assert(app_descriptor_at(0) != NULL);
    assert(app_descriptor_at(1) != NULL);
    assert(app_descriptor_at(2) == NULL);

    app_registry_reset();
    assert(app_registry_count() == 0);
    return 0;
}
