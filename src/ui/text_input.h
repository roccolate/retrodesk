#ifndef RETRODESK_UI_TEXT_INPUT_H
#define RETRODESK_UI_TEXT_INPUT_H

#include <stdbool.h>
#include <stddef.h>

#include "core/event.h"
#include "render/render.h"

/* Single-line text input widget with cursor, viewport scrolling, and
   standard editing shortcuts. Apps embed a TextInput and forward key
   events to it; the widget appends draw commands to a DrawList. */

typedef struct TextInput TextInput;

/* Lifecycle */
TextInput *text_input_create(size_t max_len);
void text_input_destroy(TextInput *input);

/* State accessors */
const char *text_input_text(const TextInput *input);
void text_input_set_text(TextInput *input, const char *text);
void text_input_clear(TextInput *input);
size_t text_input_cursor(const TextInput *input);
size_t text_input_length(const TextInput *input);

/* Event handling — returns true if the key was consumed. */
bool text_input_handle_key(TextInput *input, const RetroKeyEvent *key);

/* Render the input field into the given draw list.
   visible_width: how many columns are available for display.
   The widget auto-scrolls to keep the cursor visible. */
void text_input_render(const TextInput *input, DrawList *draw_list,
                       int y, int x, int visible_width,
                       const RenderStyle *style,
                       const RenderStyle *cursor_style);

#endif
