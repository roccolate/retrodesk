#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/key_chord.h"
#include "ui/text_input.h"

/* Helper to simulate a printable key press. */
static RetroKeyEvent key_char(char ch) {
    RetroKeyEvent k = {0};
    k.key_code = (int)ch;
    k.is_printable = true;
    k.ascii = ch;
    return k;
}

/* Helper to simulate a control/special key press. */
static RetroKeyEvent key_code(int code) {
    RetroKeyEvent k = {0};
    k.key_code = code;
    k.is_printable = false;
    k.ascii = '\0';
    return k;
}

static void test_create_destroy(void) {
    TextInput *ti = text_input_create(0);
    assert(ti != NULL);
    assert(text_input_length(ti) == 0);
    assert(text_input_cursor(ti) == 0);
    assert(strcmp(text_input_text(ti), "") == 0);
    text_input_destroy(ti);
    printf("  PASS: create_destroy\n");
}

static void test_insert_chars(void) {
    TextInput *ti = text_input_create(0);
    RetroKeyEvent k;

    k = key_char('H'); text_input_handle_key(ti, &k);
    k = key_char('i'); text_input_handle_key(ti, &k);
    k = key_char('!'); text_input_handle_key(ti, &k);

    assert(strcmp(text_input_text(ti), "Hi!") == 0);
    assert(text_input_length(ti) == 3);
    assert(text_input_cursor(ti) == 3);

    text_input_destroy(ti);
    printf("  PASS: insert_chars\n");
}

static void test_backspace(void) {
    TextInput *ti = text_input_create(0);
    text_input_set_text(ti, "ABC");

    /* Backspace at end */
    RetroKeyEvent k = key_code(127);
    text_input_handle_key(ti, &k);
    assert(strcmp(text_input_text(ti), "AB") == 0);
    assert(text_input_cursor(ti) == 2);

    /* Backspace again */
    text_input_handle_key(ti, &k);
    assert(strcmp(text_input_text(ti), "A") == 0);

    /* Backspace again */
    text_input_handle_key(ti, &k);
    assert(strcmp(text_input_text(ti), "") == 0);

    /* Backspace on empty — should not crash */
    bool consumed = text_input_handle_key(ti, &k);
    assert(!consumed);
    assert(text_input_length(ti) == 0);

    text_input_destroy(ti);
    printf("  PASS: backspace\n");
}

static void test_cursor_movement(void) {
    TextInput *ti = text_input_create(0);
    text_input_set_text(ti, "Hello");
    assert(text_input_cursor(ti) == 5); /* set_text puts cursor at end */

    /* Move left */
    RetroKeyEvent left = key_code(RETRO_KEY_LEFT);
    text_input_handle_key(ti, &left);
    assert(text_input_cursor(ti) == 4);

    text_input_handle_key(ti, &left);
    text_input_handle_key(ti, &left);
    text_input_handle_key(ti, &left);
    text_input_handle_key(ti, &left);
    assert(text_input_cursor(ti) == 0);

    /* Left at position 0 — stays */
    text_input_handle_key(ti, &left);
    assert(text_input_cursor(ti) == 0);

    /* Move right */
    RetroKeyEvent right = key_code(RETRO_KEY_RIGHT);
    text_input_handle_key(ti, &right);
    assert(text_input_cursor(ti) == 1);

    /* Insert in middle */
    RetroKeyEvent k = key_char('X');
    text_input_handle_key(ti, &k);
    assert(strcmp(text_input_text(ti), "HXello") == 0);
    assert(text_input_cursor(ti) == 2);

    text_input_destroy(ti);
    printf("  PASS: cursor_movement\n");
}

static void test_ctrl_shortcuts(void) {
    TextInput *ti = text_input_create(0);
    text_input_set_text(ti, "Hello World");

    /* Ctrl+A — home */
    RetroKeyEvent ctrl_a = key_code(1);
    text_input_handle_key(ti, &ctrl_a);
    assert(text_input_cursor(ti) == 0);

    /* Ctrl+E — end */
    RetroKeyEvent ctrl_e = key_code(5);
    text_input_handle_key(ti, &ctrl_e);
    assert(text_input_cursor(ti) == 11);

    /* Ctrl+K — kill to end */
    RetroKeyEvent ctrl_a2 = key_code(1);
    text_input_handle_key(ti, &ctrl_a2); /* go home */

    /* Move right 5 positions */
    RetroKeyEvent right = key_code(RETRO_KEY_RIGHT);
    for (int i = 0; i < 5; i++) text_input_handle_key(ti, &right);
    RetroKeyEvent ctrl_k = key_code(11);
    text_input_handle_key(ti, &ctrl_k);
    assert(strcmp(text_input_text(ti), "Hello") == 0);

    /* Ctrl+U — clear all */
    RetroKeyEvent ctrl_u = key_code(21);
    text_input_handle_key(ti, &ctrl_u);
    assert(text_input_length(ti) == 0);
    assert(text_input_cursor(ti) == 0);

    text_input_destroy(ti);
    printf("  PASS: ctrl_shortcuts\n");
}

static void test_max_length(void) {
    TextInput *ti = text_input_create(5);

    RetroKeyEvent k;
    k = key_char('A'); text_input_handle_key(ti, &k);
    k = key_char('B'); text_input_handle_key(ti, &k);
    k = key_char('C'); text_input_handle_key(ti, &k);
    k = key_char('D'); text_input_handle_key(ti, &k);
    k = key_char('E'); text_input_handle_key(ti, &k);
    assert(text_input_length(ti) == 5);

    /* Should not insert beyond max */
    k = key_char('F');
    bool consumed = text_input_handle_key(ti, &k);
    assert(!consumed);
    assert(text_input_length(ti) == 5);
    assert(strcmp(text_input_text(ti), "ABCDE") == 0);

    text_input_destroy(ti);
    printf("  PASS: max_length\n");
}

static void test_set_text_and_clear(void) {
    TextInput *ti = text_input_create(0);

    text_input_set_text(ti, "foo bar");
    assert(strcmp(text_input_text(ti), "foo bar") == 0);
    assert(text_input_cursor(ti) == 7);

    text_input_clear(ti);
    assert(text_input_length(ti) == 0);
    assert(text_input_cursor(ti) == 0);
    assert(strcmp(text_input_text(ti), "") == 0);

    /* set_text with max_len */
    TextInput *ti2 = text_input_create(3);
    text_input_set_text(ti2, "Hello");
    assert(strcmp(text_input_text(ti2), "Hel") == 0);
    assert(text_input_length(ti2) == 3);

    text_input_destroy(ti);
    text_input_destroy(ti2);
    printf("  PASS: set_text_and_clear\n");
}

static void test_delete_forward(void) {
    TextInput *ti = text_input_create(0);
    text_input_set_text(ti, "ABCD");

    /* Go to beginning */
    RetroKeyEvent ctrl_a = key_code(1);
    text_input_handle_key(ti, &ctrl_a);

    /* Ctrl+D — delete forward */
    RetroKeyEvent ctrl_d = key_code(4);
    text_input_handle_key(ti, &ctrl_d);
    assert(strcmp(text_input_text(ti), "BCD") == 0);
    assert(text_input_cursor(ti) == 0);

    /* Delete at end — should not work */
    RetroKeyEvent ctrl_e = key_code(5);
    text_input_handle_key(ti, &ctrl_e);
    bool consumed = text_input_handle_key(ti, &ctrl_d);
    assert(!consumed);
    assert(strcmp(text_input_text(ti), "BCD") == 0);

    text_input_destroy(ti);
    printf("  PASS: delete_forward\n");
}

static void test_null_safety(void) {
    /* All functions should handle NULL gracefully. */
    assert(text_input_text(NULL) != NULL);
    assert(text_input_cursor(NULL) == 0);
    assert(text_input_length(NULL) == 0);
    assert(!text_input_handle_key(NULL, NULL));
    text_input_clear(NULL);
    text_input_set_text(NULL, "test");
    text_input_destroy(NULL);
    text_input_render(NULL, NULL, 0, 0, 10, NULL, NULL);
    printf("  PASS: null_safety\n");
}

int main(void) {
    printf("text_input_test:\n");
    test_create_destroy();
    test_insert_chars();
    test_backspace();
    test_cursor_movement();
    test_ctrl_shortcuts();
    test_max_length();
    test_set_text_and_clear();
    test_delete_forward();
    test_null_safety();
    printf("All text_input tests passed.\n");
    return 0;
}
