#ifndef RETRODESK_UI_TEXT_BUFFER_H
#define RETRODESK_UI_TEXT_BUFFER_H

#include <stdbool.h>
#include <stddef.h>

#include "core/event.h"
#include "render/render.h"

/* Multi-line text buffer with line array, insert/delete, and cursor
   tracking (row + col). Apps embed a TextBuffer and forward key events;
   the widget appends draw commands to a DrawList.

   Each line is stored as a separate heap-allocated string. The buffer
   always contains at least one line (even when empty). */

typedef struct TextBuffer TextBuffer;

/* Lifecycle */
TextBuffer *text_buffer_create(void);
void text_buffer_destroy(TextBuffer *buf);

/* Content */
void text_buffer_clear(TextBuffer *buf);
bool text_buffer_set_text(TextBuffer *buf, const char *text);
size_t text_buffer_line_count(const TextBuffer *buf);
const char *text_buffer_line(const TextBuffer *buf, size_t row);
size_t text_buffer_line_length(const TextBuffer *buf, size_t row);
/* Returns a newly allocated LF-separated snapshot.  The caller owns it. */
char *text_buffer_to_text(const TextBuffer *buf, size_t *length);

/* Cursor state */
size_t text_buffer_cursor_row(const TextBuffer *buf);
size_t text_buffer_cursor_col(const TextBuffer *buf);
void text_buffer_set_cursor(TextBuffer *buf, size_t row, size_t col);

/* Scroll state */
size_t text_buffer_scroll_row(const TextBuffer *buf);
size_t text_buffer_scroll_col(const TextBuffer *buf);

/* Editing */
bool text_buffer_insert_char(TextBuffer *buf, char ch);
bool text_buffer_insert_newline(TextBuffer *buf);
bool text_buffer_delete_backward(TextBuffer *buf);
bool text_buffer_delete_forward(TextBuffer *buf);

/* Event handling — returns true if the key was consumed. */
bool text_buffer_handle_key(TextBuffer *buf, const RetroKeyEvent *key);

/* Render into a draw list.
   visible_rows/visible_cols define the viewport size.
   The widget auto-scrolls to keep the cursor visible. */
void text_buffer_render(const TextBuffer *buf, DrawList *draw_list,
                        int y, int x, int visible_rows, int visible_cols,
                        const RenderStyle *style,
                        const RenderStyle *cursor_style);

#endif
