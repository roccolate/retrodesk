#include "apps/apps.h"

#include "app/app_runtime.h"

const RetroAppDescriptor *filemanager_app_descriptor(void);
const RetroAppDescriptor *notepad_app_descriptor(void);
const RetroAppDescriptor *terminal_app_descriptor(void);

void apps_register_builtin(void) {
    app_register(filemanager_app_descriptor());
    app_register(notepad_app_descriptor());
    app_register(terminal_app_descriptor());
}
