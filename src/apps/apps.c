#include "apps/apps.h"
#include "apps/apps_internal.h"

void apps_register_builtin(AppRegistry *registry) {
    if (!registry) return;
    app_registry_register(registry, filemanager_app_descriptor());
    app_registry_register(registry, notepad_app_descriptor());
    app_registry_register(registry, terminal_app_descriptor());
}
