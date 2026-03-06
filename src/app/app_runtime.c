#include "app/app_runtime.h"

#include <stdlib.h>
#include <string.h>

static const RetroAppDescriptor **g_registry = NULL;
static size_t g_registry_count = 0;
static size_t g_registry_capacity = 0;

static bool registry_reserve(size_t want) {
    if (want <= g_registry_capacity) return true;
    size_t new_cap = g_registry_capacity ? g_registry_capacity * 2 : 8;
    while (new_cap < want) new_cap *= 2;
    const RetroAppDescriptor **next =
        realloc(g_registry, new_cap * sizeof(*next));
    if (!next) return false;
    g_registry = next;
    g_registry_capacity = new_cap;
    return true;
}

bool app_register(const RetroAppDescriptor *desc) {
    if (!desc || !desc->app_id || !desc->app_id[0]) return false;
    if (app_find(desc->app_id)) return false;
    if (!registry_reserve(g_registry_count + 1)) return false;
    g_registry[g_registry_count++] = desc;
    return true;
}

void app_registry_reset(void) {
    free(g_registry);
    g_registry = NULL;
    g_registry_count = 0;
    g_registry_capacity = 0;
}

const RetroAppDescriptor *app_find(const char *app_id) {
    if (!app_id) return NULL;
    for (size_t i = 0; i < g_registry_count; ++i) {
        if (strcmp(g_registry[i]->app_id, app_id) == 0) return g_registry[i];
    }
    return NULL;
}

size_t app_registry_count(void) {
    return g_registry_count;
}

const RetroAppDescriptor *app_descriptor_at(size_t index) {
    if (index >= g_registry_count) return NULL;
    return g_registry[index];
}

void app_handle_event(RetroAppInstance *app, const RetroEvent *event) {
    if (!app || !event || !app->descriptor || !app->descriptor->on_event) return;
    app->descriptor->on_event(app, event);
}

void app_render(RetroAppInstance *app, RenderContext *ctx) {
    if (!app || !ctx || !app->descriptor || !app->descriptor->on_render) return;
    app->descriptor->on_render(app, ctx);
}

void app_destroy(RetroAppInstance *app) {
    if (!app) return;
    if (app->descriptor && app->descriptor->destroy) {
        app->descriptor->destroy(app);
    }
    free(app);
}

void app_request_close(RetroAppInstance *app) {
    if (!app) return;
    app->close_requested = true;
}

bool app_is_close_requested(const RetroAppInstance *app) {
    return app && app->close_requested;
}
