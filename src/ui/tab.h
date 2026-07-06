#ifndef RETRODESK_UI_TAB_H
#define RETRODESK_UI_TAB_H

#include <stdbool.h>
#include <stddef.h>

#include "core/event.h"
#include "render/render.h"

/* Tab bar widget — switchable views within a window. The bar is a
   single-row strip of "[ label ]" segments separated by spaces; the
   active tab is rendered with a distinct style. Arrow keys cycle the
   active tab; clicks select a tab directly. Produces DrawList commands
   and never calls backend draw APIs directly. */

typedef void (*TabChangeCallback)(size_t old_index, size_t new_index,
                                  void *user_data);

typedef struct Tab Tab;

/* --- lifecycle ------------------------------------------------------- */

Tab *tab_create(void);
void tab_destroy(Tab *tab);

/* --- tabs ------------------------------------------------------------ */

/* Add a tab with the given label (copied internally). Returns its index
   or SIZE_MAX on failure. */
size_t tab_add(Tab *tab, const char *label);
size_t tab_count(const Tab *tab);
const char *tab_label(const Tab *tab, size_t index);

/* --- active tab ------------------------------------------------------ */

size_t tab_active(const Tab *tab);
void tab_set_active(Tab *tab, size_t index);

/* Invoked when the active tab changes via keyboard, mouse, or API.
   Not invoked if the index does not actually change. */
void tab_set_change_callback(Tab *tab, TabChangeCallback callback,
                             void *user_data);

/* --- events ---------------------------------------------------------- */

bool tab_handle_key(Tab *tab, const RetroKeyEvent *key);
bool tab_handle_mouse(Tab *tab, const RetroMouseEvent *mouse,
                      int tab_y, int tab_x, int tab_w);

/* --- rendering ------------------------------------------------------- */

/* Render the tab strip at the given position. The strip occupies one
   row. Returns the number of columns consumed (clipped to max_width). */
int tab_render(const Tab *tab, DrawList *draw_list,
               int y, int x, int max_width,
               const RenderStyle *normal_style,
               const RenderStyle *active_style);

/* Total rendered width of the strip (sum of all "[ label ]" + gaps). */
int tab_width(const Tab *tab);

/* Tab strip is always 1 row tall. */
int tab_height(void);

#endif