#ifndef RETRODESK_CORE_CHECKED_SIZE_H
#define RETRODESK_CORE_CHECKED_SIZE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Compute a geometrically grown element capacity without overflowing either
   the capacity arithmetic or the eventual byte-size multiplication. */
static inline bool retro_checked_capacity_grow(size_t current_capacity,
                                     size_t required_count,
                                     size_t element_size,
                                     size_t initial_capacity,
                                     size_t *out_capacity) {
    if (!out_capacity || element_size == 0 || initial_capacity == 0) return false;

    const size_t max_elements = SIZE_MAX / element_size;
    if (current_capacity > max_elements || required_count > max_elements) return false;

    if (required_count <= current_capacity) {
        *out_capacity = current_capacity;
        return true;
    }

    size_t next_capacity = current_capacity ? current_capacity : initial_capacity;
    if (next_capacity > max_elements) return false;

    while (next_capacity < required_count) {
        if (next_capacity > max_elements / 2) {
  next_capacity = required_count;
  break;
        }
        next_capacity *= 2;
    }

    if (next_capacity < required_count || next_capacity > max_elements) return false;
    *out_capacity = next_capacity;
    return true;
}

#endif
