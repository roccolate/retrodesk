#include "test_harness.h"
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

static bool deferred_can_close(RetroAppInstance *instance) {
    (void)instance;
    return false;
}

static void test_close_resolution_contract(void) {
    RetroAppDescriptor descriptor = {
        .app_id = "close-test",
        .display_name = "Close Test",
        .can_close = deferred_can_close,
    };
    RetroAppInstance instance = {.descriptor = &descriptor};

    TEST_REQUIRE(app_request_close(&instance) == RETRO_CLOSE_DEFERRED);
    TEST_REQUIRE(app_is_close_pending(&instance));
    TEST_REQUIRE(!app_is_close_requested(&instance));

    app_resolve_close(&instance, RETRO_CLOSE_CANCELLED);
    TEST_REQUIRE(!app_is_close_pending(&instance));
    TEST_REQUIRE(app_close_result(&instance) == RETRO_CLOSE_CANCELLED);
    TEST_REQUIRE(!app_is_close_requested(&instance));

    TEST_REQUIRE(app_request_close(&instance) == RETRO_CLOSE_DEFERRED);
    app_resolve_close(&instance, RETRO_CLOSE_ALLOWED);
    TEST_REQUIRE(app_is_close_requested(&instance));
    app_reset_close_request(&instance);
    TEST_REQUIRE(!app_is_close_requested(&instance));
    TEST_REQUIRE(!app_is_close_pending(&instance));
}

int main(void) {
    AppRegistry *registry = app_registry_create();
    TEST_REQUIRE(registry != NULL);

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

    TEST_REQUIRE(app_registry_register(registry, &app_a));
    TEST_REQUIRE(app_registry_register(registry, &app_b));
    TEST_REQUIRE(!app_registry_register(registry, &app_a)); /* duplicate app_id should fail */

    TEST_REQUIRE(app_registry_count(registry) == 2);
    TEST_REQUIRE(app_registry_find(registry, "test-a") == &app_a);
    TEST_REQUIRE(app_registry_find(registry, "test-b") == &app_b);
    TEST_REQUIRE(app_registry_find(registry, "missing") == NULL);

    TEST_REQUIRE(app_registry_descriptor_at(registry, 0) != NULL);
    TEST_REQUIRE(app_registry_descriptor_at(registry, 1) != NULL);
    TEST_REQUIRE(app_registry_descriptor_at(registry, 2) == NULL);

    app_registry_reset(registry);
    TEST_REQUIRE(app_registry_count(registry) == 0);
    app_registry_destroy(registry);
    test_close_resolution_contract();
    return 0;
}
