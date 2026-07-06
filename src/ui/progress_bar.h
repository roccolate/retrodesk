#ifndef RETRODESK_UI_PROGRESS_BAR_H
#define RETRODESK_UI_PROGRESS_BAR_H

#include <stdbool.h>
#include <stddef.h>

#include "render/render.h"

/* Progress bar widget. Supports two styles:
     - DETERMINATE:   [####-----] value%   (value 0..100)
     - INDETERMINATE: [  ====    ]         (animated block bouncing)
   The widget never calls backend draw APIs directly; it appends draw
   commands to the supplied DrawList. */

typedef enum ProgressBarStyle {
    PROGRESS_BAR_DETERMINATE = 0,
    PROGRESS_BAR_INDETERMINATE,
} ProgressBarStyle;

typedef struct ProgressBar ProgressBar;

/* --- lifecycle ------------------------------------------------------- */

ProgressBar *progress_bar_create(void);
void progress_bar_destroy(ProgressBar *bar);

/* --- configuration --------------------------------------------------- */

void progress_bar_set_style(ProgressBar *bar, ProgressBarStyle style);
ProgressBarStyle progress_bar_style(const ProgressBar *bar);

/* Determinate value: clamped to 0..100. */
void progress_bar_set_value(ProgressBar *bar, int value);
int progress_bar_value(const ProgressBar *bar);

/* Advance indeterminate animation by one tick (wraps). */
void progress_bar_tick(ProgressBar *bar);

/* Visible width of the bar in characters (excluding brackets). Default 20. */
void progress_bar_set_width(ProgressBar *bar, int width);
int progress_bar_width(const ProgressBar *bar);

/* Optional label appended to the right of the bar (copied internally). */
void progress_bar_set_label(ProgressBar *bar, const char *label);
const char *progress_bar_label(const ProgressBar *bar);

/* --- rendering ------------------------------------------------------- */

/* Render the bar at the given position. visible_width is the total
   horizontal space available (brackets + label); if 0, width + label
   are used. Returns the number of columns consumed. */
int progress_bar_render(const ProgressBar *bar, DrawList *draw_list,
                        int y, int x, int visible_width,
                        const RenderStyle *frame_style,
                        const RenderStyle *fill_style,
                        const RenderStyle *label_style);

#endif