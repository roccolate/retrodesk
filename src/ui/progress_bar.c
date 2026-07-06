#include "ui/progress_bar.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum {
    PROGRESS_BAR_DEFAULT_WIDTH = 20,
    PROGRESS_BAR_INDETERMINATE_BLOCK = 4,
    PROGRESS_BAR_LABEL_INIT_CAP = 32,
};

struct ProgressBar {
    ProgressBarStyle style;
    int value;            /* 0..100 */
    int width;            /* inner width (excluding brackets) */
    int anim_pos;         /* 0..(width+block-1) */
    char *label;
};

/* --- internal helpers ------------------------------------------------- */

static char *progress_bar_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = malloc(len + 1);
    if (!dup) return NULL;
    memcpy(dup, s, len + 1);
    return dup;
}

static int progress_bar_clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* --- lifecycle ------------------------------------------------------- */

ProgressBar *progress_bar_create(void) {
    ProgressBar *bar = calloc(1, sizeof(*bar));
    if (!bar) return NULL;
    bar->style = PROGRESS_BAR_DETERMINATE;
    bar->value = 0;
    bar->width = PROGRESS_BAR_DEFAULT_WIDTH;
    bar->anim_pos = 0;
    bar->label = NULL;
    return bar;
}

void progress_bar_destroy(ProgressBar *bar) {
    if (!bar) return;
    free(bar->label);
    bar->label = NULL;
    free(bar);
}

/* --- configuration --------------------------------------------------- */

void progress_bar_set_style(ProgressBar *bar, ProgressBarStyle style) {
    if (!bar) return;
    bar->style = style;
}

ProgressBarStyle progress_bar_style(const ProgressBar *bar) {
    if (!bar) return PROGRESS_BAR_DETERMINATE;
    return bar->style;
}

void progress_bar_set_value(ProgressBar *bar, int value) {
    if (!bar) return;
    bar->value = progress_bar_clamp(value, 0, 100);
}

int progress_bar_value(const ProgressBar *bar) {
    if (!bar) return 0;
    return bar->value;
}

void progress_bar_tick(ProgressBar *bar) {
    if (!bar) return;
    if (bar->width <= 0) {
        bar->anim_pos = 0;
        return;
    }
    /* Position ranges over a span of `width + block - 1` cells, then
       bounces back. */
    int span = bar->width + PROGRESS_BAR_INDETERMINATE_BLOCK - 1;
    if (span < 1) span = 1;
    int period = span * 2 - 2;
    if (period < 1) period = 1;
    bar->anim_pos = (bar->anim_pos + 1) % period;
}

void progress_bar_set_width(ProgressBar *bar, int width) {
    if (!bar) return;
    if (width < 1) width = 1;
    bar->width = width;
}

int progress_bar_width(const ProgressBar *bar) {
    if (!bar) return 0;
    return bar->width;
}

void progress_bar_set_label(ProgressBar *bar, const char *label) {
    if (!bar) return;
    free(bar->label);
    bar->label = progress_bar_strdup(label ? label : "");
    if (!bar->label) return;
}

const char *progress_bar_label(const ProgressBar *bar) {
    if (!bar || !bar->label) return "";
    return bar->label;
}

/* --- rendering ------------------------------------------------------- */

static int progress_bar_determinate_fill(const ProgressBar *bar, char *out,
                                         int out_size) {
    if (!bar || !out || out_size <= 0) return 0;
    int filled = (bar->value * bar->width + 50) / 100;
    if (filled > bar->width) filled = bar->width;
    int written = 0;
    if (filled > 0 && written + filled < out_size) {
        memset(out + written, '#', (size_t)filled);
        written += filled;
    }
    int empty = bar->width - filled;
    if (empty > 0 && written + empty < out_size) {
        memset(out + written, '-', (size_t)empty);
        written += empty;
    }
    out[written] = '\0';
    return written;
}

static int progress_bar_indeterminate_fill(const ProgressBar *bar, char *out,
                                           int out_size) {
    if (!bar || !out || out_size <= 0) return 0;
    int span = bar->width + PROGRESS_BAR_INDETERMINATE_BLOCK - 1;
    if (span < 1) span = 1;
    int period = span * 2 - 2;
    if (period < 1) period = 1;
    /* Triangle wave: 0..span-1..0. */
    int p = bar->anim_pos % period;
    int pos;
    if (p < span) {
        pos = p;
    } else {
        pos = period - p;
    }
    /* Place the block at [pos, pos+block). */
    int written = 0;
    for (int i = 0; i < bar->width && written < out_size - 1; i++) {
        bool inside = (i >= pos && i < pos + PROGRESS_BAR_INDETERMINATE_BLOCK);
        out[written++] = inside ? '=' : ' ';
    }
    out[written] = '\0';
    return written;
}

static int progress_bar_format_label(const ProgressBar *bar, char *out,
                                     int out_size) {
    if (!bar || !out || out_size <= 0) return 0;
    int written = 0;
    if (bar->style == PROGRESS_BAR_DETERMINATE) {
        written = snprintf(out, (size_t)out_size, " %d%%", bar->value);
        if (written < 0) written = 0;
        if (written >= out_size) written = out_size - 1;
    } else if (bar->label && bar->label[0]) {
        written = snprintf(out, (size_t)out_size, " %s", bar->label);
        if (written < 0) written = 0;
        if (written >= out_size) written = out_size - 1;
    }
    return written;
}

int progress_bar_render(const ProgressBar *bar, DrawList *draw_list,
                        int y, int x, int visible_width,
                        const RenderStyle *frame_style,
                        const RenderStyle *fill_style,
                        const RenderStyle *label_style) {
    if (!bar || !draw_list) return 0;
    if (visible_width < 0) return 0;

    int w = bar->width;
    /* Compute label length for layout. */
    char label_buf[128];
    int label_len = progress_bar_format_label(bar, label_buf, (int)sizeof(label_buf));
    int total = 1 /* [ */ + w + 1 /* ] */ + label_len;
    if (visible_width > 0 && total > visible_width) {
        /* Shrink inner width to fit. */
        int max_inner = visible_width - 2 - label_len;
        if (max_inner < 1) max_inner = 1;
        w = max_inner;
        total = 1 + w + 1 + label_len;
    }

    /* Build the full row text in a single buffer to minimize DrawList
       entries and to keep fill/label rendering interleaved correctly. */
    char row[256];
    if (total >= (int)sizeof(row)) total = (int)sizeof(row) - 1;
    int p = 0;
    row[p++] = '[';
    char inner[128];
    if (bar->style == PROGRESS_BAR_DETERMINATE) {
        progress_bar_determinate_fill(bar, inner, (int)sizeof(inner));
    } else {
        progress_bar_indeterminate_fill(bar, inner, (int)sizeof(inner));
    }
    /* If the inner width was clamped, adjust the buffer copy. */
    int inner_len = (int)strlen(inner);
    if (inner_len > w) inner_len = w;
    memcpy(row + p, inner, (size_t)inner_len);
    p += inner_len;
    row[p++] = ']';
    /* Append label text. */
    for (int i = 0; i < label_len && p < (int)sizeof(row) - 1; i++) {
        row[p++] = label_buf[i];
    }
    row[p] = '\0';

    (void)frame_style;
    (void)fill_style;

    /* Render fill as the inner segment (overrides the plain text chars),
       then the rest with frame_style / label_style. */
    if (fill_style && inner_len > 0) {
        char fill_segment[128];
        if (inner_len >= (int)sizeof(fill_segment)) inner_len = (int)sizeof(fill_segment) - 1;
        memcpy(fill_segment, inner, (size_t)inner_len);
        fill_segment[inner_len] = '\0';
        draw_list_text(draw_list, y, x + 1, fill_segment, fill_style);
    }

    /* Render the brackets and label separately. */
    if (frame_style) {
        char brk[2] = {'[', 0};
        draw_list_text(draw_list, y, x, brk, frame_style);
        if (x + 1 + w < x + total) {
            char brk2[2] = {']', 0};
            draw_list_text(draw_list, y, x + 1 + w, brk2, frame_style);
        }
    }
    if (label_style && label_len > 0) {
        draw_list_text(draw_list, y, x + 1 + w + 1, label_buf, label_style);
    }

    return total;
}