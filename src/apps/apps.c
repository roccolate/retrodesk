#include "apps/apps.h"

const RetroAppDescriptor *filemanager_app_descriptor(void);
const RetroAppDescriptor *notepad_app_descriptor(void);
const RetroAppDescriptor *terminal_app_descriptor(void);

void apps_register_builtin(AppRegistry *registry) {
    if (!registry) return;
    app_registry_register(registry, filemanager_app_descriptor());
    app_registry_register(registry, notepad_app_descriptor());
    app_registry_register(registry, terminal_app_descriptor());
}
