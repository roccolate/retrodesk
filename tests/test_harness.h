#ifndef RETRODESK_TEST_HARNESS_H
#define RETRODESK_TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>

/*
 * Test oracles must remain active in every build configuration.  In
 * In particular, do not implement these macros with the standard debug-only
 * assertion facility: Release builds define NDEBUG and would silently skip
 * both the check and any action embedded in its expression.
 */
static inline void retro_test_fail(const char *kind, const char *expression,
                                   const char *file, int line) {
    fprintf(stderr, "%s failed at %s:%d: %s\n", kind, file, line, expression);
    fflush(stderr);
    exit(EXIT_FAILURE);
}

#define TEST_CHECK(expression)                                                   \
    do {                                                                         \
        if (!(expression)) {                                                     \
            retro_test_fail("TEST_CHECK", #expression, __FILE__, __LINE__);     \
        }                                                                        \
    } while (0)

#define TEST_REQUIRE(expression)                                                 \
    do {                                                                         \
        if (!(expression)) {                                                     \
            retro_test_fail("TEST_REQUIRE", #expression, __FILE__, __LINE__);   \
        }                                                                        \
    } while (0)

#endif
