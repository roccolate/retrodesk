#ifndef RETRODESK_UI_DIALOG_H
#define RETRODESK_UI_DIALOG_H

#include <stdbool.h>
#include <stddef.h>

#include "core/event.h"
#include "render/render.h"

/* Dialog widget — modal message box, confirm, or input prompt.
   Dialogs produce DrawList commands and never call backend draw APIs
   directly. They compose Button and TextInput widgets internally.

   Dialog types:
     DIALOG_MESSAGE  — informational message with [OK]
     DIALOG_CONFIRM  — question with [OK] [Cancel]
     DIALOG_INPUT    — text field with [OK] [Cancel]

   The caller polls dialog_result() after forwarding events. A non-NONE
   result means the dialog is dismissed and can be destroyed.

   Tab / Shift-Tab cycles focus between the input field (if present) and
   buttons. Escape maps to Cancel (or OK for message dialogs). */

typedef enum DialogType {
    DIALOG_MESSAGE = 0,
    DIALOG_CONFIRM,
    DIALOG_INPUT,
} DialogType;

typedef enum DialogResult {
    DIALOG_RESULT_NONE = 0,
    DIALOG_RESULT_OK,
    DIALOG_RESULT_CANCEL,
} DialogResult;

/* Styles passed to dialog_render. All pointers must be non-NULL. */
typedef struct DialogStyles {
    RenderStyle frame;       /* border + background fill  */
    RenderStyle title;       /* title text in border       */
    RenderStyle message;     /* message body text          */
    RenderStyle input;       /* text input normal          */
    RenderStyle input_cursor;/* text input cursor          */
    RenderStyle btn_normal;  /* button normal              */
    RenderStyle btn_focused; /* button focused/selected    */
} DialogStyles;

typedef struct Dialog Dialog;

/* --- lifecycle ------------------------------------------------------- */

Dialog *dialog_create_message(const char *title, const char *message);
Dialog *dialog_create_confirm(const char *title, const char *message);
Dialog *dialog_create_input(const char *title, const char *message,
                            size_t max_input_len);
void dialog_destroy(Dialog *dialog);

/* --- accessors ------------------------------------------------------- */

DialogType dialog_type(const Dialog *dialog);
DialogResult dialog_result(const Dialog *dialog);

/* Returns the text entered in an INPUT dialog, or "" otherwise. */
const char *dialog_input_text(const Dialog *dialog);

/* Pre-fill the input field (INPUT dialogs only). */
bool dialog_set_input_text(Dialog *dialog, const char *text);

/* --- event handling -------------------------------------------------- */

/* Returns true if the event was consumed. Dismisses the dialog by
   setting the result on OK/Cancel activation or Escape. */
bool dialog_handle_key(Dialog *dialog, const RetroKeyEvent *key);
bool dialog_handle_mouse(Dialog *dialog, const RetroMouseEvent *mouse,
                         int dialog_y, int dialog_x,
                         int dialog_w, int dialog_h);

/* --- rendering ------------------------------------------------------- */

/* Render the dialog into the draw list at the given position and size.
   The dialog draws its own border, title, message, and buttons.
   Minimum useful size: 20 wide × 7 tall. */
void dialog_render(const Dialog *dialog, DrawList *draw_list,
                   int y, int x, int w, int h,
                   const DialogStyles *styles);

/* Returns a suggested width for the dialog based on title/message
   length (clamped to max_w). Useful for auto-sizing. */
int dialog_suggest_width(const Dialog *dialog, int max_w);

/* Returns a suggested height for the dialog. */
int dialog_suggest_height(const Dialog *dialog);

#endif
