#include "app/app_runtime.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct AppRegistry {
    const RetroAppDescriptor **entries;
    size_t count;
    size_t capacity;
};

static bool app_registry_reserve(AppRegistry *registry, size_t want) {
    if (!registry) return false;
    if (want <= registry->capacity) return true;
    if (want > SIZE_MAX / sizeof(*registry->entries)) return false;

    size_t new_cap = registry->capacity ? registry->capacity : 8;
    while (new_cap < want) {
        if (new_cap > SIZE_MAX / 2) {
            new_cap = want;
            break;
        }
        new_cap *= 2;
    }

    const RetroAppDescriptor **next =
        realloc(registry->entries, new_cap * sizeof(*next));
    if (!next) return false;
    registry->entries = next;
    registry->capacity = new_cap;
    return true;
}

AppRegistry *app_registry_create(void) {
    return calloc(1, sizeof(AppRegistry));
}

void app_registry_destroy(AppRegistry *registry) {
    if (!registry) return;
    free(registry->entries);
    free(registry);
}

void app_registry_reset(AppRegistry *registry) {
    if (!registry) return;
    free(registry->entries);
    registry->entries = NULL;
    registry->count = 0;
    registry->capacity = 0;
}

bool app_registry_register(AppRegistry *registry, const RetroAppDescriptor *desc) {
    if (!registry || !desc || !desc->app_id || !desc->app_id[0]) return false;
    if (registry->count == SIZE_MAX) return false;
    if (app_registry_find(registry, desc->app_id)) return false;
    if (!app_registry_reserve(registry, registry->count + 1)) return false;
    registry->entries[registry->count++] = desc;
    return true;
}

const RetroAppDescriptor *app_registry_find(const AppRegistry *registry,
                                             const char *app_id) {
    if (!registry || !app_id) return NULL;
    for (size_t i = 0; i < registry->count; ++i) {
        if (strcmp(registry->entries[i]->app_id, app_id) == 0) {
            return registry->entries[i];
        }
    }
    return NULL;
}

size_t app_registry_count(const AppRegistry *registry) {
    if (!registry) return 0;
    return registry->count;
}

const RetroAppDescriptor *app_registry_descriptor_at(const AppRegistry *registry,
                                                      size_t index) {
    if (!registry || index >= registry->count) return NULL;
    return registry->entries[index];
}

void app_handle_event(RetroAppInstance *app, const RetroEvent *event) {
    if (!app || !event || !app->descriptor || !app->descriptor->on_event) return;
    app->descriptor->on_event(app, event);
}

RetroAppServiceResult app_service(RetroAppInstance *app, size_t work_budget) {
    if (!app || !app->descriptor || !app->descriptor->on_service ||
        work_budget == 0) {
        return RETRO_APP_SERVICE_IDLE;
    }
    return app->descriptor->on_service(app, work_budget);
}

void app_render(RetroAppInstance *app, DrawList *draw_list) {
    if (!app || !draw_list || !app->descriptor || !app->descriptor->on_render) return;
    app->descriptor->on_render(app, draw_list);
}

void app_destroy(RetroAppInstance *app) {
    if (!app) return;
    if (app->descriptor && app->descriptor->destroy) {
        app->descriptor->destroy(app);
    }
    app->state = NULL;
    app->descriptor = NULL;
    free(app->resource_path_owned);
    app->resource_path_owned = NULL;
    free(app);
}

RetroCloseResult app_request_close(RetroAppInstance *app) {
    if (!app) return RETRO_CLOSE_CANCELLED;

    app->close_requested = false;
    app->close_pending = false;
    app->close_result = RETRO_CLOSE_ALLOWED;

    if (app->descriptor && app->descriptor->can_close &&
        !app->descriptor->can_close(app)) {
        app->close_pending = true;
        app->close_result = RETRO_CLOSE_DEFERRED;
        return RETRO_CLOSE_DEFERRED;
    }

    app->close_requested = true;
    return RETRO_CLOSE_ALLOWED;
}

void app_resolve_close(RetroAppInstance *app, RetroCloseResult result) {
    if (!app || !app->close_pending || result == RETRO_CLOSE_DEFERRED) return;
    app->close_pending = false;
    app->close_result = result;
    app->close_requested = result == RETRO_CLOSE_ALLOWED;
}

void app_reset_close_request(RetroAppInstance *app) {
    if (!app) return;
    app->close_requested = false;
    app->close_pending = false;
    app->close_result = RETRO_CLOSE_ALLOWED;
}

bool app_is_close_requested(const RetroAppInstance *app) {
    return app && app->close_requested;
}

bool app_is_close_pending(const RetroAppInstance *app) {
    return app && app->close_pending;
}

RetroCloseResult app_close_result(const RetroAppInstance *app) {
    return app ? app->close_result : RETRO_CLOSE_CANCELLED;
}
