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

static void dummy_render(RetroAppInstance *instance, DrawList *draw_list) {
    (void)instance;
    (void)draw_list;
}

static void dummy_destroy(RetroAppInstance *instance) {
    (void)instance;
}

int main(void) {
    AppRegistry *registry = app_registry_create();
    assert(registry != NULL);

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

    assert(app_registry_register(registry, &app_a));
    assert(app_registry_register(registry, &app_b));
    assert(!app_registry_register(registry, &app_a)); /* duplicate app_id should fail */

    assert(app_registry_count(registry) == 2);
    assert(app_registry_find(registry, "test-a") == &app_a);
    assert(app_registry_find(registry, "test-b") == &app_b);
    assert(app_registry_find(registry, "missing") == NULL);

    assert(app_registry_descriptor_at(registry, 0) != NULL);
    assert(app_registry_descriptor_at(registry, 1) != NULL);
    assert(app_registry_descriptor_at(registry, 2) == NULL);

    app_registry_reset(registry);
    assert(app_registry_count(registry) == 0);
    app_registry_destroy(registry);
    return 0;
}
