#ifndef RETRODESK_APPS_APPS_INTERNAL_H
#define RETRODESK_APPS_APPS_INTERNAL_H

#include "app/app_runtime.h"

const RetroAppDescriptor *filemanager_app_descriptor(void);
const RetroAppDescriptor *notepad_app_descriptor(void);
const RetroAppDescriptor *terminal_app_descriptor(void);

/* File Manager behavior probes used by the runtime tests. */
size_t filemanager_item_count_for_test(const RetroAppInstance *instance);
const char *filemanager_selected_name_for_test(const RetroAppInstance *instance);
size_t filemanager_scroll_offset_for_test(const RetroAppInstance *instance);
bool filemanager_show_hidden_for_test(const RetroAppInstance *instance);
bool filemanager_has_item_for_test(const RetroAppInstance *instance, const char *name);

#endif
