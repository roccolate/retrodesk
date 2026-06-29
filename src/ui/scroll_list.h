#ifndef RETRODESK_UI_SCROLL_LIST_H
#define RETRODESK_UI_SCROLL_LIST_H

#include <stdbool.h>
#include <stddef.h>

#include "core/event.h"
#include "render/render.h"

/* Scrollable list widget with single-item selection, viewport scrolling,
   and item count display. Apps embed a ScrollList, set items, and forward
   key/mouse events. The widget appends draw commands to a DrawList. */

typedef struct ScrollList ScrollList;

/* Lifecycle */
ScrollList *scroll_list_create(void);
void scroll_list_destroy(ScrollList *list);

/* Item management — items are copied internally. */
bool scroll_list_set_items(ScrollList *list, const char *const *items,
                           size_t count);
bool scroll_list_append(ScrollList *list, const char *item);
void scroll_list_clear(ScrollList *list);
size_t scroll_list_count(const ScrollList *list);
const char *scroll_list_item(const ScrollList *list, size_t index);

/* Selection */
size_t scroll_list_selected(const ScrollList *list);
void scroll_list_select(ScrollList *list, size_t index);

/* Scroll state */
size_t scroll_list_scroll_offset(const ScrollList *list);

/* Event handling — returns true if the event was consumed. */
bool scroll_list_handle_key(ScrollList *list, const RetroKeyEvent *key);
bool scroll_list_handle_mouse(ScrollList *list, const RetroMouseEvent *mouse,
                              int widget_y, int widget_x,
                              int visible_height, int visible_width);

/* Render the list into the given draw list.
   visible_height: how many rows are available for items.
   visible_width: how many columns are available. */
void scroll_list_render(const ScrollList *list, DrawList *draw_list,
                        int y, int x, int visible_height, int visible_width,
                        const RenderStyle *normal_style,
                        const RenderStyle *selected_style);

#endif
