#include "test_harness.h"

#include <stdbool.h>
#include <string.h>

#include "platform/tty_decoder.h"

static bool decode_event(TtyDecoder *decoder, const TtyDecoderKeyMap *keys,
                         RetroEvent *out_event) {
    memset(out_event, 0, sizeof(*out_event));
    return tty_decoder_decode(decoder, keys, out_event);
}

static void require_navigation(const RetroEvent *event, int key_code,
                               unsigned int modifiers) {
    TEST_REQUIRE(event != NULL);
    TEST_REQUIRE(event->type == RETRO_EVENT_KEY);
    TEST_REQUIRE(event->data.key.key_code == key_code);
    TEST_REQUIRE(!event->data.key.is_printable);
    TEST_REQUIRE(event->data.key.ascii == 0);
    TEST_REQUIRE(event->data.key.text_codepoint == 0);
    TEST_REQUIRE(event->data.key.modifiers == modifiers);
}

static void require_non_text_byte(const RetroEvent *event,
                                  unsigned char byte) {
    TEST_REQUIRE(event != NULL);
    TEST_REQUIRE(event->type == RETRO_EVENT_KEY);
    TEST_REQUIRE(event->data.key.key_code == (int)byte);
    TEST_REQUIRE(!event->data.key.is_printable);
    TEST_REQUIRE(event->data.key.ascii == 0);
    TEST_REQUIRE(event->data.key.text_codepoint == 0);
    TEST_REQUIRE(event->data.key.modifiers == RETRO_MOD_NONE);
}

int main(void) {
    TtyDecoder decoder;
    tty_decoder_init(&decoder);
    TtyDecoderKeyMap keys = {
        .up = 1001,
        .down = 1002,
        .left = 1003,
        .right = 1004,
    };
    RetroEvent event = {0};

    const unsigned char printable[] = {'a'};
    tty_decoder_append(&decoder, printable, sizeof(printable));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    TEST_REQUIRE(event.type == RETRO_EVENT_KEY);
    TEST_REQUIRE(event.data.key.key_code == 'a');
    TEST_REQUIRE(event.data.key.is_printable);
    TEST_REQUIRE(event.data.key.ascii == 'a');
    TEST_REQUIRE(event.data.key.text_codepoint == 'a');
    TEST_REQUIRE(event.data.key.modifiers == RETRO_MOD_NONE);

    /* Until the raw decoder gains incremental UTF-8 support, high-bit bytes
       are ordinary non-text key bytes and must not be advertised as Unicode
       text events by the platform capability contract. */
    const unsigned char utf8_n_tilde[] = {0xC3, 0xB1};
    tty_decoder_append(&decoder, utf8_n_tilde, sizeof(utf8_n_tilde));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    require_non_text_byte(&event, utf8_n_tilde[0]);
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    require_non_text_byte(&event, utf8_n_tilde[1]);

    const unsigned char up[] = {0x1b, '[', 'A'};
    tty_decoder_append(&decoder, up, sizeof(up));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    require_navigation(&event, keys.up, RETRO_MOD_NONE);

    const unsigned char parameterized_up[] = {0x1b, '[', '1', 'A'};
    tty_decoder_append(&decoder, parameterized_up,
                       sizeof(parameterized_up));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    require_navigation(&event, keys.up, RETRO_MOD_NONE);

    const unsigned char shift_left[] = {
        0x1b, '[', '1', ';', '2', 'D'
    };
    tty_decoder_append(&decoder, shift_left, sizeof(shift_left));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    require_navigation(&event, keys.left, RETRO_MOD_SHIFT);

    const unsigned char shift_ctrl_right[] = {
        0x1b, '[', '1', ';', '6', 'C'
    };
    tty_decoder_append(&decoder, shift_ctrl_right,
                       sizeof(shift_ctrl_right));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    require_navigation(&event, keys.right,
                       RETRO_MOD_SHIFT | RETRO_MOD_CTRL);

    const unsigned char alt_up[] = {
        0x1b, '[', '1', ';', '3', 'A'
    };
    tty_decoder_append(&decoder, alt_up, sizeof(alt_up));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    require_navigation(&event, keys.up, RETRO_MOD_ALT);

    const unsigned char shift_down_part_1[] = {
        0x1b, '[', '1', ';', '2'
    };
    const unsigned char shift_down_part_2[] = {'B'};
    tty_decoder_append(&decoder, shift_down_part_1,
                       sizeof(shift_down_part_1));
    TEST_REQUIRE(!decode_event(&decoder, &keys, &event));
    tty_decoder_append(&decoder, shift_down_part_2,
                       sizeof(shift_down_part_2));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    require_navigation(&event, keys.down, RETRO_MOD_SHIFT);

    const unsigned char invalid_modifier[] = {
        0x1b, '[', '1', ';', '9', 'D'
    };
    tty_decoder_append(&decoder, invalid_modifier,
                       sizeof(invalid_modifier));
    TEST_REQUIRE(!decode_event(&decoder, &keys, &event));

    const unsigned char mouse_part_1[] = {
        0x1b, '[', '<', '0', ';', '1', '0'
    };
    const unsigned char mouse_part_2[] = {';', '5', 'M'};
    tty_decoder_append(&decoder, mouse_part_1, sizeof(mouse_part_1));
    TEST_REQUIRE(!decode_event(&decoder, &keys, &event));
    tty_decoder_append(&decoder, mouse_part_2, sizeof(mouse_part_2));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    TEST_REQUIRE(event.type == RETRO_EVENT_MOUSE);
    TEST_REQUIRE(event.data.mouse.button1_pressed);
    TEST_REQUIRE(event.data.mouse.x == 9);
    TEST_REQUIRE(event.data.mouse.y == 4);

    const unsigned char mouse_release[] = {
        0x1b, '[', '<', '0', ';', '1', '0', ';', '5', 'm'
    };
    tty_decoder_append(&decoder, mouse_release, sizeof(mouse_release));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    TEST_REQUIRE(event.type == RETRO_EVENT_MOUSE);
    TEST_REQUIRE(event.data.mouse.button1_released);
    TEST_REQUIRE(event.data.mouse.button1_clicked);

    const unsigned char wheel_up[] = {
        0x1b, '[', '<', '6', '4', ';', '2', ';', '3', 'M'
    };
    tty_decoder_append(&decoder, wheel_up, sizeof(wheel_up));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    TEST_REQUIRE(event.type == RETRO_EVENT_MOUSE);
    TEST_REQUIRE(event.data.mouse.scroll_up);

    const unsigned char right_click[] = {
        0x1b, '[', '<', '2', ';', '4', ';', '2', 'M'
    };
    tty_decoder_append(&decoder, right_click, sizeof(right_click));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    TEST_REQUIRE(event.type == RETRO_EVENT_MOUSE);
    TEST_REQUIRE(event.data.mouse.button3_pressed);
    TEST_REQUIRE(event.data.mouse.button3_clicked);

    const unsigned char invalid_mouse[] = {
        0x1b, '[', '<', 'x', ';', '1', ';', '1', 'M'
    };
    tty_decoder_append(&decoder, invalid_mouse, sizeof(invalid_mouse));
    TEST_REQUIRE(!decode_event(&decoder, &keys, &event));

    const unsigned char fallback[] = {'z'};
    tty_decoder_append(&decoder, fallback, sizeof(fallback));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    TEST_REQUIRE(event.type == RETRO_EVENT_KEY);
    TEST_REQUIRE(event.data.key.key_code == 'z');
    TEST_REQUIRE(event.data.key.ascii == 'z');
    TEST_REQUIRE(event.data.key.text_codepoint == 'z');
    TEST_REQUIRE(event.data.key.modifiers == RETRO_MOD_NONE);

    return 0;
}
