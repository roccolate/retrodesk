#include <assert.h>
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
    assert(decode_event(&decoder, &keys, &event));
    assert(event.type == RETRO_EVENT_KEY);
    assert(event.data.key.key_code == 'a');
    assert(event.data.key.is_printable);
    assert(event.data.key.ascii == 'a');

    const unsigned char up[] = {0x1b, '[', 'A'};
    tty_decoder_append(&decoder, up, sizeof(up));
    assert(decode_event(&decoder, &keys, &event));
    assert(event.type == RETRO_EVENT_KEY);
    assert(event.data.key.key_code == keys.up);
    assert(!event.data.key.is_printable);

    const unsigned char mouse_part_1[] = {0x1b, '[', '<', '0', ';', '1', '0'};
    const unsigned char mouse_part_2[] = {';', '5', 'M'};
    tty_decoder_append(&decoder, mouse_part_1, sizeof(mouse_part_1));
    assert(!decode_event(&decoder, &keys, &event));
    tty_decoder_append(&decoder, mouse_part_2, sizeof(mouse_part_2));
    assert(decode_event(&decoder, &keys, &event));
    assert(event.type == RETRO_EVENT_MOUSE);
    assert(event.data.mouse.button1_pressed);
    assert(event.data.mouse.x == 9);
    assert(event.data.mouse.y == 4);

    const unsigned char mouse_release[] = {0x1b, '[', '<', '0', ';', '1', '0',
                                           ';',  '5', 'm'};
    tty_decoder_append(&decoder, mouse_release, sizeof(mouse_release));
    assert(decode_event(&decoder, &keys, &event));
    assert(event.type == RETRO_EVENT_MOUSE);
    assert(event.data.mouse.button1_released);
    assert(event.data.mouse.button1_clicked);

    const unsigned char wheel_up[] = {0x1b, '[', '<', '6', '4', ';', '2', ';', '3', 'M'};
    tty_decoder_append(&decoder, wheel_up, sizeof(wheel_up));
    assert(decode_event(&decoder, &keys, &event));
    assert(event.type == RETRO_EVENT_MOUSE);
    assert(event.data.mouse.scroll_up);

    const unsigned char right_click[] = {0x1b, '[', '<', '2', ';', '4', ';', '2', 'M'};
    tty_decoder_append(&decoder, right_click, sizeof(right_click));
    assert(decode_event(&decoder, &keys, &event));
    assert(event.type == RETRO_EVENT_MOUSE);
    assert(event.data.mouse.button3_pressed);
    assert(event.data.mouse.button3_clicked);

    const unsigned char invalid_mouse[] = {0x1b, '[', '<', 'x', ';', '1', ';', '1', 'M'};
    tty_decoder_append(&decoder, invalid_mouse, sizeof(invalid_mouse));
    assert(!decode_event(&decoder, &keys, &event));

    const unsigned char fallback[] = {'z'};
    tty_decoder_append(&decoder, fallback, sizeof(fallback));
    assert(decode_event(&decoder, &keys, &event));
    assert(event.type == RETRO_EVENT_KEY);
    assert(event.data.key.key_code == 'z');
    assert(event.data.key.ascii == 'z');

    return 0;
}
