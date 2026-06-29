#ifndef RETRODESK_UI_BUTTON_H
#define RETRODESK_UI_BUTTON_H

#include <stdbool.h>
#include <stddef.h>

#include "core/event.h"
#include "render/render.h"

/* Button widget — clickable, keyboard-focusable button with a label.
   Renders as [ Label ] and can be activated via mouse click, Enter, or
   Space. Buttons produce DrawList commands and never call backend draw
   APIs directly. */

typedef struct Button Button;

/* Callback invoked when the button is activated (click or key press).
   user_data is the pointer passed to button_set_callback. */
typedef void (*ButtonCallback)(Button *button, void *user_data);

/* Lifecycle */
Button *button_create(const char *label);
void button_destroy(Button *button);

/* Label management — label is copied internally. */
const char *button_label(const Button *button);
bool button_set_label(Button *button, const char *label);

/* Focus state */
bool button_focused(const Button *button);
void button_set_focused(Button *button, bool focused);

/* Enabled state — disabled buttons ignore input and render dimmed. */
bool button_enabled(const Button *button);
void button_set_enabled(Button *button, bool enabled);

/* Callback */
void button_set_callback(Button *button, ButtonCallback callback,
                         void *user_data);

/* Event handling — returns true if the event was consumed.
   For keys: Enter and Space activate the button when focused.
   For mouse: click within the button bounds activates it. */
bool button_handle_key(Button *button, const RetroKeyEvent *key);
bool button_handle_mouse(Button *button, const RetroMouseEvent *mouse,
                         int widget_y, int widget_x, int widget_w);

/* Render the button into the given draw list.
   Renders as "[ Label ]" padded to fit. */
void button_render(const Button *button, DrawList *draw_list,
                   int y, int x,
                   const RenderStyle *normal_style,
                   const RenderStyle *focused_style,
                   const RenderStyle *disabled_style);

/* Returns the rendered width of the button: strlen("[ ") + label + strlen(" ]") */
int button_width(const Button *button);

#endif
