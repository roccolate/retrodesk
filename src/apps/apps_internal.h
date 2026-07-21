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
bool filemanager_has_item_for_test(const RetroAppInstance *instance,
                                   const char *name);

/* Notepad probes keep user-visible close and bounded-history contracts
   covered without exposing the private NotepadState structure. */
bool notepad_dirty_for_test(const RetroAppInstance *instance);
bool notepad_close_prompt_for_test(const RetroAppInstance *instance);
bool notepad_save_as_for_test(const RetroAppInstance *instance);
bool notepad_close_after_save_for_test(const RetroAppInstance *instance);
size_t notepad_undo_count_for_test(const RetroAppInstance *instance);
size_t notepad_redo_count_for_test(const RetroAppInstance *instance);
const char *notepad_line_for_test(const RetroAppInstance *instance, size_t row);
size_t notepad_cursor_col_for_test(const RetroAppInstance *instance);

#endif
