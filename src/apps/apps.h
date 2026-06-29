#ifndef RETRODESK_APPS_APPS_H
#define RETRODESK_APPS_APPS_H

#include "app/app_runtime.h"

/* Registers the bundled stub apps (filemanager, notepad, terminal). */

void apps_register_builtin(AppRegistry *registry);

#endif
