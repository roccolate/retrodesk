#include <assert.h>
#include <stdio.h>

#include "core/event.h"
#include "core/key_chord.h"

/* --- ASCII chord macros ----------------------------------------------- */

static void test_ctrl_macro_letters(void) {
    /* Uppercase. */
    assert(RETRO_CTRL('A') == 1);
    assert(RETRO_CTRL('B') == 2);
    assert(RETRO_CTRL('E') == 5);
    assert(RETRO_CTRL('K') == 11);
    assert(RETRO_CTRL('U') == 21);
    assert(RETRO_CTRL('Z') == 26);
    /* Lowercase produces the same value (5th bit difference). */
    assert(RETRO_CTRL('a') == 1);
    assert(RETRO_CTRL('e') == 5);
    assert(RETRO_CTRL('k') == 11);
    assert(RETRO_CTRL('z') == 26);
    printf("  PASS: ctrl_macro_letters\n");
}

static void test_named_control_aliases(void) {
    assert(RETRO_KEY_BS     == 0x08);
    assert(RETRO_KEY_TAB    == 0x09);
    assert(RETRO_KEY_LF     == 0x0A);
    assert(RETRO_KEY_CR     == 0x0D);
    assert(RETRO_KEY_ESC    == 0x1B);
    assert(RETRO_KEY_DEL    == 0x7F);
    assert(RETRO_KEY_CTRL_A == 0x01);
    assert(RETRO_KEY_CTRL_K == 0x0B);
    assert(RETRO_KEY_CTRL_U == 0x15);
    printf("  PASS: named_control_aliases\n");
}

/* --- navigation chord codes ------------------------------------------ */

static void test_navigation_chords(void) {
    assert(RETRO_KEY_UP    == 0x1000);
    assert(RETRO_KEY_DOWN  == 0x1001);
    assert(RETRO_KEY_LEFT  == 0x1002);
    assert(RETRO_KEY_RIGHT == 0x1003);
    assert(RETRO_KEY_HOME  == 0x1004);
    assert(RETRO_KEY_END   == 0x1005);
    assert(RETRO_KEY_PPAGE == 0x1006);
    assert(RETRO_KEY_NPAGE == 0x1007);
    assert(RETRO_KEY_DC    == 0x1008);
    assert(RETRO_KEY_IC    == 0x1009);
    assert(RETRO_KEY_BTAB  == 0x100A);
    /* No collisions between the navigation range and ASCII. */
    assert(RETRO_KEY_UP    > 0xFF);
    assert(RETRO_KEY_RIGHT > RETRO_KEY_UP);
    printf("  PASS: navigation_chords\n");
}

static void test_function_keys(void) {
    const int fkeys[] = {
        RETRO_KEY_F1,  RETRO_KEY_F2,  RETRO_KEY_F3,  RETRO_KEY_F4,
        RETRO_KEY_F5,  RETRO_KEY_F6,  RETRO_KEY_F7,  RETRO_KEY_F8,
        RETRO_KEY_F9,  RETRO_KEY_F10, RETRO_KEY_F11, RETRO_KEY_F12,
    };

    for (int i = 0; i < 12; ++i) {
        assert(fkeys[i] == RETRO_KEY_F1 + i);
        assert(retro_key_is_chord(fkeys[i]));
        assert(!retro_key_is_printable(fkeys[i]));
        assert(!retro_key_is_control(fkeys[i]));
    }

    assert(RETRO_KEY_F1  == 0x1100);
    assert(RETRO_KEY_F12 == 0x110B);
    assert(RETRO_KEY_F12 == RETRO_KEY_F1 + 11);
    /* F-keys live outside both the ASCII and the navigation ranges. */
    assert(RETRO_KEY_F1 > 0xFF);
    assert(RETRO_KEY_F1 > RETRO_KEY_BTAB);
    printf("  PASS: function_keys\n");
}

/* --- modifier masks -------------------------------------------------- */

static void test_modifier_masks(void) {
    assert(RETRO_MOD_NONE  == 0x0000);
    assert(RETRO_MOD_SHIFT == 0x0001);
    assert(RETRO_MOD_CTRL  == 0x0002);
    assert(RETRO_MOD_ALT   == 0x0004);
    /* Modifiers are bit flags; combining them should OR cleanly. */
    int combined = RETRO_MOD_CTRL | RETRO_MOD_SHIFT;
    assert((combined & RETRO_MOD_CTRL)  == RETRO_MOD_CTRL);
    assert((combined & RETRO_MOD_SHIFT) == RETRO_MOD_SHIFT);
    assert((combined & RETRO_MOD_ALT)   == 0);
    printf("  PASS: modifier_masks\n");
}

/* --- helper functions ------------------------------------------------ */

static void test_is_chord(void) {
    /* ASCII bytes are not chords. */
    assert(!retro_key_is_chord('a'));
    assert(!retro_key_is_chord('Z'));
    assert(!retro_key_is_chord('1'));
    assert(!retro_key_is_chord(0));
    assert(!retro_key_is_chord(0x7F));
    assert(!retro_key_is_chord(0x80));
    /* Navigation chords are chords. */
    assert(retro_key_is_chord(RETRO_KEY_UP));
    assert(retro_key_is_chord(RETRO_KEY_DOWN));
    assert(retro_key_is_chord(RETRO_KEY_LEFT));
    assert(retro_key_is_chord(RETRO_KEY_RIGHT));
    assert(retro_key_is_chord(RETRO_KEY_HOME));
    assert(retro_key_is_chord(RETRO_KEY_DC));
    /* Function keys are chords. */
    assert(retro_key_is_chord(RETRO_KEY_F1));
    assert(retro_key_is_chord(RETRO_KEY_F12));
    /* Negative and out-of-range values are not. */
    assert(!retro_key_is_chord(-1));
    assert(!retro_key_is_chord(0x2000));
    printf("  PASS: is_chord\n");
}

static void test_is_printable(void) {
    assert(!retro_key_is_printable(0));
    assert(!retro_key_is_printable(0x1F));
    assert(retro_key_is_printable(0x20));
    assert(retro_key_is_printable('a'));
    assert(retro_key_is_printable('Z'));
    assert(retro_key_is_printable('1'));
    assert(retro_key_is_printable(0x7E));
    assert(!retro_key_is_printable(0x7F));
    assert(!retro_key_is_printable(0x80));
    /* Chords are not printable. */
    assert(!retro_key_is_printable(RETRO_KEY_LEFT));
    assert(!retro_key_is_printable(RETRO_KEY_UP));
    assert(!retro_key_is_printable(RETRO_KEY_F5));
    printf("  PASS: is_printable\n");
}

static void test_is_control(void) {
    /* ASCII control range. */
    assert(retro_key_is_control(0x00));
    assert(retro_key_is_control(0x01));
    assert(retro_key_is_control(0x1F));
    /* DEL is a control. */
    assert(retro_key_is_control(0x7F));
    /* Printable ASCII is not. */
    assert(!retro_key_is_control(0x20));
    assert(!retro_key_is_control('a'));
    assert(!retro_key_is_control('Z'));
    /* Extended ASCII and chords are not. */
    assert(!retro_key_is_control(0x80));
    assert(!retro_key_is_control(0xFF));
    assert(!retro_key_is_control(RETRO_KEY_LEFT));
    assert(!retro_key_is_control(-1));
    printf("  PASS: is_control\n");
}

static void test_key_event_ascii_byte_is_unsigned(void) {
    RetroKeyEvent key = {0};
    key.ascii = (unsigned char)0xE9;
    assert(key.ascii == 0xE9u);
    assert((int)key.ascii == 233);
    assert(!retro_key_is_chord((int)key.ascii));
    assert(!retro_key_is_printable((int)key.ascii));
    assert(!retro_key_is_control((int)key.ascii));
    printf("  PASS: key_event_ascii_byte_is_unsigned\n");
}

/* --- collision invariants ------------------------------------------- */

static void test_no_collisions(void) {
    /* ASCII range (0..127) must not overlap with navigation range
       (0x1000+) or function-key range (0x1100+). */
    assert(RETRO_KEY_DEL < RETRO_KEY_UP);
    assert(RETRO_KEY_NPAGE < RETRO_KEY_F1);
    /* All named navigation chords fit inside a single 0x1000..0x10FF slot. */
    assert(RETRO_KEY_BTAB == 0x100A);
    printf("  PASS: no_collisions\n");
}

/* --- null / boundary safety ----------------------------------------- */

static void test_boundaries(void) {
    /* Chords range: 0x1000..0x1FFF (inclusive). 0x2000 is past the end. */
    assert(!retro_key_is_chord(0x0FFF));
    assert(retro_key_is_chord(0x1000));
    assert(retro_key_is_chord(0x10FF));
    assert(retro_key_is_chord(0x1100));
    assert(retro_key_is_chord(0x1FFF));
    assert(!retro_key_is_chord(0x2000));
    /* Printability around the boundaries. */
    assert(!retro_key_is_printable(0x1F));
    assert(retro_key_is_printable(0x20));
    assert(retro_key_is_printable(0x7E));
    assert(!retro_key_is_printable(0x7F));
    printf("  PASS: boundaries\n");
}

int main(void) {
    printf("key_chord_test:\n");
    test_ctrl_macro_letters();
    test_named_control_aliases();
    test_navigation_chords();
    test_function_keys();
    test_modifier_masks();
    test_is_chord();
    test_is_printable();
    test_is_control();
    test_key_event_ascii_byte_is_unsigned();
    test_no_collisions();
    test_boundaries();
    printf("All key_chord tests passed.\n");
    return 0;
}
