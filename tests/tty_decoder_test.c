#include "test_harness.h"
#include <stdbool.h>
#include <string.h>

#include "platform/tty_decoder.h"

static bool decode_event(TtyDecoder *decoder, const TtyDecoderKeyMap *keys,
                         RetroEvent *out_event) {
    memset(out_event, 0, sizeof(*out_event));
    return tty_decoder_decode(decoder, keys, out_event);
}

int main(void) {
    TtyDecoder decoder;
    tty_decoder_init(&decoder);
    TtyDecoderKeyMap keys = {.up = 1001, .down = 1002, .left = 1003, .right = 1004};
    RetroEvent event = {0};

    const unsigned char printable[] = {'a'};
    tty_decoder_append(&decoder, printable, sizeof(printable));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    TEST_REQUIRE(event.type == RETRO_EVENT_KEY);
    TEST_REQUIRE(event.data.key.key_code == 'a');
    TEST_REQUIRE(event.data.key.is_printable);
    TEST_REQUIRE(event.data.key.ascii == 'a');

    const unsigned char up[] = {0x1b, '[', 'A'};
    tty_decoder_append(&decoder, up, sizeof(up));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    TEST_REQUIRE(event.type == RETRO_EVENT_KEY);
    TEST_REQUIRE(event.data.key.key_code == keys.up);
    TEST_REQUIRE(!event.data.key.is_printable);

    const unsigned char mouse_part_1[] = {0x1b, '[', '<', '0', ';', '1', '0'};
    const unsigned char mouse_part_2[] = {';', '5', 'M'};
    tty_decoder_append(&decoder, mouse_part_1, sizeof(mouse_part_1));
    TEST_REQUIRE(!decode_event(&decoder, &keys, &event));
    tty_decoder_append(&decoder, mouse_part_2, sizeof(mouse_part_2));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    TEST_REQUIRE(event.type == RETRO_EVENT_MOUSE);
    TEST_REQUIRE(event.data.mouse.button1_pressed);
    TEST_REQUIRE(event.data.mouse.x == 9);
    TEST_REQUIRE(event.data.mouse.y == 4);

    const unsigned char mouse_release[] = {0x1b, '[', '<', '0', ';', '1', '0',
                                           ';',  '5', 'm'};
    tty_decoder_append(&decoder, mouse_release, sizeof(mouse_release));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    TEST_REQUIRE(event.type == RETRO_EVENT_MOUSE);
    TEST_REQUIRE(event.data.mouse.button1_released);
    TEST_REQUIRE(event.data.mouse.button1_clicked);

    const unsigned char wheel_up[] = {0x1b, '[', '<', '6', '4', ';', '2', ';', '3', 'M'};
    tty_decoder_append(&decoder, wheel_up, sizeof(wheel_up));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    TEST_REQUIRE(event.type == RETRO_EVENT_MOUSE);
    TEST_REQUIRE(event.data.mouse.scroll_up);

    const unsigned char right_click[] = {0x1b, '[', '<', '2', ';', '4', ';', '2', 'M'};
    tty_decoder_append(&decoder, right_click, sizeof(right_click));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    TEST_REQUIRE(event.type == RETRO_EVENT_MOUSE);
    TEST_REQUIRE(event.data.mouse.button3_pressed);
    TEST_REQUIRE(event.data.mouse.button3_clicked);

    const unsigned char invalid_mouse[] = {0x1b, '[', '<', 'x', ';', '1', ';', '1', 'M'};
    tty_decoder_append(&decoder, invalid_mouse, sizeof(invalid_mouse));
    TEST_REQUIRE(!decode_event(&decoder, &keys, &event));

    const unsigned char fallback[] = {'z'};
    tty_decoder_append(&decoder, fallback, sizeof(fallback));
    TEST_REQUIRE(decode_event(&decoder, &keys, &event));
    TEST_REQUIRE(event.type == RETRO_EVENT_KEY);
    TEST_REQUIRE(event.data.key.key_code == 'z');
    TEST_REQUIRE(event.data.key.ascii == 'z');

    return 0;
}
